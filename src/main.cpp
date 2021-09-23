#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

using std::cout;
using std::cin;

constexpr const char* fname = "gummy_fifo";

int main(int argc, char **argv)
{
    if (argc == 1 || strcmp(argv[1], "start") != 0) {
        cout << "Gummyd v0.1\n";
        cout << "Type: 'gummyd start'\n";
        return 0;
    }

    cout << "gummyd starting\n";

    mknod(fname, S_IFIFO|0640, 0);

    while (1) {
        int fd = open(fname, O_RDONLY);

        char rdbuf[255];

        read(fd, rdbuf, sizeof(rdbuf));
        std::string in = std::string(rdbuf);

        cout << "Received string: " << in;
        cout << ((in == "stop") ? "yes\n" : "no\n");

        if (in == "stop") {
            close(fd);
            break;
        }
    }

    cout << "gummyd stopping\n";
    return 0;
}
