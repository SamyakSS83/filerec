#include <gtest/gtest.h>
#include "core/recovery_engine.h"
#include "utils/logger.h"
#include <fstream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <numeric>

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
        config_.target_file_types = {"JPEG", "PDF", "PNG"}; // Use uppercase to match carver implementations
        config_.use_signature_recovery = true;
        config_.use_metadata_recovery = false;
        config_.num_threads = 2;
        config_.verbose_logging = true; // Enable verbose logging to debug issues
        config_.chunk_size = 512 * 1024; // 512KB chunks for faster testing
        
        // Initialize the recovery engine with our config
        engine_ = std::make_unique<RecoveryEngine>(config_);
    }
    
    void TearDown() override {
        // Clean up test data
        // std::filesystem::remove_all(test_data_dir_);
        // std::filesystem::remove_all(output_dir_);
        // std::filesystem::remove("test_recovery.log");
    }
    
    void createTestImage() {
        test_image_path_ = test_data_dir_ + "/recovery_test.img";
        
        // Create a test disk image with various file types
        std::ofstream img_file(test_image_path_, std::ios::binary);
        
        std::vector<uint8_t> disk_data(2 * 1024 * 1024, 0x00); // 2MB
        
        // Add complete JPEG file at offset 1000 (same as before)
        // ...
        
        // Improved PDF file at offset 50000
        std::vector<uint8_t> pdf_data = {0x25, 0x50, 0x44, 0x46, 0x2D, 0x31, 0x2E, 0x35}; // %PDF-1.5
        std::string pdf_content = "\n"
            "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n"
            "2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n"
            "3 0 obj\n<< /Type /Page /MediaBox [0 0 612 792] /Parent 2 0 R >>\nendobj\n"
            "xref\n0 4\n0000000000 65535 f\n0000000010 00000 n\n0000000064 00000 n\n0000000125 00000 n\n"
            "trailer\n<< /Root 1 0 R /Size 4 >>\n"
            "startxref\n200\n";
        for (char c : pdf_content) {
            pdf_data.push_back(static_cast<uint8_t>(c));
        }
        pdf_data.insert(pdf_data.end(), {0x25, 0x25, 0x45, 0x4F, 0x46}); // %%EOF
        
        std::copy(pdf_data.begin(), pdf_data.end(), disk_data.begin() + 50000);
        
        // Improved PNG file at offset 100000
        // Provide a pre-validated small PNG file
        std::vector<uint8_t> png_data = {
            0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, // PNG signature
            0x00, 0x00, 0x00, 0x0D, // IHDR length
            0x49, 0x48, 0x44, 0x52, // "IHDR"
            0x00, 0x00, 0x00, 0x01, // Width: 1
            0x00, 0x00, 0x00, 0x01, // Height: 1
            0x08,                   // Bit depth
            0x06,                   // Color type (RGBA)
            0x00,                   // Compression
            0x00,                   // Filter
            0x00,                   // Interlace
            0x1F, 0x15, 0xC4, 0x89, // CRC
            // IDAT chunk with minimal pixel data
            0x00, 0x00, 0x00, 0x0A, // IDAT length
            0x49, 0x44, 0x41, 0x54, // "IDAT"
            0x78, 0x9C, 0x63, 0x00, 0x01, 0x00, 0x00, 0x05, 0x00, 0x01, // zlib data
            0x0D, 0x0A, 0x2D, 0xB4, // CRC
            // IEND chunk
            0x00, 0x00, 0x00, 0x00, // IEND length
            0x49, 0x45, 0x4E, 0x44, // "IEND"
            0xAE, 0x42, 0x60, 0x82  // CRC
        };
        
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
    
    // Helper to check for files with any of the extensions
    bool findFileWithExtensions(const std::string& dir, const std::vector<std::string>& extensions) {
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            std::string filename = entry.path().filename().string();
            for (const auto& ext : extensions) {
                if (stringEndsWith(filename, ext)) {
                    return true;
                }
            }
        }
        return false;
    }
    
    std::string test_data_dir_;
    std::string output_dir_;
    std::string test_image_path_;
    ScanConfig config_;
    std::unique_ptr<RecoveryEngine> engine_;
};

