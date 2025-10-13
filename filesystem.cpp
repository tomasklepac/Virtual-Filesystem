#define _CRT_SECURE_NO_WARNINGS
#include "filesystem.h"
#include <cstring>      // memset, strcpy
#include <vector>       // std::vector
#include <filesystem>   // resize_file

// Creates the virtual disk file, writes Superblock, bitmaps and inode table
bool FileSystem::format(int sizeMB) {
    std::ofstream file(filename_, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        std::cerr << "CANNOT CREATE FILE" << std::endl;
        return false;
    }

    // --- Calculate total size ---
    long long totalBytes = static_cast<long long>(sizeMB) * BYTES_PER_MB;

    // --- Superblock setup ---
    Superblock sb{};
    std::memset(&sb, 0, sizeof(Superblock));

    std::strcpy(sb.signature, "klepac");
    std::strcpy(sb.volume_descriptor, "ZOS_FS_2025");
    sb.disk_size = static_cast<int32_t>(totalBytes);
    sb.cluster_size = CLUSTER_SIZE;
    sb.cluster_count = sb.disk_size / sb.cluster_size;

    // Layout addresses
    sb.bitmapi_start_adress = sizeof(Superblock);
    sb.bitmap_start_adress = sb.bitmapi_start_adress + INODE_BITMAP_SIZE;
    sb.inode_start_adress = sb.bitmap_start_adress + DATA_BITMAP_SIZE;
    sb.data_start_adress = sb.inode_start_adress + INODE_TABLE_SIZE;

    // --- Write Superblock ---
    file.write(reinterpret_cast<char*>(&sb), sizeof(Superblock));

    // --- Write empty bitmaps ---
    const int bitmapInodesSize = INODE_BITMAP_SIZE;
    const int bitmapDataSize = DATA_BITMAP_SIZE;
    std::vector<char> emptyInodeBitmap(bitmapInodesSize, 0);
    std::vector<char> emptyDataBitmap(bitmapDataSize, 0);
    emptyInodeBitmap[0] = 1; // root inode reserved
	emptyDataBitmap[0] = 1; // root directory block reserved

    file.write(emptyInodeBitmap.data(), bitmapInodesSize);
    file.write(emptyDataBitmap.data(), bitmapDataSize);

    // --- Create inode table ---
    const int inodeTableSize = INODE_TABLE_SIZE; // 4 KB for inode table
    const int inodeCount = inodeTableSize / sizeof(Inode);

    std::vector<Inode> inodeTable(inodeCount);
    std::memset(inodeTable.data(), 0, inodeTable.size() * sizeof(Inode));

    // Root inode setup (inode 0)
    inodeTable[0].id = 0;
    inodeTable[0].is_directory = true;
    inodeTable[0].references = 1;
	inodeTable[0].file_size = sizeof(DirectoryItem); // size of one directory item
	inodeTable[0].direct1 = 0; // first data block (block 0)

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

// Prints the superblock information to the console
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

// Reads and returns the superblock from the filesystem file
Superblock FileSystem::readSuperblock() {
    std::ifstream file(filename_, std::ios::binary);
    Superblock sb{};
    if (!file.is_open()) {
        std::cerr << "Cannot open filesystem file: " << filename_ << std::endl;
		return sb; // return empty superblock
    }
    file.read(reinterpret_cast<char*>(&sb), sizeof(Superblock));
    return sb;
}

// Reads a specific inode by its ID from the inode table
Inode FileSystem::readInode(int inodeId) {
	// Load superblock to get inode table start address
    Superblock sb = readSuperblock();

	// Open the filesystem file
    std::ifstream file(filename_, std::ios::binary);
	Inode inode{};  // Struct to hold the read inode

    if (!file.is_open()) {
        std::cerr << "Cannot open filesystem file for readInode()." << std::endl;
        return inode;
    }

	// Count offset to the desired inode
    long long offset = static_cast<long long>(sb.inode_start_adress)
        + static_cast<long long>(inodeId) * static_cast<long long>(sizeof(Inode));

	// Skip to the inode position
    file.seekg(offset);

	// Load the inode data
    file.read(reinterpret_cast<char*>(&inode), sizeof(Inode));

    return inode;
}

// Writes a specific inode by its ID to the inode table
void FileSystem::writeInode(int inodeId, const Inode& inode) {
    // Load superblock to get inode table start address
    Superblock sb = readSuperblock();

    // Open the filesystem file
    std::fstream file(filename_, std::ios::in | std::ios::out | std::ios::binary);

    if (!file.is_open()) {
        std::cerr << "Cannot open filesystem file for writeInode()." << std::endl;
        return;
    }

    // Calculate offset to the desired inode
    long long offset = static_cast<long long>(sb.inode_start_adress)
        + static_cast<long long>(inodeId) * static_cast<long long>(sizeof(Inode));

    // Move to the inode position
    file.seekp(offset);

    // Write the inode data
    file.write(reinterpret_cast<const char*>(&inode), sizeof(Inode));
}

// Allocates a free inode, marks it in the bitmap, and returns its ID
int FileSystem::allocateFreeInode() {
    Superblock sb = readSuperblock();

    std::fstream file(filename_, std::ios::in | std::ios::out | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Cannot open filesystem file for allocateFreeInode()." << std::endl;
        return -1;
    }

    // Load inode bitmap
    std::vector<char> bitmap(INODE_BITMAP_SIZE);
    file.seekg(sb.bitmapi_start_adress);
    file.read(bitmap.data(), INODE_BITMAP_SIZE);

    // Find first free inode (value 0)
    for (int i = 0; i < INODE_BITMAP_SIZE; ++i) {
        if (bitmap[i] == 0) {
            bitmap[i] = 1; // mark as used

            // Write updated bitmap back
            file.seekp(sb.bitmapi_start_adress);
            file.write(bitmap.data(), INODE_BITMAP_SIZE);
            file.close();

            return i; // return allocated inode ID
        }
    }

    std::cerr << "No free inodes available." << std::endl;
    file.close();
    return -1;
}

// Allocates a free data block, marks it in the bitmap, and returns its ID
int FileSystem::allocateFreeDataBlock() {
    Superblock sb = readSuperblock();

    // Open filesystem file for reading and writing
    std::fstream file(filename_, std::ios::in | std::ios::out | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Cannot open filesystem file for allocateFreeDataBlock()." << std::endl;
        return -1;
    }

    // Load data bitmap
    std::vector<char> bitmap(DATA_BITMAP_SIZE);
    file.seekg(sb.bitmap_start_adress);
    file.read(bitmap.data(), DATA_BITMAP_SIZE);

    // Find first free data block
    for (int i = 0; i < DATA_BITMAP_SIZE; ++i) {
        if (bitmap[i] == 0) { // 0 = free block
            bitmap[i] = 1; // mark as used

            // Save updated bitmap back to disk
            file.seekp(sb.bitmap_start_adress);
            file.write(bitmap.data(), DATA_BITMAP_SIZE);

            // Return allocated block ID
            return i;
        }
    }

    std::cerr << "No free data blocks available." << std::endl;
    return -1;
}

// Calculates the byte offset of a data block by its ID
long long FileSystem::dataBlockOffset(int blokId) {
    Superblock sb = readSuperblock();
    return static_cast<long long>(sb.data_start_adress)
        + static_cast<long long>(blokId) * static_cast<long long>(sb.cluster_size);
}

// Checks if a directory (by its inode ID) contains an item with the given name
bool FileSystem::directoryContains(int dirInodeId, const std::string& name) {
    // Read the directory inode
    Inode dirInode = readInode(dirInodeId);
    if (!dirInode.is_directory) {
        std::cerr << "Given inode is not a directory." << std::endl;
        return false;
    }

    // Read the directory's data block
    Superblock sb = readSuperblock();
    std::ifstream file(filename_, std::ios::binary);
    file.seekg(dataBlockOffset(dirInode.direct1));

    DirectoryItem item{};
    int entries = dirInode.file_size / sizeof(DirectoryItem);

    for (int i = 0; i < entries; ++i) {
        file.read(reinterpret_cast<char*>(&item), sizeof(DirectoryItem));
        if (std::string(item.item_name) == name) {
            return true; // Name found
        }
    }
    return false; // Name free
}

// Creates a new directory inside the current working directory (root for now)
void FileSystem::mkdir(const std::string& name) {
    const int parentInodeId = currentDirInode_; // Current working directory (root)

    // --- STEP 1: Validate input name ---
    if (name.empty()) {
        std::cerr << "[mkdir] Error: directory name is empty." << std::endl;
        return;
    }

    if (name.size() > 11) { // DirectoryItem::item_name[12] ? 11 usable chars + '\0'
        std::cerr << "[mkdir] Error: directory name too long (max 11 characters)." << std::endl;
        return;
    }

    if (name.find('/') != std::string::npos) {
        std::cerr << "[mkdir] Error: directory name cannot contain '/'." << std::endl;
        return;
    }

    // --- STEP 2: Check if item already exists in parent directory ---
    if (directoryContains(parentInodeId, name)) {
        std::cerr << "[mkdir] Error: an item with this name already exists." << std::endl;
        return;
    }

    // --- STEP 3: Load parent inode (root) ---
    Inode parentInode = readInode(parentInodeId);
    if (!parentInode.is_directory) {
        std::cerr << "[mkdir] Error: current inode is not a directory (filesystem corruption?)." << std::endl;
        return;
    }

    // --- STEP 4: Allocate new inode and data block ---
    int newInodeId = allocateFreeInode();
    int newBlockId = allocateFreeDataBlock();

    if (newInodeId == -1 || newBlockId == -1) {
        std::cerr << "[mkdir] Error: unable to allocate inode or data block." << std::endl;
        return;
    }

    // --- STEP 5: Initialize inode structure for the new directory ---
    Inode newInode{};
    newInode.id = newInodeId;
    newInode.is_directory = true;
    newInode.references = 1;                          // One reference (from parent)
    newInode.file_size = 2 * sizeof(DirectoryItem);   // Entries: "." and ".."
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
        << " for directory '" << name << "'." << std::endl;

    // --- STEP 6: Create '.' and '..' directory entries ---
    DirectoryItem dot{};
    dot.inode = newInodeId;
    std::strcpy(dot.item_name, ".");

    DirectoryItem dotdot{};
    dotdot.inode = parentInodeId;
    std::strcpy(dotdot.item_name, "..");

    std::fstream fs(filename_, std::ios::in | std::ios::out | std::ios::binary);
    if (!fs.is_open()) {
        std::cerr << "[mkdir] Error: cannot open filesystem file to write directory data." << std::endl;
        return;
    }

    fs.seekp(dataBlockOffset(newBlockId));
    fs.write(reinterpret_cast<char*>(&dot), sizeof(DirectoryItem));
    fs.write(reinterpret_cast<char*>(&dotdot), sizeof(DirectoryItem));
    fs.close();

    std::cout << "[mkdir] Initialized directory data with '.' and '..'." << std::endl;

    // --- STEP 7: Add the new directory to its parent ---
    DirectoryItem newEntry{};
    newEntry.inode = newInodeId;
    std::strncpy(newEntry.item_name, name.c_str(), sizeof(newEntry.item_name) - 1);
    newEntry.item_name[sizeof(newEntry.item_name) - 1] = '\0';

    std::fstream fsParent(filename_, std::ios::in | std::ios::out | std::ios::binary);
    if (!fsParent.is_open()) {
        std::cerr << "[mkdir] Error: cannot open filesystem file to update parent directory." << std::endl;
        return;
    }

    long long offset = dataBlockOffset(parentInode.direct1) + parentInode.file_size;
    fsParent.seekp(offset);
    fsParent.write(reinterpret_cast<char*>(&newEntry), sizeof(DirectoryItem));
    fsParent.close();

    parentInode.file_size += sizeof(DirectoryItem);
    writeInode(parentInodeId, parentInode);

    std::cout << "[mkdir] Directory '" << name << "' successfully created and added to parent." << std::endl;
}

// Lists contents of a directory (default: root)
void FileSystem::ls(const std::string& name) {
    int targetInodeId = currentDirInode_; // default = root

    // --- STEP 1: If name is specified, find that directory in root ---
    if (!name.empty()) {
        Inode root = readInode(0);
        Superblock sb = readSuperblock();

        std::ifstream file(filename_, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "[ls] Error: cannot open filesystem file." << std::endl;
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
            std::cerr << "[ls] Error: directory '" << name << "' not found." << std::endl;
            return;
        }
    }

    // --- STEP 2: Load target inode ---
    Inode dirInode = readInode(targetInodeId);
    if (!dirInode.is_directory) {
        std::cerr << "[ls] Error: '" << name << "' is not a directory." << std::endl;
        return;
    }

    // --- STEP 3: Read and print contents ---
    std::ifstream file(filename_, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[ls] Error: cannot open filesystem file." << std::endl;
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
            std::cout << std::endl;
        }
    }

    file.close();
    std::cout << std::endl;
}

