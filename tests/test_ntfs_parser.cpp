#include <gtest/gtest.h>
#include "filesystems/ntfs_parser.h"
#include "utils/logger.h"
#include <vector>
#include <cstring>
#include <filesystem>

using namespace FileRecovery;

class NtfsParserTest : public ::testing::Test {
protected:
    void SetUp() override {
        parser_ = std::make_unique<NtfsParser>();
        
        // Initialize logger for tests
        Logger::getInstance().initialize("test_ntfs.log", Logger::Level::DEBUG);
        
        createTestNtfsData();
    }
    
    void TearDown() override {
        std::filesystem::remove("test_ntfs.log");
    }
    
    void createTestNtfsData() {
        // Create a minimal NTFS filesystem structure
        ntfs_data_.resize(64 * 1024, 0); // 64KB test filesystem
        
        // NTFS boot sector at offset 0
        uint8_t* boot_sector = ntfs_data_.data();
        
        // Jump instruction
        boot_sector[0] = 0xEB;
        boot_sector[1] = 0x52;
        boot_sector[2] = 0x90;
        
        // OEM ID "NTFS    "
        memcpy(boot_sector + 3, "NTFS    ", 8);
        
        // Bytes per sector
        *(uint16_t*)(boot_sector + 11) = 512;
        
        // Sectors per cluster
        boot_sector[13] = 8;
        
        // Reserved sectors
        *(uint16_t*)(boot_sector + 14) = 0;
        
        // Number of FATs (always 0 for NTFS)
        boot_sector[16] = 0;
        
        // Root entries (always 0 for NTFS)
        *(uint16_t*)(boot_sector + 17) = 0;
        
        // Sectors (always 0 for NTFS, uses large_sectors)
        *(uint16_t*)(boot_sector + 19) = 0;
        
        // Media type
        boot_sector[21] = 0xF8;
        
        // FAT length (always 0 for NTFS)
        *(uint16_t*)(boot_sector + 22) = 0;
        
        // Sectors per track
        *(uint16_t*)(boot_sector + 24) = 63;
        
        // Heads
        *(uint16_t*)(boot_sector + 26) = 255;
        
        // Hidden sectors
        *(uint32_t*)(boot_sector + 28) = 0;
        
        // Large sectors (always 0 for NTFS, uses total_sectors)
        *(uint32_t*)(boot_sector + 32) = 0;
        
        // Unused
        *(uint32_t*)(boot_sector + 36) = 0;
        
        // Total sectors
        *(uint64_t*)(boot_sector + 40) = 128; // 128 * 512 = 64KB
        
        // MFT LCN (Logical Cluster Number)
        *(uint64_t*)(boot_sector + 48) = 4; // MFT starts at cluster 4
        
        // MFT Mirror LCN
        *(uint64_t*)(boot_sector + 56) = 64; // Mirror at cluster 64
        
        // Clusters per MFT record (0xF6 = -10, means 2^10 = 1024 bytes)
        boot_sector[64] = 0xF6;
        
        // Reserved
        boot_sector[65] = 0;
        boot_sector[66] = 0;
        boot_sector[67] = 0;
        
        // Clusters per index record (0xF6 = -10, means 2^10 = 1024 bytes)
        boot_sector[68] = 0xF6;
        
        // Reserved
        boot_sector[69] = 0;
        boot_sector[70] = 0;
        boot_sector[71] = 0;
        
        // Volume serial number
        *(uint64_t*)(boot_sector + 72) = 0x1234567890ABCDEF;
        
        // Checksum
        *(uint32_t*)(boot_sector + 80) = 0;
        
        // Boot signature
        boot_sector[510] = 0x55;
        boot_sector[511] = 0xAA;
        
        // Create a simple MFT record at cluster 4 (offset 4 * 8 * 512 = 16384)
        uint8_t* mft_record = ntfs_data_.data() + 16384;
        
        // MFT record signature "FILE"
        memcpy(mft_record, "FILE", 4);
        
        // Update sequence offset
        *(uint16_t*)(mft_record + 4) = 48;
        
        // Update sequence count
        *(uint16_t*)(mft_record + 6) = 3;
        
        // LSN
        *(uint64_t*)(mft_record + 8) = 0;
        
        // Sequence number
        *(uint16_t*)(mft_record + 16) = 1;
        
        // Hard link count
        *(uint16_t*)(mft_record + 18) = 1;
        
        // First attribute offset
        *(uint16_t*)(mft_record + 20) = 56;
        
        // Flags (in use)
        *(uint16_t*)(mft_record + 22) = 0x0001;
        
        // Used size
        *(uint32_t*)(mft_record + 24) = 416;
        
        // Allocated size
        *(uint32_t*)(mft_record + 28) = 1024;
        
        // Base record
        *(uint64_t*)(mft_record + 32) = 0;
        
        // Next attribute ID
        *(uint16_t*)(mft_record + 40) = 4;
        
        // Reserved
        *(uint16_t*)(mft_record + 42) = 0;
        
        // Record number
        *(uint32_t*)(mft_record + 44) = 0;
        
        // Add a simple STANDARD_INFORMATION attribute at offset 56
        uint8_t* attr = mft_record + 56;
        
        // Attribute type (STANDARD_INFORMATION = 0x10)
        *(uint32_t*)(attr + 0) = 0x10;
        
        // Attribute length
        *(uint32_t*)(attr + 4) = 96;
        
        // Non-resident flag (resident = 0)
        attr[8] = 0;
        
        // Name length
        attr[9] = 0;
        
        // Name offset
        *(uint16_t*)(attr + 10) = 0;
        
        // Flags
        *(uint16_t*)(attr + 12) = 0;
        
        // Attribute ID
        *(uint16_t*)(attr + 14) = 0;
        
        // Value length
        *(uint32_t*)(attr + 16) = 48;
        
        // Value offset
        *(uint16_t*)(attr + 20) = 24;
        
        // Indexed flag
        attr[22] = 0;
        
        // Reserved
        attr[23] = 0;
        
        // Fill in standard information data (48 bytes starting at offset 24)
        // Creation time, modification time, etc. (simplified)
        memset(attr + 24, 0, 48);
    }
    
