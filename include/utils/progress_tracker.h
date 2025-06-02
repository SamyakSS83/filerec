#pragma once

#include "utils/types.h"
#include <atomic>
#include <functional>
#include <string>
#include <chrono>
#include <mutex>

namespace FileRecovery {

struct ProgressInfo {
    uint64_t bytes_processed;
    uint64_t total_bytes;
    uint32_t files_found;
    uint32_t files_recovered;
    double progress_percentage;
    double speed_mbps;
    std::chrono::seconds estimated_time_remaining;
    std::string current_operation;
    std::string current_file_type;
};

using ProgressCallback = std::function<void(const ProgressInfo&)>;

class ProgressTracker {
public:
    ProgressTracker();
    ~ProgressTracker();

    /**
     * Set total bytes to process
     */
    void set_total_bytes(uint64_t total);

    /**
     * Update processed bytes
     */
    void update_bytes_processed(uint64_t bytes);

    /**
     * Increment files found counter
     */
    void increment_files_found();

    /**
     * Increment files recovered counter
     */
    void increment_files_recovered();

    /**
     * Set current operation description
     */
    void set_current_operation(const std::string& operation);

    /**
     * Set current file type being processed
     */
    void set_current_file_type(const std::string& file_type);

    /**
     * Get current progress information
     */
    ProgressInfo get_progress() const;

    /**
     * Set progress callback for real-time updates
     */
    void set_callback(const ProgressCallback& callback);

    /**
     * Start progress tracking
     */
    void start();

    /**
     * Stop progress tracking
     */
    void stop();

    /**
     * Reset all counters
     */
    void reset();

    /**
     * Check if progress tracking is active
     */
    bool is_active() const;

private:
    mutable std::mutex mutex_;
    std::atomic<uint64_t> total_bytes_;
    std::atomic<uint64_t> bytes_processed_;
    std::atomic<uint32_t> files_found_;
    std::atomic<uint32_t> files_recovered_;
    std::atomic<bool> active_;
    
    std::string current_operation_;
    std::string current_file_type_;
    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point last_update_;
    
    ProgressCallback callback_;
    
    void notify_progress();
    double calculate_speed_mbps() const;
    std::chrono::seconds estimate_time_remaining() const;
};

} // namespace FileRecovery
