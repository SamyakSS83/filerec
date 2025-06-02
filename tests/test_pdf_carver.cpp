#include <gtest/gtest.h>
#include "carvers/pdf_carver.h"
#include "utils/logger.h"
#include <fstream>
#include <vector>
#include <filesystem>

using namespace FileRecovery;

class PdfCarverTest : public ::testing::Test {
protected:
    void SetUp() override {
        carver_ = std::make_unique<PdfCarver>();
        
        // Initialize logger for tests
        Logger::getInstance().initialize("test_pdf.log", Logger::Level::DEBUG);
        
        // Create test data directory
        test_data_dir_ = "test_pdf_data";
        std::filesystem::create_directories(test_data_dir_);
        
        // Create test PDF data
        createTestPdfData();
    }
    
    void TearDown() override {
        // Clean up test data
        std::filesystem::remove_all(test_data_dir_);
        // std::filesystem::remove("test_pdf.log");
    }
    
    void createTestPdfData() {
        // Valid PDF header: %PDF-1.4
        valid_pdf_header_ = {0x25, 0x50, 0x44, 0x46, 0x2D, 0x31, 0x2E, 0x34};
        
        // Valid PDF footer: %%EOF
        valid_pdf_footer_ = {0x25, 0x25, 0x45, 0x4F, 0x46};
        
        // Create a simple test PDF file
        test_pdf_data_.insert(test_pdf_data_.end(), valid_pdf_header_.begin(), valid_pdf_header_.end());
        
        // Add minimal PDF structure
        std::string pdf_content = "\n1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n";
        pdf_content += "2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n";
        pdf_content += "3 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>\nendobj\n";
        pdf_content += "xref\n0 4\n0000000000 65535 f \n";
        pdf_content += "0000000009 00000 n \n0000000058 00000 n \n0000000115 00000 n \n";
        pdf_content += "trailer\n<< /Size 4 /Root 1 0 R >>\nstartxref\n173\n";
        
        for (char c : pdf_content) {
            test_pdf_data_.push_back(static_cast<uint8_t>(c));
        }
        
        // Add footer
        test_pdf_data_.insert(test_pdf_data_.end(), valid_pdf_footer_.begin(), valid_pdf_footer_.end());
        
        // Create corrupted PDF data (missing footer)
        corrupted_pdf_data_ = valid_pdf_header_;
        for (int i = 0; i < 50; ++i) {
            corrupted_pdf_data_.push_back(static_cast<uint8_t>(i % 256));
        }
        // No footer - simulates corrupted file
        
        // Create non-PDF data
        non_pdf_data_ = {0xFF, 0xD8, 0xFF, 0xE0}; // JPEG header
    }
    
    std::unique_ptr<PdfCarver> carver_;
    std::string test_data_dir_;
    std::vector<uint8_t> valid_pdf_header_;
    std::vector<uint8_t> valid_pdf_footer_;
    std::vector<uint8_t> test_pdf_data_;
    std::vector<uint8_t> corrupted_pdf_data_;
    std::vector<uint8_t> non_pdf_data_;
};

TEST_F(PdfCarverTest, CanCarveValidPdf) {
    // Replace canCarve with checks for getFileSignatures()
    auto signatures = carver_->getFileSignatures();
    bool canDetectPdf = false;
    for (const auto& sig : signatures) {
        if (sig.size() >= 5 && sig[0] == 0x25 && sig[1] == 0x50 && sig[2] == 0x44 && sig[3] == 0x46) {
            canDetectPdf = true;
            break;
        }
    }
    EXPECT_TRUE(canDetectPdf);
}

TEST_F(PdfCarverTest, CarveValidPdf) {
    // Replace carve with carveFiles
    auto results = carver_->carveFiles(test_pdf_data_.data(), test_pdf_data_.size(), 0);
    
    ASSERT_FALSE(results.empty());
    
    const auto& file = results[0];
    EXPECT_EQ(file.file_type, "PDF");
    EXPECT_EQ(file.start_offset, 0);
    EXPECT_EQ(file.file_size, test_pdf_data_.size());
    EXPECT_GT(file.confidence_score, 0.7); // Should have high confidence
}