TEST_F(RecoveryEngineTest, BasicRecovery) {
    // Enhanced debug logging
    Logger::getInstance().setConsoleOutput(true); // Ensure logs appear in console
    LOG_INFO("Starting BasicRecovery test with test image: " + test_image_path_);
    LOG_INFO("Output directory: " + output_dir_);
    
    // Log the config settings
    LOG_INFO("Target file types: " + 
             std::accumulate(config_.target_file_types.begin(), config_.target_file_types.end(), 
                           std::string(), 
                           [](const std::string& a, const std::string& b) {
                               return a.empty() ? b : a + ", " + b;
                           }));
    
    // Set progress callback to track progress with enhanced logging
    int progress_updates = 0;
    engine_->setProgressCallback([&progress_updates](double progress, const std::string& message) {
        progress_updates++;
        EXPECT_GE(progress, 0.0);
        EXPECT_LE(progress, 100.0);
        EXPECT_FALSE(message.empty());
        
        // Print progress for debugging
        std::cout << "Progress: " << progress << "% - " << message << std::endl;
        LOG_DEBUG("Recovery progress: " + std::to_string(progress) + "% - " + message);
    });
    
    // Start recovery
    LOG_INFO("Starting recovery engine");
    RecoveryStatus status = engine_->startRecovery();
    LOG_INFO("Recovery complete with status: " + std::to_string(static_cast<int>(status)));
    
    ASSERT_EQ(status, RecoveryStatus::SUCCESS) << "Recovery failed with status: " << static_cast<int>(status);
    
    // Wait for completion (recovery is synchronous in this case)
    EXPECT_FALSE(engine_->isRunning());
    
    // Check that progress was tracked
    EXPECT_GT(progress_updates, 0);
    EXPECT_DOUBLE_EQ(engine_->getProgress(), 100.0);
    
    // Enhanced file listing
    LOG_INFO("Files in output directory:");
    std::cout << "Files in output directory:" << std::endl;
    int file_count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(output_dir_)) {
        file_count++;
        std::string file_info = "  " + entry.path().filename().string() + 
                              " (" + std::to_string(entry.file_size()) + " bytes)";
        std::cout << file_info << std::endl;
        LOG_INFO(file_info);
    }
    LOG_INFO("Total files found: " + std::to_string(file_count));
    
    // Check that output files were created with more extension options
    bool found_jpg = findFileWithExtensions(output_dir_, {".jpg", ".jpeg", ".JPG", ".JPEG"});
    bool found_pdf = findFileWithExtensions(output_dir_, {".pdf", ".PDF"});
    bool found_png = findFileWithExtensions(output_dir_, {".png", ".PNG"});
    
    LOG_INFO("Found JPEG files: " + std::string(found_jpg ? "YES" : "NO"));
    LOG_INFO("Found PDF files: " + std::string(found_pdf ? "YES" : "NO"));
    LOG_INFO("Found PNG files: " + std::string(found_png ? "YES" : "NO"));
    
    EXPECT_TRUE(found_jpg) << "Expected to find at least one recovered JPEG file";
    EXPECT_TRUE(found_pdf) << "Expected to find at least one recovered PDF file";
    EXPECT_TRUE(found_png) << "Expected to find at least one recovered PNG file";
    
    // Get more detailed recovery info
    LOG_INFO("Total recovered files according to engine: " + 
             std::to_string(engine_->getRecoveredFileCount()));
    
    // Additional debug for failing test
    if (!found_jpg || !found_pdf || !found_png) {
        LOG_WARNING("Test may fail - some file types not found");
        std::cout << "Total recovered files: " << engine_->getRecoveredFileCount() << std::endl;
        
        // Check file naming format - maybe files exist but with different extensions
        LOG_INFO("Checking for files with alternative naming patterns:");
        for (const auto& entry : std::filesystem::directory_iterator(output_dir_)) {
            std::string filename = entry.path().filename().string();
            LOG_INFO("Found file: " + filename);
            
            // Try to determine file type from name
            if (filename.find("jpeg") != std::string::npos || 
                filename.find("jpg") != std::string::npos ||
                filename.find("JPEG") != std::string::npos ||
                filename.find("JPG") != std::string::npos) {
                LOG_INFO("  This appears to be a JPEG file");
            }
            else if (filename.find("pdf") != std::string::npos ||
                    filename.find("PDF") != std::string::npos) {
                LOG_INFO("  This appears to be a PDF file");
            }
            else if (filename.find("png") != std::string::npos ||
                    filename.find("PNG") != std::string::npos) {
                LOG_INFO("  This appears to be a PNG file");
            }
            else {
                LOG_INFO("  Unknown file type");
            }
        }
    }
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
