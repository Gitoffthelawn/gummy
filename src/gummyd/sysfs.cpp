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
#include <cmath>

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
	std::string als_path = "/sys/bus/iio/devices";
	std::vector<Sysfs::ALS> als;

	if (!fs::exists(als_path))
		return als;

	udev *udev = udev_new();
	for (const auto &s : fs::directory_iterator(als_path)) {
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

std::string Sysfs::Device::path() const
{
	return udev_device_get_syspath(_dev);
}

Sysfs::Device::~Device()
{
	udev_device_unref(_dev);
}

std::string Sysfs::Device::get(const std::string &attr) const
{
	const char *s = udev_device_get_sysattr_value(_dev, attr.c_str());
	return s ? s : "";
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
	: _dev(udev, path),
	  _lux_scale(1.0)
{
	const std::array<std::string, 2> lux_names = {
	    "in_illuminance_input",
	    "in_illuminance_raw"
	};
	for (const auto &name : lux_names) {
		if (!_dev.get(name).empty()) {
			_lux_name = name;
			break;
		}
	}
	if (_lux_name.empty()) {
		syslog(LOG_ERR, "ALS output file not found");
	}

	const std::string scale = _dev.get("in_illuminance_scale");
	if (!scale.empty())
		_lux_scale = std::stod(scale);
}

void Sysfs::ALS::update()
{
	udev *u = udev_new();
	Sysfs::Device dev(u, _dev.path());
	udev_unref(u);
	const double lux = std::stod(dev.get(_lux_name)) * _lux_scale;
	_lux_step = calc_lux_step(lux);
}

int Sysfs::ALS::lux_step() const
{
	return _lux_step;
}

/* Rationale:
 * - light is perceived logarithmically by the human eye.
 * - the max illuminance detected by my laptop sensor is 21090;
 * - that's roughly 4.3 in log10. Divide by 5 as an approximation. */
int Sysfs::calc_lux_step(double lux)
{
	if (lux == 0.)
		return 0;
	return log10(lux) / 5 * brt_steps_max;
}
