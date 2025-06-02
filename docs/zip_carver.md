# ZIP File Carving

## Overview

The ZIP carver is designed to recover ZIP archives and ZIP-based formats (such as DOCX, XLSX, APK, JAR) from raw data. This document describes the implementation details, structure validation logic, and boundary detection mechanisms used in the ZIP carver.

## ZIP Format Structure

A ZIP archive typically contains:

1. **Local File Headers** - One for each file, containing:
   - Signature (0x50, 0x4B, 0x03, 0x04)
   - Version needed to extract
   - General purpose bit flag
   - Compression method
   - Last modification time/date
   - CRC-32 checksum
   - Compressed and uncompressed sizes
   - Filename length
   - Extra field length
   - Filename
   - Extra field
   - File data

2. **Central Directory** - Contains metadata about all files:
   - Header signature (0x50, 0x4B, 0x01, 0x02)
   - File headers with additional metadata
   - Relative offsets to local file headers

3. **End of Central Directory (EOCD)** - Contains:
   - Signature (0x50, 0x4B, 0x05, 0x06)
   - Number of entries
   - Size and offset of central directory
   - Optional comment

## Carving Strategy

The ZIP carver implements several strategies for reliable file recovery:

### Signature Detection

The carver searches for three possible signatures:
- 0x50, 0x4B, 0x03, 0x04 (Local File Header)
- 0x50, 0x4B, 0x05, 0x06 (End of Central Directory)
- 0x50, 0x4B, 0x07, 0x08 (Spanned Archive)

### Size Calculation

The size is determined using the following strategy:

1. Look for the next ZIP header signature to identify boundaries between adjacent ZIP files
2. Find the End of Central Directory record
3. If EOCD is found, calculate size based on the EOCD structure
4. If no EOCD is found, estimate size by parsing Local File Headers
5. Ensure the calculated size doesn't extend beyond the next ZIP header

### Handling Adjacent ZIPs

A critical feature of the ZIP carver is its ability to detect multiple adjacent ZIP files. This is achieved by:

1. Finding all ZIP signatures in the buffer
2. For each signature, calculating the size while being aware of the next ZIP header
3. Filtering out overlapping candidates
4. Retaining only candidates that don't overlap with previously carved files

This approach ensures that all valid ZIPs are carved, even if they are adjacent with no padding between them.

### Structure Validation

The ZIP carver validates:

1. **Local File Header**:
   - Valid signature
   - Reasonable version (≤63)
   - Known compression method (≤99)
   - Reasonable filename length (≤512)
   - Reasonable extra field length (≤1024)

2. **Central Directory Header**:
   - Valid signature
   - Matching fields with local headers

3. **End of Central Directory**:
   - Valid signature
   - Reasonable entry counts
   - Reasonable comment length

### Confidence Scoring

Confidence scores (0.0-1.0) are assigned based on:

- Presence of a valid Local File Header (+0.2)
- Additional header validation (+0.1)
- Presence of End of Central Directory (+0.3)
- Entropy in the expected range (+0.1)

Lower scores are assigned to potentially corrupted files.

## Implementation Details

### Key Methods

1. `carveFiles(const Byte* data, Size size, Offset base_offset)` - Main entry point for ZIP carving
2. `calculate_zip_size(const uint8_t* data, size_t max_size)` - Determines ZIP file boundaries
3. `find_end_of_central_directory(const uint8_t* data, size_t size)` - Locates the EOCD record
4. `validate_zip_structure(const uint8_t* data, size_t size)` - Validates overall ZIP structure
5. `calculateConfidence(const Byte* data, Size size)` - Assigns confidence score

### Optimization Techniques

- Early rejection of invalid headers
- Deduplication of candidate offsets
- Efficient boundary detection
- Minimal memory allocation during carving
- Smart overlap filtering to handle adjacent files

## Common Issues and Solutions

### Issue: ZIPs Not Detected

Possible causes:
- Corrupted Local File Header
- Missing End of Central Directory
- Invalid compression method

Solution: The carver attempts to recover partially valid ZIPs with lower confidence scores.

### Issue: Multiple ZIPs Detected as One

This was a critical issue that was resolved by implementing boundary detection. The solution involves:
1. Finding the next ZIP signature during size calculation
2. Limiting each ZIP's size to prevent it from consuming the next ZIP
3. Ensuring proper overlap checking in the candidate filtering phase

### Issue: False Positives

To minimize false positives, the carver:
1. Validates structural elements
2. Requires minimal valid content
3. Uses entropy checks
4. Applies strict header validation
