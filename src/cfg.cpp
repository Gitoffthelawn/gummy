/**
 * Copyright (C) Francesco Fusco. All rights reserved.
 * License: https://github.com/Fushko/gammy#license
 */

#include "cfg.h"
#include "utils.h"
#include "defs.h"
#include <fstream>
#include <iostream>

json getDefault()
{
	return
	{
		{"brt_auto", true},
		{"brt_fps", 60},
		{"brt_step", brt_steps_max},
		{"brt_min", brt_steps_max / 2},
		{"brt_max", brt_steps_max},
		{"brt_offset", brt_steps_max / 3},
		{"brt_speed", 1000},
		{"brt_threshold", 8},
		{"brt_polling_rate", 100},
		{"brt_extend", false},

		{"temp_auto", false},
		{"temp_fps", 45},
		{"temp_step", 0},
		{"temp_high", temp_k_min},
		{"temp_low", 3400},
		{"temp_speed", 60.0},
		{"temp_sunrise", "06:00"},
		{"temp_sunset", "16:00"},

		{"log_level", plog::warning},
	};
}

json cfg = getDefault();

void config::read()
{
	const auto path = config::getPath();
	LOGV << "Reading from: " << path;

	std::ifstream file(path, std::fstream::in | std::fstream::app);

	if (!file.good() || !file.is_open()) {
		LOGE << "Unable to open config";
		return;
	}

	file.seekg(0, std::ios::end);

	if (file.tellg() == 0) {
		config::write();
		return;
	}

	file.seekg(0);

	json tmp;

	try {
		file >> tmp;
	} catch (json::exception &e) {
		LOGE << e.what() << " - Resetting config...";
		cfg = getDefault();
		config::write();
		return;
	}

	cfg.update(tmp);

	LOGV << "Config parsed";
}

void config::write()
{
	const auto path = config::getPath();
	LOGV << "Writing to: " << path;

	std::ofstream file(path, std::ofstream::out);

	if (!file.good() || !file.is_open()) {
		LOGE << "Unable to open config";
		return;
	}

	try {
		file << std::setw(4) << cfg;
	} catch (json::exception &e) {
		LOGE << e.what() << " id: " << e.id;
		return;
	}

	LOGV << "Config set";
}

std::string config::getPath()
{
	const char *home   = getenv("XDG_CONFIG_HOME");
	const char *format = "/";

	if (!home) {
		format = "/.config/";
		home = getenv("HOME");
	}

	std::stringstream ss;
	ss << home << format << config_name;
	return ss.str();
}
