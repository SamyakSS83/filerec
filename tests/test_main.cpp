#include <gtest/gtest.h>
#include <cstdlib>
#include <filesystem>

// Global test setup
class FileRecoveryTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        // Set up test environment
        const char* test_data_dir = std::getenv("TEST_DATA_DIR");
        if (test_data_dir) {
            test_data_path_ = test_data_dir;
        } else {
            test_data_path_ = "./test_data";
        }
        
        // Ensure test data directory exists
        std::filesystem::create_directories(test_data_path_);
        
        // Set environment variable for other tests
        std::string env_var = "TEST_DATA_DIR=" + test_data_path_;
        putenv(const_cast<char*>(env_var.c_str()));
    }
    
    void TearDown() override {
        // Clean up any temporary test files
    }
    
    static std::string getTestDataPath() {
        return test_data_path_;
    }
    
private:
    static std::string test_data_path_;
};

std::string FileRecoveryTestEnvironment::test_data_path_;

int main(int argc, char** argv) {
    // Initialize Google Test
    ::testing::InitGoogleTest(&argc, argv);
    
    // Add global test environment
    ::testing::AddGlobalTestEnvironment(new FileRecoveryTestEnvironment);
    
    // Run all tests
    return RUN_ALL_TESTS();
}
