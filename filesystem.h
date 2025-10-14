#pragma once
#include <string>
#include <fstream>
#include <iostream>
#include "structures.h"

// Class representing our virtual file system
class FileSystem {
public:
    // Constructor that saves the filename (i.e., "disk")
    explicit FileSystem(std::string filename) : filename_(std::move(filename)) {}

    // Create a new empty file system
    bool format(int sizeMB);

    // Print the superblock information
    void printSuperblock();

    // Create a new directory inside the current working directory
    void mkdir(const std::string& name);

    // List contents of the current directory (root for now)
    void ls(const std::string& name = "");

	// Change current working directory
	void cd(const std::string& name);

	// Print the current working directory path
	void pwd();

	// Create a new empty file inside the current directory
	void touch(const std::string& name);

	// Display the contents of a file
	void cat(const std::string& name);

	// Write data to a file (overwrite existing content)
	void write(const std::string& name, const std::string& content);

	// Delete a file or empty directory
	void rm(const std::string& name);

private:
    // --- Filesystem layout constants ---
    // All values are in bytes unless stated otherwise.
    static constexpr int CLUSTER_SIZE = 1024;             // 1 KB per cluster (data block)
    static constexpr int INODE_BITMAP_SIZE = 128;              // 128 bytes => 128 inodes max
    static constexpr int DATA_BITMAP_SIZE = 128;              // 128 bytes => 128 data blocks max
    static constexpr int INODE_TABLE_SIZE = 4096;             // 4 KB reserved for inode table
    static constexpr long long BYTES_PER_MB = 1024LL * 1024LL;  // Bytes in one MB

	int currentDirInode_ = 0; // Current working directory inode (default = root)

    // --- Internal state ---
    std::string filename_;  // Name of the binary file (e.g., "myfs.dat")

    // --- Core file operations ---
    // Read the superblock from disk
    Superblock readSuperblock();

    // Read a specific inode by its ID
    Inode readInode(int inodeId);

    // Write a specific inode by its ID
    void writeInode(int inodeId, const Inode& inode);

    // --- Allocation helpers ---
    // Find a free inode, mark it used, and return its ID
    int allocateFreeInode();

    // Find a free data block, mark it used, and return its ID
    int allocateFreeDataBlock();

    // Calculate the byte offset of a data block by its ID
    long long dataBlockOffset(int blockId);

    // Check if a directory contains an item with the given name
    bool directoryContains(int dirInodeId, const std::string& name);

	// Return the parent inode ID of a directory
	int getParentInodeId(int dirInodeId);

	// In parent directory, find the name of the entry that points to childInodeId
	std::string findNameInParent(int parentInodeId, int childInodeId);
};
