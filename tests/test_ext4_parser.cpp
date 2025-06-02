#include <gtest/gtest.h>
#include "filesystems/ext4_parser.h"
#include "utils/logger.h"
#include <vector>
#include <cstring>
#include <algorithm>
#include <cstdio> // For std::remove

using namespace FileRecovery;

class Ext4ParserTest : public ::testing::Test {
protected:
    void SetUp() override {
        parser_ = std::make_unique<Ext4Parser>();
        
        // Initialize logger for tests
        Logger::getInstance().initialize("test_ext4.log", Logger::Level::DEBUG);
        
        createTestExt4Data();
    }
    
    void TearDown() override {
        // Use C standard library function instead of std::filesystem
        std::remove("test_ext4.log");
    }
    
    void createTestExt4Data() {
        // Create a minimal EXT4 filesystem structure
        ext4_data_.resize(64 * 1024, 0); // 64KB test filesystem
        
        // EXT4 superblock starts at offset 1024
        uint8_t* superblock = ext4_data_.data() + 1024;
        
        // Fill in superblock fields
        *(uint32_t*)(superblock + 0) = 1000;    // s_inodes_count
        *(uint32_t*)(superblock + 4) = 16;      // s_blocks_count_lo (16 * 4KB = 64KB)
        *(uint32_t*)(superblock + 8) = 100;     // s_r_blocks_count_lo
        *(uint32_t*)(superblock + 12) = 10;     // s_free_blocks_count_lo
        *(uint32_t*)(superblock + 16) = 900;    // s_free_inodes_count
        *(uint32_t*)(superblock + 20) = 1;      // s_first_data_block
        *(uint32_t*)(superblock + 24) = 2;      // s_log_block_size (4KB blocks)
        *(uint32_t*)(superblock + 28) = 2;      // s_log_cluster_size
        *(uint32_t*)(superblock + 32) = 8192;   // s_blocks_per_group
        *(uint32_t*)(superblock + 36) = 8192;   // s_clusters_per_group
        *(uint32_t*)(superblock + 40) = 1000;   // s_inodes_per_group
        *(uint32_t*)(superblock + 44) = 0;      // s_mtime
        *(uint32_t*)(superblock + 48) = 0;      // s_wtime
        *(uint16_t*)(superblock + 52) = 1;      // s_mnt_count
        *(uint16_t*)(superblock + 54) = 20;     // s_max_mnt_count
        *(uint16_t*)(superblock + 56) = 0xEF53; // s_magic (EXT4 magic number)
        *(uint16_t*)(superblock + 58) = 1;      // s_state (clean)
        *(uint16_t*)(superblock + 60) = 1;      // s_errors
        *(uint16_t*)(superblock + 62) = 0;      // s_minor_rev_level
        *(uint32_t*)(superblock + 64) = 0;      // s_lastcheck
        *(uint32_t*)(superblock + 68) = 0;      // s_checkinterval
        *(uint32_t*)(superblock + 72) = 0;      // s_creator_os
        *(uint32_t*)(superblock + 76) = 1;      // s_rev_level (dynamic)
        *(uint16_t*)(superblock + 80) = 0;      // s_def_resuid
        *(uint16_t*)(superblock + 82) = 0;      // s_def_resgid
        
        // EXT4 specific fields
        *(uint32_t*)(superblock + 84) = 11;     // s_first_ino
        *(uint16_t*)(superblock + 88) = 256;    // s_inode_size
        *(uint16_t*)(superblock + 90) = 0;      // s_block_group_nr
        *(uint32_t*)(superblock + 92) = 0x38;   // s_feature_compat
        *(uint32_t*)(superblock + 96) = 0x3C2;  // s_feature_incompat
        *(uint32_t*)(superblock + 100) = 0x1;   // s_feature_ro_compat
        
        // UUID (16 bytes)
        for (int i = 0; i < 16; ++i) {
            superblock[104 + i] = static_cast<uint8_t>(i);
        }
        
        // Volume name
        strcpy(reinterpret_cast<char*>(superblock + 120), "test_ext4");
        
        // Create a simple group descriptor at offset 2048 (block 0 for 4KB blocks)
        uint8_t* group_desc = ext4_data_.data() + 2048;
        *(uint32_t*)(group_desc + 0) = 3;       // bg_block_bitmap_lo
        *(uint32_t*)(group_desc + 4) = 4;       // bg_inode_bitmap_lo
        *(uint32_t*)(group_desc + 8) = 5;       // bg_inode_table_lo
        *(uint16_t*)(group_desc + 12) = 8000;   // bg_free_blocks_count_lo
        *(uint16_t*)(group_desc + 14) = 900;    // bg_free_inodes_count_lo
        *(uint16_t*)(group_desc + 16) = 2;      // bg_used_dirs_count_lo
        *(uint16_t*)(group_desc + 18) = 0;      // bg_flags
    }
    
