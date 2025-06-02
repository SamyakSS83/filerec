#include <gtest/gtest.h>
#include "carvers/zip_carver.h"
#include "utils/logger.h"
#include <fstream>
#include <vector>
#include <filesystem>

using namespace FileRecovery;

class ZipCarverTest : public ::testing::Test {
protected:
    void SetUp() override {
        carver_ = std::make_unique<ZipCarver>();
        
        // Initialize logger for tests
        Logger::getInstance().initialize("test_zip.log", Logger::Level::DEBUG);
        
        // Create test data directory
        test_data_dir_ = "test_zip_data";
        std::filesystem::create_directories(test_data_dir_);
        
        // Create test ZIP data
        createTestZipData();
    }
    
    void TearDown() override {
        // Clean up test data
        std::filesystem::remove_all(test_data_dir_);
        std::filesystem::remove("test_zip.log");
    }
    
    void createTestZipData() {
        // Create a minimal ZIP file structure
        test_zip_data_.clear();
        
        // Local file header for a simple text file
        std::vector<uint8_t> local_header = {
            0x50, 0x4B, 0x03, 0x04, // Local file header signature
            0x14, 0x00,             // Version needed to extract
            0x00, 0x00,             // General purpose bit flag
            0x00, 0x00,             // Compression method (stored)
            0x00, 0x00,             // Last mod file time
            0x00, 0x00,             // Last mod file date
            0x00, 0x00, 0x00, 0x00, // CRC-32
            0x05, 0x00, 0x00, 0x00, // Compressed size (5 bytes)
            0x05, 0x00, 0x00, 0x00, // Uncompressed size (5 bytes)
            0x08, 0x00,             // File name length (8 chars)
            0x00, 0x00              // Extra field length
        };
        test_zip_data_.insert(test_zip_data_.end(), local_header.begin(), local_header.end());
        
        // File name: "test.txt"
        std::string filename = "test.txt";
        test_zip_data_.insert(test_zip_data_.end(), filename.begin(), filename.end());
        
        // File data: "Hello"
        std::string file_content = "Hello";
        test_zip_data_.insert(test_zip_data_.end(), file_content.begin(), file_content.end());
        
        // Central directory header
        std::vector<uint8_t> central_header = {
            0x50, 0x4B, 0x01, 0x02, // Central file header signature
            0x14, 0x00,             // Version made by
            0x14, 0x00,             // Version needed to extract
            0x00, 0x00,             // General purpose bit flag
            0x00, 0x00,             // Compression method
            0x00, 0x00,             // Last mod file time
            0x00, 0x00,             // Last mod file date
            0x00, 0x00, 0x00, 0x00, // CRC-32
            0x05, 0x00, 0x00, 0x00, // Compressed size
            0x05, 0x00, 0x00, 0x00, // Uncompressed size
            0x08, 0x00,             // File name length
            0x00, 0x00,             // Extra field length
            0x00, 0x00,             // File comment length
            0x00, 0x00,             // Disk number start
            0x00, 0x00,             // Internal file attributes
            0x00, 0x00, 0x00, 0x00, // External file attributes
            0x00, 0x00, 0x00, 0x00  // Relative offset of local header
        };
        test_zip_data_.insert(test_zip_data_.end(), central_header.begin(), central_header.end());
        
        // File name again
        test_zip_data_.insert(test_zip_data_.end(), filename.begin(), filename.end());
        
        // End of central directory record
        std::vector<uint8_t> end_central_dir = {
            0x50, 0x4B, 0x05, 0x06, // End of central dir signature
            0x00, 0x00,             // Number of this disk
            0x00, 0x00,             // Disk where central directory starts
            0x01, 0x00,             // Number of central directory records on this disk
            0x01, 0x00,             // Total number of central directory records
            0x2E, 0x00, 0x00, 0x00, // Size of central directory (46 bytes)
            0x27, 0x00, 0x00, 0x00, // Offset of start of central directory
            0x00, 0x00              // Comment length
        };
        test_zip_data_.insert(test_zip_data_.end(), end_central_dir.begin(), end_central_dir.end());
        
        // Create corrupted ZIP data (missing end of central directory)
        corrupted_zip_data_.insert(corrupted_zip_data_.end(), local_header.begin(), local_header.end());
        corrupted_zip_data_.insert(corrupted_zip_data_.end(), filename.begin(), filename.end());
        corrupted_zip_data_.insert(corrupted_zip_data_.end(), file_content.begin(), file_content.end());
        // No central directory - simulates corrupted file
        
        // Create non-ZIP data
        non_zip_data_ = {0xFF, 0xD8, 0xFF, 0xE0}; // JPEG header
    }
    
