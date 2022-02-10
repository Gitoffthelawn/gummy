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

#ifndef SCREENCTL_H
#define SCREENCTL_H

#include "xorg.h"
#include "sysfs.h"
#include "../common/defs.h"

#include <thread>
#include <condition_variable>
#include <sdbus-c++/ProxyInterfaces.h>

struct Timestamps {
	std::time_t cur;
	std::time_t sunrise;
	std::time_t sunset;
};
void update_times(Timestamps &ts);
bool is_daytime(const Timestamps &ts);

namespace scrctl {

/**
 * Temperature is adjusted in two steps.
 * The first one is for quickly catching up to the proper temperature when:
 * - time is checked sometime after the start time
 * - the system wakes up
 * - temperature settings change */
class Temp
{
public:
	Temp(Xorg&);
	~Temp();
	int  current_step() const;
	void notify();
	void quit();
private:
	std::condition_variable _temp_cv;
	std::unique_ptr<std::thread> _thr;
	std::unique_ptr<sdbus::IProxy> _dbus_proxy;
	int  _current_step{};
	bool _force{};
	bool _quit{};

	void temp_loop(Xorg&);
	void notify_on_wakeup();
};

class Monitor
{
public:
	Monitor(Xorg* xorg, Sysfs::Backlight*, Sysfs::ALS*, int id);
	Monitor(Monitor&&);
	~Monitor();
	void notify();
	void quit();
private:
	std::condition_variable _ss_cv;
	std::mutex _brt_mtx;
	Xorg *_xorg;
	Sysfs::Backlight *_bl;
	Sysfs::ALS *_als;	
	int _id;
	std::unique_ptr<std::thread> _thr;
	
	int  _current_step{};
	int  _ss_brt;
	bool _brt_needs_change{};
	bool _force{};
	bool _quit{};

	void capture();
	void brt_loop(std::condition_variable&);
};

struct Brt
{
    	Brt(Xorg &);
	std::vector<Sysfs::Backlight> backlights;
	std::vector<Sysfs::ALS> als;
	std::vector<Monitor> monitors;
};

class Gamma_Refresh
{
public:
	Gamma_Refresh(Xorg&);
	~Gamma_Refresh();
private:
	std::condition_variable _cv;
	std::unique_ptr<std::thread> _thr;
	bool _quit{};
	void loop(Xorg&);
};

}

#endif // SCREENCTL_H
