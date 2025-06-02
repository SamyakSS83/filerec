# FileRec - Advanced File Recovery Tool

FileRec is a high-performance forensic file carving tool designed to recover deleted or lost files from disk images. It uses signature-based detection and structure validation to recover various file types even when filesystem metadata is damaged or unavailable.

## Features

- **Multiple File Type Support**: Recovers JPEG, PNG, PDF, and ZIP/Archive files
- **Signature-based Detection**: Uses file signatures (magic numbers) to identify file types
- **Structure Validation**: Validates recovered files by analyzing their internal structure
- **File System Awareness**: Can work with Ext4, NTFS, and FAT32 file systems
- **Confidence Scoring**: Provides confidence scores for recovered files
- **High Performance**: Optimized for fast scanning of large disk images
- **Overlapping File Detection**: Correctly handles adjacent files and overlapping signatures

## Building from Source

### Prerequisites

- C++17 compatible compiler (GCC 7+ or Clang 5+)
- CMake 3.16 or higher
- OpenSSL development libraries (libssl-dev)
- Google Test Framework (libgtest-dev)
- Doxygen (for documentation)
- (Optional) OpenMP for parallel scanning

#### Installing Dependencies on Ubuntu/Debian:

```bash
sudo apt-get update
sudo apt-get install build-essential cmake libssl-dev libgtest-dev doxygen
```

#### Installing Dependencies on Fedora/RHEL/CentOS:

```bash
sudo dnf install gcc-c++ cmake openssl-devel gtest-devel doxygen
```

#### Installing Dependencies on macOS:

```bash
brew install cmake openssl googletest doxygen
```

### Build Instructions

```bash
# Clone the repository
git clone https://github.com/yourusername/filerec.git
cd filerec

# Create build directory
mkdir -p build && cd build

# Configure and build
cmake ..
make

# Run tests
ctest
```

## Usage

```bash
# Basic usage
./build/FileRecoveryTool [options] <disk_image_path> <output_directory>

# Examples:
./build/FileRecoveryTool /dev/sda1 ./recovered
./build/FileRecoveryTool -v -t 4 -f jpg,pdf disk.img ./output

# Options:
#   -v, --verbose           Enable verbose logging
#   -t, --threads NUM       Number of threads to use (default: auto)
#   -c, --chunk-size SIZE   Chunk size in MB (default: 1)
#   -f, --file-types TYPES  Comma-separated list of file types (default: all)
#   -m, --metadata-only     Use only metadata-based recovery
#   -s, --signature-only    Use only signature-based recovery
#   -l, --log-file FILE     Log file path (default: recovery.log)
#   --read-only             Verify device is mounted read-only (safety check)
#   -h, --help              Show help message
```

## Architecture

FileRec is designed with a modular architecture:

1. **Core Components**:
   - `DiskScanner`: Manages reading data from disk images
   - `RecoveryEngine`: Orchestrates the file recovery process
   - `FileSystemDetector`: Identifies filesystem types

2. **File Carvers**:
   - Each file type has a specialized carver implementing the `FileCarver` interface
   - `BaseCarver`: Common functionality shared by all carvers

3. **Filesystem Parsers**:
   - Parsers for Ext4, NTFS, and FAT32 filesystems
   - Used when filesystem metadata is intact

## Testing

The project includes comprehensive test suites:

- Unit tests for individual components
- Integration tests for carver interactions
- Performance tests for evaluating scanning speed
- Edge case tests for robustness
- Blackbox tests for real-world recovery scenarios

### Running Unit and Integration Tests

Run tests with: `ctest` or `make test` in the build directory.

### Blackbox Testing

FileRec includes a blackbox testing script that tests the tool in real-world scenarios by:

1. Copying sample files to an actual disk partition
2. Creating a zip archive of the files
3. Deleting the files
4. Running FileRec to attempt recovery
5. Verifying recovery results

To run the blackbox test:

```bash
# Requires root privileges to mount/unmount partitions
sudo ./scripts/blackbox_test.sh [options] /dev/sdXY

# Options:
#  -v, --verbose     Show detailed output during testing
#  -q, --quiet       Show minimal output (errors and summary only)
#  -h, --help        Show help message

# Examples:
sudo ./scripts/blackbox_test.sh /dev/sda1
sudo ./scripts/blackbox_test.sh --verbose /dev/sda1
sudo ./scripts/blackbox_test.sh --quiet /dev/sda1
```

> **Warning**: Only run the blackbox test on partitions that do not contain important data. The test will create and delete files on the specified partition.

#### Troubleshooting Blackbox Tests

- **Already Mounted Partition**: If the partition is already mounted, the script will give you an option to use the existing mount point.
- **NTFS Partitions**: If using an NTFS partition, you may need to install ntfs-3g: `sudo apt-get install ntfs-3g`.
- **Permission Issues**: The script needs root privileges to mount/unmount partitions.
- **Verification**: After recovery, the script will attempt to verify the recovered files against the original files.
- **Command Line Usage**: The script runs the tool with the correct arguments: `./build/FileRecoveryTool -v /dev/sdXY /path/to/recovery/dir`

## Development

### Adding a New File Carver

1. Create a new class inheriting from `BaseCarver`
2. Implement required methods:
   - `getSupportedTypes()`
   - `getFileSignatures()`
   - `getFileFooters()`  
   - `carveFiles()`
   - `validateFile()`

### Adding a New Filesystem Parser

1. Create a new class implementing the `FilesystemParser` interface
2. Implement required methods for parsing the filesystem structure

### Extending the Blackbox Tester

The blackbox testing script can be extended to:

1. Test additional file types
2. Simulate different deletion scenarios
3. Test fragmentation recovery
4. Benchmark recovery performance on different media

When extending the blackbox tester, make sure to:
- Add proper error handling
- Include verification steps for recovered files
- Document any new parameters or test scenarios

## License

This project is licensed under the MIT License - see the LICENSE file for details.
