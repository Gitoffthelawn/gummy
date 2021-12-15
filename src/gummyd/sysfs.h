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

class Device;
class Sysfs
{
	Sysfs() = delete;
	~Sysfs() = delete;
public:
	static std::vector<Device> getDevices();
};

class Device
{
public:
	Device(std::string, std::string, std::string, int);
	Device(Device&&);
	void setBacklight(int);
private:
	std::string name;
	std::string path;
	std::string brt_file;
	int max_brt;
};

#endif // SYSFS_H
