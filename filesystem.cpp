#define _CRT_SECURE_NO_WARNINGS
#include "filesystem.h"
#include <cstring>      // memset, strcpy
#include <vector>       // std::vector
#include <filesystem>   // resize_file

// Implementation of FileSystem::format()
// Creates the virtual disk file, writes Superblock, bitmaps and inode table
bool FileSystem::format(int sizeMB) {
    std::ofstream file(filename_, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        std::cerr << "CANNOT CREATE FILE" << std::endl;
        return false;
    }

    // --- Calculate total size ---
    long long totalBytes = static_cast<long long>(sizeMB) * 1024 * 1024;

    // --- Superblock setup ---
    Superblock sb{};
    std::memset(&sb, 0, sizeof(Superblock));

    std::strcpy(sb.signature, "klepac");
    std::strcpy(sb.volume_descriptor, "ZOS_FS_2025");
    sb.disk_size = static_cast<int32_t>(totalBytes);
    sb.cluster_size = 1024;
    sb.cluster_count = sb.disk_size / sb.cluster_size;

    // Layout addresses
    sb.bitmapi_start_adress = sizeof(Superblock);
    sb.bitmap_start_adress = sb.bitmapi_start_adress + 128;
    sb.inode_start_adress = sb.bitmap_start_adress + 128;
    sb.data_start_adress = sb.inode_start_adress + 4096;

    // --- Write Superblock ---
    file.write(reinterpret_cast<char*>(&sb), sizeof(Superblock));

    // --- Write empty bitmaps ---
    const int bitmapInodesSize = 128;
    const int bitmapDataSize = 128;
    std::vector<char> emptyInodeBitmap(bitmapInodesSize, 0);
    std::vector<char> emptyDataBitmap(bitmapDataSize, 0);
    emptyInodeBitmap[0] = 1; // root inode reserved

    file.write(emptyInodeBitmap.data(), bitmapInodesSize);
    file.write(emptyDataBitmap.data(), bitmapDataSize);

    // --- Create inode table ---
    const int inodeTableSize = 4096; // 4 KB for inode table
    const int inodeCount = inodeTableSize / sizeof(Inode);

    std::vector<Inode> inodeTable(inodeCount);
    std::memset(inodeTable.data(), 0, inodeTable.size() * sizeof(Inode));

    // Root inode setup (inode 0)
    inodeTable[0].id = 0;
    inodeTable[0].is_directory = true;
    inodeTable[0].references = 1;
    inodeTable[0].file_size = sizeof(DirectoryItem); // bude mít 1 položku "."
    inodeTable[0].direct1 = 0; // první datový blok

    // --- Write inode table to disk ---
    file.write(reinterpret_cast<char*>(inodeTable.data()), inodeTableSize);

    // --- Create root directory data block ---
    DirectoryItem rootDir{};
    rootDir.inode = 0;
    std::strcpy(rootDir.item_name, ".");

    // Move file pointer to start of data area
    file.seekp(sb.data_start_adress);

    // Write root directory block
    file.write(reinterpret_cast<char*>(&rootDir), sizeof(DirectoryItem));

    // --- Safely expand file to total size ---
    file.close(); // close before resizing

    try {
        std::filesystem::resize_file(filename_, totalBytes);
    }
    catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error expanding file: " << e.what() << std::endl;
        return false;
    }

    std::cout << "Filesystem formatted (" << sizeMB << " MB)" << std::endl;
    std::cout << "Root directory (inode 0) created." << std::endl;
    return true;
}

void FileSystem::printSuperblock() {
    std::ifstream file(filename_, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Cannot open filesystem file: " << filename_ << std::endl;
        return;
    }

    Superblock sb{};
    file.read(reinterpret_cast<char*>(&sb), sizeof(Superblock));

    std::cout << "----- Superblock Information -----" << std::endl;
    std::cout << "Signature: " << sb.signature << std::endl;
    std::cout << "Volume descriptor: " << sb.volume_descriptor << std::endl;
    std::cout << "Disk size: " << sb.disk_size << " bytes" << std::endl;
    std::cout << "Cluster size: " << sb.cluster_size << " bytes" << std::endl;
    std::cout << "Cluster count: " << sb.cluster_count << std::endl;
    std::cout << "Inode bitmap start: " << sb.bitmapi_start_adress << " bytes" << std::endl;
    std::cout << "Data bitmap start: " << sb.bitmap_start_adress << " bytes" << std::endl;
    std::cout << "Inode table start: " << sb.inode_start_adress << " bytes" << std::endl;
    std::cout << "Data area start: " << sb.data_start_adress << " bytes" << std::endl;
    std::cout << "----------------------------------" << std::endl;

    file.close();
}

