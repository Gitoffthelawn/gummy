/**
* gummy
* Copyright (C) 2022  Francesco Fusco
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "../common/defs.h"
#include "../common/utils.h"
#include "cfg.h"
#include "xorg.h"
#include "screenctl.h"
#include "sysfs.h"

#include <iostream>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <fstream>

int readMessages(ScreenCtl &screenctl)
{
	if (mkfifo(fifo_name, S_IFIFO|0640) == 1) {
		syslog(LOG_ERR, "unable to make fifo (err %d), aborting\n", errno);
		return 1;
	}

	while (1) {

		std::ifstream fs(fifo_name);

		if (!fs.good()) {
			syslog(LOG_ERR, "unable to open fifo, aborting\n");
			return 1;
		}

		std::ostringstream ss;
		ss << fs.rdbuf();
		std::string s(ss.str());

		if (s == "stop") {
			return 0;
		}

		screenctl.applyOptions(s);

		cfg.write();
	}

	return 0;
}

int main(int argc, char **argv)
{
	if (argc > 1 && strcmp(argv[1], "-v") == 0) {
		std::cout << VERSION << '\n';
		std::exit(0);
	}

	openlog("gummyd", LOG_PID, LOG_DAEMON);

	if (int err = set_lock() > 0) {
		syslog(LOG_ERR, "lockfile err %d", err);
		std::exit(1);
	}

	Xorg xorg;

	cfg.init(xorg.screenCount());

	ScreenCtl screenctl(&xorg);

	return readMessages(screenctl);
}
