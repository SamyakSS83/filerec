# Unit Tests for File Recovery Tool

# Find or fetch Google Test
find_package(GTest QUIET)
if(NOT GTest_FOUND)
    # Try to find gtest manually
    find_path(GTEST_INCLUDE_DIRS gtest/gtest.h PATHS /usr/include /usr/local/include)
    find_library(GTEST_LIBRARIES NAMES gtest PATHS /usr/lib /usr/local/lib)
    find_library(GTEST_MAIN_LIBRARIES NAMES gtest_main PATHS /usr/lib /usr/local/lib)
    
    if(GTEST_INCLUDE_DIRS AND GTEST_LIBRARIES AND GTEST_MAIN_LIBRARIES)
        message(STATUS "Found GTest manually")
        add_library(gtest STATIC IMPORTED)
        set_target_properties(gtest PROPERTIES IMPORTED_LOCATION ${GTEST_LIBRARIES})
        add_library(gtest_main STATIC IMPORTED)
        set_target_properties(gtest_main PROPERTIES IMPORTED_LOCATION ${GTEST_MAIN_LIBRARIES})
        include_directories(${GTEST_INCLUDE_DIRS})
    else()
        include(FetchContent)
        FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/v1.14.0.zip
        )
        # For Windows: Prevent overriding the parent project's compiler/linker settings
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    endif()
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Test sources
set(TEST_SOURCES
    # Core tests
    test_disk_scanner.cpp
    test_recovery_engine_fixed.cpp  # Using the fixed version
    test_file_system_detector.cpp
    
    # Filesystem parser tests
    test_ext4_parser.cpp
    test_ntfs_parser.cpp
    test_fat32_parser.cpp
    
    # File carver tests
    test_jpeg_carver.cpp
    test_pdf_carver.cpp
    test_png_carver.cpp
    test_zip_carver.cpp
    test_carver_integration.cpp
    test_carver_performance.cpp
    
    # Utility tests
    test_logger.cpp
    
    # Main test runner
    test_main.cpp
)

# Create test executable
add_executable(FileRecoveryTests ${TEST_SOURCES})

# Link libraries
target_link_libraries(FileRecoveryTests
    PRIVATE
    gtest
    gtest_main
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
    ${CMAKE_DL_LIBS}
)

# Link OpenMP if available
if(OpenMP_CXX_FOUND)
    target_link_libraries(FileRecoveryTests PUBLIC OpenMP::OpenMP_CXX)
endif()

# Add sources from main project (excluding main.cpp)
target_sources(FileRecoveryTests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core/disk_scanner.cpp
    ${CMAKE_SOURCE_DIR}/src/core/recovery_engine.cpp
    ${CMAKE_SOURCE_DIR}/src/core/file_system_detector.cpp
    ${CMAKE_SOURCE_DIR}/src/filesystems/ext4_parser.cpp
    ${CMAKE_SOURCE_DIR}/src/filesystems/ntfs_parser.cpp
    ${CMAKE_SOURCE_DIR}/src/filesystems/fat32_parser.cpp
    ${CMAKE_SOURCE_DIR}/src/carvers/jpeg_carver.cpp
    ${CMAKE_SOURCE_DIR}/src/carvers/png_carver.cpp
    ${CMAKE_SOURCE_DIR}/src/carvers/pdf_carver.cpp
    ${CMAKE_SOURCE_DIR}/src/carvers/zip_carver.cpp
    ${CMAKE_SOURCE_DIR}/src/carvers/base_carver.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/logger.cpp
)

# Discover tests
include(GoogleTest)
gtest_discover_tests(FileRecoveryTests)

# Add individual test targets
add_test(NAME AllTests COMMAND FileRecoveryTests)

# Create test data directory
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/test_data)

# Copy test files
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/test_files/ 
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/test_data/)

# Set up environment for tests
set_tests_properties(AllTests PROPERTIES
    ENVIRONMENT "TEST_DATA_DIR=${CMAKE_CURRENT_BINARY_DIR}/test_data"
)
