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

scrctl::Temp::Temp()
    : _current_step(0),
      _force(false),
      _quit(false),
      _tick(false)
{
}

void scrctl::Temp::start(Xorg &xorg)
{
	notify_on_wakeup();
	temp_loop(xorg);
}

void scrctl::Temp::notify()
{
	_force = true;
	_temp_cv.notify_one();
}

void scrctl::Temp::stop()
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

	std::mutex temp_mtx;
	std::condition_variable clock_cv;
	std::thread clock_thr([&] { clock(clock_cv, temp_mtx); });

	Timestamps ts;
	update_times(ts);

	bool needs_change = cfg.temp_auto;
	bool first_step = true;

	while (true) {
		{
			std::unique_lock<std::mutex> lock(temp_mtx);

			_temp_cv.wait(lock, [&] {
				return needs_change || !first_step || _tick || _force || _quit;
			});

			_tick = false;

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

		Animation a;
		a.elapsed    = 0.;
		a.fps        = cfg.temp_auto_fps;
		a.slice      = 1. / a.fps;
		a.duration_s = animation_s;
		a.start_step = _current_step;
		a.diff       = target_step - a.start_step;

		temp_animation_loop(-1, _current_step, target_step, a, xorg);
		first_step = false;
	}

	clock_cv.notify_one();
	clock_thr.join();
}

void scrctl::Temp::clock(std::condition_variable &cv, std::mutex &temp_mtx)
{
	using namespace std::chrono;
	using namespace std::chrono_literals;

	std::mutex mtx;

	{
		std::unique_lock lk(mtx);
		cv.wait_until(lk, system_clock::now() + 60s, [&] {
			return _quit;
		});
	}

	if (_quit || !cfg.temp_auto)
		return;

	{
		std::lock_guard lk(temp_mtx);
		_tick = true;
	}

	_temp_cv.notify_one();
	clock(cv, mtx);
}

