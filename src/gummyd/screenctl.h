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

class TempCtl
{
public:
    TempCtl(Xorg*);
	~TempCtl();
	int getCurrentStep();
	void notify(bool quit = false);
private:
	std::unique_ptr<std::thread> _thr;
	std::condition_variable _temp_cv;
	int  _current_step = 0;
	bool _quit = false;
	bool _force_change;
	std::time_t _cur_time;
	std::time_t _sunrise_time;
	std::time_t _sunset_time;

	/**
	 * Temperature is adjusted in two steps.
	 * The first one is for quickly catching up to the proper temperature when:
	 * - time is checked sometime after the start time
	 * - the system wakes up
	 * - temperature settings change */
	void adjust(Xorg*);
	void updateInterval();
};

class Monitor
{
public:
    Monitor(Monitor&&);
	Monitor(Xorg* server, Device *device, int scr_idx);
	bool hasBacklight();
	void setBacklight(int perc);

	void notify();
	~Monitor();
private:
	Xorg *_server;
	Device *_device;
	const int _scr_idx;
	std::unique_ptr<std::thread> _thr;
	std::condition_variable _ss_cv;
	std::mutex _brt_mtx;
	int _ss_brt;
	bool _brt_needs_change;
	bool _quit = false;

	void capture();
	void adjustBrightness(std::condition_variable&);
};

class ScreenCtl
{
public:
    ScreenCtl(Xorg *server);
	~ScreenCtl();
	void applyOptions(const std::string&);
private:
	void notifyMonitor(int scr_idx);
	int getAutoTempStep();

	Xorg *_server;
	std::vector<Device> _devices;
	std::vector<std::thread> _threads;
	std::vector<Monitor> _monitors;
	std::condition_variable _gamma_refresh_cv;
	TempCtl _temp_ctl;
	bool _quit = false;

	void reapplyGamma();
	void adjustTemperature();

	std::unique_ptr<sdbus::IProxy> _dbus_proxy;
	void registerWakeupSig();
};

#endif // SCREENCTL_H
