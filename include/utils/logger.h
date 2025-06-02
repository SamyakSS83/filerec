#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <memory>

namespace FileRecovery {

/**
 * @brief Thread-safe logging utility
 */
class Logger {
public:
    enum class Level {
        DEBUG = 0,
        INFO = 1,
        WARNING = 2,
        ERROR = 3,
        CRITICAL = 4
    };
    
    /**
     * @brief Get the singleton logger instance
     * @return Reference to the logger
     */
    static Logger& getInstance();
    
    /**
     * @brief Initialize the logger with log file
     * @param log_file Path to the log file
     * @param min_level Minimum log level to record
     */
    void initialize(const std::string& log_file, Level min_level = Level::INFO);
    
    /**
     * @brief Log a message
     * @param level Log level
     * @param message Message to log
     */
    void log(Level level, const std::string& message);
    
    /**
     * @brief Log debug message
     * @param message Message to log
     */
    void debug(const std::string& message) { log(Level::DEBUG, message); }
    
    /**
     * @brief Log info message
     * @param message Message to log
     */
    void info(const std::string& message) { log(Level::INFO, message); }
    
    /**
     * @brief Log warning message
     * @param message Message to log
     */
    void warning(const std::string& message) { log(Level::WARNING, message); }
    
    /**
     * @brief Log error message
     * @param message Message to log
     */
    void error(const std::string& message) { log(Level::ERROR, message); }
    
    /**
     * @brief Log critical message
     * @param message Message to log
     */
    void critical(const std::string& message) { log(Level::CRITICAL, message); }
    
    /**
     * @brief Set minimum log level
     * @param level Minimum level to log
     */
    void setLevel(Level level) { min_level_ = level; }
    
    /**
     * @brief Enable/disable console output
     * @param enable Whether to output to console
     */
    void setConsoleOutput(bool enable) { console_output_ = enable; }
    
private:
    Logger() = default;
    ~Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    std::mutex log_mutex_;
    std::unique_ptr<std::ofstream> log_file_;
    Level min_level_ = Level::INFO;
    bool console_output_ = true;
    
    /**
     * @brief Convert log level to string
     * @param level Log level
     * @return String representation
     */
    std::string levelToString(Level level) const;
    
    /**
     * @brief Get current timestamp string
     * @return Formatted timestamp
     */
    std::string getCurrentTimestamp() const;
};

// Convenience macros
#define LOG_DEBUG(msg) FileRecovery::Logger::getInstance().debug(msg)
#define LOG_INFO(msg) FileRecovery::Logger::getInstance().info(msg)
#define LOG_WARNING(msg) FileRecovery::Logger::getInstance().warning(msg)
#define LOG_ERROR(msg) FileRecovery::Logger::getInstance().error(msg)
#define LOG_CRITICAL(msg) FileRecovery::Logger::getInstance().critical(msg)

} // namespace FileRecovery
