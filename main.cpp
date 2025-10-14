#include "filesystem.h"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// Helper function to capture the current path as string
std::string getCurrentPath(FileSystem& fs) {
    // Redirect cout temporarily to capture fs.pwd() output
    std::ostringstream buffer;
    std::streambuf* oldCout = std::cout.rdbuf(buffer.rdbuf());
    fs.pwd();
    std::cout.rdbuf(oldCout);
    std::string path = buffer.str();

    // Remove newline from end
    if (!path.empty() && path.back() == '\n')
        path.pop_back();

    return path;
}

int main() {
    FileSystem fs("myfs.dat");
    std::string input, cmd, arg;

    std::cout << "===== Virtual Filesystem Shell =====" << std::endl;
    std::cout << "Type 'help' to see available commands." << std::endl;

    while (true) {
        std::string path = getCurrentPath(fs);
        std::cout << path << "> ";
        std::getline(std::cin, input);

        std::istringstream iss(input);
        std::string cmd, arg1;
        iss >> cmd >> arg1;

        if (cmd.empty()) continue;

        if (cmd == "exit") {
            std::cout << "Exiting filesystem shell." << std::endl;
            break;
        }

        else if (cmd == "help") {
            std::cout << "Available commands:\n"
                << " format [sizeMB]   - create new filesystem\n"
                << " mkdir [name]      - create directory\n"
                << " ls [name]         - list directory contents\n"
                << " cd [name]         - change directory (use .. to go up)\n"
                << " pwd               - show current path\n"
                << " exit              - quit program\n";
        }

        else if (cmd == "format") {
            if (arg1.empty()) {
                std::cerr << "Usage: format [sizeMB]" << std::endl;
            }
            else {
                int size = std::stoi(arg1);
                fs.format(size);
            }
        }

        else if (cmd == "mkdir") {
            if (arg1.empty()) std::cerr << "Usage: mkdir [name]" << std::endl;
            else fs.mkdir(arg1);
        }

        else if (cmd == "ls") {
            fs.ls(arg1);
        }

        else if (cmd == "cd") {
            if (arg1.empty()) std::cerr << "Usage: cd [name]" << std::endl;
            else fs.cd(arg1);
        }

        else if (cmd == "pwd") {
            fs.pwd();
        }

        else if (cmd == "touch") {
            if (arg1.empty()) std::cerr << "Usage: touch [filename]" << std::endl;
            else fs.touch(arg1);
        }

        else if (cmd == "cat") {
            if (arg1.empty()) std::cerr << "Usage: cat [filename]" << std::endl;
            else fs.cat(arg1);
        }

        else if (cmd == "write") {
            std::string filename, text;
            std::istringstream iss(input);
            iss >> cmd >> filename;
            std::getline(iss, text);

            // Remove leading space before text
            if (!text.empty() && text[0] == ' ') text.erase(0, 1);

            if (filename.empty()) {
                std::cerr << "Usage: write [filename] [text]" << std::endl;
            }
            else {
                fs.write(filename, text);
            }
        }

        else if (cmd == "rm") {
            if (arg1.empty()) std::cerr << "Usage: rm [name]" << std::endl;
            else fs.rm(arg1);
        }

        else {
            std::cerr << "Unknown command: " << cmd << std::endl;
        }
    }

    return 0;
}
