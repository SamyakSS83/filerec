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
    
    const auto* sb = reinterpret_cast<const Ext4Superblock*>(disk_data_ + SUPERBLOCK_OFFSET);
    
    // Parse deleted inodes using existing method
    auto file_entries = parse_deleted_inodes(reinterpret_cast<const uint8_t*>(disk_data_), disk_size_, sb, 0);
    
    // Convert FileEntry to RecoveredFile
    std::vector<RecoveredFile> recovered_files;
    for (const auto& entry : file_entries) {
        RecoveredFile recovered;
        recovered.filename = entry.filename;
        recovered.size = entry.size;
        recovered.file_type = entry.file_type;
        recovered.is_deleted = entry.is_deleted;
        recovered.confidence = 80; // Medium confidence for metadata recovery
        recovered.source_location = entry.data_locations.empty() ? 0 : entry.data_locations[0].offset;
        recovered_files.push_back(recovered);
    }
    
    LOG_INFO("Found " + std::to_string(recovered_files.size()) + " files in ext4 filesystem");
    return recovered_files;
}

std::string Ext4Parser::getFileSystemInfo() const {
    return "ext4 File System";
}

bool Ext4Parser::canParse(const Byte* data, Size size) const {
    if (size < SUPERBLOCK_OFFSET + sizeof(Ext4Superblock)) {
        return false;
    }
    
    const auto* sb = reinterpret_cast<const Ext4Superblock*>(data + SUPERBLOCK_OFFSET);
    return validate_superblock(sb);
}

std::vector<FileEntry> Ext4Parser::parse_metadata(const uint8_t* data, size_t size, uint64_t partition_offset) {
    LOG_INFO("Parsing ext4 filesystem metadata");
    
    if (!can_parse(data, size)) {
        LOG_ERROR("Invalid ext4 filesystem");
        return {};
    }
    
    const auto* sb = reinterpret_cast<const Ext4Superblock*>(data + SUPERBLOCK_OFFSET);
    
    std::vector<FileEntry> files;
    
    // Parse regular inodes
    auto regular_files = parse_inodes(data, size, sb, partition_offset);
    files.insert(files.end(), regular_files.begin(), regular_files.end());
    
    // Parse deleted inodes
    auto deleted_files = parse_deleted_inodes(data, size, sb, partition_offset);
    files.insert(files.end(), deleted_files.begin(), deleted_files.end());
    
    LOG_INFO("Found " + std::to_string(files.size()) + " files in ext4 filesystem");
    return files;
}

