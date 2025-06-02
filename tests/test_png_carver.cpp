#include <gtest/gtest.h>
#include "carvers/png_carver.h"
#include "utils/logger.h"
#include <fstream>
#include <vector>
#include <filesystem>

using namespace FileRecovery;

class PngCarverTest : public ::testing::Test {
protected:
    void SetUp() override {
        carver_ = std::make_unique<PngCarver>();
        
        // Initialize logger for tests
        Logger::getInstance().initialize("test_png.log", Logger::Level::DEBUG);
        
        // Create test data directory
        test_data_dir_ = "test_png_data";
        std::filesystem::create_directories(test_data_dir_);
        
        // Create test PNG data
        createTestPngData();
    }
    
    void TearDown() override {
        // Clean up test data
        std::filesystem::remove_all(test_data_dir_);
        std::filesystem::remove("test_png.log");
    }
    
    void createTestPngData() {
        // Valid PNG signature: 8 bytes
        valid_png_header_ = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
        
        // IEND chunk: length(4) + "IEND"(4) + CRC(4)
        valid_png_footer_ = {0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82};
        
        // Create a minimal test PNG file
        test_png_data_.insert(test_png_data_.end(), valid_png_header_.begin(), valid_png_header_.end());
        
        // IHDR chunk: length(4) + "IHDR"(4) + data(13) + CRC(4)
        std::vector<uint8_t> ihdr_chunk = {
            0x00, 0x00, 0x00, 0x0D, // Length: 13 bytes
            0x49, 0x48, 0x44, 0x52, // "IHDR"
            0x00, 0x00, 0x00, 0x01, // Width: 1
            0x00, 0x00, 0x00, 0x01, // Height: 1
            0x08, 0x02, 0x00, 0x00, 0x00, // Bit depth, color type, compression, filter, interlace
            0x90, 0x77, 0x53, 0xDE  // CRC32 (calculated for this specific IHDR)
        };
        test_png_data_.insert(test_png_data_.end(), ihdr_chunk.begin(), ihdr_chunk.end());
        
        // IDAT chunk with minimal data
        std::vector<uint8_t> idat_chunk = {
            0x00, 0x00, 0x00, 0x0C, // Length: 12 bytes
            0x49, 0x44, 0x41, 0x54, // "IDAT"
            0x78, 0x9C, 0x62, 0x00, 0x02, 0x00, 0x00, 0x05, 0x00, 0x01, 0x0D, 0x0A, // Compressed data
            0x2D, 0xB4, 0x34, 0xFB  // CRC32
        };
        test_png_data_.insert(test_png_data_.end(), idat_chunk.begin(), idat_chunk.end());
        
        // Add IEND footer
        test_png_data_.insert(test_png_data_.end(), valid_png_footer_.begin(), valid_png_footer_.end());
        
        // Create corrupted PNG data (missing IEND)
        corrupted_png_data_ = valid_png_header_;
        corrupted_png_data_.insert(corrupted_png_data_.end(), ihdr_chunk.begin(), ihdr_chunk.end());
        for (int i = 0; i < 50; ++i) {
            corrupted_png_data_.push_back(static_cast<uint8_t>(i % 256));
        }
        // No IEND - simulates corrupted file
        
        // Create non-PNG data
        non_png_data_ = {0xFF, 0xD8, 0xFF, 0xE0}; // JPEG header
    }
    
    std::unique_ptr<PngCarver> carver_;
    std::string test_data_dir_;
    std::vector<uint8_t> valid_png_header_;
    std::vector<uint8_t> valid_png_footer_;
    std::vector<uint8_t> test_png_data_;
    std::vector<uint8_t> corrupted_png_data_;
    std::vector<uint8_t> non_png_data_;
};

TEST_F(PngCarverTest, DetectPngSignature) {
    // Test if the carver can detect PNG signatures (replaces canCarve test)
    auto signatures = carver_->getFileSignatures();
    EXPECT_FALSE(signatures.empty());
    
    // Check if PNG header is in the signatures
    bool found_png_header = false;
    for (const auto& sig : signatures) {
        if (sig.size() >= 8 && 
            sig[0] == 0x89 && sig[1] == 0x50 && sig[2] == 0x4E && sig[3] == 0x47 &&
            sig[4] == 0x0D && sig[5] == 0x0A && sig[6] == 0x1A && sig[7] == 0x0A) {
            found_png_header = true;
            break;
        }
    }
    EXPECT_TRUE(found_png_header);
}

TEST_F(PngCarverTest, CarveValidPng) {
    auto results = carver_->carveFiles(test_png_data_.data(), test_png_data_.size(), 0);
    
    ASSERT_EQ(results.size(), 1);
    
    const auto& file = results[0];
    EXPECT_EQ(file.file_type, "PNG");
    EXPECT_EQ(file.start_offset, 0);
    EXPECT_EQ(file.file_size, test_png_data_.size());
    EXPECT_GT(file.confidence_score, 0.7); // Should have high confidence
    EXPECT_FALSE(file.is_fragmented);
    EXPECT_EQ(file.fragments.size(), 1);
}

TEST_F(PngCarverTest, CarveMultiplePngs) {
    // Create buffer with multiple PNG files
    std::vector<uint8_t> multi_png_data;
    
    // First PNG
    multi_png_data.insert(multi_png_data.end(), test_png_data_.begin(), test_png_data_.end());
    
    // Some padding
    for (int i = 0; i < 50; ++i) {
        multi_png_data.push_back(0x00);
    }
    
    // Second PNG
    size_t second_png_offset = multi_png_data.size();
    multi_png_data.insert(multi_png_data.end(), test_png_data_.begin(), test_png_data_.end());
    
    auto results = carver_->carveFiles(multi_png_data.data(), multi_png_data.size(), 0);
    
    ASSERT_EQ(results.size(), 2);
    
    // Check first PNG
    EXPECT_EQ(results[0].start_offset, 0);
    EXPECT_EQ(results[0].file_size, test_png_data_.size());
    
    // Check second PNG
    EXPECT_EQ(results[1].start_offset, second_png_offset);
    EXPECT_EQ(results[1].file_size, test_png_data_.size());
}

