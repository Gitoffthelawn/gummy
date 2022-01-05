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

#include "screenctl.h"
#include "cfg.h"
#include "../common/utils.h"

#include <mutex>
#include <ctime>
#include <sdbus-c++/sdbus-c++.h>
#include <syslog.h>

void update_times(Timestamps &ts)
{
	// Get current timestamp
	ts.cur = std::time(nullptr);

	// Get tm struct from it
	std::tm *cur_tm = std::localtime(&ts.cur);

	// Set hour and min for sunrise
	const std::string sr(cfg.temp_auto_sunrise);
	cur_tm->tm_hour = std::stoi(sr.substr(0, 2));
	cur_tm->tm_min  = std::stoi(sr.substr(3, 2));

	// Subtract adaptation time
	cur_tm->tm_sec  = 0;
	cur_tm->tm_sec -= cfg.temp_auto_speed * 60;

	ts.sunrise = std::mktime(cur_tm);

	// Set hour and min for sunset
	const std::string sn(cfg.temp_auto_sunset);
	cur_tm->tm_hour = std::stoi(sn.substr(0, 2));
	cur_tm->tm_min  = std::stoi(sn.substr(3, 2));

	// Subtract adaptation time
	cur_tm->tm_sec  = 0;
	cur_tm->tm_sec -= cfg.temp_auto_speed * 60;

	ts.sunset = std::mktime(cur_tm);
}

bool is_daytime(const Timestamps &ts)
{
	return ts.cur >= ts.sunrise && ts.cur < ts.sunset;
}

scrctl::Temp::Temp(Xorg &xorg)
    : _thr(std::make_unique<std::thread>([&] { temp_loop(xorg); }))
{
	notify_on_wakeup();
}

scrctl::Temp::~Temp()
{
	quit();
	_thr->join();
}

void scrctl::Temp::notify()
{
	_force = true;
	_temp_cv.notify_one();
}

void scrctl::Temp::quit()
{
	_quit = true;
	_temp_cv.notify_one();
}

int scrctl::Temp::current_step() const
{
	return _current_step;
}

void scrctl::Temp::temp_loop(Xorg &xorg)
{
	using namespace std::this_thread;
	using namespace std::chrono;
	using namespace std::chrono_literals;

	bool needs_change = cfg.temp_auto;

	std::mutex temp_mtx;
	std::condition_variable clock_cv;

	std::thread clock([&] {
		std::mutex clock_mtx;
		while (true) {
			{
				std::unique_lock lk(clock_mtx);
				clock_cv.wait_until(lk, system_clock::now() + 60s, [&] {
					return _quit;
				});
			}

			if (_quit)
				return;

			if (!cfg.temp_auto)
				continue;

			{
				std::lock_guard lk(temp_mtx);
				needs_change = true;
			}

			_temp_cv.notify_one();
		}
	});

	bool first_step = true;

	Timestamps ts;
	update_times(ts);

	while (true) {
		{
			std::unique_lock<std::mutex> lock(temp_mtx);

			_temp_cv.wait(lock, [&] {
				return needs_change || !first_step || _force || _quit;
			});

			if (_quit)
				break;

			if (_force) {
				update_times(ts);
				_force = false;
				first_step = true;
			}

			needs_change = false;
		}

		if (!cfg.temp_auto)
			continue;

		const double adaptation_time_s = double(cfg.temp_auto_speed) * 60;

		ts.cur = std::time(nullptr);

		const bool daytime = is_daytime(ts);
		int target_temp;
		long tmp;

		if (daytime) {
			target_temp = cfg.temp_auto_high;
			tmp = ts.sunrise;
		} else {
			target_temp = cfg.temp_auto_low;
			tmp = ts.sunset;
		}

		int time_since_start_s = std::abs(ts.cur - tmp);

		if (time_since_start_s > adaptation_time_s)
			time_since_start_s = adaptation_time_s;

		double animation_s = 2;

		if (first_step) {
			if (daytime) {
				target_temp = remap(
				    time_since_start_s, 0, adaptation_time_s,
				    cfg.temp_auto_low, cfg.temp_auto_high
				);
			} else {
				target_temp = remap(
				    time_since_start_s, 0, adaptation_time_s,
				    cfg.temp_auto_high, cfg.temp_auto_low
				);
			}
		} else {
			animation_s = adaptation_time_s - time_since_start_s;
			if (animation_s < 2)
				animation_s = 2;
		}

		const int target_step = int(remap(target_temp, temp_k_max, temp_k_min, temp_steps_max, 0));

		if (_current_step == target_step) {
			first_step = true;
			continue;
		}

		const int FPS      = cfg.temp_auto_fps;
		const double slice = 1. / FPS;

		double time = 0;
		const int start_step = _current_step;
		const int diff = target_step - start_step;

		int prev_step = -1;

		while (_current_step != target_step) {

			if (_force || !cfg.temp_auto || _quit)
				break;

			time += slice;

			_current_step = int(easeInOutQuad(time, start_step, diff, animation_s));

			if (_current_step != prev_step) {

				for (int i = 0; i < xorg.scr_count(); ++i) {

					if (cfg.screens[i].temp_auto) {

						cfg.screens[i].temp_step = _current_step;
						xorg.set_gamma(
						    i,
						    cfg.screens[i].brt_step,
						    _current_step
						);
					}
				}
			}

			prev_step = _current_step;

			sleep_for(milliseconds(1000 / FPS));
		}
		first_step = false;
	}

	clock_cv.notify_one();
	clock.join();
}

