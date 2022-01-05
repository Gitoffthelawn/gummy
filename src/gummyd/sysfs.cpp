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

#include "sysfs.h"
#include "../common/defs.h"

#include <filesystem>
#include <algorithm>
#include <syslog.h>
#include <libudev.h>

std::vector<Sysfs::Device> Sysfs::get_devices()
{
	namespace fs = std::filesystem;

	std::vector<Device> devices;
	udev *udev = udev_new();

	for (const auto &entry : fs::directory_iterator("/sys/class/backlight")) {
		const auto path = entry.path();
		const auto path_str = path.generic_string();
		udev_device *dev = udev_device_new_from_syspath(udev, path_str.c_str());
		const std::string max_brt(udev_device_get_sysattr_value(dev, "max_brightness"));
		devices.emplace_back(
		    dev,
		    std::stoi(max_brt)
		);
	}

	udev_unref(udev);
	return devices;
}

Sysfs::Device::Device(udev_device *dev, const int max_brt)
    : max_brt(max_brt),
      dev(dev) {}

Sysfs::Device::~Device()
{
	udev_device_unref(dev);
}

Sysfs::Device::Device(Device&& d)
    : max_brt(d.max_brt),
      dev(d.dev) {}

void Sysfs::Device::set_backlight(int brt)
{
    brt = std::clamp(brt, 0, max_brt);
    udev_device_set_sysattr_value(dev, "brightness", std::to_string(brt).c_str());
}
