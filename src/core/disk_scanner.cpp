#include "core/disk_scanner.h"
#include "utils/logger.h"
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/fs.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <cstring>

namespace FileRecovery {

DiskScanner::DiskScanner(const std::string& device_path)
    : device_path_(device_path)
    , device_fd_(-1)
    , device_size_(0)
    , is_initialized_(false) {
}

DiskScanner::~DiskScanner() {
    if (device_fd_ >= 0) {
        close(device_fd_);
    }
}

bool DiskScanner::initialize() {
    std::lock_guard<std::mutex> lock(access_mutex_);
    
    if (is_initialized_) {
        return true;
    }
    
    LOG_INFO("Initializing disk scanner for device: " + device_path_);
    
    // Verify device access before opening
    if (!verifyDeviceAccess()) {
        LOG_ERROR("Device access verification failed for: " + device_path_);
        return false;
    }
    
    // Open device in read-only mode
    device_fd_ = open(device_path_.c_str(), O_RDONLY | O_LARGEFILE);
    if (device_fd_ < 0) {
        LOG_ERROR("Failed to open device: " + device_path_ + " - " + std::string(strerror(errno)));
        return false;
    }
    
    // Get device size
    device_size_ = getDeviceSizeInternal();
    if (device_size_ == 0) {
        LOG_ERROR("Failed to determine device size for: " + device_path_);
        close(device_fd_);
        device_fd_ = -1;
        return false;
    }
    
    is_initialized_ = true;
    LOG_INFO("Successfully initialized disk scanner. Device size: " + std::to_string(device_size_) + " bytes");
    
    return true;
}

Size DiskScanner::readChunk(Offset offset, Size size, Byte* buffer) {
    if (!is_initialized_ || device_fd_ < 0) {
        LOG_ERROR("Scanner not initialized");
        return 0;
    }
    
    if (offset >= device_size_) {
        LOG_WARNING("Read offset beyond device size");
        return 0;
    }
    
    // Adjust size if it would read beyond device end
    if (offset + size > device_size_) {
        size = device_size_ - offset;
    }
    
    std::lock_guard<std::mutex> lock(access_mutex_);
    
    // Seek to offset
    if (lseek(device_fd_, offset, SEEK_SET) != static_cast<off_t>(offset)) {
        LOG_ERROR("Failed to seek to offset: " + std::to_string(offset));
        return 0;
    }
    
    // Read data
    ssize_t bytes_read = read(device_fd_, buffer, size);
    if (bytes_read < 0) {
        LOG_ERROR("Failed to read from device: " + std::string(strerror(errno)));
        return 0;
    }
    
    return static_cast<Size>(bytes_read);
}

const Byte* DiskScanner::mapRegion(Offset offset, Size size) {
    if (!is_initialized_ || device_fd_ < 0) {
        LOG_ERROR("Scanner not initialized");
        return nullptr;
    }
    
    if (offset >= device_size_ || offset + size > device_size_) {
        LOG_ERROR("Map region beyond device boundaries");
        return nullptr;
    }
    
    // Align offset to page boundary
    size_t page_size = getpagesize();
    Offset aligned_offset = (offset / page_size) * page_size;
    Size offset_adjustment = offset - aligned_offset;
    Size aligned_size = size + offset_adjustment;
    
    void* mapped = mmap(nullptr, aligned_size, PROT_READ, MAP_PRIVATE, device_fd_, aligned_offset);
    if (mapped == MAP_FAILED) {
        LOG_ERROR("Failed to map region: " + std::string(strerror(errno)));
        return nullptr;
    }
    
    // Return pointer adjusted for the original offset
    return static_cast<const Byte*>(mapped) + offset_adjustment;
}

void DiskScanner::unmapRegion(const Byte* ptr, Size size) {
    if (ptr == nullptr) {
        return;
    }
    
    // Calculate the actual mapped address (page-aligned)
    size_t page_size = getpagesize();
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    uintptr_t aligned_addr = (addr / page_size) * page_size;
    Size offset_adjustment = addr - aligned_addr;
    Size aligned_size = size + offset_adjustment;
    
    if (munmap(reinterpret_cast<void*>(aligned_addr), aligned_size) != 0) {
        LOG_ERROR("Failed to unmap region: " + std::string(strerror(errno)));
    }
}

std::vector<Byte> DiskScanner::readEntireDevice(Size max_size) {
    if (!is_initialized_) {
        LOG_ERROR("Scanner not initialized");
        return {};
    }
    
    if (device_size_ > max_size) {
        LOG_ERROR("Device size exceeds maximum allowed size for full read");
        return {};
    }
    
    LOG_INFO("Reading entire device into memory (" + std::to_string(device_size_) + " bytes)");
    
    std::vector<Byte> data(device_size_);
    Size bytes_read = readChunk(0, device_size_, data.data());
    
    if (bytes_read != device_size_) {
        LOG_ERROR("Failed to read complete device");
        return {};
    }
    
    return data;
}

std::string DiskScanner::getDeviceInfo() const {
    if (!is_initialized_) {
        return "Device not initialized";
    }
    
    std::string info = "Device: " + device_path_ + "\\n";
    info += "Size: " + std::to_string(device_size_) + " bytes\\n";
    info += "Read-only: " + std::string(isReadOnly() ? "Yes" : "No");
    
    return info;
}

bool DiskScanner::isReadOnly() const {
    struct stat st;
    if (stat(device_path_.c_str(), &st) != 0) {
        return false;
    }
    
    // Check if we have write permissions
    return access(device_path_.c_str(), W_OK) != 0;
}

Size DiskScanner::getDeviceSizeInternal() const {
    struct stat st;
    if (fstat(device_fd_, &st) != 0) {
        LOG_ERROR("Failed to stat device");
        return 0;
    }
    
    if (S_ISREG(st.st_mode)) {
        // Regular file
        return static_cast<Size>(st.st_size);
    } else if (S_ISBLK(st.st_mode)) {
        // Block device
        uint64_t size;
        if (ioctl(device_fd_, BLKGETSIZE64, &size) == 0) {
            return static_cast<Size>(size);
        } else {
            LOG_ERROR("Failed to get block device size");
            return 0;
        }
    }
    
    LOG_ERROR("Unsupported device type");
    return 0;
}

bool DiskScanner::verifyDeviceAccess() const {
    // Check if device exists
    struct stat st;
    if (stat(device_path_.c_str(), &st) != 0) {
        LOG_ERROR("Device does not exist: " + device_path_);
        return false;
    }
    
    // Check if we have read permissions
    if (access(device_path_.c_str(), R_OK) != 0) {
        LOG_ERROR("No read permission for device: " + device_path_);
        return false;
    }
    
    // Verify it's a regular file or block device
    if (!S_ISREG(st.st_mode) && !S_ISBLK(st.st_mode)) {
        LOG_ERROR("Device is not a regular file or block device: " + device_path_);
        return false;
    }
    
    return true;
}

} // namespace FileRecovery
