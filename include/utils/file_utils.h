#pragma once

#include <string>
#include <vector>
#include <chrono>
#include "utils/types.h"

namespace FileRecovery {

/**
 * @brief Utility functions for file operations
 */
class FileUtils {
public:
    /**
     * @brief Calculate SHA-256 hash of data
     * @param data Pointer to data
     * @param size Size of data
     * @return Hex string representation of hash
     */
    static std::string calculateSHA256(const Byte* data, Size size);
    
    /**
     * @brief Get file extension from filename
     * @param filename Filename to analyze
     * @return File extension (without dot)
     */
    static std::string getFileExtension(const std::string& filename);
    
    /**
     * @brief Format file size for human reading
     * @param size Size in bytes
     * @return Formatted string (e.g., "1.5 MB")
     */
    static std::string formatFileSize(Size size);
    
    /**
     * @brief Format duration for human reading
     * @param duration Duration to format
     * @return Formatted string (e.g., "2m 30s")
     */
    static std::string formatDuration(std::chrono::duration<double> duration);
    
    /**
     * @brief Check if directory exists and is writable
     * @param path Directory path
     * @return true if directory is accessible
     */
    static bool isDirectoryWritable(const std::string& path);
    
    /**
     * @brief Get available disk space in bytes
     * @param path Directory path to check
     * @return Available space in bytes
     */
    static Size getAvailableSpace(const std::string& path);
    
    /**
     * @brief Create directory recursively
     * @param path Directory path to create
     * @return true if successful
     */
    static bool createDirectory(const std::string& path);
    
    /**
     * @brief Generate unique filename if file already exists
     * @param base_path Base file path
     * @return Unique file path
     */
    static std::string generateUniqueFilename(const std::string& base_path);
};

} // namespace FileRecovery
