#include "filesystem.h"

int main() {
    FileSystem fs("myfs.dat");
    fs.format(10);

    // Vytvoøíme pár adresáøù
    fs.mkdir("test");
    fs.mkdir("docs");
    fs.mkdir("games");

    std::cout << "Filesystem initialized and directories created." << std::endl;
    return 0;
}

