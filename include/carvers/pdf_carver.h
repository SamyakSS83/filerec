#pragma once

#include "carvers/base_carver.h"

namespace FileRecovery {

/**
 * @brief PDF file carver
 * 
 * Recovers PDF documents using signature-based detection.
 * Supports various PDF versions with proper validation.
 */
class PdfCarver : public BaseCarver {
public:
    PdfCarver() = default;
    ~PdfCarver() override = default;
    
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
     * @brief Find the end of a PDF file
     * @param data Data to search in
     * @param size Size of the data
     * @param start_offset Offset where PDF starts
     * @return Size of the PDF file, or 0 if not found
     */
    Size findPdfEnd(const Byte* data, Size size, Offset start_offset) const;
    
    /**
     * @brief Validate PDF file structure
     * @param data PDF file data
     * @param size Size of the data
     * @return true if structure is valid
     */
    bool validatePdfStructure(const Byte* data, Size size) const;
    
    /**
     * @brief Extract PDF metadata
     * @param data PDF file data
     * @param size Size of the data
     * @return Metadata string
     */
    std::string extractMetadata(const Byte* data, Size size) const override;
    
    /**
     * @brief Check for PDF trailer
     * @param data PDF file data
     * @param size Size of the data
     * @return true if valid trailer found
     */
    bool hasValidTrailer(const Byte* data, Size size) const;
    
    /**
     * @brief Estimate PDF size when no clear end marker found
     * @param data PDF file data
     * @param max_size Maximum size to search
     * @return Estimated file size
     */
    Size estimatePdfSize(const Byte* data, Size max_size) const;
};

} // namespace FileRecovery
