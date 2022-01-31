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

#ifndef SYSFS_H
#define SYSFS_H

#include <vector>
#include <filesystem>
#include <libudev.h>

namespace Sysfs
{
	class Device
	{
	public:
		Device(udev*, const std::string &path);
		Device(Device&&);
		~Device();
		std::string get(const std::string &attr) const;
		void        set(const std::string &attr, const std::string &val);
	private:
		udev_device *_dev;
	};

	class Backlight
	{
	public:
		Backlight(udev*, const std::string &path);
		int max_brt() const;
		void set(int);
	private:
		Device _dev;
		int _max_brt;
	};

	class ALS
	{
	public:
		ALS(udev*, const std::string &path);
		int get_lux() const;
	private:
		Device _dev;
	};

	std::vector<Backlight> get_bl();
	std::vector<ALS> get_als();
};

#endif // SYSFS_H
