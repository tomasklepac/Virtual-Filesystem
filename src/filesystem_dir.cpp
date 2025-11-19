// =============================================
// filesystem_dir.cpp
// ---------------------------------------------
// Directory-level operations
// Handles:
//   - Creating and removing directories (mkdir, rmdir)
//   - Listing and navigating directories (ls, cd, pwd)
//   - Resolving parent/child relationships (getParentInodeId, findNameInParent)
// =============================================

#define _CRT_SECURE_NO_WARNINGS
#include "filesystem.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <cstring>

// -------------------------------------------------
// mkdir
// -------------------------------------------------
// Creates a new subdirectory in the current working directory.
// Allocates inode and data block, initializes "." and "..",
// and links the new directory to its parent.
// -------------------------------------------------
void FileSystem::mkdir(const std::string& name) {
    const int parentInodeId = currentDirInode_; // current working directory

    // --- STEP 1: Validate input name ---
    if (name.empty()) {
        std::cerr << "INVALID NAME\n";
        return;
    }

    if (name.size() > MAX_NAME_LENGTH) {
        std::cerr << "INVALID NAME\n";
        return;
    }

    if (name.find('/') != std::string::npos) {
        std::cerr << "INVALID NAME\n";
        return;
    }

    // --- STEP 2: Check if directory already exists ---
    if (directoryContains(parentInodeId, name)) {
        std::cerr << "EXIST\n";
        return;
    }

    // --- STEP 3: Load parent inode ---
    Inode parentInode = readInode(parentInodeId);
    if (!parentInode.is_directory) {
        std::cerr << "PATH NOT FOUND\n";
        return;
    }

    // --- STEP 4: Allocate new inode and data block ---
    int newInodeId = allocateFreeInode();
    int newBlockId = allocateFreeDataBlock();
    if (newInodeId == -1 || newBlockId == -1) {
        std::cerr << "NO SPACE\n";
        return;
    }

    // --- STEP 5: Initialize inode for new directory ---
    Inode newInode{};
    newInode.id = newInodeId;
    newInode.is_directory = true;
    newInode.references = 1; // one reference from parent
    newInode.file_size = 2 * sizeof(DirectoryItem); // entries: "." and ".."
    newInode.direct1 = newBlockId;

    writeInode(newInodeId, newInode);

    // --- STEP 6: Create '.' and '..' entries ---
    DirectoryItem dot{};
    dot.inode = newInodeId;
    std::strcpy(dot.item_name, ".");

    DirectoryItem dotdot{};
    dotdot.inode = parentInodeId;
    std::strcpy(dotdot.item_name, "..");

    std::fstream dirFile(filename_, std::ios::in | std::ios::out | std::ios::binary);
    if (!dirFile.is_open()) {
        std::cerr << "PATH NOT FOUND\n";
        return;
    }

    dirFile.seekp(dataBlockOffset(newBlockId));
    dirFile.write(reinterpret_cast<char*>(&dot), sizeof(DirectoryItem));
    dirFile.write(reinterpret_cast<char*>(&dotdot), sizeof(DirectoryItem));
    dirFile.close();

    // --- STEP 7: Add entry to parent directory ---
    DirectoryItem newEntry{};
    newEntry.inode = newInodeId;
    std::strncpy(newEntry.item_name, name.c_str(), MAX_NAME_LENGTH);
    newEntry.item_name[MAX_NAME_LENGTH] = '\0';

    std::fstream parentFile(filename_, std::ios::in | std::ios::out | std::ios::binary);
    if (!parentFile.is_open()) {
        std::cerr << "PATH NOT FOUND\n";
        return;
    }

    long long offset = dataBlockOffset(parentInode.direct1) + parentInode.file_size;
    parentFile.seekp(offset);
    parentFile.write(reinterpret_cast<char*>(&newEntry), sizeof(DirectoryItem));
    parentFile.close();

    parentInode.file_size += sizeof(DirectoryItem);
    writeInode(parentInodeId, parentInode);

    std::cout << "OK\n";
}

