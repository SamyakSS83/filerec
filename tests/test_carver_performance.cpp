#include <gtest/gtest.h>
#include "carvers/jpeg_carver.h"
#include "carvers/pdf_carver.h"
#include "carvers/png_carver.h"
#include "carvers/zip_carver.h"
#include "utils/logger.h"
#include <vector>
#include <chrono>
#include <random>
#include <filesystem>

using namespace FileRecovery;

class CarverPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        jpeg_carver_ = std::make_unique<JpegCarver>();
        pdf_carver_ = std::make_unique<PdfCarver>();
        png_carver_ = std::make_unique<PngCarver>();
        zip_carver_ = std::make_unique<ZipCarver>();
        
        // Initialize logger
        Logger::getInstance().initialize("test_performance.log", Logger::Level::INFO);
        
        // Create large test data
        createLargeTestData();
    }
    
    void TearDown() override {
        std::filesystem::remove("test_performance.log");
    }
    
    void createLargeTestData() {
        // Create a 10MB buffer with random data
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> distrib(0, 255);
        
        const size_t buffer_size = 10 * 1024 * 1024;
        large_data_.resize(buffer_size);
        
        for (size_t i = 0; i < buffer_size; ++i) {
            large_data_[i] = static_cast<uint8_t>(distrib(gen));
        }
        
        // Add 10 JPEG signatures at random positions
        addRandomSignatures(jpeg_signatures_, 10);
        
        // Add 10 PDF signatures at random positions
        addRandomSignatures(pdf_signatures_, 10);
        
        // Add 10 PNG signatures at random positions
        addRandomSignatures(png_signatures_, 10);
        
        // Add 10 ZIP signatures at random positions
        addRandomSignatures(zip_signatures_, 10);
    }
    
    void addRandomSignatures(const std::vector<uint8_t>& signature, int count) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> distrib(0, large_data_.size() - signature.size() - 1);
        
        for (int i = 0; i < count; ++i) {
            size_t pos = distrib(gen);
            std::copy(signature.begin(), signature.end(), large_data_.begin() + pos);
        }
    }
    
    std::unique_ptr<JpegCarver> jpeg_carver_;
    std::unique_ptr<PdfCarver> pdf_carver_;
    std::unique_ptr<PngCarver> png_carver_;
    std::unique_ptr<ZipCarver> zip_carver_;
    std::vector<uint8_t> large_data_;
    
    const std::vector<uint8_t> jpeg_signatures_ = {0xFF, 0xD8, 0xFF, 0xE0};
    const std::vector<uint8_t> pdf_signatures_ = {0x25, 0x50, 0x44, 0x46, 0x2D, 0x31, 0x2E};
    const std::vector<uint8_t> png_signatures_ = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    const std::vector<uint8_t> zip_signatures_ = {0x50, 0x4B, 0x03, 0x04};
};

TEST_F(CarverPerformanceTest, JpegCarverPerformance) {
    auto start = std::chrono::high_resolution_clock::now();
    
    auto results = jpeg_carver_->carveFiles(large_data_.data(), large_data_.size(), 0);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "JPEG carver took " << duration.count() << "ms for 10MB data" << std::endl;
    
    // Performance should be reasonable (adjust based on your hardware)
    EXPECT_LE(duration.count(), 5000); // Should take less than 5 seconds
}

TEST_F(CarverPerformanceTest, PdfCarverPerformance) {
    auto start = std::chrono::high_resolution_clock::now();
    
    auto results = pdf_carver_->carveFiles(large_data_.data(), large_data_.size(), 0);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "PDF carver took " << duration.count() << "ms for 10MB data" << std::endl;
    
    // Performance should be reasonable (adjust based on your hardware)
    EXPECT_LE(duration.count(), 5000); // Should take less than 5 seconds
}