bool Ext4Parser::validate_superblock(const Ext4Superblock* sb) const {
    if (sb->s_magic != EXT4_MAGIC) {
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

std::vector<FileEntry> Ext4Parser::parse_inodes(const uint8_t* data, size_t size, 
                                               const Ext4Superblock* sb, uint64_t partition_offset) {
    std::vector<FileEntry> files;
    
    uint32_t block_size = get_block_size(sb);
    uint32_t inode_size = sb->s_inode_size > 0 ? sb->s_inode_size : 128;
    uint32_t inodes_per_block = block_size / inode_size;
    uint32_t group_count = (sb->s_blocks_count_lo + sb->s_blocks_per_group - 1) / sb->s_blocks_per_group;
    
    LOG_DEBUG("Parsing " + std::to_string(group_count) + " block groups");
    
    for (uint32_t group = 0; group < group_count && group < 1000; group++) {
        uint64_t inode_table_offset = get_inode_table_offset(group, sb);
        
        if (inode_table_offset >= size) {
            continue;
        }
        
        uint32_t inodes_in_group = std::min(sb->s_inodes_per_group, 
                                           sb->s_inodes_count - group * sb->s_inodes_per_group);
        
        for (uint32_t i = 0; i < inodes_in_group; i++) {
            uint64_t inode_offset = inode_table_offset + i * inode_size;
            
            if (inode_offset + sizeof(Ext4Inode) > size) {
                break;
            }
            
            const auto* inode = reinterpret_cast<const Ext4Inode*>(data + inode_offset);
            uint32_t inode_number = group * sb->s_inodes_per_group + i + 1;
            
            // Skip if inode is not in use or is a directory/special file
            if (inode->i_mode == 0 || (inode->i_mode & 0xF000) != 0x8000) { // Not a regular file
                continue;
            }
            
            // Skip if file size is 0
            uint64_t file_size = inode->i_size_lo;
            if (sb->s_feature_ro_compat & 0x0002) { // LARGE_FILE feature
                file_size |= static_cast<uint64_t>(inode->i_size_high) << 32;
            }
            
            if (file_size == 0 || file_size > (1ULL << 40)) { // Skip very large files (>1TB)
                continue;
            }
            
            auto file_entry = parse_inode_to_file_entry(inode, inode_number, data, sb, partition_offset);
            if (!file_entry.filename.empty()) {
                files.push_back(file_entry);
            }
        }
    }
    
    return files;
}

std::vector<FileEntry> Ext4Parser::parse_deleted_inodes(const uint8_t* data, size_t size,
                                                       const Ext4Superblock* sb, uint64_t partition_offset) {
    std::vector<FileEntry> files;
    
    uint32_t block_size = get_block_size(sb);
    uint32_t inode_size = sb->s_inode_size > 0 ? sb->s_inode_size : 128;
    uint32_t group_count = (sb->s_blocks_count_lo + sb->s_blocks_per_group - 1) / sb->s_blocks_per_group;
    
    LOG_DEBUG("Searching for deleted inodes");
    
    for (uint32_t group = 0; group < group_count && group < 1000; group++) {
        uint64_t inode_table_offset = get_inode_table_offset(group, sb);
        
        if (inode_table_offset >= size) {
            continue;
        }
        
        uint32_t inodes_in_group = std::min(sb->s_inodes_per_group,
                                           sb->s_inodes_count - group * sb->s_inodes_per_group);
        
        for (uint32_t i = 0; i < inodes_in_group; i++) {
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
            
            if (file_size == 0 || file_size > (1ULL << 40)) {
                continue;
            }
            
            auto file_entry = parse_inode_to_file_entry(inode, inode_number, data, sb, partition_offset);
            file_entry.is_deleted = true;
            file_entry.filename = "deleted_" + std::to_string(inode_number) + ".recovered";
            
            files.push_back(file_entry);
        }
    }
    
    return files;
}

FileEntry Ext4Parser::parse_inode_to_file_entry(const Ext4Inode* inode, uint32_t inode_number,
                                               const uint8_t* data, const Ext4Superblock* sb,
                                               uint64_t partition_offset) {
    FileEntry entry;
    
    entry.filename = get_filename_from_inode(inode, inode_number, data, sb);
    entry.size = inode->i_size_lo;
    if (sb->s_feature_ro_compat & 0x0002) {
        entry.size |= static_cast<uint64_t>(inode->i_size_high) << 32;
    }
    
    entry.creation_time = inode->i_ctime;
    entry.modification_time = inode->i_mtime;
    entry.access_time = inode->i_atime;
    entry.deletion_time = inode->i_dtime;
    entry.is_deleted = (inode->i_dtime != 0);
    
    // Get data block locations
    auto data_blocks = get_data_blocks(inode, sb);
    uint32_t block_size = get_block_size(sb);
    
    for (uint64_t block : data_blocks) {
        uint64_t block_offset = partition_offset + block * block_size;
        entry.data_locations.push_back({block_offset, block_size});
    }
    
    // Set file type based on filename extension or content
    if (!entry.filename.empty()) {
        size_t dot_pos = entry.filename.find_last_of('.');
        if (dot_pos != std::string::npos) {
            entry.file_type = entry.filename.substr(dot_pos + 1);
        }
    }
    
    return entry;
}

std::string Ext4Parser::get_filename_from_inode(const Ext4Inode* inode, uint32_t inode_number,
                                               const uint8_t* data, const Ext4Superblock* sb) {
    // For now, generate a filename based on inode number
    // In a full implementation, we would traverse directory entries
    return "inode_" + std::to_string(inode_number);
}

std::vector<uint64_t> Ext4Parser::get_data_blocks(const Ext4Inode* inode, const Ext4Superblock* sb) {
    std::vector<uint64_t> blocks;
    
    // For simplicity, only handle direct blocks for now
    // A full implementation would handle indirect blocks and extents
    for (int i = 0; i < 12; i++) {
        if (inode->i_block[i] != 0) {
            blocks.push_back(inode->i_block[i]);
        }
    }
    
    return blocks;
}

uint64_t Ext4Parser::get_inode_table_offset(uint32_t group, const Ext4Superblock* sb) {
    uint32_t block_size = get_block_size(sb);
    
    // Group descriptor table starts after superblock
    uint64_t gdt_offset = (sb->s_first_data_block + 1) * block_size;
    
    // Each group descriptor is 32 bytes (or 64 for 64-bit ext4)
    uint32_t gdt_size = 32;
    if (sb->s_feature_incompat & 0x0080) { // 64BIT feature
        gdt_size = 64;
    }
    
    // Read the inode table block number from group descriptor
    // For simplicity, calculate estimated position
    uint64_t estimated_offset = SUPERBLOCK_OFFSET + sizeof(Ext4Superblock) + 
                               group * sb->s_inodes_per_group * (sb->s_inode_size > 0 ? sb->s_inode_size : 128);
    
    return estimated_offset;
}

uint32_t Ext4Parser::get_block_size(const Ext4Superblock* sb) const {
    return 1024 << sb->s_log_block_size;
}

bool Ext4Parser::is_deleted_inode(const Ext4Inode* inode) const {
    return inode->i_dtime != 0 && inode->i_links_count == 0 && inode->i_size_lo > 0;
}

std::string Ext4Parser::extract_filename_from_directory(uint32_t target_inode, const uint8_t* dir_data, 
                                                       size_t dir_size) {
    size_t offset = 0;
    
    while (offset + sizeof(Ext4DirEntry) < dir_size) {
        const auto* entry = reinterpret_cast<const Ext4DirEntry*>(dir_data + offset);
        
        if (entry->rec_len == 0) break;
        
        if (entry->inode == target_inode && entry->name_len > 0) {
            return std::string(entry->name, entry->name_len);
        }
        
        offset += entry->rec_len;
    }
    
    return "";
}

} // namespace FileRecovery
