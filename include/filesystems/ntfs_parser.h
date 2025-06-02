#pragma once

#include "interfaces/filesystem_parser.h"
#include "utils/types.h"

namespace FileRecovery {

class NtfsParser : public FilesystemParser {
public:
    NtfsParser();
    ~NtfsParser() override = default;

    // Interface implementations
    bool initialize(const Byte* data, Size size) override;
    bool canParse(const Byte* data, Size size) const override;
    FileSystemType getFileSystemType() const override { return FileSystemType::NTFS; }
    std::vector<RecoveredFile> recoverDeletedFiles() override;
    std::string getFileSystemInfo() const override;

public:
    struct NtfsBootSector {
        uint8_t jump[3];
        char oem_id[8];
        uint16_t bytes_per_sector;
        uint8_t sectors_per_cluster;
        uint16_t reserved_sectors;
        uint8_t fats;
        uint16_t root_entries;
        uint16_t sectors;
        uint8_t media_type;
        uint16_t fat_length;
        uint16_t sectors_per_track;
        uint16_t heads;
        uint32_t hidden_sectors;
        uint32_t large_sectors;
        uint32_t unused;
        uint64_t total_sectors;
        uint64_t mft_lcn;
        uint64_t mft_mirror_lcn;
        int8_t clusters_per_mft_record;
        uint8_t reserved1[3];
        int8_t clusters_per_index_record;
        uint8_t reserved2[3];
        uint64_t volume_serial;
        uint32_t checksum;
        uint8_t bootstrap[426];
        uint16_t end_marker;
    } __attribute__((packed));

    struct MftRecord {
        char signature[4];
        uint16_t update_sequence_offset;
        uint16_t update_sequence_count;
        uint64_t lsn;
        uint16_t sequence_number;
        uint16_t hard_link_count;
        uint16_t first_attribute_offset;
        uint16_t flags;
        uint32_t used_size;
        uint32_t allocated_size;
        uint64_t base_record;
        uint16_t next_attribute_id;
        uint16_t reserved;
        uint32_t record_number;
    } __attribute__((packed));

    struct AttributeHeader {
        uint32_t type;
        uint32_t length;
        uint8_t non_resident_flag;
        uint8_t name_length;
        uint16_t name_offset;
        uint16_t flags;
        uint16_t attribute_id;
        union {
            struct {
                uint32_t value_length;
                uint16_t value_offset;
                uint8_t indexed;
                uint8_t reserved;
            } resident;
            struct {
                uint64_t start_vcn;
                uint64_t end_vcn;
                uint16_t run_list_offset;
                uint8_t compression_unit_size;
                uint8_t reserved[5];
                uint64_t allocated_size;
                uint64_t data_size;
                uint64_t initialized_size;
            } non_resident_data;
        };
    } __attribute__((packed));

    // NTFS Attribute Types
    static constexpr uint32_t AT_STANDARD_INFORMATION = 0x10;
    static constexpr uint32_t AT_ATTRIBUTE_LIST = 0x20;
    static constexpr uint32_t AT_FILE_NAME = 0x30;
    static constexpr uint32_t AT_OBJECT_ID = 0x40;
    static constexpr uint32_t AT_SECURITY_DESCRIPTOR = 0x50;
    static constexpr uint32_t AT_VOLUME_NAME = 0x60;
    static constexpr uint32_t AT_VOLUME_INFORMATION = 0x70;
    static constexpr uint32_t AT_DATA = 0x80;
    static constexpr uint32_t AT_INDEX_ROOT = 0x90;
    static constexpr uint32_t AT_INDEX_ALLOCATION = 0xA0;
    static constexpr uint32_t AT_BITMAP = 0xB0;

    // MFT Record Flags
    static constexpr uint16_t MFT_RECORD_IN_USE = 0x0001;
    static constexpr uint16_t MFT_RECORD_IS_DIRECTORY = 0x0002;

    bool validate_boot_sector(const NtfsBootSector* boot) const;
    bool validate_mft_record(const MftRecord* record) const;
    
    std::vector<RecoveredFile> parse_mft_records(const uint8_t* data, size_t size,
                                               const NtfsBootSector* boot, uint64_t partition_offset);
    
    RecoveredFile parse_mft_record_to_file(const MftRecord* record, const uint8_t* record_data,
                                          const NtfsBootSector* boot, uint64_t partition_offset);
    
    std::string extract_filename_attribute(const uint8_t* record_data, size_t record_size);
    uint64_t extract_file_size_attribute(const uint8_t* record_data, size_t record_size);
    std::vector<std::pair<Offset, Size>> extract_data_runs(const uint8_t* record_data, size_t record_size,
                                                          const NtfsBootSector* boot, uint64_t partition_offset);
    
    uint64_t get_mft_offset(const NtfsBootSector* boot) const;
    uint32_t get_cluster_size(const NtfsBootSector* boot) const;
    uint32_t get_mft_record_size(const NtfsBootSector* boot) const;
    
    std::vector<uint64_t> parse_data_runs(const uint8_t* run_data, size_t run_length,
                                         uint32_t cluster_size, uint64_t partition_offset);
};

} // namespace FileRecovery
