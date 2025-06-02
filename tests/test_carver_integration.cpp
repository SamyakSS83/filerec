#include <gtest/gtest.h>
#include "carvers/jpeg_carver.h"
#include "carvers/pdf_carver.h"
#include "carvers/png_carver.h"
#include "carvers/zip_carver.h"
#include "utils/logger.h"
#include <vector>
#include <memory>
#include <filesystem>

using namespace FileRecovery;

class CarverIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        jpeg_carver_ = std::make_unique<JpegCarver>();
        pdf_carver_ = std::make_unique<PdfCarver>();
        png_carver_ = std::make_unique<PngCarver>();
        zip_carver_ = std::make_unique<ZipCarver>();
        
        // Initialize logger
        Logger::getInstance().initialize("test_integration.log", Logger::Level::DEBUG);
        
        // Create mixed test data
        createMixedTestData();
    }
    
    void TearDown() override {
        std::filesystem::remove("test_integration.log");
    }
    
    void createMixedTestData() {
        // Create a buffer with various file types
        mixed_data_.resize(10000, 0);
        
        // JPEG signature at offset 0
        const uint8_t jpeg_sig[] = {0xFF, 0xD8, 0xFF, 0xE0};
        std::copy(jpeg_sig, jpeg_sig + 4, mixed_data_.begin());
        
        // Add JPEG footer at offset 1000
        const uint8_t jpeg_footer[] = {0xFF, 0xD9};
        std::copy(jpeg_footer, jpeg_footer + 2, mixed_data_.begin() + 1000);
        
        // PDF signature at offset 2000
        const uint8_t pdf_sig[] = {0x25, 0x50, 0x44, 0x46, 0x2D, 0x31, 0x2E};
        std::copy(pdf_sig, pdf_sig + 7, mixed_data_.begin() + 2000);
        
        // PDF footer at offset 3000
        const uint8_t pdf_footer[] = {0x25, 0x25, 0x45, 0x4F, 0x46};
        std::copy(pdf_footer, pdf_footer + 5, mixed_data_.begin() + 3000);
        
        // PNG signature at offset 4000
        const uint8_t png_sig[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
        std::copy(png_sig, png_sig + 8, mixed_data_.begin() + 4000);
        
        // PNG IEND chunk at offset 5000
        const uint8_t png_end[] = {0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82};
        std::copy(png_end, png_end + 12, mixed_data_.begin() + 5000);
        
        // ZIP signature at offset 6000
        const uint8_t zip_sig[] = {0x50, 0x4B, 0x03, 0x04};
        std::copy(zip_sig, zip_sig + 4, mixed_data_.begin() + 6000);
        
        // ZIP footer at offset 7000
        const uint8_t zip_end[] = {0x50, 0x4B, 0x05, 0x06};
        std::copy(zip_end, zip_end + 4, mixed_data_.begin() + 7000);
    }
    
    std::unique_ptr<JpegCarver> jpeg_carver_;
    std::unique_ptr<PdfCarver> pdf_carver_;
    std::unique_ptr<PngCarver> png_carver_;
    std::unique_ptr<ZipCarver> zip_carver_;
    std::vector<uint8_t> mixed_data_;
};

TEST_F(CarverIntegrationTest, DetectMultipleFileTypes) {
    // Test that each carver can find its file type in mixed data
    
    auto jpeg_results = jpeg_carver_->carveFiles(mixed_data_.data(), mixed_data_.size(), 0);
    auto pdf_results = pdf_carver_->carveFiles(mixed_data_.data(), mixed_data_.size(), 0);
    auto png_results = png_carver_->carveFiles(mixed_data_.data(), mixed_data_.size(), 0);
    auto zip_results = zip_carver_->carveFiles(mixed_data_.data(), mixed_data_.size(), 0);
    
    bool found_jpeg = false;
    for (const auto& file : jpeg_results) {
        if (file.start_offset == 0) found_jpeg = true;
    }
    
    bool found_pdf = false;
    for (const auto& file : pdf_results) {
        if (file.start_offset == 2000) found_pdf = true;
    }
    
    bool found_png = false;
    for (const auto& file : png_results) {
        if (file.start_offset == 4000) found_png = true;
    }
    
    bool found_zip = false;
    for (const auto& file : zip_results) {
        if (file.start_offset == 6000) found_zip = true;
    }
    
    EXPECT_TRUE(found_jpeg);
    EXPECT_TRUE(found_pdf);
    EXPECT_TRUE(found_png);
    EXPECT_TRUE(found_zip);
}

TEST_F(CarverIntegrationTest, CompareConfidenceScores) {
    // Test that file types match the expected confidence patterns
    
    auto jpeg_results = jpeg_carver_->carveFiles(mixed_data_.data(), mixed_data_.size(), 0);
    auto pdf_results = pdf_carver_->carveFiles(mixed_data_.data(), mixed_data_.size(), 0);
    auto png_results = png_carver_->carveFiles(mixed_data_.data(), mixed_data_.size(), 0);
    auto zip_results = zip_carver_->carveFiles(mixed_data_.data(), mixed_data_.size(), 0);
    
    // Valid files should have reasonable confidence
    for (const auto& file : jpeg_results) {
        if (file.start_offset == 0) {
            EXPECT_GT(file.confidence_score, 0.5);
        }
    }
    
    for (const auto& file : pdf_results) {
        if (file.start_offset == 2000) {
            EXPECT_GT(file.confidence_score, 0.5);
        }
    }
    
    for (const auto& file : png_results) {
        if (file.start_offset == 4000) {
            EXPECT_GT(file.confidence_score, 0.5);
        }
    }
    
    for (const auto& file : zip_results) {
        if (file.start_offset == 6000) {
            EXPECT_GT(file.confidence_score, 0.5);
        }
    }
}