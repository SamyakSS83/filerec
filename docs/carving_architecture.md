# File Carving Architecture

## Overview

The file carving subsystem is responsible for detecting and extracting files from raw data, without relying on filesystem metadata. This document explains the architecture and design principles behind FileRec's carving capabilities.

## Components

### FileCarver Interface

The `FileCarver` interface (`interfaces/file_carver.h`) defines the contract that all file carvers must implement:

```cpp
class FileCarver {
public:
    virtual ~FileCarver() = default;
    
    // Return list of supported file types/extensions
    virtual std::vector<std::string> getSupportedTypes() const = 0;
    
    // Return list of file signatures (magic numbers)
    virtual std::vector<std::vector<Byte>> getFileSignatures() const = 0;
    
    // Return list of file footer signatures (if applicable)
    virtual std::vector<std::vector<Byte>> getFileFooters() const = 0;
    
    // Carve files from raw data
    virtual std::vector<RecoveredFile> carveFiles(
        const Byte* data, Size size, Offset base_offset) = 0;
    
    // Validate a carved file and return confidence score
    virtual double validateFile(const RecoveredFile& file, const Byte* data) = 0;
    
    // Get maximum expected file size (for optimization)
    virtual Size getMaxFileSize() const = 0;
};
```

### BaseCarver

The `BaseCarver` class provides common functionality used by all carvers:

- Pattern matching for signatures
- Entropy calculation
- Validation helpers
- Filename generation
- Confidence score calculation

### Specialized Carvers

Each file type has a specialized carver with type-specific knowledge:

1. **JpegCarver**: 
   - Handles JPEG files with SOI/EOI markers
   - Validates segment structure
   - Identifies embedded thumbnails

2. **PngCarver**:
   - Handles PNG files with chunk-based structure
   - Validates IHDR, IDAT, IEND chunks
   - Checks CRC values for integrity

3. **PdfCarver**:
   - Handles PDF documents 
   - Locates header and %%EOF markers
   - Validates cross-reference table

4. **ZipCarver**:
   - Handles ZIP archives and formats based on ZIP (DOCX, APK, JAR)
   - Validates local file headers and central directory
   - Handles adjacent ZIP files correctly

## Carving Process

The general carving process follows these steps:

1. **Signature Detection**: Identify file signatures in the data buffer
2. **Size Calculation**: Determine the file's size by looking for footers or analyzing structure
3. **Boundary Detection**: Check for adjacent file signatures to avoid overcarving
4. **Structure Validation**: Validate the file's internal structure
5. **Confidence Scoring**: Calculate a confidence score (0.0-1.0) based on structural integrity
6. **Overlap Filtering**: Filter out overlapping files based on confidence and position

## Handling Special Cases

### Adjacent Files

When multiple files are stored adjacently (without gaps), the carvers detect the start of the next file and use it as a boundary for the current file. This prevents overcarving and ensures all files are properly recovered.

### Corrupted Files

When a file is partially corrupted, the carvers attempt to recover as much valid data as possible, and assign a lower confidence score to indicate potential issues.

### Embedded Files

Some formats (like PDFs) may contain embedded files. The current implementation focuses on the container format, but future versions may extract embedded content.

## Extending the Carver System

To add support for a new file type:

1. Create a new class inheriting from `BaseCarver`
2. Define the file signatures and footers
3. Implement size calculation based on the file format
4. Add structure validation specific to the file type
5. Integrate with the `RecoveryEngine`

## Performance Considerations

For optimal performance:

- Avoid excessive memory allocation during carving
- Use efficient pattern matching algorithms
- Implement early rejection of invalid files
- Limit validation depth for large files
- Use the `getMaxFileSize()` method to optimize buffer handling
