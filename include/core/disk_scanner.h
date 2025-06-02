#pragma once

#include <string>
#include <memory>
#include <fstream>
#include <mutex>
#include "utils/types.h"

namespace FileRecovery {

/**
 * @brief Thread-safe disk scanner for raw disk access
 * 
 * Provides safe, read-only access to disk devices with memory mapping
 * and efficient chunked reading capabilities.
 */
class DiskScanner {
public:
    /**
     * @brief Constructor
     * @param device_path Path to the device (e.g., "/dev/sda1")
     */
    explicit DiskScanner(const std::string& device_path);
    
    /**
     * @brief Destructor - ensures proper cleanup
     */
    ~DiskScanner();
    
    /**
     * @brief Initialize the scanner and open the device
     * @return true if successful
     */
    bool initialize();
    
    /**
     * @brief Check if the scanner is ready for use
     * @return true if initialized and device is accessible
     */
    bool isReady() const { return is_initialized_; }
    
    /**
     * @brief Get the total size of the device
     * @return Device size in bytes
     */
    Size getDeviceSize() const { return device_size_; }
    
    /**
     * @brief Get the device path
     * @return Device path string
     */
    const std::string& getDevicePath() const { return device_path_; }
    
    /**
     * @brief Read a chunk of data from the device
     * @param offset Offset to start reading from
     * @param size Number of bytes to read
     * @param buffer Buffer to store the data
     * @return Number of bytes actually read
     */
    Size readChunk(Offset offset, Size size, Byte* buffer);
    
    /**
     * @brief Memory-map a region of the device (for large sequential reads)
     * @param offset Offset to start mapping from
     * @param size Size of the region to map
     * @return Pointer to mapped memory, or nullptr on failure
     */
    const Byte* mapRegion(Offset offset, Size size);
    
    /**
     * @brief Unmap a previously mapped region
     * @param ptr Pointer returned by mapRegion
     * @param size Size of the mapped region
     */
    void unmapRegion(const Byte* ptr, Size size);
    
    /**
     * @brief Read the entire device into memory (only for small devices)
     * @param max_size Maximum size to read (safety limit)
     * @return Vector containing all device data
     */
    std::vector<Byte> readEntireDevice(Size max_size = 32ULL * 1024 * 1024 * 1024); // 32GB limit
    
    /**
     * @brief Get device information
     * @return String containing device details
     */
    std::string getDeviceInfo() const;
    
    /**
     * @brief Check if the device is read-only mounted
     * @return true if read-only
     */
    bool isReadOnly() const;
    
private:
    std::string device_path_;
    int device_fd_;
    Size device_size_;
    bool is_initialized_;
    mutable std::mutex access_mutex_;
    
    /**
     * @brief Get the size of the device file/block device
     * @return Device size in bytes, or 0 on error
     */
    Size getDeviceSizeInternal() const;
    
    /**
     * @brief Verify that the device is accessible and safe to read
     * @return true if device is safe to access
     */
    bool verifyDeviceAccess() const;
};

} // namespace FileRecovery
