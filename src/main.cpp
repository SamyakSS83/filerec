#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <getopt.h>
#include <signal.h>
#include <atomic>

#include "core/recovery_engine.h"
#include "utils/logger.h"
#include "utils/types.h"

using namespace FileRecovery;

// Global flag for signal handling
std::atomic<bool> g_interrupt_received(false);
RecoveryEngine* g_recovery_engine = nullptr;

void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nInterrupt received, stopping recovery..." << std::endl;
        g_interrupt_received = true;
        if (g_recovery_engine) {
            g_recovery_engine->stopRecovery();
        }
    }
}

void printUsage(const char* program_name) {
    std::cout << "Advanced File Recovery Tool v1.0.0\n\n";
    std::cout << "Usage: " << program_name << " [OPTIONS] DEVICE OUTPUT_DIR\n\n";
    std::cout << "Arguments:\n";
    std::cout << "  DEVICE      Device or image file to scan (e.g., /dev/sda1, disk.img)\n";
    std::cout << "  OUTPUT_DIR  Directory to save recovered files\n\n";
    std::cout << "Options:\n";
    std::cout << "  -h, --help              Show this help message\n";
    std::cout << "  -v, --verbose           Enable verbose logging\n";
    std::cout << "  -t, --threads NUM       Number of threads to use (default: auto)\n";
    std::cout << "  -c, --chunk-size SIZE   Chunk size in MB (default: 1)\n";
    std::cout << "  -f, --file-types TYPES  Comma-separated list of file types (default: all)\n";
    std::cout << "  -m, --metadata-only     Use only metadata-based recovery\n";
    std::cout << "  -s, --signature-only    Use only signature-based recovery\n";
    std::cout << "  -l, --log-file FILE     Log file path (default: recovery.log)\n";
    std::cout << "  --read-only             Verify device is mounted read-only (safety check)\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program_name << " /dev/sda1 ./recovered\n";
    std::cout << "  " << program_name << " -v -t 4 -f jpg,pdf disk.img ./output\n";
    std::cout << "  " << program_name << " --signature-only /dev/sdb1 ./photos\n\n";
    std::cout << "Safety Notes:\n";
    std::cout << "  - Always use read-only access to prevent data corruption\n";
    std::cout << "  - Consider creating a disk image first with: dd if=/dev/sdX of=image.img\n";
    std::cout << "  - Ensure sufficient space in the output directory\n";
}

void printProgress(double progress, const std::string& message) {
    static int last_percent = -1;
    int current_percent = static_cast<int>(progress);
    
    if (current_percent != last_percent || progress >= 100.0) {
        std::cout << "\r[" << std::string(current_percent / 2, '=') 
                  << std::string(50 - current_percent / 2, ' ') << "] " 
                  << current_percent << "% - " << message << std::flush;
        
        if (progress >= 100.0) {
            std::cout << std::endl;
        }
        
        last_percent = current_percent;
    }
}

std::vector<std::string> parseFileTypes(const std::string& types_str) {
    std::vector<std::string> types;
    std::stringstream ss(types_str);
    std::string type;
    
    while (std::getline(ss, type, ',')) {
        // Remove whitespace
        type.erase(0, type.find_first_not_of(" \t"));
        type.erase(type.find_last_not_of(" \t") + 1);
        
        if (!type.empty()) {
            types.push_back(type);
        }
    }
    
    return types;
}

