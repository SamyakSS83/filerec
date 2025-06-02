# FileRec - Documentation Index

## Project Status Documents

- [Project Status](project_status.md) - Current implementation status of all components
- [Recovery Status](recovery_status.md) - Status of different recovery methods
- [Test Results](test_results.md) - What to expect when running tests

## User Documentation

- [README](../README.md) - Main project overview
- [Blackbox Testing Guide](blackbox_testing.md) - How to run blackbox tests

## Developer Documentation

- [Developer Guide](developer_guide.md) - Guide for contributors
- [Project Structure](project_structure.md) - Code organization
- [Test Summary](test_summary.md) - Overview of the test suite
- [Carving Architecture](carving_architecture.md) - Details on the carving system

## Component Documentation

- [ZIP Carver](zip_carver.md) - Details on the ZIP file carver implementation

## ToDo List (High Priority)

1. **Implement Filesystem Metadata Recovery**
   - Complete EXT4 deleted inode recovery
   - Complete NTFS deleted MFT entry recovery
   - Complete FAT32 deleted directory entry recovery

2. **Improve Test Coverage**
   - Add tests for deleted file recovery once implemented
   - Add more filesystem corruption scenarios

3. **Enhance Carver Capabilities**
   - Add more file types (DOC, DOCX, XLS, etc.)
   - Improve fragmented file handling

## Recommended Development Workflow

1. Start with the simple blackbox test to understand current functionality
2. Read the component documentation for the area you want to work on
3. Implement changes with appropriate tests
4. Verify changes with both unit tests and blackbox tests