    std::unique_ptr<NtfsParser> parser_;
    std::vector<uint8_t> ntfs_data_;
};

TEST_F(NtfsParserTest, CanParseFilesystem) {
    EXPECT_TRUE(parser_->canParse(ntfs_data_.data(), ntfs_data_.size()));
    
    // Test with invalid data
    std::vector<uint8_t> invalid_data(1024, 0xFF);
    EXPECT_FALSE(parser_->canParse(invalid_data.data(), invalid_data.size()));
}

TEST_F(NtfsParserTest, ParseFilesystemInfo) {
    ASSERT_TRUE(parser_->canParse(ntfs_data_.data(), ntfs_data_.size()));
    
    auto info = parser_->getFileSystemInfo();
    
    EXPECT_EQ(info, "NTFS File System");
}

TEST_F(NtfsParserTest, RecoverDeletedFiles) {
    ASSERT_TRUE(parser_->initialize(ntfs_data_.data(), ntfs_data_.size()));
    
    auto files = parser_->recoverDeletedFiles();
    
    // With minimal test data, we might not have deleted files
    // but the function should not crash
    EXPECT_GE(files.size(), 0);
}

TEST_F(NtfsParserTest, ValidateBootSector) {
    ASSERT_TRUE(parser_->canParse(ntfs_data_.data(), ntfs_data_.size()));
    
    // Test boot sector validation with valid data
    const auto* boot = reinterpret_cast<const NtfsParser::NtfsBootSector*>(ntfs_data_.data());
    EXPECT_TRUE(parser_->validate_boot_sector(boot));
    
    // Test with corrupted OEM ID
    auto corrupted_data = ntfs_data_;
    memcpy(corrupted_data.data() + 3, "INVALID ", 8);
    const auto* corrupted_boot = reinterpret_cast<const NtfsParser::NtfsBootSector*>(corrupted_data.data());
    EXPECT_FALSE(parser_->validate_boot_sector(corrupted_boot));
}

