#pragma once

#include <vector>
#include <memory>
#include <thread>
#include <future>
#include <atomic>
#include "utils/types.h"
#include "core/disk_scanner.h"
#include "interfaces/filesystem_parser.h"
#include "interfaces/file_carver.h"

namespace FileRecovery {

/**
 * @brief Main recovery engine that coordinates all recovery operations
 * 
 * This class orchestrates metadata-based and signature-based recovery,
 * manages multithreading, and provides progress tracking.
 */
class RecoveryEngine {
public:
    /**
     * @brief Constructor
     * @param config Recovery configuration
     */
    explicit RecoveryEngine(const ScanConfig& config);
    
    /**
     * @brief Destructor
     */
    ~RecoveryEngine();
    
    /**
     * @brief Start the recovery process
     * @return Recovery status
     */
    RecoveryStatus startRecovery();
    
    /**
     * @brief Stop the recovery process
     */
    void stopRecovery();
    
    /**
     * @brief Check if recovery is currently running
     * @return true if recovery is in progress
     */
    bool isRunning() const { return is_running_; }
    
    /**
     * @brief Get current progress percentage
     * @return Progress as percentage (0.0 - 100.0)
     */
    double getProgress() const;
    
    /**
     * @brief Get the number of files recovered so far
     * @return Number of recovered files
     */
    size_t getRecoveredFileCount() const { return recovered_files_.size(); }
    
    /**
     * @brief Get all recovered files
     * @return Vector of recovered files
     */
    const std::vector<RecoveredFile>& getRecoveredFiles() const { return recovered_files_; }
    
    /**
     * @brief Add a custom file carver
     * @param carver Unique pointer to the carver
     */
    void addFileCarver(std::unique_ptr<FileCarver> carver);
    
    /**
     * @brief Add a custom file system parser
     * @param parser Unique pointer to the parser
     */
    void addFilesystemParser(std::unique_ptr<FilesystemParser> parser);
    
    /**
     * @brief Set progress callback function
     * @param callback Function to call with progress updates
     */
    void setProgressCallback(std::function<void(double, const std::string&)> callback);
    
private:
    ScanConfig config_;
    std::unique_ptr<DiskScanner> disk_scanner_;
    std::vector<std::unique_ptr<FileCarver>> file_carvers_;
    std::vector<std::unique_ptr<FilesystemParser>> filesystem_parsers_;
    std::vector<RecoveredFile> recovered_files_;
    
    std::atomic<bool> is_running_;
    std::atomic<bool> should_stop_;
    std::atomic<double> current_progress_;
    std::function<void(double, const std::string&)> progress_callback_;
    
    mutable std::mutex results_mutex_;
    std::vector<std::thread> worker_threads_;
    
    /**
     * @brief Initialize all default carvers and parsers
     */
    void initializeDefaultModules();
    
    /**
     * @brief Perform metadata-based recovery
     * @return Vector of files recovered using metadata
     */
    std::vector<RecoveredFile> performMetadataRecovery();
    
    /**
     * @brief Perform signature-based recovery
     * @return Vector of files recovered using signatures
     */
    std::vector<RecoveredFile> performSignatureRecovery();
    
    /**
     * @brief Worker function for multithreaded signature scanning
     * @param chunk_start Start offset of the chunk
     * @param chunk_size Size of the chunk to scan
     * @param thread_id Thread identifier
     */
    void scanChunkWorker(Offset chunk_start, Size chunk_size, int thread_id);
    
    /**
     * @brief Save a recovered file to disk
     * @param file Recovered file information
     * @return true if file was saved successfully
     */
    bool saveRecoveredFile(const RecoveredFile& file);
    
    /**
     * @brief Deduplicate recovered files (remove duplicates)
     */
    void deduplicateFiles();
    
    /**
     * @brief Update progress and call callback if set
     * @param progress New progress value
     * @param status_message Status message
     */
    void updateProgress(double progress, const std::string& status_message);
    
    /**
     * @brief Get optimal number of threads for current system
     * @return Number of threads to use
     */
    size_t getOptimalThreadCount() const;
};

} // namespace FileRecovery
