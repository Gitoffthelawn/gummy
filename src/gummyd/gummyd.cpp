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
		cout << "0.1\n";
		return 0;
	}

	if (alreadyRunning()) {
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

	int new_screens = xcb.screensDetected() - cfg["screens"].size();
	if (new_screens > 0) {
		config::addScreenEntries(cfg, new_screens);
	}

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

		std::unordered_map<std::string, int> args;
		args["brt"]    = -1;
		args["temp"]   = -1;
		args["screen"] = -1;

		for (size_t i = 0; i < in.size(); ++i) {
			if (in[i] != '-')
				continue;

			auto space_idx  = in.substr(i, in.size()).find_first_of(' ');
			std::string opt = in.substr(i, space_idx);

			auto start          = i + opt.size() + 1;
			auto next_space_idx = in.substr(start, in.size()).find_first_of(' ');
			std::string val     = in.substr(start, next_space_idx);
			LOGV << "opt: " << opt << " val: " << val;

			if (opt == "-b") {
				args["brt"] = std::stoi(val);
			} else if (opt == "-t") {
				args["temp"] = std::stoi(val);
			} else if (opt == "-s") {
				args["screen"] = std::stoi(val);
			}
		}

		if (args["screen"] == -1) {
			for (int i = 0; i < xcb.screensDetected(); ++i) {
				if (args["brt"] != -1)
					cfg["screens"].at(i)["brt_step"] = int(remap(args["brt"], 0, 100, 0, brt_steps_max));
				if (args["temp"] != -1)
					cfg["screens"].at(i)["temp_step"] = int(remap(args["temp"], temp_k_min, temp_k_max, 0, temp_steps_max));
				xcb.setGamma(i,
				             cfg["screens"].at(i)["brt_step"],
				             cfg["screens"].at(i)["temp_step"]);
			}
		} else {

			if (args["screen"] > xcb.screensDetected() - 1) {
				LOGE << "Screen not available.";
				continue;
			}

			if (args["brt"] != -1)
				cfg["screens"].at(args["screen"])["brt_step"] = int(remap(args["brt"], 0, 100, 0, brt_steps_max));
			if (args["temp"] != -1)
				cfg["screens"].at(args["screen"])["temp_step"] = int(remap(args["temp"], temp_k_min, temp_k_max, 0, temp_steps_max));
			xcb.setGamma(args["screen"],
			        cfg["screens"].at(args["screen"])["brt_step"],
			        cfg["screens"].at(args["screen"])["temp_step"]);
		}

		config::write();
	}

	cout << "gummyd stopping\n";
	return 0;
}
