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

    // Function to create a new empty file system
    bool format(int sizeMB);

    // Function to print the superblock information
    void printSuperblock();

private:
    // --- Filesystem layout constants ---
    // All values are in bytes unless stated otherwise.
    static constexpr int CLUSTER_SIZE = 1024;  // 1 KB per cluster (data block)
    static constexpr int INODE_BITMAP_SIZE = 128;   // 128 bytes => 128 inodes max
    static constexpr int DATA_BITMAP_SIZE = 128;   // 128 bytes => 128 data blocks max
    static constexpr int INODE_TABLE_SIZE = 4096;  // 4 KB reserved for inode table
	static constexpr long long BYTES_PER_MB = 1024LL * 1024LL; // Bytes in one MB

    // --- Internal state ---
    std::string filename_; // Name of the binary file (e.g., "myfs.dat")

    // --- Core file operations ---
    Superblock readSuperblock();                      // Read the superblock from disk
    Inode readInode(int inodeId);                     // Read a specific inode by its ID
    void writeInode(int inodeId, const Inode& inode); // Write a specific inode by its ID

    // --- Allocation helpers ---
    int allocateFreeInode();                          // Find a free inode, mark it used, return its ID
	int allocateFreeDataBlock();                     // Find a free data block, mark it used, return its ID

	long long dataBlockOffset(int blokId); // Calculate byte offset of a data block by its ID
};
