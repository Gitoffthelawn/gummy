#include <iostream>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <plog/Init.h>
#include <plog/Appenders/ColorConsoleAppender.h>
#include <plog/Appenders/RollingFileAppender.h>

#include "cfg.h"
#include "utils.h"
#include "xorg.h"

using std::cout;
using std::cin;

constexpr const char* fname = "/tmp/gummy";

int main(int argc, char **argv)
{
	if (argc > 1 && strcmp(argv[1], "-v") == 0) {
		cout << "v0.1\n";
		return 0;
	}

	static plog::RollingFileAppender<plog::TxtFormatter> f("gummy.log", 1024 * 1024 * 5, 1);
	static plog::ColorConsoleAppender<plog::TxtFormatter> c;
	plog::init(plog::Severity(plog::debug), &c);
	const auto logger = plog::get();
	logger->addAppender(&f);
	config::read();
	logger->setMaxSeverity(plog::Severity(cfg["log_level"].get<int>()));

	XCB xcb_conn;

	mkfifo(fname, S_IFIFO|0640);

	while (1) {
		int fd = open(fname, O_RDONLY);
		char rdbuf[255];

		int len = read(fd, rdbuf, sizeof(rdbuf));
		rdbuf[len] = '\0';

		if (strcmp(rdbuf, "stop") == 0) {
			close(fd);
			break;
		}

		std::string in (rdbuf);
		size_t space_pos = in.find(' ');

		if (space_pos > in.size())
			continue;

		std::string opt = in.substr(0, space_pos);
		std::string val = in.substr(space_pos + 1, in.size());
		val = val.substr(0, val.find(' '));

		cout << "opt: " << opt << " val: " << val << '\n';

		if (opt == "-b") {
			cout << "Setting brightness to " << val << "%\n";
		}
	}

	cout << "gummyd stopping\n";
	return 0;
}
