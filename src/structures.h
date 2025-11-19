#pragma once
#include <cstdint>

// =============================================
// structures.h
// ---------------------------------------------
// Defines all core data structures used by the
// virtual filesystem, including the Superblock,
// Inode, and DirectoryItem.
// =============================================

// Constant representing an unused directory entry
constexpr int32_t ID_ITEM_FREE = 0;

// ---------------- Superblock ----------------
// Contains global metadata about the entire filesystem layout.
struct Superblock {
    char signature[9];               // Author or system signature (null-terminated)
    char volume_descriptor[251];     // Short human-readable volume description
    int32_t disk_size;               // Total size of the virtual disk (bytes)
    int32_t cluster_size;            // Size of one cluster (bytes)
    int32_t cluster_count;           // Total number of clusters
    int32_t bitmapi_start_address;   // Byte offset to the inode bitmap
    int32_t bitmap_start_address;    // Byte offset to the data bitmap
    int32_t inode_start_address;     // Byte offset to the inode table
    int32_t data_start_address;      // Byte offset to the data area
};

// ---------------- Inode ----------------
// Describes a single file or directory (its metadata only).
struct Inode {
    int32_t id;               // Inode ID (unique identifier)
    bool is_directory;        // True if this inode represents a directory
    int8_t references;        // Number of hard links referencing this inode
    int32_t file_size;        // File size in bytes (or directory entry count * sizeof(DirectoryItem))
    int32_t direct1, direct2, direct3, direct4, direct5; // Direct data block addresses
    int32_t indirect1, indirect2; // Indirect block addresses (for larger files)
};

// ---------------- DirectoryItem ----------------
// Maps a name to its corresponding inode.
struct DirectoryItem {
    int32_t inode;            // ID of the referenced inode
    char item_name[12];       // File or directory name (null-terminated, max 11 chars)
};
