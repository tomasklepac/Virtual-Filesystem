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
	std::string filename_; // Name of the binary file (e.g., "myfs.dat")
};
