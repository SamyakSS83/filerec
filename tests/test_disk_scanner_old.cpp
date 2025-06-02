#include <gtest/gtest.h>
#include "core/disk_scanner.h"
#include "utils/logger.h"
#include <fstream>
#include <filesystem>

using namespace FileRecovery;

class DiskScannerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test data directory and files
        test_data_dir_ = "test_scanner_data";
        std::filesystem::create_directories(test_data_dir_);
        
        createTestImage();
        
        // Initialize scanner with test image path
        scanner_ = std::make_unique<DiskScanner>(test_image_path_);
        
        // Initialize logger for tests
        Logger::getInstance().initialize("test_scanner.log", Logger::Level::DEBUG);
    }
    
    void TearDown() override {
        // Clean up test data
        std::filesystem::remove_all(test_data_dir_);
        std::filesystem::remove("test_scanner.log");
    }
    
    void createTestImage() {
        test_image_path_ = test_data_dir_ + "/test_disk.img";
        
        // Create a small test disk image (1MB)
        std::ofstream img_file(test_image_path_, std::ios::binary);
        
        // Write some test data with file signatures
        std::vector<uint8_t> disk_data(1024 * 1024, 0x00);
        
        // Add JPEG signature at offset 1000
        std::vector<uint8_t> jpeg_sig = {0xFF, 0xD8, 0xFF, 0xE0};
        std::copy(jpeg_sig.begin(), jpeg_sig.end(), disk_data.begin() + 1000);
        
        // Add PDF signature at offset 5000
        std::vector<uint8_t> pdf_sig = {0x25, 0x50, 0x44, 0x46, 0x2D};
        std::copy(pdf_sig.begin(), pdf_sig.end(), disk_data.begin() + 5000);
        
        // Add PNG signature at offset 10000
        std::vector<uint8_t> png_sig = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
        std::copy(png_sig.begin(), png_sig.end(), disk_data.begin() + 10000);
        
        // Write the data
        img_file.write(reinterpret_cast<const char*>(disk_data.data()), disk_data.size());
        img_file.close();
    }
    
    std::unique_ptr<DiskScanner> scanner_;
    std::string test_data_dir_;
    std::string test_image_path_;
};

TEST_F(DiskScannerTest, InitializeScanner) {
    EXPECT_TRUE(scanner_->initialize(test_image_path_));
    EXPECT_EQ(scanner_->getDiskSize(), 1024 * 1024);
    EXPECT_TRUE(scanner_->isInitialized());
}

TEST_F(DiskScannerTest, InitializeInvalidPath) {
    EXPECT_FALSE(scanner_->initialize("/nonexistent/path"));
    EXPECT_FALSE(scanner_->isInitialized());
    EXPECT_EQ(scanner_->getDiskSize(), 0);
}

TEST_F(DiskScannerTest, ReadData) {
    ASSERT_TRUE(scanner_->initialize(test_image_path_));
    
    // Test reading from beginning
    std::vector<uint8_t> buffer(1024);
    EXPECT_TRUE(scanner_->readData(0, buffer.data(), buffer.size()));
    
    // Test reading from middle
    EXPECT_TRUE(scanner_->readData(5000, buffer.data(), buffer.size()));
    
    // Test reading beyond end
    EXPECT_FALSE(scanner_->readData(1024 * 1024, buffer.data(), 1));
}

TEST_F(DiskScannerTest, ReadDataChunks) {
    ASSERT_TRUE(scanner_->initialize(test_image_path_));
    
    size_t chunk_size = 4096;
    auto chunks = scanner_->readDataChunks(0, 1024 * 1024, chunk_size);
    
    EXPECT_EQ(chunks.size(), (1024 * 1024 + chunk_size - 1) / chunk_size);
    
    // Verify each chunk has correct size (except possibly the last one)
    for (size_t i = 0; i < chunks.size() - 1; ++i) {
        EXPECT_EQ(chunks[i].size(), chunk_size);
    }
    
    // Last chunk may be smaller
    EXPECT_LE(chunks.back().size(), chunk_size);
}

TEST_F(DiskScannerTest, ScanForSignatures) {
    ASSERT_TRUE(scanner_->initialize(test_image_path_));
    
    // Define signatures to search for
    std::vector<std::vector<uint8_t>> signatures = {
        {0xFF, 0xD8, 0xFF, 0xE0}, // JPEG
        {0x25, 0x50, 0x44, 0x46}, // PDF
        {0x89, 0x50, 0x4E, 0x47}  // PNG
    };
    
    auto results = scanner_->scanForSignatures(signatures, 0, 1024 * 1024);
    
    // Should find the signatures we placed
    EXPECT_GE(results.size(), 3);
    
    // Check that offsets are approximately correct
    bool found_jpeg = false, found_pdf = false, found_png = false;
    for (const auto& result : results) {
        if (result.offset >= 1000 && result.offset < 1010) found_jpeg = true;
        if (result.offset >= 5000 && result.offset < 5010) found_pdf = true;
        if (result.offset >= 10000 && result.offset < 10010) found_png = true;
    }
    
    EXPECT_TRUE(found_jpeg);
    EXPECT_TRUE(found_pdf);
    EXPECT_TRUE(found_png);
}

