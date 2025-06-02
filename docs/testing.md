# Testing Architecture

## Overview

FileRec employs a comprehensive testing strategy to ensure reliability, performance, and correctness. This document describes the testing architecture, test categories, and best practices for writing and maintaining tests.

## Test Categories

### Blackbox Tests

Blackbox tests provide end-to-end validation in real-world scenarios:

- `scripts/blackbox_test.sh` - Tests the complete recovery workflow on actual partitions
- Creates and deletes files on real partitions to simulate data loss scenarios
- Verifies successful recovery of deleted files

### Unit Tests

Unit tests validate individual components in isolation. Each component has dedicated test files:

- `test_disk_scanner.cpp` - Tests for the `DiskScanner` class
- `test_recovery_engine_fixed.cpp` - Tests for the `RecoveryEngine` class
- `test_file_system_detector.cpp` - Tests for the `FileSystemDetector` class
- `test_ext4_parser.cpp` - Tests for the `Ext4Parser` class
- `test_ntfs_parser.cpp` - Tests for the `NtfsParser` class
- `test_fat32_parser.cpp` - Tests for the `Fat32Parser` class
- `test_jpeg_carver.cpp` - Tests for the `JpegCarver` class
- `test_pdf_carver.cpp` - Tests for the `PdfCarver` class
- `test_png_carver.cpp` - Tests for the `PngCarver` class
- `test_zip_carver.cpp` - Tests for the `ZipCarver` class
- `test_logger.cpp` - Tests for the `Logger` class

### Integration Tests

Integration tests validate interactions between components:

- `test_carver_integration.cpp` - Tests file carvers working together
- Tests with mixed file types and adjacent files

### Performance Tests

Performance tests measure execution time and resource usage:

- `test_carver_performance.cpp` - Benchmarks carving operations
- Tests with large data sets (10MB+)

### Edge Case Tests

Edge case tests validate behavior in unusual situations:

- Tests with corrupted files
- Tests with minimal-size files
- Tests with overlapping signatures
- Tests with invalid data

## Test Infrastructure

### Google Test Framework

Tests use the Google Test framework with the following features:

- Test fixtures (`TEST_F`) for shared setup/teardown
- Test parameterization for testing multiple similar cases
- Assertions for validating results
- Death tests for validating error handling

### Test Data Generation

Test data is generated in several ways:

1. **Built-in Test Data**: Small test files created programmatically in test fixtures
2. **Test Files**: Real files included in the `test_files` directory
3. **Generated Data**: Random data with injected file signatures for performance testing

### Continuous Integration

Tests are automatically run in the CI pipeline:

- All unit tests run on every commit
- Integration tests run on merge requests
- Performance tests run on scheduled intervals

## Writing Good Tests

### Guidelines for Unit Tests

1. Test a single unit of functionality
2. Use descriptive test names (e.g., `ValidateZipStructure_WithValidHeader_ReturnsTrue`)
3. Focus on behavior, not implementation details
4. Use appropriate assertions (`EXPECT_EQ`, `ASSERT_TRUE`, etc.)
5. Mock external dependencies
6. Clean up resources in teardown

### Guidelines for Integration Tests

1. Focus on component interactions
2. Test realistic scenarios
3. Validate end-to-end behavior
4. Use minimal dependencies on external systems

### Guidelines for Performance Tests

1. Establish baseline performance
2. Test with realistic data sizes
3. Measure relevant metrics (time, memory usage)
4. Avoid machine-dependent assertions
5. Document performance expectations

### Guidelines for Blackbox Tests

1. Test in realistic environments
2. Document partition requirements and permissions
3. Include verification steps for recovery results
4. Clean up all test data after testing
5. Provide clear documentation for test prerequisites
6. Include safeguards against accidental data loss

## Running Tests

### Unit and Integration Tests

```bash
# In the build directory
ctest                     # Run all tests
ctest -R UnitTest         # Run unit tests
ctest -R IntegrationTest  # Run integration tests
```

### Blackbox Tests

```bash
# From the project root directory
sudo ./scripts/blackbox_test.sh /dev/sdXY  # Replace with actual partition

# Example:
sudo ./scripts/blackbox_test.sh /dev/sda1
```

> **Important**: Only run blackbox tests on partitions without important data, as the test will create and delete files.

Tests can be run using CTest or directly:

```bash
# Run all tests
cd build && ctest

# Run a specific test category
cd build && ctest -R ZipCarverTest

# Run a specific test
cd build && ctest -R ZipCarverTest.CarveMultipleZips

# Run tests with verbose output
cd build && ctest -V

# Run tests with output on failure
cd build && ctest --output-on-failure
```

## Adding New Tests

To add a new test file:

1. Create a new `.cpp` file in the `tests` directory
2. Include appropriate headers
3. Create a test fixture class if needed
4. Add `TEST` or `TEST_F` definitions
5. Add the file to `tests/CMakeLists.txt`

## Test Maintenance

Guidelines for maintaining tests:

1. Update tests when component behavior changes
2. Regularly review test coverage
3. Fix flaky tests promptly
4. Refactor tests to remove duplication
5. Keep test code as clean as production code
