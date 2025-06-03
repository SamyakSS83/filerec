#!/bin/bash
# Comprehensive filesystem testing script for FileRec
# This script formats a device with different filesystems, adds test files,
# deletes them, and tests recovery with both signature-based and metadata-based methods

# Define color codes for output
GREEN="\033[0;32m"
RED="\033[0;31m"
YELLOW="\033[0;33m"
BLUE="\033[0;34m"
RESET="\033[0m"

# Initialize log file
LOG_FILE="/home/threesamyak/filerec/filesystem_test.log"
echo "-----------------------------------------------" > "$LOG_FILE" 
echo "FileRec Filesystem Testing Log - $(date)" >> "$LOG_FILE"
echo "-----------------------------------------------" >> "$LOG_FILE"

# Set directories and paths
FILEREC_DIR="/home/threesamyak/filerec"
BUILD_DIR="$FILEREC_DIR/build"
TEST_FILES_DIR="$FILEREC_DIR/tests/test_files"
OUTPUT_BASE_DIR="$FILEREC_DIR/test_output/filesystem_test"

# Check if user is root
if [ "$EUID" -ne 0 ]; then
  echo -e "${RED}Please run as root (use sudo)${RESET}"
  exit 1
fi

# Check if FileRec tool is built
if [ ! -f "$BUILD_DIR/FileRecoveryTool" ]; then
  echo -e "${RED}Error: FileRecoveryTool not found at $BUILD_DIR/FileRecoveryTool${RESET}"
  echo -e "${YELLOW}Please build the project first by running: cd $BUILD_DIR && cmake .. && make${RESET}"
  exit 1
fi

# Verify the device exists
DEVICE="${1:-/dev/sda1}"
if [ ! -b "$DEVICE" ]; then
  echo -e "${RED}Error: Device $DEVICE does not exist or is not a block device${RESET}"
  exit 1
fi

# Confirm before proceeding
echo -e "${RED}WARNING: This script will format $DEVICE with different filesystems.${RESET}"
echo -e "${RED}ALL DATA ON $DEVICE WILL BE PERMANENTLY ERASED!${RESET}"
echo ""
read -p "Are you sure you want to proceed? (y/n): " -n 1 -r
echo ""
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
  echo -e "${YELLOW}Operation cancelled.${RESET}"
  exit 0
fi

# Create mount point and output directory
MOUNT_POINT="/mnt/filerec_test"
mkdir -p "$MOUNT_POINT"
mkdir -p "$OUTPUT_BASE_DIR"

# Define filesystems to test
declare -a FILESYSTEMS=("ext4" "ntfs" "fat32" "exfat")

# Function to unmount device if mounted
unmount_device() {
  if mount | grep -q "$DEVICE"; then
    echo "Unmounting $DEVICE..." | tee -a "$LOG_FILE"
    umount "$DEVICE" || {
      echo "Warning: Failed to unmount $DEVICE" | tee -a "$LOG_FILE"
      return 1
    }
  fi
  return 0
}

# Function to run recovery tests
run_recovery_tests() {
  local fs_type="$1"
  local output_dir="$OUTPUT_BASE_DIR/$fs_type"
  
  echo -e "\n${BLUE}=== Running Recovery Tests for $fs_type ===${RESET}" | tee -a "$LOG_FILE"
  
  # Create output directories
  mkdir -p "$output_dir/signature"
  mkdir -p "$output_dir/metadata"
  mkdir -p "$output_dir/full"
  
  # Run signature-based recovery
  echo -e "${GREEN}Running signature-based recovery...${RESET}" | tee -a "$LOG_FILE"
  "$BUILD_DIR/FileRecoveryTool" -v -s "$DEVICE" "$output_dir/signature" 2>&1 | tee -a "$LOG_FILE"
  
  # Run metadata-based recovery
  echo -e "${GREEN}Running metadata-based recovery...${RESET}" | tee -a "$LOG_FILE"
  "$BUILD_DIR/FileRecoveryTool" -v -m "$DEVICE" "$output_dir/metadata" 2>&1 | tee -a "$LOG_FILE"
  
  # Run full recovery
  echo -e "${GREEN}Running full recovery...${RESET}" | tee -a "$LOG_FILE"
  "$BUILD_DIR/FileRecoveryTool" -v "$DEVICE" "$output_dir/full" 2>&1 | tee -a "$LOG_FILE"
  
  # Count recovered files
  sig_count=$(find "$output_dir/signature" -type f | grep -v "recovery.log" | wc -l)
  meta_count=$(find "$output_dir/metadata" -type f | grep -v "recovery.log" | wc -l)
  full_count=$(find "$output_dir/full" -type f | grep -v "recovery.log" | wc -l)
  
  echo -e "\n${BLUE}=== Recovery Results for $fs_type ===${RESET}" | tee -a "$LOG_FILE"
  echo "Signature-based recovery: $sig_count files" | tee -a "$LOG_FILE"
  echo "Metadata-based recovery: $meta_count files" | tee -a "$LOG_FILE"
  echo "Full recovery: $full_count files" | tee -a "$LOG_FILE"
}

