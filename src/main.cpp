#include "filesystem.h"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>

// =============================================
// main.cpp
// ---------------------------------------------
// Command-line shell for interacting with the
// virtual filesystem (FileSystem class).
// Supports basic file and directory operations,
// import/export, and batch command execution.
// =============================================

// Helper function to capture the current working path
std::string getCurrentPath(FileSystem& fs) {
    std::ostringstream buffer;
    std::streambuf* oldCout = std::cout.rdbuf(buffer.rdbuf());
    fs.pwd();
    std::cout.rdbuf(oldCout);
    std::string path = buffer.str();

    if (!path.empty() && path.back() == '\n')
        path.pop_back();

    return path;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << (argc > 0 ? argv[0] : "filesystem") << " <filesystem_file>\n";
        return 1;
    }

    // Find bin directory by going up from executable location
    std::string filename;
    std::string arg = std::string(argv[1]);
    
    // Try to find bin/ directory by checking parent directories
    std::string currentPath = ".";
    for (int i = 0; i < 5; i++) {  // Go up max 5 levels
        std::string binPath = currentPath + "/bin/" + arg;
        if (std::filesystem::exists(currentPath + "/bin")) {
            filename = binPath;
            break;
        }
        currentPath = currentPath + "/..";
    }
    
    // If not found, try current directory
    if (filename.empty() || !std::filesystem::exists(filename)) {
        filename = arg;
    }

    FileSystem fs(filename);
    std::string input;

    std::cout << "===== Virtual Filesystem Shell =====\n";
    std::cout << "Type 'help' for a list of commands.\n\n";

    while (true) {
        std::string path = getCurrentPath(fs);
        std::cout << path << "> ";

        if (!std::getline(std::cin, input))
            break;

        std::istringstream iss(input);
        std::string cmd, arg1, arg2, arg3;
        iss >> cmd >> arg1 >> arg2 >> arg3;

        if (cmd.empty()) continue;

        // ---------------- exit ----------------
        if (cmd == "exit") {
            std::cout << "Terminating shell.\n";
            break;
        }

        // ---------------- help ----------------
        else if (cmd == "help") {
            std::cout
                << "\nAvailable commands:\n"
                << " format [MB]          - create new filesystem\n"
                << " mkdir [name]         - create directory\n"
                << " rmdir [name]         - remove empty directory\n"
                << " ls [name]            - list directory contents\n"
                << " cd [name]            - change directory (.. to go up)\n"
                << " pwd                  - print current path\n"
                << " touch [file]         - create empty file\n"
                << " write [file] [text]  - overwrite file content\n"
                << " cat [file]           - show file content\n"
                << " rm [file]            - delete file\n"
                << " cp [src] [dst]       - copy file\n"
                << " mv [src] [dst]       - move or rename file\n"
                << " info [item]          - show file/dir metadata\n"
                << " statfs               - show filesystem stats\n"
                << " incp [host] [vfs]    - import file from host\n"
                << " outcp [vfs] [host]   - export file to host\n"
                << " xcp [f1] [f2] [out]  - concatenate two files\n"
                << " add [f1] [f2]        - append f2 to f1\n"
                << " load [script]        - execute batch commands\n"
                << " exit                 - quit program\n\n";
        }

        // ---------------- format ----------------
        else if (cmd == "format") {
            if (arg1.empty()) std::cerr << "Usage: format [sizeMB]\n";
            else fs.format(std::stoi(arg1));
        }

        // ---------------- directory commands ----------------
        else if (cmd == "mkdir") { if (arg1.empty()) std::cerr << "Usage: mkdir [name]\n"; else fs.mkdir(arg1); }
        else if (cmd == "rmdir") { if (arg1.empty()) std::cerr << "Usage: rmdir [name]\n"; else fs.rmdir(arg1); }
        else if (cmd == "ls") { fs.ls(arg1); }
        else if (cmd == "cd") { if (arg1.empty()) std::cerr << "Usage: cd [name]\n"; else fs.cd(arg1); }
        else if (cmd == "pwd") { fs.pwd(); }

        // ---------------- file commands ----------------
        else if (cmd == "touch") { if (arg1.empty()) std::cerr << "Usage: touch [file]\n"; else fs.touch(arg1); }
        else if (cmd == "cat") { if (arg1.empty()) std::cerr << "Usage: cat [file]\n"; else fs.cat(arg1); }

        else if (cmd == "write") {
            std::string filename, text;
            std::istringstream iss2(input);
            iss2 >> cmd >> filename;
            std::getline(iss2, text);
            if (!text.empty() && text[0] == ' ') text.erase(0, 1);
            if (filename.empty()) std::cerr << "Usage: write [file] [text]\n";
            else fs.write(filename, text);
        }

        else if (cmd == "rm") { if (arg1.empty()) std::cerr << "Usage: rm [file]\n"; else fs.rm(arg1); }
        else if (cmd == "info") { if (arg1.empty()) std::cerr << "Usage: info [item]\n"; else fs.info(arg1); }
        else if (cmd == "statfs") { fs.statfs(); }

        // ---------------- file manipulation ----------------
        else if (cmd == "cp") { if (arg1.empty() || arg2.empty()) std::cerr << "Usage: cp [src] [dst]\n"; else fs.cp(arg1, arg2); }
        else if (cmd == "mv") { if (arg1.empty() || arg2.empty()) std::cerr << "Usage: mv [src] [dst]\n"; else fs.mv(arg1, arg2); }
        else if (cmd == "xcp") { if (arg1.empty() || arg2.empty() || arg3.empty()) std::cerr << "Usage: xcp [f1] [f2] [out]\n"; else fs.xcp(arg1, arg2, arg3); }
        else if (cmd == "add") { if (arg1.empty() || arg2.empty()) std::cerr << "Usage: add [f1] [f2]\n"; else fs.add(arg1, arg2); }

        // ---------------- host integration ----------------
        else if (cmd == "incp") { if (arg1.empty() || arg2.empty()) std::cerr << "Usage: incp [host] [vfs]\n"; else fs.incp(arg1, arg2); }
        else if (cmd == "outcp") { if (arg1.empty() || arg2.empty()) std::cerr << "Usage: outcp [vfs] [host]\n"; else fs.outcp(arg1, arg2); }
        else if (cmd == "load") { if (arg1.empty()) std::cerr << "Usage: load [script]\n"; else fs.load(arg1); }

        // ---------------- fallback ----------------
        else {
            std::cerr << "Unknown command: " << cmd << "\n";
        }
    }

    return 0;
}
