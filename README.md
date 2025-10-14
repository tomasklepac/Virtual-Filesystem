# 🗂️ Virtual Filesystem

A lightweight virtual filesystem implemented in **C++** as part of the *Operating Systems Fundamentals (ZOS)* course at the University of West Bohemia.  
This project simulates the structure and behavior of a real filesystem inside a single binary file (`myfs.dat`).

---

## 🧠 Project Overview
This project demonstrates core operating system concepts such as:
- superblock and metadata management,
- inode table and hierarchical directory structure,
- virtual data blocks (clusters),
- formatted disk initialization and allocation,
- real implementation of filesystem commands (`mkdir`, `ls`, `cd`, `touch`, `write`, `cat`, `rm`, `pwd`).

The goal is to emulate the low-level mechanisms used in real filesystems (e.g., ext2, MINIX), including binary data layout and direct manipulation of structures on disk.

---

## 🧩 Current Features
✅ Create and format a virtual filesystem  
✅ Write metadata (superblock, bitmaps, inode table)  
✅ Create the root directory (inode 0)  
✅ Implement `mkdir` for creating new directories  
✅ Automatic creation of `.` and `..` entries  
✅ Implement `ls` for listing directory contents  
✅ Implement `cd` for directory navigation  
✅ Implement `pwd` for showing the current working path  
✅ Implement `touch` for creating new files  
✅ Implement `write` for writing data into files  
✅ Implement `cat` for displaying file content  
✅ Implement `rm` for removing files or empty directories  
✅ Fully functional interactive user shell with prompt and command handling  
✅ Modular codebase (`filesystem_core.cpp`, `filesystem_dir.cpp`, `filesystem_file.cpp`) for clarity and maintainability  

---

## ⚙️ Build and Run
**Requirements**
- Visual Studio 2022 (C++ workload installed)
- Windows 10 or later

**Steps to build:**
1. Clone the repository:
   ```bash
   git clone https://github.com/<your-username>/VirtualFilesystem.git
