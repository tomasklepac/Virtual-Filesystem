# 🗂️ Virtual Filesystem (VFS)

A lightweight virtual filesystem implemented in **C++** as part of the *Operating Systems Fundamentals (ZOS)* course at the **University of West Bohemia**.  
It simulates the structure and behavior of a simple UNIX-like filesystem inside a single binary file (`myfs.dat`).

---

## 🧠 Overview
The project demonstrates key operating system concepts, including:
- superblock and metadata management,
- inode table and hierarchical directories,
- virtual data block allocation,
- file and directory operations with persistent structure,
- import/export between the virtual and host filesystem.

The goal is to emulate low-level filesystem mechanisms (similar to ext2/MINIX) using direct binary manipulation.

---

## ✨ Features
✅ Filesystem formatting with metadata initialization  
✅ Hierarchical directory management (`mkdir`, `rmdir`, `cd`, `ls`, `pwd`)  
✅ File manipulation (`touch`, `write`, `cat`, `rm`, `info`)  
✅ Advanced operations (`cp`, `mv`, `xcp`, `add`)  
✅ Host filesystem integration (`incp`, `outcp`)  
✅ System statistics via `statfs`  
✅ Script execution via `load`  
✅ Clean modular structure (`core`, `dir`, `file`)  

---

## ⚙️ Build & Run
### Requirements
- **C++17** or newer  
- **Visual Studio 2022** (or any modern C++ compiler)  
- **Windows 10 / Linux (WSL)** compatible  

### Steps
```bash
git clone https://github.com/<your-username>/VirtualFilesystem.git
cd VirtualFilesystem
```

Then compile and run:
```bash
g++ -std=c++17 main.cpp filesystem_core.cpp filesystem_dir.cpp filesystem_file.cpp -o vfs
./vfs
```

---

## 💡 Example Usage
```bash
format 5
mkdir test
cd test
touch notes.txt
write notes.txt Hello_World!
cat notes.txt
ls
info notes.txt
statfs
```

---

## 🧩 Project Structure
```
📁 VirtualFilesystem
 ┣ 📄 main.cpp                 → interactive shell interface
 ┣ 📄 filesystem_core.cpp      → core structures, allocation, format
 ┣ 📄 filesystem_dir.cpp       → directory operations
 ┣ 📄 filesystem_file.cpp      → file operations
 ┣ 📄 filesystem.h             → class definition
 ┣ 📄 structures.h             → core structures (Superblock, Inode)
 ┗ 📄 README.md                → documentation
```

---

## 🧾 Author
**Tomáš Klepač**  
Faculty of Applied Sciences (FAV) – University of West Bohemia  
2025
