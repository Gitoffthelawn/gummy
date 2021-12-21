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

void readMessages(ScreenCtl &screenctl)
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

		screenctl.applyOptions(rdbuf);

		config::write();
	}
}

int main(int argc, char **argv)
{
	if (argc > 1 && strcmp(argv[1], "-v") == 0) {
		std::cout << VERSION << '\n';
		std::exit(0);
	}

	openlog("gummyd", LOG_PID, LOG_DAEMON);

	if (alreadyRunning()) {
		syslog(LOG_ERR, "already running");
		std::exit(1);
	}

	config::read();

	Xorg xorg;

	int new_screens = xorg.screenCount() - cfg["screens"].size();
	if (new_screens > 0) {
		config::addScreenEntries(cfg, new_screens);
		config::write();
	}

	ScreenCtl screenctl(&xorg);

	readMessages(screenctl);

	return 0;
}
