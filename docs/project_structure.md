# Project Structure and Components

## Overview

The FileRec tool is organized into several logical components, each with specific responsibilities. This document outlines the project structure and explains the purpose and interactions of the key components.

## Directory Structure

```
filerec/
├── include/                 # Header files
│   ├── carvers/             # File carver interfaces and implementations
│   ├── core/                # Core system components
│   ├── filesystems/         # Filesystem parser interfaces
│   ├── interfaces/          # Common interfaces
│   └── utils/               # Utility classes and functions
├── src/                     # Implementation files
│   ├── carvers/             # File carver implementations
│   ├── core/                # Core system implementations
│   ├── filesystems/         # Filesystem parser implementations
│   └── utils/               # Utility implementations
├── tests/                   # Test suites
│   └── test_files/          # Test file resources
├── docs/                    # Documentation
│   ├── project_status.md    # Current implementation status
│   ├── recovery_status.md   # Status of recovery methods
│   ├── test_results.md      # Expected test results
│   └── ...                  # Other documentation files
├── scripts/                 # Testing and utility scripts
│   ├── blackbox_test.sh     # Full blackbox testing script
│   ├── blackbox_test_simple.sh  # Simple blackbox testing script
│   └── test.sh              # Simple signature test script
└── build/                   # Build output (generated)
```

## Core Components

### Disk Scanner

The `DiskScanner` component (`core/disk_scanner.h`) is responsible for:
- Reading data from physical disks or disk image files
- Managing memory mapping for efficient access
- Providing chunk-based access for large disks
- Handling I/O errors gracefully

### Recovery Engine

The `RecoveryEngine` component (`core/recovery_engine.h`) orchestrates the recovery process:
- Initializes scanning of the disk
- Coordinates between filesystem parsers and file carvers
- Manages the recovery workflow
- Handles progress tracking and reporting
- Dispatches carved files to the output directory

### File System Detector

The `FileSystemDetector` component (`core/file_system_detector.h`):
- Identifies filesystem types (Ext4, NTFS, FAT32)
- Extracts filesystem parameters
- Selects appropriate filesystem parser

## File Carvers

All file carvers implement the `FileCarver` interface (`interfaces/file_carver.h`):

### Base Carver

The `BaseCarver` (`carvers/base_carver.h`) provides:
- Common pattern matching algorithms
- Entropy calculation
- Basic validation methods
- Filename generation utilities

### Specialized Carvers

1. **JPEG Carver** (`carvers/jpeg_carver.h`)
   - Handles JPEG image recovery
   - Validates JPEG segment structure

2. **PDF Carver** (`carvers/pdf_carver.h`)
   - Handles PDF document recovery
   - Validates PDF structure and xref tables

3. **PNG Carver** (`carvers/png_carver.h`)
   - Handles PNG image recovery
   - Validates PNG chunks and CRCs

4. **ZIP Carver** (`carvers/zip_carver.h`)
   - Handles ZIP archives and derived formats (DOCX, XLSX, APK)
   - Validates ZIP structure and boundaries

## Filesystem Parsers

All filesystem parsers implement the `FilesystemParser` interface:

1. **Ext4 Parser** (`filesystems/ext4_parser.h`)
   - Handles Ext4 filesystem parsing
   - Recovers deleted inodes

2. **NTFS Parser** (`filesystems/ntfs_parser.h`)
   - Handles NTFS filesystem parsing
   - Processes MFT records

3. **FAT32 Parser** (`filesystems/fat32_parser.h`)
   - Handles FAT32 filesystem parsing
   - Processes directory entries

## Utility Components

1. **Logger** (`utils/logger.h`)
   - Thread-safe logging system
   - Multiple log levels
   - File and console output

2. **Progress Tracker** (`utils/progress_tracker.h`)
   - Tracks and reports recovery progress
   - Supports callbacks for UI updates

3. **File Utils** (`utils/file_utils.h`)
   - File I/O helpers
   - Utility functions for file operations

## Component Interactions

1. **Recovery Process Flow**:
   - `RecoveryEngine` initializes the `DiskScanner`
   - `FileSystemDetector` identifies filesystem type
   - Appropriate `FilesystemParser` is selected if filesystem is intact
   - If filesystem is damaged, `FileCarver` instances are used
   - Recovered files are saved to the output directory

2. **Carving Process Flow**:
   - `RecoveryEngine` provides data chunks to carvers
   - Each `FileCarver` searches for file signatures
   - When a signature is found, the carver extracts and validates the file
   - Validated files are returned to the `RecoveryEngine`

3. **Filesystem Parsing Flow**:
   - `FilesystemParser` reads filesystem structures
   - File metadata is extracted from intact structures
   - Deleted files are identified through filesystem-specific mechanisms
   - File data is reconstructed and returned to the `RecoveryEngine`

## Extension Points

The modular architecture allows for easy extension:

1. Add new file carvers by implementing the `FileCarver` interface
2. Add new filesystem parsers by implementing the `FilesystemParser` interface
3. Enhance or replace component implementations without affecting other parts
