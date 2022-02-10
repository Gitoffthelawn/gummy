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
#include "../common/utils.h"

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
	Temp();
	void start(Xorg&);
	int  current_step() const;
	void notify();
	void stop();
private:
	std::condition_variable _temp_cv;
	std::unique_ptr<sdbus::IProxy> _dbus_proxy;
	int  _current_step;
	bool _force;
	bool _quit;
	bool _tick;

	void clock(std::condition_variable &cv, std::mutex&);
	void temp_loop(Xorg&);
	void notify_on_wakeup();
	void temp_animation_loop(int prev_step, int cur_step, int target_step, Animation a, Xorg&);
};

class Monitor
{
public:
	Monitor(Xorg* xorg, Sysfs::Backlight*, Sysfs::ALS*, int id);
	Monitor(Monitor&&);
	void init();
	void notify();
	void quit();
private:
	std::condition_variable _ss_cv;
	std::mutex              _brt_mtx;

	struct {
		int ss_brt;
		int cfg_min;
		int cfg_max;
		int cfg_offset;
	} prev;
	void capture_loop(std::condition_variable&, int img_delta);

	Xorg             *_xorg;
	Sysfs::Backlight *_bl;
	Sysfs::ALS       *_als;

	int  _id;
	int  _ss_brt;
	bool _brt_needs_change;
	bool _force;
	bool _quit;

	void wait_for_auto_brt_active(std::condition_variable &);
	void brt_adjust_loop(std::condition_variable&, int cur_step);
	int  brt_animation_loop(int prev_step, int cur_step, int target_step, Animation a);
};

struct Brt
{
	Brt(Xorg&);
	void start();
	void stop();
	std::vector<Sysfs::Backlight> backlights;
	std::vector<Sysfs::ALS>       als;
	std::vector<std::thread>      threads;
	std::vector<Monitor>          monitors;
};

class Gamma_Refresh
{
public:
	Gamma_Refresh();
	void loop(Xorg&);
	void stop();
private:
	std::condition_variable _cv;
	bool _quit;
};

}

#endif // SCREENCTL_H
