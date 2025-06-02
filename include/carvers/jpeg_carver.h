#pragma once

#include "carvers/base_carver.h"

namespace FileRecovery {

/**
 * @brief JPEG file carver
 * 
 * Recovers JPEG images using signature-based detection.
 * Supports JFIF and EXIF formats with proper validation.
 */
class JpegCarver : public BaseCarver {
public:
    JpegCarver() = default;
    ~JpegCarver() override = default;
    
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
     * @brief Find the end of a JPEG file starting from offset
     * @param data Data to search in
     * @param size Size of the data
     * @param start_offset Offset where JPEG starts
     * @return Size of the JPEG file, or 0 if not found
     */
    Size findJpegEnd(const Byte* data, Size size, Offset start_offset) const;
    
    /**
     * @brief Validate JPEG file structure
     * @param data JPEG file data
     * @param size Size of the data
     * @return true if structure is valid
     */
    bool validateJpegStructure(const Byte* data, Size size) const;
    
    /**
     * @brief Extract EXIF metadata from JPEG
     * @param data JPEG file data
     * @param size Size of the data
     * @return Metadata string
     */
    std::string extractMetadata(const Byte* data, Size size) const override;
    
    /**
     * @brief Check if JPEG has valid segments
     * @param data JPEG file data
     * @param size Size of the data
     * @return true if segments are valid
     */
    bool hasValidSegments(const Byte* data, Size size) const;
    
    /**
     * @brief Estimate JPEG size from segments when no end marker found
     * @param data JPEG file data
     * @param max_size Maximum size to search
     * @return Estimated file size
     */
    Size estimateSizeFromSegments(const Byte* data, Size max_size) const;
};

} // namespace FileRecovery