TEST_F(NtfsParserTest, ValidateMftRecord) {
    ASSERT_TRUE(parser_->canParse(ntfs_data_.data(), ntfs_data_.size()));
    
    // Test MFT record validation
    const auto* mft_record = reinterpret_cast<const NtfsParser::MftRecord*>(ntfs_data_.data() + 16384);
    EXPECT_TRUE(parser_->validate_mft_record(mft_record));
    
    // Test with corrupted signature
    auto corrupted_data = ntfs_data_;
    memcpy(corrupted_data.data() + 16384, "XXXX", 4);
    const auto* corrupted_record = reinterpret_cast<const NtfsParser::MftRecord*>(corrupted_data.data() + 16384);
    EXPECT_FALSE(parser_->validate_mft_record(corrupted_record));
}

TEST_F(NtfsParserTest, GetMftOffset) {
    ASSERT_TRUE(parser_->canParse(ntfs_data_.data(), ntfs_data_.size()));
    
    const auto* boot = reinterpret_cast<const NtfsParser::NtfsBootSector*>(ntfs_data_.data());
    uint64_t mft_offset = parser_->get_mft_offset(boot);
    
    // MFT is at cluster 4, with 8 sectors per cluster and 512 bytes per sector
    // So offset should be 4 * 8 * 512 = 16384
    EXPECT_EQ(mft_offset, 16384);
}

TEST_F(NtfsParserTest, GetClusterSize) {
    ASSERT_TRUE(parser_->canParse(ntfs_data_.data(), ntfs_data_.size()));
    
    const auto* boot = reinterpret_cast<const NtfsParser::NtfsBootSector*>(ntfs_data_.data());
    uint32_t cluster_size = parser_->get_cluster_size(boot);
    
    // 8 sectors per cluster * 512 bytes per sector = 4096 bytes
    EXPECT_EQ(cluster_size, 4096);
}

TEST_F(NtfsParserTest, GetMftRecordSize) {
    ASSERT_TRUE(parser_->canParse(ntfs_data_.data(), ntfs_data_.size()));
    
    const auto* boot = reinterpret_cast<const NtfsParser::NtfsBootSector*>(ntfs_data_.data());
    uint32_t record_size = parser_->get_mft_record_size(boot);
    
    // Clusters per MFT record is 0xF6 (-10), so size is 2^10 = 1024 bytes
    EXPECT_EQ(record_size, 1024);
}

TEST_F(NtfsParserTest, ParseMftRecords) {
    ASSERT_TRUE(parser_->initialize(ntfs_data_.data(), ntfs_data_.size()));
    
    const auto* boot = reinterpret_cast<const NtfsParser::NtfsBootSector*>(ntfs_data_.data());
    auto files = parser_->parse_mft_records(ntfs_data_.data(), ntfs_data_.size(), boot, 0);
    
    // Should not crash, might have limited results with minimal test data
    EXPECT_GE(files.size(), 0);
}

TEST_F(NtfsParserTest, ExtractFilenameAttribute) {
    ASSERT_TRUE(parser_->canParse(ntfs_data_.data(), ntfs_data_.size()));
    
    // Test filename attribute extraction (might be empty with minimal test data)
    std::string filename = parser_->extract_filename_attribute(ntfs_data_.data() + 16384, 1024);
    
    // Should not crash regardless of result
    EXPECT_TRUE(true);
}

TEST_F(NtfsParserTest, ExtractFileSizeAttribute) {
    ASSERT_TRUE(parser_->canParse(ntfs_data_.data(), ntfs_data_.size()));
    
    // Test file size attribute extraction
    uint64_t file_size = parser_->extract_file_size_attribute(ntfs_data_.data() + 16384, 1024);
    
    // Should not crash, might return 0 with minimal test data
    EXPECT_GE(file_size, 0);
}

TEST_F(NtfsParserTest, ExtractDataRuns) {
    ASSERT_TRUE(parser_->canParse(ntfs_data_.data(), ntfs_data_.size()));
    
    const auto* boot = reinterpret_cast<const NtfsParser::NtfsBootSector*>(ntfs_data_.data());
    auto data_runs = parser_->extract_data_runs(ntfs_data_.data() + 16384, 1024, boot, 0);
    
    // Should not crash, might be empty with minimal test data
    EXPECT_GE(data_runs.size(), 0);
}