int main(int argc, char* argv[]) {
    // Set up signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // Command line options
    ScanConfig config;
    std::string log_file = "recovery.log";
    bool read_only_check = false;
    
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"verbose", no_argument, 0, 'v'},
        {"threads", required_argument, 0, 't'},
        {"chunk-size", required_argument, 0, 'c'},
        {"file-types", required_argument, 0, 'f'},
        {"metadata-only", no_argument, 0, 'm'},
        {"signature-only", no_argument, 0, 's'},
        {"log-file", required_argument, 0, 'l'},
        {"read-only", no_argument, 0, 'r'},
        {0, 0, 0, 0}
    };
    
    int option_index = 0;
    int c;
    
    while ((c = getopt_long(argc, argv, "hvt:c:f:msl:", long_options, &option_index)) != -1) {
        switch (c) {
            case 'h':
                printUsage(argv[0]);
                return 0;
                
            case 'v':
                config.verbose_logging = true;
                break;
                
            case 't':
                config.num_threads = std::stoul(optarg);
                break;
                
            case 'c':
                config.chunk_size = std::stoul(optarg) * 1024 * 1024; // Convert MB to bytes
                break;
                
            case 'f':
                config.target_file_types = parseFileTypes(optarg);
                break;
                
            case 'm':
                config.use_metadata_recovery = true;
                config.use_signature_recovery = false;
                break;
                
            case 's':
                config.use_metadata_recovery = false;
                config.use_signature_recovery = true;
                break;
                
            case 'l':
                log_file = optarg;
                break;
                
            case 'r':
                read_only_check = true;
                break;
                
            default:
                std::cerr << "Unknown option. Use --help for usage information.\n";
                return 1;
        }
    }
    
    // Check for required arguments
    if (optind + 2 != argc) {
        std::cerr << "Error: Missing required arguments.\n";
        printUsage(argv[0]);
        return 1;
    }
    
    config.device_path = argv[optind];
    config.output_directory = argv[optind + 1];
    
    // Initialize logging
    Logger::getInstance().initialize(log_file, config.verbose_logging ? Logger::Level::DEBUG : Logger::Level::INFO);
    Logger::getInstance().setConsoleOutput(true);
    
    LOG_INFO("Starting Advanced File Recovery Tool");
    LOG_INFO("Device: " + config.device_path);
    LOG_INFO("Output: " + config.output_directory);
    
    // Validate inputs
    if (config.device_path.empty() || config.output_directory.empty()) {
        LOG_ERROR("Device path and output directory must be specified");
        return 1;
    }
    
    // Safety checks
    if (read_only_check) {
        DiskScanner scanner(config.device_path);
        if (scanner.initialize()) {
            if (!scanner.isReadOnly()) {
                LOG_ERROR("Safety check failed: Device is not read-only mounted");
                LOG_ERROR("Please mount the device read-only or use --force to override");
                return 1;
            }
            LOG_INFO("Safety check passed: Device is read-only");
        }
    }
    
    try {
        // Create recovery engine
        RecoveryEngine engine(config);
        g_recovery_engine = &engine;
        
        // Set progress callback
        engine.setProgressCallback(printProgress);
        
        std::cout << "Starting file recovery...\n";
        
        // Start recovery
        RecoveryStatus status = engine.startRecovery();
        
        // Print results
        switch (status) {
            case RecoveryStatus::SUCCESS:
                std::cout << "\nRecovery completed successfully!\n";
                std::cout << "Files recovered: " << engine.getRecoveredFileCount() << "\n";
                std::cout << "Output directory: " << config.output_directory << "\n";
                break;
                
            case RecoveryStatus::PARTIAL_SUCCESS:
                std::cout << "\nRecovery partially completed.\n";
                std::cout << "Files recovered: " << engine.getRecoveredFileCount() << "\n";
                break;
                
            case RecoveryStatus::DEVICE_NOT_FOUND:
                std::cerr << "\nError: Could not access device: " << config.device_path << "\n";
                return 1;
                
            case RecoveryStatus::INSUFFICIENT_SPACE:
                std::cerr << "\nError: Insufficient space in output directory.\n";
                return 1;
                
            case RecoveryStatus::ACCESS_DENIED:
                std::cerr << "\nError: Access denied. Try running with sudo.\n";
                return 1;
                
            case RecoveryStatus::FAILED:
            default:
                std::cerr << "\nRecovery failed. Check log file for details.\n";
                return 1;
        }
        
        if (g_interrupt_received) {
            std::cout << "Recovery was interrupted by user.\n";
            return 130; // Standard exit code for SIGINT
        }
        
    } catch (const std::exception& e) {
        LOG_CRITICAL("Unhandled exception: " + std::string(e.what()));
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    LOG_INFO("File recovery tool finished");
    return 0;
}
