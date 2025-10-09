#pragma once
#include <cstdint>

const int32_t ID_ITEM_FREE = 0;

// ---------------- Superblock ----------------
// Contains global information about the entire filesystem.
struct Superblock {
    char signature[9];              // author/login
    char volume_descriptor[251];    // short filesystem description
    int32_t disk_size;              // total disk size in bytes
    int32_t cluster_size;           // size of one cluster (e.g., 1024 bytes)
    int32_t cluster_count;          // total number of clusters
    int32_t bitmapi_start_adress;   // offset (in bytes) to the inode bitmap
    int32_t bitmap_start_adress;    // offset (in bytes) to the data bitmap
    int32_t inode_start_adress;     // offset (in bytes) to the inode table
    int32_t data_start_adress;      // offset (in bytes) to the data area
};

// ---------------- Inode ----------------
// Describes a single file or directory (metadata only).
struct Inode {
    int32_t id;              // inode ID (unique identifier)
    bool is_directory;       // true = directory, false = file
    int8_t references;       // number of hard links (references)
    int32_t file_size;       // file size in bytes
    int32_t direct1, direct2, direct3, direct4, direct5; // direct data block addresses
    int32_t indirect1, indirect2; // indirect addresses (for larger files)
};

// ---------------- DirectoryItem ----------------
// Connects a filename to its corresponding inode.
struct DirectoryItem {
    int32_t inode;           // ID of the inode this item refers to
    char item_name[12];      // file or directory name
};