# Function to test a specific filesystem
test_filesystem() {
  local fs_type="$1"
  local fs_label="FILEREC_$fs_type"
  
  echo -e "\n${BLUE}=============================================${RESET}" | tee -a "$LOG_FILE"
  echo -e "${BLUE}=== Testing filesystem: $fs_type ===${RESET}" | tee -a "$LOG_FILE"
  echo -e "${BLUE}=============================================${RESET}" | tee -a "$LOG_FILE"
  
  # Unmount device if mounted
  unmount_device || return 1
  
  # Format device with the specified filesystem
  echo "Formatting $DEVICE as $fs_type..." | tee -a "$LOG_FILE"
  
  case "$fs_type" in
    "ext4")
      mkfs.ext4 -F -L "$fs_label" "$DEVICE" 2>&1 | tee -a "$LOG_FILE"
      ;;
    "ntfs")
      mkfs.ntfs -f -L "$fs_label" "$DEVICE" 2>&1 | tee -a "$LOG_FILE"
      ;;
    "fat32")
      mkfs.vfat -F 32 -n "${fs_label:0:11}" "$DEVICE" 2>&1 | tee -a "$LOG_FILE"
      ;;
    "exfat")
      mkfs.exfat -n "$fs_label" "$DEVICE" 2>&1 | tee -a "$LOG_FILE"
      ;;
    *)
      echo "Unsupported filesystem: $fs_type" | tee -a "$LOG_FILE"
      return 1
      ;;
  esac
  
  # Mount the device
  echo "Mounting $DEVICE as $fs_type..." | tee -a "$LOG_FILE"
  
  case "$fs_type" in
    "ext4")
      mount -t ext4 "$DEVICE" "$MOUNT_POINT" 2>&1 | tee -a "$LOG_FILE"
      ;;
    "ntfs")
      mount -t ntfs-3g "$DEVICE" "$MOUNT_POINT" 2>&1 | tee -a "$LOG_FILE"
      ;;
    "fat32")
      mount -t vfat "$DEVICE" "$MOUNT_POINT" 2>&1 | tee -a "$LOG_FILE"
      ;;
    "exfat")
      mount -t exfat "$DEVICE" "$MOUNT_POINT" 2>&1 | tee -a "$LOG_FILE"
      ;;
  esac
  
  # Check if mount succeeded
  if [ $? -ne 0 ]; then
    echo -e "${RED}Failed to mount $DEVICE as $fs_type${RESET}" | tee -a "$LOG_FILE"
    return 1
  fi
  
  # Create test directory
  TEST_DIR="$MOUNT_POINT/filerec_test"
  mkdir -p "$TEST_DIR"
  
  # Copy test files
  echo "Copying test files to $TEST_DIR..." | tee -a "$LOG_FILE"
  cp -v "$TEST_FILES_DIR"/* "$TEST_DIR/" 2>&1 | tee -a "$LOG_FILE"
  
  # Create some additional test text files
  echo "Creating additional test files..." | tee -a "$LOG_FILE"
  echo "This is a test text file for $fs_type filesystem" > "$TEST_DIR/test_$fs_type.txt"
  dd if=/dev/urandom of="$TEST_DIR/random_1KB.bin" bs=1K count=1 2>&1 | tee -a "$LOG_FILE"
  dd if=/dev/urandom of="$TEST_DIR/random_10KB.bin" bs=1K count=10 2>&1 | tee -a "$LOG_FILE"
  
  # Create nested directories
  mkdir -p "$TEST_DIR/dir1/dir2/dir3"
  echo "Nested directory test file" > "$TEST_DIR/dir1/dir2/dir3/nested.txt"
  cp "$TEST_FILES_DIR/normal.jpeg" "$TEST_DIR/dir1/dir2/test.jpeg"
  
  # List files
  echo -e "\nFiles in test directory:" | tee -a "$LOG_FILE"
  find "$TEST_DIR" -type f | sort | tee -a "$LOG_FILE"
  
  # Create a list of test files for verification
  find "$TEST_DIR" -type f > "$OUTPUT_BASE_DIR/${fs_type}_original_files.txt"
  
  # Sync to ensure all data is written
  sync
  
  # Unmount
  echo "Unmounting $DEVICE..." | tee -a "$LOG_FILE"
  umount "$DEVICE" 2>&1 | tee -a "$LOG_FILE"
  
  # Mount again and delete files
  case "$fs_type" in
    "ext4")
      mount -t ext4 "$DEVICE" "$MOUNT_POINT" 2>&1 | tee -a "$LOG_FILE"
      ;;
    "ntfs")
      mount -t ntfs-3g "$DEVICE" "$MOUNT_POINT" 2>&1 | tee -a "$LOG_FILE"
      ;;
    "fat32")
      mount -t vfat "$DEVICE" "$MOUNT_POINT" 2>&1 | tee -a "$LOG_FILE"
      ;;
    "exfat")
      mount -t exfat "$DEVICE" "$MOUNT_POINT" 2>&1 | tee -a "$LOG_FILE"
      ;;
  esac
  
  # Delete test files
  echo -e "\nDeleting test files..." | tee -a "$LOG_FILE"
  find "$TEST_DIR" -type f -delete
  
  # Delete directories
  rm -rf "$TEST_DIR"
  
  # Sync to ensure deletions are flushed
  sync
  
  # Unmount device
  echo "Unmounting $DEVICE again..." | tee -a "$LOG_FILE"
  umount "$DEVICE" 2>&1 | tee -a "$LOG_FILE"
  
  # Run recovery tests
  run_recovery_tests "$fs_type"
  
  return 0
}

# Main loop to test each filesystem
for fs in "${FILESYSTEMS[@]}"; do
  test_filesystem "$fs"
done

# Generate summary report
echo -e "\n${BLUE}=============================================${RESET}" | tee -a "$LOG_FILE"
echo -e "${BLUE}=== FileRec Filesystem Test Summary ===${RESET}" | tee -a "$LOG_FILE"
echo -e "${BLUE}=============================================${RESET}" | tee -a "$LOG_FILE"

for fs in "${FILESYSTEMS[@]}"; do
  echo -e "\n${YELLOW}=== $fs Filesystem Results ===${RESET}" | tee -a "$LOG_FILE"
  
  # Count original files
  orig_count=$(wc -l < "$OUTPUT_BASE_DIR/${fs}_original_files.txt")
  
  # Count recovered files
  sig_count=$(find "$OUTPUT_BASE_DIR/$fs/signature" -type f | grep -v "recovery.log" | wc -l)
  meta_count=$(find "$OUTPUT_BASE_DIR/$fs/metadata" -type f | grep -v "recovery.log" | wc -l)
  full_count=$(find "$OUTPUT_BASE_DIR/$fs/full" -type f | grep -v "recovery.log" | wc -l)
  
  # Calculate recovery rates
  sig_rate=$((sig_count * 100 / orig_count))
  meta_rate=$((meta_count * 100 / orig_count))
  full_rate=$((full_count * 100 / orig_count))
  
  echo "Original files: $orig_count" | tee -a "$LOG_FILE"
  echo "Signature recovery: $sig_count files ($sig_rate%)" | tee -a "$LOG_FILE"
  echo "Metadata recovery: $meta_count files ($meta_rate%)" | tee -a "$LOG_FILE"
  echo "Full recovery: $full_count files ($full_rate%)" | tee -a "$LOG_FILE"
  
  # Check file types recovered
  echo -e "\nFile types recovered by signature:" | tee -a "$LOG_FILE"
  find "$OUTPUT_BASE_DIR/$fs/signature" -type f -not -name "recovery.log" -exec file {} \; | awk -F: '{print $2}' | sort | uniq -c | tee -a "$LOG_FILE"
  
  echo -e "\nFile types recovered by metadata:" | tee -a "$LOG_FILE"
  find "$OUTPUT_BASE_DIR/$fs/metadata" -type f -not -name "recovery.log" -exec file {} \; | awk -F: '{print $2}' | sort | uniq -c | tee -a "$LOG_FILE"
done

echo -e "\n${BLUE}Filesystem testing completed at $(date)${RESET}" | tee -a "$LOG_FILE"
echo -e "${GREEN}See $LOG_FILE for detailed log${RESET}"
