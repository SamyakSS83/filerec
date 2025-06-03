#include "filesystems/ntfs_parser.h"
#include "utils/logger.h"
#include <cstring>
#include <algorithm>
#include <numeric> // For std::accumulate

namespace FileRecovery {

NtfsParser::NtfsParser() = default;

bool NtfsParser::initialize(const Byte* data, Size size) {
    disk_data_ = data;
    disk_size_ = size;
    return canParse(data, size);
}

bool NtfsParser::canParse(const Byte* data, Size size) const {
    if (size < sizeof(NtfsBootSector)) {
        return false;
    }
    
    const auto* boot = reinterpret_cast<const NtfsBootSector*>(data);
    return validate_boot_sector(boot);
}

std::vector<RecoveredFile> NtfsParser::recoverDeletedFiles() {
    if (!disk_data_ || disk_size_ == 0) {
        LOG_ERROR("NTFS parser not initialized");
        return {};
    }
    
    LOG_INFO("Parsing NTFS filesystem metadata");
    
    const auto* boot = reinterpret_cast<const NtfsBootSector*>(disk_data_);
    auto files = parse_mft_records(disk_data_, disk_size_, boot, 0);
    
    LOG_INFO("Found " + std::to_string(files.size()) + " files in NTFS filesystem");
    return files;
}

std::string NtfsParser::getFileSystemInfo() const {
    return "NTFS File System";
}

bool NtfsParser::validate_boot_sector(const NtfsBootSector* boot) const {
    // Check NTFS signature
    if (std::memcmp(boot->oem_id, "NTFS    ", 8) != 0) {
        return false;
    }
    
    // Check end marker
    if (boot->end_marker != 0xAA55) {
        return false;
    }
    
    // Validate sector size
    if (boot->bytes_per_sector != 512) {
        return false;
    }
    
    // Validate sectors per cluster (must be power of 2)
    uint8_t spc = boot->sectors_per_cluster;
    if (spc == 0 || (spc & (spc - 1)) != 0) {
        return false;
    }
    
    // Validate MFT location
    if (boot->mft_lcn == 0 || boot->mft_mirror_lcn == 0) {
        return false;
    }
    
    return true;
}

bool NtfsParser::validate_mft_record(const MftRecord* record) const {
    // Check FILE signature
    if (std::memcmp(record->signature, "FILE", 4) != 0) {
        return false;
    }
    
    // Check reasonable sizes
    if (record->used_size > record->allocated_size) {
        return false;
    }
    
    if (record->allocated_size > 4096) { // Standard MFT record size
        return false;
    }
    
    return true;
}

std::vector<RecoveredFile> NtfsParser::parse_mft_records(const uint8_t* data, size_t size,
                                                       const NtfsBootSector* boot, uint64_t partition_offset) {
    std::vector<RecoveredFile> files;
    
    uint64_t mft_offset = get_mft_offset(boot);
    uint32_t record_size = get_mft_record_size(boot);
    
    if (mft_offset >= size) {
        LOG_ERROR("MFT offset beyond data size");
        return files;
    }
    
    LOG_DEBUG("MFT located at offset " + std::to_string(mft_offset) + 
                 ", record size: " + std::to_string(record_size));
    
    // Parse MFT records
    size_t current_offset = mft_offset;
    uint32_t record_count = 0;
    const uint32_t max_records = 100000; // Reasonable limit
    
    while (current_offset + record_size <= size && record_count < max_records) {
        const auto* record = reinterpret_cast<const MftRecord*>(data + current_offset);
        
        if (!validate_mft_record(record)) {
            current_offset += record_size;
            record_count++;
            continue;
        }
        
        // Process both in use and deleted files
        bool is_deleted = !(record->flags & MFT_RECORD_IN_USE);
        
        // Skip directories for now (focus on files)
        if (record->flags & MFT_RECORD_IS_DIRECTORY) {
            current_offset += record_size;
            record_count++;
            continue;
        }
        
        // If record is deleted, log it for debugging
        if (is_deleted) {
            LOG_DEBUG("Found deleted MFT record at offset " + std::to_string(current_offset));
        }
        
        auto file_entry = parse_mft_record_to_file(record, data + current_offset, boot, partition_offset);
        
        if (!file_entry.filename.empty() && file_entry.file_size > 0) {
            files.push_back(file_entry);
        }
        
        current_offset += record_size;
        record_count++;
    }
    
    LOG_DEBUG("Processed " + std::to_string(record_count) + " MFT records");
    return files;
}

RecoveredFile NtfsParser::parse_mft_record_to_file(const MftRecord* record, const uint8_t* record_data,
                                                    const NtfsBootSector* boot, uint64_t partition_offset) {
    RecoveredFile entry;
    
    // Extract filename
    entry.filename = extract_filename_attribute(record_data, record->used_size);
    
    // Extract file size
    entry.file_size = extract_file_size_attribute(record_data, record->used_size);
    
    // Extract data locations (store as fragments)
    auto data_locations = extract_data_runs(record_data, record->used_size, boot, partition_offset);
    if (!data_locations.empty()) {
        entry.start_offset = data_locations[0].first;
        entry.fragments = data_locations;
        entry.is_fragmented = data_locations.size() > 1;
    }
    
    // Check if record is marked as deleted in MFT
    bool is_deleted = !(record->flags & MFT_RECORD_IN_USE) || (record->sequence_number > 1);
    entry.confidence_score = is_deleted ? 0.7 : 0.95;
    
    // For deleted files, prefix filename with "DELETED_"
    if (is_deleted) {
        entry.filename = "DELETED_" + entry.filename;
        LOG_DEBUG("Found deleted file: " + entry.filename + ", size: " + std::to_string(entry.file_size));
    }
    
    // Set file type based on extension
    if (!entry.filename.empty()) {
        size_t dot_pos = entry.filename.find_last_of('.');
        if (dot_pos != std::string::npos) {
            entry.file_type = entry.filename.substr(dot_pos + 1);
        }
    }
    
    return entry;
}

std::string NtfsParser::extract_filename_attribute(const uint8_t* record_data, size_t record_size) {
    size_t offset = sizeof(MftRecord);
    std::string best_filename = "unknown_file";
    bool found_long_name = false;
    
    // We'll check all filename attributes and prioritize long filenames over short ones
    while (offset + sizeof(AttributeHeader) < record_size) {
        const auto* attr = reinterpret_cast<const AttributeHeader*>(record_data + offset);
        
        if (attr->type == 0xFFFFFFFF) { // End marker
            break;
        }
        
        if (attr->type == AT_FILE_NAME && attr->length > 0) {
            if (attr->non_resident_flag == 0) { // Resident attribute
                size_t value_offset = offset + attr->resident.value_offset;
                
                // Ensure we have enough space for the filename attribute header
                if (value_offset + 66 <= record_size) {
                    // Structure of $FILENAME attribute:
                    // 0-7: Parent directory reference
                    // 8-15: Creation time
                    // 16-23: Last data modification time
                    // 24-31: Last MFT modification time
                    // 32-39: Last access time
                    // 40-47: Allocated size
                    // 48-55: Real size
                    // 56-59: Flags
                    // 60-63: Reparse/EA info
                    // 64: Filename length (in characters)
                    // 65: Namespace (POSIX, Win32, DOS, Win32+DOS)
                    // 66+: Filename (Unicode)
                    
                    uint8_t name_length = record_data[value_offset + 64]; // Name length at offset 64
                    uint8_t namespace_type = record_data[value_offset + 65]; // Namespace
                    
                    if (name_length > 0 && name_length < 256 &&
                        value_offset + 66 + (name_length * 2) <= record_size) {
                        
                        // Get Unicode filename
                        const uint16_t* name_unicode = reinterpret_cast<const uint16_t*>(record_data + value_offset + 66);
                        std::string filename;
                        
                        // Convert to ASCII (UTF-8 would be better but requires more code)
                        for (int i = 0; i < name_length; i++) {
                            // Get UTF-16 character (handle byte order)
                            uint16_t unicode_char = name_unicode[i];
                            
                            // Simple conversion of printable ASCII range
                            if (unicode_char >= 32 && unicode_char < 127) {
                                filename += static_cast<char>(unicode_char);
                            } else if (unicode_char < 32) {
                                // Special characters, replace with underscore
                                filename += '_';
                            } else {
                                // For non-ASCII, use a placeholder
                                filename += '?';
                            }
                        }
                        
                        if (!filename.empty()) {
                            // Namespace type 2 or 3 is typically a Win32 long name (preferred)
                            if ((namespace_type == 2 || namespace_type == 3) && !found_long_name) {
                                best_filename = filename;
                                found_long_name = true;
                            } else if (!found_long_name) {
                                // DOS 8.3 name, use only if we haven't found a long name
                                best_filename = filename;
                            }
                        }
                    }
                }
            }
        }
        
        if (attr->length == 0) break;
        offset += attr->length;
    }
    
    return best_filename;
}

uint64_t NtfsParser::extract_file_size_attribute(const uint8_t* record_data, size_t record_size) {
    size_t offset = sizeof(MftRecord);
    
    while (offset + sizeof(AttributeHeader) < record_size) {
        const auto* attr = reinterpret_cast<const AttributeHeader*>(record_data + offset);
        
        if (attr->type == 0xFFFFFFFF) {
            break;
        }
        
        if (attr->type == AT_DATA && attr->length > 0) {
            if (attr->non_resident_flag == 0) { // Resident data
                return attr->resident.value_length;
            } else { // Non-resident data
                return attr->non_resident_data.data_size;
            }
        }
        
        if (attr->length == 0) break;
        offset += attr->length;
    }
    
    return 0;
}

std::vector<std::pair<Offset, Size>> NtfsParser::extract_data_runs(const uint8_t* record_data, size_t record_size,
                                                       const NtfsBootSector* boot, uint64_t partition_offset) {
    std::vector<std::pair<Offset, Size>> locations;
    size_t offset = sizeof(MftRecord);
    uint32_t cluster_size = get_cluster_size(boot);
    bool is_deleted = !(reinterpret_cast<const MftRecord*>(record_data)->flags & MFT_RECORD_IN_USE);
    
    // For deleted files, we may need to check more attributes
    int data_attrs_found = 0;
    
    while (offset + sizeof(AttributeHeader) < record_size) {
        const auto* attr = reinterpret_cast<const AttributeHeader*>(record_data + offset);
        
        if (attr->type == 0xFFFFFFFF) {
            break;
        }
        
        if (attr->type == AT_DATA && attr->length > 0) {
            // Log data attribute found - useful for debugging
            LOG_DEBUG("Found DATA attribute at offset " + std::to_string(offset) + 
                      ", resident: " + std::to_string(attr->non_resident_flag == 0));
            
            // Handle according to residence status
            if (attr->non_resident_flag == 0) { // Resident data
                size_t data_offset = offset + attr->resident.value_offset;
                if (data_offset + attr->resident.value_length <= record_size) {
                    LOG_DEBUG("Found resident data of size " + std::to_string(attr->resident.value_length));
                    locations.push_back({partition_offset + data_offset, attr->resident.value_length});
                    data_attrs_found++;
                }
            } else { // Non-resident data
                size_t run_list_offset = offset + attr->non_resident_data.run_list_offset;
                if (run_list_offset < record_size) {
                    LOG_DEBUG("Processing non-resident data, size: " + 
                              std::to_string(attr->non_resident_data.data_size));
                    
                    // Parse the run list
                    auto clusters = parse_data_runs(record_data + run_list_offset,
                                                  record_size - run_list_offset,
                                                  cluster_size, partition_offset);
                    
                    // Process recovered clusters
                    uint64_t remaining_size = attr->non_resident_data.data_size;
                    for (uint64_t cluster_addr : clusters) {
                        uint64_t fragment_size = std::min(static_cast<uint64_t>(cluster_size), remaining_size);
                        if (fragment_size > 0) {
                            locations.push_back({cluster_addr, fragment_size});
                            remaining_size -= fragment_size;
                        }
                    }
                    
                    data_attrs_found++;
                }
            }
            
            // For regular files, one $DATA attribute is enough
            // For deleted files, we try to get as much as possible
            if (!is_deleted || data_attrs_found >= 3) {
                break;
            }
        }
        
        // Safety check
        if (attr->length == 0) break;
        offset += attr->length;
    }
    
    if (locations.empty()) {
        LOG_DEBUG("No data runs found for file");
    } else {
        LOG_DEBUG("Found " + std::to_string(locations.size()) + 
                  " data fragments totaling " + 
                  std::to_string(std::accumulate(locations.begin(), locations.end(), 0ULL, 
                                              [](uint64_t sum, const auto& loc) { 
                                                  return sum + loc.second; 
                                              })) + 
                  " bytes");
    }
    
    return locations;
}

uint64_t NtfsParser::get_mft_offset(const NtfsBootSector* boot) const {
    uint32_t cluster_size = get_cluster_size(boot);
    return boot->mft_lcn * cluster_size;
}

uint32_t NtfsParser::get_cluster_size(const NtfsBootSector* boot) const {
    return boot->bytes_per_sector * boot->sectors_per_cluster;
}

uint32_t NtfsParser::get_mft_record_size(const NtfsBootSector* boot) const {
    if (boot->clusters_per_mft_record > 0) {
        return boot->clusters_per_mft_record * get_cluster_size(boot);
    } else {
        // Negative value indicates size as power of 2
        return 1 << (-boot->clusters_per_mft_record);
    }
}

std::vector<uint64_t> NtfsParser::parse_data_runs(const uint8_t* run_data, size_t run_length,
                                                  uint32_t cluster_size, uint64_t partition_offset) {
    std::vector<uint64_t> clusters;
    
    // Enhanced data run parsing for better handling of fragmented files
    size_t offset = 0;
    int64_t last_cluster_offset = 0;  // Keep track of last cluster offset for relative positioning
    
    while (offset < run_length && run_data[offset] != 0) {
        uint8_t header = run_data[offset++];
        if (header == 0) break;
        
        uint8_t length_bytes = header & 0x0F;
        uint8_t offset_bytes = (header >> 4) & 0x0F;
        
        // Validate byte counts to avoid buffer overruns
        if (length_bytes == 0) break;
        if (offset + length_bytes > run_length) break;
        if (offset_bytes > 0 && offset + length_bytes + offset_bytes > run_length) break;
        
        // Extract run length
        uint64_t run_clusters = 0;
        for (int i = 0; i < length_bytes; i++) {
            run_clusters |= static_cast<uint64_t>(run_data[offset + i]) << (i * 8);
        }
        offset += length_bytes;
        
        // Calculate current LCN (for non-sparse runs)
        if (offset_bytes > 0) {
            // Extract the cluster offset (handle signed value correctly for relative positioning)
            int64_t cluster_offset_value = 0;
            for (int i = 0; i < offset_bytes; i++) {
                cluster_offset_value |= static_cast<uint64_t>(run_data[offset + i]) << (i * 8);
            }
            
            // Sign extend if highest bit is set
            if (run_data[offset + offset_bytes - 1] & 0x80) {
                uint64_t sign_bits = ~0ULL << (offset_bytes * 8);
                cluster_offset_value |= sign_bits;
            }
            
            // Update the last LCN for relative addressing
            last_cluster_offset += cluster_offset_value;
            offset += offset_bytes;
            
            // For deleted files, the clusters may already be reallocated
            // but we attempt to recover them anyway
            uint64_t start_cluster_offset = partition_offset + (last_cluster_offset * cluster_size);
            
            // Add each cluster to our list
            for (uint64_t i = 0; i < run_clusters && i < 10000; i++) {
                clusters.push_back(start_cluster_offset + (i * cluster_size));
            }
            
            // Add debug information about data runs
            LOG_DEBUG("Data run: LCN=" + std::to_string(last_cluster_offset) + 
                      ", clusters=" + std::to_string(run_clusters));
        } else {
            // This is a sparse run (no disk allocation)
            LOG_DEBUG("Sparse data run found: clusters=" + std::to_string(run_clusters));
            // For sparse runs, we can't recover the data as it's not on disk
            offset += offset_bytes;
        }
        
        // Safety limit
        if (clusters.size() > 50000) {
            LOG_WARNING("Too many clusters in data run, truncating");
            break;
        }
    }
    
    return clusters;
}

} // namespace FileRecovery
