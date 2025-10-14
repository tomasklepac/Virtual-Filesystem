// =============================================
// filesystem_file.cpp
// ---------------------------------------------
// File-level operations
// Handles:
//   - Creating files (touch)
//   - Reading file content (cat)
//   - Writing data to files (write)
//   - Removing files and empty directories (rm)
// =============================================

#define _CRT_SECURE_NO_WARNINGS
#include "filesystem.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <cstring>

// -------------------------------------------------
// touch
// -------------------------------------------------
// Creates an empty file in the current working directory.
// Steps:
//   1) Validate file name
//   2) Check for duplicates
//   3) Allocate inode for the file
//   4) Initialize inode and write it to disk
//   5) Add file entry to parent directory
// -------------------------------------------------
void FileSystem::touch(const std::string& name) {
    const int parentInodeId = currentDirInode_;

    // --- STEP 1: Validate name ---
    if (name.empty()) {
        std::cerr << "[touch] Error: file name is empty.\n";
        return;
    }
    if (name.size() > 11) {
        std::cerr << "[touch] Error: file name too long (max 11 characters).\n";
        return;
    }
    if (name.find('/') != std::string::npos) {
        std::cerr << "[touch] Error: file name cannot contain '/'.\n";
        return;
    }

    // --- STEP 2: Check for duplicates ---
    if (directoryContains(parentInodeId, name)) {
        std::cerr << "[touch] Error: file or directory with this name already exists.\n";
        return;
    }

    // --- STEP 3: Allocate inode for new file ---
    int newInodeId = allocateFreeInode();
    if (newInodeId == -1) {
        std::cerr << "[touch] Error: cannot allocate inode.\n";
        return;
    }

    // --- STEP 4: Initialize inode structure ---
    Inode newFile{};
    newFile.id = newInodeId;
    newFile.is_directory = false;
    newFile.references = 1;
    newFile.file_size = 0;
    newFile.direct1 = 0;
    newFile.direct2 = 0;
    newFile.direct3 = 0;
    newFile.direct4 = 0;
    newFile.direct5 = 0;
    newFile.indirect1 = 0;
    newFile.indirect2 = 0;

    writeInode(newInodeId, newFile);
    std::cout << "[touch] Created inode " << newInodeId
        << " for file '" << name << "'.\n";

    // --- STEP 5: Add entry to parent directory ---
    Inode parent = readInode(parentInodeId);
    if (!parent.is_directory) {
        std::cerr << "[touch] Error: parent inode is not a directory.\n";
        return;
    }

    DirectoryItem newItem{};
    newItem.inode = newInodeId;
    std::strncpy(newItem.item_name, name.c_str(), sizeof(newItem.item_name) - 1);
    newItem.item_name[sizeof(newItem.item_name) - 1] = '\0';

    std::fstream file(filename_, std::ios::in | std::ios::out | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[touch] Error: cannot open filesystem file.\n";
        return;
    }

    long long offset = dataBlockOffset(parent.direct1) + parent.file_size;
    file.seekp(offset);
    file.write(reinterpret_cast<char*>(&newItem), sizeof(DirectoryItem));
    file.close();

    parent.file_size += sizeof(DirectoryItem);
    writeInode(parentInodeId, parent);

    std::cout << "[touch] File '" << name << "' created successfully.\n";
}