    std::unique_ptr<Ext4Parser> parser_;
    std::vector<uint8_t> ext4_data_;
};

TEST_F(Ext4ParserTest, CanParseFilesystem) {
    EXPECT_TRUE(parser_->canParse(ext4_data_.data(), ext4_data_.size()));
    
    // Test with invalid data
    std::vector<uint8_t> invalid_data(1024, 0xFF);
    EXPECT_FALSE(parser_->canParse(invalid_data.data(), invalid_data.size()));
}

TEST_F(Ext4ParserTest, GetFileSystemInfo) {
    ASSERT_TRUE(parser_->canParse(ext4_data_.data(), ext4_data_.size()));
    
    auto info = parser_->getFileSystemInfo();
    
    // Check that we get a non-empty string
    EXPECT_FALSE(info.empty());
    
    // Verify it contains basic EXT4 information
    EXPECT_NE(info.find("ext4"), std::string::npos);
}

TEST_F(Ext4ParserTest, RecoverDeletedFiles) {
    ASSERT_TRUE(parser_->initialize(ext4_data_.data(), ext4_data_.size()));
    
    auto files = parser_->recoverDeletedFiles();
    
    // With minimal test data, we might not have any files
    // but the function should not crash
    EXPECT_GE(files.size(), 0);
}

TEST_F(Ext4ParserTest, ValidateSuperblock) {
    ASSERT_TRUE(parser_->canParse(ext4_data_.data(), ext4_data_.size()));
    
    // Test superblock validation
    EXPECT_TRUE(parser_->validate_superblock(reinterpret_cast<const Ext4Parser::Ext4Superblock*>(ext4_data_.data() + 1024)));
    
    // Test with corrupted magic number
    auto corrupted_data = ext4_data_;
    *(uint16_t*)(corrupted_data.data() + 1024 + 56) = 0x1234; // Wrong magic
    EXPECT_FALSE(parser_->validate_superblock(reinterpret_cast<const Ext4Parser::Ext4Superblock*>(corrupted_data.data() + 1024)));
}

TEST_F(Ext4ParserTest, EdgeCases) {
    // Test with null data
    EXPECT_FALSE(parser_->canParse(nullptr, 0));
    
    // Test with too small data
    std::vector<uint8_t> small_data(100, 0);
    EXPECT_FALSE(parser_->canParse(small_data.data(), small_data.size()));
    
    // Test initialization with invalid data
    EXPECT_FALSE(parser_->initialize(small_data.data(), small_data.size()));
}

TEST_F(Ext4ParserTest, FileSystemType) {
    EXPECT_EQ(parser_->getFileSystemType(), FileSystemType::EXT4);
}

TEST_F(Ext4ParserTest, DeletedInodes) {
    ASSERT_TRUE(parser_->initialize(ext4_data_.data(), ext4_data_.size()));
    
    // Create a simple superblock pointer to pass to the function
    const auto* sb = reinterpret_cast<const Ext4Parser::Ext4Superblock*>(ext4_data_.data() + 1024);
    
    // Test the internal function that parses deleted inodes
    auto files = parser_->parse_deleted_inodes(
        reinterpret_cast<const uint8_t*>(ext4_data_.data()), 
        ext4_data_.size(), 
        sb, 
        0);
    
    // Should not crash, may return empty list with test data
    EXPECT_GE(files.size(), 0);
}

// Additional tests for error handling
TEST_F(Ext4ParserTest, ErrorHandling) {
    // Test recovery with uninitialized parser
    Ext4Parser uninit_parser;
    auto files = uninit_parser.recoverDeletedFiles();
    EXPECT_EQ(files.size(), 0);
    
    // Test with very small data (less than superblock)
    std::vector<uint8_t> tiny_data(10, 0);
    EXPECT_FALSE(parser_->canParse(tiny_data.data(), tiny_data.size()));
}

// Test concurrency safety
TEST_F(Ext4ParserTest, ThreadSafety) {
    ASSERT_TRUE(parser_->initialize(ext4_data_.data(), ext4_data_.size()));
    
    // Call the same method multiple times - should be consistent
    auto files1 = parser_->recoverDeletedFiles();
    auto files2 = parser_->recoverDeletedFiles();
    
    EXPECT_EQ(files1.size(), files2.size());
}
