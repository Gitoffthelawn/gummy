#include <iostream>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <plog/Init.h>
#include <plog/Appenders/RollingFileAppender.h>

#include "../commons/defs.h"
#include "../commons/utils.h"
#include "cfg.h"

#include "xorg.h"
#include "screenctl.h"
#include "sysfs.h"

using std::cout;

void readMessages(Xorg &xorg)
{
	mkfifo(fifo_name, S_IFIFO|0640);

	while (1) {
		int fd = open(fifo_name, O_RDONLY);
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

		std::map<std::string, int> fl;
		fl["brt"]    = -1;
		fl["temp"]   = -1;
		fl["screen"] = -1;

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
				fl["brt"] = std::stoi(val);
			} else if (opt == "-t") {
				fl["temp"] = std::stoi(val);
			} else if (opt == "-s") {
				fl["screen"] = std::stoi(val);
			} else if (opt == "-bm") {
				fl["bm"] = std::stoi(val);
			}
		}

		if (fl["screen"] == -1) {
			for (int i = 0; i < xorg.screenCount(); ++i) {
				if (fl["brt"] != -1)
					cfg["screens"][i]["brt_step"] = int(remap(fl["brt"], 0, 100, 0, brt_steps_max));
				if (fl["temp"] != -1)
					cfg["screens"][i]["temp_step"] = int(remap(fl["temp"], temp_k_min, temp_k_max, 0, temp_steps_max));

				xorg.setGamma();
			}
		} else {

			if (fl["screen"] > xorg.screenCount() - 1) {
				cout << "Screen " << fl["screen"] << " not available.\n";
				continue;
			}

			if (fl["brt"] != -1)
				cfg["screens"][fl["screen"]]["brt_step"] = int(remap(fl["brt"], 0, 100, 0, brt_steps_max));
			if (fl["temp"] != -1)
				cfg["screens"][fl["screen"]]["temp_step"] = int(remap(fl["temp"], temp_k_min, temp_k_max, 0, temp_steps_max));

			xorg.setGamma(fl["screen"],
			        cfg["screens"][fl["screen"]]["brt_step"],
			        cfg["screens"][fl["screen"]]["temp_step"]);
		}

		config::write();
	}
}

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
	plog::init(plog::Severity(plog::debug), &f);

	config::read();
	plog::get()->setMaxSeverity(plog::Severity(cfg["log_level"].get<int>()));

	Xorg xorg;

	int new_screens = xorg.screenCount() - cfg["screens"].size();
	if (new_screens > 0) {
		config::addScreenEntries(cfg, new_screens);
		config::write();
	}

	ScreenCtl screenctl(&xorg);

	readMessages(xorg);

	cout << "gummy stopped\n";
	return 0;
}
