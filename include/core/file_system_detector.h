#pragma once

#include "utils/types.h"
#include <string>
#include <vector>
#include <memory>

namespace FileRecovery {

struct FileSystemInfo {
    FileSystemType type;
    std::string name;
    uint64_t cluster_size;
    uint64_t total_size;
    uint64_t used_size;
    uint64_t boot_sector_offset;
    std::string label;
    bool is_valid;
};

class FileSystemDetector {
public:
    FileSystemDetector();
    ~FileSystemDetector();

    /**
     * Detect filesystem type from device/file
     */
    FileSystemInfo detect(const std::string& device_path);

    /**
     * Detect filesystem from raw data
     */
    FileSystemInfo detect_from_data(const uint8_t* data, size_t size, uint64_t offset = 0);

    /**
     * Get filesystem name from type
     */
    static std::string get_filesystem_name(FileSystemType type);

    /**
     * Check if filesystem supports metadata recovery
     */
    static bool supports_metadata_recovery(FileSystemType type);

private:
    FileSystemType detect_ext_filesystem(const uint8_t* data, size_t size);
    FileSystemType detect_fat_filesystem(const uint8_t* data, size_t size);
    FileSystemType detect_ntfs_filesystem(const uint8_t* data, size_t size);
    FileSystemType detect_other_filesystem(const uint8_t* data, size_t size);

    FileSystemInfo parse_ext_info(const uint8_t* data, size_t size, FileSystemType type);
    FileSystemInfo parse_fat_info(const uint8_t* data, size_t size, FileSystemType type);
    FileSystemInfo parse_ntfs_info(const uint8_t* data, size_t size);

    bool verify_ext_superblock(const uint8_t* superblock);
    bool verify_fat_boot_sector(const uint8_t* boot_sector);
    bool verify_ntfs_boot_sector(const uint8_t* boot_sector);
};

} // namespace FileRecovery
