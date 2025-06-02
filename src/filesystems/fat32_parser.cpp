#include "filesystems/fat32_parser.h"
#include "utils/logger.h"
#include <cstring>
#include <algorithm>
#include <cctype>

namespace FileRecovery {

Fat32Parser::Fat32Parser() = default;

bool Fat32Parser::initialize(const Byte* data, Size size) {
    disk_data_ = data;
    disk_size_ = size;
    return canParse(data, size);
}

bool Fat32Parser::canParse(const Byte* data, Size size) const {
    if (size < sizeof(Fat32BootSector)) {
        return false;
    }
    
    const auto* boot = reinterpret_cast<const Fat32BootSector*>(data);
    return validate_boot_sector(boot);
}

std::vector<RecoveredFile> Fat32Parser::recoverDeletedFiles() {
    if (!disk_data_ || disk_size_ == 0) {
        LOG_ERROR("FAT32 parser not initialized");
        return {};
    }
    
    LOG_INFO("Parsing FAT32 filesystem metadata");
    
    const auto* boot = reinterpret_cast<const Fat32BootSector*>(disk_data_);
    auto files = parse_directory_entries(disk_data_, disk_size_, boot, 0);
    
    LOG_INFO("Found " + std::to_string(files.size()) + " files in FAT32 filesystem");
    return files;
}

std::string Fat32Parser::getFileSystemInfo() const {
    return "FAT32 File System";
}

bool Fat32Parser::validate_boot_sector(const Fat32BootSector* boot) const {
    // Check bootable partition signature
    if (boot->bootable_partition_signature != 0xAA55) {
        return false;
    }
    
    // Check bytes per sector
    if (boot->bytes_per_sector != 512) {
        return false;
    }
    
    // Check sectors per cluster (must be power of 2)
    uint8_t spc = boot->sectors_per_cluster;
    if (spc == 0 || (spc & (spc - 1)) != 0) {
        return false;
    }
    
    // FAT32 specific checks
    if (boot->table_size_16 != 0) { // Should be 0 for FAT32
        return false;
    }
    
    if (boot->table_size_32 == 0) {
        return false;
    }
    
    if (boot->root_cluster < 2) {
        return false;
    }
    
    // Check FAT type label
    if (std::memcmp(boot->fat_type_label, "FAT32   ", 8) != 0) {
        return false;
    }
    
    return true;
}

std::vector<RecoveredFile> Fat32Parser::parse_directory_entries(const uint8_t* data, size_t size,
                                                           const Fat32BootSector* boot, uint64_t partition_offset) {
    std::vector<RecoveredFile> files;
    
    uint64_t data_offset = get_data_offset(boot);
    uint32_t cluster_size = get_cluster_size(boot);
    uint64_t fat_offset = get_fat_offset(boot);
    
    if (data_offset >= size || fat_offset >= size) {
        LOG_ERROR("FAT32 data or FAT offset beyond data size");
        return files;
    }
    
    const uint8_t* fat_table = data + fat_offset;
    
    // Start with root directory
    std::vector<uint32_t> clusters_to_process = {boot->root_cluster};
    std::vector<LongNameEntry> lfn_entries;
    
    while (!clusters_to_process.empty()) {
        uint32_t cluster = clusters_to_process.back();
        clusters_to_process.pop_back();
        
        if (!is_valid_cluster(cluster)) {
            continue;
        }
        
        uint64_t cluster_offset = cluster_to_sector(cluster, boot) * boot->bytes_per_sector;
        if (cluster_offset >= size) {
            continue;
        }
        
        // Process directory entries in this cluster
        for (size_t i = 0; i < cluster_size; i += sizeof(Fat32DirEntry)) {
            if (cluster_offset + i + sizeof(Fat32DirEntry) > size) {
                break;
            }
            
            const auto* entry = reinterpret_cast<const Fat32DirEntry*>(data + cluster_offset + i);
            
            // Check for end of directory
            if (entry->filename[0] == 0x00) {
                break;
            }
            
            // Skip deleted entries for now
            if (static_cast<uint8_t>(entry->filename[0]) == 0xE5) {
                lfn_entries.clear();
                continue;
            }
            
            // Skip volume label entries
            if (entry->attributes & ATTR_VOLUME_ID) {
                continue;
            }
            
            // Handle long filename entries
            if (entry->attributes == ATTR_LONG_NAME) {
                const auto* lfn = reinterpret_cast<const LongNameEntry*>(entry);
                lfn_entries.push_back(*lfn);
                continue;
            }
            
            // Regular directory entry
            std::string long_name;
            if (!lfn_entries.empty()) {
                long_name = extract_long_name(lfn_entries);
                lfn_entries.clear();
            }
            
            auto file_entry = parse_dir_entry_to_file(entry, long_name, boot, partition_offset);
            
            if (!file_entry.filename.empty() && file_entry.file_size > 0) {
                files.push_back(file_entry);
            }
            
            // If this is a directory, add it to processing queue
            if ((entry->attributes & ATTR_DIRECTORY) && 
                file_entry.filename != "." && file_entry.filename != "..") {
                uint32_t dir_cluster = (static_cast<uint32_t>(entry->first_cluster_high) << 16) | 
                                      entry->first_cluster_low;
                if (is_valid_cluster(dir_cluster)) {
                    clusters_to_process.push_back(dir_cluster);
                }
            }
        }
        
        // Get next cluster in chain
        uint32_t next_cluster = fat_entry_value(fat_table, cluster);
        if (is_valid_cluster(next_cluster) && next_cluster < EOC_MARK) {
            clusters_to_process.push_back(next_cluster);
        }
        
        // Limit processing to avoid infinite loops
        if (files.size() > 100000) {
            break;
        }
    }
    
    return files;
}

std::vector<RecoveredFile> Fat32Parser::parse_deleted_entries(const uint8_t* data, size_t size,
                                                         const Fat32BootSector* boot, uint64_t partition_offset) {
    std::vector<RecoveredFile> files;
    
    uint64_t data_offset = get_data_offset(boot);
    uint32_t cluster_size = get_cluster_size(boot);
    
    if (data_offset >= size) {
        return files;
    }
    
    // Scan data area for deleted directory entries
    for (uint64_t offset = data_offset; offset + sizeof(Fat32DirEntry) < size; offset += cluster_size) {
        for (size_t i = 0; i < cluster_size; i += sizeof(Fat32DirEntry)) {
            if (offset + i + sizeof(Fat32DirEntry) > size) {
                break;
            }
            
            const auto* entry = reinterpret_cast<const Fat32DirEntry*>(data + offset + i);
            
            // Look for deleted entries (first byte is 0xE5)
            if (static_cast<uint8_t>(entry->filename[0]) != 0xE5) {
                continue;
            }
            
            // Skip if it's a long filename entry
            if (entry->attributes == ATTR_LONG_NAME) {
                continue;
            }
            
            // Skip volume labels and directories
            if (entry->attributes & (ATTR_VOLUME_ID | ATTR_DIRECTORY)) {
                continue;
            }
            
            // Check if it looks like a valid deleted file entry
            if (entry->file_size > 0 && entry->file_size < (1ULL << 32)) {
                auto file_entry = parse_dir_entry_to_file(entry, "", boot, partition_offset);
                file_entry.filename = "deleted_" + file_entry.filename;
                file_entry.confidence_score = 60.0; // Lower confidence for deleted files
                
                if (!file_entry.filename.empty()) {
                    files.push_back(file_entry);
                }
            }
        }
        
        // Limit processing
        if (files.size() > 10000) {
            break;
        }
    }
    
    return files;
}

RecoveredFile Fat32Parser::parse_dir_entry_to_file(const Fat32DirEntry* entry, const std::string& long_name,
                                                    const Fat32BootSector* boot, uint64_t partition_offset) {
    RecoveredFile file_entry;
    
    // Use long name if available, otherwise short name
    file_entry.filename = long_name.empty() ? extract_short_name(entry) : long_name;
    file_entry.file_size = entry->file_size;
    file_entry.file_type = determine_file_type(file_entry.filename);
    file_entry.confidence_score = 85.0; // High confidence for directory entries
    
    // Get cluster chain
    uint32_t first_cluster = (static_cast<uint32_t>(entry->first_cluster_high) << 16) | 
                            entry->first_cluster_low;
    
    if (is_valid_cluster(first_cluster)) {
        uint64_t fat_offset = get_fat_offset(boot);
        if (fat_offset < SIZE_MAX) { // Ensure it fits in size_t for array access
            // For deleted files, we can't rely on FAT chain, so just use first cluster
            uint64_t cluster_offset = cluster_to_sector(first_cluster, boot) * boot->bytes_per_sector;
            file_entry.start_offset = partition_offset + cluster_offset;
            file_entry.fragments.push_back({partition_offset + cluster_offset, get_cluster_size(boot)});
        }
    }
    
    return file_entry;
}

std::string Fat32Parser::extract_short_name(const Fat32DirEntry* entry) const {
    std::string name;
    
    // Extract filename (8 characters)
    for (int i = 0; i < 8; i++) {
        if (entry->filename[i] != ' ') {
            name += static_cast<char>(std::tolower(entry->filename[i]));
        }
    }
    
    // Extract extension (3 characters)
    std::string ext;
    for (int i = 8; i < 11; i++) {
        if (entry->filename[i] != ' ') {
            ext += static_cast<char>(std::tolower(entry->filename[i]));
        }
    }
    
    if (!ext.empty()) {
        name += "." + ext;
    }
    
    return name;
}

std::string Fat32Parser::extract_long_name(const std::vector<LongNameEntry>& lfn_entries) const {
    if (lfn_entries.empty()) {
        return "";
    }
    
    std::string name;
    
    // Process LFN entries in reverse order
    for (auto it = lfn_entries.rbegin(); it != lfn_entries.rend(); ++it) {
        const auto& lfn = *it;
        
        // Extract characters from name fields
        for (int i = 0; i < 5; i++) {
            uint16_t ch = lfn.name1[i];
            if (ch == 0 || ch == 0xFFFF) break;
            if (ch < 128) name += static_cast<char>(ch);
        }
        
        for (int i = 0; i < 6; i++) {
            uint16_t ch = lfn.name2[i];
            if (ch == 0 || ch == 0xFFFF) break;
            if (ch < 128) name += static_cast<char>(ch);
        }
        
        for (int i = 0; i < 2; i++) {
            uint16_t ch = lfn.name3[i];
            if (ch == 0 || ch == 0xFFFF) break;
            if (ch < 128) name += static_cast<char>(ch);
        }
    }
    
    return name;
}

uint64_t Fat32Parser::cluster_to_sector(uint32_t cluster, const Fat32BootSector* boot) const {
    return get_data_offset(boot) / boot->bytes_per_sector + 
           (cluster - 2) * boot->sectors_per_cluster;
}

uint64_t Fat32Parser::get_fat_offset(const Fat32BootSector* boot) const {
    return boot->reserved_sector_count * boot->bytes_per_sector;
}

uint64_t Fat32Parser::get_data_offset(const Fat32BootSector* boot) const {
    return get_fat_offset(boot) + boot->table_count * boot->table_size_32 * boot->bytes_per_sector;
}

uint32_t Fat32Parser::get_cluster_size(const Fat32BootSector* boot) const {
    return boot->sectors_per_cluster * boot->bytes_per_sector;
}

bool Fat32Parser::is_valid_cluster(uint32_t cluster) const {
    return cluster >= 2 && cluster < EOC_MARK;
}

uint32_t Fat32Parser::fat_entry_value(const uint8_t* fat_table, uint32_t cluster) const {
    uint32_t offset = cluster * 4;
    return *reinterpret_cast<const uint32_t*>(fat_table + offset) & 0x0FFFFFFF;
}

time_t Fat32Parser::fat_time_to_unix(uint16_t time, uint16_t date) const {
    if (date == 0) return 0;
    
    struct tm tm_time = {};
    tm_time.tm_year = ((date >> 9) & 0x7F) + 80; // Years since 1980 -> years since 1900
    tm_time.tm_mon = ((date >> 5) & 0x0F) - 1;   // Month (0-11)
    tm_time.tm_mday = date & 0x1F;               // Day (1-31)
    tm_time.tm_hour = (time >> 11) & 0x1F;       // Hour (0-23)
    tm_time.tm_min = (time >> 5) & 0x3F;         // Minute (0-59)
    tm_time.tm_sec = (time & 0x1F) * 2;          // Second (0-58, 2-second resolution)
    
    return mktime(&tm_time);
}

std::string Fat32Parser::determine_file_type(const std::string& filename) {
    size_t dot_pos = filename.find_last_of('.');
    if (dot_pos != std::string::npos && dot_pos < filename.length() - 1) {
        std::string extension = filename.substr(dot_pos + 1);
        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
        return extension;
    }
    return "unknown";
}

} // namespace FileRecovery
