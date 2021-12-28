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

TempCtl::TempCtl(Xorg *xorg)
 : _thr(std::make_unique<std::thread>([&] { adjust(xorg); }))
{ }

TempCtl::~TempCtl()
{
	notify(true);
	_thr->join();
}

void TempCtl::notify(bool quit)
{
	_force_change = true;
	_quit = quit;
	_temp_cv.notify_one();
}

int TempCtl::getCurrentStep()
{
	return _current_step;
}

void TempCtl::adjust(Xorg *server)
{
	using namespace std::this_thread;
	using namespace std::chrono;
	using namespace std::chrono_literals;

	updateInterval();

	bool needs_change = cfg["temp_auto"].get<bool>();

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
				break;

			if (!cfg["temp_auto"].get<bool>())
				continue;

			{
				std::lock_guard lk(temp_mtx);
				needs_change = true;
			}

			_temp_cv.notify_one();
		}
	});

	_current_step = 0;
	bool first_step = true;

	while (true) {
		{
			std::unique_lock<std::mutex> lock(temp_mtx);

			_temp_cv.wait(lock, [&] {
				return needs_change || !first_step || _force_change || _quit;
			});

			if (_quit)
				break;

			if (_force_change) {
				updateInterval();
				_force_change = false;
				first_step = true;
			}

			needs_change = false;
		}

		if (!cfg["temp_auto"].get<bool>())
			continue;

		const double adaptation_time_s = cfg["temp_auto_speed"].get<double>() * 60;

		_cur_time = std::time(nullptr);

		bool daytime = _cur_time >= _sunrise_time && _cur_time < _sunset_time;
		int target_temp;
		long tmp;

		if (daytime) {
			target_temp = cfg["temp_auto_high"];
			tmp = _sunrise_time;
		} else {
			target_temp = cfg["temp_auto_low"];
			tmp = _sunset_time;
		}

		int time_since_start_s = std::abs(_cur_time - tmp);

		if (time_since_start_s > adaptation_time_s)
			time_since_start_s = adaptation_time_s;

		double animation_s = 2;

		if (first_step) {
			if (daytime) {
				target_temp = remap(
				    time_since_start_s, 0, adaptation_time_s, cfg["temp_auto_low"], cfg["temp_auto_high"]
				);
			} else {
				target_temp = remap(
				    time_since_start_s, 0, adaptation_time_s, cfg["temp_auto_high"], cfg["temp_auto_low"]
				);
			}
		} else {
			animation_s = adaptation_time_s - time_since_start_s;
			if (animation_s < 2)
				animation_s = 2;
		}

		int target_step = int(remap(target_temp, temp_k_max, temp_k_min, temp_steps_max, 0));

		if (_current_step == target_step) {
			first_step = true;
			continue;
		}

		const int FPS      = cfg["temp_auto_fps"];
		const int diff     = target_step - _current_step;
		const double slice = 1. / FPS;

		double time   = 0;
		int step      = -1;
		int prev_step = -1;

		while (step != target_step) {

			if (_force_change || !cfg["temp_auto"].get<bool>() || _quit)
				break;

			time += slice;

			step = int(easeInOutQuad(time, _current_step, diff, animation_s));

			if (step != prev_step) {

				for (int i = 0; i < server->screenCount(); ++i) {

					if (cfg["screens"][i]["temp_auto"].get<bool>()) {

						cfg["screens"][i]["temp_step"] = step;
						server->setGamma(
						    i,
						    cfg["screens"][i]["brt_step"],
						    step
						);
					}
				}
			}

			prev_step = step;

			sleep_for(milliseconds(1000 / FPS));
		}

		_current_step = step;
		first_step = false;
	}

	clock_cv.notify_one();
	clock.join();
}

