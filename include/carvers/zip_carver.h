#pragma once

#include "carvers/base_carver.h"

namespace FileRecovery {

class ZipCarver : public BaseCarver {
public:
    ZipCarver();
    ~ZipCarver() override = default;

    // FileCarver interface implementation
    std::vector<std::string> getSupportedTypes() const override;
    std::vector<std::vector<Byte>> getFileSignatures() const override;
    std::vector<std::vector<Byte>> getFileFooters() const override;
    std::vector<RecoveredFile> carveFiles(const Byte* data, Size size, Offset base_offset) override;
    double validateFile(const RecoveredFile& file, const Byte* data) override;
    Size getMaxFileSize() const override;

private:
    struct ZipLocalFileHeader {
        uint32_t signature;
        uint16_t version_needed;
        uint16_t general_purpose_flag;
        uint16_t compression_method;
        uint16_t last_mod_time;
        uint16_t last_mod_date;
        uint32_t crc32;
        uint32_t compressed_size;
        uint32_t uncompressed_size;
        uint16_t filename_length;
        uint16_t extra_field_length;
    } __attribute__((packed));

    struct ZipCentralDirHeader {
        uint32_t signature;
        uint16_t version_made_by;
        uint16_t version_needed;
        uint16_t general_purpose_flag;
        uint16_t compression_method;
        uint16_t last_mod_time;
        uint16_t last_mod_date;
        uint32_t crc32;
        uint32_t compressed_size;
        uint32_t uncompressed_size;
        uint16_t filename_length;
        uint16_t extra_field_length;
        uint16_t file_comment_length;
        uint16_t disk_number_start;
        uint16_t internal_file_attributes;
        uint32_t external_file_attributes;
        uint32_t relative_offset_of_local_header;
    } __attribute__((packed));

    struct ZipEndOfCentralDir {
        uint32_t signature;
        uint16_t number_of_disk;
        uint16_t disk_with_central_dir;
        uint16_t central_dir_entries_on_disk;
        uint16_t total_central_dir_entries;
        uint32_t central_dir_size;
        uint32_t central_dir_offset;
        uint16_t comment_length;
    } __attribute__((packed));

    static constexpr uint32_t LOCAL_FILE_HEADER_SIG = 0x04034b50;
    static constexpr uint32_t CENTRAL_DIR_HEADER_SIG = 0x02014b50;
    static constexpr uint32_t END_OF_CENTRAL_DIR_SIG = 0x06054b50;
    static constexpr uint32_t DATA_DESCRIPTOR_SIG = 0x08074b50;

    bool validate_zip_structure(const uint8_t* data, size_t size) const;
    size_t find_end_of_central_directory(const uint8_t* data, size_t size) const;
    bool validate_local_file_header(const ZipLocalFileHeader* header) const;
    bool validate_central_dir_header(const ZipCentralDirHeader* header) const;
    bool validate_end_of_central_dir(const ZipEndOfCentralDir* header) const;
    
    size_t calculate_zip_size(const uint8_t* data, size_t max_size) const;
    std::string extract_zip_metadata(const uint8_t* data, size_t size) const;
    uint32_t count_zip_entries(const uint8_t* data, size_t size) const;
    
    double calculateConfidence(const Byte* data, Size size) const;
};

} // namespace FileRecovery