// -------------------------------------------------
// ls
// -------------------------------------------------
// Lists contents of the current or specified directory.
// Displays files and subdirectories with '/' suffix.
// -------------------------------------------------
void FileSystem::ls(const std::string& name) {
    int targetInodeId = currentDirInode_;  // current directory

    // --- STEP 1: Resolve target directory ---
    if (!name.empty()) {
        Inode current = readInode(currentDirInode_);
        std::ifstream file(filename_, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "PATH NOT FOUND\n";
            return;
        }

        file.seekg(dataBlockOffset(current.direct1));

        DirectoryItem item{};
        int entries = current.file_size / sizeof(DirectoryItem);
        bool found = false;

        for (int i = 0; i < entries; ++i) {
            file.read(reinterpret_cast<char*>(&item), sizeof(DirectoryItem));
            if (std::string(item.item_name) == name) {
                targetInodeId = item.inode;
                found = true;
                break;
            }
        }
        file.close();

        if (!found) {
            std::cerr << "FILE NOT FOUND\n";
            return;
        }
    }

    // --- STEP 2: Load inode and verify directory ---
    Inode dirInode = readInode(targetInodeId);
    if (!dirInode.is_directory) {
        std::cerr << "PATH NOT FOUND\n";
        return;
    }

    // --- STEP 3: Read and print directory entries ---
    std::ifstream file(filename_, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "PATH NOT FOUND\n";
        return;
    }

    file.seekg(dataBlockOffset(dirInode.direct1));

    DirectoryItem item{};
    int entries = dirInode.file_size / sizeof(DirectoryItem);

    for (int i = 0; i < entries; ++i) {
        file.read(reinterpret_cast<char*>(&item), sizeof(DirectoryItem));
        if (item.inode != 0) {
            Inode entry = readInode(item.inode);
            if (entry.is_directory)
                std::cout << "DIR: ";
            else
                std::cout << "FILE: ";
            std::cout << item.item_name << "\n";
        }
    }

    file.close();
}

// -------------------------------------------------
// cd
// -------------------------------------------------
// Changes the current working directory.
// Supports navigation into subdirectories
// and '..' for moving up one level.
// -------------------------------------------------
void FileSystem::cd(const std::string& name) {
    // --- STEP 1: Handle "cd .." ---
    if (name == "..") {
        Inode current = readInode(currentDirInode_);

        std::ifstream file(filename_, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "PATH NOT FOUND\n";
            return;
        }

        // Skip "." entry, read ".."
        file.seekg(dataBlockOffset(current.direct1) + sizeof(DirectoryItem));
        DirectoryItem parent{};
        file.read(reinterpret_cast<char*>(&parent), sizeof(DirectoryItem));
        file.close();

        currentDirInode_ = parent.inode;
        std::cout << "OK\n";
        return;
    }

    // --- STEP 2: Load current inode ---
    Inode current = readInode(currentDirInode_);
    if (!current.is_directory) {
        std::cerr << "PATH NOT FOUND\n";
        return;
    }

    // --- STEP 3: Search for target ---
    std::ifstream file(filename_, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "PATH NOT FOUND\n";
        return;
    }

    file.seekg(dataBlockOffset(current.direct1));

    DirectoryItem item{};
    int entries = current.file_size / sizeof(DirectoryItem);
    bool found = false;

    for (int i = 0; i < entries; ++i) {
        file.read(reinterpret_cast<char*>(&item), sizeof(DirectoryItem));
        if (std::string(item.item_name) == name) {
            Inode target = readInode(item.inode);
            if (!target.is_directory) {
                std::cerr << "PATH NOT FOUND\n";
                file.close();
                return;
            }
            currentDirInode_ = item.inode;
            found = true;
            break;
        }
    }

    file.close();

    if (!found) {
        std::cerr << "PATH NOT FOUND\n";
        return;
    }

    std::cout << "OK\n";
}

// -------------------------------------------------
// getParentInodeId
// -------------------------------------------------
// Returns the inode ID of the parent directory.
// Reads the ".." entry from the given directory block.
// Returns -1 if the provided inode is not a directory
// or if the filesystem file cannot be opened.
// -------------------------------------------------
int FileSystem::getParentInodeId(int dirInodeId) {
    Inode dirInode = readInode(dirInodeId);
    if (!dirInode.is_directory) {
        return -1;
    }

    std::ifstream file(filename_, std::ios::binary);
    if (!file.is_open()) {
        return -1;
    }

    // Skip "." and read ".."
    file.seekg(dataBlockOffset(dirInode.direct1) + sizeof(DirectoryItem));
    DirectoryItem parent{};
    file.read(reinterpret_cast<char*>(&parent), sizeof(DirectoryItem));
    file.close();

    return parent.inode;
}


