#!/bin/bash
# Filesystem parser analysis and improvement script
# This script helps inspect filesystems and improve the parsers

# Define color codes for output
GREEN="\033[0;32m"
YELLOW="\033[0;33m"
BLUE="\033[0;34m"
RED="\033[0;31m"
RESET="\033[0m"

# Set directories
FILEREC_DIR="/home/threesamyak/filerec"
BUILD_DIR="$FILEREC_DIR/build"
ANALYSIS_DIR="$FILEREC_DIR/filesystem_analysis"

# Check if running as root
if [ "$EUID" -ne 0 ]; then
  echo -e "${RED}Please run as root (use sudo)${RESET}"
  exit 1
fi

# Verify the device exists
DEVICE="${1:-/dev/sda1}"
if [ ! -b "$DEVICE" ]; then
  echo -e "${RED}Error: Device $DEVICE does not exist or is not a block device${RESET}"
  exit 1
fi

# Create analysis directory
mkdir -p "$ANALYSIS_DIR"

# Function to analyze ext4 filesystem
analyze_ext4() {
  echo -e "\n${BLUE}=== Analyzing EXT4 Filesystem ===${RESET}"
  
  # Create raw dump of superblock
  echo -e "${GREEN}Creating raw superblock dump...${RESET}"
  dd if=$DEVICE of="$ANALYSIS_DIR/ext4_superblock.bin" bs=1024 count=2 skip=1 2>/dev/null
  
  # Create hexdump of superblock
  echo -e "${GREEN}Creating hexdump of superblock...${RESET}"
  hexdump -C "$ANALYSIS_DIR/ext4_superblock.bin" > "$ANALYSIS_DIR/ext4_superblock.hex"
  
  # Get filesystem info
  echo -e "${GREEN}Getting filesystem info...${RESET}"
  tune2fs -l $DEVICE > "$ANALYSIS_DIR/ext4_info.txt"
  
  # Get inode information
  echo -e "${GREEN}Getting inode information...${RESET}"
  debugfs -R 'stat /' $DEVICE 2>/dev/null > "$ANALYSIS_DIR/ext4_root_inode.txt"
  
  # Check deleted files
  echo -e "${GREEN}Checking for deleted files...${RESET}"
  debugfs -R 'lsdel' $DEVICE 2>/dev/null > "$ANALYSIS_DIR/ext4_deleted_files.txt"
  
  # Get filesystem structure
  echo -e "${GREEN}Getting filesystem structure...${RESET}"
  dumpe2fs $DEVICE > "$ANALYSIS_DIR/ext4_structure.txt" 2>/dev/null
  
  echo -e "${YELLOW}For EXT4 metadata-based recovery, concentrate on:${RESET}"
  echo -e "1. Inode tables and deleted inodes (see ext4_deleted_files.txt)"
  echo -e "2. Directory entry recovery from journal"
  echo -e "3. Block groups and data blocks containing deleted files"
}

# Function to analyze NTFS filesystem
analyze_ntfs() {
  echo -e "\n${BLUE}=== Analyzing NTFS Filesystem ===${RESET}"
  
  # Create raw dump of boot sector
  echo -e "${GREEN}Creating raw boot sector dump...${RESET}"
  dd if=$DEVICE of="$ANALYSIS_DIR/ntfs_bootsector.bin" bs=512 count=1 2>/dev/null
  
  # Create hexdump of boot sector
  echo -e "${GREEN}Creating hexdump of boot sector...${RESET}"
  hexdump -C "$ANALYSIS_DIR/ntfs_bootsector.bin" > "$ANALYSIS_DIR/ntfs_bootsector.hex"
  
  # Get NTFS info
  echo -e "${GREEN}Getting NTFS info...${RESET}"
  ntfsinfo $DEVICE > "$ANALYSIS_DIR/ntfs_info.txt" 2>/dev/null
  
  # Get MFT info
  echo -e "${GREEN}Getting MFT info...${RESET}"
  ntfsinfo -m $DEVICE > "$ANALYSIS_DIR/ntfs_mft_info.txt" 2>/dev/null
  
  # Dump MFT records
  echo -e "${GREEN}Dumping first 10 MFT records...${RESET}"
  for i in {0..10}; do
    ntfscat -f $DEVICE "::$i" > "$ANALYSIS_DIR/ntfs_mft_$i.bin" 2>/dev/null
    hexdump -C "$ANALYSIS_DIR/ntfs_mft_$i.bin" > "$ANALYSIS_DIR/ntfs_mft_$i.hex" 2>/dev/null
  done
  
  echo -e "${YELLOW}For NTFS metadata-based recovery, concentrate on:${RESET}"
  echo -e "1. MFT structure and record flags (see ntfs_mft_info.txt)"
  echo -e "2. File record attributes, especially \$FILE_NAME and \$DATA"
  echo -e "3. Deleted file records marked with appropriate flags"
}

