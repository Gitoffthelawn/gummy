#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <string>
#include <sstream>
#include "../commons/defs.h"
#include "../commons/utils.h"

using std::cout;
using std::cin;

int main(int argc, char **argv)
{
	if (argc == 1) {
		cout << "0.1\n";
		return 0;
	}

	bool running = alreadyRunning();

	if (strcmp(argv[1], "status") == 0) {
		cout << (running ? "running\n" : "stopped\n");
		return 0;
	}

	if (strcmp(argv[1], "start") == 0) {
		if (running) {
			cout << "already started.\n";
			return 0;
		}
		cout << "starting gummy\n";
		pid_t pid = fork();
		if (pid == 0) {
			execv("./gummyd", argv);
		}
		return 0;
	}

	if (!running) {
		if (strcmp(argv[1], "stop") == 0) {
			cout << "already stopped.\n";
		} else {
			cout << "needs to be started first.\nType `gummy start`.\n";
		}
		return 0;
	}

	int fd = open(fifo_name, O_CREAT|O_WRONLY, 0600);

	std::ostringstream ss;
	for (int i = 1; i < argc; ++i)
		ss << argv[i] << ' ';
	std::string s(ss.str());

	write(fd, s.c_str(), s.size() - 1);
	close(fd);

	return 0;
}