// -------------------------------------------------
// findNameInParent
// -------------------------------------------------
// Finds and returns the directory entry name in the
// parent directory that references the given child inode.
// Returns an empty string if not found or if the parent
// is not a directory.
// -------------------------------------------------
std::string FileSystem::findNameInParent(int parentInodeId, int childInodeId) {
    Inode parent = readInode(parentInodeId);
    if (!parent.is_directory) {
        return "";
    }

    std::ifstream file(filename_, std::ios::binary);
    if (!file.is_open()) {
        return "";
    }

    file.seekg(dataBlockOffset(parent.direct1));

    DirectoryItem item{};
    int entries = parent.file_size / sizeof(DirectoryItem);
    std::string result;

    for (int i = 0; i < entries; ++i) {
        file.read(reinterpret_cast<char*>(&item), sizeof(DirectoryItem));
        if (item.inode == childInodeId &&
            std::strcmp(item.item_name, ".") != 0 &&
            std::strcmp(item.item_name, "..") != 0) {
            result = item.item_name;
            break;
        }
    }

    file.close();
    return result;
}


// -------------------------------------------------
// pwd
// -------------------------------------------------
// Prints the absolute path of the current working directory.
// Traverses parent directories upward using ".." links.
// -------------------------------------------------
void FileSystem::pwd() {
    int currentId = currentDirInode_;
    std::vector<std::string> pathParts;

    // --- STEP 1: Root special case ---
    if (currentId == 0) {
        std::cout << "/\n";
        return;
    }

    // --- STEP 2: Walk upward through parent links ---
    while (currentId != 0) {
        int parentId = getParentInodeId(currentId);
        if (parentId == -1)
            break;

        std::string name = findNameInParent(parentId, currentId);
        if (name.empty())
            break;

        pathParts.push_back(name);
        currentId = parentId;
    }

    // --- STEP 3: Print path ---
    std::cout << "/";
    for (auto it = pathParts.rbegin(); it != pathParts.rend(); ++it) {
        std::cout << *it;
        if (it + 1 != pathParts.rend())
            std::cout << "/";
    }
    std::cout << "\n";
}

// -------------------------------------------------
// rmdir
// -------------------------------------------------
// Removes an empty subdirectory from the current directory.
// Frees its inode and data block from the bitmaps.
// -------------------------------------------------
void FileSystem::rmdir(const std::string& name) {
    const int parentInodeId = currentDirInode_;

    // --- STEP 1: Validate input ---
    if (name.empty()) {
        std::cerr << "INVALID NAME\n";
        return;
    }

    // --- STEP 2: Verify parent directory ---
    Inode parent = readInode(parentInodeId);
    if (!parent.is_directory) {
        std::cerr << "PATH NOT FOUND\n";
        return;
    }

    // --- STEP 3: Locate target directory entry ---
    std::fstream file(filename_, std::ios::in | std::ios::out | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "PATH NOT FOUND\n";
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
        std::cerr << "FILE NOT FOUND\n";
        file.close();
        return;
    }

    // --- STEP 4: Verify target is a directory ---
    Inode target = readInode(targetInodeId);
    if (!target.is_directory) {
        std::cerr << "FILE NOT FOUND\n";
        file.close();
        return;
    }

    // --- STEP 5: Check if directory is empty ---
    if (target.file_size > 2 * sizeof(DirectoryItem)) {
        std::cerr << "NOT EMPTY\n";
        file.close();
        return;
    }

    // --- STEP 6: Free inode and data block bitmaps ---
    Superblock sb = readSuperblock();

    std::vector<char> inodeBitmap(INODE_BITMAP_SIZE);
    file.seekg(sb.bitmapi_start_address);
    file.read(inodeBitmap.data(), INODE_BITMAP_SIZE);
    inodeBitmap[targetInodeId] = 0;
    file.seekp(sb.bitmapi_start_address);
    file.write(inodeBitmap.data(), INODE_BITMAP_SIZE);

    std::vector<char> dataBitmap(DATA_BITMAP_SIZE);
    file.seekg(sb.bitmap_start_address);
    file.read(dataBitmap.data(), DATA_BITMAP_SIZE);
    if (target.direct1 > 0 && target.direct1 < DATA_BITMAP_SIZE) {
        dataBitmap[target.direct1] = 0;
        file.seekp(sb.bitmap_start_address);
        file.write(dataBitmap.data(), DATA_BITMAP_SIZE);
    }

    // --- STEP 7: Remove entry from parent directory ---
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

    std::cout << "OK\n";
}
