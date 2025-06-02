#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>

namespace FileRecovery {

// Type definitions
using Byte = uint8_t;
using Sector = uint64_t;
using Offset = uint64_t;
using Size = uint64_t;

// Constants
constexpr Size SECTOR_SIZE = 512;
constexpr Size BLOCK_SIZE_4K = 4096;
constexpr Size DEFAULT_CHUNK_SIZE = 1024 * 1024; // 1MB chunks
constexpr Size MAX_FILE_SIZE = 1ULL << 32; // 4GB max file size

// Recovery result structure
struct RecoveredFile {
    std::string filename;
    std::string file_type;
    Offset start_offset;
    Size file_size;
    double confidence_score;
    std::string hash_sha256;
    bool is_fragmented;
    std::vector<std::pair<Offset, Size>> fragments;
    
    RecoveredFile() : start_offset(0), file_size(0), confidence_score(0.0), is_fragmented(false) {}
};

// Scan configuration
struct ScanConfig {
    std::string device_path;
    std::string output_directory;
    std::vector<std::string> target_file_types;
    bool use_metadata_recovery;
    bool use_signature_recovery;
    size_t num_threads;
    Size chunk_size;
    bool verbose_logging;
    
    ScanConfig() : 
        use_metadata_recovery(true), 
        use_signature_recovery(true),
        num_threads(0), // 0 = auto-detect
        chunk_size(DEFAULT_CHUNK_SIZE),
        verbose_logging(false) {}
};

// File system types
enum class FileSystemType {
    UNKNOWN,
    EXT2,
    EXT3,
    EXT4,
    NTFS,
    FAT12,
    FAT16,
    FAT32,
    EXFAT,
    BTRFS,
    XFS,
    HFS_PLUS,
    APFS,
    RAW
};

// Recovery status
enum class RecoveryStatus {
    SUCCESS,
    PARTIAL_SUCCESS,
    FAILED,
    ACCESS_DENIED,
    DEVICE_NOT_FOUND,
    INSUFFICIENT_SPACE
};

} // namespace FileRecovery