# Function to analyze FAT32 filesystem
analyze_fat32() {
  echo -e "\n${BLUE}=== Analyzing FAT32 Filesystem ===${RESET}"
  
  # Create raw dump of boot sector
  echo -e "${GREEN}Creating raw boot sector dump...${RESET}"
  dd if=$DEVICE of="$ANALYSIS_DIR/fat32_bootsector.bin" bs=512 count=1 2>/dev/null
  
  # Create hexdump of boot sector
  echo -e "${GREEN}Creating hexdump of boot sector...${RESET}"
  hexdump -C "$ANALYSIS_DIR/fat32_bootsector.bin" > "$ANALYSIS_DIR/fat32_bootsector.hex"
  
  # Get FAT info
  echo -e "${GREEN}Getting FAT info...${RESET}"
  fsck.fat -n $DEVICE > "$ANALYSIS_DIR/fat32_info.txt" 2>&1
  
  # Create raw dump of first FAT
  echo -e "${GREEN}Creating raw dump of first FAT entries...${RESET}"
  # Find out where FAT starts - typically at sector 32 for FAT32
  dd if=$DEVICE of="$ANALYSIS_DIR/fat32_fat.bin" bs=512 skip=32 count=32 2>/dev/null
  hexdump -C "$ANALYSIS_DIR/fat32_fat.bin" > "$ANALYSIS_DIR/fat32_fat.hex"
  
  # Create raw dump of root directory
  echo -e "${GREEN}Creating raw dump of root directory entries...${RESET}"
  # Root directory typically starts at cluster 2 which is after the FATs
  # But location depends on number of FATs and their size
  dd if=$DEVICE of="$ANALYSIS_DIR/fat32_root.bin" bs=512 skip=544 count=32 2>/dev/null
  hexdump -C "$ANALYSIS_DIR/fat32_root.bin" > "$ANALYSIS_DIR/fat32_root.hex"
  
  echo -e "${YELLOW}For FAT32 metadata-based recovery, concentrate on:${RESET}"
  echo -e "1. Directory entries with first byte marked as deleted (0xE5)"
  echo -e "2. Chain of clusters in the FAT table"
  echo -e "3. Directory structure and long filename entries"
}