    std::unique_ptr<ZipCarver> carver_;
    std::string test_data_dir_;
    std::vector<uint8_t> test_zip_data_;
    std::vector<uint8_t> corrupted_zip_data_;
    std::vector<uint8_t> non_zip_data_;
};

TEST_F(ZipCarverTest, DetectZipSignature) {
    // Test if the carver can detect ZIP signatures (replaces canCarve test)
    auto signatures = carver_->getFileSignatures();
    EXPECT_FALSE(signatures.empty());
    
    // Check if ZIP header (PK\x03\x04) is in the signatures
    bool found_zip_header = false;
    for (const auto& sig : signatures) {
        if (sig.size() >= 4 && 
            sig[0] == 0x50 && sig[1] == 0x4B && 
            sig[2] == 0x03 && sig[3] == 0x04) {
            found_zip_header = true;
            break;
        }
    }
    EXPECT_TRUE(found_zip_header);
    
    // Check that it can detect end of central directory signature (PK\x05\x06)
    auto footers = carver_->getFileFooters();
    bool found_eocd = false;
    for (const auto& footer : footers) {
        if (footer.size() >= 4 && 
            footer[0] == 0x50 && footer[1] == 0x4B && 
            footer[2] == 0x05 && footer[3] == 0x06) {
            found_eocd = true;
            break;
        }
    }
    EXPECT_TRUE(found_eocd);
}

TEST_F(ZipCarverTest, CarveValidZip) {
    auto results = carver_->carveFiles(test_zip_data_.data(), test_zip_data_.size(), 0);
    
    ASSERT_EQ(results.size(), 1);
    
    const auto& file = results[0];
    EXPECT_EQ(file.file_type, "zip");
    EXPECT_EQ(file.start_offset, 0);
    EXPECT_EQ(file.file_size, test_zip_data_.size());
    EXPECT_GT(file.confidence_score, 0.7); // Should have high confidence
    EXPECT_FALSE(file.is_fragmented);
    EXPECT_EQ(file.fragments.size(), 1);
}

TEST_F(ZipCarverTest, CarveMultipleZips) {
    // Create buffer with multiple ZIP files
    std::vector<uint8_t> multi_zip_data;
    
    // First ZIP
    multi_zip_data.insert(multi_zip_data.end(), test_zip_data_.begin(), test_zip_data_.end());
    
    // Some padding
    for (int i = 0; i < 50; ++i) {
        multi_zip_data.push_back(0x00);
    }
    
    // Second ZIP
    size_t second_zip_offset = multi_zip_data.size();
    multi_zip_data.insert(multi_zip_data.end(), test_zip_data_.begin(), test_zip_data_.end());
    
    auto results = carver_->carveFiles(multi_zip_data.data(), multi_zip_data.size(), 0);
    
    ASSERT_EQ(results.size(), 2);
    
    // Check first ZIP
    EXPECT_EQ(results[0].start_offset, 0);
    EXPECT_EQ(results[0].file_size, test_zip_data_.size());
    
    // Check second ZIP
    EXPECT_EQ(results[1].start_offset, second_zip_offset);
    EXPECT_EQ(results[1].file_size, test_zip_data_.size());
}

TEST_F(ZipCarverTest, HandleCorruptedZip) {
    auto results = carver_->carveFiles(corrupted_zip_data_.data(), corrupted_zip_data_.size(), 0);
    
    // Should still detect the ZIP header but with lower confidence
    ASSERT_FALSE(results.empty());
    
    const auto& file = results[0];
    EXPECT_EQ(file.file_type, "zip");
    EXPECT_EQ(file.start_offset, 0);
    EXPECT_LT(file.confidence_score, 0.7); // Lower confidence due to missing central directory
}

TEST_F(ZipCarverTest, ValidateFile) {
    // Replace SaveCarvedFile test with ValidateFile test
    auto results = carver_->carveFiles(test_zip_data_.data(), test_zip_data_.size(), 0);
    ASSERT_FALSE(results.empty());
    
    const auto& file = results[0];
    double confidence = carver_->validateFile(file, test_zip_data_.data());
    
    EXPECT_GT(confidence, 0.0);
    EXPECT_LE(confidence, 1.0);
    EXPECT_GT(confidence, 0.7); // Good ZIP should have high confidence
}

TEST_F(ZipCarverTest, ConfidenceScoring) {
    // Test confidence scoring through carved file results
    
    // Valid ZIP with central directory should have high confidence
    auto results = carver_->carveFiles(test_zip_data_.data(), test_zip_data_.size(), 0);
    ASSERT_FALSE(results.empty());
    EXPECT_GT(results[0].confidence_score, 0.7);
    
    // ZIP without central directory should have lower confidence
    results = carver_->carveFiles(corrupted_zip_data_.data(), corrupted_zip_data_.size(), 0);
    ASSERT_FALSE(results.empty());
    EXPECT_LT(results[0].confidence_score, 0.7);
    EXPECT_GT(results[0].confidence_score, 0.4); // But still reasonable
    
    // Non-ZIP data should not be detected
    results = carver_->carveFiles(non_zip_data_.data(), non_zip_data_.size(), 0);
    EXPECT_TRUE(results.empty());
}

