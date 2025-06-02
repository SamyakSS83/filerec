#include "utils/progress_tracker.h"
#include <algorithm>

namespace FileRecovery {

ProgressTracker::ProgressTracker()
    : total_bytes_(0)
    , bytes_processed_(0)
    , files_found_(0)
    , files_recovered_(0)
    , active_(false)
    , start_time_(std::chrono::steady_clock::now())
    , last_update_(std::chrono::steady_clock::now()) {
}

ProgressTracker::~ProgressTracker() {
    stop();
}

void ProgressTracker::set_total_bytes(uint64_t total) {
    total_bytes_ = total;
    notify_progress();
}

void ProgressTracker::update_bytes_processed(uint64_t bytes) {
    bytes_processed_ = bytes;
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update_);
    
    // Update progress every 100ms to avoid too frequent callbacks
    if (elapsed.count() >= 100) {
        last_update_ = now;
        notify_progress();
    }
}

void ProgressTracker::increment_files_found() {
    files_found_++;
    notify_progress();
}

void ProgressTracker::increment_files_recovered() {
    files_recovered_++;
    notify_progress();
}

void ProgressTracker::set_current_operation(const std::string& operation) {
    std::lock_guard<std::mutex> lock(mutex_);
    current_operation_ = operation;
    notify_progress();
}

void ProgressTracker::set_current_file_type(const std::string& file_type) {
    std::lock_guard<std::mutex> lock(mutex_);
    current_file_type_ = file_type;
    notify_progress();
}

ProgressInfo ProgressTracker::get_progress() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    uint64_t total = total_bytes_.load();
    uint64_t processed = bytes_processed_.load();
    
    double percentage = 0.0;
    if (total > 0) {
        percentage = (static_cast<double>(processed) / static_cast<double>(total)) * 100.0;
        percentage = std::min(100.0, percentage);
    }
    
    return {
        processed,
        total,
        files_found_.load(),
        files_recovered_.load(),
        percentage,
        calculate_speed_mbps(),
        estimate_time_remaining(),
        current_operation_,
        current_file_type_
    };
}

void ProgressTracker::set_callback(const ProgressCallback& callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = callback;
}

void ProgressTracker::start() {
    active_ = true;
    start_time_ = std::chrono::steady_clock::now();
    last_update_ = start_time_;
    
    std::lock_guard<std::mutex> lock(mutex_);
    current_operation_ = "Starting recovery...";
    notify_progress();
}

void ProgressTracker::stop() {
    active_ = false;
    
    std::lock_guard<std::mutex> lock(mutex_);
    current_operation_ = "Recovery completed";
    notify_progress();
}

void ProgressTracker::reset() {
    bytes_processed_ = 0;
    files_found_ = 0;
    files_recovered_ = 0;
    total_bytes_ = 0;
    active_ = false;
    
    std::lock_guard<std::mutex> lock(mutex_);
    current_operation_.clear();
    current_file_type_.clear();
    start_time_ = std::chrono::steady_clock::now();
    last_update_ = start_time_;
}

bool ProgressTracker::is_active() const {
    return active_.load();
}

void ProgressTracker::notify_progress() {
    if (!active_.load()) return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    if (callback_) {
        auto info = get_progress();
        callback_(info);
    }
}

double ProgressTracker::calculate_speed_mbps() const {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_);
    
    if (elapsed.count() == 0) return 0.0;
    
    uint64_t processed = bytes_processed_.load();
    double seconds = elapsed.count() / 1000.0;
    double mbytes = processed / (1024.0 * 1024.0);
    
    return mbytes / seconds;
}

std::chrono::seconds ProgressTracker::estimate_time_remaining() const {
    uint64_t total = total_bytes_.load();
    uint64_t processed = bytes_processed_.load();
    
    if (processed == 0 || total == 0 || processed >= total) {
        return std::chrono::seconds(0);
    }
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_);
    
    if (elapsed.count() == 0) return std::chrono::seconds(0);
    
    double progress_ratio = static_cast<double>(processed) / static_cast<double>(total);
    double estimated_total_time = elapsed.count() / progress_ratio;
    double remaining_time = estimated_total_time - elapsed.count();
    
    return std::chrono::seconds(static_cast<long>(std::max(0.0, remaining_time)));
}

} // namespace FileRecovery
