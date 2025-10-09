#include "filesystem.h"

int main() {
    FileSystem fs("myfs.dat");
    fs.format(10);
	fs.printSuperblock();
    return 0;
}
