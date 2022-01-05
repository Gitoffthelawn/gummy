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
#include <fstream>
#include <iostream>
#include <syslog.h>

Config cfg;

Config::Config()
    : _path(path()),
      brt_auto_fps(60),
      temp_auto(false),
      temp_auto_fps(45),
      temp_auto_speed(60),
      temp_auto_high(temp_k_min),
      temp_auto_low(3400),
      temp_auto_sunrise("06:00"),
      temp_auto_sunset("16:00")
{
}

Config::Screen::Screen()
    : brt_auto(false),
      brt_auto_min(brt_steps_max / 2),
      brt_auto_max(brt_steps_max),
      brt_auto_offset(0),
      brt_auto_speed(1000),
      brt_auto_threshold(8),
      brt_auto_polling_rate(1000),
      brt_step(brt_steps_max),
      temp_auto(false),
      temp_step(0)
{
}

void Config::init(const int detected_screens)
{
	cfg.read();

	int new_screens = detected_screens - cfg.screens.size();
	while (new_screens--)
		screens.emplace_back(Screen());

	cfg.write();
}

void Config::read()
{
	std::ifstream fs(_path, std::fstream::in | std::fstream::app);
\
	if (fs.fail()) {
		syslog(LOG_ERR, "Unable to open config\n");
		return;
	}

	fs.seekg(0, std::ios::end);

	if (fs.tellg() == 0) {
		Config::write();
		return;
	}

	fs.seekg(0);

	json j;

	try {
		fs >> j;
	} catch (json::exception &e) {
		syslog(LOG_ERR, "%s\n", e.what());
		Config::write();
		return;
	}

	from_json(j);
}

void Config::write()
{
	std::ofstream fs(_path);

	if (fs.fail()) {
		syslog(LOG_ERR, "Unable to open config\n");
		return;
	}

	try {
		fs << std::setw(4) << cfg.to_json();
	} catch (json::exception &e) {
		syslog(LOG_ERR, "%s\n", e.what());
		return;
	}
}

void Config::from_json(const json &in)
{
	brt_auto_fps      = in["brt_auto_fps"];
	temp_auto         = in["temp_auto"];
	temp_auto_fps     = in["temp_auto_fps"];
	temp_auto_speed   = in["temp_auto_speed"];
	temp_auto_sunrise = in["temp_auto_sunrise"];
	temp_auto_sunset  = in["temp_auto_sunset"];
	temp_auto_high    = in["temp_auto_high"];
	temp_auto_low     = in["temp_auto_low"];
	screens.clear();
	for (size_t i = 0; i < in["screens"].size(); ++i) {
		screens.emplace_back(
		    in["screens"][i]["brt_auto"],
		    in["screens"][i]["brt_auto_min"],
		    in["screens"][i]["brt_auto_max"],
		    in["screens"][i]["brt_auto_offset"],
		    in["screens"][i]["brt_auto_speed"],
		    in["screens"][i]["brt_auto_threshold"],
		    in["screens"][i]["brt_auto_polling_rate"],
		    in["screens"][i]["brt_step"],
		    in["screens"][i]["temp_auto"],
		    in["screens"][i]["temp_step"]
		);
	}
}

json Config::to_json()
{
	json ret({
	    {"brt_auto_fps", brt_auto_fps},
	    {"temp_auto", temp_auto},
	    {"temp_auto_fps", temp_auto_fps},
	    {"temp_auto_speed", temp_auto_speed},
	    {"temp_auto_sunrise", temp_auto_sunrise},
	    {"temp_auto_sunset", temp_auto_sunset},
	    {"temp_auto_high", temp_auto_high},
	    {"temp_auto_low", temp_auto_low},
	    {"screens", json::array()}
	});

	for (const auto &s : screens) {
		ret["screens"].emplace_back(json({
		    {"brt_auto", s.brt_auto},
		    {"brt_auto_min", s.brt_auto_min},
		    {"brt_auto_max", s.brt_auto_max},
		    {"brt_auto_offset", s.brt_auto_offset},
		    {"brt_auto_speed", s.brt_auto_speed},
		    {"brt_auto_threshold", s.brt_auto_threshold},
		    {"brt_auto_polling_rate", s.brt_auto_polling_rate},
		    {"brt_step", s.brt_step},
		    {"temp_auto", s.temp_auto},
		    {"temp_step", s.temp_step},
		}));
	}

	return ret;
}

std::string Config::path()
{
	const char *home   = getenv("XDG_CONFIG_HOME");
	const char *format = "/";

	if (!home) {
		format = "/.config/";
		home = getenv("HOME");
	}

	std::ostringstream ss;
	ss << home << format << config_name;
	return ss.str();
}

Config::Screen::Screen(
    bool brt_auto,
    int brt_auto_min,
    int brt_auto_max,
    int brt_auto_offset,
    int brt_auto_speed,
    int brt_auto_threshold,
    int brt_auto_polling_rate,
    int brt_step,
    bool temp_auto,
    int temp_step
    ) : brt_auto(brt_auto),
    brt_auto_min(brt_auto_min),
    brt_auto_max(brt_auto_max),
    brt_auto_offset(brt_auto_offset),
    brt_auto_speed(brt_auto_speed),
    brt_auto_threshold(brt_auto_threshold),
    brt_auto_polling_rate(brt_auto_polling_rate),
    brt_step(brt_step),
    temp_auto(temp_auto),
    temp_step(temp_step)
{
}

Message::Message(const std::string &j)
{
	json msg = json::parse(j);
	scr_no               = msg["scr_no"];
	brt_perc             = msg["brt_perc"];
	brt_auto             = msg["brt_mode"];
	brt_auto_min         = msg["brt_auto_min"];
	brt_auto_max         = msg["brt_auto_max"];
	brt_auto_offset      = msg["brt_auto_offset"];
	brt_auto_speed       = msg["brt_auto_speed"];
	screenshot_rate_ms   = msg["brt_auto_screenshot_rate"];
	temp_k               = msg["temp_k"];
	temp_auto            = msg["temp_mode"];
	temp_day_k           = msg["temp_day_k"];
	temp_night_k         = msg["temp_night_k"];
	sunrise_time         = msg["sunrise_time"];
	sunset_time          = msg["sunset_time"];
	temp_adaptation_time = msg["temp_adaptation_time"];
}
