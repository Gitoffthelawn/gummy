#include "sysfs.h"
#include "../common/defs.h"
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

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <syslog.h>

std::vector<Device> Sysfs::getDevices()
{
	namespace fs = std::filesystem;

	std::vector<Device> devices;

	for (const auto &entry : fs::directory_iterator("/sys/class/backlight")) {

		const auto path         = entry.path();
		const auto path_str     = path.generic_string();
		const auto max_brt_file = path_str + "/max_brightness";

		std::ifstream stream(max_brt_file);
		if (!stream.is_open()) {
			syslog(LOG_ERR, "Unable to open %s", max_brt_file.c_str());
			exit(1);
		}

		std::string max_brt;
		stream.read(max_brt.data(), sizeof(max_brt));

		devices.emplace_back(
		    path.filename(),
		    path,
		    path_str + "/brightness",
		    std::stoi(max_brt)
		);
	}

	return devices;
}

Device::Device(std::string name, std::string path, std::string brt_file, int max_brt)
    : name(name),
      path(path),
      brt_file(brt_file),
      max_brt(max_brt)
{

}

Device::Device(Device&& d)
    : name(d.name),
      path(d.path),
      brt_file(d.brt_file),
      max_brt(d.max_brt)
{

}

void Device::setBacklight(int brt)
{
    brt = std::clamp(brt, 0, max_brt);
    std::string out(std::to_string(brt) + '\n');

    std::ofstream stream(brt_file);
    stream.write(out.c_str(), out.size());
}
