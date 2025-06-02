#pragma once

#include "interfaces/file_carver.h"

namespace FileRecovery {

/**
 * @brief Base implementation for file carvers
 * 
 * Provides common functionality for all file carvers including
 * pattern searching and entropy calculation.
 */
class BaseCarver : public FileCarver {
public:
    BaseCarver() = default;
    virtual ~BaseCarver() = default;
    
protected:
    /**
     * @brief Find all occurrences of a pattern in data
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
     * @brief Calculate Shannon entropy of data
     * @param data Data to analyze
     * @param size Size of the data
     * @return Entropy value (0.0 - 8.0)
     */
    double calculateEntropy(const Byte* data, Size size) const;
    
    /**
     * @brief Check if data matches expected file structure
     * @param data File data
     * @param size Size of the data
     * @return true if structure appears valid
     */
    virtual bool validateFileStructure(const Byte* data, Size size) const;
    
    /**
     * @brief Extract metadata from file if possible
     * @param data File data
     * @param size Size of the data
     * @return Metadata string
     */
    virtual std::string extractMetadata(const Byte* data, Size size) const;
    
    /**
     * @brief Generate filename for recovered file
     * @param offset File offset in disk
     * @param file_type File type/extension
     * @return Generated filename
     */
    std::string generateFilename(Offset offset, const std::string& file_type) const;
    
    /**
     * @brief Dump binary data to log for debugging
     * @param data Pointer to the data to dump
     * @param size Size of the data
     * @param prefix Prefix message for the log entry
     */
    void dumpData(const Byte* data, Size size, const std::string& prefix) const;
    
    /**
     * @brief Calculate confidence score based on multiple factors
     * @param has_valid_header Header validation result
     * @param has_valid_footer Footer validation result (if applicable)
     * @param entropy_score Entropy of the file
     * @param structure_valid Structure validation result
     * @return Confidence score (0.0 - 1.0)
     */
    double calculateConfidenceScore(
        bool has_valid_header,
        bool has_valid_footer,
        double entropy_score,
        bool structure_valid
    ) const;
};

} // namespace FileRecovery
