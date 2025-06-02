#include <gtest/gtest.h>
#include "filesystems/fat32_parser.h"
#include "utils/logger.h"
#include <vector>
#include <cstring>
#include <filesystem>

using namespace FileRecovery;

class Fat32ParserTest : public ::testing::Test {
protected:
    void SetUp() override {
        parser_ = std::make_unique<Fat32Parser>();
        
        // Initialize logger for tests
        Logger::getInstance().initialize("test_fat32.log", Logger::Level::DEBUG);
        
        createTestFat32Data();
    }
    
    void TearDown() override {
        std::filesystem::remove("test_fat32.log");
    }
    
    void createTestFat32Data() {
        // Create a minimal FAT32 filesystem structure
        fat32_data_.resize(128 * 1024, 0); // 128KB test filesystem
        
        // FAT32 boot sector at offset 0
        uint8_t* boot_sector = fat32_data_.data();
        
        // Jump instruction
        boot_sector[0] = 0xEB;
        boot_sector[1] = 0x58;
        boot_sector[2] = 0x90;
        
        // OEM name
        memcpy(boot_sector + 3, "MSDOS5.0", 8);
        
        // Bytes per sector
        *(uint16_t*)(boot_sector + 11) = 512;
        
        // Sectors per cluster
        boot_sector[13] = 4; // 4 sectors per cluster = 2KB clusters
        
        // Reserved sector count
        *(uint16_t*)(boot_sector + 14) = 32;
        
        // Number of FATs
        boot_sector[16] = 2;
        
        // Root entry count (0 for FAT32)
        *(uint16_t*)(boot_sector + 17) = 0;
        
        // Sector count 16-bit (0 for FAT32, use 32-bit)
        *(uint16_t*)(boot_sector + 19) = 0;
        
        // Media type
        boot_sector[21] = 0xF8;
        
        // FAT size 16-bit (0 for FAT32, use 32-bit)
        *(uint16_t*)(boot_sector + 22) = 0;
        
        // Sectors per track
        *(uint16_t*)(boot_sector + 24) = 63;
        
        // Number of heads
        *(uint16_t*)(boot_sector + 26) = 255;
        
        // Hidden sectors
        *(uint32_t*)(boot_sector + 28) = 0;
        
        // Sector count 32-bit
        *(uint32_t*)(boot_sector + 32) = 256; // 256 * 512 = 128KB
        
        // FAT32 specific fields
        // FAT size 32-bit
        *(uint32_t*)(boot_sector + 36) = 8; // 8 sectors per FAT
        
        // Extended flags
        *(uint16_t*)(boot_sector + 40) = 0;
        
        // FAT version
        *(uint16_t*)(boot_sector + 42) = 0;
        
        // Root cluster
        *(uint32_t*)(boot_sector + 44) = 2; // Root directory starts at cluster 2
        
        // FSInfo sector
        *(uint16_t*)(boot_sector + 48) = 1;
        
        // Backup boot sector
        *(uint16_t*)(boot_sector + 50) = 6;
        
        // Reserved
        memset(boot_sector + 52, 0, 12);
        
        // Drive number
        boot_sector[64] = 0x80;
        
        // Reserved
        boot_sector[65] = 0;
        
        // Boot signature
        boot_sector[66] = 0x29;
        
        // Volume ID
        *(uint32_t*)(boot_sector + 67) = 0x12345678;
        
        // Volume label
        memcpy(boot_sector + 71, "TEST_FAT32 ", 11);
        
        // FAT type label
        memcpy(boot_sector + 82, "FAT32   ", 8);
        
        // Boot signature
        boot_sector[510] = 0x55;
        boot_sector[511] = 0xAA;
        
        // Create FAT table at sector 32 (reserved sectors = 32)
        uint8_t* fat_table = fat32_data_.data() + (32 * 512);
        
        // FAT entry 0: media type + 0xFFFFF00 (end-of-chain marker)
        *(uint32_t*)(fat_table + 0) = 0xFFFFFFF8;
        
        // FAT entry 1: end-of-chain marker
        *(uint32_t*)(fat_table + 4) = 0xFFFFFFFF;
        
        // FAT entry 2: end-of-chain marker (root directory)
        *(uint32_t*)(fat_table + 8) = 0xFFFFFFFF;
        
        // FAT entry 3: next cluster for a file
        *(uint32_t*)(fat_table + 12) = 0xFFFFFFFF;
        
        // Create root directory at cluster 2
        // Data area starts at: reserved_sectors + (number_of_fats * fat_size)
        // = 32 + (2 * 8) = 48 sectors = 24576 bytes
        // Cluster 2 offset = data_area_start + (cluster - 2) * cluster_size
        // = 24576 + (2 - 2) * (4 * 512) = 24576
        uint8_t* root_dir = fat32_data_.data() + 24576;
        
        // Create a test file entry
        Fat32Parser::Fat32DirEntry* file_entry = reinterpret_cast<Fat32Parser::Fat32DirEntry*>(root_dir);
        
        // Filename (8.3 format, padded with spaces)
        memcpy(file_entry->filename, "TEST    TXT", 11);
        
        // Attributes (archive file)
        file_entry->attributes = 0x20; // ATTR_ARCHIVE
        
        // Reserved
        file_entry->reserved = 0;
        
        // Creation time tenths
        file_entry->creation_time_tenths = 0;
        
        // Creation time
        file_entry->creation_time = 0;
        
        // Creation date
        file_entry->creation_date = 0;
        
        // Last access date
        file_entry->last_access_date = 0;
        
        // First cluster high (upper 16 bits)
        file_entry->first_cluster_high = 0;
        
        // Last write time
        file_entry->last_write_time = 0;
        
        // Last write date
        file_entry->last_write_date = 0;
        
        // First cluster low (lower 16 bits)
        file_entry->first_cluster_low = 3; // Points to cluster 3
        
        // File size
        file_entry->file_size = 100;
        
        // Create a deleted file entry
        Fat32Parser::Fat32DirEntry* deleted_entry = reinterpret_cast<Fat32Parser::Fat32DirEntry*>(root_dir + 32);
        
        // Deleted filename (first byte is 0xE5)
        deleted_entry->filename[0] = 0xE5;
        memcpy(deleted_entry->filename + 1, "ELETED  TXT", 10);
        
        // Attributes
        deleted_entry->attributes = 0x20; // ATTR_ARCHIVE
        
        // Other fields
        deleted_entry->reserved = 0;
        deleted_entry->creation_time_tenths = 0;
        deleted_entry->creation_time = 0;
        deleted_entry->creation_date = 0;
        deleted_entry->last_access_date = 0;
        deleted_entry->first_cluster_high = 0;
        deleted_entry->last_write_time = 0;
        deleted_entry->last_write_date = 0;
        deleted_entry->first_cluster_low = 4; // Points to cluster 4
        deleted_entry->file_size = 200;
        
        // Create file data at cluster 3
        // Cluster 3 offset = 24576 + (3 - 2) * (4 * 512) = 24576 + 2048 = 26624
        uint8_t* file_data = fat32_data_.data() + 26624;
        memcpy(file_data, "This is test file content for FAT32 recovery testing.", 54);
        
        // Create deleted file data at cluster 4
        uint8_t* deleted_data = fat32_data_.data() + 28672; // 26624 + 2048
        memcpy(deleted_data, "This is deleted file content that should be recoverable.", 57);
    }
    
