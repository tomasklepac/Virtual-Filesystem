// =============================================
// filesystem_core.cpp
// ---------------------------------------------
// Core filesystem operations
// Handles:
//   - Superblock and bitmap management
//   - Inode read/write
//   - Block allocation and freeing
//   - Filesystem formatting
//   - Core system commands (statfs, load)
// =============================================

#define _CRT_SECURE_NO_WARNINGS
#include "filesystem.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <cstring>
#include <filesystem>
#include <sstream>

// -------------------------------------------------
// format
// -------------------------------------------------
// Performs formatting of the virtual filesystem file.
// Initializes all core structures including:
//   - Superblock
//   - Inode and data bitmaps
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
    sb.bitmapi_start_address = sizeof(Superblock);
    sb.bitmap_start_address = sb.bitmapi_start_address + INODE_BITMAP_SIZE;
    sb.inode_start_address = sb.bitmap_start_address + DATA_BITMAP_SIZE;
    sb.data_start_address = sb.inode_start_address + INODE_TABLE_SIZE;

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
    inodeTable[0].file_size = 2 * sizeof(DirectoryItem);  // "." and ".."
    inodeTable[0].direct1 = 0;

    file.write(reinterpret_cast<char*>(inodeTable.data()), INODE_TABLE_SIZE);

    // --- STEP 6: Create root directory block ---
    DirectoryItem dot{};
    dot.inode = 0;
    std::strcpy(dot.item_name, ".");

    DirectoryItem dotdot{};
    dotdot.inode = 0;  // root's parent is itself
    std::strcpy(dotdot.item_name, "..");

    file.seekp(sb.data_start_address);
    file.write(reinterpret_cast<char*>(&dot), sizeof(DirectoryItem));
    file.write(reinterpret_cast<char*>(&dotdot), sizeof(DirectoryItem));
    file.close();

    // --- STEP 7: Expand file to full size ---
    try {
        std::filesystem::resize_file(filename_, totalBytes);
    }
    catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "[core] Error expanding file: " << e.what() << "\n";
        return false;
    }

    std::cout << "OK\n";

    currentDirInode_ = 0; // reset working directory
    return true;
}

// -------------------------------------------------
// readSuperblock
// -------------------------------------------------
// Loads and returns the current superblock from disk.
// Returns empty superblock if file doesn't exist (will be created by format).
// --------------------------------------------------
Superblock FileSystem::readSuperblock() {
    std::ifstream file(filename_, std::ios::binary);
    Superblock sb{};
    if (!file.is_open()) {
        // File doesn't exist yet - will be created by format() command
        return sb;
    }
    file.read(reinterpret_cast<char*>(&sb), sizeof(Superblock));
    file.close();
    return sb;
}

// -------------------------------------------------
// readInode
// -------------------------------------------------
// Reads a specific inode structure by its ID from disk.
// -------------------------------------------------
Inode FileSystem::readInode(int inodeId) {
    Superblock sb = readSuperblock();
    Inode inode{};
    
    // If superblock is empty, file hasn't been formatted yet
    if (sb.disk_size == 0) {
        return inode;
    }
    
    std::ifstream file(filename_, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[core] Error: cannot open filesystem file (readInode).\n";
        return inode;
    }

    long long offset = static_cast<long long>(sb.inode_start_address)
        + static_cast<long long>(inodeId) * sizeof(Inode);
    file.seekg(offset);
    file.read(reinterpret_cast<char*>(&inode), sizeof(Inode));
    file.close();
    return inode;
}

// -------------------------------------------------
// writeInode
// -------------------------------------------------
// Writes an inode structure to its correct position on disk.
// -------------------------------------------------
void FileSystem::writeInode(int inodeId, const Inode& inode) {
    Superblock sb = readSuperblock();
    std::fstream file(filename_, std::ios::in | std::ios::out | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[core] Error: cannot open filesystem file (writeInode).\n";
        return;
    }

    long long offset = static_cast<long long>(sb.inode_start_address)
        + static_cast<long long>(inodeId) * sizeof(Inode);
    file.seekp(offset);
    file.write(reinterpret_cast<const char*>(&inode), sizeof(Inode));
    file.close();
}

// -------------------------------------------------
// allocateFreeInode
// -------------------------------------------------
// Searches for a free inode in the bitmap,
// marks it as used, and returns its ID.
// -------------------------------------------------
int FileSystem::allocateFreeInode() {
    Superblock sb = readSuperblock();
    std::fstream file(filename_, std::ios::in | std::ios::out | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[alloc] Error: cannot open filesystem file (inode allocation).\n";
        return -1;
    }

    std::vector<char> bitmap(INODE_BITMAP_SIZE);
    file.seekg(sb.bitmapi_start_address);
    file.read(bitmap.data(), INODE_BITMAP_SIZE);

    for (int i = 0; i < INODE_BITMAP_SIZE; ++i) {
        if (bitmap[i] == 0) {
            bitmap[i] = 1;
            file.seekp(sb.bitmapi_start_address);
            file.write(bitmap.data(), INODE_BITMAP_SIZE);
            file.close();
            return i;
        }
    }

    std::cerr << "NO SPACE\n";
    file.close();
    return -1;
}

