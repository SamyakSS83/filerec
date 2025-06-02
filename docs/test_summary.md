# FileRec - Test Suite Summary

This document provides an overview of the test suite for FileRec, including the purpose and status of each test file.

## Test Directory Organization

```
tests/
├── CMakeLists.txt              # Test build configuration
├── test_main.cpp               # Main test runner
├── test_disk_scanner.cpp       # Tests for DiskScanner
├── test_file_system_detector.cpp # Tests for FileSystemDetector
├── test_recovery_engine_fixed.cpp # Tests for RecoveryEngine
├── test_ext4_parser.cpp        # Tests for Ext4Parser
├── test_ntfs_parser.cpp        # Tests for NtfsParser
├── test_fat32_parser.cpp       # Tests for Fat32Parser
├── test_jpeg_carver.cpp        # Tests for JpegCarver
├── test_pdf_carver.cpp         # Tests for PdfCarver
├── test_png_carver.cpp         # Tests for PngCarver
├── test_zip_carver.cpp         # Tests for ZipCarver
├── test_logger.cpp             # Tests for Logger
├── test_carver_integration.cpp # Integration tests for carvers
├── test_carver_performance.cpp # Performance tests for carvers
└── test_files/                 # Test files directory
    ├── normal.jpeg             # Test JPEG file
    ├── ok.pdf                  # Test PDF file
    └── lele.png                # Test PNG file
```

## Test Status Summary

| Test File                     | Purpose                                    | Status                |
|-------------------------------|--------------------------------------------|-----------------------|
| test_disk_scanner.cpp         | Tests disk image/device reading            | ✅ Working             |
| test_file_system_detector.cpp | Tests filesystem type detection            | ✅ Working             |
| test_recovery_engine_fixed.cpp| Tests recovery workflow                    | ✅ Working             |
| test_ext4_parser.cpp          | Tests EXT4 filesystem parsing              | ⚠️ Partial (detection) |
| test_ntfs_parser.cpp          | Tests NTFS filesystem parsing              | ⚠️ Partial (detection) |
| test_fat32_parser.cpp         | Tests FAT32 filesystem parsing             | ⚠️ Partial (detection) |
| test_jpeg_carver.cpp          | Tests JPEG signature detection             | ✅ Working             |
| test_pdf_carver.cpp           | Tests PDF signature detection              | ✅ Working             |
| test_png_carver.cpp           | Tests PNG signature detection              | ✅ Working             |
| test_zip_carver.cpp           | Tests ZIP signature detection              | ✅ Working             |
| test_logger.cpp               | Tests logging functionality                | ✅ Working             |
| test_carver_integration.cpp   | Tests multiple carvers working together    | ✅ Working             |
| test_carver_performance.cpp   | Tests carver performance with large files  | ✅ Working             |

## Test Categories

### 1. Unit Tests

Tests for individual components that verify their behavior in isolation. These tests use mock objects to simulate dependencies and focus on specific component functionality.

**Files**: All test_*.cpp files except integration and performance tests.

### 2. Integration Tests

Tests that verify multiple components working together correctly.

**Files**: test_carver_integration.cpp

### 3. Performance Tests

Tests that measure the performance of components under various conditions and data sizes.

**Files**: test_carver_performance.cpp

### 4. Blackbox Tests

Scripts that test the entire system end-to-end by creating real data and attempting recovery.

**Files**: 
- scripts/blackbox_test.sh
- scripts/blackbox_test_simple.sh

## Running Tests

Tests can be run using CMake's test runner after building:

```bash
# Build tests
cd build
cmake .. -DBUILD_TESTING=ON
make

# Run all tests
ctest

# Run a specific test
ctest -R test_jpeg_carver
```

## Test Limitations and Known Issues

1. **Filesystem Parser Tests**: Currently, these tests only verify that the parsers can detect the filesystem type and read basic structures. They do not test recovery of deleted files, as this functionality is still under development.

2. **Recovery Engine Tests**: These tests verify that the recovery engine correctly orchestrates the recovery process but do not validate actual recovery results from real devices.

3. **API Alignment**: Some tests were initially written with expected APIs that differ from the actual implementation. These have been fixed or are in the process of being fixed.

## Future Test Improvements

1. Add comprehensive tests for deleted file recovery once implemented
2. Add more edge cases for filesystem corruption scenarios
3. Create automated performance benchmarking tests
4. Expand test file types and scenarios