// -------------------------------------------------
// cat
// -------------------------------------------------
// Displays the contents of a file.
// Steps:
//   1) Validate input
//   2) Locate file in current directory
//   3) Verify it’s a file
//   4) Read and print its content
// -------------------------------------------------
void FileSystem::cat(const std::string& name) {
    const int parentInodeId = currentDirInode_;

    // --- STEP 1: Validate input ---
    if (name.empty()) {
        std::cerr << "[cat] Error: file name is empty.\n";
        return;
    }

    // --- STEP 2: Locate file in current directory ---
    Inode dir = readInode(parentInodeId);
    if (!dir.is_directory) {
        std::cerr << "[cat] Error: current inode is not a directory.\n";
        return;
    }

    std::ifstream file(filename_, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[cat] Error: cannot open filesystem file.\n";
        return;
    }

    file.seekg(dataBlockOffset(dir.direct1));
    DirectoryItem item{};
    int entries = dir.file_size / sizeof(DirectoryItem);
    int fileInodeId = -1;

    for (int i = 0; i < entries; ++i) {
        file.read(reinterpret_cast<char*>(&item), sizeof(DirectoryItem));
        if (std::string(item.item_name) == name) {
            fileInodeId = item.inode;
            break;
        }
    }
    file.close();

    if (fileInodeId == -1) {
        std::cerr << "[cat] Error: file '" << name << "' not found.\n";
        return;
    }

    // --- STEP 3: Verify it’s a file ---
    Inode target = readInode(fileInodeId);
    if (target.is_directory) {
        std::cerr << "[cat] Error: '" << name << "' is a directory.\n";
        return;
    }

    if (target.file_size == 0 || target.direct1 == 0) {
        std::cout << "<empty file>\n";
        return;
    }

    // --- STEP 4: Read file content ---
    std::ifstream fs(filename_, std::ios::binary);
    if (!fs.is_open()) {
        std::cerr << "[cat] Error: cannot open filesystem file.\n";
        return;
    }

    fs.seekg(dataBlockOffset(target.direct1));
    std::vector<char> buffer(target.file_size + 1, 0);
    fs.read(buffer.data(), target.file_size);
    fs.close();

    std::cout << buffer.data() << "\n";
}

// -------------------------------------------------
// write
// -------------------------------------------------
// Writes text into an existing file.
// Steps:
//   1) Validate input
//   2) Find target file in current directory
//   3) Allocate data block if needed
//   4) Write content
//   5) Update inode size
// -------------------------------------------------
void FileSystem::write(const std::string& name, const std::string& content) {
    const int parentInodeId = currentDirInode_;

    // --- STEP 1: Validate input ---
    if (name.empty()) {
        std::cerr << "[write] Error: file name is empty.\n";
        return;
    }
    if (content.empty()) {
        std::cerr << "[write] Warning: empty content (nothing written).\n";
        return;
    }

    // --- STEP 2: Locate target file ---
    Inode dir = readInode(parentInodeId);
    if (!dir.is_directory) {
        std::cerr << "[write] Error: current inode is not a directory.\n";
        return;
    }

    std::ifstream file(filename_, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[write] Error: cannot open filesystem file.\n";
        return;
    }

    file.seekg(dataBlockOffset(dir.direct1));
    DirectoryItem item{};
    int entries = dir.file_size / sizeof(DirectoryItem);
    int fileInodeId = -1;

    for (int i = 0; i < entries; ++i) {
        file.read(reinterpret_cast<char*>(&item), sizeof(DirectoryItem));
        if (std::string(item.item_name) == name) {
            fileInodeId = item.inode;
            break;
        }
    }
    file.close();

    if (fileInodeId == -1) {
        std::cerr << "[write] Error: file '" << name << "' not found.\n";
        return;
    }

    // --- STEP 3: Load inode and prepare block ---
    Inode target = readInode(fileInodeId);
    if (target.is_directory) {
        std::cerr << "[write] Error: '" << name << "' is a directory.\n";
        return;
    }

    if (target.direct1 == 0) {
        int newBlock = allocateFreeDataBlock();
        if (newBlock == -1) {
            std::cerr << "[write] Error: no free data blocks available.\n";
            return;
        }
        target.direct1 = newBlock;
    }

    // --- STEP 4: Write content ---
    std::fstream fileOut(filename_, std::ios::in | std::ios::out | std::ios::binary);
    if (!fileOut.is_open()) {
        std::cerr << "[write] Error: cannot open filesystem file.\n";
        return;
    }

    fileOut.seekp(dataBlockOffset(target.direct1));
    fileOut.write(content.c_str(), content.size());
    fileOut.close();

    // --- STEP 5: Update inode ---
    target.file_size = static_cast<int>(content.size());
    writeInode(fileInodeId, target);

    std::cout << "[write] Wrote " << content.size()
        << " bytes to file '" << name
        << "' (inode " << fileInodeId << ").\n";
}

