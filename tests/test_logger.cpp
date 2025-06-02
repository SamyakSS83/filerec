#include <gtest/gtest.h>
#include "utils/logger.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

using namespace FileRecovery;

class LoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_log_file_ = "test_logger.log";
        // Clean up any existing log file
        std::filesystem::remove(test_log_file_);
    }
    
    void TearDown() override {
        // Clean up test log file
        std::filesystem::remove(test_log_file_);
    }
    
    std::string readLogFile() {
        std::ifstream file(test_log_file_);
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }
    
    std::string test_log_file_;
};

TEST_F(LoggerTest, InitializationAndBasicLogging) {
    // Test logger initialization
    Logger& logger = Logger::getInstance();
    logger.initialize(test_log_file_, Logger::Level::INFO);
    
    // Test basic logging
    LOG_INFO("Test info message");
    LOG_WARNING("Test warning message");
    LOG_ERROR("Test error message");
    
    // Since there's no flush method, give the logger a moment to write to disk
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Read log file and verify contents
    std::string log_contents = readLogFile();
    EXPECT_TRUE(log_contents.find("Test info message") != std::string::npos);
    EXPECT_TRUE(log_contents.find("Test warning message") != std::string::npos);
    EXPECT_TRUE(log_contents.find("Test error message") != std::string::npos);
}

TEST_F(LoggerTest, LogLevels) {
    Logger& logger = Logger::getInstance();
    
    // Test with DEBUG level
    logger.initialize(test_log_file_, Logger::Level::DEBUG);
    LOG_DEBUG("Debug message");
    LOG_INFO("Info message");
    LOG_WARNING("Warning message");
    LOG_ERROR("Error message");
    LOG_CRITICAL("Critical message");
    
    // Allow time for log to be written
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    std::string log_contents = readLogFile();
    
    EXPECT_TRUE(log_contents.find("Debug message") != std::string::npos);
    EXPECT_TRUE(log_contents.find("Info message") != std::string::npos);
    EXPECT_TRUE(log_contents.find("Warning message") != std::string::npos);
    EXPECT_TRUE(log_contents.find("Error message") != std::string::npos);
    EXPECT_TRUE(log_contents.find("Critical message") != std::string::npos);
    
    // Clear log file
    std::filesystem::remove(test_log_file_);
    
    // Test with ERROR level - should only show ERROR and CRITICAL
    logger.initialize(test_log_file_, Logger::Level::ERROR);
    LOG_DEBUG("Debug message");
    LOG_INFO("Info message");
    LOG_WARNING("Warning message");
    LOG_ERROR("Error message");
    LOG_CRITICAL("Critical message");
    
    // Allow time for log to be written
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    log_contents = readLogFile();
    
    EXPECT_TRUE(log_contents.find("Debug message") == std::string::npos);
    EXPECT_TRUE(log_contents.find("Info message") == std::string::npos);
    EXPECT_TRUE(log_contents.find("Warning message") == std::string::npos);
    EXPECT_TRUE(log_contents.find("Error message") != std::string::npos);
    EXPECT_TRUE(log_contents.find("Critical message") != std::string::npos);
}

TEST_F(LoggerTest, ThreadSafety) {
    Logger& logger = Logger::getInstance();
    logger.initialize(test_log_file_, Logger::Level::INFO);
    
    // Create multiple threads that log simultaneously
    std::vector<std::thread> threads;
    const int num_threads = 10;
    const int messages_per_thread = 100;
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([i, messages_per_thread]() {
            for (int j = 0; j < messages_per_thread; ++j) {
                LOG_INFO("Thread " + std::to_string(i) + " message " + std::to_string(j));
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Allow time for all logs to be written
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Verify all messages were logged
    std::string log_contents = readLogFile();
    
    // Count the number of log entries
    size_t count = 0;
    size_t pos = 0;
    while ((pos = log_contents.find("Thread", pos)) != std::string::npos) {
        count++;
        pos++;
    }
    
    EXPECT_EQ(count, num_threads * messages_per_thread);
}

TEST_F(LoggerTest, ConsoleOutput) {
    Logger& logger = Logger::getInstance();
    logger.initialize(test_log_file_, Logger::Level::INFO);
    
    // Test enabling/disabling console output
    logger.setConsoleOutput(true);
    
    // This test mainly verifies that the function calls don't crash
    // In a real environment, you'd capture stdout/stderr to verify output
    LOG_INFO("Console output test");
    
    logger.setConsoleOutput(false);
    LOG_INFO("Console output disabled test");
    
    // Allow time for logs to be written
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Verify file logging still works
    std::string log_contents = readLogFile();
    EXPECT_TRUE(log_contents.find("Console output test") != std::string::npos);
    EXPECT_TRUE(log_contents.find("Console output disabled test") != std::string::npos);
}

TEST_F(LoggerTest, SingletonBehavior) {
    // Test that Logger is a singleton
    Logger& logger1 = Logger::getInstance();
    Logger& logger2 = Logger::getInstance();
    
    EXPECT_EQ(&logger1, &logger2);
}

TEST_F(LoggerTest, LogRotation) {
    // This would test log rotation if implemented
    // For now, just verify the interface exists
    Logger& logger = Logger::getInstance();
    logger.initialize(test_log_file_, Logger::Level::INFO);
    
    // Log a large amount of data
    for (int i = 0; i < 1000; ++i) {
        LOG_INFO("Large log test message " + std::to_string(i) + 
                 " with some additional content to make it longer");
    }
    
    // Allow time for logs to be written
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Verify the log file exists and has content
    EXPECT_TRUE(std::filesystem::exists(test_log_file_));
    EXPECT_GT(std::filesystem::file_size(test_log_file_), 0);
}
