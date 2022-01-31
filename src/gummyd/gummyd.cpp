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

#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <fstream>

void apply_options(const Message &opts, Xorg &xorg, scrctl::Brt &brtctl, scrctl::Temp &tempctl)
{
	bool notify_temp = false;

	// Non-screen specific options
	{
		if (opts.temp_day_k != -1) {
			cfg.temp_auto_high = opts.temp_day_k;
			notify_temp = true;
		}

		if (opts.temp_night_k != -1) {
			cfg.temp_auto_low = opts.temp_night_k;
			notify_temp = true;
		}

		if (!opts.sunrise_time.empty()) {
			cfg.temp_auto_sunrise = opts.sunrise_time;
			notify_temp = true;
		}

		if (!opts.sunset_time.empty()) {
			cfg.temp_auto_sunset = opts.sunset_time;
			notify_temp = true;
		}

		if (opts.temp_adaptation_time != -1) {
			cfg.temp_auto_speed = opts.temp_adaptation_time;
			notify_temp = true;
		}
	}

	int start = 0;
	int end = xorg.scr_count() - 1;

	if (opts.scr_no != -1) {
		if (opts.scr_no > end) {
			syslog(LOG_ERR, "Screen %d not available", opts.scr_no);
			return;
		}
		start = end = opts.scr_no;
	} else {
		if (opts.temp_k != -1) {
			cfg.temp_auto = false;
			notify_temp = true;
		} else if (opts.temp_auto != -1) {
			cfg.temp_auto = bool(opts.temp_auto);
			notify_temp = true;
		}
	}

	for (int i = start; i <= end; ++i) {

		if (opts.brt_auto != -1) {
			cfg.screens[i].brt_auto = bool(opts.brt_auto);
			brtctl.monitors[i].notify();
		}

		if (opts.brt_perc != -1) {
			cfg.screens[i].brt_auto = false;
			brtctl.monitors[i].notify();

			if (&brtctl.backlights[i]) {
				cfg.screens[i].brt_step = brt_steps_max;
				brtctl.backlights[i].set(opts.brt_perc * 255 / 100);
			} else {
				cfg.screens[i].brt_step = int(remap(opts.brt_perc, 0, 100, 0, brt_steps_max));
			}
		}

		if (opts.brt_auto_min != -1) {
			cfg.screens[i].brt_auto_min = int(remap(opts.brt_auto_min, 0, 100, 0, brt_steps_max));
		}

		if (opts.brt_auto_max != -1) {
			cfg.screens[i].brt_auto_max = int(remap(opts.brt_auto_max, 0, 100, 0, brt_steps_max));
		}

		if (opts.brt_auto_offset != -1) {
			cfg.screens[i].brt_auto_offset = int(remap(opts.brt_auto_offset, 0, 100, 0, brt_steps_max));
		}

		if (opts.brt_auto_speed != -1) {
			cfg.screens[i].brt_auto_speed = opts.brt_auto_speed;
		}

		if (opts.screenshot_rate_ms != -1) {
			cfg.screens[i].brt_auto_polling_rate = opts.screenshot_rate_ms;
		}

		if (opts.temp_k != -1) {
			cfg.screens[i].temp_step = int(remap(opts.temp_k, temp_k_min, temp_k_max, 0, temp_steps_max));
			cfg.screens[i].temp_auto = false;
		} else if (opts.temp_auto != -1) {
			cfg.screens[i].temp_auto = bool(opts.temp_auto);
			if (opts.temp_auto == 1) {
				cfg.screens[i].temp_step = tempctl.current_step();
			}
		}

		xorg.set_gamma(
		    i,
		    cfg.screens[i].brt_step,
		    cfg.screens[i].temp_step
		);
	}

	if (notify_temp)
		tempctl.notify();
}

int message_loop(Xorg &xorg, scrctl::Brt &brtctl, scrctl::Temp &tempctl)
{
	if (mkfifo(fifo_name, S_IFIFO|0640) == 1) {
		syslog(LOG_ERR, "unable to make fifo (err %d), aborting\n", errno);
		return 1;
	}

	while (1) {

		std::ifstream fs(fifo_name);

		if (fs.fail()) {
			syslog(LOG_ERR, "unable to open fifo, aborting\n");
			return 1;
		}

		std::ostringstream ss;
		ss << fs.rdbuf();
		const std::string s(ss.str());

		if (s == "stop") {
			return 0;
		}

		apply_options(Message(s), xorg, brtctl, tempctl);
		cfg.write();
	}

	return 0;
}

int main(int argc, char **argv)
{
	if (argc > 1 && strcmp(argv[1], "-v") == 0) {
		puts(VERSION);
		exit(0);
	}

	openlog("gummyd", LOG_PID, LOG_DAEMON);

	if (int err = set_lock() > 0) {
		syslog(LOG_ERR, "lockfile err %d", err);
		exit(1);
	}

	// Init X API
	Xorg xorg;

	// Init cfg
	cfg.init(xorg.scr_count());

	// Refresh gamma periodically
	scrctl::Gamma_Refresh g(xorg);

	// Control brightness
	scrctl::Brt b(xorg);

	// Control temperature
	scrctl::Temp t(xorg);

	return message_loop(xorg, b, t);
}
