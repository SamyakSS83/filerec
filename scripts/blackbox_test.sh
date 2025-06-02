#!/bin/bash
# Blackbox test for FileRec
# This script tests the FileRec tool by creating test data on a real disk,
# deleting it, and attempting to recover it.

# Usage: ./blackbox_test.sh [partition]
# Example: sudo ./blackbox_test.sh /dev/sda1

# Define cleanup function to handle exit
cleanup() {
  local exit_code=$?
  echo "Cleaning up..." | tee -a "$LOG_FILE"
  
  # If we mounted the partition, try to unmount it
  if [ -n "$ALREADY_MOUNTED" ] && [ "$ALREADY_MOUNTED" = false ] && mountpoint -q "$MOUNT_POINT"; then
    echo "Unmounting $MOUNT_POINT..." | tee -a "$LOG_FILE"
    umount "$MOUNT_POINT" || echo "Warning: Failed to unmount $MOUNT_POINT" | tee -a "$LOG_FILE"
  fi
  
  echo "Blackbox test exited with code $exit_code at $(date)" | tee -a "$LOG_FILE"
  exit $exit_code
}

# Register cleanup function
trap cleanup EXIT INT TERM

# Initialize log file
LOG_FILE="/home/threesamyak/filerec/blackbox_test.log"
echo "-----------------------------------------------" > "$LOG_FILE" 
echo "FileRec Blackbox Test Log - $(date)" >> "$LOG_FILE"
echo "-----------------------------------------------" >> "$LOG_FILE"

# Check if running as root
if [ "$EUID" -ne 0 ]; then
  echo "Please run as root (use sudo)"
  exit 1
fi

# Process arguments
PARTITION=${1:-"/dev/sda1"}
TEST_DIR="/home/threesamyak/filerec/tests/test_files"
MOUNT_POINT="/mnt/filerec_test"
TEMP_DIR="${MOUNT_POINT}/filerec_test_data"
RECOVERY_DIR="/home/threesamyak/filerec/test_output/blackbox_recovery"
BUILD_DIR="/home/threesamyak/filerec/build"

# Check if test files directory exists
if [ ! -d "$TEST_DIR" ]; then
  echo "Error: Test files directory $TEST_DIR does not exist" | tee -a "$LOG_FILE"
  echo "Please make sure the repository is properly set up with test files" | tee -a "$LOG_FILE"
  exit 1
fi

# Check if FileRec tool is built
if [ ! -f "$BUILD_DIR/FileRecoveryTool" ]; then
  echo "Error: FileRecoveryTool not found at $BUILD_DIR/FileRecoveryTool" | tee -a "$LOG_FILE"
  echo "Please build the project first by running: cd $BUILD_DIR && cmake .. && make" | tee -a "$LOG_FILE"
  exit 1
fi

echo "Starting FileRec blackbox test at $(date)" | tee -a "$LOG_FILE"
echo "Using partition: $PARTITION" | tee -a "$LOG_FILE"

# Ensure partition exists
if [ ! -b "$PARTITION" ]; then
  echo "Error: Partition $PARTITION does not exist" | tee -a "$LOG_FILE"
  exit 1
fi

# Create mount point if it doesn't exist
mkdir -p "$MOUNT_POINT"

# Check if partition is already mounted
if mount | grep -q "$PARTITION"; then
  ALREADY_MOUNTED=true
  CURRENT_MOUNT=$(mount | grep "$PARTITION" | awk '{print $3}')
  echo "Partition $PARTITION is already mounted at $CURRENT_MOUNT" | tee -a "$LOG_FILE"
  
  # Ask user if they want to continue with the current mount point
  read -p "Do you want to continue using the existing mount point? (y/n): " USE_EXISTING
  if [[ "$USE_EXISTING" =~ ^[Yy]$ ]]; then
    echo "Using existing mount at $CURRENT_MOUNT" | tee -a "$LOG_FILE"
    MOUNT_POINT="$CURRENT_MOUNT"
  else
    echo "Please unmount the partition first with: sudo umount $PARTITION" | tee -a "$LOG_FILE"
    echo "Then run this script again." | tee -a "$LOG_FILE"
    exit 1
  fi
else
  ALREADY_MOUNTED=false
  # Mount the partition
  echo "Mounting $PARTITION to $MOUNT_POINT" | tee -a "$LOG_FILE"
  mount "$PARTITION" "$MOUNT_POINT" || {
    echo "Failed to mount $PARTITION. It may be in use or require special options." | tee -a "$LOG_FILE"
    echo "For NTFS partitions, try: sudo ntfs-3g $PARTITION $MOUNT_POINT" | tee -a "$LOG_FILE"
    exit 1
  }
fi

# Create test directory
mkdir -p "$TEMP_DIR"
mkdir -p "$RECOVERY_DIR"

