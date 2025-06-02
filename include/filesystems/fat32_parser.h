#pragma once

#include "interfaces/filesystem_parser.h"
#include "utils/types.h"

namespace FileRecovery {

class Fat32Parser : public FilesystemParser {
public:
    Fat32Parser();
    ~Fat32Parser() override = default;

    // Interface implementations
    bool initialize(const Byte* data, Size size) override;
    bool canParse(const Byte* data, Size size) const override;
    FileSystemType getFileSystemType() const override { return FileSystemType::FAT32; }
    std::vector<RecoveredFile> recoverDeletedFiles() override;
    std::string getFileSystemInfo() const override;

public:
    struct Fat32BootSector {
        uint8_t jump_boot[3];
        char oem_name[8];
        uint16_t bytes_per_sector;
        uint8_t sectors_per_cluster;
        uint16_t reserved_sector_count;
        uint8_t table_count;
        uint16_t root_entry_count;
        uint16_t sector_count_16;
        uint8_t media_type;
        uint16_t table_size_16;
        uint16_t sectors_per_track;
        uint16_t head_side_count;
        uint32_t hidden_sector_count;
        uint32_t sector_count_32;
        uint32_t table_size_32;
        uint16_t extended_flags;
        uint16_t fat_version;
        uint32_t root_cluster;
        uint16_t fat_info;
        uint16_t backup_BS_sector;
        uint8_t reserved_0[12];
        uint8_t drive_number;
        uint8_t reserved_1;
        uint8_t boot_signature;
        uint32_t volume_id;
        char volume_label[11];
        char fat_type_label[8];
        uint8_t boot_code[420];
        uint16_t bootable_partition_signature;
    } __attribute__((packed));

    struct Fat32DirEntry {
        char filename[11];
        uint8_t attributes;
        uint8_t reserved;
        uint8_t creation_time_tenths;
        uint16_t creation_time;
        uint16_t creation_date;
        uint16_t last_access_date;
        uint16_t first_cluster_high;
        uint16_t last_write_time;
        uint16_t last_write_date;
        uint16_t first_cluster_low;
        uint32_t file_size;
    } __attribute__((packed));

    struct LongNameEntry {
        uint8_t order;
        uint16_t name1[5];
        uint8_t attributes;
        uint8_t type;
        uint8_t checksum;
        uint16_t name2[6];
        uint16_t first_cluster_low;
        uint16_t name3[2];
    } __attribute__((packed));

    // FAT32 constants
    static constexpr uint8_t ATTR_READ_ONLY = 0x01;
    static constexpr uint8_t ATTR_HIDDEN = 0x02;
    static constexpr uint8_t ATTR_SYSTEM = 0x04;
    static constexpr uint8_t ATTR_VOLUME_ID = 0x08;
    static constexpr uint8_t ATTR_DIRECTORY = 0x10;
    static constexpr uint8_t ATTR_ARCHIVE = 0x20;
    static constexpr uint8_t ATTR_LONG_NAME = 0x0F;

    static constexpr uint32_t EOC_MARK = 0x0FFFFFF8;
    static constexpr uint32_t BAD_CLUSTER = 0x0FFFFFF7;
    static constexpr uint32_t FREE_CLUSTER = 0x00000000;

    bool validate_boot_sector(const Fat32BootSector* boot) const;
    
    std::vector<RecoveredFile> parse_directory_entries(const uint8_t* data, size_t size,
                                                      const Fat32BootSector* boot, uint64_t partition_offset);
    
    std::vector<RecoveredFile> parse_deleted_entries(const uint8_t* data, size_t size,
                                                    const Fat32BootSector* boot, uint64_t partition_offset);
    
    RecoveredFile parse_dir_entry_to_file(const Fat32DirEntry* entry, const std::string& long_name,
                                         const Fat32BootSector* boot, uint64_t partition_offset);
    
    std::string extract_short_name(const Fat32DirEntry* entry) const;
    std::string extract_long_name(const std::vector<LongNameEntry>& lfn_entries) const;
    
    std::vector<uint32_t> get_cluster_chain(uint32_t start_cluster, const uint8_t* fat_table, 
                                           const Fat32BootSector* boot) const;
    
    uint64_t cluster_to_sector(uint32_t cluster, const Fat32BootSector* boot) const;
    uint64_t get_fat_offset(const Fat32BootSector* boot) const;
    uint64_t get_data_offset(const Fat32BootSector* boot) const;
    uint32_t get_cluster_size(const Fat32BootSector* boot) const;
    
    bool is_valid_cluster(uint32_t cluster) const;
    uint32_t fat_entry_value(const uint8_t* fat_table, uint32_t cluster) const;
    
    time_t fat_time_to_unix(uint16_t time, uint16_t date) const;
    std::string determine_file_type(const std::string& filename);
};

} // namespace FileRecovery
