// =============================================
// filesystem_file.cpp
// ---------------------------------------------
// File-level operations
// Handles:
//   - Creating, reading, writing and deleting files
//   - Copying, moving and concatenating files
//   - Importing and exporting between host FS and VFS
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
// Creates an empty file in the current directory.
// Validates the name, checks for duplicates,
// allocates an inode, and links it to the parent.
// -------------------------------------------------
void FileSystem::touch(const std::string& name) {
    const int parentInodeId = currentDirInode_;

    // --- STEP 1: Validate name ---
    if (name.empty() || name.size() > MAX_NAME_LENGTH || name.find('/') != std::string::npos) {
        std::cerr << "INVALID NAME\n";
        return;
    }

    // --- STEP 2: Check for duplicates ---
    if (directoryContains(parentInodeId, name)) {
        std::cerr << "EXIST\n";
        return;
    }

    // --- STEP 3: Allocate inode ---
    int newInodeId = allocateFreeInode();
    if (newInodeId == -1) {
        std::cerr << "NO SPACE\n";
        return;
    }

    // --- STEP 4: Initialize inode ---
    Inode newFile{};
    newFile.id = newInodeId;
    newFile.is_directory = false;
    newFile.references = 1;
    newFile.file_size = 0;
    writeInode(newInodeId, newFile);

    // --- STEP 5: Add entry to parent directory ---
    Inode parent = readInode(parentInodeId);
    if (!parent.is_directory) {
        std::cerr << "PATH NOT FOUND\n";
        return;
    }

    DirectoryItem newItem{};
    newItem.inode = newInodeId;
    std::strncpy(newItem.item_name, name.c_str(), MAX_NAME_LENGTH);
    newItem.item_name[MAX_NAME_LENGTH] = '\0';

    std::fstream file(filename_, std::ios::in | std::ios::out | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "PATH NOT FOUND\n";
        return;
    }

    long long offset = dataBlockOffset(parent.direct1) + parent.file_size;
    file.seekp(offset);
    file.write(reinterpret_cast<char*>(&newItem), sizeof(DirectoryItem));
    file.close();

    parent.file_size += sizeof(DirectoryItem);
    writeInode(parentInodeId, parent);

    std::cout << "OK\n";
}