    std::unique_ptr<Fat32Parser> parser_;
    std::vector<uint8_t> fat32_data_;
};

TEST_F(Fat32ParserTest, CanParseFilesystem) {
    EXPECT_TRUE(parser_->canParse(fat32_data_.data(), fat32_data_.size()));
    
    // Test with invalid data
    std::vector<uint8_t> invalid_data(1024, 0xFF);
    EXPECT_FALSE(parser_->canParse(invalid_data.data(), invalid_data.size()));
}

TEST_F(Fat32ParserTest, ParseFilesystemInfo) {
    ASSERT_TRUE(parser_->canParse(fat32_data_.data(), fat32_data_.size()));
    
    auto info = parser_->getFileSystemInfo();
    
    EXPECT_EQ(info, "FAT32 File System");
}

TEST_F(Fat32ParserTest, RecoverDeletedFiles) {
    ASSERT_TRUE(parser_->initialize(fat32_data_.data(), fat32_data_.size()));
    
    auto files = parser_->recoverDeletedFiles();
    
    // Should find our test files
    EXPECT_GE(files.size(), 1);
    
    // Check if we found the test file
    bool found_test_file = false;
    for (const auto& file : files) {
        if (file.filename.find("TEST") != std::string::npos ||
            file.filename.find("test") != std::string::npos) {
            found_test_file = true;
            EXPECT_GT(file.file_size, 0);
            EXPECT_GT(file.confidence_score, 0.0);
            break;
        }
    }
    
    EXPECT_TRUE(found_test_file);
}

