// =====================================
// filesystem_dir.cpp
// Directory management and navigation
// Part of the Virtual Filesystem project (ZOS 2025)
// -------------------------------------
// Contains:
//  - mkdir()  → create directories
//  - ls()     → list directory contents
//  - cd()     → change current directory
//  - pwd()    → print working directory
//  - getParentInodeId(), findNameInParent()
// =====================================

#define _CRT_SECURE_NO_WARNINGS
#include "filesystem.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <cstring>

// -------------------------------------------------
// mkdir
// -------------------------------------------------
// Creates a new directory inside the current working directory.
// Steps:
//   1) Validate directory name
//   2) Check for duplicates in the parent directory
//   3) Read the parent inode
//   4) Allocate inode + data block for the new directory
//   5) Initialize inode structure and write it to disk
//   6) Create "." and ".." entries inside the new directory
//   7) Add the new directory entry to the parent directory
// -------------------------------------------------
void FileSystem::mkdir(const std::string& name) {
    const int parentInodeId = currentDirInode_; // current working directory

    // --- STEP 1: Validate input name ---
    if (name.empty()) {
        std::cerr << "[mkdir] Error: directory name is empty.\n";
        return;
    }

    if (name.size() > 11) {
        std::cerr << "[mkdir] Error: directory name too long (max 11 characters).\n";
        return;
    }

    if (name.find('/') != std::string::npos) {
        std::cerr << "[mkdir] Error: directory name cannot contain '/'.\n";
        return;
    }

    // --- STEP 2: Check if directory already exists ---
    if (directoryContains(parentInodeId, name)) {
        std::cerr << "[mkdir] Error: an item with this name already exists.\n";
        return;
    }

    // --- STEP 3: Load parent inode ---
    Inode parentInode = readInode(parentInodeId);
    if (!parentInode.is_directory) {
        std::cerr << "[mkdir] Error: current inode is not a directory (possible corruption).\n";
        return;
    }

    // --- STEP 4: Allocate new inode and data block ---
    int newInodeId = allocateFreeInode();
    int newBlockId = allocateFreeDataBlock();
    if (newInodeId == -1 || newBlockId == -1) {
        std::cerr << "[mkdir] Error: failed to allocate inode or data block.\n";
        return;
    }

    // --- STEP 5: Initialize inode for new directory ---
    Inode newInode{};
    newInode.id = newInodeId;
    newInode.is_directory = true;
    newInode.references = 1; // one reference from parent
    newInode.file_size = 2 * sizeof(DirectoryItem); // entries: "." and ".."
    newInode.direct1 = newBlockId;
    newInode.direct2 = 0;
    newInode.direct3 = 0;
    newInode.direct4 = 0;
    newInode.direct5 = 0;
    newInode.indirect1 = 0;
    newInode.indirect2 = 0;

    writeInode(newInodeId, newInode);

    std::cout << "[mkdir] Allocated inode " << newInodeId
        << " and data block " << newBlockId
        << " for directory '" << name << "'.\n";

    // --- STEP 6: Create '.' and '..' entries ---
    DirectoryItem dot{};
    dot.inode = newInodeId;
    std::strcpy(dot.item_name, ".");

    DirectoryItem dotdot{};
    dotdot.inode = parentInodeId;
    std::strcpy(dotdot.item_name, "..");

    std::fstream dirFile(filename_, std::ios::in | std::ios::out | std::ios::binary);
    if (!dirFile.is_open()) {
        std::cerr << "[mkdir] Error: cannot open filesystem for writing directory entries.\n";
        return;
    }

    dirFile.seekp(dataBlockOffset(newBlockId));
    dirFile.write(reinterpret_cast<char*>(&dot), sizeof(DirectoryItem));
    dirFile.write(reinterpret_cast<char*>(&dotdot), sizeof(DirectoryItem));
    dirFile.close();

    std::cout << "[mkdir] Initialized '.' and '..' in new directory (block "
        << newBlockId << ").\n";

    // --- STEP 7: Add entry to parent directory ---
    DirectoryItem newEntry{};
    newEntry.inode = newInodeId;
    std::strncpy(newEntry.item_name, name.c_str(), sizeof(newEntry.item_name) - 1);
    newEntry.item_name[sizeof(newEntry.item_name) - 1] = '\0';

    std::fstream parentFile(filename_, std::ios::in | std::ios::out | std::ios::binary);
    if (!parentFile.is_open()) {
        std::cerr << "[mkdir] Error: cannot open filesystem for updating parent directory.\n";
        return;
    }

    long long offset = dataBlockOffset(parentInode.direct1) + parentInode.file_size;
    parentFile.seekp(offset);
    parentFile.write(reinterpret_cast<char*>(&newEntry), sizeof(DirectoryItem));
    parentFile.close();

    parentInode.file_size += sizeof(DirectoryItem);
    writeInode(parentInodeId, parentInode);

    std::cout << "[mkdir] Directory '" << name
        << "' (inode " << newInodeId << ") successfully created and linked to parent.\n";
}

