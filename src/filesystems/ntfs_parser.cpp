#include "filesystems/ntfs_parser.h"
#include "utils/logger.h"
#include <cstring>
#include <algorithm>

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
        
        // Skip if record is not in use
        if (!(record->flags & MFT_RECORD_IN_USE)) {
            current_offset += record_size;
            record_count++;
            continue;
        }
        
        // Skip directories for now (focus on files)
        if (record->flags & MFT_RECORD_IS_DIRECTORY) {
            current_offset += record_size;
            record_count++;
            continue;
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
    
    // Set confidence based on deletion status
    bool is_deleted = (record->sequence_number > 1); // Rough heuristic
    entry.confidence_score = is_deleted ? 0.7 : 0.95;
    
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
    
    while (offset + sizeof(AttributeHeader) < record_size) {
        const auto* attr = reinterpret_cast<const AttributeHeader*>(record_data + offset);
        
        if (attr->type == 0xFFFFFFFF) { // End marker
            break;
        }
        
        if (attr->type == AT_FILE_NAME && attr->length > 0) {
            if (attr->non_resident_flag == 0) { // Resident attribute
                size_t value_offset = offset + attr->resident.value_offset;
                if (value_offset + 66 <= record_size) { // Minimum filename attribute size
                    // Skip the filename attribute header (66 bytes) and get the name
                    const uint16_t* name_unicode = reinterpret_cast<const uint16_t*>(record_data + value_offset + 66);
                    uint8_t name_length = record_data[value_offset + 64]; // Name length at offset 64
                    
                    if (name_length > 0 && name_length < 256) {
                        std::string filename;
                        for (int i = 0; i < name_length; i++) {
                            char c = static_cast<char>(name_unicode[i] & 0xFF);
                            if (c >= 32 && c < 127) { // Printable ASCII
                                filename += c;
                            }
                        }
                        if (!filename.empty()) {
                            return filename;
                        }
                    }
                }
            }
        }
        
        if (attr->length == 0) break;
        offset += attr->length;
    }
    
    return "unknown_file";
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
    
    while (offset + sizeof(AttributeHeader) < record_size) {
        const auto* attr = reinterpret_cast<const AttributeHeader*>(record_data + offset);
        
        if (attr->type == 0xFFFFFFFF) {
            break;
        }
        
        if (attr->type == AT_DATA && attr->length > 0) {
            if (attr->non_resident_flag == 0) { // Resident data
                size_t data_offset = offset + attr->resident.value_offset;
                if (data_offset < record_size) {
                    locations.push_back({partition_offset + data_offset, attr->resident.value_length});
                }
            } else { // Non-resident data
                size_t run_list_offset = offset + attr->non_resident_data.run_list_offset;
                if (run_list_offset < record_size) {
                    auto clusters = parse_data_runs(record_data + run_list_offset,
                                                  record_size - run_list_offset,
                                                  cluster_size, partition_offset);
                    for (uint64_t cluster_addr : clusters) {
                        locations.push_back({cluster_addr, cluster_size});
                    }
                }
            }
            break; // Found data attribute
        }
        
        if (attr->length == 0) break;
        offset += attr->length;
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
    
    // Simplified data run parsing
    // In a full implementation, this would properly decode the run list format
    size_t offset = 0;
    
    while (offset < run_length && run_data[offset] != 0) {
        uint8_t header = run_data[offset++];
        if (header == 0) break;
        
        uint8_t length_bytes = header & 0x0F;
        uint8_t offset_bytes = (header >> 4) & 0x0F;
        
        if (length_bytes == 0 || offset_bytes == 0) break;
        if (offset + length_bytes + offset_bytes > run_length) break;
        
        // Extract run length (simplified)
        uint64_t run_clusters = 0;
        for (int i = 0; i < length_bytes; i++) {
            run_clusters |= static_cast<uint64_t>(run_data[offset + i]) << (i * 8);
        }
        offset += length_bytes;
        
        // Extract cluster offset (simplified)
        int64_t cluster_offset = 0;
        for (int i = 0; i < offset_bytes; i++) {
            cluster_offset |= static_cast<int64_t>(run_data[offset + i]) << (i * 8);
        }
        offset += offset_bytes;
        
        // Add clusters to result
        uint64_t start_cluster = partition_offset + cluster_offset * cluster_size;
        for (uint64_t i = 0; i < run_clusters && i < 1000; i++) {
            clusters.push_back(start_cluster + i * cluster_size);
        }
        
        // Limit number of runs processed
        if (clusters.size() > 10000) break;
    }
    
    return clusters;
}

} // namespace FileRecovery
