// =============================================
// filesystem_core.cpp
// ---------------------------------------------
// Low-level filesystem operations
// Responsible for:
//   - Creating and formatting the virtual disk
//   - Reading/writing inodes and superblock
//   - Managing inode/data bitmaps and block offsets
// =============================================

#define _CRT_SECURE_NO_WARNINGS
#include "filesystem.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <cstring>
#include <filesystem>

// -------------------------------------------------
// format
// -------------------------------------------------
// Creates a new virtual filesystem file and initializes:
//   - Superblock
//   - Bitmaps
//   - Inode table
//   - Root directory (inode 0)
// -------------------------------------------------
bool FileSystem::format(int sizeMB) {
    std::ofstream file(filename_, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        std::cerr << "[core] Error: cannot create filesystem file.\n";
        return false;
    }

    // --- STEP 1: Calculate total size ---
    long long totalBytes = static_cast<long long>(sizeMB) * BYTES_PER_MB;

    // --- STEP 2: Prepare superblock ---
    Superblock sb{};
    std::memset(&sb, 0, sizeof(Superblock));

    std::strcpy(sb.signature, "klepac");
    std::strcpy(sb.volume_descriptor, "ZOS_FS_2025");
    sb.disk_size = static_cast<int32_t>(totalBytes);
    sb.cluster_size = CLUSTER_SIZE;
    sb.cluster_count = sb.disk_size / sb.cluster_size;

    // Layout offsets
    sb.bitmapi_start_adress = sizeof(Superblock);
    sb.bitmap_start_adress = sb.bitmapi_start_adress + INODE_BITMAP_SIZE;
    sb.inode_start_adress = sb.bitmap_start_adress + DATA_BITMAP_SIZE;
    sb.data_start_adress = sb.inode_start_adress + INODE_TABLE_SIZE;

    // --- STEP 3: Write superblock ---
    file.write(reinterpret_cast<char*>(&sb), sizeof(Superblock));

    // --- STEP 4: Initialize bitmaps ---
    std::vector<char> inodeBitmap(INODE_BITMAP_SIZE, 0);
    std::vector<char> dataBitmap(DATA_BITMAP_SIZE, 0);
    inodeBitmap[0] = 1; // root inode reserved
    dataBitmap[0] = 1;  // root data block reserved
    file.write(inodeBitmap.data(), INODE_BITMAP_SIZE);
    file.write(dataBitmap.data(), DATA_BITMAP_SIZE);

    // --- STEP 5: Initialize inode table ---
    const int inodeCount = INODE_TABLE_SIZE / sizeof(Inode);
    std::vector<Inode> inodeTable(inodeCount);
    std::memset(inodeTable.data(), 0, inodeTable.size() * sizeof(Inode));

    // Root inode setup
    inodeTable[0].id = 0;
    inodeTable[0].is_directory = true;
    inodeTable[0].references = 1;
    inodeTable[0].file_size = sizeof(DirectoryItem);
    inodeTable[0].direct1 = 0;

    file.write(reinterpret_cast<char*>(inodeTable.data()), INODE_TABLE_SIZE);

    // --- STEP 6: Create root directory block ---
    DirectoryItem root{};
    root.inode = 0;
    std::strcpy(root.item_name, ".");
    file.seekp(sb.data_start_adress);
    file.write(reinterpret_cast<char*>(&root), sizeof(DirectoryItem));
    file.close();

    // --- STEP 7: Expand file to full size ---
    try {
        std::filesystem::resize_file(filename_, totalBytes);
    }
    catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "[core] Error expanding file: " << e.what() << "\n";
        return false;
    }

    std::cout << "[core] Filesystem formatted (" << sizeMB << " MB)\n";
    std::cout << "[core] Root directory (inode 0) created.\n";

    currentDirInode_ = 0; // reset working directory
    return true;
}

// -------------------------------------------------
// printSuperblock
// -------------------------------------------------
// Prints superblock metadata information to console.
// -------------------------------------------------
void FileSystem::printSuperblock() {
    std::ifstream file(filename_, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[core] Error: cannot open filesystem file.\n";
        return;
    }

    Superblock sb{};
    file.read(reinterpret_cast<char*>(&sb), sizeof(Superblock));
    file.close();

    std::cout << "\n----- Superblock Information -----\n";
    std::cout << "Signature: " << sb.signature << "\n";
    std::cout << "Volume descriptor: " << sb.volume_descriptor << "\n";
    std::cout << "Disk size: " << sb.disk_size << " bytes\n";
    std::cout << "Cluster size: " << sb.cluster_size << " bytes\n";
    std::cout << "Cluster count: " << sb.cluster_count << "\n";
    std::cout << "Inode bitmap start: " << sb.bitmapi_start_adress << "\n";
    std::cout << "Data bitmap start: " << sb.bitmap_start_adress << "\n";
    std::cout << "Inode table start: " << sb.inode_start_adress << "\n";
    std::cout << "Data area start: " << sb.data_start_adress << "\n";
    std::cout << "----------------------------------\n";
}

