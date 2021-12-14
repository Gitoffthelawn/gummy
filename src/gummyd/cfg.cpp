/**
 * Copyright (C) Francesco Fusco. All rights reserved.
 * License: https://github.com/Fushko/gammy#license
 */

#include "../common/utils.h"
#include "../common/defs.h"
#include "cfg.h"
#include <fstream>
#include <iostream>

json getDefault()
{
	json ret;
	config::addScreenEntries(ret, 1);

	ret["brt_auto_fps"]      = 60;
	ret["brt_auto_speed"]    = 1000; // ms

	ret["temp_auto"]         = false;
	ret["temp_auto_fps"]     = 45;
	ret["temp_auto_speed"]   = 60; // min
	ret["temp_auto_sunrise"] = "06:00";
	ret["temp_auto_sunset"]  = "16:00";
	ret["temp_auto_high"]    = temp_k_min;
	ret["temp_auto_low"]     = 3400;

	ret["log_level"] = plog::warning;

	return ret;
}

void config::addScreenEntries(json &config, int n)
{
	json screen = {
	    {"brt_auto", false},
	    {"brt_auto_threshold", 8},
	    {"brt_auto_polling_rate", 100},
	    {"brt_auto_min", brt_steps_max / 2},
	    {"brt_auto_max", brt_steps_max},
	    {"brt_auto_offset", brt_steps_max / 3},
	    {"brt_step", brt_steps_max},
	    {"temp_auto", false},
	    {"temp_step", 0},
	};

	if (!config.contains("screens"))
		config["screens"] = json::array();

	while (n--)
		config["screens"].push_back(screen);
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
