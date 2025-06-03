#pragma once

#include "interfaces/filesystem_parser.h"
#include "utils/types.h"
#include <memory>

namespace FileRecovery {

class Ext4Parser : public FilesystemParser {
public:
    Ext4Parser();
    ~Ext4Parser() override = default;

    // Interface implementations
    bool initialize(const Byte* data, Size size) override;
    bool canParse(const Byte* data, Size size) const override;
    FileSystemType getFileSystemType() const override { return FileSystemType::EXT4; }
    std::vector<RecoveredFile> recoverDeletedFiles() override;
    std::string getFileSystemInfo() const override;

public:
    struct Ext4Superblock {
        uint32_t s_inodes_count;
        uint32_t s_blocks_count_lo;
        uint32_t s_r_blocks_count_lo;
        uint32_t s_free_blocks_count_lo;
        uint32_t s_free_inodes_count;
        uint32_t s_first_data_block;
        uint32_t s_log_block_size;
        uint32_t s_log_cluster_size;
        uint32_t s_blocks_per_group;
        uint32_t s_clusters_per_group;
        uint32_t s_inodes_per_group;
        uint32_t s_mtime;
        uint32_t s_wtime;
        uint16_t s_mnt_count;
        uint16_t s_max_mnt_count;
        uint16_t s_magic;
        uint16_t s_state;
        uint16_t s_errors;
        uint16_t s_minor_rev_level;
        uint32_t s_lastcheck;
        uint32_t s_checkinterval;
        uint32_t s_creator_os;
        uint32_t s_rev_level;
        uint16_t s_def_resuid;
        uint16_t s_def_resgid;
        // Extended superblock fields for ext4
        uint32_t s_first_ino;
        uint16_t s_inode_size;
        uint16_t s_block_group_nr;
        uint32_t s_feature_compat;
        uint32_t s_feature_incompat;
        uint32_t s_feature_ro_compat;
        uint8_t s_uuid[16];
        char s_volume_name[16];
        char s_last_mounted[64];
        uint32_t s_algorithm_usage_bitmap;
        // More fields...
    } __attribute__((packed));

    struct Ext4Inode {
        uint16_t i_mode;
        uint16_t i_uid;
        uint32_t i_size_lo;
        uint32_t i_atime;
        uint32_t i_ctime;
        uint32_t i_mtime;
        uint32_t i_dtime;
        uint16_t i_gid;
        uint16_t i_links_count;
        uint32_t i_blocks_lo;
        uint32_t i_flags;
        uint32_t i_osd1;
        uint32_t i_block[15];
        uint32_t i_generation;
        uint32_t i_file_acl_lo;
        uint32_t i_size_high;
        uint32_t i_obso_faddr;
        // More fields for ext4...
    } __attribute__((packed));

    struct Ext4DirEntry {
        uint32_t inode;
        uint16_t rec_len;
        uint8_t name_len;
        uint8_t file_type;
        char name[0];
    } __attribute__((packed));
    
    // Group descriptor structure for ext4
    struct Ext4GroupDesc {
        uint32_t bg_block_bitmap_lo;     // Block containing block bitmap
        uint32_t bg_inode_bitmap_lo;     // Block containing inode bitmap
        uint32_t bg_inode_table_lo;      // Block containing inode table
        uint16_t bg_free_blocks_count_lo; // Free blocks count
        uint16_t bg_free_inodes_count_lo; // Free inodes count
        uint16_t bg_used_dirs_count_lo;   // Directories count
        uint16_t bg_flags;               // Flags
        uint32_t bg_exclude_bitmap_lo;    // Snapshot exclusion bitmap
        uint16_t bg_block_bitmap_csum_lo; // Block bitmap checksum
        uint16_t bg_inode_bitmap_csum_lo; // Inode bitmap checksum
        uint16_t bg_unused;              // Unused
        uint16_t bg_checksum;            // Group descriptor checksum
        // 64-bit fields (s_desc_size >= 64)
        uint32_t bg_block_bitmap_hi;
        uint32_t bg_inode_bitmap_hi;
        uint32_t bg_inode_table_hi;
        uint16_t bg_free_blocks_count_hi;
        uint16_t bg_free_inodes_count_hi;
        uint16_t bg_used_dirs_count_hi;
        uint16_t bg_itable_unused_hi;
        uint32_t bg_exclude_bitmap_hi;
        uint16_t bg_block_bitmap_csum_hi;
        uint16_t bg_inode_bitmap_csum_hi;
        uint32_t bg_reserved;
    } __attribute__((packed));

    static constexpr uint16_t EXT4_MAGIC = 0xEF53;
    static constexpr size_t SUPERBLOCK_OFFSET = 1024;
    static constexpr uint32_t EXT4_FEATURE_INCOMPAT_EXTENTS = 0x0040;
    
    // File types
    static constexpr uint8_t EXT4_FT_REG_FILE = 1;
    static constexpr uint8_t EXT4_FT_DIR = 2;
    static constexpr uint8_t EXT4_FT_SYMLINK = 7;
    static constexpr uint8_t EXT4_FT_UNKNOWN = 0;

    bool validate_superblock(const Ext4Superblock* sb) const;
    std::vector<RecoveredFile> parse_deleted_inodes(const uint8_t* data, size_t size,
                                                   const Ext4Superblock* sb, uint64_t partition_offset);
    
    uint64_t estimate_inode_table_offset(uint32_t group, const Ext4Superblock* sb) const;
    bool is_deleted_inode(const Ext4Inode* inode) const;
    
    // Helper methods
    uint32_t get_block_size(const Ext4Superblock* sb) const;
    uint64_t get_group_desc_offset(const Ext4Superblock* sb) const;
    uint64_t get_inode_table_offset(uint32_t group, const Ext4Superblock* sb, const uint8_t* data, size_t size) const;
    std::string detect_file_type(const uint8_t* data, size_t size) const;
    
    // Feature flags
    static constexpr uint32_t EXT4_FEATURE_INCOMPAT_64BIT = 0x0080;
    
    // Inode flags
    static constexpr uint32_t EXT4_EXTENTS_FL = 0x00080000;
};

} // namespace FileRecovery
