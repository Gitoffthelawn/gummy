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

void timestamps_update(Timestamps &ts)
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

scrctl::Temp_Manager::Temp_Manager()
    : _current_step(0),
      _notified(false),
      _quit(false),
      _tick(false)
{
}

void scrctl::Temp_Manager::init(Xorg &xorg)
{
	start(xorg);
	notify_on_wakeup();
}

void scrctl::Temp_Manager::notify()
{
	_notified = true;
	_temp_cv.notify_one();
}

void scrctl::Temp_Manager::stop()
{
	_quit = true;
	_temp_cv.notify_one();
}

int scrctl::Temp_Manager::current_step() const
{
	return _current_step;
}

void scrctl::Temp_Manager::start(Xorg &xorg)
{
	std::mutex temp_mtx;
	std::condition_variable clock_cv;
	std::thread clock_thr([&] { clock(clock_cv, temp_mtx); });

	check_auto_temp_loop(xorg, temp_mtx);

	clock_cv.notify_one();
	clock_thr.join();
}

void scrctl::Temp_Manager::check_auto_temp_loop(Xorg &xorg, std::mutex &temp_mtx)
{
	std::mutex mtx;

	{
		std::unique_lock lk(mtx);
		_temp_cv.wait(lk, [&] {
			return cfg.temp_auto || _quit;
		});
	}

	if (_quit)
		return;

	Timestamps ts;
	timestamps_update(ts);
	_tick = true;
	temp_loop(xorg, temp_mtx, ts, true);

	check_auto_temp_loop(xorg, temp_mtx);
}

void scrctl::Temp_Manager::temp_loop(Xorg &xorg, std::mutex &temp_mtx, Timestamps &ts, bool catch_up)
{
	{
		std::unique_lock lk(temp_mtx);
		_temp_cv.wait(lk, [&] {
			return !catch_up || _tick || _notified || _quit;
		});
		if (_quit)
			return;
		if (_notified) {
			_notified = false;
			catch_up = true;
			timestamps_update(ts);
		}
		_tick = false;
	}

	if (!cfg.temp_auto)
		return;

	ts.cur = std::time(nullptr);
	const bool daytime = is_daytime(ts);

	int target_temp;
	std::time_t time_to_subtract;
	if (daytime) {
		target_temp = cfg.temp_auto_high;
		time_to_subtract = ts.sunrise;
	} else {
		target_temp = cfg.temp_auto_low;
		time_to_subtract = ts.sunset;
	}

	const double adaptation_time_s = double(cfg.temp_auto_speed) * 60;
	int time_since_start_s = std::abs(ts.cur - time_to_subtract);
	if (time_since_start_s > adaptation_time_s)
		time_since_start_s = adaptation_time_s;

	double animation_s = 2;
	if (catch_up) {
		if (daytime) {
			target_temp = remap(time_since_start_s, 0, adaptation_time_s, cfg.temp_auto_low, cfg.temp_auto_high);
		} else {
			target_temp = remap(time_since_start_s, 0, adaptation_time_s, cfg.temp_auto_high, cfg.temp_auto_low);
		}
	} else {
		animation_s = adaptation_time_s - time_since_start_s;
		if (animation_s < 2)
			animation_s = 2;
	}

	const int target_step = int(remap(target_temp, temp_k_max, temp_k_min, temp_steps_max, 0));
	if (_current_step != target_step) {
		Animation a;
		a.elapsed    = 0.;
		a.fps        = cfg.temp_auto_fps;
		a.slice      = 1. / a.fps;
		a.duration_s = animation_s;
		a.start_step = _current_step;
		a.diff       = target_step - a.start_step;
		temp_animation_loop(-1, _current_step, target_step, a, xorg);
	}

	temp_loop(xorg, temp_mtx, ts, !catch_up);
}

void scrctl::Temp_Manager::clock(std::condition_variable &cv, std::mutex &temp_mtx)
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

