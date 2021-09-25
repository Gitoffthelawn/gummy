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

constexpr const char* fname = "/tmp/gummy";

int main(int argc, char **argv)
{
    if (argc == 1 || strcmp(argv[1], "start") != 0) {
        cout << "Gummyd v0.1\n";
        cout << "Type: 'gummyd start'\n";
        return 0;
    }

    cout << "gummyd started\n";

    mkfifo(fname, S_IFIFO|0640);

    while (1) {
        int fd = open(fname, O_RDONLY);
        char rdbuf[255];

        int len = read(fd, rdbuf, sizeof(rdbuf));
        rdbuf[len] = '\0';

        cout << "Received string: " << rdbuf << " of length: " << len <<  '\n';

        if (strcmp(rdbuf, "stop") == 0) {
            close(fd);
            break;
        }
    }

    cout << "gummyd stopping\n";
    return 0;
}
