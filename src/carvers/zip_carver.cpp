#include "carvers/zip_carver.h"
#include "utils/logger.h"
#include <cstring>
#include <algorithm>
#include <sstream>

namespace FileRecovery {

ZipCarver::ZipCarver() = default;

std::vector<std::string> ZipCarver::getSupportedTypes() const {
    return {"zip", "jar", "apk", "docx", "xlsx", "pptx"};
}

std::vector<std::vector<Byte>> ZipCarver::getFileSignatures() const {
    return {
        {0x50, 0x4B, 0x03, 0x04}, // PK\x03\x04 - standard ZIP
        {0x50, 0x4B, 0x05, 0x06}, // PK\x05\x06 - empty archive
        {0x50, 0x4B, 0x07, 0x08}  // PK\x07\x08 - spanned archive
    };
}

std::vector<std::vector<Byte>> ZipCarver::getFileFooters() const {
    // ZIP files can end with End of Central Directory signature
    return {
        {0x50, 0x4B, 0x05, 0x06} // End of central directory
    };
}

std::vector<RecoveredFile> ZipCarver::carveFiles(const Byte* data, Size size, Offset base_offset) {
    std::vector<RecoveredFile> recovered_files;
    
    // Search for ZIP signatures
    auto signatures = getFileSignatures();
    for (const auto& signature : signatures) {
        auto offsets = findPattern(data, size, signature);
        
        for (auto offset : offsets) {
            if (offset + sizeof(ZipLocalFileHeader) > size) continue;
            
            const auto* header = reinterpret_cast<const ZipLocalFileHeader*>(data + offset);
            if (!validate_local_file_header(header)) continue;
            
            // Calculate ZIP file size
            size_t zip_size = calculate_zip_size(data + offset, size - offset);
            if (zip_size == 0 || offset + zip_size > size) continue;
            
            // Validate structure
            if (!validate_zip_structure(data + offset, zip_size)) continue;
            
            // Create recovered file entry
            RecoveredFile file;
            file.filename = "recovered_" + std::to_string(base_offset + offset) + ".zip";
            file.file_type = "zip";
            file.start_offset = base_offset + offset;
            file.file_size = zip_size;
            file.confidence_score = calculateConfidence(data + offset, zip_size);
            file.is_fragmented = false;
            file.fragments = {{base_offset + offset, zip_size}};
            
            recovered_files.push_back(file);
        }
    }
    
    return recovered_files;
}

double ZipCarver::validateFile(const RecoveredFile& file, const Byte* data) {
    if (file.file_size < sizeof(ZipLocalFileHeader)) {
        return 0.0;
    }
    
    return calculateConfidence(data, file.file_size);
}

Size ZipCarver::getMaxFileSize() const {
    return 100 * 1024 * 1024; // 100MB max for ZIP files
}

double ZipCarver::calculateConfidence(const Byte* data, Size size) const {
    double confidence = 0.5; // Base confidence
    
    // Check if we have a valid local file header
    if (size >= sizeof(ZipLocalFileHeader)) {
        const auto* header = reinterpret_cast<const ZipLocalFileHeader*>(data);
        if (validate_local_file_header(header)) {
            confidence += 0.2;
        }
    }
    
    // Check for end of central directory
    if (find_end_of_central_directory(data, size) > 0) {
        confidence += 0.2;
    }
    
    // Check entropy (ZIP files should have reasonable compression)
    double entropy = calculateEntropy(data, std::min(size, Size(8192)));
    if (entropy > 3.0 && entropy < 7.5) {
        confidence += 0.1;
    }
    
    return std::min(confidence, 1.0);
}

/*
// Legacy method for compatibility - remove old method calls
bool ZipCarver::can_carve(const uint8_t* data, size_t size) const {
    if (!BaseCarver::can_carve(data, size)) {
        return false;
    }
    
    // Additional ZIP-specific validation
    if (size < sizeof(ZipLocalFileHeader)) {
        return false;
    }
    
    const auto* header = reinterpret_cast<const ZipLocalFileHeader*>(data);
    return validate_local_file_header(header);
}

CarveResult ZipCarver::carve_file(const uint8_t* data, size_t size, uint64_t offset) const {
    if (!can_carve(data, size)) {
        return {false, 0, 0, {}};
    }
    
    LOG_DEBUG("Attempting to carve ZIP file at offset " + std::to_string(offset));
    
    // Find the actual size of the ZIP file
    size_t zip_size = calculate_zip_size(data, size);
    if (zip_size == 0) {
        LOG_DEBUG("Could not determine ZIP file size");
        return {false, 0, 0, {}};
    }
    
    // Validate the entire ZIP structure
    if (!validate_zip_structure(data, zip_size)) {
        LOG_DEBUG("ZIP structure validation failed");
        return {false, 0, 0, {}};
    }
    
    // Calculate confidence based on various factors
    uint32_t confidence = calculate_confidence(data, zip_size);
    
    // Extract metadata
    auto metadata = extract_metadata(data, zip_size);
    metadata["file_count"] = std::to_string(count_zip_entries(data, zip_size));
    metadata["zip_metadata"] = extract_zip_metadata(data, zip_size);
    
    LOG_INFO("Successfully carved ZIP file of size " + std::to_string(zip_size) + 
                " bytes with confidence " + std::to_string(confidence) + "%");
    
    return {true, zip_size, confidence, metadata};
}
*/

bool ZipCarver::validate_zip_structure(const uint8_t* data, size_t size) const {
    if (size < sizeof(ZipLocalFileHeader)) {
        return false;
    }
    
    size_t pos = 0;
    uint32_t file_count = 0;
    const uint32_t max_files = 10000; // Reasonable limit
    
    // Parse local file headers
    while (pos < size - sizeof(uint32_t)) {
        uint32_t signature = *reinterpret_cast<const uint32_t*>(data + pos);
        
        if (signature == LOCAL_FILE_HEADER_SIG) {
            if (pos + sizeof(ZipLocalFileHeader) > size) break;
            
            const auto* header = reinterpret_cast<const ZipLocalFileHeader*>(data + pos);
            if (!validate_local_file_header(header)) {
                return false;
            }
            
            // Skip to next entry
            size_t entry_size = sizeof(ZipLocalFileHeader) + 
                               header->filename_length + 
                               header->extra_field_length + 
                               header->compressed_size;
            
            // Check for data descriptor
            if (header->general_purpose_flag & 0x0008) {
                entry_size += 12; // CRC32 + sizes
                if (pos + entry_size + 4 <= size && 
                    *reinterpret_cast<const uint32_t*>(data + pos + entry_size - 12) == DATA_DESCRIPTOR_SIG) {
                    entry_size += 4; // Include signature
                }
            }
            
            pos += entry_size;
            file_count++;
            
            if (file_count > max_files) {
                LOG_WARNING("ZIP file has too many entries, might be corrupted");
                return false;
            }
        }
        else if (signature == CENTRAL_DIR_HEADER_SIG) {
            // Reached central directory, this is good
            break;
        }
        else {
            // Unknown signature, might be data or corruption
            pos++;
        }
    }
    
    return file_count > 0;
}

size_t ZipCarver::find_end_of_central_directory(const uint8_t* data, size_t size) const {
    // Search from the end for End of Central Directory signature
    if (size < sizeof(ZipEndOfCentralDir)) {
        return 0;
    }
    
    // Start from the end and work backwards
    for (size_t i = size - sizeof(ZipEndOfCentralDir); i > 0; i--) {
        uint32_t signature = *reinterpret_cast<const uint32_t*>(data + i);
        if (signature == END_OF_CENTRAL_DIR_SIG) {
            const auto* eocd = reinterpret_cast<const ZipEndOfCentralDir*>(data + i);
            if (validate_end_of_central_dir(eocd)) {
                return i;
            }
        }
    }
    
    return 0;
}

bool ZipCarver::validate_local_file_header(const ZipLocalFileHeader* header) const {
    if (header->signature != LOCAL_FILE_HEADER_SIG) {
        return false;
    }
    
    // Check version (should be reasonable)
    if (header->version_needed > 63) {
        return false;
    }
    
    // Check compression method (0=stored, 8=deflated are most common)
    if (header->compression_method > 99) {
        return false;
    }
    
    // Filename length should be reasonable
    if (header->filename_length > 512) {
        return false;
    }
    
    // Extra field length should be reasonable
    if (header->extra_field_length > 1024) {
        return false;
    }
    
    return true;
}

bool ZipCarver::validate_central_dir_header(const ZipCentralDirHeader* header) const {
    if (header->signature != CENTRAL_DIR_HEADER_SIG) {
        return false;
    }
    
    // Similar validation as local file header
    if (header->version_needed > 63 || header->compression_method > 99) {
        return false;
    }
    
    if (header->filename_length > 512 || header->extra_field_length > 1024) {
        return false;
    }
    
    return true;
}

bool ZipCarver::validate_end_of_central_dir(const ZipEndOfCentralDir* header) const {
    if (header->signature != END_OF_CENTRAL_DIR_SIG) {
        return false;
    }
    
    // Comment length should be reasonable
    if (header->comment_length > 1024) {
        return false;
    }
    
    // Entry counts should match
    if (header->central_dir_entries_on_disk != header->total_central_dir_entries) {
        // This might be a multi-disk archive, which is less common but valid
    }
    
    return true;
}

size_t ZipCarver::calculate_zip_size(const uint8_t* data, size_t max_size) const {
    // Find End of Central Directory record
    size_t eocd_pos = find_end_of_central_directory(data, max_size);
    if (eocd_pos == 0) {
        // Try to estimate size from local file headers
        size_t pos = 0;
        size_t last_valid_pos = 0;
        
        while (pos < max_size - sizeof(uint32_t)) {
            uint32_t signature = *reinterpret_cast<const uint32_t*>(data + pos);
            
            if (signature == LOCAL_FILE_HEADER_SIG) {
                if (pos + sizeof(ZipLocalFileHeader) > max_size) break;
                
                const auto* header = reinterpret_cast<const ZipLocalFileHeader*>(data + pos);
                if (!validate_local_file_header(header)) break;
                
                size_t entry_size = sizeof(ZipLocalFileHeader) + 
                                   header->filename_length + 
                                   header->extra_field_length + 
                                   header->compressed_size;
                
                if (header->general_purpose_flag & 0x0008) {
                    entry_size += 12; // Data descriptor
                }
                
                pos += entry_size;
                last_valid_pos = pos;
            } else {
                break;
            }
        }
        
        return last_valid_pos;
    }
    
    const auto* eocd = reinterpret_cast<const ZipEndOfCentralDir*>(data + eocd_pos);
    return eocd_pos + sizeof(ZipEndOfCentralDir) + eocd->comment_length;
}

std::string ZipCarver::extract_zip_metadata(const uint8_t* data, size_t size) const {
    std::ostringstream metadata;
    
    size_t eocd_pos = find_end_of_central_directory(data, size);
    if (eocd_pos > 0) {
        const auto* eocd = reinterpret_cast<const ZipEndOfCentralDir*>(data + eocd_pos);
        
        metadata << "entries:" << eocd->total_central_dir_entries;
        metadata << ",central_dir_size:" << eocd->central_dir_size;
        
        if (eocd->comment_length > 0 && eocd_pos + sizeof(ZipEndOfCentralDir) + eocd->comment_length <= size) {
            const char* comment = reinterpret_cast<const char*>(data + eocd_pos + sizeof(ZipEndOfCentralDir));
            std::string comment_str(comment, std::min(static_cast<size_t>(eocd->comment_length), size_t(100)));
            if (!comment_str.empty()) {
                metadata << ",comment:" << comment_str;
            }
        }
    }
    
    return metadata.str();
}

uint32_t ZipCarver::count_zip_entries(const uint8_t* data, size_t size) const {
    size_t eocd_pos = find_end_of_central_directory(data, size);
    if (eocd_pos > 0) {
        const auto* eocd = reinterpret_cast<const ZipEndOfCentralDir*>(data + eocd_pos);
        return eocd->total_central_dir_entries;
    }
    
    // Count manually from local file headers
    uint32_t count = 0;
    size_t pos = 0;
    
    while (pos < size - sizeof(uint32_t)) {
        uint32_t signature = *reinterpret_cast<const uint32_t*>(data + pos);
        
        if (signature == LOCAL_FILE_HEADER_SIG) {
            if (pos + sizeof(ZipLocalFileHeader) > size) break;
            
            const auto* header = reinterpret_cast<const ZipLocalFileHeader*>(data + pos);
            if (!validate_local_file_header(header)) break;
            
            size_t entry_size = sizeof(ZipLocalFileHeader) + 
                               header->filename_length + 
                               header->extra_field_length + 
                               header->compressed_size;
            
            if (header->general_purpose_flag & 0x0008) {
                entry_size += 12;
            }
            
            pos += entry_size;
            count++;
        } else {
            break;
        }
    }
    
    return count;
}

} // namespace FileRecovery
