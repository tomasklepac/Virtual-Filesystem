#pragma once
#include <string>
#include <fstream>
#include <iostream>
#include "structures.h"

// =============================================
// filesystem.h
// ---------------------------------------------
// Defines the FileSystem class, which implements
// all functionality of the virtual filesystem,
// including directory and file operations,
// metadata management, and data persistence.
// =============================================
class FileSystem {
public:
    // ------------------------------------------
    // Core lifecycle
    // ------------------------------------------
    explicit FileSystem(std::string filename) : filename_(std::move(filename)) {}

    // Formats a new virtual filesystem (creates all metadata structures)
    bool format(int sizeMB);

    // Prints detailed superblock information
    void printSuperblock();

    // ------------------------------------------
    // Directory operations
    // ------------------------------------------
    void mkdir(const std::string& name);                       // Create new directory
    void ls(const std::string& name = "");                     // List directory contents
    void cd(const std::string& name);                          // Change current directory
    void pwd();                                                // Print current working path
    void rmdir(const std::string& name);                       // Remove empty directory

    // ------------------------------------------
    // File operations
    // ------------------------------------------
    void touch(const std::string& name);                       // Create new empty file
    void cat(const std::string& name);                         // Display file content
    void write(const std::string& name, const std::string& content); // Overwrite file
    void rm(const std::string& name);                          // Delete file
    void info(const std::string& name);                        // Show file/directory details
    void statfs();                                             // Show overall filesystem stats

    // ------------------------------------------
    // File manipulation (copy / move / concat)
    // ------------------------------------------
    void cp(const std::string& source, const std::string& destination);      // Copy file inside VFS
    void mv(const std::string& source, const std::string& destination);      // Move or rename file
    void xcp(const std::string& first, const std::string& second, const std::string& result); // Concatenate two files
    void add(const std::string& target, const std::string& source);          // Append file content

    // ------------------------------------------
    // Host integration
    // ------------------------------------------
    void incp(const std::string& sourceHostPath, const std::string& destVfsPath); // Import file from host
    void outcp(const std::string& sourceVfsPath, const std::string& destHostPath); // Export file to host
    void load(const std::string& hostFilePath);                                   // Execute commands from a script file

private:
    // ------------------------------------------
    // Filesystem constants
    // ------------------------------------------
    static constexpr int CLUSTER_SIZE = 1024;                   // 1 KB per data block
    static constexpr int INODE_BITMAP_SIZE = 128;               // 128 B => 128 inodes max
    static constexpr int DATA_BITMAP_SIZE = 128;                // 128 B => 128 data blocks max
    static constexpr int INODE_TABLE_SIZE = 4096;               // 4 KB reserved for inode table
    static constexpr long long BYTES_PER_MB = 1024LL * 1024LL;  // Bytes in one MB
    static constexpr int MAX_NAME_LENGTH = 11;                  // 11 chars (8+3 format)

    // ------------------------------------------
    // State
    // ------------------------------------------
    std::string filename_;      // Name of the filesystem image (e.g. "myfs.dat")
    int currentDirInode_ = 0;   // Current working directory inode ID (root = 0)

    // ------------------------------------------
    // Core helpers
    // ------------------------------------------
    Superblock readSuperblock();                              // Read superblock from disk
    Inode readInode(int inodeId);                             // Read inode by ID
    void writeInode(int inodeId, const Inode& inode);         // Write inode to disk

    // ------------------------------------------
    // Allocation utilities
    // ------------------------------------------
    int allocateFreeInode();                                  // Find and reserve free inode
    int allocateFreeDataBlock();                              // Find and reserve free data block
    long long dataBlockOffset(int blockId);                   // Get byte offset of a data block
    bool directoryContains(int dirInodeId, const std::string& name); // Check if dir contains item

    // ------------------------------------------
    // Directory relationship helpers
    // ------------------------------------------
    int getParentInodeId(int dirInodeId);                     // Get parent inode of directory
    std::string findNameInParent(int parentInodeId, int childInodeId); // Find entry name by child inode
};
