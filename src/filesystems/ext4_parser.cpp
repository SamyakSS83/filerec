#include "filesystems/ext4_parser.h"
#include "utils/logger.h"
#include <cstring>
#include <algorithm>
#include <sstream>
#include <iomanip>   // For std::hex, std::setw
#include <iostream>  // For advanced formatting

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
    
    // Log filesystem details for debugging
    uint32_t block_size = get_block_size(sb);
    uint32_t inode_size = sb->s_inode_size > 0 ? sb->s_inode_size : 128;
    uint32_t group_count = (sb->s_blocks_count_lo + sb->s_blocks_per_group - 1) / sb->s_blocks_per_group;
    
    LOG_DEBUG("EXT4 filesystem details:");
    LOG_DEBUG(" - Block size: " + std::to_string(block_size) + " bytes");
    LOG_DEBUG(" - Inode size: " + std::to_string(inode_size) + " bytes");
    LOG_DEBUG(" - Inodes per group: " + std::to_string(sb->s_inodes_per_group));
    LOG_DEBUG(" - Block groups: " + std::to_string(group_count));
    LOG_DEBUG(" - Group descriptor table offset: " + std::to_string(get_group_desc_offset(sb)));
    LOG_DEBUG(" - 64-bit feature: " + std::to_string((sb->s_feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT) != 0));
    
    // Parse deleted inodes with enhanced method
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
    
    uint32_t block_size = get_block_size(sb);
    uint32_t inode_size = sb->s_inode_size > 0 ? sb->s_inode_size : 128;
    uint32_t group_count = (sb->s_blocks_count_lo + sb->s_blocks_per_group - 1) / sb->s_blocks_per_group;
    
    LOG_DEBUG("Searching for deleted inodes across " + std::to_string(group_count) + " block groups");
    
    // To avoid spending too much time, limit the number of groups we scan
    uint32_t max_groups = std::min(group_count, uint32_t(200));
    
    for (uint32_t group = 0; group < max_groups; group++) {
        // Get actual inode table offset from group descriptors
        uint64_t inode_table_offset = get_inode_table_offset(group, sb, data, size);
        
        if (inode_table_offset == 0 || inode_table_offset >= size) {
            LOG_DEBUG("Skipping group " + std::to_string(group) + " - invalid inode table offset");
            continue;
        }
        
        std::stringstream ss;
        ss << "Scanning group " << group << ", inode table at offset 0x" << std::hex << inode_table_offset << std::dec;
        LOG_DEBUG(ss.str());
        
        uint32_t inodes_in_group = std::min(sb->s_inodes_per_group,
                                         sb->s_inodes_count - group * sb->s_inodes_per_group);
        
        // For performance, limit the number of inodes we scan per group
        uint32_t max_inodes_to_scan = std::min(inodes_in_group, uint32_t(2000));
        
        // Start from inode 12 (first non-reserved inode)
        uint32_t start_inode_index = (group == 0) ? 11 : 0;
        
        for (uint32_t i = start_inode_index; i < max_inodes_to_scan; i++) {
            uint64_t inode_offset = inode_table_offset + i * inode_size;
            
            if (inode_offset + sizeof(Ext4Inode) > size) {
                break;
            }
            
            const auto* inode = reinterpret_cast<const Ext4Inode*>(data + inode_offset);
            uint32_t inode_number = group * sb->s_inodes_per_group + i + 1;
            
            // Skip if not a deleted inode
            if (!is_deleted_inode(inode)) {
                continue;
            }
            
            // Get file size (handling 64-bit sizes)
            uint64_t file_size = inode->i_size_lo;
            if (sb->s_feature_ro_compat & 0x0002) {
                file_size |= static_cast<uint64_t>(inode->i_size_high) << 32;
            }
            
            // Skip empty or unreasonably large files
            if (file_size == 0 || file_size > (1ULL << 30)) {
                continue;
            }
            
            // Create recovered file record
            RecoveredFile recovered;
            recovered.filename = "deleted_inode_" + std::to_string(inode_number) + ".recovered";
            recovered.file_size = file_size;
            recovered.fragments.clear();
            
            // Try to extract data blocks - different handling for extents vs direct blocks
            bool has_extents = (inode->i_flags & EXT4_EXTENTS_FL) != 0;
            bool success = false;
            
            if (has_extents && (sb->s_feature_incompat & EXT4_FEATURE_INCOMPAT_EXTENTS) != 0) {
                // Extent-based file
                LOG_DEBUG("Inode " + std::to_string(inode_number) + " uses extents");
                
                // For simplicity, we only check the first extent here
                // A full implementation would parse the full extent tree
                
                // Locate first data block
                uint64_t first_block = 0;
                if (file_size > 0 && inode->i_blocks_lo > 0) {
                    // Here we assume the first extent header and entry is directly in i_block
                    // This is a simplification - a real implementation would parse the extent tree
                    first_block = inode->i_block[3]; // Simple approximation
                    
                    if (first_block > 0 && first_block < sb->s_blocks_count_lo) {
                        uint64_t data_offset = first_block * block_size;
                        LOG_DEBUG("Found data for inode " + std::to_string(inode_number) + 
                                  " at block " + std::to_string(first_block));
                        
                        recovered.start_offset = partition_offset + data_offset;
                        recovered.fragments.push_back({recovered.start_offset, file_size});
                        success = true;
                    }
                }
            } else {
                // Direct block file
                bool found_block = false;
                
                // Check direct blocks
                for (int j = 0; j < 12; j++) {
                    if (inode->i_block[j] > 0 && inode->i_block[j] < sb->s_blocks_count_lo) {
                        uint64_t data_offset = inode->i_block[j] * block_size;
                        uint64_t block_size_to_use = std::min(file_size, uint64_t(block_size));
                        
                        if (!found_block) {
                            recovered.start_offset = partition_offset + data_offset;
                            found_block = true;
                        }
                        
                        recovered.fragments.push_back({partition_offset + data_offset, block_size_to_use});
                        file_size -= block_size_to_use;
                        
                        if (file_size == 0) break;
                    }
                }
                
                success = found_block;
            }
            
            // Skip if we couldn't locate any data
            if (!success) {
                continue;
            }
            
            // Try to detect file type based on content
            if (recovered.start_offset < size) {
                const uint8_t* file_data = data + recovered.start_offset - partition_offset;
                uint64_t available_size = std::min(uint64_t(512), size - (recovered.start_offset - partition_offset));
                
                if (available_size > 16) {
                    std::string detected_type = detect_file_type(file_data, available_size);
                    recovered.file_type = detected_type;
                    recovered.filename = "deleted_" + std::to_string(inode_number) + "." + detected_type;
                }
            }
            
            recovered.is_fragmented = recovered.fragments.size() > 1;
            recovered.confidence_score = 70.0; // Lower confidence for deleted files
            
            LOG_DEBUG("Found deleted file: " + recovered.filename + 
                     ", size: " + std::to_string(recovered.file_size) +
                     ", type: " + recovered.file_type);
            
            files.push_back(recovered);
        }
    }
    
    return files;
}

