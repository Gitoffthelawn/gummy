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

TempCtl::TempCtl(Xorg *xorg)
    : _thr(std::make_unique<std::thread>([&] { temp_loop(xorg); }))
{
	notify_on_wakeup();

	if (cfg.temp_auto) {
		for (size_t i = 0; i < cfg.screens.size(); ++i) {
			if (cfg.screens[i].temp_auto) {
				cfg.screens[i].temp_step = 0;
			}
		}
	}

	xorg->setGamma();
}

TempCtl::~TempCtl()
{
	quit();
	_thr->join();
}

void TempCtl::notify()
{
	_force = true;
	_temp_cv.notify_one();
}

void TempCtl::quit()
{
	_quit = true;
	_temp_cv.notify_one();
}

int TempCtl::current_step() const
{
	return _current_step;
}

void TempCtl::temp_loop(Xorg *server)
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

				for (int i = 0; i < server->screenCount(); ++i) {

					if (cfg.screens[i].temp_auto) {

						cfg.screens[i].temp_step = _current_step;
						server->setGamma(
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

void TempCtl::notify_on_wakeup()
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

ScreenCtl::ScreenCtl(Xorg *server)
    : _server(server),
      _temp_ctl(server),
      _devices(Sysfs::getDevices()),
      _gamma_refresh_thr(std::make_unique<std::thread>([this] { reapplyGamma(); }))
{
	_monitors.reserve(_server->screenCount());
	for (int i = 0; i < _server->screenCount(); ++i) {
		Device *dev = nullptr;
		if (_devices.size() > size_t(i))
			dev = &_devices[i];
		_monitors.emplace_back(_server, dev, i);
	}
	assert(_monitors.size() == size_t(_server->screenCount()));
}

ScreenCtl::~ScreenCtl()
{
	_temp_ctl.quit();
	_quit = true;
	_gamma_refresh_cv.notify_one();
	_gamma_refresh_thr->join();
}

void ScreenCtl::reapplyGamma()
{
	using namespace std::chrono;
	using namespace std::chrono_literals;

	std::mutex mtx;
	while (true) {
		{
			std::unique_lock lk(mtx);
			_gamma_refresh_cv.wait_until(lk, system_clock::now() + 10s, [this] {
				return _quit;
			});
		}
		if (_quit)
			break;
		_server->setGamma();
	}
}

Options::Options(const std::string &in)
{
	json msg = json::parse(in);
	scr_no               = msg["scr_no"];
	brt_perc             = msg["brt_perc"];
	brt_auto             = msg["brt_mode"];
	brt_auto_min         = msg["brt_auto_min"];
	brt_auto_max         = msg["brt_auto_max"];
	brt_auto_offset      = msg["brt_auto_offset"];
	brt_auto_speed       = msg["brt_auto_speed"];
	screenshot_rate_ms   = msg["brt_auto_screenshot_rate"];
	temp_k               = msg["temp_k"];
	temp_auto            = msg["temp_mode"];
	temp_day_k           = msg["temp_day_k"];
	temp_night_k         = msg["temp_night_k"];
	sunrise_time         = msg["sunrise_time"];
	sunset_time          = msg["sunset_time"];
	temp_adaptation_time = msg["temp_adaptation_time"];
}

void ScreenCtl::applyOptions(const std::string &json)
{
	Options opts(json);
	bool notify_temp = false;

	// Non-screen specific options
	{
		if (opts.temp_day_k != -1) {
			cfg.temp_auto_high = opts.temp_day_k;
			notify_temp = true;
		}

		if (opts.temp_night_k != -1) {
			cfg.temp_auto_low = opts.temp_night_k;
			notify_temp = true;
		}

		if (!opts.sunrise_time.empty()) {
			cfg.temp_auto_sunrise = opts.sunrise_time;
			notify_temp = true;
		}

		if (!opts.sunset_time.empty()) {
			cfg.temp_auto_sunset = opts.sunset_time;
			notify_temp = true;
		}

		if (opts.temp_adaptation_time != -1) {
			cfg.temp_auto_speed = opts.temp_adaptation_time;
			notify_temp = true;
		}
	}

	int start = 0;
	int end = _server->screenCount() - 1;

	if (opts.scr_no != -1) {
		if (opts.scr_no > _server->screenCount() - 1) {
			syslog(LOG_ERR, "Screen %d not available", opts.scr_no);
			return;
		}
		start = end = opts.scr_no;
	} else {
		if (opts.temp_k != -1) {
			cfg.temp_auto = false;
			notify_temp = true;
		} else if (opts.temp_auto != -1) {
			cfg.temp_auto = bool(opts.temp_auto);
			notify_temp = true;
		}
	}

	for (int i = start; i <= end; ++i) {

		if (opts.brt_auto != -1) {
			cfg.screens[i].brt_auto = bool(opts.brt_auto);
			_monitors[i].notify();
		}

		if (opts.brt_perc != -1) {
			cfg.screens[i].brt_auto = false;
			_monitors[i].notify();

			if (size_t(i) < _devices.size()) {
				cfg.screens[i].brt_step = brt_steps_max;
				_devices[i].setBacklight(opts.brt_perc * 255 / 100);
			} else {
				cfg.screens[i].brt_step = int(remap(opts.brt_perc, 0, 100, 0, brt_steps_max));
			}
		}

		if (opts.brt_auto_min != -1) {
			cfg.screens[i].brt_auto_min = int(remap(opts.brt_auto_min, 0, 100, 0, brt_steps_max));
		}

		if (opts.brt_auto_max != -1) {
			cfg.screens[i].brt_auto_max = int(remap(opts.brt_auto_max, 0, 100, 0, brt_steps_max));
		}

		if (opts.brt_auto_offset != -1) {
			cfg.screens[i].brt_auto_offset = int(remap(opts.brt_auto_offset, 0, 100, 0, brt_steps_max));
		}

		if (opts.brt_auto_speed != -1) {
			cfg.screens[i].brt_auto_speed = opts.brt_auto_speed;
		}

		if (opts.screenshot_rate_ms != -1) {
			cfg.screens[i].brt_auto_polling_rate = opts.screenshot_rate_ms;
		}

		if (opts.temp_k != -1) {
			cfg.screens[i].temp_step = int(remap(opts.temp_k, temp_k_min, temp_k_max, 0, temp_steps_max));
			cfg.screens[i].temp_auto = false;
		} else if (opts.temp_auto != -1) {
			cfg.screens[i].temp_auto = bool(opts.temp_auto);
			if (opts.temp_auto == 1) {
				cfg.screens[i].temp_step = _temp_ctl.current_step();
			}
		}

		_server->setGamma(
		    i,
		    cfg.screens[i].brt_step,
		    cfg.screens[i].temp_step
		);
	}

	if (notify_temp)
		_temp_ctl.notify();
}

Monitor::Monitor(Xorg* server, Device* device, const int id)
   :  _server(server),
      _device(device),
      _id(id),
      _thr(std::make_unique<std::thread>([this] { capture(); }))
{
}

Monitor::Monitor(Monitor &&o)
    :  _server(o._server),
       _device(o._device),
       _id(o._id)
{
	_thr.swap(o._thr);
}

Monitor::~Monitor()
{
	_quit = true;
	_ss_cv.notify_one();
	_thr->join();
}

void Monitor::notify()
{
	_force = true;
	_ss_cv.notify_one();
}

void Monitor::capture()
{
	using namespace std::this_thread;
	using namespace std::chrono;

	std::condition_variable brt_cv;
	std::thread brt_thr([&] { adjustBrightness(brt_cv); });

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

			const int ss_brt = _server->getScreenBrightness(_id);

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

void Monitor::adjustBrightness(std::condition_variable &brt_cv)
{
	using namespace std::this_thread;
	using namespace std::chrono;
	using namespace std::chrono_literals;

	int cur_step = brt_steps_max;

	if (!_device) {
		_server->setGamma(_id, brt_steps_max, cfg.screens[_id].temp_step);
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
			_device->setBacklight(cur_step * _device->max_brt / brt_steps_max);
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

				_server->setGamma(
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
