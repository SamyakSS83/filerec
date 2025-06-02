#include <gtest/gtest.h>
#include "carvers/jpeg_carver.h"
#include "utils/logger.h"
#include <fstream>
#include <vector>
#include <filesystem>

using namespace FileRecovery;

class JpegCarverTest : public ::testing::Test {
protected:
    void SetUp() override {
        carver_ = std::make_unique<JpegCarver>();
        
        // Initialize logger for tests
        Logger::getInstance().initialize("test_jpeg.log", Logger::Level::DEBUG);
        
        // Create test data directory
        test_data_dir_ = "test_jpeg_data";
        std::filesystem::create_directories(test_data_dir_);
        
        // Create test JPEG headers
        createTestJpegData();
    }
    
    void TearDown() override {
        // Clean up test data
        std::filesystem::remove_all(test_data_dir_);
        std::filesystem::remove("test_jpeg.log");
    }
    
    void createTestJpegData() {
        // Valid JPEG header: FF D8 FF E0
        valid_jpeg_header_ = {0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 'J', 'F', 'I', 'F'};
        
        // Valid JPEG footer: FF D9
        valid_jpeg_footer_ = {0xFF, 0xD9};
        
        // Create a simple test JPEG file
        test_jpeg_data_.insert(test_jpeg_data_.end(), valid_jpeg_header_.begin(), valid_jpeg_header_.end());
        
        // Add some dummy JPEG data
        for (int i = 0; i < 100; ++i) {
            test_jpeg_data_.push_back(static_cast<uint8_t>(i % 256));
        }
        
        // Add footer
        test_jpeg_data_.insert(test_jpeg_data_.end(), valid_jpeg_footer_.begin(), valid_jpeg_footer_.end());
        
        // Create corrupted JPEG data (missing footer)
        corrupted_jpeg_data_ = valid_jpeg_header_;
        for (int i = 0; i < 50; ++i) {
            corrupted_jpeg_data_.push_back(static_cast<uint8_t>(i % 256));
        }
        // No footer - simulates corrupted file
        
        // Create non-JPEG data
        non_jpeg_data_ = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A}; // PNG header
    }
    
    std::unique_ptr<JpegCarver> carver_;
    std::string test_data_dir_;
    std::vector<uint8_t> valid_jpeg_header_;
    std::vector<uint8_t> valid_jpeg_footer_;
    std::vector<uint8_t> test_jpeg_data_;
    std::vector<uint8_t> corrupted_jpeg_data_;
    std::vector<uint8_t> non_jpeg_data_;
};

TEST_F(JpegCarverTest, DetectJpegSignature) {
    // Test if the carver can detect JPEG signatures
    auto signatures = carver_->getFileSignatures();
    EXPECT_FALSE(signatures.empty());
    
    // Check if JPEG header is in the signatures
    bool found_jpeg_header = false;
    for (const auto& sig : signatures) {
        if (sig.size() >= 2 && sig[0] == 0xFF && sig[1] == 0xD8) {
            found_jpeg_header = true;
            break;
        }
    }
    EXPECT_TRUE(found_jpeg_header);
}

TEST_F(JpegCarverTest, CarveValidJpeg) {
    auto results = carver_->carveFiles(test_jpeg_data_.data(), test_jpeg_data_.size(), 0);
    
    ASSERT_FALSE(results.empty());
    
    const auto& file = results[0];
    EXPECT_EQ(file.file_type, "JPEG");
    EXPECT_EQ(file.start_offset, 0);
    EXPECT_EQ(file.file_size, test_jpeg_data_.size());
    EXPECT_GT(file.confidence_score, 0.8); // Should have high confidence
}

TEST_F(JpegCarverTest, CarveMultipleJpegs) {
    // Create buffer with multiple JPEG files
    std::vector<uint8_t> multi_jpeg_data;
    
    // First JPEG
    multi_jpeg_data.insert(multi_jpeg_data.end(), test_jpeg_data_.begin(), test_jpeg_data_.end());
    
    // Some padding
    for (int i = 0; i < 50; ++i) {
        multi_jpeg_data.push_back(0x00);
    }
    
    // Second JPEG
    size_t second_jpeg_offset = multi_jpeg_data.size();
    multi_jpeg_data.insert(multi_jpeg_data.end(), test_jpeg_data_.begin(), test_jpeg_data_.end());
    
    auto results = carver_->carveFiles(multi_jpeg_data.data(), multi_jpeg_data.size(), 0);
    
    ASSERT_GE(results.size(), 2);
    
    // Check first JPEG
    EXPECT_EQ(results[0].start_offset, 0);
    EXPECT_EQ(results[0].file_size, test_jpeg_data_.size());
    
    // Check second JPEG
    EXPECT_EQ(results[1].start_offset, second_jpeg_offset);
    EXPECT_EQ(results[1].file_size, test_jpeg_data_.size());
}

