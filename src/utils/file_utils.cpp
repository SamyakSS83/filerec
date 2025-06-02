#include "utils/file_utils.h"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <openssl/sha.h>

namespace FileRecovery {

std::string FileUtils::calculateSHA256(const Byte* data, Size size) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(data, size, hash);
    
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    
    return ss.str();
}

std::string FileUtils::getFileExtension(const std::string& filename) {
    size_t dot_pos = filename.find_last_of('.');
    if (dot_pos == std::string::npos || dot_pos == filename.length() - 1) {
        return "";
    }
    return filename.substr(dot_pos + 1);
}

std::string FileUtils::formatFileSize(Size size) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    const int num_units = sizeof(units) / sizeof(units[0]);
    
    double size_double = static_cast<double>(size);
    int unit_index = 0;
    
    while (size_double >= 1024.0 && unit_index < num_units - 1) {
        size_double /= 1024.0;
        unit_index++;
    }
    
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1) << size_double << " " << units[unit_index];
    return ss.str();
}

std::string FileUtils::formatDuration(std::chrono::duration<double> duration) {
    auto total_seconds = static_cast<int>(duration.count());
    
    int hours = total_seconds / 3600;
    int minutes = (total_seconds % 3600) / 60;
    int seconds = total_seconds % 60;
    
    std::stringstream ss;
    if (hours > 0) {
        ss << hours << "h ";
    }
    if (minutes > 0 || hours > 0) {
        ss << minutes << "m ";
    }
    ss << seconds << "s";
    
    return ss.str();
}

bool FileUtils::isDirectoryWritable(const std::string& path) {
    try {
        std::filesystem::path fs_path(path);
        
        // Check if directory exists
        if (!std::filesystem::exists(fs_path)) {
            return false;
        }
        
        // Check if it's actually a directory
        if (!std::filesystem::is_directory(fs_path)) {
            return false;
        }
        
        // Try to create a temporary file to test write permissions
        std::filesystem::path temp_file = fs_path / ".temp_write_test";
        std::ofstream test_file(temp_file);
        if (test_file.is_open()) {
            test_file.close();
            std::filesystem::remove(temp_file);
            return true;
        }
        
        return false;
        
    } catch (const std::exception&) {
        return false;
    }
}

Size FileUtils::getAvailableSpace(const std::string& path) {
    try {
        std::filesystem::space_info space = std::filesystem::space(path);
        return static_cast<Size>(space.available);
    } catch (const std::exception&) {
        return 0;
    }
}

bool FileUtils::createDirectory(const std::string& path) {
    try {
        return std::filesystem::create_directories(path);
    } catch (const std::exception&) {
        return false;
    }
}

std::string FileUtils::generateUniqueFilename(const std::string& base_path) {
    std::filesystem::path fs_path(base_path);
    
    if (!std::filesystem::exists(fs_path)) {
        return base_path;
    }
    
    std::string stem = fs_path.stem().string();
    std::string extension = fs_path.extension().string();
    std::filesystem::path parent = fs_path.parent_path();
    
    int counter = 1;
    std::filesystem::path new_path;
    
    do {
        std::string new_filename = stem + "_" + std::to_string(counter) + extension;
        new_path = parent / new_filename;
        counter++;
    } while (std::filesystem::exists(new_path));
    
    return new_path.string();
}

} // namespace FileRecovery