// -------------------------------------------------
// allocateFreeDataBlock
// -------------------------------------------------
// Searches for a free data block in the bitmap,
// marks it as used, and returns its block ID.
// -------------------------------------------------
int FileSystem::allocateFreeDataBlock() {
    Superblock sb = readSuperblock();
    std::fstream file(filename_, std::ios::in | std::ios::out | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[alloc] Error: cannot open filesystem file (data block allocation).\n";
        return -1;
    }

    std::vector<char> bitmap(DATA_BITMAP_SIZE);
    file.seekg(sb.bitmap_start_address);
    file.read(bitmap.data(), DATA_BITMAP_SIZE);

    for (int i = 0; i < DATA_BITMAP_SIZE; ++i) {
        if (bitmap[i] == 0) {
            bitmap[i] = 1;
            file.seekp(sb.bitmap_start_address);
            file.write(bitmap.data(), DATA_BITMAP_SIZE);
            file.close();
            return i;
        }
    }

    std::cerr << "NO SPACE\n";
    file.close();
    return -1;
}

// -------------------------------------------------
// dataBlockOffset
// -------------------------------------------------
// Computes the absolute byte offset of a data block
// within the virtual filesystem file.
// --------------------------------------------------
long long FileSystem::dataBlockOffset(int blockId) {
    Superblock sb = readSuperblock();
    return static_cast<long long>(sb.data_start_address)
        + static_cast<long long>(blockId) * sb.cluster_size;
}

// -------------------------------------------------
// directoryContains
// -------------------------------------------------
// Checks if a directory contains an item
// with the given name.
// -------------------------------------------------
bool FileSystem::directoryContains(int dirInodeId, const std::string& name) {
    Inode dirInode = readInode(dirInodeId);
    if (!dirInode.is_directory) {
        std::cerr << "PATH NOT FOUND\n";
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

// -------------------------------------------------
// statfs
// -------------------------------------------------
// Prints overall filesystem statistics such as
// used/free inodes, data blocks, and directory count.
// -------------------------------------------------
void FileSystem::statfs() {
    Superblock sb = readSuperblock();

    std::ifstream file(filename_, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[statfs] Error: cannot open filesystem file.\n";
        return;
    }

    // --- Read bitmaps ---
    std::vector<char> inodeBitmap(INODE_BITMAP_SIZE);
    std::vector<char> dataBitmap(DATA_BITMAP_SIZE);
    file.seekg(sb.bitmapi_start_address);
    file.read(inodeBitmap.data(), INODE_BITMAP_SIZE);
    file.seekg(sb.bitmap_start_address);
    file.read(dataBitmap.data(), DATA_BITMAP_SIZE);

    // --- Count used and free bits ---
    int usedInodes = 0, usedBlocks = 0;
    for (char bit : inodeBitmap) if (bit == 1) usedInodes++;
    for (char bit : dataBitmap) if (bit == 1) usedBlocks++;

    int freeInodes = INODE_BITMAP_SIZE - usedInodes;
    int freeBlocks = DATA_BITMAP_SIZE - usedBlocks;

    // --- Count directories ---
    int directoryCount = 0;
    file.seekg(sb.inode_start_address);
    const int inodeCount = INODE_TABLE_SIZE / sizeof(Inode);
    for (int i = 0; i < inodeCount; ++i) {
        Inode inode{};
        file.read(reinterpret_cast<char*>(&inode), sizeof(Inode));
        if (inode.is_directory && inode.id != 0)
            directoryCount++;
    }

    file.close();

    // --- Print results ---
    std::cout << "\nFilesystem statistics:\n";
    std::cout << "- Disk size: " << sb.disk_size << " bytes\n";
    std::cout << "- Cluster size: " << sb.cluster_size << " bytes\n";
    std::cout << "- Used inodes: " << usedInodes << " / " << INODE_BITMAP_SIZE << "\n";
    std::cout << "- Free inodes: " << freeInodes << "\n";
    std::cout << "- Used data blocks: " << usedBlocks << " / " << DATA_BITMAP_SIZE << "\n";
    std::cout << "- Free data blocks: " << freeBlocks << "\n";
    std::cout << "- Directories: " << directoryCount << "\n\n";
}

// -------------------------------------------------
// load
// -------------------------------------------------
// Executes a batch of commands from a text file
// located on the host filesystem.
// -------------------------------------------------
void FileSystem::load(const std::string& hostFilePath) {
    std::ifstream script(hostFilePath);
    if (!script.is_open()) {
        std::cerr << "FILE NOT FOUND\n";
        return;
    }

    std::string line;
    while (std::getline(script, line)) {
        // Skip empty lines or comments (#)
        if (line.empty() || line[0] == '#')
            continue;

        std::istringstream iss(line);
        std::string cmd, arg1, arg2, arg3;
        iss >> cmd >> arg1 >> arg2 >> arg3;

        // --- Basic command parser ---
        if (cmd == "format") { int n = std::stoi(arg1); format(n); }
        else if (cmd == "mkdir") mkdir(arg1);
        else if (cmd == "rmdir") rmdir(arg1);
        else if (cmd == "ls") ls();
        else if (cmd == "cd") cd(arg1);
        else if (cmd == "pwd") pwd();
        else if (cmd == "touch") touch(arg1);
        else if (cmd == "write") write(arg1, arg2);
        else if (cmd == "cat") cat(arg1);
        else if (cmd == "rm") rm(arg1);
        else if (cmd == "cp") cp(arg1, arg2);
        else if (cmd == "mv") mv(arg1, arg2);
        else if (cmd == "info") info(arg1);
        else if (cmd == "statfs") statfs();
        else if (cmd == "incp") incp(arg1, arg2);
        else if (cmd == "outcp") outcp(arg1, arg2);
        else if (cmd == "xcp") xcp(arg1, arg2, arg3);
        else if (cmd == "add") add(arg1, arg2);
        else if (cmd == "exit") { std::cout << "Terminating script.\n"; break; }
        else std::cerr << "UNKNOWN COMMAND\n";
    }

    script.close();
    std::cout << "OK\n";
}