uint32_t Ext4Parser::get_block_size(const Ext4Superblock* sb) const {
    return 1024 << sb->s_log_block_size;
}

uint64_t Ext4Parser::get_group_desc_offset(const Ext4Superblock* sb) const {
    uint32_t block_size = get_block_size(sb);
    
    // Group descriptor table follows the superblock
    // In ext4, if block_size >= 2048, superblock is in block 0
    // Otherwise, superblock is in block 1, and group descriptors start at block 2
    return sb->s_first_data_block == 0 ? block_size : (block_size * 2);
}

uint64_t Ext4Parser::get_inode_table_offset(uint32_t group, const Ext4Superblock* sb, 
                                          const uint8_t* data, size_t size) const {
    uint32_t block_size = get_block_size(sb);
    uint64_t gdt_offset = get_group_desc_offset(sb);
    bool is_64bit = (sb->s_feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT) != 0;
    uint32_t desc_size = is_64bit ? 64 : 32;
    
    uint64_t desc_offset = gdt_offset + (group * desc_size);
    
    if (desc_offset + desc_size > size) {
        LOG_WARNING("Group descriptor offset beyond data size");
        return 0;
    }
    
    const auto* gdesc = reinterpret_cast<const Ext4GroupDesc*>(data + desc_offset);
    
    // Get the inode table block number
    uint64_t inode_table_block;
    if (is_64bit) {
        inode_table_block = gdesc->bg_inode_table_lo | 
                          (static_cast<uint64_t>(gdesc->bg_inode_table_hi) << 32);
    } else {
        inode_table_block = gdesc->bg_inode_table_lo;
    }
    
    return inode_table_block * block_size;
}

