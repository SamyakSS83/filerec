#pragma once

#include <vector>
#include <string>
#include "utils/types.h"

namespace FileRecovery {

/**
 * @brief Base interface for file carvers
 * 
 * File carvers use signature-based recovery to identify and extract
 * files from raw disk data without relying on file system metadata.
 */
class FileCarver {
public:
    virtual ~FileCarver() = default;
    
    /**
     * @brief Get the file types this carver can handle
     * @return Vector of file extensions (e.g., "jpg", "pdf")
     */
    virtual std::vector<std::string> getSupportedTypes() const = 0;
    
    /**
     * @brief Get the file signatures this carver looks for
     * @return Vector of byte patterns that identify file headers
     */
    virtual std::vector<std::vector<Byte>> getFileSignatures() const = 0;
    
    /**
     * @brief Get the file footers (if any) for this file type
     * @return Vector of byte patterns that identify file endings
     */
    virtual std::vector<std::vector<Byte>> getFileFooters() const = 0;
    
    /**
     * @brief Carve files from a chunk of raw data
     * @param data Pointer to raw data
     * @param size Size of the data
     * @param base_offset Base offset in the disk for this chunk
     * @return Vector of recovered files found in this chunk
     */
    virtual std::vector<RecoveredFile> carveFiles(
        const Byte* data, 
        Size size, 
        Offset base_offset
    ) = 0;
    
    /**
     * @brief Validate if a recovered file is likely valid
     * @param file The recovered file to validate
     * @param data Raw data containing the file
     * @return Confidence score (0.0 - 1.0)
     */
    virtual double validateFile(const RecoveredFile& file, const Byte* data) = 0;
    
    /**
     * @brief Get maximum expected file size for this type
     * @return Maximum file size in bytes
     */
    virtual Size getMaxFileSize() const = 0;
    
protected:
    /**
     * @brief Helper function to search for byte patterns
     * @param data Data to search in
     * @param size Size of the data
     * @param pattern Pattern to search for
     * @return Vector of offsets where pattern was found
     */
    std::vector<Offset> findPattern(
        const Byte* data, 
        Size size, 
        const std::vector<Byte>& pattern
    ) const;
    
    /**
     * @brief Calculate file entropy (randomness measure)
     * @param data File data
     * @param size Size of the data
     * @return Entropy value (0.0 - 8.0)
     */
    double calculateEntropy(const Byte* data, Size size) const;
};

} // namespace FileRecovery
