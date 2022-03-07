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

#ifndef CFG_H
#define CFG_H

#include "json.hpp"
#include "../common/defs.h"

using json = nlohmann::json;
enum Brt_mode { MANUAL, SCREENSHOT, ALS };
struct Config
{
	struct Screen
	{
		Screen();
		Screen(
		    Brt_mode brt_mode,
		    int brt_auto_min,
		    int brt_auto_max,
		    int brt_auto_offset,
		    int brt_auto_speed,
		    int brt_auto_threshold,
		    int brt_auto_polling_rate,
		    int brt_step,
		    bool temp_auto,
		    int temp_step
		);
		Brt_mode brt_mode;
		int brt_auto_min;
		int brt_auto_max;
		int brt_auto_offset;
		int brt_auto_speed; // ms
		int brt_auto_threshold;
		int brt_auto_polling_rate; // ms
		int brt_step;
		bool temp_auto;
		int temp_step;
	};

	Config();
	const std::string _path;
	int brt_auto_fps;
	int als_polling_rate; // ms
	bool temp_auto;
	int temp_auto_fps;
	int temp_auto_speed;
	int temp_auto_high;
	int temp_auto_low;
	std::string temp_auto_sunrise;
	std::string temp_auto_sunset;
	std::vector<Screen> screens;

	std::string path();
	void init(size_t);
	void read();
	void from_json(const json &);
	void write();
	json to_json();
};

json json_sanitize(const json&);
json screen_to_json(const Config::Screen &s);

extern Config cfg;

struct Message
{
	Message(const std::string &json);
	int scr_no               = -1;
	int brt_perc             = -1;
	int brt_mode             = -1;
	int brt_auto_min         = -1;
	int brt_auto_max         = -1;
	int brt_auto_offset      = -1;
	int brt_auto_speed       = -1;
	int screenshot_rate_ms   = -1;
	int als_poll_rate_ms     = -1;
	int temp_auto            = -1;
	int temp_k               = -1;
	int temp_day_k           = -1;
	int temp_night_k         = -1;
	int temp_adaptation_time = -1;
	std::string sunrise_time;
	std::string sunset_time;
};

#endif // CFG_H