// -------------------------------------------------
// ls
// -------------------------------------------------
// Lists contents of the current directory or a specified subdirectory.
// Steps:
//   1) Determine target inode (current or specified name)
//   2) Load target inode and verify it's a directory
//   3) Read entries and print their names
// -------------------------------------------------
void FileSystem::ls(const std::string& name) {
    int targetInodeId = currentDirInode_;

    // --- STEP 1: Resolve target directory ---
    if (!name.empty()) {
        Inode root = readInode(0);
        std::ifstream file(filename_, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "[ls] Error: cannot open filesystem file.\n";
            return;
        }

        file.seekg(dataBlockOffset(root.direct1));

        DirectoryItem item{};
        int entries = root.file_size / sizeof(DirectoryItem);
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
            std::cerr << "[ls] Error: directory '" << name << "' not found.\n";
            return;
        }
    }

    // --- STEP 2: Load inode and verify directory ---
    Inode dirInode = readInode(targetInodeId);
    if (!dirInode.is_directory) {
        std::cerr << "[ls] Error: '" << name << "' is not a directory.\n";
        return;
    }

    // --- STEP 3: Read and print directory entries ---
    std::ifstream file(filename_, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[ls] Error: cannot open filesystem file.\n";
        return;
    }

    file.seekg(dataBlockOffset(dirInode.direct1));

    DirectoryItem item{};
    int entries = dirInode.file_size / sizeof(DirectoryItem);

    std::cout << "\nContents of directory '"
        << (name.empty() ? "/" : name)
        << "' (inode " << targetInodeId << "):\n";

    for (int i = 0; i < entries; ++i) {
        file.read(reinterpret_cast<char*>(&item), sizeof(DirectoryItem));
        if (item.inode != 0) {
            std::cout << " - " << item.item_name;
            Inode tmp = readInode(item.inode);
            if (tmp.is_directory)
                std::cout << "/";
            std::cout << "\n";
        }
    }

    file.close();
    std::cout << "\n";
}

// -------------------------------------------------
// cd
// -------------------------------------------------
// Changes the current working directory.
// Steps:
//   1) Handle "cd .." for moving up
//   2) Read current directory inode
//   3) Search for target name and move into it
// -------------------------------------------------
void FileSystem::cd(const std::string& name) {
    // --- STEP 1: Handle "cd .." ---
    if (name == "..") {
        Inode current = readInode(currentDirInode_);

        std::ifstream file(filename_, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "[cd] Error: cannot open filesystem file.\n";
            return;
        }

        // Skip "." entry, read ".."
        file.seekg(dataBlockOffset(current.direct1) + sizeof(DirectoryItem));
        DirectoryItem parent{};
        file.read(reinterpret_cast<char*>(&parent), sizeof(DirectoryItem));
        file.close();

        currentDirInode_ = parent.inode;
        std::cout << "[cd] Moved to parent directory (inode "
            << currentDirInode_ << ").\n";
        return;
    }

    // --- STEP 2: Load current inode ---
    Inode current = readInode(currentDirInode_);
    if (!current.is_directory) {
        std::cerr << "[cd] Error: current inode is not a directory.\n";
        return;
    }

    // --- STEP 3: Search for target ---
    std::ifstream file(filename_, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[cd] Error: cannot open filesystem file.\n";
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
                std::cerr << "[cd] Error: '" << name << "' is not a directory.\n";
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
        std::cerr << "[cd] Error: directory '" << name << "' not found.\n";
        return;
    }

    std::cout << "[cd] Moved into directory '" << name
        << "' (inode " << currentDirInode_ << ").\n";
}

// -------------------------------------------------
// getParentInodeId
// -------------------------------------------------
// Returns the parent inode id of a given directory.
// Reads the ".." entry from the directory's data block.
// -------------------------------------------------
int FileSystem::getParentInodeId(int dirInodeId) {
    Inode dirInode = readInode(dirInodeId);
    if (!dirInode.is_directory) {
        std::cerr << "[getParentInodeId] Error: given inode is not a directory.\n";
        return -1;
    }

    std::ifstream file(filename_, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[getParentInodeId] Error: cannot open filesystem file.\n";
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
// Finds the name of the directory entry in its parent
// that points to the given child inode.
// -------------------------------------------------
std::string FileSystem::findNameInParent(int parentInodeId, int childInodeId) {
    Inode parent = readInode(parentInodeId);
    if (!parent.is_directory) {
        std::cerr << "[findNameInParent] Error: parent inode is not a directory.\n";
        return "";
    }

    std::ifstream file(filename_, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[findNameInParent] Error: cannot open filesystem file.\n";
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
// Steps:
//   1) Start from current inode
//   2) Traverse up through ".." until reaching root
//   3) Print collected names in reverse order
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
        if (parentId == -1) {
            std::cerr << "[pwd] Error: invalid parent inode.\n";
            return;
        }

        std::string name = findNameInParent(parentId, currentId);
        if (name.empty()) {
            std::cerr << "[pwd] Error: directory name not found in parent.\n";
            return;
        }

        pathParts.push_back(name);
        currentId = parentId;
    }

    // --- STEP 3: Print path ---
    std::cout << "/";
    for (auto it = pathParts.rbegin(); it != pathParts.rend(); ++it)
        std::cout << *it << "/";
    std::cout << "\n";
}