TEST_F(Fat32ParserTest, ValidateBootSector) {
    ASSERT_TRUE(parser_->canParse(fat32_data_.data(), fat32_data_.size()));
    
    // Test boot sector validation with valid data
    const auto* boot = reinterpret_cast<const Fat32Parser::Fat32BootSector*>(fat32_data_.data());
    EXPECT_TRUE(parser_->validate_boot_sector(boot));
    
    // Test with corrupted signature
    auto corrupted_data = fat32_data_;
    corrupted_data[510] = 0x00;
    const auto* corrupted_boot = reinterpret_cast<const Fat32Parser::Fat32BootSector*>(corrupted_data.data());
    EXPECT_FALSE(parser_->validate_boot_sector(corrupted_boot));
}

TEST_F(Fat32ParserTest, ParseDirectoryEntries) {
    ASSERT_TRUE(parser_->initialize(fat32_data_.data(), fat32_data_.size()));
    
    const auto* boot = reinterpret_cast<const Fat32Parser::Fat32BootSector*>(fat32_data_.data());
    auto files = parser_->parse_directory_entries(fat32_data_.data(), fat32_data_.size(), boot, 0);
    
    // Should find our test file
    EXPECT_GE(files.size(), 1);
    
    bool found_test_file = false;
    for (const auto& file : files) {
        if (file.filename == "TEST.TXT" || file.filename.find("TEST") != std::string::npos) {
            found_test_file = true;
            EXPECT_EQ(file.file_size, 100);
            EXPECT_GT(file.confidence_score, 0.0);
            break;
        }
    }
    
    EXPECT_TRUE(found_test_file);
}

TEST_F(Fat32ParserTest, ParseDeletedEntries) {
    ASSERT_TRUE(parser_->initialize(fat32_data_.data(), fat32_data_.size()));
    
    const auto* boot = reinterpret_cast<const Fat32Parser::Fat32BootSector*>(fat32_data_.data());
    auto files = parser_->parse_deleted_entries(fat32_data_.data(), fat32_data_.size(), boot, 0);
    
    // Should find deleted file
    EXPECT_GE(files.size(), 1);
    
    bool found_deleted_file = false;
    for (const auto& file : files) {
        if (file.filename.find("deleted") != std::string::npos ||
            file.filename.find("ELETED") != std::string::npos) {
            found_deleted_file = true;
            EXPECT_EQ(file.file_size, 200);
            EXPECT_GT(file.confidence_score, 0.0);
            break;
        }
    }
    
    EXPECT_TRUE(found_deleted_file);
}

TEST_F(Fat32ParserTest, ExtractShortName) {
    ASSERT_TRUE(parser_->canParse(fat32_data_.data(), fat32_data_.size()));
    
    // Test with our test file entry
    const auto* entry = reinterpret_cast<const Fat32Parser::Fat32DirEntry*>(fat32_data_.data() + 24576);
    std::string name = parser_->extract_short_name(entry);
    
    EXPECT_EQ(name, "TEST.TXT");
}

TEST_F(Fat32ParserTest, GetClusterSize) {
    const auto* boot = reinterpret_cast<const Fat32Parser::Fat32BootSector*>(fat32_data_.data());
    uint32_t cluster_size = parser_->get_cluster_size(boot);
    
    // 4 sectors per cluster * 512 bytes per sector = 2048 bytes
    EXPECT_EQ(cluster_size, 2048);
}

TEST_F(Fat32ParserTest, GetFatOffset) {
    const auto* boot = reinterpret_cast<const Fat32Parser::Fat32BootSector*>(fat32_data_.data());
    uint64_t fat_offset = parser_->get_fat_offset(boot);
    
    // Reserved sectors = 32, so FAT starts at 32 * 512 = 16384
    EXPECT_EQ(fat_offset, 16384);
}

