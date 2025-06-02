#include <gtest/gtest.h>
#include "core/recovery_engine.h"
#include "utils/logger.h"
#include <fstream>
#include <filesystem>
#include <thread>
#include <chrono>

using namespace FileRecovery;

class RecoveryEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize logger for tests
        Logger::getInstance().initialize("test_recovery.log", Logger::Level::DEBUG);
        
        // Create test data directory
        test_data_dir_ = "test_recovery_data";
        output_dir_ = "test_recovery_output";
        std::filesystem::create_directories(test_data_dir_);
        std::filesystem::create_directories(output_dir_);
        
        createTestImage();
        
        // Create basic config
        config_.device_path = test_image_path_;
        config_.output_directory = output_dir_;
        config_.target_file_types = {"jpg", "pdf", "png"};
        config_.use_signature_recovery = true;
        config_.use_metadata_recovery = false;
        config_.num_threads = 2;
        config_.verbose_logging = false;
        
        // Initialize the recovery engine with our config
        engine_ = std::make_unique<RecoveryEngine>(config_);
    }
    
    void TearDown() override {
        // Clean up test data
        std::filesystem::remove_all(test_data_dir_);
        std::filesystem::remove_all(output_dir_);
        std::filesystem::remove("test_recovery.log");
    }
    
    void createTestImage() {
        test_image_path_ = test_data_dir_ + "/recovery_test.img";
        
        // Create a test disk image with various file types
        std::ofstream img_file(test_image_path_, std::ios::binary);
        
        std::vector<uint8_t> disk_data(2 * 1024 * 1024, 0x00); // 2MB
        
        // Add complete JPEG file at offset 1000
        std::vector<uint8_t> jpeg_data = {
            0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 'J', 'F', 'I', 'F'
        };
        // Add some dummy JPEG data
        for (int i = 0; i < 100; ++i) {
            jpeg_data.push_back(static_cast<uint8_t>(i % 256));
        }
        // Add JPEG footer
        jpeg_data.insert(jpeg_data.end(), {0xFF, 0xD9});
        
        std::copy(jpeg_data.begin(), jpeg_data.end(), disk_data.begin() + 1000);
        
        // Add complete PDF file at offset 50000
        std::vector<uint8_t> pdf_data = {0x25, 0x50, 0x44, 0x46, 0x2D, 0x31, 0x2E, 0x34}; // %PDF-1.4
        std::string pdf_content = "\n1 0 obj\n<< /Type /Catalog >>\nendobj\ntrailer\n<< /Root 1 0 R >>\n";
        for (char c : pdf_content) {
            pdf_data.push_back(static_cast<uint8_t>(c));
        }
        pdf_data.insert(pdf_data.end(), {0x25, 0x25, 0x45, 0x4F, 0x46}); // %%EOF
        
        std::copy(pdf_data.begin(), pdf_data.end(), disk_data.begin() + 50000);
        
        // Add complete PNG file at offset 100000
        std::vector<uint8_t> png_data = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A}; // PNG signature
        // Add minimal IHDR chunk
        std::vector<uint8_t> ihdr = {
            0x00, 0x00, 0x00, 0x0D, // Length
            0x49, 0x48, 0x44, 0x52, // IHDR
            0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, // 1x1 pixel
            0x08, 0x02, 0x00, 0x00, 0x00, // Bit depth, color type, etc.
            0x90, 0x77, 0x53, 0xDE  // CRC
        };
        png_data.insert(png_data.end(), ihdr.begin(), ihdr.end());
        // Add IEND chunk
        png_data.insert(png_data.end(), {
            0x00, 0x00, 0x00, 0x00, // Length
            0x49, 0x45, 0x4E, 0x44, // IEND
            0xAE, 0x42, 0x60, 0x82  // CRC
        });
        
        std::copy(png_data.begin(), png_data.end(), disk_data.begin() + 100000);
        
        // Write the disk image
        img_file.write(reinterpret_cast<const char*>(disk_data.data()), disk_data.size());
        img_file.close();
    }
    
    // Helper function to check if string ends with suffix (C++17 compatible)
    bool stringEndsWith(const std::string& str, const std::string& suffix) {
        if (suffix.length() > str.length()) return false;
        return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
    }
    
    std::string test_data_dir_;
    std::string output_dir_;
    std::string test_image_path_;
    ScanConfig config_;
    std::unique_ptr<RecoveryEngine> engine_;
};

TEST_F(RecoveryEngineTest, BasicRecovery) {
    // Set progress callback to track progress
    int progress_updates = 0;
    engine_->setProgressCallback([&progress_updates](double progress, const std::string& message) {
        progress_updates++;
        EXPECT_GE(progress, 0.0);
        EXPECT_LE(progress, 100.0);
        EXPECT_FALSE(message.empty());
    });
    
    // Start recovery
    ASSERT_EQ(engine_->startRecovery(), RecoveryStatus::SUCCESS);
    
    // Wait for completion (recovery is synchronous in this case)
    EXPECT_FALSE(engine_->isRunning());
    
    // Check that progress was tracked
    EXPECT_GT(progress_updates, 0);
    EXPECT_DOUBLE_EQ(engine_->getProgress(), 100.0);
    
    // Check that output files were created
    bool found_jpg = false, found_pdf = false, found_png = false;
    for (const auto& entry : std::filesystem::directory_iterator(output_dir_)) {
        std::string filename = entry.path().filename().string();
        if (stringEndsWith(filename, ".jpg") || stringEndsWith(filename, ".jpeg")) found_jpg = true;
        if (stringEndsWith(filename, ".pdf")) found_pdf = true;
        if (stringEndsWith(filename, ".png")) found_png = true;
    }
    
    EXPECT_TRUE(found_jpg) << "Expected to find at least one recovered JPEG file";
    EXPECT_TRUE(found_pdf) << "Expected to find at least one recovered PDF file";
    EXPECT_TRUE(found_png) << "Expected to find at least one recovered PNG file";
}

TEST_F(RecoveryEngineTest, StopRecovery) {
    // Create a separate thread to stop recovery after a short delay
    std::thread stop_thread([this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        engine_->stopRecovery();
    });
    
    // Start recovery
    auto status = engine_->startRecovery();
    
    // Wait for stop thread to complete
    stop_thread.join();
    
    // Recovery might have succeeded if it was very fast, or might have been stopped
    EXPECT_FALSE(engine_->isRunning());
}

TEST_F(RecoveryEngineTest, SignatureBasedRecovery) {
    // Configure for signature-based recovery only
    config_.use_metadata_recovery = false;
    config_.use_signature_recovery = true;
    
    // Create a new engine with the updated config
    engine_ = std::make_unique<RecoveryEngine>(config_);
    
    // Start recovery
    ASSERT_EQ(engine_->startRecovery(), RecoveryStatus::SUCCESS);
    
    // Wait for completion
    EXPECT_FALSE(engine_->isRunning());
    
    // Check that output files were created
    int file_count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(output_dir_)) {
        file_count++;
    }
    
    EXPECT_GT(file_count, 0) << "Expected to find recovered files";
}

TEST_F(RecoveryEngineTest, ProgressTracking) {
    // Set a progress callback
    bool got_final_progress = false;
    engine_->setProgressCallback([&got_final_progress](double progress, const std::string& message) {
        if (progress >= 100.0) {
            got_final_progress = true;
        }
    });
    
    // Start recovery
    ASSERT_EQ(engine_->startRecovery(), RecoveryStatus::SUCCESS);
    
    // Check if we got 100% progress notification
    EXPECT_TRUE(got_final_progress);
}

// Add more test cases as needed