void TempCtl::updateInterval()
{
	// Get current timestamp
	_cur_time = std::time(nullptr);

	// Get tm struct from it
	std::tm *cur_tm = std::localtime(&_cur_time);

	// Set hour and min for sunrise
	std::string sr(cfg["temp_auto_sunrise"]);
	cur_tm->tm_hour = std::stoi(sr.substr(0, 2));
	cur_tm->tm_min  = std::stoi(sr.substr(3, 2));

	// Subtract adaptation time
	cur_tm->tm_sec  = 0;
	cur_tm->tm_sec -= int(cfg["temp_auto_speed"].get<double>() * 60);

	_sunrise_time = std::mktime(cur_tm);

	// Set hour and min for sunset
	std::string sn(cfg["temp_auto_sunset"]);
	cur_tm->tm_hour = std::stoi(sn.substr(0, 2));
	cur_tm->tm_min  = std::stoi(sn.substr(3, 2));

	// Subtract adaptation time
	cur_tm->tm_sec  = 0;
	cur_tm->tm_sec -= int(cfg["temp_auto_speed"].get<double>() * 60);

	_sunset_time = std::mktime(cur_tm);
}

struct Options
{
    Options(std::string in)
	{
		json msg = json::parse(in);
		scr_no          = msg["scr_no"];
		brt_perc        = msg["brt_perc"];
		brt_auto        = msg["brt_mode"];
		brt_auto_min    = msg["brt_auto_min"];
		brt_auto_max    = msg["brt_auto_max"];
		brt_auto_offset = msg["brt_auto_offset"];
		brt_auto_speed  = msg["brt_auto_speed"];
		screenshot_rate_ms = msg["brt_auto_screenshot_rate"];
		temp_k          = msg["temp_k"];
		temp_auto       = msg["temp_mode"];
		temp_day_k      = msg["temp_day_k"];
		temp_night_k    = msg["temp_night_k"];
		sunrise_time    = msg["sunrise_time"];
		sunset_time     = msg["sunset_time"];
		temp_adaptation_time = msg["temp_adaptation_time"];
	}
	int scr_no          = -1;
	int brt_perc        = -1;
	int brt_auto        = -1;
	int brt_auto_min    = -1;
	int brt_auto_max    = -1;
	int brt_auto_offset = -1;
	int brt_auto_speed  = -1;
	int screenshot_rate_ms = -1;
	int temp_auto       = -1;
	int temp_k          = -1;
	int temp_day_k      = -1;
	int temp_night_k    = -1;
	int temp_adaptation_time = -1;
	std::string sunrise_time;
	std::string sunset_time;
};

ScreenCtl::ScreenCtl(Xorg *server)
    : _server(server),
      _devices(Sysfs::getDevices()),
      _temp_ctl(server)
{
	registerWakeupSig();

	_monitors.reserve(_server->screenCount());
	for (int i = 0; i < _server->screenCount(); ++i) {

		Device *dev = nullptr;
		if (_devices.size() > size_t(i))
			dev = &_devices[i];

		_monitors.emplace_back(_server, dev, i);
	}

	assert(_monitors.size() == size_t(_server->screenCount()));

	if (cfg["temp_auto"].get<bool>()) {
		for (size_t i = 0; i < _monitors.size(); ++i) {
			if (cfg["screens"][i]["temp_auto"].get<bool>()) {
				cfg["screens"][i]["temp_step"] = 0;
			}
		}
	}

	_server->setGamma();
	_threads.emplace_back([this] { reapplyGamma(); });
}

void ScreenCtl::registerWakeupSig()
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
			if (going_to_sleep)
				return;
			_temp_ctl.notify();
		});
		_dbus_proxy->finishRegistration();
	} catch (sdbus::Error e) {
		syslog(LOG_ERR, "sdbus error: %s", e.what());
	}
}

ScreenCtl::~ScreenCtl()
{
	_quit = true;
	_temp_ctl.notify(true);
	_gamma_refresh_cv.notify_one();
	for (auto &t : _threads)
		t.join();
}

void ScreenCtl::notifyMonitor(int scr_idx)
{
	_monitors[scr_idx].notify();
}

void ScreenCtl::reapplyGamma()
{
	using namespace std::this_thread;
	using namespace std::chrono;
	using namespace std::chrono_literals;

	std::mutex mtx;

	while (true) {
		{
			std::unique_lock<std::mutex> lock(mtx);
			_gamma_refresh_cv.wait_until(lock, system_clock::now() + 10s, [&] {
				return _quit;
			});
		}
		if (_quit)
			break;
		_server->setGamma();
	}
}