// Changes the current working directory
void FileSystem::cd(const std::string& name) {
    // --- STEP 1: Handle special case: go to parent (cd ..) ---
    if (name == "..") {
        Inode current = readInode(currentDirInode_);

        // Read current directory data
        std::ifstream file(filename_, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "[cd] Error: cannot open filesystem file." << std::endl;
            return;
        }

        file.seekg(dataBlockOffset(current.direct1));
        DirectoryItem item{};
        file.read(reinterpret_cast<char*>(&item), sizeof(DirectoryItem)); // "."
        file.read(reinterpret_cast<char*>(&item), sizeof(DirectoryItem)); // ".."

        file.close();

        currentDirInode_ = item.inode; // set to parent
        std::cout << "[cd] Moved to parent directory." << std::endl;
        return;
    }

    // --- STEP 2: Read current directory inode ---
    Inode current = readInode(currentDirInode_);
    if (!current.is_directory) {
        std::cerr << "[cd] Error: current inode is not a directory." << std::endl;
        return;
    }

    // --- STEP 3: Search for target directory ---
    std::ifstream file(filename_, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[cd] Error: cannot open filesystem file." << std::endl;
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
                std::cerr << "[cd] Error: '" << name << "' is not a directory." << std::endl;
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
        std::cerr << "[cd] Error: directory '" << name << "' not found." << std::endl;
        return;
    }

    std::cout << "[cd] Moved into directory '" << name << "' (inode " << currentDirInode_ << ")." << std::endl;
}