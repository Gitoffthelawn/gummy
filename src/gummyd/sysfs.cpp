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

std::vector<Sysfs::Backlight> Sysfs::get_bl()
{
	namespace fs = std::filesystem;
	std::vector<Sysfs::Backlight> bl;
	udev *udev = udev_new();
	for (const auto &s : fs::directory_iterator("/sys/class/backlight")) {
		bl.emplace_back(udev, s.path().generic_string());
	}
	udev_unref(udev);
	return bl;
}

std::vector<Sysfs::ALS> Sysfs::get_als()
{
	namespace fs = std::filesystem;
	std::vector<Sysfs::ALS> als;
	udev *udev = udev_new();
	for (const auto &s : fs::directory_iterator("/sys/bus/iio/devices")) {  
		const auto f = s.path().stem().string(); 
		if (f.find("iio:device") == std::string::npos)
			continue;
		als.emplace_back(udev, s.path());
	}
	udev_unref(udev);
	return als;
}

Sysfs::Device::Device(udev *udev, const std::string &path)
{
	_dev = udev_device_new_from_syspath(udev, path.c_str());
}

Sysfs::Device::Device(Device &&d) : _dev(d._dev)
{

}

Sysfs::Device::~Device()
{
	udev_device_unref(_dev);
}

std::string Sysfs::Device::get(const std::string &attr) const
{
	return udev_device_get_sysattr_value(_dev, attr.c_str());
}

void Sysfs::Device::set(const std::string &attr, const std::string &val)
{
	udev_device_set_sysattr_value(_dev, attr.c_str(), val.c_str());
}

Sysfs::Backlight::Backlight(udev *udev, const std::string &path)
	: _dev(udev, path),
	  _max_brt(std::stoi(_dev.get("max_brightness")))
{

}

void Sysfs::Backlight::set(int brt)
{
	brt = std::clamp(brt, 0, _max_brt);
	_dev.set("brightness", std::to_string(brt).c_str());
}

int Sysfs::Backlight::max_brt() const 
{
	return _max_brt;
}

Sysfs::ALS::ALS(udev *udev, const std::string &path)
	: _dev(udev, path)
{

}

int Sysfs::ALS::get_lux() const
{
	return std::stoi(_dev.get("in_illuminance_raw"));
}