// -------------------------------------------------
// cat
// -------------------------------------------------
// Displays the contents of a file.
// Prints its content or "<empty file>" if empty.
// -------------------------------------------------
void FileSystem::cat(const std::string& name) {
    const int parentInodeId = currentDirInode_;

    // --- STEP 1: Validate input ---
    if (name.empty()) {
        std::cerr << "INVALID NAME\n";
        return;
    }

    // --- STEP 2: Locate file in current directory ---
    Inode dir = readInode(parentInodeId);
    if (!dir.is_directory) {
        std::cerr << "PATH NOT FOUND\n";
        return;
    }

    std::ifstream file(filename_, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "PATH NOT FOUND\n";
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
        std::cerr << "FILE NOT FOUND\n";
        return;
    }

    // --- STEP 3: Verify it’s a file ---
    Inode target = readInode(fileInodeId);
    if (target.is_directory) {
        std::cerr << "IS DIRECTORY\n";
        return;
    }

    if (target.file_size == 0 || target.direct1 == 0) {
        std::cout << "<empty file>\n";
        return;
    }

    // --- STEP 4: Read and print content ---
    std::ifstream fs(filename_, std::ios::binary);
    if (!fs.is_open()) {
        std::cerr << "PATH NOT FOUND\n";
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
// Overwrites current content and updates inode size.
// -------------------------------------------------
void FileSystem::write(const std::string& name, const std::string& content) {
    const int parentInodeId = currentDirInode_;

    // --- STEP 1: Validate input ---
    if (name.empty()) {
        std::cerr << "INVALID NAME\n";
        return;
    }
    if (content.empty()) {
        std::cerr << "INVALID INPUT\n";
        return;
    }

    // --- STEP 2: Locate target file ---
    Inode dir = readInode(parentInodeId);
    if (!dir.is_directory) {
        std::cerr << "PATH NOT FOUND\n";
        return;
    }

    std::ifstream file(filename_, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "PATH NOT FOUND\n";
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
        std::cerr << "FILE NOT FOUND\n";
        return;
    }

    // --- STEP 3: Load inode and prepare block ---
    Inode target = readInode(fileInodeId);
    if (target.is_directory) {
        std::cerr << "FILE NOT FOUND\n";
        return;
    }

    if (target.direct1 == 0) {
        int newBlock = allocateFreeDataBlock();
        if (newBlock == -1) {
            std::cerr << "NO SPACE\n";
            return;
        }
        target.direct1 = newBlock;
    }

    // --- STEP 4: Write content ---
    std::fstream fileOut(filename_, std::ios::in | std::ios::out | std::ios::binary);
    if (!fileOut.is_open()) {
        std::cerr << "PATH NOT FOUND\n";
        return;
    }

    fileOut.seekp(dataBlockOffset(target.direct1));
    fileOut.write(content.c_str(), content.size());
    fileOut.close();

    // --- STEP 5: Update inode ---
    target.file_size = static_cast<int>(content.size());
    writeInode(fileInodeId, target);

    std::cout << "OK\n";
}

// -------------------------------------------------
// rm
// -------------------------------------------------
// Deletes a file or an empty directory from the
// current working directory and frees its inode.
// -------------------------------------------------
void FileSystem::rm(const std::string& name) {
    const int parentInodeId = currentDirInode_;

    // --- STEP 1: Validate input ---
    if (name.empty()) {
        std::cerr << "INVALID NAME\n";
        return;
    }

    // --- STEP 2: Locate target in current directory ---
    Inode parent = readInode(parentInodeId);
    if (!parent.is_directory) {
        std::cerr << "PATH NOT FOUND\n";
        return;
    }

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

    // --- STEP 3: Load target inode ---
    Inode target = readInode(targetInodeId);
    if (target.is_directory) {
        std::cerr << "FILE NOT FOUND\n";
        file.close();
        return;
    }

    // --- STEP 4: Free data block and inode ---
    Superblock sb = readSuperblock();
    std::vector<char> dataBitmap(DATA_BITMAP_SIZE);
    file.seekg(sb.bitmap_start_address);
    file.read(dataBitmap.data(), DATA_BITMAP_SIZE);

    if (target.direct1 > 0 && target.direct1 < DATA_BITMAP_SIZE) {
        dataBitmap[target.direct1] = 0;
        file.seekp(sb.bitmap_start_address);
        file.write(dataBitmap.data(), DATA_BITMAP_SIZE);
    }

    std::vector<char> inodeBitmap(INODE_BITMAP_SIZE);
    file.seekg(sb.bitmapi_start_address);
    file.read(inodeBitmap.data(), INODE_BITMAP_SIZE);

    if (targetInodeId < INODE_BITMAP_SIZE) {
        inodeBitmap[targetInodeId] = 0;
        file.seekp(sb.bitmapi_start_address);
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

    std::cout << "OK\n";
}

// -------------------------------------------------
// info
// -------------------------------------------------
// Prints detailed information about a file or directory.
// Includes size, inode number, and direct data blocks.
// -------------------------------------------------
void FileSystem::info(const std::string& name) {
    const int parentInodeId = currentDirInode_;

    // --- STEP 1: Validate input ---
    if (name.empty()) {
        std::cerr << "INVALID NAME\n";
        return;
    }

    // --- STEP 2: Locate target in current directory ---
    Inode parent = readInode(parentInodeId);
    if (!parent.is_directory) {
        std::cerr << "PATH NOT FOUND\n";
        return;
    }

    std::ifstream file(filename_, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "PATH NOT FOUND\n";
        return;
    }

    file.seekg(dataBlockOffset(parent.direct1));
    DirectoryItem item{};
    int entries = parent.file_size / sizeof(DirectoryItem);
    int targetInodeId = -1;

    for (int i = 0; i < entries; ++i) {
        file.read(reinterpret_cast<char*>(&item), sizeof(DirectoryItem));
        if (std::string(item.item_name) == name) {
            targetInodeId = item.inode;
            break;
        }
    }

    file.close();

    if (targetInodeId == -1) {
        std::cerr << "FILE NOT FOUND\n";
        return;
    }

    // --- STEP 3: Load inode ---
    Inode target = readInode(targetInodeId);

    // --- STEP 4: Print info ---
    std::cout << name
        << " - " << target.file_size << " B"
        << " - inode " << target.id
        << " - ";

    int directBlocks[5] = { target.direct1, target.direct2, target.direct3,
                            target.direct4, target.direct5 };

    bool hasBlocks = false;
    for (int b : directBlocks) {
        if (b > 0) {
            hasBlocks = true;
            break;
        }
    }

    if (hasBlocks) {
        std::cout << "direct_blocks ";
        bool first = true;
        for (int b : directBlocks) {
            if (b > 0) {
                if (!first) std::cout << ", ";
                std::cout << b;
                first = false;
            }
        }
    }

    if (target.indirect1 > 0 || target.indirect2 > 0) {
        if (hasBlocks) std::cout << " - ";
        std::cout << "indirect_blocks ";
        bool first = true;
        if (target.indirect1 > 0) {
            std::cout << target.indirect1;
            first = false;
        }
        if (target.indirect2 > 0) {
            if (!first) std::cout << ", ";
            std::cout << target.indirect2;
        }
    }

    std::cout << "\n";
}

// -------------------------------------------------
// cp
// -------------------------------------------------
// Copies a file within the virtual filesystem.
// Reads the content of the source file and creates
// a duplicate under a new name in the same directory.
// -------------------------------------------------
void FileSystem::cp(const std::string& source, const std::string& destination) {
    const int parentInodeId = currentDirInode_;

    // --- STEP 1: Validate ---
    if (source.empty() || destination.empty()) {
        std::cerr << "INVALID INPUT\n";
        return;
    }

    // --- STEP 2: Locate source file ---
    Inode parent = readInode(parentInodeId);
    if (!parent.is_directory) {
        std::cerr << "PATH NOT FOUND\n";
        return;
    }

    std::ifstream file(filename_, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "PATH NOT FOUND\n";
        return;
    }

    file.seekg(dataBlockOffset(parent.direct1));
    DirectoryItem item{};
    int entries = parent.file_size / sizeof(DirectoryItem);
    int srcInodeId = -1;

    for (int i = 0; i < entries; ++i) {
        file.read(reinterpret_cast<char*>(&item), sizeof(DirectoryItem));
        if (std::string(item.item_name) == source) {
            srcInodeId = item.inode;
            break;
        }
    }
    file.close();

    if (srcInodeId == -1) {
        std::cerr << "FILE NOT FOUND\n";
        return;
    }

    Inode src = readInode(srcInodeId);
    if (src.is_directory) {
        std::cerr << "FILE NOT FOUND\n";
        return;
    }

    // --- STEP 3: Read content from source ---
    std::string content;
    if (src.file_size > 0 && src.direct1 > 0) {
        std::ifstream fs(filename_, std::ios::binary);
        fs.seekg(dataBlockOffset(src.direct1));
        std::vector<char> buffer(src.file_size + 1, 0);
        fs.read(buffer.data(), src.file_size);
        fs.close();
        content = std::string(buffer.data(), src.file_size);
    }

    // --- STEP 4: Check if destination exists ---
    if (directoryContains(parentInodeId, destination)) {
        std::cerr << "EXIST\n";
        return;
    }

    // --- STEP 5: Create destination file ---
    int newInodeId = allocateFreeInode();
    if (newInodeId == -1) {
        std::cerr << "NO SPACE\n";
        return;
    }

    Inode newFile{};
    newFile.id = newInodeId;
    newFile.is_directory = false;
    newFile.references = 1;
    newFile.file_size = static_cast<int>(content.size());

    if (!content.empty()) {
        int newBlock = allocateFreeDataBlock();
        if (newBlock == -1) {
            std::cerr << "NO SPACE\n";
            return;
        }
        newFile.direct1 = newBlock;

        std::fstream fs(filename_, std::ios::in | std::ios::out | std::ios::binary);
        fs.seekp(dataBlockOffset(newBlock));
        fs.write(content.c_str(), content.size());
        fs.close();
    }

    writeInode(newInodeId, newFile);

    // --- STEP 6: Add directory entry ---
    DirectoryItem newItem{};
    newItem.inode = newInodeId;
    std::strncpy(newItem.item_name, destination.c_str(), MAX_NAME_LENGTH);
    newItem.item_name[MAX_NAME_LENGTH] = '\0';

    std::fstream parentFile(filename_, std::ios::in | std::ios::out | std::ios::binary);
    long long offset = dataBlockOffset(parent.direct1) + parent.file_size;
    parentFile.seekp(offset);
    parentFile.write(reinterpret_cast<char*>(&newItem), sizeof(DirectoryItem));
    parentFile.close();

    parent.file_size += sizeof(DirectoryItem);
    writeInode(parentInodeId, parent);

    std::cout << "OK\n";
}

// -------------------------------------------------
// mv
// -------------------------------------------------
// Moves or renames a file inside the virtual filesystem.
// If destination is a path, moves the file to that folder.
// Otherwise, renames it within the same directory.
// -------------------------------------------------
void FileSystem::mv(const std::string& source, const std::string& destination) {
    const int parentInodeId = currentDirInode_;

    // --- STEP 1: Validate input ---
    if (source.empty() || destination.empty()) {
        std::cerr << "INVALID INPUT\n";
        return;
    }

    // --- STEP 2: Find source item in current directory ---
    Inode parent = readInode(parentInodeId);
    if (!parent.is_directory) {
        std::cerr << "PATH NOT FOUND\n";
        return;
    }

    std::fstream fs(filename_, std::ios::in | std::ios::out | std::ios::binary);
    if (!fs.is_open()) {
        std::cerr << "PATH NOT FOUND\n";
        return;
    }

    DirectoryItem srcItem{};
    int entries = parent.file_size / sizeof(DirectoryItem);
    int srcInodeId = -1;
    long long srcPos = -1;

    for (int i = 0; i < entries; ++i) {
        long long pos = dataBlockOffset(parent.direct1) + i * sizeof(DirectoryItem);
        fs.seekg(pos);
        fs.read(reinterpret_cast<char*>(&srcItem), sizeof(DirectoryItem));
        if (std::string(srcItem.item_name) == source) {
            srcInodeId = srcItem.inode;
            srcPos = pos;
            break;
        }
    }

    if (srcInodeId == -1) {
        std::cerr << "FILE NOT FOUND\n";
        fs.close();
        return;
    }

    // --- STEP 3: Parse destination ---
    size_t slashPos = destination.find('/');
    std::string destDirName;
    std::string destFileName;

    if (slashPos == std::string::npos) {
        destDirName = "";
        destFileName = destination;
    }
    else {
        destDirName = destination.substr(0, slashPos);
        destFileName = destination.substr(slashPos + 1);
    }

    // --- STEP 4: Locate destination directory ---
    int destDirInodeId = parentInodeId;

    if (!destDirName.empty()) {
        fs.seekg(dataBlockOffset(parent.direct1));
        DirectoryItem dirItem{};
        bool foundDir = false;
        for (int i = 0; i < entries; ++i) {
            fs.read(reinterpret_cast<char*>(&dirItem), sizeof(DirectoryItem));
            if (std::string(dirItem.item_name) == destDirName) {
                Inode check = readInode(dirItem.inode);
                if (check.is_directory) {
                    destDirInodeId = dirItem.inode;
                    foundDir = true;
                }
                break;
            }
        }

        if (!foundDir) {
            std::cerr << "PATH NOT FOUND\n";
            fs.close();
            return;
        }
    }

    Inode destDir = readInode(destDirInodeId);
    if (!destDir.is_directory) {
        std::cerr << "PATH NOT FOUND\n";
        fs.close();
        return;
    }

    // --- STEP 5: Rename if in same directory ---
    if (destDirInodeId == parentInodeId) {
        std::strncpy(srcItem.item_name, destFileName.c_str(), MAX_NAME_LENGTH);
        srcItem.item_name[MAX_NAME_LENGTH] = '\0';
        fs.seekp(srcPos);
        fs.write(reinterpret_cast<char*>(&srcItem), sizeof(DirectoryItem));
        fs.close();
        std::cout << "OK\n";
        return;
    }

    // --- STEP 6: Move to another directory ---
    // Remove from current directory
    if (entries > 1 && srcPos != dataBlockOffset(parent.direct1) + (entries - 1) * sizeof(DirectoryItem)) {
        DirectoryItem last{};
        long long lastPos = dataBlockOffset(parent.direct1) + (entries - 1) * sizeof(DirectoryItem);
        fs.seekg(lastPos);
        fs.read(reinterpret_cast<char*>(&last), sizeof(DirectoryItem));

        fs.seekp(srcPos);
        fs.write(reinterpret_cast<char*>(&last), sizeof(DirectoryItem));
    }

    parent.file_size -= sizeof(DirectoryItem);
    writeInode(parentInodeId, parent);

    // Add entry to destination directory
    DirectoryItem newEntry{};
    newEntry.inode = srcInodeId;
    std::strncpy(newEntry.item_name, destFileName.c_str(), MAX_NAME_LENGTH);
    newEntry.item_name[MAX_NAME_LENGTH] = '\0';

    std::fstream destFs(filename_, std::ios::in | std::ios::out | std::ios::binary);
    long long destOffset = dataBlockOffset(destDir.direct1) + destDir.file_size;
    destFs.seekp(destOffset);
    destFs.write(reinterpret_cast<char*>(&newEntry), sizeof(DirectoryItem));
    destFs.close();

    destDir.file_size += sizeof(DirectoryItem);
    writeInode(destDirInodeId, destDir);

    fs.close();
    std::cout << "OK\n";
}

// -------------------------------------------------
// incp
// -------------------------------------------------
// Imports a file from the host filesystem into the VFS.
// Reads the real file, allocates inode and data block,
// and writes its content to the virtual filesystem.
// -------------------------------------------------
void FileSystem::incp(const std::string& sourceHostPath, const std::string& destVfsPath) {
    // --- STEP 1: Read real file ---
    std::ifstream input(sourceHostPath, std::ios::binary);
    if (!input.is_open()) {
        std::cerr << "FILE NOT FOUND\n";
        return;
    }

    std::vector<char> content((std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
    input.close();

    // Debug: Check for BOM
    if (content.size() >= 3) {
        unsigned char b0 = (unsigned char)content[0];
        unsigned char b1 = (unsigned char)content[1];
        unsigned char b2 = (unsigned char)content[2];
        if (b0 == 0xEF && b1 == 0xBB && b2 == 0xBF) {
            // Remove UTF-8 BOM
            content.erase(content.begin(), content.begin() + 3);
        }
    }

    // --- STEP 2: Parse destination path ---
    size_t slashPos = destVfsPath.find('/');
    std::string destDirName;
    std::string destFileName;

    if (slashPos == std::string::npos) {
        destDirName = "";
        destFileName = destVfsPath;
    }
    else {
        destDirName = destVfsPath.substr(0, slashPos);
        destFileName = destVfsPath.substr(slashPos + 1);
    }

    // --- STEP 3: Locate destination directory ---
    int destDirInodeId = currentDirInode_;
    Inode parent = readInode(destDirInodeId);

    std::ifstream fs(filename_, std::ios::binary);
    if (!fs.is_open()) {
        std::cerr << "PATH NOT FOUND\n";
        return;
    }

    if (!destDirName.empty()) {
        fs.seekg(dataBlockOffset(parent.direct1));
        DirectoryItem item{};
        int entries = parent.file_size / sizeof(DirectoryItem);
        bool found = false;

        for (int i = 0; i < entries; ++i) {
            fs.read(reinterpret_cast<char*>(&item), sizeof(DirectoryItem));
            if (std::string(item.item_name) == destDirName) {
                Inode check = readInode(item.inode);
                if (check.is_directory) {
                    destDirInodeId = item.inode;
                    found = true;
                }
                break;
            }
        }

        if (!found) {
            std::cerr << "PATH NOT FOUND\n";
            fs.close();
            return;
        }
    }
    fs.close();

    // --- STEP 4: Create file in destination directory ---
    if (directoryContains(destDirInodeId, destFileName)) {
        std::cerr << "EXIST\n";
        return;
    }

    int newInodeId = allocateFreeInode();
    int newBlockId = allocateFreeDataBlock();
    if (newInodeId == -1 || newBlockId == -1) {
        std::cerr << "NO SPACE\n";
        return;
    }

    // --- STEP 5: Write data into VFS ---
    std::fstream vfs(filename_, std::ios::in | std::ios::out | std::ios::binary);
    vfs.seekp(dataBlockOffset(newBlockId));
    vfs.write(content.data(), content.size());
    vfs.close();

    // --- STEP 6: Create inode and directory entry ---
    Inode newFile{};
    newFile.id = newInodeId;
    newFile.is_directory = false;
    newFile.references = 1;
    newFile.file_size = static_cast<int>(content.size());
    newFile.direct1 = newBlockId;
    writeInode(newInodeId, newFile);

    Inode destDir = readInode(destDirInodeId);
    DirectoryItem newItem{};
    newItem.inode = newInodeId;
    std::strncpy(newItem.item_name, destFileName.c_str(), MAX_NAME_LENGTH);
    newItem.item_name[MAX_NAME_LENGTH] = '\0';

    std::fstream dirFile(filename_, std::ios::in | std::ios::out | std::ios::binary);
    long long offset = dataBlockOffset(destDir.direct1) + destDir.file_size;
    dirFile.seekp(offset);
    dirFile.write(reinterpret_cast<char*>(&newItem), sizeof(DirectoryItem));
    dirFile.close();

    destDir.file_size += sizeof(DirectoryItem);
    writeInode(destDirInodeId, destDir);

    std::cout << "OK\n";
}

// -------------------------------------------------
// outcp
// -------------------------------------------------
// Exports a file from the virtual filesystem to the
// host filesystem. Reads VFS content and writes it
// into a real file on the host disk.
// -------------------------------------------------
void FileSystem::outcp(const std::string& sourceVfsPath, const std::string& destHostPath) {
    const int parentInodeId = currentDirInode_;

    // --- STEP 1: Validate input ---
    if (sourceVfsPath.empty() || destHostPath.empty()) {
        std::cerr << "INVALID INPUT\n";
        return;
    }

    // --- STEP 2: Parse VFS source path ---
    size_t slashPos = sourceVfsPath.find('/');
    std::string srcDirName;
    std::string srcFileName;

    if (slashPos == std::string::npos) {
        srcDirName = "";
        srcFileName = sourceVfsPath;
    }
    else {
        srcDirName = sourceVfsPath.substr(0, slashPos);
        srcFileName = sourceVfsPath.substr(slashPos + 1);
    }

    // --- STEP 3: Find source file in current or specified directory ---
    int srcDirInodeId = parentInodeId;
    Inode parent = readInode(srcDirInodeId);

    std::ifstream fs(filename_, std::ios::binary);
    if (!fs.is_open()) {
        std::cerr << "PATH NOT FOUND\n";
        return;
    }

    if (!srcDirName.empty()) {
        fs.seekg(dataBlockOffset(parent.direct1));
        DirectoryItem item{};
        int entries = parent.file_size / sizeof(DirectoryItem);
        bool foundDir = false;

        for (int i = 0; i < entries; ++i) {
            fs.read(reinterpret_cast<char*>(&item), sizeof(DirectoryItem));
            if (std::string(item.item_name) == srcDirName) {
                Inode check = readInode(item.inode);
                if (check.is_directory) {
                    srcDirInodeId = item.inode;
                    foundDir = true;
                }
                break;
            }
        }

        if (!foundDir) {
            std::cerr << "PATH NOT FOUND\n";
            fs.close();
            return;
        }
    }

    fs.close();

    // --- STEP 4: Locate file ---
    Inode srcDir = readInode(srcDirInodeId);
    std::ifstream vfs(filename_, std::ios::binary);
    if (!vfs.is_open()) {
        std::cerr << "PATH NOT FOUND\n";
        return;
    }

    vfs.seekg(dataBlockOffset(srcDir.direct1));
    DirectoryItem item{};
    int entries = srcDir.file_size / sizeof(DirectoryItem);
    int fileInodeId = -1;

    for (int i = 0; i < entries; ++i) {
        vfs.read(reinterpret_cast<char*>(&item), sizeof(DirectoryItem));
        if (std::string(item.item_name) == srcFileName) {
            fileInodeId = item.inode;
            break;
        }
    }

    if (fileInodeId == -1) {
        std::cerr << "FILE NOT FOUND\n";
        vfs.close();
        return;
    }

    Inode srcFile = readInode(fileInodeId);
    if (srcFile.is_directory) {
        std::cerr << "FILE NOT FOUND\n";
        vfs.close();
        return;
    }

    // --- STEP 5: Read file content from VFS ---
    if (srcFile.file_size == 0 || srcFile.direct1 == 0) {
        std::ofstream output(destHostPath, std::ios::binary);
        if (!output.is_open()) {
            std::cerr << "PATH NOT FOUND\n";
            vfs.close();
            return;
        }
        output.close();
        std::cout << "OK\n";
        return;
    }

    vfs.seekg(dataBlockOffset(srcFile.direct1));
    std::vector<char> buffer(srcFile.file_size);
    vfs.read(buffer.data(), srcFile.file_size);
    vfs.close();

    // --- STEP 6: Write to host file ---
    std::ofstream output(destHostPath, std::ios::binary);
    if (!output.is_open()) {
        std::cerr << "PATH NOT FOUND\n";
        return;
    }

    output.write(buffer.data(), srcFile.file_size);
    output.close();

    std::cout << "OK\n";
}

// -------------------------------------------------
// xcp
// -------------------------------------------------
// Concatenates two files (s1 + s2) into a new file s3
// inside the same directory of the virtual filesystem.
// -------------------------------------------------
void FileSystem::xcp(const std::string& s1, const std::string& s2, const std::string& s3) {
    const int parentInodeId = currentDirInode_;

    // --- STEP 1: Validate input ---
    if (s1.empty() || s2.empty() || s3.empty()) {
        std::cerr << "INVALID INPUT\n";
        return;
    }

    // --- STEP 2: Verify current directory ---
    Inode parent = readInode(parentInodeId);
    if (!parent.is_directory) {
        std::cerr << "PATH NOT FOUND\n";
        return;
    }

    std::ifstream vfs(filename_, std::ios::binary);
    if (!vfs.is_open()) {
        std::cerr << "PATH NOT FOUND\n";
        return;
    }

    // --- STEP 3: Find s1 ---
    vfs.seekg(dataBlockOffset(parent.direct1));
    DirectoryItem item{};
    int entries = parent.file_size / sizeof(DirectoryItem);
    int inode1 = -1;

    for (int i = 0; i < entries; ++i) {
        vfs.read(reinterpret_cast<char*>(&item), sizeof(DirectoryItem));
        if (std::string(item.item_name) == s1) {
            inode1 = item.inode;
            break;
        }
    }

    if (inode1 == -1) {
        std::cerr << "FILE NOT FOUND\n";
        vfs.close();
        return;
    }

    Inode f1 = readInode(inode1);
    if (f1.is_directory) {
        std::cerr << "FILE NOT FOUND\n";
        vfs.close();
        return;
    }

    // --- STEP 4: Find s2 ---
    vfs.clear();
    vfs.seekg(dataBlockOffset(parent.direct1));
    int inode2 = -1;

    for (int i = 0; i < entries; ++i) {
        vfs.read(reinterpret_cast<char*>(&item), sizeof(DirectoryItem));
        if (std::string(item.item_name) == s2) {
            inode2 = item.inode;
            break;
        }
    }

    if (inode2 == -1) {
        std::cerr << "FILE NOT FOUND\n";
        vfs.close();
        return;
    }

    Inode f2 = readInode(inode2);
    if (f2.is_directory) {
        std::cerr << "FILE NOT FOUND\n";
        vfs.close();
        return;
    }

    // --- STEP 5: Read content of s1 and s2 ---
    std::string combined;
    if (f1.file_size > 0 && f1.direct1 > 0) {
        vfs.seekg(dataBlockOffset(f1.direct1));
        std::vector<char> buf1(f1.file_size);
        vfs.read(buf1.data(), f1.file_size);
        combined.append(buf1.begin(), buf1.end());
    }

    if (f2.file_size > 0 && f2.direct1 > 0) {
        vfs.seekg(dataBlockOffset(f2.direct1));
        std::vector<char> buf2(f2.file_size);
        vfs.read(buf2.data(), f2.file_size);
        combined.append(buf2.begin(), buf2.end());
    }

    vfs.close();

    // --- STEP 6: Check destination existence ---
    if (directoryContains(parentInodeId, s3)) {
        std::cerr << "EXIST\n";
        return;
    }

    // --- STEP 7: Create new file s3 ---
    int newInodeId = allocateFreeInode();
    if (newInodeId == -1) {
        std::cerr << "NO SPACE\n";
        return;
    }

    Inode newFile{};
    newFile.id = newInodeId;
    newFile.is_directory = false;
    newFile.references = 1;
    newFile.file_size = static_cast<int>(combined.size());

    if (!combined.empty()) {
        int newBlock = allocateFreeDataBlock();
        if (newBlock == -1) {
            std::cerr << "NO SPACE\n";
            return;
        }

        newFile.direct1 = newBlock;
        std::fstream fs(filename_, std::ios::in | std::ios::out | std::ios::binary);
        fs.seekp(dataBlockOffset(newBlock));
        fs.write(combined.c_str(), combined.size());
        fs.close();
    }

    writeInode(newInodeId, newFile);

    // --- STEP 8: Add directory entry ---
    DirectoryItem newItem{};
    newItem.inode = newInodeId;
    std::strncpy(newItem.item_name, s3.c_str(), MAX_NAME_LENGTH);
    newItem.item_name[MAX_NAME_LENGTH] = '\0';

    std::fstream parentFile(filename_, std::ios::in | std::ios::out | std::ios::binary);
    long long offset = dataBlockOffset(parent.direct1) + parent.file_size;
    parentFile.seekp(offset);
    parentFile.write(reinterpret_cast<char*>(&newItem), sizeof(DirectoryItem));
    parentFile.close();

    parent.file_size += sizeof(DirectoryItem);
    writeInode(parentInodeId, parent);

    std::cout << "OK\n";
}

// -------------------------------------------------
// add
// -------------------------------------------------
// Appends the content of file s2 to file s1
// within the same directory of the virtual filesystem.
// -------------------------------------------------
void FileSystem::add(const std::string& s1, const std::string& s2) {
    const int parentInodeId = currentDirInode_;

    // --- STEP 1: Validate input ---
    if (s1.empty() || s2.empty()) {
        std::cerr << "INVALID INPUT\n";
        return;
    }

    Inode parent = readInode(parentInodeId);
    if (!parent.is_directory) {
        std::cerr << "PATH NOT FOUND\n";
        return;
    }

    std::ifstream vfs(filename_, std::ios::binary);
    if (!vfs.is_open()) {
        std::cerr << "PATH NOT FOUND\n";
        return;
    }

    // --- STEP 2: Locate s1 ---
    vfs.seekg(dataBlockOffset(parent.direct1));
    DirectoryItem item{};
    int entries = parent.file_size / sizeof(DirectoryItem);
    int inode1 = -1;

    for (int i = 0; i < entries; ++i) {
        vfs.read(reinterpret_cast<char*>(&item), sizeof(DirectoryItem));
        if (std::string(item.item_name) == s1) {
            inode1 = item.inode;
            break;
        }
    }

    if (inode1 == -1) {
        std::cerr << "FILE NOT FOUND\n";
        vfs.close();
        return;
    }

    Inode f1 = readInode(inode1);
    if (f1.is_directory) {
        std::cerr << "FILE NOT FOUND\n";
        vfs.close();
        return;
    }

    // --- STEP 3: Locate s2 ---
    vfs.clear();
    vfs.seekg(dataBlockOffset(parent.direct1));
    int inode2 = -1;

    for (int i = 0; i < entries; ++i) {
        vfs.read(reinterpret_cast<char*>(&item), sizeof(DirectoryItem));
        if (std::string(item.item_name) == s2) {
            inode2 = item.inode;
            break;
        }
    }

    if (inode2 == -1) {
        std::cerr << "FILE NOT FOUND\n";
        vfs.close();
        return;
    }

    Inode f2 = readInode(inode2);
    if (f2.is_directory) {
        std::cerr << "FILE NOT FOUND\n";
        vfs.close();
        return;
    }

    // --- STEP 4: Read s2 content ---
    std::string content2;
    if (f2.file_size > 0 && f2.direct1 > 0) {
        vfs.seekg(dataBlockOffset(f2.direct1));
        std::vector<char> buf2(f2.file_size);
        vfs.read(buf2.data(), f2.file_size);
        content2.assign(buf2.begin(), buf2.end());
    }
    vfs.close();

    // --- STEP 5: Read s1 content ---
    std::string content1;
    if (f1.file_size > 0 && f1.direct1 > 0) {
        std::ifstream fs(filename_, std::ios::binary);
        fs.seekg(dataBlockOffset(f1.direct1));
        std::vector<char> buf1(f1.file_size);
        fs.read(buf1.data(), f1.file_size);
        fs.close();
        content1.assign(buf1.begin(), buf1.end());
    }

    // --- STEP 6: Combine and write back ---
    std::string combined = content1 + content2;

    int newBlock = f1.direct1;
    if (newBlock == 0) {
        newBlock = allocateFreeDataBlock();
        if (newBlock == -1) {
            std::cerr << "NO SPACE\n";
            return;
        }
        f1.direct1 = newBlock;
    }

    std::fstream fs(filename_, std::ios::in | std::ios::out | std::ios::binary);
    fs.seekp(dataBlockOffset(newBlock));
    fs.write(combined.c_str(), combined.size());
    fs.close();

    f1.file_size = static_cast<int>(combined.size());
    writeInode(inode1, f1);

    std::cout << "OK\n";
}
