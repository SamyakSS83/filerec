#include "carvers/jpeg_carver.h"
#include "utils/logger.h"
#include <algorithm>

namespace FileRecovery {

std::vector<std::string> JpegCarver::getSupportedTypes() const {
    return {"JPEG", "JPG"}; // Change to uppercase to match test expectations
}

std::vector<std::vector<Byte>> JpegCarver::getFileSignatures() const {
    return {
        {0xFF, 0xD8, 0xFF, 0xE0}, // JFIF
        {0xFF, 0xD8, 0xFF, 0xE1}, // EXIF
        {0xFF, 0xD8, 0xFF, 0xDB}  // Raw JPEG
    };
}

std::vector<std::vector<Byte>> JpegCarver::getFileFooters() const {
    return {
        {0xFF, 0xD9} // JPEG end marker
    };
}

std::vector<RecoveredFile> JpegCarver::carveFiles(
    const Byte* data, 
    Size size, 
    Offset base_offset
) {
    std::vector<RecoveredFile> recovered_files;
    
    if (size < 10) { // Minimum JPEG size
        return recovered_files;
    }
    
    // Search for JPEG signatures
    auto signatures = getFileSignatures();
    for (const auto& signature : signatures) {
        auto matches = findPattern(data, size, signature);
        
        for (Offset match_offset : matches) {
            // Find the end of this JPEG
            Size jpeg_size = findJpegEnd(data, size, match_offset);
            if (jpeg_size == 0 || jpeg_size < 100) { // Skip tiny files
                continue;
            }
            
            // Create recovered file entry
            RecoveredFile file;
            file.filename = generateFilename(base_offset + match_offset, "jpg");
            file.file_type = "JPEG";
            file.start_offset = base_offset + match_offset;
            file.file_size = jpeg_size;
            file.is_fragmented = false;
            
            // Validate and calculate confidence
            file.confidence_score = validateFile(file, data + match_offset);
            
            if (file.confidence_score > 0.3) { // Only include files with reasonable confidence
                recovered_files.push_back(file);
                LOG_DEBUG("Found JPEG at offset " + std::to_string(file.start_offset) + 
                         ", size: " + std::to_string(file.file_size) + 
                         ", confidence: " + std::to_string(file.confidence_score));
            }
        }
    }
    
    return recovered_files;
}

double JpegCarver::validateFile(const RecoveredFile& file, const Byte* data) {
    if (file.file_size < 10) {
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
    if (file.file_size >= 2) {
        if (data[file.file_size - 2] == 0xFF && data[file.file_size - 1] == 0xD9) {
            has_valid_footer = true;
        }
    }
    
    // Validate structure
    structure_valid = validateJpegStructure(data, file.file_size);
    
    // Calculate entropy
    double entropy = calculateEntropy(data, std::min(file.file_size, Size(4096)));
    
    return calculateConfidenceScore(has_valid_header, has_valid_footer, entropy, structure_valid);
}

Size JpegCarver::getMaxFileSize() const {
    return 100 * 1024 * 1024; // 100MB max for JPEG
}

Size JpegCarver::findJpegEnd(const Byte* data, Size size, Offset start_offset) const {
    if (start_offset + 10 >= size) {
        return 0;
    }
    
    // Look for the JPEG end marker (0xFF 0xD9)
    for (Size i = start_offset + 10; i < size - 1; ++i) {
        if (data[i] == 0xFF && data[i + 1] == 0xD9) {
            return i - start_offset + 2; // Include the end marker
        }
        
        // Safety check - don't search beyond reasonable JPEG size
        if (i - start_offset > getMaxFileSize()) {
            break;
        }
    }
    
    // If no end marker found, try to estimate size based on segments
    return estimateSizeFromSegments(data + start_offset, size - start_offset);
}

bool JpegCarver::validateJpegStructure(const Byte* data, Size size) const {
    if (size < 10) {
        return false;
    }
    
    // Check for valid JPEG markers
    return hasValidSegments(data, size);
}

std::string JpegCarver::extractMetadata(const Byte* data, Size size) const {
    if (size < 20) {
        return "";
    }
    
    std::string metadata = "JPEG";
    
    // Check for EXIF data
    if (size >= 14 && data[6] == 'E' && data[7] == 'x' && data[8] == 'i' && data[9] == 'f') {
        metadata += " with EXIF";
    }
    
    // Extract basic dimensions if possible (simplified)
    // Real EXIF parsing would be much more complex
    for (Size i = 0; i < std::min(size, Size(1024)) - 4; ++i) {
        if (data[i] == 0xFF && (data[i + 1] == 0xC0 || data[i + 1] == 0xC2)) {
            // Found SOF (Start of Frame) marker
            if (i + 9 < size) {
                uint16_t height = (data[i + 5] << 8) | data[i + 6];
                uint16_t width = (data[i + 7] << 8) | data[i + 8];
                metadata += " (" + std::to_string(width) + "x" + std::to_string(height) + ")";
                break;
            }
        }
    }
    
    return metadata;
}

bool JpegCarver::hasValidSegments(const Byte* data, Size size) const {
    if (size < 4) {
        return false;
    }
    
    Size offset = 2; // Skip initial 0xFF 0xD8
    int segment_count = 0;
    
    while (offset < size - 1 && segment_count < 100) { // Limit segments to prevent infinite loops
        if (data[offset] != 0xFF) {
            break;
        }
        
        Byte marker = data[offset + 1];
        
        // Check for valid JPEG markers
        if (marker == 0x00 || marker == 0xFF) {
            offset += 2;
            continue;
        }
        
        if (marker == 0xD9) { // End of image
            return true;
        }
        
        if (marker >= 0xD0 && marker <= 0xD7) { // RST markers (no length)
            offset += 2;
            segment_count++;
            continue;
        }
        
        // Read segment length
        if (offset + 3 >= size) {
            break;
        }
        
        uint16_t segment_length = (data[offset + 2] << 8) | data[offset + 3];
        if (segment_length < 2) {
            break;
        }
        
        offset += 2 + segment_length;
        segment_count++;
    }
    
    return segment_count > 0;
}

Size JpegCarver::estimateSizeFromSegments(const Byte* data, Size max_size) const {
    Size offset = 2; // Skip initial 0xFF 0xD8
    Size last_valid_offset = offset;
    
    while (offset < max_size - 1) {
        if (data[offset] != 0xFF) {
            break;
        }
        
        Byte marker = data[offset + 1];
        
        if (marker == 0xD9) { // Found end marker
            return offset + 2;
        }
        
        if (marker >= 0xD0 && marker <= 0xD7) { // RST markers
            offset += 2;
            last_valid_offset = offset;
            continue;
        }
        
        if (offset + 3 >= max_size) {
            break;
        }
        
        uint16_t segment_length = (data[offset + 2] << 8) | data[offset + 3];
        if (segment_length < 2 || offset + 2 + segment_length > max_size) {
            break;
        }
        
        offset += 2 + segment_length;
        last_valid_offset = offset;
        
        // Don't go beyond reasonable size
        if (offset > getMaxFileSize()) {
            break;
        }
    }
    
    return last_valid_offset;
}

} // namespace FileRecovery
