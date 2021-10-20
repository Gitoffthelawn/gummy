#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <string>
#include <sstream>
#include "../gummyd/defs.h"

using std::cout;
using std::cin;

int main(int argc, char **argv)
{
	if (argc == 1) {
		cout << "0.1\n";
		return 0;
	}

	if (strcmp(argv[1], "start") == 0) {
		pid_t pid = fork();
		if (pid == 0)
			execv("./gummyd", argv);
		else
			return 0;
	}

	int fd = open(fifo_name, O_CREAT|O_WRONLY);

	std::ostringstream ss;
	for (int i = 1; i < argc; ++i)
		ss << argv[i] << ' ';
	std::string s(ss.str());

	write(fd, s.c_str(), s.size() - 1);
	close(fd);

	return 0;
}
