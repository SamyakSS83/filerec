#pragma once

#include "carvers/base_carver.h"

namespace FileRecovery {

/**
 * @brief PNG file carver
 * 
 * Recovers PNG images using signature-based detection.
 * Supports proper PNG structure validation.
 */
class PngCarver : public BaseCarver {
public:
    PngCarver() = default;
    ~PngCarver() override = default;
    
    std::vector<std::string> getSupportedTypes() const override;
    std::vector<std::vector<Byte>> getFileSignatures() const override;
    std::vector<std::vector<Byte>> getFileFooters() const override;
    
    std::vector<RecoveredFile> carveFiles(
        const Byte* data, 
        Size size, 
        Offset base_offset
    ) override;
    
    double validateFile(const RecoveredFile& file, const Byte* data) override;
    Size getMaxFileSize() const override;
    
private:
    /**
     * @brief Find the end of a PNG file
     * @param data Data to search in
     * @param size Size of the data
     * @param start_offset Offset where PNG starts
     * @return Size of the PNG file, or 0 if not found
     */
    Size findPngEnd(const Byte* data, Size size, Offset start_offset) const;
    
    /**
     * @brief Validate PNG file structure
     * @param data PNG file data
     * @param size Size of the data
     * @return true if structure is valid
     */
    bool validatePngStructure(const Byte* data, Size size) const;
    
    /**
     * @brief Extract PNG metadata
     * @param data PNG file data
     * @param size Size of the data
     * @return Metadata string
     */
    std::string extractMetadata(const Byte* data, Size size) const override;
    
    /**
     * @brief Check PNG chunks for validity
     * @param data PNG file data
     * @param size Size of the data
     * @return true if chunks are valid
     */
    bool hasValidChunks(const Byte* data, Size size) const;
    
    /**
     * @brief Check if PNG data contains a valid IEND chunk
     * @param data PNG file data
     * @param size Size of the data
     * @return true if a valid IEND chunk is found
     */
    bool hasValidIendChunk(const Byte* data, Size size) const;
    
    /**
     * @brief Calculate CRC32 for PNG chunk validation
     * @param data Data for CRC calculation
     * @param length Length of data
     * @return CRC32 value
     */
    uint32_t calculateCRC32(const Byte* data, Size length) const;
};

} // namespace FileRecovery