// -------------------------------------------------
// readSuperblock
// -------------------------------------------------
Superblock FileSystem::readSuperblock() {
    std::ifstream file(filename_, std::ios::binary);
    Superblock sb{};
    if (!file.is_open()) {
        std::cerr << "[core] Error: cannot open filesystem file.\n";
        return sb;
    }
    file.read(reinterpret_cast<char*>(&sb), sizeof(Superblock));
    file.close();
    return sb;
}

// -------------------------------------------------
// readInode
// -------------------------------------------------
Inode FileSystem::readInode(int inodeId) {
    Superblock sb = readSuperblock();
    Inode inode{};
    std::ifstream file(filename_, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[core] Error: cannot open filesystem file (readInode).\n";
        return inode;
    }

    long long offset = static_cast<long long>(sb.inode_start_adress)
        + static_cast<long long>(inodeId) * sizeof(Inode);
    file.seekg(offset);
    file.read(reinterpret_cast<char*>(&inode), sizeof(Inode));
    file.close();
    return inode;
}

// -------------------------------------------------
// writeInode
// -------------------------------------------------
void FileSystem::writeInode(int inodeId, const Inode& inode) {
    Superblock sb = readSuperblock();
    std::fstream file(filename_, std::ios::in | std::ios::out | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[core] Error: cannot open filesystem file (writeInode).\n";
        return;
    }

    long long offset = static_cast<long long>(sb.inode_start_adress)
        + static_cast<long long>(inodeId) * sizeof(Inode);
    file.seekp(offset);
    file.write(reinterpret_cast<const char*>(&inode), sizeof(Inode));
    file.close();
}

// -------------------------------------------------
// allocateFreeInode
// -------------------------------------------------
// Finds the first free inode in the bitmap, marks it as used, and returns its ID.
// -------------------------------------------------
int FileSystem::allocateFreeInode() {
    Superblock sb = readSuperblock();
    std::fstream file(filename_, std::ios::in | std::ios::out | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[alloc] Error: cannot open filesystem file (inode allocation).\n";
        return -1;
    }

    std::vector<char> bitmap(INODE_BITMAP_SIZE);
    file.seekg(sb.bitmapi_start_adress);
    file.read(bitmap.data(), INODE_BITMAP_SIZE);

    for (int i = 0; i < INODE_BITMAP_SIZE; ++i) {
        if (bitmap[i] == 0) {
            bitmap[i] = 1;
            file.seekp(sb.bitmapi_start_adress);
            file.write(bitmap.data(), INODE_BITMAP_SIZE);
            file.close();
            return i;
        }
    }

    std::cerr << "[alloc] Error: no free inodes available.\n";
    file.close();
    return -1;
}

// -------------------------------------------------
// allocateFreeDataBlock
// -------------------------------------------------
int FileSystem::allocateFreeDataBlock() {
    Superblock sb = readSuperblock();
    std::fstream file(filename_, std::ios::in | std::ios::out | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[alloc] Error: cannot open filesystem file (data block allocation).\n";
        return -1;
    }

    std::vector<char> bitmap(DATA_BITMAP_SIZE);
    file.seekg(sb.bitmap_start_adress);
    file.read(bitmap.data(), DATA_BITMAP_SIZE);

    for (int i = 0; i < DATA_BITMAP_SIZE; ++i) {
        if (bitmap[i] == 0) {
            bitmap[i] = 1;
            file.seekp(sb.bitmap_start_adress);
            file.write(bitmap.data(), DATA_BITMAP_SIZE);
            file.close();
            return i;
        }
    }

    std::cerr << "[alloc] Error: no free data blocks available.\n";
    file.close();
    return -1;
}

// -------------------------------------------------
// dataBlockOffset
// -------------------------------------------------
long long FileSystem::dataBlockOffset(int blockId) {
    Superblock sb = readSuperblock();
    return static_cast<long long>(sb.data_start_adress)
        + static_cast<long long>(blockId) * sb.cluster_size;
}

// -------------------------------------------------
// directoryContains
// -------------------------------------------------
// Checks if a directory contains an item with the specified name.
// -------------------------------------------------
bool FileSystem::directoryContains(int dirInodeId, const std::string& name) {
    Inode dirInode = readInode(dirInodeId);
    if (!dirInode.is_directory) {
        std::cerr << "[core] Error: inode " << dirInodeId << " is not a directory.\n";
        return false;
    }

    std::ifstream file(filename_, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[core] Error: cannot open filesystem file.\n";
        return false;
    }

    file.seekg(dataBlockOffset(dirInode.direct1));

    DirectoryItem item{};
    int entries = dirInode.file_size / sizeof(DirectoryItem);

    for (int i = 0; i < entries; ++i) {
        file.read(reinterpret_cast<char*>(&item), sizeof(DirectoryItem));
        if (std::string(item.item_name) == name) {
            file.close();
            return true;
        }
    }

    file.close();
    return false;
}
