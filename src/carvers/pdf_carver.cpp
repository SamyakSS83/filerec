#include "carvers/pdf_carver.h"
#include "utils/logger.h"
#include <algorithm>
#include <string>
#include <cstring>

namespace FileRecovery {

std::vector<std::string> PdfCarver::getSupportedTypes() const {
    return {"pdf"};
}

std::vector<std::vector<Byte>> PdfCarver::getFileSignatures() const {
    return {
        {0x25, 0x50, 0x44, 0x46, 0x2D} // "%PDF-"
    };
}

std::vector<std::vector<Byte>> PdfCarver::getFileFooters() const {
    return {
        {0x25, 0x25, 0x45, 0x4F, 0x46}, // "%%EOF"
        {0x0A, 0x25, 0x25, 0x45, 0x4F, 0x46}, // "\n%%EOF"
        {0x0D, 0x0A, 0x25, 0x25, 0x45, 0x4F, 0x46} // "\r\n%%EOF"
    };
}

std::vector<RecoveredFile> PdfCarver::carveFiles(
    const Byte* data, 
    Size size, 
    Offset base_offset
) {
    std::vector<RecoveredFile> recovered_files;
    
    if (size < 20) { // Minimum PDF size
        return recovered_files;
    }
    
    // Search for PDF signatures
    auto signatures = getFileSignatures();
    for (const auto& signature : signatures) {
        auto matches = findPattern(data, size, signature);
        
        for (Offset match_offset : matches) {
            // Find the end of this PDF
            Size pdf_size = findPdfEnd(data, size, match_offset);
            if (pdf_size == 0 || pdf_size < 100) { // Skip tiny files
                continue;
            }
            
            // Create recovered file entry
            RecoveredFile file;
            file.filename = generateFilename(base_offset + match_offset, "pdf");
            file.file_type = "PDF";
            file.start_offset = base_offset + match_offset;
            file.file_size = pdf_size;
            file.is_fragmented = false;
            
            // Validate and calculate confidence
            file.confidence_score = validateFile(file, data + match_offset);
            
            if (file.confidence_score > 0.3) { // Only include files with reasonable confidence
                recovered_files.push_back(file);
                LOG_DEBUG("Found PDF at offset " + std::to_string(file.start_offset) + 
                         ", size: " + std::to_string(file.file_size) + 
                         ", confidence: " + std::to_string(file.confidence_score));
            }
        }
    }
    
    return recovered_files;
}

double PdfCarver::validateFile(const RecoveredFile& file, const Byte* data) {
    if (file.file_size < 20) {
        return 0.0;
    }
    
    bool has_valid_header = false;
    bool has_valid_footer = false;
    bool structure_valid = false;
    
    // Check header
    auto signatures = getFileSignatures();
    for (const auto& signature : signatures) {
        if (file.file_size >= signature.size() && 
            std::equal(signature.begin(), signature.end(), data)) {
            has_valid_header = true;
            break;
        }
    }
    
    // Check footer
    has_valid_footer = hasValidTrailer(data, file.file_size);
    
    // Validate structure
    structure_valid = validatePdfStructure(data, file.file_size);
    
    // Calculate entropy
    double entropy = calculateEntropy(data, std::min(file.file_size, Size(4096)));
    
    return calculateConfidenceScore(has_valid_header, has_valid_footer, entropy, structure_valid);
}

Size PdfCarver::getMaxFileSize() const {
    return 1ULL * 1024 * 1024 * 1024; // 1GB max for PDF
}

Size PdfCarver::findPdfEnd(const Byte* data, Size size, Offset start_offset) const {
    if (start_offset + 20 >= size) {
        return 0;
    }
    
    // Look for PDF end markers
    auto footers = getFileFooters();
    
    // Search backwards from a reasonable endpoint to find the last %%EOF
    Size search_end = std::min(size, start_offset + getMaxFileSize());
    
    for (Size i = search_end - 1; i > start_offset + 20; --i) {
        for (const auto& footer : footers) {
            if (i >= footer.size() && 
                std::equal(footer.begin(), footer.end(), data + i - footer.size() + 1)) {
                return i - start_offset + 1;
            }
        }
    }
    
    // If no footer found, try to estimate based on structure
    return estimatePdfSize(data + start_offset, size - start_offset);
}

bool PdfCarver::validatePdfStructure(const Byte* data, Size size) const {
    if (size < 20) {
        return false;
    }
    
    // Check for PDF version in header
    std::string header(reinterpret_cast<const char*>(data), std::min(size, Size(20)));
    if (header.find("%PDF-1.") != 0) {
        return false;
    }
    
    // Look for essential PDF objects
    std::string content(reinterpret_cast<const char*>(data), std::min(size, Size(4096)));
    bool has_objects = content.find(" obj") != std::string::npos;
    bool has_xref = content.find("xref") != std::string::npos || 
                   content.find("/XRef") != std::string::npos;
    
    return has_objects;
}

std::string PdfCarver::extractMetadata(const Byte* data, Size size) const {
    if (size < 20) {
        return "";
    }
    
    std::string metadata = "PDF";
    
    // Extract PDF version
    std::string header(reinterpret_cast<const char*>(data), std::min(size, Size(20)));
    size_t version_pos = header.find("%PDF-");
    if (version_pos != std::string::npos && version_pos + 8 < header.length()) {
        metadata += " v" + header.substr(version_pos + 5, 3);
    }
    
    // Look for title in first part of file
    std::string content(reinterpret_cast<const char*>(data), std::min(size, Size(2048)));
    size_t title_pos = content.find("/Title");
    if (title_pos != std::string::npos) {
        metadata += " (with metadata)";
    }
    
    return metadata;
}

bool PdfCarver::hasValidTrailer(const Byte* data, Size size) const {
    if (size < 10) {
        return false;
    }
    
    auto footers = getFileFooters();
    
    // Check last part of file for %%EOF
    Size check_size = std::min(size, Size(1024));
    const Byte* end_data = data + size - check_size;
    
    for (const auto& footer : footers) {
        auto matches = findPattern(end_data, check_size, footer);
        if (!matches.empty()) {
            return true;
        }
    }
    
    return false;
}

Size PdfCarver::estimatePdfSize(const Byte* data, Size max_size) const {
    // Look for last occurrence of PDF objects or references
    std::string content(reinterpret_cast<const char*>(data), std::min(max_size, Size(32768)));
    
    // Find last meaningful PDF content
    size_t last_obj = content.rfind(" obj");
    size_t last_endobj = content.rfind("endobj");
    size_t last_stream = content.rfind("endstream");
    
    Size estimated_end = std::max({last_obj, last_endobj, last_stream});
    if (estimated_end != std::string::npos) {
        return estimated_end + 100; // Add some padding
    }
    
    // Fallback: use a reasonable default size
    return std::min(max_size, Size(10 * 1024 * 1024)); // 10MB default
}

} // namespace FileRecovery