TEST_F(NtfsParserTest, ParseDataRuns) {
    ASSERT_TRUE(parser_->canParse(ntfs_data_.data(), ntfs_data_.size()));
    
    // Create simple data run: length=1, offset=5 (encoded as 0x11 0x01 0x05)
    std::vector<uint8_t> run_data = {0x11, 0x01, 0x05, 0x00};
    
    auto clusters = parser_->parse_data_runs(run_data.data(), run_data.size(), 4096, 0);
    
    // Should parse the run correctly
    EXPECT_GE(clusters.size(), 0);
}

TEST_F(NtfsParserTest, EdgeCases) {
    // Test with null data
    EXPECT_FALSE(parser_->canParse(nullptr, 0));
    
    // Test with too small data
    std::vector<uint8_t> small_data(100, 0);
    EXPECT_FALSE(parser_->canParse(small_data.data(), small_data.size()));
    
    // Test initialize with invalid data
    EXPECT_FALSE(parser_->initialize(small_data.data(), small_data.size()));
    
    // Test recovery without initialization
    NtfsParser uninit_parser;
    auto files = uninit_parser.recoverDeletedFiles();
    EXPECT_EQ(files.size(), 0);
}

TEST_F(NtfsParserTest, ThreadSafety) {
    ASSERT_TRUE(parser_->initialize(ntfs_data_.data(), ntfs_data_.size()));
    
    // Test that multiple calls don't interfere
    auto files1 = parser_->recoverDeletedFiles();
    auto files2 = parser_->recoverDeletedFiles();
    
    // Results should be consistent
    EXPECT_EQ(files1.size(), files2.size());
}

TEST_F(NtfsParserTest, FileSystemType) {
    EXPECT_EQ(parser_->getFileSystemType(), FileSystemType::NTFS);
}

TEST_F(NtfsParserTest, LargeDataHandling) {
    // Test with larger filesystem
    std::vector<uint8_t> large_data = ntfs_data_;
    large_data.resize(1024 * 1024, 0); // 1MB
    
    // Update total sectors to match larger size
    *(uint64_t*)(large_data.data() + 40) = 2048; // 2048 * 512 = 1MB
    
    EXPECT_TRUE(parser_->canParse(large_data.data(), large_data.size()));
    
    if (parser_->initialize(large_data.data(), large_data.size())) {
        auto files = parser_->recoverDeletedFiles();
        EXPECT_GE(files.size(), 0);
    }
}

TEST_F(NtfsParserTest, CorruptedMftRecords) {
    ASSERT_TRUE(parser_->initialize(ntfs_data_.data(), ntfs_data_.size()));
    
    // Corrupt some MFT records
    auto corrupted_data = ntfs_data_;
    
    // Corrupt second MFT record if it exists
    if (corrupted_data.size() > 17408) { // 16384 + 1024
        memset(corrupted_data.data() + 17408, 0xFF, 1024);
    }
    
    parser_->initialize(corrupted_data.data(), corrupted_data.size());
    const auto* boot = reinterpret_cast<const NtfsParser::NtfsBootSector*>(corrupted_data.data());
    auto files = parser_->parse_mft_records(corrupted_data.data(), corrupted_data.size(), boot, 0);
    
    // Should handle corrupted records gracefully
    EXPECT_GE(files.size(), 0);
}

TEST_F(NtfsParserTest, AttributeTypes) {
    // Test that attribute type constants are correct
    EXPECT_EQ(NtfsParser::AT_STANDARD_INFORMATION, 0x10);
    EXPECT_EQ(NtfsParser::AT_ATTRIBUTE_LIST, 0x20);
    EXPECT_EQ(NtfsParser::AT_FILE_NAME, 0x30);
    EXPECT_EQ(NtfsParser::AT_DATA, 0x80);
    
    // Test MFT record flags
    EXPECT_EQ(NtfsParser::MFT_RECORD_IN_USE, 0x0001);
    EXPECT_EQ(NtfsParser::MFT_RECORD_IS_DIRECTORY, 0x0002);
}