void scrctl::Temp_Manager::temp_animation_loop(int prev_step, int cur_step, int target_step, Animation a, Xorg &xorg)
{
	using namespace std::this_thread;
	using namespace std::chrono;
	using namespace std::chrono_literals;

	if (_current_step == target_step || _notified || !cfg.temp_auto || _quit)
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

void scrctl::Temp_Manager::notify_on_wakeup()
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

scrctl::Brightness_Manager::Brightness_Manager(Xorg &xorg)
     : backlights(Sysfs::get_bl()),
       als(Sysfs::get_als())
{
	monitors.reserve(xorg.scr_count());
	threads.reserve(xorg.scr_count());

	for (size_t i = 0; i < xorg.scr_count(); ++i) {
		monitors.emplace_back(&xorg,
		                      i < xorg.scr_count() ? &backlights[i] : nullptr,
		                      als.size() > 0 ? &als[0] : nullptr,
		                      i);
	}

	assert(monitors.size() == xorg.scr_count());
}

void scrctl::Brightness_Manager::start()
{
	for (auto &m : monitors)
		threads.emplace_back([&] { monitor_init(m); });
}

void scrctl::Brightness_Manager::stop()
{
	for (auto &m : monitors)
		monitor_stop(m);
	for (auto &t : threads)
		t.join();
}

scrctl::Monitor::Monitor(Xorg *xorg,
		Sysfs::Backlight *bl,
		Sysfs::ALS *als,
		int id)
   :  xorg(xorg),
      backlight(bl),
      als(als),
      id(id),
      ss_brt(0),
      flags({0,0,0})
{
	if (!backlight) {
		xorg->set_gamma(id,
		                 brt_steps_max,
		                 cfg.screens[id].temp_step);
	}
}

scrctl::Monitor::Monitor(Monitor &&o)
    :  xorg(o.xorg),
       backlight(o.backlight),
       id(o.id),
       flags(o.flags)
{
}

void scrctl::monitor_init(Monitor &mon)
{
	Sync brt_sync;
	brt_sync.flag = false;

	std::thread adjust_thr([&] {
		monitor_brt_adjust_loop(mon, brt_sync, brt_steps_max);
	});
	monitor_is_auto_loop(mon, brt_sync);

	{
		std::lock_guard lk(brt_sync.mtx);
		brt_sync.flag = true;
	}
	brt_sync.cv.notify_one();

	adjust_thr.join();
}

void scrctl::monitor_is_auto_loop(Monitor &mon, Sync &brt_sync)
{
	std::mutex mtx; {
		std::unique_lock lk(mtx);
		mon.cv.wait(lk, [&] { return !mon.flags.paused; });
	}
	if (mon.flags.stopped)
		return;
	monitor_capture_loop(mon, brt_sync, Previous_capture_state{0,0,0,0}, 0);
	monitor_is_auto_loop(mon, brt_sync);
}

void scrctl::monitor_capture_loop(Monitor &mon, Sync &brt_sync, Previous_capture_state prev, int ss_delta)
{
	if (mon.flags.paused || mon.flags.stopped)
		return;

	const int ss_brt = mon.xorg->get_screen_brightness(mon.id);
	ss_delta += abs(prev.ss_brt - ss_brt);

	const auto &scr = cfg.screens[mon.id];
	if (ss_delta > scr.brt_auto_threshold) {
		ss_delta = 0;
		{
			std::lock_guard lk(brt_sync.mtx);
			brt_sync.flag = true;
			mon.ss_brt = ss_brt;
		}
		brt_sync.cv.notify_one();
	}

	if (scr.brt_auto_min != prev.cfg_min
	    || scr.brt_auto_max != prev.cfg_max
	    || scr.brt_auto_offset != prev.cfg_offset) {
		ss_delta = 255;
		mon.flags.cfg_updated = true; // not worth syncing
	}

	prev.ss_brt     = ss_brt;
	prev.cfg_min    = scr.brt_auto_min;
	prev.cfg_max    = scr.brt_auto_max;
	prev.cfg_offset = scr.brt_auto_offset;

	std::this_thread::sleep_for(std::chrono::milliseconds(scr.brt_auto_polling_rate));
	monitor_capture_loop(mon, brt_sync, prev, ss_delta);
}

void scrctl::monitor_brt_adjust_loop(Monitor &mon, Sync &brt_sync, int cur_step)
{
	int ss_brt; {
		std::unique_lock lk(brt_sync.mtx);
		brt_sync.cv.wait(lk, [&] { return brt_sync.flag; });
		brt_sync.flag = false;
		ss_brt = mon.ss_brt;
	}

	if (mon.flags.stopped)
		return;

	const auto &scr = cfg.screens[mon.id];
	const int target_step = calc_brt_target(ss_brt, scr.brt_auto_min, scr.brt_auto_max, scr.brt_auto_offset);

	if (cur_step != target_step || mon.flags.cfg_updated) {
		mon.flags.cfg_updated = false;
		if (mon.backlight) {
			cur_step = target_step;
			mon.backlight->set(cur_step * mon.backlight->max_brt() / brt_steps_max);
		} else {
			Animation a = animation_init(cur_step, target_step, cfg.brt_auto_fps, scr.brt_auto_speed);
			cur_step = monitor_brt_animation_loop(mon, a, -1, cur_step, target_step, ss_brt);
		}
	}

	monitor_brt_adjust_loop(mon, brt_sync, cur_step);
}

int scrctl::monitor_brt_animation_loop(Monitor &mon, Animation a, int prev_step, int cur_step, int target_step, int ss_brt)
{
	if (mon.ss_brt != ss_brt)
		return cur_step;
	if (mon.flags.paused || mon.flags.cfg_updated || mon.flags.stopped)
		return cur_step;
	if (cur_step == target_step)
		return cur_step;

	a.elapsed += a.slice;
	cur_step = int(round(ease_out_expo(a.elapsed, a.start_step, a.diff, a.duration_s)));

	if (cur_step != prev_step) {
		cfg.screens[mon.id].brt_step = cur_step;
		mon.xorg->set_gamma(mon.id,
		                    cur_step,
		                    cfg.screens[mon.id].temp_step);
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(1000 / a.fps));
	return monitor_brt_animation_loop(mon, a, cur_step, cur_step, target_step, ss_brt);
}

void scrctl::monitor_stop(Monitor &mon)
{
	mon.flags.paused = false;
	mon.flags.stopped = true;
	mon.cv.notify_one();
}

void scrctl::monitor_pause(Monitor &mon)
{
	mon.flags.paused = true;
}

void scrctl::monitor_resume(Monitor &mon)
{
	mon.flags.paused = false;
	mon.cv.notify_one();
}

void scrctl::monitor_toggle(Monitor &mon, bool toggle)
{
	if (toggle)
		monitor_resume(mon);
	else
		monitor_pause(mon);
}

int scrctl::calc_brt_target(int ss_brt, int min, int max, int offset)
{
	const int ss_step     = ss_brt * brt_steps_max / 255;
	const int offset_step = offset * brt_steps_max / max;
	return std::clamp(brt_steps_max - ss_step + offset_step, min, max);
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