TEST_F(JpegCarverTest, HandleCorruptedJpeg) {
    auto results = carver_->carveFiles(corrupted_jpeg_data_.data(), corrupted_jpeg_data_.size(), 0);
    
    // Should still detect the JPEG header but with lower confidence
    if (!results.empty()) {
        const auto& file = results[0];
        EXPECT_EQ(file.file_type, "JPEG");
        EXPECT_EQ(file.start_offset, 0);
        EXPECT_LE(file.confidence_score, 0.8); // Lower confidence due to missing footer
    }
}

TEST_F(JpegCarverTest, ValidateFile) {
    auto results = carver_->carveFiles(test_jpeg_data_.data(), test_jpeg_data_.size(), 0);
    if (!results.empty()) {
        const auto& file = results[0];
        double confidence = carver_->validateFile(file, test_jpeg_data_.data());
        EXPECT_GE(confidence, 0.0);
        EXPECT_LE(confidence, 1.0);
    }
}

TEST_F(JpegCarverTest, CalculateConfidenceScore) {
    // Test confidence calculation for various scenarios
    
    // Valid JPEG with footer should have high confidence
    auto results = carver_->carveFiles(test_jpeg_data_.data(), test_jpeg_data_.size(), 0);
    if (!results.empty()) {
        EXPECT_GT(results[0].confidence_score, 0.8);
    }
    
    // JPEG without footer should have lower confidence
    results = carver_->carveFiles(corrupted_jpeg_data_.data(), corrupted_jpeg_data_.size(), 0);
    if (!results.empty()) {
        EXPECT_LT(results[0].confidence_score, 0.8);
    }
    
    // Non-JPEG data should not be detected
    results = carver_->carveFiles(non_jpeg_data_.data(), non_jpeg_data_.size(), 0);
    EXPECT_TRUE(results.empty());
}

TEST_F(JpegCarverTest, GetSupportedTypes) {
    auto types = carver_->getSupportedTypes();
    EXPECT_FALSE(types.empty());
    
    bool found_jpeg = false;
    for (const auto& type : types) {
        if (type == "JPEG" || type == "JPG") {
            found_jpeg = true;
            break;
        }
    }
    EXPECT_TRUE(found_jpeg);
}

TEST_F(JpegCarverTest, LargeDataHandling) {
    // Test with large amounts of data to ensure performance
    std::vector<uint8_t> large_data;
    
    // Create 1MB of random data with a JPEG embedded
    large_data.resize(1024 * 1024, 0xAA);
    
    // Embed JPEG at position 500KB
    size_t jpeg_pos = 500 * 1024;
    std::copy(test_jpeg_data_.begin(), test_jpeg_data_.end(), large_data.begin() + jpeg_pos);
    
    auto results = carver_->carveFiles(large_data.data(), large_data.size(), 0);
    
    // Should find at least one JPEG
    bool found_jpeg = false;
    for (const auto& file : results) {
        if (file.start_offset == jpeg_pos) {
            found_jpeg = true;
            break;
        }
    }
    EXPECT_TRUE(found_jpeg);
}

TEST_F(JpegCarverTest, EdgeCases) {
    // Test with null pointer
    auto results = carver_->carveFiles(nullptr, 0, 0);
    EXPECT_TRUE(results.empty());
    
    // Test with zero size
    results = carver_->carveFiles(test_jpeg_data_.data(), 0, 0);
    EXPECT_TRUE(results.empty());
    
    // Test with very small data
    uint8_t small_data[] = {0xFF};
    results = carver_->carveFiles(small_data, 1, 0);
    EXPECT_TRUE(results.empty());
}

TEST_F(JpegCarverTest, GetMaxFileSize) {
    Size max_size = carver_->getMaxFileSize();
    EXPECT_GT(max_size, 0);
    
    // Should be reasonable for JPEG files (e.g., at least 10MB)
    EXPECT_GE(max_size, 10 * 1024 * 1024);
}
