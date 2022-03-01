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

core::Temp_Manager::Temp_Manager(Xorg *xorg)
    : xorg(xorg),
      current_step(0),
      notified(false)
{
	auto_sync.wake_up = false;
	clock_sync.wake_up = false;
}

void core::temp_init(Temp_Manager &t)
{
	temp_on_system_wakeup(t);
	temp_start(t);
}

void core::temp_start(Temp_Manager &t)
{
	{
		std::unique_lock lk(t.auto_sync.mtx);
		t.auto_sync.cv.wait(lk, [&] {
			return cfg.temp_auto || t.auto_sync.wake_up;
		});
		if (t.auto_sync.wake_up)
			return;
	}

	std::thread clock_thr([&] { temp_time_check_loop(t); } );

	Timestamps ts;
	timestamps_update(ts);
	temp_notify(t);
	temp_adjust_loop(t, ts, true);

	t.clock_sync.cv.notify_one();
	clock_thr.join();
}

void core::temp_notify(Temp_Manager &t)
{
	{
		std::lock_guard(t.auto_sync.mtx);
		t.notified = true;
	}
	t.auto_sync.cv.notify_one();
}

void core::temp_stop(Temp_Manager &t)
{
	{
		std::lock_guard(t.auto_sync.mtx);
		t.auto_sync.wake_up = true;
	}
	t.auto_sync.cv.notify_one();
	t.clock_sync.cv.notify_one();
}

void core::temp_time_check_loop(Temp_Manager &t)
{
	using namespace std::chrono;
	using namespace std::chrono_literals;
	{
		std::unique_lock lk(t.clock_sync.mtx);
		t.clock_sync.cv.wait_until(lk, system_clock::now() + 60s, [&t] {
			return t.auto_sync.wake_up;
		});
		if (t.auto_sync.wake_up)
			return;
	}
	if (!cfg.temp_auto)
		return;
	{
		std::lock_guard lk(t.clock_sync.mtx);
		t.clock_sync.wake_up = true;
	}
	t.auto_sync.cv.notify_one();
	temp_time_check_loop(t);
}

