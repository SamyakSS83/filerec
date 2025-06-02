#!/bin/bash
# Simple blackbox test for FileRec
# This script creates a test image with real files then attempts to recover them
# using both signature and metadata-based recovery

# Initialize log file
LOG_FILE="/home/threesamyak/filerec/blackbox_test_simple.log"
echo "-----------------------------------------------" > "$LOG_FILE" 
echo "FileRec Simple Blackbox Test Log - $(date)" >> "$LOG_FILE"
echo "-----------------------------------------------" >> "$LOG_FILE"

# Set directories
FILEREC_DIR="/home/threesamyak/filerec"
BUILD_DIR="$FILEREC_DIR/build"
TEST_FILES_DIR="$FILEREC_DIR/tests/test_files"
OUTPUT_DIR="$FILEREC_DIR/test_output/simple_blackbox"
TEST_IMAGE="$FILEREC_DIR/test_blackbox_image.img"

# Check if FileRec tool is built
if [ ! -f "$BUILD_DIR/FileRecoveryTool" ]; then
  echo "Error: FileRecoveryTool not found at $BUILD_DIR/FileRecoveryTool" | tee -a "$LOG_FILE"
  echo "Please build the project first by running: cd $BUILD_DIR && cmake .. && make" | tee -a "$LOG_FILE"
  exit 1
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"

echo "Starting FileRec simple blackbox test at $(date)" | tee -a "$LOG_FILE"

# Step 1: Create a test image with a small partition and test files
echo "Creating test image with filesystem..." | tee -a "$LOG_FILE"
# Create a 20MB raw image file
dd if=/dev/zero of="$TEST_IMAGE" bs=1M count=20 2>&1 | tee -a "$LOG_FILE"

# Create an ext4 filesystem in the image
mkfs.ext4 -F "$TEST_IMAGE" 2>&1 | tee -a "$LOG_FILE"

# Create a temporary mount point
TEMP_MOUNT="/tmp/filerec_test_mount"
mkdir -p "$TEMP_MOUNT"

# Mount the image
mount -o loop "$TEST_IMAGE" "$TEMP_MOUNT" 2>&1 | tee -a "$LOG_FILE"

# Copy test files to the mounted filesystem
echo "Copying test files to mounted filesystem..." | tee -a "$LOG_FILE"
cp -v "$TEST_FILES_DIR"/* "$TEMP_MOUNT/" 2>&1 | tee -a "$LOG_FILE"

# List files in mounted filesystem
echo "Files in test filesystem:" | tee -a "$LOG_FILE"
ls -la "$TEMP_MOUNT" | tee -a "$LOG_FILE"

# Create a test directory and additional files
mkdir -p "$TEMP_MOUNT/test_directory"
echo "This is a test text file" > "$TEMP_MOUNT/test_directory/test.txt"
cp "$TEST_FILES_DIR/normal.jpeg" "$TEMP_MOUNT/test_directory/"

# Sync to ensure all data is written
sync

# Create a file list for verification
find "$TEMP_MOUNT" -type f | sort > "$OUTPUT_DIR/original_files.txt"
echo "Original files:" | tee -a "$LOG_FILE"
cat "$OUTPUT_DIR/original_files.txt" | tee -a "$LOG_FILE"

# Unmount the filesystem
umount "$TEMP_MOUNT" 2>&1 | tee -a "$LOG_FILE"

# Step 2: Run signature-only recovery
echo "Running signature-based recovery..." | tee -a "$LOG_FILE"
"$BUILD_DIR/FileRecoveryTool" -v -s "$TEST_IMAGE" "$OUTPUT_DIR/signature" 2>&1 | tee -a "$LOG_FILE"

# Step 3: Run metadata-based recovery
echo "Running metadata-based recovery..." | tee -a "$LOG_FILE"
"$BUILD_DIR/FileRecoveryTool" -v -m "$TEST_IMAGE" "$OUTPUT_DIR/metadata" 2>&1 | tee -a "$LOG_FILE"

# Step 4: Run both recovery methods together
echo "Running full recovery (both methods)..." | tee -a "$LOG_FILE"
"$BUILD_DIR/FileRecoveryTool" -v "$TEST_IMAGE" "$OUTPUT_DIR/full" 2>&1 | tee -a "$LOG_FILE"

# Clean up
rm -rf "$TEMP_MOUNT"

# Print results
echo "----- RECOVERY RESULTS -----" | tee -a "$LOG_FILE"
echo "Signature-based recovery files:" | tee -a "$LOG_FILE"
find "$OUTPUT_DIR/signature" -type f | grep -v "recovery.log" | sort | tee -a "$LOG_FILE"

echo "Metadata-based recovery files:" | tee -a "$LOG_FILE"
find "$OUTPUT_DIR/metadata" -type f | grep -v "recovery.log" | sort | tee -a "$LOG_FILE"

echo "Full recovery files:" | tee -a "$LOG_FILE"
find "$OUTPUT_DIR/full" -type f | grep -v "recovery.log" | sort | tee -a "$LOG_FILE"

# Compare results
echo "----- COMPARISON RESULTS -----" | tee -a "$LOG_FILE"

# Count original files
ORIGINAL_COUNT=$(grep -v "original_files.txt" "$OUTPUT_DIR/original_files.txt" | wc -l)

# Count recovered files
SIGNATURE_COUNT=$(find "$OUTPUT_DIR/signature" -type f | grep -v "recovery.log" | wc -l)
METADATA_COUNT=$(find "$OUTPUT_DIR/metadata" -type f | grep -v "recovery.log" | wc -l)
FULL_COUNT=$(find "$OUTPUT_DIR/full" -type f | grep -v "recovery.log" | wc -l)

echo "Original files: $ORIGINAL_COUNT" | tee -a "$LOG_FILE"
echo "Signature-based recovery: $SIGNATURE_COUNT files" | tee -a "$LOG_FILE"
echo "Metadata-based recovery: $METADATA_COUNT files" | tee -a "$LOG_FILE"
echo "Full recovery: $FULL_COUNT files" | tee -a "$LOG_FILE"

# Check file types for signature recovery
echo "Checking file types for signature recovery..." | tee -a "$LOG_FILE"
find "$OUTPUT_DIR/signature" -type f -not -name "recovery.log" -exec file {} \; | tee -a "$LOG_FILE"

# Check file types for metadata recovery
echo "Checking file types for metadata recovery..." | tee -a "$LOG_FILE"
find "$OUTPUT_DIR/metadata" -type f -not -name "recovery.log" -exec file {} \; | tee -a "$LOG_FILE"

echo "Simple blackbox test completed at $(date)" | tee -a "$LOG_FILE"
echo "See $LOG_FILE for detailed log"
