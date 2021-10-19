#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using std::cout;
using std::cin;

int main(int argc, char **argv)
{
	if (argc == 1) {
		cout << "gummy v0.1\n";
		return 0;
	}

	int fd = open("/tmp/gummy", O_CREAT|O_WRONLY);

	std::string s;
	for (int i = 1; i < argc; ++i) {
		s.append(argv[i]);
		if (i + 1 < argc)
			s += ' ';
	}

	write(fd, s.c_str(), s.size());
	close(fd);

	return 0;
}
