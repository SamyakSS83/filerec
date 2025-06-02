#include "carvers/base_carver.h"
#include "utils/logger.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace FileRecovery {

std::vector<Offset> BaseCarver::findPattern(
    const Byte* data, 
    Size size, 
    const std::vector<Byte>& pattern
) const {
    std::vector<Offset> matches;
    
    if (pattern.empty() || size < pattern.size()) {
        return matches;
    }
    
    // Use Boyer-Moore-like approach for efficiency
    for (Size i = 0; i <= size - pattern.size(); ++i) {
        if (std::equal(pattern.begin(), pattern.end(), data + i)) {
            matches.push_back(i);
        }
    }
    
    return matches;
}

double BaseCarver::calculateEntropy(const Byte* data, Size size) const {
    if (size == 0) {
        return 0.0;
    }
    
    // Count frequency of each byte value
    std::array<Size, 256> frequency{};
    for (Size i = 0; i < size; ++i) {
        frequency[data[i]]++;
    }
    
    // Calculate Shannon entropy
    double entropy = 0.0;
    for (Size freq : frequency) {
        if (freq > 0) {
            double probability = static_cast<double>(freq) / size;
            entropy -= probability * std::log2(probability);
        }
    }
    
    return entropy;
}

bool BaseCarver::validateFileStructure(const Byte* data, Size size) const {
    // Basic validation - check if data is not all zeros or all same value
    if (size < 16) {
        return false;
    }
    
    Byte first_byte = data[0];
    bool all_same = true;
    for (Size i = 1; i < std::min(size, Size(1024)); ++i) {
        if (data[i] != first_byte) {
            all_same = false;
            break;
        }
    }
    
    return !all_same;
}

std::string BaseCarver::extractMetadata(const Byte* data, Size size) const {
    // Base implementation returns empty metadata
    // Derived classes should override this for specific file types
    return "";
}

std::string BaseCarver::generateFilename(Offset offset, const std::string& file_type) const {
    std::stringstream ss;
    ss << "recovered_" << std::hex << std::setfill('0') << std::setw(16) << offset;
    ss << "." << file_type;
    return ss.str();
}

// Add this method to help debug file carver issues
void BaseCarver::dumpData(const Byte* data, Size size, const std::string& prefix) const {
    std::stringstream ss;
    ss << prefix << " (size=" << size << "): ";
    
    const size_t max_bytes = std::min(size, Size(32)); // Show up to 32 bytes
    for (size_t i = 0; i < max_bytes; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') 
           << static_cast<int>(data[i]) << " ";
    }
    
    if (size > max_bytes) {
        ss << "...";
    }
    
    LOG_DEBUG(ss.str());
}

// Improve confidence scoring
double BaseCarver::calculateConfidenceScore(
    bool has_valid_header,
    bool has_valid_footer,
    double entropy_score,
    bool structure_valid
) const {
    double score = 0.0;
    
    LOG_DEBUG("Calculating confidence - header:" + std::to_string(has_valid_header) + 
              " footer:" + std::to_string(has_valid_footer) + 
              " entropy:" + std::to_string(entropy_score) + 
              " structure:" + std::to_string(structure_valid));
    
    // Header validity (40% weight)
    if (has_valid_header) {
        score += 0.4;
    }
    
    // Footer validity (20% weight) - if applicable
    if (has_valid_footer) {
        score += 0.2;
    }
    
    // Entropy score (20% weight)
    // Good entropy for most file types is between 6.0 and 8.0
    if (entropy_score >= 6.0 && entropy_score <= 8.0) {
        score += 0.2;
    } else if (entropy_score >= 4.0 && entropy_score < 6.0) {
        score += 0.1; // Partial credit for moderate entropy
    }
    
    // Structure validity (20% weight)
    if (structure_valid) {
        score += 0.2;
    }
    
    LOG_DEBUG("Final confidence score: " + std::to_string(score));
    return std::min(score, 1.0);
}

} // namespace FileRecovery