void scrctl::Temp::notify_on_wakeup()
{
	const std::string service("org.freedesktop.login1");
	const std::string obj_path("/org/freedesktop/login1");
	const std::string interface("org.freedesktop.login1.Manager");
	const std::string signal("PrepareForSleep");

	_dbus_proxy = sdbus::createProxy(service, obj_path);

	try {
		_dbus_proxy->registerSignalHandler(interface, signal, [this] (sdbus::Signal &sig) {
			bool going_to_sleep;
			sig >> going_to_sleep;
			if (!going_to_sleep)
				notify();
		});
		_dbus_proxy->finishRegistration();
	} catch (sdbus::Error e) {
		syslog(LOG_ERR, "sdbus error: %s", e.what());
	}
}

scrctl::Brt::Brt(Xorg &xorg)
     : devices(Sysfs::get_devices())
{
	monitors.reserve(xorg.scr_count());
	for (size_t i = 0; i < xorg.scr_count(); ++i) {
		monitors.emplace_back(&xorg,
		   devices.size() > i ? &devices[i] : nullptr,
		   i);
	}
	assert(monitors.size() == xorg.scr_count());
}

scrctl::Gamma_Refresh::Gamma_Refresh(Xorg &xorg)
     : _thr(std::make_unique<std::thread>([&] { loop(xorg); }))
{}

scrctl::Gamma_Refresh::~Gamma_Refresh()
{
	_quit = true;
	_cv.notify_one();
	_thr->join();
}

void scrctl::Gamma_Refresh::loop(Xorg &xorg)
{
	using namespace std::chrono;
	using namespace std::chrono_literals;
	std::mutex mtx;

	const auto refresh = [&] {
		for (size_t i = 0; i < xorg.scr_count(); ++i) {
			xorg.set_gamma(i,
			    cfg.screens[i].brt_step,
			    cfg.screens[i].temp_step);
		}
	};

	refresh();

	while (true) {
		{
			std::unique_lock lk(mtx);
			_cv.wait_until(lk, system_clock::now() + 10s, [this] {
				return _quit;
			});
		}
		if (_quit)
			return;
		refresh();
	}
}

scrctl::Monitor::Monitor(Xorg* xorg, Sysfs::Device *device, const int id)
   :  _xorg(xorg),
      _device(device),
      _id(id),
      _thr(std::make_unique<std::thread>([this] { capture(); }))
{
}

scrctl::Monitor::Monitor(Monitor &&o)
    :  _xorg(o._xorg),
       _device(o._device),
       _id(o._id)
{
	_thr.swap(o._thr);
}

scrctl::Monitor::~Monitor()
{
	_quit = true;
	_ss_cv.notify_one();
	_thr->join();
}

void scrctl::Monitor::notify()
{
	_force = true;
	_ss_cv.notify_one();
}

