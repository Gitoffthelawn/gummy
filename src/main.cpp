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

#include "defs.h"
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

	XCB xcb;

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

		if (opt == "-b") {
			LOGD << "Setting brightness to " << val << "%\n";
			int b = remap(std::stoi(val), 0, 100, 0, brt_steps_max);
			cfg["brt_step"] = b;
			xcb.setGamma(0, b, cfg["temp_step"].get<int>());
			config::write();
		} else if (opt == "-t") {
			LOGD << "Setting temperature to " << val << "%\n";
			int t = remap(std::stoi(val), temp_k_min, temp_k_max, 0, temp_steps_max);
			cfg["temp_step"] = t;
			xcb.setGamma(0, cfg["brt_step"].get<int>(), t);
			config::write();
		}
	}

	cout << "gummyd stopping\n";
	return 0;
}
