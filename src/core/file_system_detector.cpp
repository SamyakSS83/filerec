#include "core/file_system_detector.h"
#include "utils/logger.h"
#include <fstream>
#include <cstring>
#include <algorithm>

namespace FileRecovery {

// Filesystem magic numbers and signatures
constexpr uint16_t EXT_MAGIC = 0xEF53;
constexpr uint32_t NTFS_MAGIC = 0x4E544653; // "NTFS"
constexpr uint8_t FAT_SIGNATURE[] = {0x55, 0xAA};

// Offsets for filesystem detection
constexpr size_t EXT_SB_OFFSET = 1024;
constexpr size_t EXT_MAGIC_OFFSET = 56;
constexpr size_t NTFS_OEM_OFFSET = 3;
constexpr size_t FAT_SIGNATURE_OFFSET = 510;

FileSystemDetector::FileSystemDetector() = default;
FileSystemDetector::~FileSystemDetector() = default;

FileSystemInfo FileSystemDetector::detect(const std::string& device_path) {
    LOG_INFO("Detecting filesystem for: " + device_path);
    
    std::ifstream file(device_path, std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open device: " + device_path);
        return {FileSystemType::UNKNOWN, "Unknown", 0, 0, 0, 0, "", false};
    }

    // Read first few sectors for analysis
    constexpr size_t buffer_size = 8192;
    auto buffer = std::make_unique<uint8_t[]>(buffer_size);
    
    file.read(reinterpret_cast<char*>(buffer.get()), buffer_size);
    size_t bytes_read = file.gcount();
    
    if (bytes_read < 512) {
        LOG_ERROR("Insufficient data read from device");
        return {FileSystemType::UNKNOWN, "Unknown", 0, 0, 0, 0, "", false};
    }

    return detect_from_data(buffer.get(), bytes_read);
}

FileSystemInfo FileSystemDetector::detect_from_data(const uint8_t* data, size_t size, uint64_t offset) {
    if (!data || size < 512) {
        return {FileSystemType::UNKNOWN, "Unknown", 0, 0, 0, 0, "", false};
    }

    // Try to detect different filesystem types
    FileSystemType type = FileSystemType::UNKNOWN;
    
    // Check for ext2/3/4 first (most common Linux filesystems)
    if (size >= EXT_SB_OFFSET + 264) {
        type = detect_ext_filesystem(data, size);
        if (type != FileSystemType::UNKNOWN) {
            auto info = parse_ext_info(data, size, type);
            info.boot_sector_offset = offset;
            return info;
        }
    }
    
    // Check for NTFS
    type = detect_ntfs_filesystem(data, size);
    if (type != FileSystemType::UNKNOWN) {
        auto info = parse_ntfs_info(data, size);
        info.boot_sector_offset = offset;
        return info;
    }
    
    // Check for FAT filesystems
    type = detect_fat_filesystem(data, size);
    if (type != FileSystemType::UNKNOWN) {
        auto info = parse_fat_info(data, size, type);
        info.boot_sector_offset = offset;
        return info;
    }
    
    // Check for other filesystems
    type = detect_other_filesystem(data, size);
    if (type != FileSystemType::UNKNOWN) {
        return {type, get_filesystem_name(type), 4096, 0, 0, offset, "", true};
    }

    LOG_WARNING("Unknown filesystem detected");
    return {FileSystemType::UNKNOWN, "Unknown", 0, 0, 0, offset, "", false};
}

FileSystemType FileSystemDetector::detect_ext_filesystem(const uint8_t* data, size_t size) {
    if (size < EXT_SB_OFFSET + 264) return FileSystemType::UNKNOWN;
    
    const uint8_t* superblock = data + EXT_SB_OFFSET;
    uint16_t magic = *reinterpret_cast<const uint16_t*>(superblock + EXT_MAGIC_OFFSET);
    
    if (magic != EXT_MAGIC) return FileSystemType::UNKNOWN;
    
    if (!verify_ext_superblock(superblock)) return FileSystemType::UNKNOWN;
    
    // Determine ext version based on features
    uint32_t features_compat = *reinterpret_cast<const uint32_t*>(superblock + 92);
    uint32_t features_incompat = *reinterpret_cast<const uint32_t*>(superblock + 96);
    
    // Check for ext4 features
    if (features_incompat & 0x0040) { // INCOMPAT_EXTENTS
        return FileSystemType::EXT4;
    }
    
    // Check for ext3 features (journal)
    if (features_compat & 0x0004) { // COMPAT_HAS_JOURNAL
        return FileSystemType::EXT3;
    }
    
    return FileSystemType::EXT2;
}

FileSystemType FileSystemDetector::detect_fat_filesystem(const uint8_t* data, size_t size) {
    if (size < 512) return FileSystemType::UNKNOWN;
    
    // Check boot signature
    if (data[FAT_SIGNATURE_OFFSET] != FAT_SIGNATURE[0] || 
        data[FAT_SIGNATURE_OFFSET + 1] != FAT_SIGNATURE[1]) {
        return FileSystemType::UNKNOWN;
    }
    
    if (!verify_fat_boot_sector(data)) return FileSystemType::UNKNOWN;
    
    // Determine FAT type
    uint16_t bytes_per_sector = *reinterpret_cast<const uint16_t*>(data + 11);
    uint8_t sectors_per_cluster = data[13];
    uint16_t reserved_sectors = *reinterpret_cast<const uint16_t*>(data + 14);
    uint8_t num_fats = data[16];
    uint16_t root_entries = *reinterpret_cast<const uint16_t*>(data + 17);
    uint32_t total_sectors = *reinterpret_cast<const uint16_t*>(data + 19);
    if (total_sectors == 0) {
        total_sectors = *reinterpret_cast<const uint32_t*>(data + 32);
    }
    uint32_t sectors_per_fat = *reinterpret_cast<const uint16_t*>(data + 22);
    if (sectors_per_fat == 0) {
        sectors_per_fat = *reinterpret_cast<const uint32_t*>(data + 36);
    }
    
    uint32_t root_dir_sectors = ((root_entries * 32) + (bytes_per_sector - 1)) / bytes_per_sector;
    uint32_t data_sectors = total_sectors - reserved_sectors - (num_fats * sectors_per_fat) - root_dir_sectors;
    uint32_t cluster_count = data_sectors / sectors_per_cluster;
    
    if (cluster_count < 4085) {
        return FileSystemType::FAT12;
    } else if (cluster_count < 65525) {
        return FileSystemType::FAT16;
    } else {
        // Check for exFAT
        if (std::memcmp(data + 3, "EXFAT   ", 8) == 0) {
            return FileSystemType::EXFAT;
        }
        return FileSystemType::FAT32;
    }
}

FileSystemType FileSystemDetector::detect_ntfs_filesystem(const uint8_t* data, size_t size) {
    if (size < 512) return FileSystemType::UNKNOWN;
    
    // Check for NTFS OEM identifier
    if (std::memcmp(data + NTFS_OEM_OFFSET, "NTFS    ", 8) != 0) {
        return FileSystemType::UNKNOWN;
    }
    
    if (!verify_ntfs_boot_sector(data)) return FileSystemType::UNKNOWN;
    
    return FileSystemType::NTFS;
}

FileSystemType FileSystemDetector::detect_other_filesystem(const uint8_t* data, size_t size) {
    if (size < 512) return FileSystemType::UNKNOWN;
    
    // Check for HFS+
    if (size >= 1024 && std::memcmp(data + 1024, "H+", 2) == 0) {
        return FileSystemType::HFS_PLUS;
    }
    
    // Check for BTRFS
    if (size >= 65536 && std::memcmp(data + 65536 + 64, "_BHRfS_M", 8) == 0) {
        return FileSystemType::BTRFS;
    }
    
    // Check for XFS
    if (std::memcmp(data, "XFSB", 4) == 0) {
        return FileSystemType::XFS;
    }
    
    return FileSystemType::UNKNOWN;
}

FileSystemInfo FileSystemDetector::parse_ext_info(const uint8_t* data, size_t size, FileSystemType type) {
    const uint8_t* sb = data + EXT_SB_OFFSET;
    
    uint32_t block_size = 1024 << *reinterpret_cast<const uint32_t*>(sb + 24);
    uint32_t total_blocks = *reinterpret_cast<const uint32_t*>(sb + 4);
    uint32_t free_blocks = *reinterpret_cast<const uint32_t*>(sb + 12);
    
    std::string label;
    const char* volume_name = reinterpret_cast<const char*>(sb + 120);
    if (volume_name[0] != '\0') {
        label = std::string(volume_name, strnlen(volume_name, 16));
    }
    
    return {
        type,
        get_filesystem_name(type),
        static_cast<uint64_t>(block_size),
        static_cast<uint64_t>(total_blocks) * block_size,
        static_cast<uint64_t>(total_blocks - free_blocks) * block_size,
        0,
        label,
        true
    };
}

FileSystemInfo FileSystemDetector::parse_fat_info(const uint8_t* data, size_t size, FileSystemType type) {
    uint16_t bytes_per_sector = *reinterpret_cast<const uint16_t*>(data + 11);
    uint8_t sectors_per_cluster = data[13];
    uint32_t total_sectors = *reinterpret_cast<const uint16_t*>(data + 19);
    if (total_sectors == 0) {
        total_sectors = *reinterpret_cast<const uint32_t*>(data + 32);
    }
    
    uint64_t cluster_size = bytes_per_sector * sectors_per_cluster;
    uint64_t total_size = static_cast<uint64_t>(total_sectors) * bytes_per_sector;
    
    std::string label;
    if (type == FileSystemType::FAT32) {
        const char* volume_label = reinterpret_cast<const char*>(data + 71);
        if (volume_label[0] != '\0' && volume_label[0] != ' ') {
            label = std::string(volume_label, strnlen(volume_label, 11));
        }
    } else {
        const char* volume_label = reinterpret_cast<const char*>(data + 43);
        if (volume_label[0] != '\0' && volume_label[0] != ' ') {
            label = std::string(volume_label, strnlen(volume_label, 11));
        }
    }
    
    return {
        type,
        get_filesystem_name(type),
        cluster_size,
        total_size,
        0, // FAT doesn't provide used space easily
        0,
        label,
        true
    };
}

FileSystemInfo FileSystemDetector::parse_ntfs_info(const uint8_t* data, size_t size) {
    uint16_t bytes_per_sector = *reinterpret_cast<const uint16_t*>(data + 11);
    uint8_t sectors_per_cluster = data[13];
    uint64_t total_sectors = *reinterpret_cast<const uint64_t*>(data + 40);
    
    uint64_t cluster_size = bytes_per_sector * sectors_per_cluster;
    uint64_t total_size = total_sectors * bytes_per_sector;
    
    return {
        FileSystemType::NTFS,
        "NTFS",
        cluster_size,
        total_size,
        0, // NTFS used space requires MFT parsing
        0,
        "", // NTFS label requires MFT parsing
        true
    };
}

bool FileSystemDetector::verify_ext_superblock(const uint8_t* superblock) {
    // Basic sanity checks for ext superblock
    uint32_t inodes_count = *reinterpret_cast<const uint32_t*>(superblock + 0);
    uint32_t blocks_count = *reinterpret_cast<const uint32_t*>(superblock + 4);
    uint32_t block_size = 1024 << *reinterpret_cast<const uint32_t*>(superblock + 24);
    
    return inodes_count > 0 && blocks_count > 0 && 
           block_size >= 1024 && block_size <= 65536;
}

bool FileSystemDetector::verify_fat_boot_sector(const uint8_t* boot_sector) {
    // Basic sanity checks for FAT boot sector
    uint16_t bytes_per_sector = *reinterpret_cast<const uint16_t*>(boot_sector + 11);
    uint8_t sectors_per_cluster = boot_sector[13];
    
    return bytes_per_sector == 512 && sectors_per_cluster > 0 && 
           (sectors_per_cluster & (sectors_per_cluster - 1)) == 0; // Power of 2
}

bool FileSystemDetector::verify_ntfs_boot_sector(const uint8_t* boot_sector) {
    // Basic sanity checks for NTFS boot sector
    uint16_t bytes_per_sector = *reinterpret_cast<const uint16_t*>(boot_sector + 11);
    uint8_t sectors_per_cluster = boot_sector[13];
    
    return bytes_per_sector == 512 && sectors_per_cluster > 0 && 
           (sectors_per_cluster & (sectors_per_cluster - 1)) == 0; // Power of 2
}

std::string FileSystemDetector::get_filesystem_name(FileSystemType type) {
    switch (type) {
        case FileSystemType::EXT2: return "ext2";
        case FileSystemType::EXT3: return "ext3";
        case FileSystemType::EXT4: return "ext4";
        case FileSystemType::NTFS: return "NTFS";
        case FileSystemType::FAT12: return "FAT12";
        case FileSystemType::FAT16: return "FAT16";
        case FileSystemType::FAT32: return "FAT32";
        case FileSystemType::EXFAT: return "exFAT";
        case FileSystemType::BTRFS: return "Btrfs";
        case FileSystemType::XFS: return "XFS";
        case FileSystemType::HFS_PLUS: return "HFS+";
        case FileSystemType::APFS: return "APFS";
        default: return "Unknown";
    }
}

bool FileSystemDetector::supports_metadata_recovery(FileSystemType type) {
    switch (type) {
        case FileSystemType::EXT2:
        case FileSystemType::EXT3:
        case FileSystemType::EXT4:
        case FileSystemType::NTFS:
        case FileSystemType::FAT32:
            return true;
        default:
            return false;
    }
}

} // namespace FileRecovery