TEST_F(PngCarverTest, HandleCorruptedPng) {
    auto results = carver_->carveFiles(corrupted_png_data_.data(), corrupted_png_data_.size(), 0);
    
    // Should still detect the PNG header but with lower confidence
    ASSERT_FALSE(results.empty());
    
    const auto& file = results[0];
    EXPECT_EQ(file.file_type, "PNG");
    EXPECT_EQ(file.start_offset, 0);
    EXPECT_LT(file.confidence_score, 0.7); // Lower confidence due to missing IEND
}

TEST_F(PngCarverTest, ValidateFile) {
    // Replace SaveCarvedFile test with ValidateFile test
    auto results = carver_->carveFiles(test_png_data_.data(), test_png_data_.size(), 0);
    ASSERT_FALSE(results.empty());
    
    const auto& file = results[0];
    double confidence = carver_->validateFile(file, test_png_data_.data());
    
    EXPECT_GT(confidence, 0.0);
    EXPECT_LE(confidence, 1.0);
    EXPECT_GT(confidence, 0.7); // Good PNG should have high confidence
}

TEST_F(PngCarverTest, ConfidenceScoring) {
    // Replace CalculateConfidence test with confidence from carveFiles results
    
    // Valid PNG with IEND should have high confidence
    auto results = carver_->carveFiles(test_png_data_.data(), test_png_data_.size(), 0);
    ASSERT_FALSE(results.empty());
    EXPECT_GT(results[0].confidence_score, 0.7);
    
    // PNG without IEND should have lower confidence
    results = carver_->carveFiles(corrupted_png_data_.data(), corrupted_png_data_.size(), 0);
    ASSERT_FALSE(results.empty());
    EXPECT_LT(results[0].confidence_score, 0.7);
    EXPECT_GT(results[0].confidence_score, 0.4); // But still reasonable
    
    // Non-PNG data should not be detected
    results = carver_->carveFiles(non_png_data_.data(), non_png_data_.size(), 0);
    EXPECT_TRUE(results.empty());
}

TEST_F(PngCarverTest, GetSupportedTypes) {
    auto types = carver_->getSupportedTypes();
    EXPECT_FALSE(types.empty());
    
    bool found_png = false;
    for (const auto& type : types) {
        if (type == "png" || type == "PNG") {
            found_png = true;
            break;
        }
    }
    EXPECT_TRUE(found_png);
}

TEST_F(PngCarverTest, ValidateFileStructureIndirectly) {
    // Test PNG structure validation through validateFile instead of direct access
    auto results = carver_->carveFiles(test_png_data_.data(), test_png_data_.size(), 0);
    ASSERT_FALSE(results.empty());
    
    const auto& file = results[0];
    double good_confidence = carver_->validateFile(file, test_png_data_.data());
    EXPECT_GT(good_confidence, 0.7);
    
    // Test with corrupted structure
    results = carver_->carveFiles(corrupted_png_data_.data(), corrupted_png_data_.size(), 0);
    if (!results.empty()) {
        double bad_confidence = carver_->validateFile(results[0], corrupted_png_data_.data());
        EXPECT_LT(bad_confidence, 0.7);
    }
}

TEST_F(PngCarverTest, LargeDataHandling) {
    // Create a large buffer with PNG data at various positions
    std::vector<uint8_t> large_data(10000, 0x00);
    
    // Insert PNG at position 1000
    std::copy(test_png_data_.begin(), test_png_data_.end(), large_data.begin() + 1000);
    
    auto results = carver_->carveFiles(large_data.data(), large_data.size(), 0);
    
    ASSERT_GE(results.size(), 1);
    bool found_png = false;
    for (const auto& file : results) {
        if (file.start_offset == 1000) {
            found_png = true;
            EXPECT_EQ(file.file_size, test_png_data_.size());
            break;
        }
    }
    EXPECT_TRUE(found_png);
}

TEST_F(PngCarverTest, EdgeCases) {
    // Test with null pointer
    auto results = carver_->carveFiles(nullptr, 0, 0);
    EXPECT_TRUE(results.empty());
    
    // Test with zero size
    results = carver_->carveFiles(test_png_data_.data(), 0, 0);
    EXPECT_TRUE(results.empty());
    
    // Test with very small data
    uint8_t small_data[] = {0x89};
    results = carver_->carveFiles(small_data, 1, 0);
    EXPECT_TRUE(results.empty());
}

TEST_F(PngCarverTest, GetFooters) {
    // Test that the carver reports expected footer patterns
    auto footers = carver_->getFileFooters();
    EXPECT_FALSE(footers.empty());
    
    bool found_iend = false;
    for (const auto& footer : footers) {
        if (footer.size() >= 4 && 
            footer[0] == 0x49 && footer[1] == 0x45 && 
            footer[2] == 0x4E && footer[3] == 0x44) {
            found_iend = true;
            break;
        }
    }
    EXPECT_TRUE(found_iend);
}

TEST_F(PngCarverTest, MaxFileSize) {
    EXPECT_GT(carver_->getMaxFileSize(), 0);
    EXPECT_LE(carver_->getMaxFileSize(), 500ULL * 1024 * 1024); // 500MB limit as per implementation
}
