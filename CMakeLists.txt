cmake_minimum_required(VERSION 3.16)
project(FileRecoveryTool VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler-specific options
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O3 -march=native")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -DDEBUG")
endif()

# Find required packages
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(OpenSSL REQUIRED)

# Check for optional dependencies
find_package(OpenMP)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Source files
set(SOURCES
    src/main.cpp
    src/core/disk_scanner.cpp
    src/core/recovery_engine.cpp
    src/core/file_system_detector.cpp
    src/filesystems/ext4_parser.cpp
    src/filesystems/ntfs_parser.cpp
    src/filesystems/fat32_parser.cpp
    src/carvers/jpeg_carver.cpp
    src/carvers/png_carver.cpp
    src/carvers/pdf_carver.cpp
    src/carvers/zip_carver.cpp
    src/carvers/base_carver.cpp
    src/utils/logger.cpp
    src/utils/file_utils.cpp
    src/utils/progress_tracker.cpp
)

# Header files
set(HEADERS
    include/core/disk_scanner.h
    include/core/recovery_engine.h
    include/core/file_system_detector.h
    include/interfaces/filesystem_parser.h
    include/interfaces/file_carver.h
    include/filesystems/ext4_parser.h
    include/filesystems/ntfs_parser.h
    include/filesystems/fat32_parser.h
    include/carvers/jpeg_carver.h
    include/carvers/png_carver.h
    include/carvers/pdf_carver.h
    include/carvers/zip_carver.h
    include/carvers/base_carver.h
    include/utils/logger.h
    include/utils/file_utils.h
    include/utils/progress_tracker.h
    include/utils/types.h
)

# Create executable
add_executable(FileRecoveryTool ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(FileRecoveryTool 
    PRIVATE 
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
    ${CMAKE_DL_LIBS}
)

# Link OpenMP if available
if(OpenMP_CXX_FOUND)
    target_link_libraries(FileRecoveryTool PUBLIC OpenMP::OpenMP_CXX)
    add_compile_definitions(USE_OPENMP)
endif()

# Install target
install(TARGETS FileRecoveryTool DESTINATION bin)

# Enable testing
enable_testing()

# Add tests subdirectory
add_subdirectory(tests)

# Documentation
find_package(Doxygen)
if(DOXYGEN_FOUND)
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
    
    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
    
    add_custom_target(docs ALL
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM)
endif()