// -------------------------------------------------
// rm
// -------------------------------------------------
// Removes a file or empty directory.
// Steps:
//   1) Validate input
//   2) Find target item in current directory
//   3) Verify target (and check emptiness if directory)
//   4) Free data blocks and inode
//   5) Remove directory entry
// -------------------------------------------------
void FileSystem::rm(const std::string& name) {
    const int parentInodeId = currentDirInode_;

    // --- STEP 1: Validate input ---
    if (name.empty()) {
        std::cerr << "[rm] Error: name is empty.\n";
        return;
    }

    // --- STEP 2: Locate target in current directory ---
    Inode parent = readInode(parentInodeId);
    if (!parent.is_directory) {
        std::cerr << "[rm] Error: current inode is not a directory.\n";
        return;
    }

    std::fstream file(filename_, std::ios::in | std::ios::out | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[rm] Error: cannot open filesystem file.\n";
        return;
    }

    file.seekg(dataBlockOffset(parent.direct1));
    DirectoryItem item{};
    int entries = parent.file_size / sizeof(DirectoryItem);
    int targetIndex = -1, targetInodeId = -1;

    for (int i = 0; i < entries; ++i) {
        long long pos = dataBlockOffset(parent.direct1) + i * sizeof(DirectoryItem);
        file.seekg(pos);
        file.read(reinterpret_cast<char*>(&item), sizeof(DirectoryItem));

        if (std::string(item.item_name) == name) {
            targetIndex = i;
            targetInodeId = item.inode;
            break;
        }
    }

    if (targetInodeId == -1) {
        std::cerr << "[rm] Error: item '" << name << "' not found.\n";
        file.close();
        return;
    }

    // --- STEP 3: Load target inode ---
    Inode target = readInode(targetInodeId);
    if (target.is_directory && target.file_size > 2 * sizeof(DirectoryItem)) {
        std::cerr << "[rm] Error: directory '" << name << "' is not empty.\n";
        file.close();
        return;
    }

    // --- STEP 4: Free data block and inode ---
    Superblock sb = readSuperblock();
    std::vector<char> dataBitmap(DATA_BITMAP_SIZE);
    file.seekg(sb.bitmap_start_adress);
    file.read(dataBitmap.data(), DATA_BITMAP_SIZE);

    if (target.direct1 > 0 && target.direct1 < DATA_BITMAP_SIZE) {
        dataBitmap[target.direct1] = 0;
        file.seekp(sb.bitmap_start_adress);
        file.write(dataBitmap.data(), DATA_BITMAP_SIZE);
    }

    std::vector<char> inodeBitmap(INODE_BITMAP_SIZE);
    file.seekg(sb.bitmapi_start_adress);
    file.read(inodeBitmap.data(), INODE_BITMAP_SIZE);

    if (targetInodeId < INODE_BITMAP_SIZE) {
        inodeBitmap[targetInodeId] = 0;
        file.seekp(sb.bitmapi_start_adress);
        file.write(inodeBitmap.data(), INODE_BITMAP_SIZE);
    }

    // --- STEP 5: Remove directory entry ---
    if (entries > 1 && targetIndex != entries - 1) {
        DirectoryItem last{};
        long long lastPos = dataBlockOffset(parent.direct1) + (entries - 1) * sizeof(DirectoryItem);
        file.seekg(lastPos);
        file.read(reinterpret_cast<char*>(&last), sizeof(DirectoryItem));

        long long replacePos = dataBlockOffset(parent.direct1) + targetIndex * sizeof(DirectoryItem);
        file.seekp(replacePos);
        file.write(reinterpret_cast<char*>(&last), sizeof(DirectoryItem));
    }

    parent.file_size -= sizeof(DirectoryItem);
    writeInode(parentInodeId, parent);
    file.close();

    std::cout << "[rm] '" << name << "' removed successfully.\n";
}