# Copy test files
echo "Copying test files to $TEMP_DIR" | tee -a "$LOG_FILE"
cp -v "$TEST_DIR"/* "$TEMP_DIR/" | tee -a "$LOG_FILE"

# List files in test directory
echo "Files in test directory:" | tee -a "$LOG_FILE"
ls -la "$TEMP_DIR" | tee -a "$LOG_FILE"

# Create zip archive of files
echo "Creating zip archive of test files" | tee -a "$LOG_FILE"
cd "$TEMP_DIR"
zip -r test_files.zip ./* | tee -a "$LOG_FILE"

# Sync to ensure all data is written
sync

# Generate file listing for verification
echo "Generating file checksums for verification" | tee -a "$LOG_FILE"
find "$TEMP_DIR" -type f -exec md5sum {} \; > "$RECOVERY_DIR/original_files.md5" 2>/dev/null || {
  echo "Warning: Could not generate MD5 checksums" | tee -a "$LOG_FILE"
}

# Create a backup of file info for verification
echo "Creating backup of file information" | tee -a "$LOG_FILE"
find "$TEMP_DIR" -type f -exec file {} \; > "$RECOVERY_DIR/original_files_info.txt" 2>/dev/null

# Delete the original files but keep the zip
echo "Deleting original files (but keeping the zip)" | tee -a "$LOG_FILE"
find "$TEMP_DIR" -type f -not -name "test_files.zip" -delete

# Delete the zip file
echo "Deleting the zip file" | tee -a "$LOG_FILE"
rm "$TEMP_DIR/test_files.zip"

# Sync again to ensure deletion is flushed to disk
sync

# Only unmount if we mounted it ourselves
if [ "$ALREADY_MOUNTED" = false ]; then
  echo "Unmounting $PARTITION from $MOUNT_POINT" | tee -a "$LOG_FILE"
  umount "$MOUNT_POINT" || {
    echo "Warning: Failed to unmount $PARTITION. It may still be in use." | tee -a "$LOG_FILE"
    echo "You may need to manually unmount it later with: sudo umount $MOUNT_POINT" | tee -a "$LOG_FILE"
  }
else
  echo "Skipping unmount as we're using an existing mount point" | tee -a "$LOG_FILE"
fi

# Run the recovery tool
echo "Running FileRec recovery tool on $PARTITION" | tee -a "$LOG_FILE"
"$BUILD_DIR/FileRecoveryTool" -v "$PARTITION" "$RECOVERY_DIR" | tee -a "$LOG_FILE"

# Print recovery results
echo "Recovery completed. Files recovered:" | tee -a "$LOG_FILE"
find "$RECOVERY_DIR" -type f | tee -a "$LOG_FILE"

# Check if original files can be verified
echo "Verifying recovered files..." | tee -a "$LOG_FILE"

# Count total recoverable files
TOTAL_FILES=$(wc -l < "$RECOVERY_DIR/original_files.md5")
FOUND_FILES=0
VERIFIED_FILES=0

# Process each recovered file
find "$RECOVERY_DIR" -type f -name "recovered_*" | while read -r recovered_file; do
  # Skip the MD5 file itself
  if [[ "$recovered_file" == *"original_files.md5"* ]]; then
    continue
  fi

  # Extract file signature to identify type
  FILE_TYPE=$(file -b "$recovered_file" | tr '[:upper:]' '[:lower:]')
  echo "Recovered file: $recovered_file ($FILE_TYPE)" | tee -a "$LOG_FILE"
  
  FOUND_FILES=$((FOUND_FILES + 1))
  
  # Try to verify based on file type
  if [[ "$FILE_TYPE" == *"jpeg"* || "$FILE_TYPE" == *"jpg"* ]]; then
    echo "  - JPEG file detected" | tee -a "$LOG_FILE"
    VERIFIED_FILES=$((VERIFIED_FILES + 1))
  elif [[ "$FILE_TYPE" == *"png"* ]]; then
    echo "  - PNG file detected" | tee -a "$LOG_FILE"
    VERIFIED_FILES=$((VERIFIED_FILES + 1))
  elif [[ "$FILE_TYPE" == *"pdf"* ]]; then
    echo "  - PDF file detected" | tee -a "$LOG_FILE"
    VERIFIED_FILES=$((VERIFIED_FILES + 1))
  elif [[ "$FILE_TYPE" == *"zip"* ]]; then
    echo "  - ZIP file detected" | tee -a "$LOG_FILE"
    # Try to list contents of the zip file
    if unzip -l "$recovered_file" &>/dev/null; then
      echo "  - ZIP file is valid" | tee -a "$LOG_FILE"
      VERIFIED_FILES=$((VERIFIED_FILES + 1))
    else
      echo "  - ZIP file is corrupted" | tee -a "$LOG_FILE"
    fi
  fi
done

# Calculate recovery rate
if [ "$TOTAL_FILES" -gt 0 ]; then
  RECOVERY_RATE=$((FOUND_FILES * 100 / TOTAL_FILES))
  VERIFICATION_RATE=$((VERIFIED_FILES * 100 / TOTAL_FILES))
  echo "Recovery summary:" | tee -a "$LOG_FILE"
  echo "  - Total files in original dataset: $TOTAL_FILES" | tee -a "$LOG_FILE"
  echo "  - Files found: $FOUND_FILES ($RECOVERY_RATE%)" | tee -a "$LOG_FILE"
  echo "  - Files verified: $VERIFIED_FILES ($VERIFICATION_RATE%)" | tee -a "$LOG_FILE"
else
  echo "No files were in the original dataset. Test may be invalid." | tee -a "$LOG_FILE"
fi

echo "Blackbox test completed at $(date)" | tee -a "$LOG_FILE"
echo "See $LOG_FILE for detailed log"