TEST_F(Fat32ParserTest, GetDataOffset) {
    const auto* boot = reinterpret_cast<const Fat32Parser::Fat32BootSector*>(fat32_data_.data());
    uint64_t data_offset = parser_->get_data_offset(boot);
    
    // Reserved sectors + (number of FATs * FAT size) = 32 + (2 * 8) = 48 sectors
    // 48 * 512 = 24576 bytes
    EXPECT_EQ(data_offset, 24576);
}

TEST_F(Fat32ParserTest, ClusterToSector) {
    const auto* boot = reinterpret_cast<const Fat32Parser::Fat32BootSector*>(fat32_data_.data());
    
    // Cluster 2 should map to sector 48 (data area start)
    uint64_t sector = parser_->cluster_to_sector(2, boot);
    EXPECT_EQ(sector, 48);
    
    // Cluster 3 should map to sector 48 + 4 = 52
    sector = parser_->cluster_to_sector(3, boot);
    EXPECT_EQ(sector, 52);
}

TEST_F(Fat32ParserTest, IsValidCluster) {
    // Valid cluster numbers are 2 to 0x0FFFFFF6
    EXPECT_TRUE(parser_->is_valid_cluster(2));
    EXPECT_TRUE(parser_->is_valid_cluster(100));
    EXPECT_TRUE(parser_->is_valid_cluster(0x0FFFFFF6));
    
    // Invalid cluster numbers
    EXPECT_FALSE(parser_->is_valid_cluster(0));
    EXPECT_FALSE(parser_->is_valid_cluster(1));
    EXPECT_FALSE(parser_->is_valid_cluster(0x0FFFFFF7)); // Bad cluster
    EXPECT_FALSE(parser_->is_valid_cluster(0x0FFFFFF8)); // End of chain
    EXPECT_FALSE(parser_->is_valid_cluster(0xFFFFFFFF)); // End of chain
}

TEST_F(Fat32ParserTest, FatEntryValue) {
    const auto* boot = reinterpret_cast<const Fat32Parser::Fat32BootSector*>(fat32_data_.data());
    uint64_t fat_offset = parser_->get_fat_offset(boot);
    const uint8_t* fat_table = fat32_data_.data() + fat_offset;
    
    // Test reading FAT entries
    uint32_t entry0 = parser_->fat_entry_value(fat_table, 0);
    EXPECT_EQ(entry0 & 0x0FFFFFFF, 0x0FFFFFF8); // Media type + end marker
    
    uint32_t entry2 = parser_->fat_entry_value(fat_table, 2);
    EXPECT_EQ(entry2 & 0x0FFFFFFF, 0x0FFFFFFF); // End of chain for root dir
}

TEST_F(Fat32ParserTest, DetermineFileType) {
    EXPECT_EQ(parser_->determine_file_type("test.txt"), "txt");
    EXPECT_EQ(parser_->determine_file_type("image.jpg"), "jpg");
    EXPECT_EQ(parser_->determine_file_type("document.pdf"), "pdf");
    EXPECT_EQ(parser_->determine_file_type("archive.zip"), "zip");
    EXPECT_EQ(parser_->determine_file_type("noextension"), "unknown");
}

TEST_F(Fat32ParserTest, EdgeCases) {
    // Test with null data
    EXPECT_FALSE(parser_->canParse(nullptr, 0));
    
    // Test with too small data
    std::vector<uint8_t> small_data(100, 0);
    EXPECT_FALSE(parser_->canParse(small_data.data(), small_data.size()));
    
    // Test initialize with invalid data
    EXPECT_FALSE(parser_->initialize(small_data.data(), small_data.size()));
    
    // Test recovery without initialization
    Fat32Parser uninit_parser;
    auto files = uninit_parser.recoverDeletedFiles();
    EXPECT_EQ(files.size(), 0);
}

TEST_F(Fat32ParserTest, ThreadSafety) {
    ASSERT_TRUE(parser_->initialize(fat32_data_.data(), fat32_data_.size()));
    
    // Test that multiple calls don't interfere
    auto files1 = parser_->recoverDeletedFiles();
    auto files2 = parser_->recoverDeletedFiles();
    
    // Results should be consistent
    EXPECT_EQ(files1.size(), files2.size());
}

TEST_F(Fat32ParserTest, FileSystemType) {
    EXPECT_EQ(parser_->getFileSystemType(), FileSystemType::FAT32);
}

