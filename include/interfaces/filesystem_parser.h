#pragma once

#include <vector>
#include <memory>
#include "utils/types.h"

namespace FileRecovery {

/**
 * @brief Base interface for file system parsers
 * 
 * This interface defines the contract for all file system parsers.
 * Each file system (ext4, NTFS, FAT32, etc.) implements this interface
 * to provide metadata-based file recovery capabilities.
 */
class FilesystemParser {
public:
    virtual ~FilesystemParser() = default;
    
    /**
     * @brief Initialize the parser with raw disk data
     * @param data Pointer to raw disk data
     * @param size Size of the data in bytes
     * @return true if initialization successful
     */
    virtual bool initialize(const Byte* data, Size size) = 0;
    
    /**
     * @brief Detect if this parser can handle the given file system
     * @param data Pointer to raw disk data
     * @param size Size of the data in bytes
     * @return true if this parser can handle the file system
     */
    virtual bool canParse(const Byte* data, Size size) const = 0;
    
    /**
     * @brief Get the file system type
     * @return The file system type this parser handles
     */
    virtual FileSystemType getFileSystemType() const = 0;
    
    /**
     * @brief Recover deleted files using file system metadata
     * @return Vector of recovered files
     */
    virtual std::vector<RecoveredFile> recoverDeletedFiles() = 0;
    
    /**
     * @brief Get file system information
     * @return String containing file system details
     */
    virtual std::string getFileSystemInfo() const = 0;
    
protected:
    const Byte* disk_data_ = nullptr;
    Size disk_size_ = 0;
};

} // namespace FileRecovery
