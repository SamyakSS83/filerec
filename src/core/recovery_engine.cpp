#include "core/recovery_engine.h"
#include "core/file_system_detector.h"
#include "carvers/jpeg_carver.h"
#include "carvers/pdf_carver.h"
#include "carvers/png_carver.h"
#include "carvers/zip_carver.h"
#include "filesystems/ext4_parser.h"
#include "filesystems/ntfs_parser.h"
#include "filesystems/fat32_parser.h"
#include "utils/logger.h"
#include <thread>
#include <future>
#include <algorithm>
#include <filesystem>
#include <fstream>

namespace FileRecovery {

RecoveryEngine::RecoveryEngine(const ScanConfig& config)
    : config_(config)
    , disk_scanner_(std::make_unique<DiskScanner>(config.device_path))
    , is_running_(false)
    , should_stop_(false)
    , current_progress_(0.0) {
    
    initializeDefaultModules();
}

RecoveryEngine::~RecoveryEngine() {
    stopRecovery();
}

RecoveryStatus RecoveryEngine::startRecovery() {
    if (is_running_) {
        LOG_WARNING("Recovery already in progress");
        return RecoveryStatus::FAILED;
    }
    
    LOG_INFO("Starting file recovery for device: " + config_.device_path);
    is_running_ = true;
    should_stop_ = false;
    current_progress_ = 0.0;
    
    // Initialize disk scanner
    if (!disk_scanner_->initialize()) {
        LOG_ERROR("Failed to initialize disk scanner");
        is_running_ = false;
        return RecoveryStatus::DEVICE_NOT_FOUND;
    }
    
    // Create output directory if it doesn't exist
    try {
        std::filesystem::create_directories(config_.output_directory);
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to create output directory: " + std::string(e.what()));
        is_running_ = false;
        return RecoveryStatus::INSUFFICIENT_SPACE;
    }
    
    updateProgress(5.0, "Initialization complete, starting recovery...");
    
    RecoveryStatus status = RecoveryStatus::SUCCESS;
    
    try {
        // Phase 1: Metadata-based recovery (if enabled)
        if (config_.use_metadata_recovery) {
            updateProgress(10.0, "Performing metadata-based recovery...");
            auto metadata_files = performMetadataRecovery();
            
            std::lock_guard<std::mutex> lock(results_mutex_);
            recovered_files_.insert(recovered_files_.end(), 
                                  metadata_files.begin(), metadata_files.end());
            
            updateProgress(30.0, "Metadata recovery complete");
        }
        
        // Phase 2: Signature-based recovery (if enabled)
        if (config_.use_signature_recovery && !should_stop_) {
            updateProgress(35.0, "Performing signature-based recovery...");
            auto signature_files = performSignatureRecovery();
            
            std::lock_guard<std::mutex> lock(results_mutex_);
            recovered_files_.insert(recovered_files_.end(), 
                                  signature_files.begin(), signature_files.end());
            
            updateProgress(80.0, "Signature recovery complete");
        }
        
        // Phase 3: Post-processing
        if (!should_stop_) {
            updateProgress(85.0, "Post-processing results...");
            deduplicateFiles();
            updateProgress(90.0, "Saving recovered files...");
            
            // Save all recovered files
            size_t saved_count = 0;
            for (const auto& file : recovered_files_) {
                if (should_stop_) break;
                if (saveRecoveredFile(file)) {
                    saved_count++;
                }
            }
            
            LOG_INFO("Recovery complete. Saved " + std::to_string(saved_count) + 
                    " out of " + std::to_string(recovered_files_.size()) + " files");
        }
        
        updateProgress(100.0, "Recovery complete");
        
    } catch (const std::exception& e) {
        LOG_ERROR("Recovery failed with exception: " + std::string(e.what()));
        status = RecoveryStatus::FAILED;
    }
    
    is_running_ = false;
    return status;
}

void RecoveryEngine::stopRecovery() {
    if (is_running_) {
        LOG_INFO("Stopping recovery...");
        should_stop_ = true;
        
        // Wait for worker threads to finish
        for (auto& thread : worker_threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        worker_threads_.clear();
        
        is_running_ = false;
        LOG_INFO("Recovery stopped");
    }
}

double RecoveryEngine::getProgress() const {
    return current_progress_;
}

void RecoveryEngine::addFileCarver(std::unique_ptr<FileCarver> carver) {
    file_carvers_.push_back(std::move(carver));
}

void RecoveryEngine::addFilesystemParser(std::unique_ptr<FilesystemParser> parser) {
    filesystem_parsers_.push_back(std::move(parser));
}

void RecoveryEngine::setProgressCallback(std::function<void(double, const std::string&)> callback) {
    progress_callback_ = callback;
}

void RecoveryEngine::initializeDefaultModules() {
    // Add default file carvers
    file_carvers_.push_back(std::make_unique<JpegCarver>());
    file_carvers_.push_back(std::make_unique<PdfCarver>());
    file_carvers_.push_back(std::make_unique<PngCarver>());
    file_carvers_.push_back(std::make_unique<ZipCarver>());
    
    // Add default filesystem parsers
    filesystem_parsers_.push_back(std::make_unique<Ext4Parser>());
    filesystem_parsers_.push_back(std::make_unique<NtfsParser>());
    filesystem_parsers_.push_back(std::make_unique<Fat32Parser>());
}

std::vector<RecoveredFile> RecoveryEngine::performMetadataRecovery() {
    std::vector<RecoveredFile> recovered;
    
    LOG_INFO("Starting metadata-based recovery");
    
    // Detect filesystem type
    FileSystemDetector detector;
    std::vector<Byte> buffer(8192);
    auto bytes_read = disk_scanner_->readChunk(0, 8192, buffer.data()); // Read first 8KB for filesystem detection
    
    if (bytes_read == 0) {
        LOG_ERROR("Failed to read data for filesystem detection");
        return recovered;
    }
    
    auto fs_info = detector.detect_from_data(buffer.data(), bytes_read);
    
    if (!fs_info.is_valid) {
        LOG_WARNING("Could not detect filesystem type");
        return recovered;
    }
    
    LOG_INFO("Detected filesystem: " + fs_info.name);
    
    // Find appropriate parser
    FilesystemParser* parser = nullptr;
    for (auto& p : filesystem_parsers_) {
        if (p->getFileSystemType() == fs_info.type) {
            parser = p.get();
            break;
        }
    }
    
    if (!parser) {
        LOG_WARNING("No parser available for filesystem: " + fs_info.name);
        return recovered;
    }
    
    // Initialize parser with data
    std::vector<Byte> partition_data(std::min(disk_scanner_->getDeviceSize(), static_cast<Size>(100 * 1024 * 1024)));
    auto partition_bytes_read = disk_scanner_->readChunk(0, partition_data.size(), partition_data.data());
    
    if (partition_bytes_read == 0) {
        LOG_ERROR("Failed to read partition data");
        return recovered;
    }
    
    if (!parser->initialize(partition_data.data(), partition_bytes_read)) {
        LOG_ERROR("Failed to initialize filesystem parser");
        return recovered;
    }
    
    // Parse filesystem metadata
    auto file_entries = parser->recoverDeletedFiles();
    
    LOG_INFO("Found " + std::to_string(file_entries.size()) + " files in filesystem metadata");
    
    // The file_entries are already RecoveredFile objects from the parser
    recovered = file_entries;
    
    return recovered;
}

std::vector<RecoveredFile> RecoveryEngine::performSignatureRecovery() {
    std::vector<RecoveredFile> recovered;
    
    Size device_size = disk_scanner_->getDeviceSize();
    Size chunk_size = config_.chunk_size;
    size_t num_threads = config_.num_threads > 0 ? config_.num_threads : getOptimalThreadCount();
    
    LOG_INFO("Starting signature-based recovery with " + std::to_string(num_threads) + 
             " threads, chunk size: " + std::to_string(chunk_size));
    
    // Calculate number of chunks
    size_t num_chunks = (device_size + chunk_size - 1) / chunk_size;
    std::atomic<size_t> completed_chunks(0);
    
    // Process chunks in parallel
    std::vector<std::future<std::vector<RecoveredFile>>> futures;
    
    for (size_t i = 0; i < num_chunks && !should_stop_; ++i) {
        Offset chunk_start = i * chunk_size;
        Size current_chunk_size = std::min(chunk_size, device_size - chunk_start);
        
        // Launch async task for this chunk
        futures.push_back(std::async(std::launch::async, [this, chunk_start, current_chunk_size, &completed_chunks, num_chunks]() {
            std::vector<RecoveredFile> chunk_results;
            
            // Read chunk data
            std::vector<Byte> chunk_data(current_chunk_size);
            Size bytes_read = disk_scanner_->readChunk(chunk_start, current_chunk_size, chunk_data.data());
            
            if (bytes_read > 0) {
                // Apply all carvers to this chunk
                for (const auto& carver : file_carvers_) {
                    if (should_stop_) break;
                    
                    auto files = carver->carveFiles(chunk_data.data(), bytes_read, chunk_start);
                    chunk_results.insert(chunk_results.end(), files.begin(), files.end());
                }
            }
            
            // Update progress
            size_t completed = ++completed_chunks;
            double progress = 35.0 + (45.0 * completed / num_chunks);
            updateProgress(progress, "Scanning chunk " + std::to_string(completed) + "/" + std::to_string(num_chunks));
            
            return chunk_results;
        }));
        
        // Limit number of concurrent tasks
        if (futures.size() >= num_threads) {
            // Wait for oldest task to complete
            auto chunk_results = futures.front().get();
            futures.erase(futures.begin());
            
            // Add results
            recovered.insert(recovered.end(), chunk_results.begin(), chunk_results.end());
        }
    }
    
    // Wait for remaining tasks
    for (auto& future : futures) {
        auto chunk_results = future.get();
        recovered.insert(recovered.end(), chunk_results.begin(), chunk_results.end());
    }
    
    LOG_INFO("Signature recovery found " + std::to_string(recovered.size()) + " potential files");
    return recovered;
}

bool RecoveryEngine::saveRecoveredFile(const RecoveredFile& file) {
    try {
        std::filesystem::path output_path = std::filesystem::path(config_.output_directory) / file.filename;
        
        // Read file data from disk
        std::vector<Byte> file_data(file.file_size);
        Size bytes_read = disk_scanner_->readChunk(file.start_offset, file.file_size, file_data.data());
        
        if (bytes_read != file.file_size) {
            LOG_WARNING("Could not read complete file: " + file.filename);
            return false;
        }
        
        // Write to output file
        std::ofstream output(output_path, std::ios::binary);
        if (!output) {
            LOG_ERROR("Failed to create output file: " + output_path.string());
            return false;
        }
        
        output.write(reinterpret_cast<const char*>(file_data.data()), file_data.size());
        output.close();
        
        if (config_.verbose_logging) {
            LOG_INFO("Saved: " + file.filename + " (" + std::to_string(file.file_size) + " bytes, confidence: " + 
                    std::to_string(file.confidence_score) + ")");
        }
        
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to save file " + file.filename + ": " + std::string(e.what()));
        return false;
    }
}

void RecoveryEngine::deduplicateFiles() {
    // Sort by start offset and remove duplicates with same offset/size
    std::sort(recovered_files_.begin(), recovered_files_.end(), 
              [](const RecoveredFile& a, const RecoveredFile& b) {
                  if (a.start_offset != b.start_offset) {
                      return a.start_offset < b.start_offset;
                  }
                  return a.file_size < b.file_size;
              });
    
    auto it = std::unique(recovered_files_.begin(), recovered_files_.end(),
                         [](const RecoveredFile& a, const RecoveredFile& b) {
                             return a.start_offset == b.start_offset && a.file_size == b.file_size;
                         });
    
    size_t original_count = recovered_files_.size();
    recovered_files_.erase(it, recovered_files_.end());
    
    if (original_count != recovered_files_.size()) {
        LOG_INFO("Removed " + std::to_string(original_count - recovered_files_.size()) + 
                " duplicate files");
    }
}

void RecoveryEngine::updateProgress(double progress, const std::string& status_message) {
    current_progress_ = progress;
    
    if (progress_callback_) {
        progress_callback_(progress, status_message);
    }
    
    if (config_.verbose_logging) {
        LOG_INFO("Progress: " + std::to_string(progress) + "% - " + status_message);
    }
}

size_t RecoveryEngine::getOptimalThreadCount() const {
    size_t hardware_threads = std::thread::hardware_concurrency();
    if (hardware_threads == 0) {
        hardware_threads = 4; // Fallback
    }
    
    // Use fewer threads than available cores to avoid overwhelming the system
    return std::max(1u, static_cast<unsigned int>(hardware_threads - 1));
}

} // namespace FileRecovery