void core::temp_adjust_loop(Temp_Manager &t, Timestamps &ts, bool catch_up)
{
	{
		std::unique_lock lk(t.auto_sync.mtx);
		t.auto_sync.cv.wait(lk, [&] {
			return !catch_up || t.notified || t.clock_sync.wake_up || t.auto_sync.wake_up;
		});
		if (t.auto_sync.wake_up)
			return;
		if (t.notified) {
			t.notified = false;
			catch_up = true;
			timestamps_update(ts);
		}
		t.clock_sync.wake_up = false;
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
	if (t.current_step != target_step) {
		Animation a = animation_init(t.current_step, target_step, cfg.temp_auto_fps, animation_s * 1000);
		temp_animation_loop(t, a, -1, t.current_step, target_step);
	}

	temp_adjust_loop(t, ts, !catch_up);
}

void core::temp_animation_loop(Temp_Manager &t, Animation a, int prev_step, int cur_step, int target_step)
{
	using namespace std::chrono;
	using namespace std::chrono_literals;

	if (cur_step == target_step)
		return;	
	if (t.notified || !cfg.temp_auto || t.auto_sync.wake_up)
		return;

	a.elapsed += a.slice;
	cur_step = int(ease_in_out_quad(a.elapsed, a.start_step, a.diff, a.duration_s));
	t.current_step = cur_step;

	if (cur_step != prev_step) {
		for (size_t i = 0; i < t.xorg->scr_count(); ++i) {
			if (cfg.screens[i].temp_auto) {
				cfg.screens[i].temp_step = cur_step;
				t.xorg->set_gamma(i,
				                  cfg.screens[i].brt_step,
				                  cur_step);
			}
		}
	}

	std::this_thread::sleep_for(milliseconds(1000 / a.fps));
	temp_animation_loop(t, a, cur_step, cur_step, target_step);
}

void core::temp_on_system_wakeup(Temp_Manager &t)
{
	const std::string service   = "org.freedesktop.login1";
	const std::string obj_path  = "/org/freedesktop/login1";
	const std::string interface = "org.freedesktop.login1.Manager";
	const std::string signal    = "PrepareForSleep";
	static auto proxy = sdbus::createProxy(service, obj_path);
	try {
		proxy->registerSignalHandler(interface, signal, [&t] (sdbus::Signal &sig) {
			bool going_to_sleep;
			sig >> going_to_sleep;
			if (!going_to_sleep)
				temp_notify(t);
		});
		proxy->finishRegistration();
	} catch (sdbus::Error e) {
		syslog(LOG_ERR, "sdbus error: %s", e.what());
	}
}

core::Brightness_Manager::Brightness_Manager(Xorg &xorg)
     : backlights(Sysfs::get_bl()),
       als(Sysfs::get_als())
{
	monitors.reserve(xorg.scr_count());
	threads.reserve(xorg.scr_count());

	for (size_t i = 0; i < xorg.scr_count(); ++i) {
		monitors.emplace_back(&xorg,
		                      i < xorg.scr_count() ? &backlights[i] : nullptr,
		                      als.size() > 0 ? &als[0] : nullptr,
		                      &als_ev,
		                      i);
	}

	assert(monitors.size() == xorg.scr_count());
}

void core::Brightness_Manager::start()
{
	als_stop.wake_up = false;
	als_ev.wake_up = false;
	if (als.size() > 0)
		threads.emplace_back([&] { als_capture_loop(als[0], als_stop, als_ev); });
	for (auto &m : monitors)
		threads.emplace_back([&] { monitor_init(m); });
}

void core::Brightness_Manager::stop()
{
	als_capture_stop(als_stop);
	als_capture_stop(als_ev);
	for (auto &m : monitors)
		monitor_stop(m);
	for (auto &t : threads)
		t.join();
}

void core::als_capture_loop(Sysfs::ALS &als, Sync &stop, Sync &ev)
{
	const int prev_step = als.lux_step();
	als.update();
	if (prev_step != als.lux_step())
		als_notify(ev);
	{
		std::unique_lock lk(stop.mtx);
		stop.cv.wait_for(lk, std::chrono::milliseconds(1000));
	}
	if (stop.wake_up)
		return;
	als_capture_loop(als, stop, ev);
}

void core::als_capture_stop(Sync &stop)
{
	stop.wake_up = true;
	stop.cv.notify_one();
}

void core::als_notify(Sync &ev)
{
	std::lock_guard lk(ev.mtx);
	ev.wake_up = true;
	ev.cv.notify_one();
}

int core::als_await(Sysfs::ALS &als, Sync &ev)
{
	std::unique_lock lk(ev.mtx);
	ev.cv.wait(lk, [&] { return ev.wake_up; });
	ev.wake_up = false;
	return als.lux_step();
}

core::Monitor::Monitor(Xorg *xorg,
		Sysfs::Backlight *bl,
        Sysfs::ALS *als,
        Sync *als_ev,
		int id)
   :  xorg(xorg),
      backlight(bl),
      als(als),
      als_ev(als_ev),
      id(id),
      ss_brt(0),
      flags({cfg.screens[id].brt_mode == MANUAL,0,0})
{
	if (!backlight) {
		xorg->set_gamma(id,
		                brt_steps_max,
		                cfg.screens[id].temp_step);
	}
}

core::Monitor::Monitor(Monitor &&o)
    :  xorg(o.xorg),
       backlight(o.backlight),
       id(o.id),
       flags(o.flags)
{
}

void core::monitor_init(Monitor &mon)
{
	Sync brt_ev;
	brt_ev.wake_up = false;
	std::thread adjust_thr([&] {
		monitor_brt_adjust_loop(mon, brt_ev, brt_steps_max);
	});
	monitor_is_auto_loop(mon, brt_ev);
	adjust_thr.join();
}

void core::monitor_is_auto_loop(Monitor &mon, Sync &brt_ev)
{
	std::mutex mtx; {
		std::unique_lock lk(mtx);
		mon.cv.wait(lk, [&] { return !mon.flags.paused; });
	}
	if (mon.flags.stopped) {
		{ std::lock_guard lk(brt_ev.mtx);
		brt_ev.wake_up = true; }
		brt_ev.cv.notify_one();
		return;
	}
	monitor_capture_loop(mon, brt_ev, *mon.als_ev, Previous_capture_state{0,0,0,0}, 0);
	monitor_is_auto_loop(mon, brt_ev);
}

void core::monitor_capture_loop(Monitor &mon, Sync &brt_ev, Sync &als_ev, Previous_capture_state prev, int ss_delta)
{
	const auto &scr = cfg.screens[mon.id];
	const int ss_brt = [&] {
		if (scr.brt_mode == ALS)
			return als_await(*mon.als, als_ev);
		return mon.xorg->get_screen_brightness(mon.id);
	}();
	if (mon.flags.paused || mon.flags.stopped)
		return;
	ss_delta += abs(prev.ss_brt - ss_brt);

	if (ss_delta > scr.brt_auto_threshold) {
		ss_delta = 0;
		{
			std::lock_guard lk(brt_ev.mtx);
			brt_ev.wake_up = true;
			mon.ss_brt = ss_brt;
		}
		brt_ev.cv.notify_one();
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

	if (scr.brt_mode == SCREENSHOT)
		std::this_thread::sleep_for(std::chrono::milliseconds(scr.brt_auto_polling_rate));
	monitor_capture_loop(mon, brt_ev, als_ev, prev, ss_delta);
}

void core::monitor_brt_adjust_loop(Monitor &mon, Sync &brt_ev, int cur_step)
{
	int ss_brt; {
		std::unique_lock lk(brt_ev.mtx);
		brt_ev.cv.wait(lk, [&] { return brt_ev.wake_up; });
		brt_ev.wake_up = false;
		ss_brt = mon.ss_brt;
	}

	if (mon.flags.stopped)
		return;

	const auto &scr = cfg.screens[mon.id];
	const int target_step = [&] {
		if (mon.als && scr.brt_mode == ALS)
			return calc_brt_target_als(ss_brt, scr.brt_auto_min, scr.brt_auto_max, scr.brt_auto_offset);
		else
			return calc_brt_target(ss_brt, scr.brt_auto_min, scr.brt_auto_max, scr.brt_auto_offset);
	}();

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

	monitor_brt_adjust_loop(mon, brt_ev, cur_step);
}

int core::monitor_brt_animation_loop(Monitor &mon, Animation a, int prev_step, int cur_step, int target_step, int ss_brt)
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

void core::monitor_stop(Monitor &mon)
{
	mon.flags.paused = false;
	mon.flags.stopped = true;
	mon.cv.notify_one();
}

void core::monitor_pause(Monitor &mon)
{
	mon.flags.paused = true;
	als_notify(*mon.als_ev);
}

void core::monitor_resume(Monitor &mon)
{
	mon.flags.paused = false;
	mon.cv.notify_one();
	als_notify(*mon.als_ev);
}

void core::monitor_toggle(Monitor &mon, bool toggle)
{
	if (toggle)
		monitor_resume(mon);
	else
		monitor_pause(mon);
}

int core::calc_brt_target_als(int als_brt, int min, int max, int offset)
{
	const int offset_step = offset * brt_steps_max / max;
	return std::clamp(als_brt + offset_step, min, max);
}

int core::calc_brt_target(int ss_brt, int min, int max, int offset)
{
	const int ss_step     = ss_brt * brt_steps_max / 255;
	const int offset_step = offset * brt_steps_max / max;
	return std::clamp(brt_steps_max - ss_step + offset_step, min, max);
}

core::Gamma_Refresh::Gamma_Refresh() : _quit(false)
{
}

void core::Gamma_Refresh::stop()
{
	_quit = true;
	_cv.notify_one();
}

void core::Gamma_Refresh::loop(Xorg &xorg)
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