TEST_F(DiskScannerTest, GetDiskGeometry) {
    ASSERT_TRUE(scanner_->initialize(test_image_path_));
    
    auto geometry = scanner_->getDiskGeometry();
    EXPECT_GT(geometry.sector_size, 0);
    EXPECT_GT(geometry.total_sectors, 0);
    EXPECT_EQ(geometry.total_sectors * geometry.sector_size, 1024 * 1024);
}

TEST_F(DiskScannerTest, ParallelScanning) {
    ASSERT_TRUE(scanner_->initialize(test_image_path_));
    
    // Enable parallel scanning
    scanner_->setParallelScanning(true, 4); // 4 threads
    
    std::vector<std::vector<uint8_t>> signatures = {
        {0xFF, 0xD8, 0xFF, 0xE0} // JPEG
    };
    
    auto results = scanner_->scanForSignatures(signatures, 0, 1024 * 1024);
    EXPECT_GE(results.size(), 1);
}

TEST_F(DiskScannerTest, ProgressCallback) {
    ASSERT_TRUE(scanner_->initialize(test_image_path_));
    
    int progress_calls = 0;
    auto progress_callback = [&progress_calls](double progress) {
        progress_calls++;
        EXPECT_GE(progress, 0.0);
        EXPECT_LE(progress, 100.0);
    };
    
    scanner_->setProgressCallback(progress_callback);
    
    std::vector<std::vector<uint8_t>> signatures = {
        {0xFF, 0xD8, 0xFF, 0xE0}
    };
    
    scanner_->scanForSignatures(signatures, 0, 1024 * 1024);
    
    EXPECT_GT(progress_calls, 0);
}

TEST_F(DiskScannerTest, ScanSpecificSectors) {
    ASSERT_TRUE(scanner_->initialize(test_image_path_));
    
    // Scan only a specific range where we know there's a signature
    std::vector<std::vector<uint8_t>> signatures = {
        {0xFF, 0xD8, 0xFF, 0xE0} // JPEG
    };
    
    auto results = scanner_->scanForSignatures(signatures, 500, 2000);
    EXPECT_GE(results.size(), 1);
    
    // Should find the JPEG signature at offset 1000
    bool found = false;
    for (const auto& result : results) {
        if (result.offset >= 1000 && result.offset < 1010) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(DiskScannerTest, HandleReadErrors) {
    ASSERT_TRUE(scanner_->initialize(test_image_path_));
    
    // Try to read beyond disk size
    std::vector<uint8_t> buffer(1024);
    EXPECT_FALSE(scanner_->readData(2 * 1024 * 1024, buffer.data(), buffer.size()));
    
    // Try to read with invalid size
    EXPECT_FALSE(scanner_->readData(0, buffer.data(), 2 * 1024 * 1024));
}

TEST_F(DiskScannerTest, MemoryUsage) {
    ASSERT_TRUE(scanner_->initialize(test_image_path_));
    
    auto usage = scanner_->getMemoryUsage();
    EXPECT_GE(usage.total_allocated, 0);
    EXPECT_GE(usage.currently_used, 0);
    EXPECT_LE(usage.currently_used, usage.total_allocated);
}

TEST_F(DiskScannerTest, ScanStatistics) {
    ASSERT_TRUE(scanner_->initialize(test_image_path_));
    
    std::vector<std::vector<uint8_t>> signatures = {
        {0xFF, 0xD8, 0xFF, 0xE0},
        {0x25, 0x50, 0x44, 0x46}
    };
    
    scanner_->scanForSignatures(signatures, 0, 1024 * 1024);
    
    auto stats = scanner_->getScanStatistics();
    EXPECT_GT(stats.bytes_scanned, 0);
    EXPECT_GT(stats.scan_time_ms, 0);
    EXPECT_GE(stats.signatures_found, 0);
}

TEST_F(DiskScannerTest, BadSectors) {
    ASSERT_TRUE(scanner_->initialize(test_image_path_));
    
    // Simulate marking some sectors as bad
    scanner_->markBadSector(100);
    scanner_->markBadSector(200);
    
    auto bad_sectors = scanner_->getBadSectors();
    EXPECT_GE(bad_sectors.size(), 2);
    
    EXPECT_TRUE(std::find(bad_sectors.begin(), bad_sectors.end(), 100) != bad_sectors.end());
    EXPECT_TRUE(std::find(bad_sectors.begin(), bad_sectors.end(), 200) != bad_sectors.end());
}