# Function to generate improvement suggestions
generate_suggestions() {
  echo -e "\n${BLUE}=== Improvement Suggestions for FileRec ===${RESET}"
  
  # Create suggestions file
  SUGGESTIONS_FILE="$ANALYSIS_DIR/improvement_suggestions.txt"
  echo "FileRec Filesystem Parser Improvement Suggestions" > "$SUGGESTIONS_FILE"
  echo "Generated on $(date)" >> "$SUGGESTIONS_FILE"
  echo "=======================================" >> "$SUGGESTIONS_FILE"
  
  # Extract improvement suggestions for each filesystem
  
  # EXT4 suggestions
  echo -e "\n${YELLOW}EXT4 Parser Improvements:${RESET}" | tee -a "$SUGGESTIONS_FILE"
  
  if [ -f "$ANALYSIS_DIR/ext4_deleted_files.txt" ]; then
    if grep -q "^[0-9]" "$ANALYSIS_DIR/ext4_deleted_files.txt"; then
      echo "1. Implement deleted inode recovery - deleted files were found" | tee -a "$SUGGESTIONS_FILE"
      echo "   See $ANALYSIS_DIR/ext4_deleted_files.txt for list" | tee -a "$SUGGESTIONS_FILE"
      
      # Extract example deleted inodes
      deleted_inodes=$(grep -m 3 "^[0-9]" "$ANALYSIS_DIR/ext4_deleted_files.txt" | awk '{print $1}')
      echo "   Example deleted inodes to recover: $deleted_inodes" | tee -a "$SUGGESTIONS_FILE"
      
      # Add code snippet example
      echo "   Code improvement (pseudocode):" | tee -a "$SUGGESTIONS_FILE"
      echo '   ```cpp
   // In Ext4Parser::recoverDeletedFiles()
   std::vector<RecoveredFile> files;
   
   // Locate inode tables using superblock info
   uint32_t inodesPerGroup = superblock.s_inodes_per_group;
   
   // Scan for deleted inodes
   for (uint32_t group = 0; group < groupCount; group++) {
       uint64_t inodeTableOffset = getInodeTableOffset(group);
       
       for (uint32_t i = 0; i < inodesPerGroup; i++) {
           Inode inode = readInode(inodeTableOffset, i);
           
           // Check if inode is deleted but data blocks are still allocated
           if (inode.i_dtime != 0 && inode.i_blocks > 0) {
               RecoveredFile file;
               file.name = generateNameFromInode(group, i);
               file.size = inode.i_size;
               
               // Read data blocks from inode
               readDataBlocks(inode, file);
               
               files.push_back(file);
           }
       }
   }
   
   return files;
   ```' | tee -a "$SUGGESTIONS_FILE"
    else
      echo "No deleted files found in EXT4 filesystem" | tee -a "$SUGGESTIONS_FILE"
    fi
  fi
  
  # NTFS suggestions
  echo -e "\n${YELLOW}NTFS Parser Improvements:${RESET}" | tee -a "$SUGGESTIONS_FILE"
  
  if [ -f "$ANALYSIS_DIR/ntfs_info.txt" ]; then
    echo "1. Implement MFT record scanning for deleted files" | tee -a "$SUGGESTIONS_FILE"
    echo "   Code improvement (pseudocode):" | tee -a "$SUGGESTIONS_FILE"
    echo '   ```cpp
   // In NtfsParser::recoverDeletedFiles()
   std::vector<RecoveredFile> files;
   
   // Calculate MFT location from boot sector
   uint64_t mftOffset = bootSector.mft_lcn * bootSector.cluster_size;
   uint32_t recordSize = bootSector.clusters_per_mft_record < 0 ?
       1 << -bootSector.clusters_per_mft_record : 
       bootSector.clusters_per_mft_record * bootSector.cluster_size;
   
   // Scan MFT records
   for (uint32_t i = 0; i < mftEntryCount; i++) {
       MftRecord record = readMftRecord(mftOffset, i, recordSize);
       
       // Check if record is for a file and is marked as deleted
       if (record.isFile() && record.isDeleted() && !record.isInUse()) {
           RecoveredFile file;
           file.name = extractFilename(record);
           file.size = extractFileSize(record);
           
           // Extract data runs from $DATA attribute
           extractDataRuns(record, file);
           
           files.push_back(file);
       }
   }
   
   return files;
   ```' | tee -a "$SUGGESTIONS_FILE"
  fi
  
  # FAT32 suggestions
  echo -e "\n${YELLOW}FAT32 Parser Improvements:${RESET}" | tee -a "$SUGGESTIONS_FILE"
  
  if [ -f "$ANALYSIS_DIR/fat32_info.txt" ]; then
    echo "1. Implement directory entry scanning for deleted files" | tee -a "$SUGGESTIONS_FILE"
    echo "   Code improvement (pseudocode):" | tee -a "$SUGGESTIONS_FILE"
    echo '   ```cpp
   // In Fat32Parser::recoverDeletedFiles()
   std::vector<RecoveredFile> files;
   
   // Read FAT boot sector to get parameters
   uint32_t bytesPerSector = fatBootSector.bytes_per_sector;
   uint32_t sectorsPerCluster = fatBootSector.sectors_per_cluster;
   uint32_t rootDirCluster = fatBootSector.root_cluster;
   
   // Process the root directory and all subdirectories
   std::queue<uint32_t> directoriesToScan;
   directoriesToScan.push(rootDirCluster);
   
   while (!directoriesToScan.empty()) {
       uint32_t currentCluster = directoriesToScan.front();
       directoriesToScan.pop();
       
       // Read directory cluster
       std::vector<DirectoryEntry> entries = readDirectoryCluster(currentCluster);
       
       for (const auto& entry : entries) {
           // Check if entry is deleted (first byte is 0xE5)
           if (entry.isDeleted()) {
               RecoveredFile file;
               file.name = reconstructFilename(entry);
               file.size = entry.file_size;
               
               // Follow cluster chain in FAT
               uint32_t firstCluster = entry.first_cluster_low | 
                                      (entry.first_cluster_high << 16);
               readFileContents(firstCluster, file);
               
               files.push_back(file);
           }
           
           // If this is a directory, add to scanning queue
           if (entry.isDirectory() && !entry.isDotOrDotDot()) {
               uint32_t dirCluster = entry.first_cluster_low | 
                                    (entry.first_cluster_high << 16);
               directoriesToScan.push(dirCluster);
           }
       }
   }
   
   return files;
   ```' | tee -a "$SUGGESTIONS_FILE"
  fi
  
  echo -e "\n${BLUE}Improvement suggestions saved to $SUGGESTIONS_FILE${RESET}"
}

# Main function to perform analysis based on filesystem type
analyze_filesystem() {
  echo -e "${BLUE}=== Detecting filesystem type on $DEVICE ===${RESET}"
  
  # Create a mount point
  MOUNT_POINT="/mnt/filerec_analyze"
  mkdir -p "$MOUNT_POINT"
  
  # Try to detect filesystem type
  FS_TYPE=$(blkid -o value -s TYPE $DEVICE)
  
  echo -e "${GREEN}Detected filesystem: $FS_TYPE${RESET}"
  
  case "$FS_TYPE" in
    "ext4")
      analyze_ext4
      ;;
    "ntfs")
      analyze_ntfs
      ;;
    "vfat")
      analyze_fat32
      ;;
    *)
      echo -e "${RED}Unsupported or unknown filesystem: $FS_TYPE${RESET}"
      echo -e "${YELLOW}Please format the device as ext4, ntfs, or fat32${RESET}"
      exit 1
      ;;
  esac
  
  # Generate improvement suggestions
  generate_suggestions
}

# Run the analysis
analyze_filesystem

echo -e "\n${BLUE}=== Analysis Complete ===${RESET}"
echo -e "${GREEN}Results saved to $ANALYSIS_DIR${RESET}"
echo -e "${YELLOW}Review the suggestions and filesystem details to improve FileRec parsers${RESET}"
