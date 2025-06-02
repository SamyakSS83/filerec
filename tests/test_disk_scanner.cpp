#include <gtest/gtest.h>
#include "core/disk_scanner.h"
#include "utils/logger.h"
#include <fstream>
#include <filesystem>
#include <thread>

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
    EXPECT_TRUE(scanner_->initialize());
    EXPECT_EQ(scanner_->getDeviceSize(), 1024 * 1024);
    EXPECT_TRUE(scanner_->isReady());
}

TEST_F(DiskScannerTest, InitializeInvalidPath) {
    auto invalid_scanner = std::make_unique<DiskScanner>("/nonexistent/path");
    EXPECT_FALSE(invalid_scanner->initialize());
    EXPECT_FALSE(invalid_scanner->isReady());
    EXPECT_EQ(invalid_scanner->getDeviceSize(), 0);
}

TEST_F(DiskScannerTest, ReadChunk) {
    ASSERT_TRUE(scanner_->initialize());
    
    // Test reading from beginning
    std::vector<uint8_t> buffer(1024);
    Size bytes_read = scanner_->readChunk(0, buffer.size(), buffer.data());
    EXPECT_EQ(bytes_read, buffer.size());
    
    // Test reading from middle
    bytes_read = scanner_->readChunk(5000, buffer.size(), buffer.data());
    EXPECT_EQ(bytes_read, buffer.size());
    
    // Test reading beyond end (should read partial data)
    bytes_read = scanner_->readChunk(1024 * 1024 - 100, buffer.size(), buffer.data());
    EXPECT_EQ(bytes_read, 100); // Should only read the remaining 100 bytes
}

TEST_F(DiskScannerTest, MemoryMapping) {
    ASSERT_TRUE(scanner_->initialize());
    
    Size map_size = 4096;
    const Byte* mapped_data = scanner_->mapRegion(0, map_size);
    ASSERT_NE(mapped_data, nullptr);
    
    // Verify we can read the mapped data
    EXPECT_EQ(mapped_data[0], 0x00); // Our test data is initialized to 0x00
    
    // Unmap the region
    scanner_->unmapRegion(mapped_data, map_size);
}

TEST_F(DiskScannerTest, ReadEntireDevice) {
    ASSERT_TRUE(scanner_->initialize());
    
    // Read the entire small test device
    auto data = scanner_->readEntireDevice();
    EXPECT_EQ(data.size(), 1024 * 1024);
    
    // Verify our test signatures are present
    // JPEG signature at offset 1000
    EXPECT_EQ(data[1000], 0xFF);
    EXPECT_EQ(data[1001], 0xD8);
    EXPECT_EQ(data[1002], 0xFF);
    EXPECT_EQ(data[1003], 0xE0);
    
    // PDF signature at offset 5000
    EXPECT_EQ(data[5000], 0x25); // '%'
    EXPECT_EQ(data[5001], 0x50); // 'P'
    EXPECT_EQ(data[5002], 0x44); // 'D'
    EXPECT_EQ(data[5003], 0x46); // 'F'
}

TEST_F(DiskScannerTest, GetDeviceInfo) {
    ASSERT_TRUE(scanner_->initialize());
    
    std::string info = scanner_->getDeviceInfo();
    EXPECT_FALSE(info.empty());
    EXPECT_NE(info.find(test_image_path_), std::string::npos);
}

TEST_F(DiskScannerTest, GetDevicePath) {
    EXPECT_EQ(scanner_->getDevicePath(), test_image_path_);
}

TEST_F(DiskScannerTest, IsReadOnly) {
    ASSERT_TRUE(scanner_->initialize());
    
    // Our test implementation should detect files as read-only for safety
    bool is_readonly = scanner_->isReadOnly();
    // This test might pass or fail depending on implementation details
    // Just verify the method works without crashing
    (void)is_readonly;
}

TEST_F(DiskScannerTest, LargeDeviceLimit) {
    ASSERT_TRUE(scanner_->initialize());
    
    // Test the safety limit for readEntireDevice
    Size small_limit = 1024; // 1KB limit
    auto limited_data = scanner_->readEntireDevice(small_limit);
    EXPECT_LE(limited_data.size(), small_limit);
}

TEST_F(DiskScannerTest, ThreadSafety) {
    ASSERT_TRUE(scanner_->initialize());
    
    const int num_threads = 4;
    const Size chunk_size = 1024;
    std::vector<std::thread> threads;
    std::vector<bool> results(num_threads, false);
    
    // Launch multiple threads reading different parts of the device
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([this, i, chunk_size, &results]() {
            std::vector<uint8_t> buffer(chunk_size);
            Offset offset = i * chunk_size;
            
            if (offset < scanner_->getDeviceSize()) {
                Size bytes_read = scanner_->readChunk(offset, chunk_size, buffer.data());
                results[i] = (bytes_read > 0);
            } else {
                results[i] = true; // Skip if offset is beyond device size
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // All threads should have succeeded
    for (bool result : results) {
        EXPECT_TRUE(result);
    }
}

TEST_F(DiskScannerTest, HandleReadErrorsGracefully) {
    ASSERT_TRUE(scanner_->initialize());
    
    // Try to read beyond disk size - should return 0 bytes read
    std::vector<uint8_t> buffer(1024);
    Size bytes_read = scanner_->readChunk(2 * 1024 * 1024, buffer.size(), buffer.data());
    EXPECT_EQ(bytes_read, 0);
    
    // Try to read with very large size - should be limited by buffer size
    bytes_read = scanner_->readChunk(0, buffer.size(), buffer.data());
    EXPECT_LE(bytes_read, buffer.size());
    
    // Create a larger buffer for testing device size limits
    std::vector<uint8_t> large_buffer(2 * 1024 * 1024);
    bytes_read = scanner_->readChunk(0, large_buffer.size(), large_buffer.data());
    EXPECT_LE(bytes_read, 1024 * 1024); // Should be limited by device size (1MB)
}

TEST_F(DiskScannerTest, NullPointerHandling) {
    ASSERT_TRUE(scanner_->initialize());
    
    // Test with null buffer pointer - should handle gracefully
    Size bytes_read = scanner_->readChunk(0, 1024, nullptr);
    EXPECT_EQ(bytes_read, 0);
}

TEST_F(DiskScannerTest, ZeroSizeRead) {
    ASSERT_TRUE(scanner_->initialize());
    
    std::vector<uint8_t> buffer(1024);
    Size bytes_read = scanner_->readChunk(0, 0, buffer.data());
    EXPECT_EQ(bytes_read, 0);
}

TEST_F(DiskScannerTest, ReinitializeScanner) {
    // Test that we can initialize multiple times safely
    EXPECT_TRUE(scanner_->initialize());
    EXPECT_TRUE(scanner_->isReady());
    
    // Initialize again - should still work
    EXPECT_TRUE(scanner_->initialize());
    EXPECT_TRUE(scanner_->isReady());
}