TEST_F(PdfCarverTest, CarveMultiplePdfs) {
    // Create buffer with multiple PDF files
    std::vector<uint8_t> multi_pdf_data;
    
    // First PDF
    multi_pdf_data.insert(multi_pdf_data.end(), test_pdf_data_.begin(), test_pdf_data_.end());
    
    // Some padding
    for (int i = 0; i < 50; ++i) {
        multi_pdf_data.push_back(0x00);
    }
    
    // Second PDF
    size_t second_pdf_offset = multi_pdf_data.size();
    multi_pdf_data.insert(multi_pdf_data.end(), test_pdf_data_.begin(), test_pdf_data_.end());
    
    auto results = carver_->carveFiles(multi_pdf_data.data(), multi_pdf_data.size(), 0);
    
    ASSERT_EQ(results.size(), 2);
    
    // Check first PDF
    EXPECT_EQ(results[0].start_offset, 0);
    EXPECT_EQ(results[0].file_size, test_pdf_data_.size());
    
    // Check second PDF
    EXPECT_EQ(results[1].start_offset, second_pdf_offset);
    EXPECT_EQ(results[1].file_size, test_pdf_data_.size());
}

TEST_F(PdfCarverTest, HandleCorruptedPdf) {
    auto results = carver_->carveFiles(corrupted_pdf_data_.data(), corrupted_pdf_data_.size(), 0);
    
    // Should still detect the PDF header but with lower confidence
    ASSERT_FALSE(results.empty());
    
    const auto& file = results[0];
    EXPECT_EQ(file.file_type, "PDF");
    EXPECT_EQ(file.start_offset, 0);
    EXPECT_LT(file.confidence_score, 0.7); // Lower confidence due to missing footer
}

// Replace SaveCarvedFile test with a test for validateFile
TEST_F(PdfCarverTest, ValidateFile) {
    auto results = carver_->carveFiles(test_pdf_data_.data(), test_pdf_data_.size(), 0);
    ASSERT_FALSE(results.empty());
    
    const auto& file = results[0];
    double confidence = carver_->validateFile(file, test_pdf_data_.data());
    
    EXPECT_GT(confidence, 0.0);
    EXPECT_LE(confidence, 1.0);
    EXPECT_GT(confidence, 0.7); // Good PDF should have high confidence
}

// Replace CalculateConfidence test with a more appropriate test
TEST_F(PdfCarverTest, ConfidenceScoring) {
    // Test confidence scores from carving
    
    // Valid PDF with footer should have high confidence
    auto results = carver_->carveFiles(test_pdf_data_.data(), test_pdf_data_.size(), 0);
    ASSERT_FALSE(results.empty());
    EXPECT_GT(results[0].confidence_score, 0.7);
    
    // PDF without footer should have lower confidence
    results = carver_->carveFiles(corrupted_pdf_data_.data(), corrupted_pdf_data_.size(), 0);
    ASSERT_FALSE(results.empty());
    EXPECT_LT(results[0].confidence_score, 0.7);
    EXPECT_GT(results[0].confidence_score, 0.4); // But still reasonable
    
    // Non-PDF data should not be detected or have very low confidence
    results = carver_->carveFiles(non_pdf_data_.data(), non_pdf_data_.size(), 0);
    if (!results.empty()) {
        EXPECT_LT(results[0].confidence_score, 0.3);
    }
}

TEST_F(PdfCarverTest, LargeDataHandling) {
    // Create a large buffer with PDF data at various positions
    std::vector<uint8_t> large_data(10000, 0x00);
    
    // Insert PDF at position 1000
    std::copy(test_pdf_data_.begin(), test_pdf_data_.end(), large_data.begin() + 1000);
    
    auto results = carver_->carveFiles(large_data.data(), large_data.size(), 0);
    
    ASSERT_GE(results.size(), 1);
    bool found_pdf = false;
    for (const auto& file : results) {
        if (file.start_offset == 1000) {
            found_pdf = true;
            EXPECT_EQ(file.file_size, test_pdf_data_.size());
            break;
        }
    }
    EXPECT_TRUE(found_pdf);
}

TEST_F(PdfCarverTest, EdgeCases) {
    // Test with null pointer
    auto results = carver_->carveFiles(nullptr, 0, 0);
    EXPECT_TRUE(results.empty());
    
    // Test with zero size
    results = carver_->carveFiles(test_pdf_data_.data(), 0, 0);
    EXPECT_TRUE(results.empty());
    
    // Test with very small data
    uint8_t small_data[] = {0x25};
    results = carver_->carveFiles(small_data, 1, 0);
    EXPECT_TRUE(results.empty());
}

TEST_F(PdfCarverTest, MaxFileSize) {
    EXPECT_GT(carver_->getMaxFileSize(), 0);
    EXPECT_LE(carver_->getMaxFileSize(), 1ULL * 1024 * 1024 * 1024); // Should be reasonable limit
}