bool Ext4Parser::is_deleted_inode(const Ext4Inode* inode) const {
    // More sophisticated check for deleted inodes:
    // 1. Deletion timestamp is non-zero
    // 2. Either link count is zero OR, for ext4, check other conditions
    // 3. Check if the inode size is reasonable
    // 4. Additional sanity checks to avoid false positives
    
    bool has_deletion_time = (inode->i_dtime != 0);
    bool has_zero_links = (inode->i_links_count == 0);
    bool has_reasonable_size = (inode->i_size_lo > 0 && inode->i_size_lo < (1ULL << 30)); // < 1GB
    bool has_data_blocks = (inode->i_blocks_lo > 0);
    bool is_regular_file = ((inode->i_mode & 0xF000) == 0x8000);
    
    return has_deletion_time && has_zero_links && has_reasonable_size && 
           has_data_blocks && is_regular_file;
}

std::string Ext4Parser::detect_file_type(const uint8_t* data, size_t size) const {
    if (size < 16) return "unknown";
    
    // Simple file type detection based on magic numbers
    if (size >= 4 && data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF) {
        return "jpg";
    }
    else if (size >= 8 && data[0] == 0x89 && data[1] == 'P' && data[2] == 'N' && data[3] == 'G') {
        return "png";
    }
    else if (size >= 5 && data[0] == '%' && data[1] == 'P' && data[2] == 'D' && data[3] == 'F' && data[4] == '-') {
        return "pdf";
    }
    else if (size >= 4 && data[0] == 'P' && data[1] == 'K' && data[2] == 0x03 && data[3] == 0x04) {
        return "zip";
    }
    else if (size >= 4 && data[0] == 0x25 && data[1] == 0x21 && data[2] == 0x50 && data[3] == 0x53) {
        return "ps";
    }
    else if (size >= 3 && data[0] == 'G' && data[1] == 'I' && data[2] == 'F') {
        return "gif";
    }
    else if (size >= 12 && (data[0] == 'I' && data[1] == 'I' && data[2] == '*' && data[3] == 0) ||
              (data[0] == 'M' && data[1] == 'M' && data[2] == 0 && data[3] == '*')) {
        return "tif";
    }
    else if (size >= 4 && data[0] == 0x7F && data[1] == 'E' && data[2] == 'L' && data[3] == 'F') {
        return "elf";
    }
    
    // Text file detection
    int text_chars = 0;
    for (size_t i = 0; i < std::min(size_t(256), size); i++) {
        if ((data[i] >= 32 && data[i] <= 126) || data[i] == '\n' || data[i] == '\r' || data[i] == '\t') {
            text_chars++;
        }
    }
    
    if (text_chars > std::min(size_t(240), static_cast<size_t>(size * 0.9))) {
        return "txt";
    }
    
    return "dat";
}

} // namespace FileRecovery