void ScreenCtl::applyOptions(const std::string &json)
{
	Options opts(json);
	bool notify_temp = false;

	// Non-screen specific options
	{
		if (opts.temp_day_k != -1) {
			cfg["temp_auto_high"] = opts.temp_day_k;
			notify_temp = true;
		}

		if (opts.temp_night_k != -1) {
			cfg["temp_auto_low"] = opts.temp_night_k;
			notify_temp = true;
		}

		if (!opts.sunrise_time.empty()) {
			cfg["temp_auto_sunrise"] = opts.sunrise_time;
			notify_temp = true;
		}

		if (!opts.sunset_time.empty()) {
			cfg["temp_auto_sunset"] = opts.sunset_time;
			notify_temp = true;
		}

		if (opts.temp_adaptation_time != -1) {
			cfg["temp_auto_speed"] = opts.temp_adaptation_time;
			notify_temp = true;
		}
	}

	int start = 0;
	int end = _server->screenCount();

	if (opts.scr_no != -1) {
		if (opts.scr_no > _server->screenCount() - 1) {
			syslog(LOG_ERR, "Screen %d not available", opts.scr_no);
			return;
		}
		start = opts.scr_no;
		end = start + 1;
	} else {
		if (opts.temp_k != -1) {
			cfg["temp_auto"] = false;
			notify_temp = true;
		} else if (opts.temp_auto != -1) {
			cfg["temp_auto"] = bool(opts.temp_auto);
			notify_temp = true;
		}
	}

	for (int i = start; i < end; ++i) {

		if (opts.brt_auto != -1) {
			cfg["screens"][i]["brt_auto"] = bool(opts.brt_auto);
			notifyMonitor(i);
		}

		if (opts.brt_perc != -1) {
			cfg["screens"][i]["brt_auto"] = false;
			notifyMonitor(i);

			if(_monitors[i].hasBacklight()) {
				cfg["screens"][i]["brt_step"] = brt_steps_max;
				_monitors[i].setBacklight(opts.brt_perc);
			} else {
				cfg["screens"][i]["brt_step"] = int(remap(opts.brt_perc, 0, 100, 0, brt_steps_max));
			}
		}

		if (opts.brt_auto_min != -1) {
			cfg["screens"][i]["brt_auto_min"] = int(remap(opts.brt_auto_min, 0, 100, 0, brt_steps_max));
		}

		if (opts.brt_auto_max != -1) {
			cfg["screens"][i]["brt_auto_max"] = int(remap(opts.brt_auto_max, 0, 100, 0, brt_steps_max));
		}

		if (opts.brt_auto_offset != -1) {
			cfg["screens"][i]["brt_auto_offset"] = int(remap(opts.brt_auto_offset, 0, 100, 0, brt_steps_max));
		}

		if (opts.brt_auto_speed != -1) {
			cfg["screens"][i]["brt_auto_speed"] = opts.brt_auto_speed;
		}

		if (opts.screenshot_rate_ms != -1) {
			cfg["screens"][i]["brt_auto_polling_rate"] = opts.screenshot_rate_ms;
		}

		if (opts.temp_k != -1) {
			cfg["screens"][i]["temp_step"] = int(remap(opts.temp_k, temp_k_min, temp_k_max, 0, temp_steps_max));
			cfg["screens"][i]["temp_auto"] = false;
		} else if (opts.temp_auto != -1) {
			cfg["screens"][i]["temp_auto"] = bool(opts.temp_auto);
			if (opts.temp_auto == 1) {
				cfg["screens"][i]["temp_step"] = _temp_ctl.getCurrentStep();
			}
		}

		_server->setGamma(
		    i,
		    cfg["screens"][i]["brt_step"],
		    cfg["screens"][i]["temp_step"]
		);
	}

	if (notify_temp)
		_temp_ctl.notify(false);
}

Monitor::Monitor(Xorg* server, Device* device, int scr_idx)
    : _server(server),
      _device(device),
      _scr_idx(scr_idx),
      _thr(std::make_unique<std::thread>([this] { capture(); }))
{

}

// This won't ever be called as the storage is static
Monitor::Monitor(Monitor &&o)
    : _server(o._server),
      _scr_idx(o._scr_idx)
{
	_thr.swap(o._thr);
}

