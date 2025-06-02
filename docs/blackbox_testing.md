# Blackbox Testing for FileRec

This document explains how to use the blackbox testing script to verify the end-to-end functionality of FileRec in real-world scenarios.

## Overview

The blackbox testing script (`scripts/blackbox_test.sh`) allows testing FileRec's recovery capabilities in realistic scenarios by:

1. Creating test files on an actual disk partition
2. Zipping the files (to simulate file compression/archiving)
3. Deleting the files
4. Using FileRec to recover the deleted files
5. Verifying the recovery results

## Requirements

- Root/sudo privileges (for mounting/unmounting partitions)
- A test partition that can be used safely (no important data should be on it)
- All dependencies for FileRec must be installed
- The FileRec tool must be built (`make` in the build directory)

## Usage

```bash
sudo ./scripts/blackbox_test.sh [partition]
```

Where `[partition]` is the device path of the partition to use for testing (e.g., `/dev/sda1`).

### Example:

```bash
sudo ./scripts/blackbox_test.sh /dev/sda1
```

## Test Process

1. **Setup**: The script mounts the specified partition and creates a test directory
2. **Data Creation**: Test files are copied from `tests/test_files/` to the test partition
3. **Compression**: Files are zipped to create an archive file
4. **File Listing**: MD5 checksums of original files are generated for verification
5. **Deletion**: Original files and the zip archive are deleted
6. **Recovery**: FileRec is run to recover files from the partition
7. **Verification**: Recovered files are listed for manual verification

## Output

- All test actions are logged to `blackbox_test.log`
- Recovered files are stored in `test_output/blackbox_recovery/`
- Original file checksums are stored in `test_output/blackbox_recovery/original_files.md5`

## Troubleshooting

### Permission Issues
- Make sure you run the script with sudo permissions
- Verify that the partition is not in use or mounted elsewhere

### Already Mounted Partitions
- If the partition is already mounted, the script will detect this and give you options:
  1. Continue using the existing mount point
  2. Exit and manually unmount the partition first
- For NTFS partitions that are "exclusively opened," you may need to use the Windows "safely remove hardware" option first, or boot into Linux directly without Windows hibernation enabled

### Partition Not Found
- Check that the partition device path is correct
- Ensure the partition exists and is accessible

### NTFS Partition Issues
- Install ntfs-3g: `sudo apt-get install ntfs-3g`
- Use ntfs-3g explicitly: `sudo ntfs-3g /dev/sdXY /mnt/filerec_test`
- If the partition has errors, run `ntfsfix` on it: `sudo ntfsfix /dev/sdXY`

### Recovery Failures
- Check that FileRec is built correctly
- Verify that the test files were copied to the partition
- Examine the log for any error messages
- Try running the tool manually: `sudo ./build/FileRecoveryTool -v /dev/sdXY output_dir`

## Extending the Tests

To add more test cases:
1. Add test files to `tests/test_files/`
2. Modify the script to handle specific file types or test scenarios
3. Add verification steps for each test case