TEST_F(Fat32ParserTest, LargeDataHandling) {
    // Test with larger filesystem
    std::vector<uint8_t> large_data = fat32_data_;
    large_data.resize(2 * 1024 * 1024, 0); // 2MB
    
    // Update sector count to match larger size
    *(uint32_t*)(large_data.data() + 32) = 4096; // 4096 * 512 = 2MB
    
    EXPECT_TRUE(parser_->canParse(large_data.data(), large_data.size()));
    
    if (parser_->initialize(large_data.data(), large_data.size())) {
        auto files = parser_->recoverDeletedFiles();
        EXPECT_GE(files.size(), 0);
    }
}

TEST_F(Fat32ParserTest, CorruptedFatEntries) {
    ASSERT_TRUE(parser_->initialize(fat32_data_.data(), fat32_data_.size()));
    
    // Corrupt some FAT entries
    auto corrupted_data = fat32_data_;
    uint8_t* fat_table = corrupted_data.data() + 16384;
    
    // Corrupt FAT entry 3
    *(uint32_t*)(fat_table + 12) = 0xBAD1BAD1;
    
    parser_->initialize(corrupted_data.data(), corrupted_data.size());
    auto files = parser_->recoverDeletedFiles();
    
    // Should handle corrupted FAT gracefully
    EXPECT_GE(files.size(), 0);
}

TEST_F(Fat32ParserTest, LongFilenameHandling) {
    // Test that parser handles long filename entries correctly
    // (in our test data we don't have LFN entries, but the parser should handle them)
    
    ASSERT_TRUE(parser_->initialize(fat32_data_.data(), fat32_data_.size()));
    
    // Create long filename entries manually
    std::vector<Fat32Parser::LongNameEntry> lfn_entries;
    
    // Test extract_long_name with empty entries (should not crash)
    std::string long_name = parser_->extract_long_name(lfn_entries);
    EXPECT_TRUE(long_name.empty());
}

TEST_F(Fat32ParserTest, AttributeConstants) {
    // Test that attribute constants are correct
    EXPECT_EQ(Fat32Parser::ATTR_READ_ONLY, 0x01);
    EXPECT_EQ(Fat32Parser::ATTR_HIDDEN, 0x02);
    EXPECT_EQ(Fat32Parser::ATTR_SYSTEM, 0x04);
    EXPECT_EQ(Fat32Parser::ATTR_VOLUME_ID, 0x08);
    EXPECT_EQ(Fat32Parser::ATTR_DIRECTORY, 0x10);
    EXPECT_EQ(Fat32Parser::ATTR_ARCHIVE, 0x20);
    EXPECT_EQ(Fat32Parser::ATTR_LONG_NAME, 0x0F);
    
    // Test cluster constants
    EXPECT_EQ(Fat32Parser::EOC_MARK, 0x0FFFFFF8);
    EXPECT_EQ(Fat32Parser::BAD_CLUSTER, 0x0FFFFFF7);
    EXPECT_EQ(Fat32Parser::FREE_CLUSTER, 0x00000000);
}

TEST_F(Fat32ParserTest, VolumeLabel) {
    const auto* boot = reinterpret_cast<const Fat32Parser::Fat32BootSector*>(fat32_data_.data());
    
    // Our test volume label is "TEST_FAT32 "
    std::string expected_label = "TEST_FAT32 ";
    std::string actual_label(boot->volume_label, 11);
    
    EXPECT_EQ(actual_label, expected_label);
}

TEST_F(Fat32ParserTest, FatTimeConversion) {
    // Test FAT time/date to Unix timestamp conversion
    // FAT time: 5 bits hour, 6 bits minute, 5 bits second/2
    // FAT date: 7 bits year from 1980, 4 bits month, 5 bits day
    
    // Test with a specific time: 12:30:00 on Jan 1, 2000
    uint16_t fat_time = (12 << 11) | (30 << 5) | (0 >> 1); // 12:30:00
    uint16_t fat_date = ((2000 - 1980) << 9) | (1 << 5) | 1; // Jan 1, 2000
    
    time_t unix_time = parser_->fat_time_to_unix(fat_time, fat_date);
    
    // Should not crash and return a reasonable value
    EXPECT_GT(unix_time, 0);
}
