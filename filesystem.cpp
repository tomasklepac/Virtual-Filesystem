#define _CRT_SECURE_NO_WARNINGS
#include "filesystem.h"
#include <cstring>      // memset, strcpy
#include <vector>       // std::vector
#include <filesystem>   // resize_file

// Creates the virtual disk file, writes Superblock, bitmaps and inode table
bool FileSystem::format(int sizeMB) {
    std::ofstream file(filename_, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        std::cerr << "CANNOT CREATE FILE" << std::endl;
        return false;
    }

    // --- Calculate total size ---
    long long totalBytes = static_cast<long long>(sizeMB) * BYTES_PER_MB;

    // --- Superblock setup ---
    Superblock sb{};
    std::memset(&sb, 0, sizeof(Superblock));

    std::strcpy(sb.signature, "klepac");
    std::strcpy(sb.volume_descriptor, "ZOS_FS_2025");
    sb.disk_size = static_cast<int32_t>(totalBytes);
    sb.cluster_size = CLUSTER_SIZE;
    sb.cluster_count = sb.disk_size / sb.cluster_size;

    // Layout addresses
    sb.bitmapi_start_adress = sizeof(Superblock);
    sb.bitmap_start_adress = sb.bitmapi_start_adress + INODE_BITMAP_SIZE;
    sb.inode_start_adress = sb.bitmap_start_adress + DATA_BITMAP_SIZE;
    sb.data_start_adress = sb.inode_start_adress + INODE_TABLE_SIZE;

    // --- Write Superblock ---
    file.write(reinterpret_cast<char*>(&sb), sizeof(Superblock));

    // --- Write empty bitmaps ---
    const int bitmapInodesSize = INODE_BITMAP_SIZE;
    const int bitmapDataSize = DATA_BITMAP_SIZE;
    std::vector<char> emptyInodeBitmap(bitmapInodesSize, 0);
    std::vector<char> emptyDataBitmap(bitmapDataSize, 0);
    emptyInodeBitmap[0] = 1; // root inode reserved

    file.write(emptyInodeBitmap.data(), bitmapInodesSize);
    file.write(emptyDataBitmap.data(), bitmapDataSize);

    // --- Create inode table ---
    const int inodeTableSize = INODE_TABLE_SIZE; // 4 KB for inode table
    const int inodeCount = inodeTableSize / sizeof(Inode);

    std::vector<Inode> inodeTable(inodeCount);
    std::memset(inodeTable.data(), 0, inodeTable.size() * sizeof(Inode));

    // Root inode setup (inode 0)
    inodeTable[0].id = 0;
    inodeTable[0].is_directory = true;
    inodeTable[0].references = 1;
	inodeTable[0].file_size = sizeof(DirectoryItem); // size of one directory item
	inodeTable[0].direct1 = 0; // first data block (block 0)

    // --- Write inode table to disk ---
    file.write(reinterpret_cast<char*>(inodeTable.data()), inodeTableSize);

    // --- Create root directory data block ---
    DirectoryItem rootDir{};
    rootDir.inode = 0;
    std::strcpy(rootDir.item_name, ".");

    // Move file pointer to start of data area
    file.seekp(sb.data_start_adress);

    // Write root directory block
    file.write(reinterpret_cast<char*>(&rootDir), sizeof(DirectoryItem));

    // --- Safely expand file to total size ---
    file.close(); // close before resizing

    try {
        std::filesystem::resize_file(filename_, totalBytes);
    }
    catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error expanding file: " << e.what() << std::endl;
        return false;
    }

    std::cout << "Filesystem formatted (" << sizeMB << " MB)" << std::endl;
    std::cout << "Root directory (inode 0) created." << std::endl;
    return true;
}

// Prints the superblock information to the console
void FileSystem::printSuperblock() {
    std::ifstream file(filename_, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Cannot open filesystem file: " << filename_ << std::endl;
        return;
    }

    Superblock sb{};
    file.read(reinterpret_cast<char*>(&sb), sizeof(Superblock));

    std::cout << "----- Superblock Information -----" << std::endl;
    std::cout << "Signature: " << sb.signature << std::endl;
    std::cout << "Volume descriptor: " << sb.volume_descriptor << std::endl;
    std::cout << "Disk size: " << sb.disk_size << " bytes" << std::endl;
    std::cout << "Cluster size: " << sb.cluster_size << " bytes" << std::endl;
    std::cout << "Cluster count: " << sb.cluster_count << std::endl;
    std::cout << "Inode bitmap start: " << sb.bitmapi_start_adress << " bytes" << std::endl;
    std::cout << "Data bitmap start: " << sb.bitmap_start_adress << " bytes" << std::endl;
    std::cout << "Inode table start: " << sb.inode_start_adress << " bytes" << std::endl;
    std::cout << "Data area start: " << sb.data_start_adress << " bytes" << std::endl;
    std::cout << "----------------------------------" << std::endl;

    file.close();
}