void scrctl::Monitor::capture()
{
	using namespace std::this_thread;
	using namespace std::chrono;

	std::condition_variable brt_cv;
	std::thread brt_thr([&] { brt_loop(brt_cv); });

	int img_delta = 0;
	bool force = false;

	struct {
	    int ss_brt;
		int min;
		int max;
		int offset;
	} prev{0, 0, 0, 0};

	std::mutex mtx;

	while (true) {
		{
			std::unique_lock lock(mtx);
			_ss_cv.wait(lock, [&] {
				return cfg.screens[_id].brt_auto || _quit;
			});
		}

		if (_quit)
			break;

		if (cfg.screens[_id].brt_auto)
			force = true;
		else
			continue;

		while (cfg.screens[_id].brt_auto && !_quit) {

			const int ss_brt = _xorg->get_screen_brightness(_id);

			img_delta += abs(prev.ss_brt - ss_brt);

			if (img_delta > cfg.screens[_id].brt_auto_threshold || force) {

				img_delta = 0;
				force = false;

				{
					std::lock_guard lock(_brt_mtx);
					_ss_brt = ss_brt;
					_brt_needs_change = true;
				}

				brt_cv.notify_one();
			}

			if (cfg.screens[_id].brt_auto_min    != prev.min
			 || cfg.screens[_id].brt_auto_max    != prev.max
			 || cfg.screens[_id].brt_auto_offset != prev.offset) {
				force = true;
			}

			prev.ss_brt = ss_brt;
			prev.min    = cfg.screens[_id].brt_auto_min;
			prev.max    = cfg.screens[_id].brt_auto_max;
			prev.offset = cfg.screens[_id].brt_auto_offset;

			sleep_for(milliseconds(cfg.screens[_id].brt_auto_polling_rate));
		}
	}

	{
		std::lock_guard lock(_brt_mtx);
		_brt_needs_change = true;
	}

	brt_cv.notify_one();
	brt_thr.join();
}

void scrctl::Monitor::brt_loop(std::condition_variable &brt_cv)
{
	using namespace std::this_thread;
	using namespace std::chrono;
	using namespace std::chrono_literals;

	int cur_step = brt_steps_max;

	if (!_device) {
		_xorg->set_gamma(_id, brt_steps_max, cfg.screens[_id].temp_step);
	}

	while (true) {
		int ss_brt;

		{
			std::unique_lock lock(_brt_mtx);

			brt_cv.wait(lock, [&] {
				return _brt_needs_change;
			});

			if (_quit)
				break;

			_brt_needs_change = false;
			ss_brt = _ss_brt;
		}

		const int ss_step = ss_brt * brt_steps_max / 255;

		// Offset relative to the max brightness.
		const int offset_step = cfg.screens[_id].brt_auto_offset
		        * brt_steps_max
		        / cfg.screens[_id].brt_auto_max;

		const int target_step = std::clamp(
		    brt_steps_max - ss_step + offset_step,
		    cfg.screens[_id].brt_auto_min,
		    cfg.screens[_id].brt_auto_max
		);

		if (cur_step == target_step && !_force) {
			continue;
		}

		if (_device) {
			cur_step = target_step;
			_device->set_backlight(cur_step * _device->max_brt / brt_steps_max);
			continue;
		}

		const int    fps   = cfg.brt_auto_fps;
		const double slice = 1. / fps;

		double time = 0;
		const int start_step = cur_step;
		const int diff       = target_step - start_step;
		const double animation_s = double(cfg.screens[_id].brt_auto_speed) / 1000;

		int prev_step = -1;

		while (cur_step != target_step || _force) {
			_force = false;

			if (_brt_needs_change || !cfg.screens[_id].brt_auto || _quit)
				break;

			time += slice;

			cur_step = int(std::round(
			    easeOutExpo(time, start_step, diff, animation_s))
			);

			if (cur_step != prev_step) {

				cfg.screens[_id].brt_step = cur_step;

				_xorg->set_gamma(
				    _id,
				    cur_step,
				    cfg.screens[_id].temp_step
				);
			}

			prev_step = cur_step;

			sleep_for(milliseconds(1000 / fps));
		}
	}
}
