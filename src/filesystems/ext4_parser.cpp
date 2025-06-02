#include "filesystems/ext4_parser.h"
#include "utils/logger.h"
#include <cstring>
#include <algorithm>
#include <sstream>

namespace FileRecovery {

Ext4Parser::Ext4Parser() = default;

bool Ext4Parser::initialize(const Byte* data, Size size) {
    disk_data_ = data;
    disk_size_ = size;
    return canParse(data, size);
}

bool Ext4Parser::canParse(const Byte* data, Size size) const {
    if (size < 1024 + sizeof(Ext4Superblock)) {
        return false;
    }
    
    const auto* superblock = reinterpret_cast<const Ext4Superblock*>(data + 1024);
    return validate_superblock(superblock);
}

std::vector<RecoveredFile> Ext4Parser::recoverDeletedFiles() {
    if (!disk_data_ || disk_size_ == 0) {
        LOG_ERROR("ext4 parser not initialized");
        return {};
    }
    
    LOG_INFO("Parsing ext4 filesystem metadata");
    
    if (!canParse(disk_data_, disk_size_)) {
        LOG_ERROR("Invalid ext4 filesystem");
        return {};
    }
    
    const auto* sb = reinterpret_cast<const Ext4Superblock*>(disk_data_ + 1024);
    
    // Parse deleted inodes
    auto files = parse_deleted_inodes(reinterpret_cast<const uint8_t*>(disk_data_), disk_size_, sb, 0);
    
    LOG_INFO("Found " + std::to_string(files.size()) + " files in ext4 filesystem");
    return files;
}

std::string Ext4Parser::getFileSystemInfo() const {
    if (!disk_data_ || disk_size_ == 0 || !canParse(disk_data_, disk_size_)) {
        return "ext4 File System (not initialized)";
    }
    
    const auto* sb = reinterpret_cast<const Ext4Superblock*>(disk_data_ + 1024);
    
    std::stringstream info;
    info << "ext4 File System\n";
    info << "Block size: " << (1024 << sb->s_log_block_size) << " bytes\n";
    info << "Total blocks: " << sb->s_blocks_count_lo << "\n";
    info << "Total inodes: " << sb->s_inodes_count << "\n";
    info << "Free inodes: " << sb->s_free_inodes_count;
    
    return info.str();
}

bool Ext4Parser::validate_superblock(const Ext4Superblock* sb) const {
    if (sb->s_magic != 0xEF53) { // EXT4_MAGIC
        return false;
    }
    
    // Additional validation
    if (sb->s_inodes_count == 0 || sb->s_blocks_count_lo == 0) {
        return false;
    }
    
    if (sb->s_inodes_per_group == 0 || sb->s_blocks_per_group == 0) {
        return false;
    }
    
    // Check block size is reasonable
    uint32_t block_size = 1024 << sb->s_log_block_size;
    if (block_size < 1024 || block_size > 65536) {
        return false;
    }
    
    return true;
}

std::vector<RecoveredFile> Ext4Parser::parse_deleted_inodes(const uint8_t* data, size_t size,
                                                           const Ext4Superblock* sb, uint64_t partition_offset) {
    std::vector<RecoveredFile> files;
    
    uint32_t block_size = 1024 << sb->s_log_block_size;
    uint32_t inode_size = sb->s_inode_size > 0 ? sb->s_inode_size : 128;
    uint32_t group_count = (sb->s_blocks_count_lo + sb->s_blocks_per_group - 1) / sb->s_blocks_per_group;
    
    LOG_DEBUG("Searching for deleted inodes");
    
    for (uint32_t group = 0; group < group_count && group < 100; group++) { // Limit groups for safety
        uint64_t inode_table_offset = estimate_inode_table_offset(group, sb);
        
        if (inode_table_offset >= size) {
            continue;
        }
        
        uint32_t inodes_in_group = std::min(sb->s_inodes_per_group,
                                           sb->s_inodes_count - group * sb->s_inodes_per_group);
        
        for (uint32_t i = 0; i < inodes_in_group && i < 1000; i++) { // Limit inodes per group
            uint64_t inode_offset = inode_table_offset + i * inode_size;
            
            if (inode_offset + sizeof(Ext4Inode) > size) {
                break;
            }
            
            const auto* inode = reinterpret_cast<const Ext4Inode*>(data + inode_offset);
            uint32_t inode_number = group * sb->s_inodes_per_group + i + 1;
            
            // Look for deleted files (dtime != 0 and some metadata still present)
            if (!is_deleted_inode(inode)) {
                continue;
            }
            
            uint64_t file_size = inode->i_size_lo;
            if (sb->s_feature_ro_compat & 0x0002) {
                file_size |= static_cast<uint64_t>(inode->i_size_high) << 32;
            }
            
            if (file_size == 0 || file_size > (1ULL << 30)) { // Max 1GB files
                continue;
            }
            
            RecoveredFile recovered;
            recovered.filename = "deleted_inode_" + std::to_string(inode_number) + ".recovered";
            recovered.file_type = "unknown";
            recovered.file_size = file_size;
            recovered.confidence_score = 70.0; // Lower confidence for deleted files
            recovered.start_offset = partition_offset + (inode->i_block[0] * block_size);
            
            files.push_back(recovered);
        }
    }
    
    return files;
}

uint64_t Ext4Parser::estimate_inode_table_offset(uint32_t group, const Ext4Superblock* sb) const {
    // This is a simplified estimation
    // A full implementation would read the group descriptor table
    uint32_t inode_size = sb->s_inode_size > 0 ? sb->s_inode_size : 128;
    uint64_t estimated_offset = 1024 + sizeof(Ext4Superblock) + 
                               group * sb->s_inodes_per_group * inode_size;
    
    return estimated_offset;
}

bool Ext4Parser::is_deleted_inode(const Ext4Inode* inode) const {
    return inode->i_dtime != 0 && inode->i_links_count == 0 && inode->i_size_lo > 0;
}

} // namespace FileRecovery
