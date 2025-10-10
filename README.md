# Virtual Filesystem

A lightweight virtual filesystem implemented in **C++** as part of the *Operating Systems Fundamentals* course at the University of West Bohemia.

---

## 🧠 Project Overview
This project simulates the structure of a real filesystem inside a single binary file (`myfs.dat`).  
It demonstrates core OS concepts such as:
- superblock and metadata management,
- inode tables and directory structures,
- virtual data clusters,
- formatted disk initialization.

---

## 🧩 Current Features
✅ Create and format a virtual filesystem  
✅ Write metadata (superblock, bitmaps, inode table)  
✅ Create the root directory (inode 0)  
✅ Read and print the superblock contents

---

## ⚙️ Build and Run
**Requirements:**
- Visual Studio 2022 (C++ workload installed)
- Windows 10 or later

**How to run:**
1. Clone the repository:
   ```bash
   git clone https://github.com/<your-username>/ZOS_VirtualFilesystem.git