Monitor::~Monitor()
{
	_quit = true;
	_ss_cv.notify_one();
	_thr->join();
}

bool Monitor::hasBacklight()
{
	return _device != nullptr;
}

void Monitor::setBacklight(int perc)
{
	return _device->setBacklight(perc * 255 / 100);
}

void Monitor::notify()
{
	_ss_cv.notify_one();
}

void Monitor::capture()
{
	using namespace std::this_thread;
	using namespace std::chrono;

	std::condition_variable brt_cv;
	std::thread brt_thr([&] { adjustBrightness(brt_cv); });
	std::mutex  mtx;

	int img_delta = 0;
	bool force = false;
	int prev_ss_brt = 0,
	    prev_min    = 0,
	    prev_max    = 0,
	    prev_offset = 0;

	while (true) {
		{
			std::unique_lock lock(mtx);
			_ss_cv.wait(lock, [&] {
				return cfg["screens"][_scr_idx]["brt_auto"].get<bool>() || _quit;
			});
		}

		if (_quit)
			break;

		if (cfg["screens"][_scr_idx]["brt_auto"].get<bool>())
			force = true;
		else
			continue;

		while (cfg["screens"][_scr_idx]["brt_auto"].get<bool>() && !_quit) {

			const int ss_brt = _server->getScreenBrightness(_scr_idx);

			img_delta += abs(prev_ss_brt - ss_brt);

			if (img_delta > cfg["screens"][_scr_idx]["brt_auto_threshold"].get<int>() || force) {

				img_delta = 0;
				force = false;

				{
					std::lock_guard lock(_brt_mtx);
					_ss_brt = ss_brt;
					_brt_needs_change = true;
				}

				brt_cv.notify_one();
			}

			if (   cfg["screens"][_scr_idx]["brt_auto_min"] != prev_min
			|| cfg["screens"][_scr_idx]["brt_auto_max"] != prev_max
			|| cfg["screens"][_scr_idx]["brt_auto_offset"] != prev_offset)
				force = true;

			prev_ss_brt = ss_brt;
			prev_min    = cfg["screens"][_scr_idx]["brt_auto_min"];
			prev_max    = cfg["screens"][_scr_idx]["brt_auto_max"];
			prev_offset = cfg["screens"][_scr_idx]["brt_auto_offset"];

			sleep_for(milliseconds(cfg["screens"][_scr_idx]["brt_auto_polling_rate"].get<int>()));
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
		_server->setGamma(_scr_idx, cur_step, cfg["screens"][_scr_idx]["temp_step"]);
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
		const int offset = cfg["screens"][_scr_idx]["brt_auto_offset"].get<int>()
		        * brt_steps_max
		        / cfg["screens"][_scr_idx]["brt_auto_max"].get<int>();

		const int target_step = std::clamp(
		    brt_steps_max - ss_step + offset,
		    cfg["screens"][_scr_idx]["brt_auto_min"].get<int>(),
		    cfg["screens"][_scr_idx]["brt_auto_max"].get<int>()
		);

		if (cur_step == target_step) {
			continue;
		}

		if (_device) {
			cur_step = target_step;
			_device->setBacklight(cur_step * _device->max_brt / brt_steps_max);
			continue;
		}

		const int    FPS         = cfg["brt_auto_fps"];
		const double slice       = 1. / FPS;
		const double animation_s = cfg["screens"][_scr_idx]["brt_auto_speed"].get<double>() / 1000;
		const int    diff        = target_step - cur_step;

		double time   = 0;
		int step      = -1;
		int prev_step = -1;

		while (step != target_step) {

			if (_brt_needs_change || !cfg["screens"][_scr_idx]["brt_auto"].get<bool>() || _quit)
				break;

			time += slice;

			step = int(std::round(
			    easeOutExpo(time, cur_step, diff, animation_s))
			);

			if (step != prev_step) {

				cfg["screens"][_scr_idx]["brt_step"] = step;

				_server->setGamma(
				    _scr_idx,
				    step,
				    cfg["screens"][_scr_idx]["temp_step"]
				);
			}

			prev_step = step;

			sleep_for(milliseconds(1000 / FPS));
		}

		cur_step = step;
	}
}