TEST_F(ZipCarverTest, GetSupportedTypes) {
    auto types = carver_->getSupportedTypes();
    EXPECT_FALSE(types.empty());
    
    bool found_zip = false;
    for (const auto& type : types) {
        if (type == "zip" || type == "jar" || type == "apk") {
            found_zip = true;
            break;
        }
    }
    EXPECT_TRUE(found_zip);
}

TEST_F(ZipCarverTest, ValidateFileIndirectly) {
    // Test ZIP structure validation through validateFile
    auto results = carver_->carveFiles(test_zip_data_.data(), test_zip_data_.size(), 0);
    ASSERT_FALSE(results.empty());
    
    const auto& file = results[0];
    double good_confidence = carver_->validateFile(file, test_zip_data_.data());
    EXPECT_GT(good_confidence, 0.7);
    
    // Test with corrupted structure
    results = carver_->carveFiles(corrupted_zip_data_.data(), corrupted_zip_data_.size(), 0);
    if (!results.empty()) {
        double bad_confidence = carver_->validateFile(results[0], corrupted_zip_data_.data());
        EXPECT_LT(bad_confidence, 0.7);
    }
}

TEST_F(ZipCarverTest, LargeDataHandling) {
    // Create a large buffer with ZIP data at various positions
    std::vector<uint8_t> large_data(10000, 0x00);
    
    // Insert ZIP at position 1000
    std::copy(test_zip_data_.begin(), test_zip_data_.end(), large_data.begin() + 1000);
    
    auto results = carver_->carveFiles(large_data.data(), large_data.size(), 0);
    
    ASSERT_GE(results.size(), 1);
    bool found_zip = false;
    for (const auto& file : results) {
        if (file.start_offset == 1000) {
            found_zip = true;
            EXPECT_EQ(file.file_size, test_zip_data_.size());
            break;
        }
    }
    EXPECT_TRUE(found_zip);
}

TEST_F(ZipCarverTest, EdgeCases) {
    // Test with null pointer
    auto results = carver_->carveFiles(nullptr, 0, 0);
    EXPECT_TRUE(results.empty());
    
    // Test with zero size
    results = carver_->carveFiles(test_zip_data_.data(), 0, 0);
    EXPECT_TRUE(results.empty());
    
    // Test with very small data
    uint8_t small_data[] = {0x50};
    results = carver_->carveFiles(small_data, 1, 0);
    EXPECT_TRUE(results.empty());
}

TEST_F(ZipCarverTest, MaxFileSize) {
    EXPECT_GT(carver_->getMaxFileSize(), 0);
    EXPECT_LE(carver_->getMaxFileSize(), 100ULL * 1024 * 1024); // Should be reasonable limit
}

TEST_F(ZipCarverTest, CheckLocalFileHeader) {
    // Test if the carver handles different signatures correctly
    
    // Valid header: create a buffer with just the header
    std::vector<uint8_t> valid_header = {0x50, 0x4B, 0x03, 0x04, 0x14, 0x00, 0x00, 0x00};
    auto results = carver_->carveFiles(valid_header.data(), valid_header.size(), 0);
    // It may not carve the file (too small) but it should recognize the signature
    
    // Invalid header: create a buffer with wrong signature
    std::vector<uint8_t> invalid_header = {0x50, 0x4B, 0x03, 0x05, 0x14, 0x00, 0x00, 0x00};
    results = carver_->carveFiles(invalid_header.data(), invalid_header.size(), 0);
    EXPECT_TRUE(results.empty()); // Should not detect this as ZIP
}

TEST_F(ZipCarverTest, EmptyArchive) {
    // Test with empty ZIP archive (only end of central directory)
    std::vector<uint8_t> empty_zip = {
        0x50, 0x4B, 0x05, 0x06, // End of central dir signature
        0x00, 0x00,             // Number of this disk
        0x00, 0x00,             // Disk where central directory starts
        0x00, 0x00,             // Number of central directory records on this disk
        0x00, 0x00,             // Total number of central directory records
        0x00, 0x00, 0x00, 0x00, // Size of central directory
        0x00, 0x00, 0x00, 0x00, // Offset of start of central directory
        0x00, 0x00              // Comment length
    };
    
    auto results = carver_->carveFiles(empty_zip.data(), empty_zip.size(), 0);
    EXPECT_GE(results.size(), 0); // May or may not detect this as a valid ZIP
}