// Reads and returns the superblock from the filesystem file
Superblock FileSystem::readSuperblock() {
    std::ifstream file(filename_, std::ios::binary);
    Superblock sb{};
    if (!file.is_open()) {
        std::cerr << "Cannot open filesystem file: " << filename_ << std::endl;
		return sb; // return empty superblock
    }
    file.read(reinterpret_cast<char*>(&sb), sizeof(Superblock));
    return sb;
}

// Reads a specific inode by its ID from the inode table
Inode FileSystem::readInode(int inodeId) {
	// Load superblock to get inode table start address
    Superblock sb = readSuperblock();

	// Open the filesystem file
    std::ifstream file(filename_, std::ios::binary);
	Inode inode{};  // Struct to hold the read inode

    if (!file.is_open()) {
        std::cerr << "Cannot open filesystem file for readInode()." << std::endl;
        return inode;
    }

	// Count offset to the desired inode
    long long offset = static_cast<long long>(sb.inode_start_adress)
        + static_cast<long long>(inodeId) * static_cast<long long>(sizeof(Inode));

	// Skip to the inode position
    file.seekg(offset);

	// Load the inode data
    file.read(reinterpret_cast<char*>(&inode), sizeof(Inode));

    return inode;
}

// Writes a specific inode by its ID to the inode table
void FileSystem::writeInode(int inodeId, const Inode& inode) {
    // Load superblock to get inode table start address
    Superblock sb = readSuperblock();

    // Open the filesystem file
    std::fstream file(filename_, std::ios::in | std::ios::out | std::ios::binary);

    if (!file.is_open()) {
        std::cerr << "Cannot open filesystem file for writeInode()." << std::endl;
        return;
    }

    // Calculate offset to the desired inode
    long long offset = static_cast<long long>(sb.inode_start_adress)
        + static_cast<long long>(inodeId) * static_cast<long long>(sizeof(Inode));

    // Move to the inode position
    file.seekp(offset);

    // Write the inode data
    file.write(reinterpret_cast<const char*>(&inode), sizeof(Inode));
}

// Allocates a free inode, marks it in the bitmap, and returns its ID
int FileSystem::allocateFreeInode() {
    Superblock sb = readSuperblock();

    std::fstream file(filename_, std::ios::in | std::ios::out | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Cannot open filesystem file for allocateFreeInode()." << std::endl;
        return -1;
    }

    // Load inode bitmap
    const int bitmapSize = INODE_BITMAP_SIZE;
}

// Allocates a free data block, marks it in the bitmap, and returns its ID
int FileSystem::allocateFreeDataBlock() {
    Superblock sb = readSuperblock();

    // Open filesystem file for reading and writing
    std::fstream file(filename_, std::ios::in | std::ios::out | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Cannot open filesystem file for allocateFreeDataBlock()." << std::endl;
        return -1;
    }

    // Load data bitmap
    std::vector<char> bitmap(DATA_BITMAP_SIZE);
    file.seekg(sb.bitmap_start_adress);
    file.read(bitmap.data(), DATA_BITMAP_SIZE);

    // Find first free data block
    for (int i = 0; i < DATA_BITMAP_SIZE; ++i) {
        if (bitmap[i] == 0) { // 0 = free block
            bitmap[i] = 1; // mark as used

            // Save updated bitmap back to disk
            file.seekp(sb.bitmap_start_adress);
            file.write(bitmap.data(), DATA_BITMAP_SIZE);

            // Return allocated block ID
            return i;
        }
    }

    std::cerr << "No free data blocks available." << std::endl;
    return -1;
}

// Calculates the byte offset of a data block by its ID
long long FileSystem::dataBlockOffset(int blokId) {
    Superblock sb = readSuperblock();
    return static_cast<long long>(sb.data_start_adress)
        + static_cast<long long>(blokId) * static_cast<long long>(sb.cluster_size);
}