void scrctl::Temp::temp_animation_loop(int prev_step, int cur_step, int target_step, Animation a, Xorg &xorg)
{
	using namespace std::this_thread;
	using namespace std::chrono;
	using namespace std::chrono_literals;
	if (_current_step == target_step || _force || !cfg.temp_auto || _quit)
		return;

	a.elapsed += a.slice;
	_current_step = int(ease_in_out_quad(a.elapsed, a.start_step, a.diff, a.duration_s));

	if (_current_step != prev_step) {
		for (size_t i = 0; i < xorg.scr_count(); ++i) {
			if (cfg.screens[i].temp_auto) {
				cfg.screens[i].temp_step = _current_step;
				xorg.set_gamma(i,
				               cfg.screens[i].brt_step,
				               _current_step);
			}
		}
	}

	sleep_for(milliseconds(1000 / a.fps));
	temp_animation_loop(cur_step, cur_step, target_step, a, xorg);
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
     : backlights(Sysfs::get_bl()),
       als(Sysfs::get_als())
{
	monitors.reserve(xorg.scr_count());
	threads.reserve(xorg.scr_count());

	for (size_t i = 0; i < xorg.scr_count(); ++i) {
		monitors.emplace_back(&xorg,
		                      &backlights[i] ? &backlights[i] : nullptr,
		                      &als[0] ? &als[0] : nullptr,
		                      i);
	}

	assert(monitors.size() == xorg.scr_count());
}

void scrctl::Brt::start()
{
	for (auto &m : monitors)
		threads.emplace_back([&] { m.init(); });
}

void scrctl::Brt::stop()
{
	for (auto &m : monitors)
		m.quit();
	for (auto &t : threads)
		t.join();
}

scrctl::Monitor::Monitor(Xorg* xorg, 
		Sysfs::Backlight *bl,
		Sysfs::ALS *als,
		int id)
   :  _xorg(xorg),
      _bl(bl),
      _als(als),
      _id(id),
      _ss_brt(0),
      _brt_needs_change(false),
      _force(false),
      _quit(false)
{
	if (!_bl) {
		_xorg->set_gamma(_id,
		                 brt_steps_max,
		                 cfg.screens[_id].temp_step);
	}
}

scrctl::Monitor::Monitor(Monitor &&o)
    :  _xorg(o._xorg),
       _bl(o._bl),
       _id(o._id)
{
}

void scrctl::Monitor::quit()
{
	_quit = true;
	_ss_cv.notify_one();
}

void scrctl::Monitor::notify()
{
	_force = true;
	_ss_cv.notify_one();
}

void scrctl::Monitor::init()
{
	std::condition_variable brt_cv;
	std::thread brt_thr([&] { brt_adjust_loop(brt_cv, brt_steps_max); });
	wait_for_auto_brt_active(brt_cv);
	{
		std::lock_guard lock(_brt_mtx);
		_brt_needs_change = true;
	}
	brt_cv.notify_one();
	brt_thr.join();
}

void scrctl::Monitor::wait_for_auto_brt_active(std::condition_variable &brt_cv)
{
	std::mutex mtx;
	{
		std::unique_lock lock(mtx);
		_ss_cv.wait(lock, [&] {
			return cfg.screens[_id].brt_auto || _quit;
		});
	}

	if (_quit)
		return;

	_force = true;
	capture_loop(brt_cv, 0);
	wait_for_auto_brt_active(brt_cv);
}

void scrctl::Monitor::capture_loop(std::condition_variable &brt_cv, int img_delta)
{
	using namespace std::this_thread;
	using namespace std::chrono;

	if (!cfg.screens[_id].brt_auto || _quit)
		return;

	const int ss_brt = _xorg->get_screen_brightness(_id);
	img_delta += abs(prev.ss_brt - ss_brt);

	if (img_delta > cfg.screens[_id].brt_auto_threshold || _force) {
		img_delta = 0;
		_force = false;
		{
			std::lock_guard lock(_brt_mtx);
			_ss_brt = ss_brt;
			_brt_needs_change = true;
		}
		brt_cv.notify_one();
	}

	if (cfg.screens[_id].brt_auto_min       != prev.cfg_min
	    || cfg.screens[_id].brt_auto_max    != prev.cfg_max
	    || cfg.screens[_id].brt_auto_offset != prev.cfg_offset) {
		_force = true;
	}

	prev.ss_brt     = ss_brt;
	prev.cfg_min    = cfg.screens[_id].brt_auto_min;
	prev.cfg_max    = cfg.screens[_id].brt_auto_max;
	prev.cfg_offset = cfg.screens[_id].brt_auto_offset;

	sleep_for(milliseconds(cfg.screens[_id].brt_auto_polling_rate));
	capture_loop(brt_cv, img_delta);
}

void scrctl::Monitor::brt_adjust_loop(std::condition_variable &brt_cv, int cur_step)
{
	int ss_brt;
	{
		std::unique_lock lock(_brt_mtx);
		brt_cv.wait(lock, [&] {
			return _brt_needs_change;
		});
		if (_quit)
			return;
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
	            cfg.screens[_id].brt_auto_max);

	if (cur_step == target_step && !_force)
		return brt_adjust_loop(brt_cv, cur_step);

	// There is an internal smooth system with backlights, avoid animations for now.
	if (_bl) {
		cur_step = target_step;
		_bl->set(cur_step * _bl->max_brt() / brt_steps_max);
	} else {
		Animation a;
		a.elapsed    = 0.;
		a.fps        = cfg.brt_auto_fps;
		a.slice      = 1. / a.fps;
		a.start_step = cur_step;
		a.diff       = target_step - a.start_step;
		a.duration_s = double(cfg.screens[_id].brt_auto_speed) / 1000;
		cur_step     = brt_animation_loop(-1, cur_step, target_step, a);
	}

	brt_adjust_loop(brt_cv, cur_step);
}

int scrctl::Monitor::brt_animation_loop(int prev_step, int cur_step, int target_step, Animation a)
{
	using namespace std::this_thread;
	using namespace std::chrono;
	using namespace std::chrono_literals;

	if (cur_step == target_step && !_force)
		return cur_step;

	_force = false; // warning: shared

	if (_brt_needs_change || !cfg.screens[_id].brt_auto || _quit)
		return cur_step;

	a.elapsed += a.slice;
	cur_step = int(std::round(ease_out_expo(a.elapsed, a.start_step, a.diff, a.duration_s)));

	if (cur_step != prev_step) {
		cfg.screens[_id].brt_step = cur_step;
		_xorg->set_gamma(_id,
		                 cur_step,
		                 cfg.screens[_id].temp_step);
	}

	sleep_for(milliseconds(1000 / a.fps));
	return brt_animation_loop(cur_step, cur_step, target_step, a);
}

scrctl::Gamma_Refresh::Gamma_Refresh() : _quit(false)
{
}

void scrctl::Gamma_Refresh::stop()
{
	_quit = true;
	_cv.notify_one();
}

void scrctl::Gamma_Refresh::loop(Xorg &xorg)
{
	using namespace std::chrono;
	using namespace std::chrono_literals;

	for (size_t i = 0; i < xorg.scr_count(); ++i) {
		xorg.set_gamma(i,
		               cfg.screens[i].brt_step,
		               cfg.screens[i].temp_step);
	}

	std::mutex mtx;
	std::unique_lock lk(mtx);
	_cv.wait_until(lk, system_clock::now() + 10s, [this] {
		return _quit;
	});
	if (_quit)
		return;
	loop(xorg);
}
