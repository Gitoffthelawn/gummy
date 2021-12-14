#include "screenctl.h"
#include "cfg.h"
#include "../common/utils.h"
#include <mutex>
#include <ctime>
#include <plog/Log.h>
#include <sdbus-c++/sdbus-c++.h>

ScreenCtl::ScreenCtl(Xorg *server)
    : m_server(server),
      m_devices(Sysfs::getDevices())
{
	listenWakeupSignal();
	m_threads.emplace_back([this] { adjustTemperature(); });

	m_monitors.reserve(m_server->screenCount());
	for (int i = 0; i < m_server->screenCount(); ++i) {

		Device *dev = nullptr;
		if (m_devices.size() > size_t(i))
			dev = &m_devices[i];

		m_monitors.emplace_back(m_server, dev, i);
	}

	if (cfg["temp_auto"].get<bool>()) {
		for (size_t i = 0; i < m_monitors.size(); ++i) {
			if (cfg["screens"][i]["temp_auto"].get<bool>()) {
				cfg["screens"][i]["temp_step"] = 0;
			}
		}
	}

	m_server->setGamma();
	//m_threads.emplace_back([this] { reapplyGamma(); });
}

bool ScreenCtl::listenWakeupSignal()
{
	const std::string service("org.freedesktop.login1");
	const std::string obj_path("/org/freedesktop/login1");
	const std::string interface("org.freedesktop.login1.Manager");
	const std::string signal("PrepareForSleep");

	m_dbus_proxy = sdbus::createProxy(service, obj_path);

	m_dbus_proxy->registerSignalHandler(interface, signal, [this] (sdbus::Signal &sig) {

		bool going_to_sleep;
		sig >> going_to_sleep;

		if (going_to_sleep)
			return;

		LOGD << "System wakeup. Forcing temp change";
		m_force_temp_change = true;
		m_temp_cv.notify_one();
	});

	m_dbus_proxy->finishRegistration();

	LOGD << "Registered wakeup signal";

	return true;
}


ScreenCtl::~ScreenCtl()
{
	m_quit = true;
	m_temp_cv.notify_one();
	m_gamma_refresh_cv.notify_one();
	for (auto &t : m_threads)
		t.join();
}

void ScreenCtl::notifyTemp()
{
	m_force_temp_change = true;
	m_temp_cv.notify_one();
}

void ScreenCtl::notifyMonitor(int scr_idx)
{
	m_monitors[scr_idx].notify();
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
			m_gamma_refresh_cv.wait_until(lock, system_clock::now() + 5s, [&] {
				return m_quit;
			});
		}
		if (m_quit)
			break;
		m_server->setGamma();
	}
}

Monitor::Monitor(Xorg* server, Device* device, int scr_idx)
    : m_server(server),
      m_device(device),
      m_scr_idx(scr_idx),
      m_ss_thr(std::make_unique<std::thread>([this] { capture(); }))
{

}

// This won't ever be called as the storage is static
Monitor::Monitor(Monitor &&o)
    : m_server(o.m_server),
      m_scr_idx(o.m_scr_idx)
{
    m_ss_thr.swap(o.m_ss_thr);
}

Monitor::~Monitor()
{
    m_quit = true;
    m_ss_cv.notify_one();
    m_ss_thr->join();
}

void Monitor::notify()
{
	m_ss_cv.notify_one();
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
			m_ss_cv.wait(lock, [&] {
				return cfg["screens"][m_scr_idx]["brt_auto"].get<bool>() || m_quit;
			});
		}

		if (m_quit)
			break;

		if (cfg["screens"][m_scr_idx]["brt_auto"].get<bool>())
			force = true;
		else
			continue;

		while (cfg["screens"][m_scr_idx]["brt_auto"].get<bool>() && !m_quit) {

			const int ss_brt = m_server->getScreenBrightness(m_scr_idx);

			//LOGV << "scr " << m_scr_idx << ": " << ss_brt;

			img_delta += abs(prev_ss_brt - ss_brt);

			if (img_delta > cfg["screens"][m_scr_idx]["brt_auto_threshold"].get<int>() || force) {

				img_delta = 0;
				force = false;

				{
					std::lock_guard lock(m_brt_mtx);
					m_ss_brt = ss_brt;
					m_brt_needs_change = true;
				}

				brt_cv.notify_one();
			}

			if (   cfg["screens"][m_scr_idx]["brt_auto_min"] != prev_min
			    || cfg["screens"][m_scr_idx]["brt_auto_max"] != prev_max
			    || cfg["screens"][m_scr_idx]["brt_auto_offset"] != prev_offset)
				force = true;

			prev_ss_brt = ss_brt;
			prev_min    = cfg["screens"][m_scr_idx]["brt_auto_min"];
			prev_max    = cfg["screens"][m_scr_idx]["brt_auto_max"];
			prev_offset = cfg["screens"][m_scr_idx]["brt_auto_offset"];

			sleep_for(milliseconds(cfg["screens"][m_scr_idx]["brt_auto_polling_rate"].get<int>()));
		}
	}

	{
		std::lock_guard lock(m_brt_mtx);
		m_brt_needs_change = true;
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

	if (!m_device) {
		m_server->setGamma(m_scr_idx, cur_step, cfg["screens"][m_scr_idx]["temp_step"]);
	}

	while (true) {
		int ss_brt;

		{
			std::unique_lock lock(m_brt_mtx);

			brt_cv.wait(lock, [&] {
				return m_brt_needs_change;
			});

			if (m_quit)
				break;

			m_brt_needs_change = false;
			ss_brt = m_ss_brt;
		}

		const int ss_step = ss_brt * brt_steps_max / 255;

		// Offset relative to the max brightness. Only relevant on max brightness > 100%.
		const int offset = cfg["screens"][m_scr_idx]["brt_auto_offset"].get<int>()
		        * brt_steps_max
		        / cfg["screens"][m_scr_idx]["brt_auto_max"].get<int>();

		const int target_step = std::clamp(
		    brt_steps_max - ss_step + offset,
		    cfg["screens"][m_scr_idx]["brt_auto_min"].get<int>(),
		    cfg["screens"][m_scr_idx]["brt_auto_max"].get<int>()
		);

		if (cur_step == target_step) {
			LOGV << "[scr " << m_scr_idx << "] Brt already at target: " << target_step;
			continue;
		}

		if (m_device) {
			const int step = target_step * 255 / brt_steps_max;
			m_device->setBacklight(step);
			continue;
		}

		const int    FPS         = cfg["brt_auto_fps"];
		const double slice       = 1. / FPS;
		const double animation_s = cfg["brt_auto_speed"].get<double>() / 1000;
		const int    diff        = target_step - cur_step;

		LOGV << "scr " << m_scr_idx << " target_step: " << target_step;

		double time   = 0;
		int step      = -1;
		int prev_step = -1;

		while (step != target_step) {

			if (m_brt_needs_change || !cfg["screens"][m_scr_idx]["brt_auto"].get<bool>() || m_quit)
				break;

			time += slice;

			step = int(std::round(
			   easeOutExpo(time, cur_step, diff, animation_s))
			);

			if (step != prev_step) {

				cfg["screens"][m_scr_idx]["brt_step"] = step;

				m_server->setGamma(
				    m_scr_idx,
				    step,
				    cfg["screens"][m_scr_idx]["temp_step"]
				);
			}

			prev_step = step;

			sleep_for(milliseconds(1000 / FPS));
		}

		cur_step = step;
	}
}

int ScreenCtl::getAutoTempStep()
{
    return m_auto_temp_step;
}

/**
 * The temperature is adjusted in two steps.
 * The first one is for quickly catching up to the proper temperature when:
 * - time is checked sometime after the start time
 * - the system wakes up
 * - temperature settings change
 */
void ScreenCtl::adjustTemperature()
{
	using namespace std::this_thread;
	using namespace std::chrono;
	using namespace std::chrono_literals;

	std::time_t cur_time;
	std::time_t sunrise_time;
	std::time_t sunset_time;

	const auto updateInterval = [&cur_time, &sunrise_time, &sunset_time] {

		// Get current timestamp
		cur_time = std::time(nullptr);

		// Get tm struct from it
		std::tm *cur_tm = std::localtime(&cur_time);

		// Set hour and min for sunrise
		std::string sr(cfg["temp_auto_sunrise"]);
		cur_tm->tm_hour = std::stoi(sr.substr(0, 2));
		cur_tm->tm_min  = std::stoi(sr.substr(3, 2));

		// Subtract adaptation time
		cur_tm->tm_sec  = 0;
		cur_tm->tm_sec -= int(cfg["temp_auto_speed"].get<double>() * 60);

		sunrise_time = std::mktime(cur_tm);

		// Set hour and min for sunset
		std::string sn(cfg["temp_auto_sunset"]);
		cur_tm->tm_hour = std::stoi(sn.substr(0, 2));
		cur_tm->tm_min  = std::stoi(sn.substr(3, 2));

		// Subtract adaptation time
		cur_tm->tm_sec  = 0;
		cur_tm->tm_sec -= int(cfg["temp_auto_speed"].get<double>() * 60);

		sunset_time = std::mktime(cur_tm);
	};

	updateInterval();

	bool needs_change = cfg["temp_auto"].get<bool>();

	std::condition_variable clock_cv;
	std::mutex clock_mtx;
	std::mutex temp_mtx;

	std::thread clock([&] {
		while (true) {
			{
				std::unique_lock lk(clock_mtx);
				clock_cv.wait_until(lk, system_clock::now() + 60s, [&] {
					return m_quit;
				});
			}

			if (m_quit)
				break;

			if (!cfg["temp_auto"].get<bool>())
				continue;

			{
				std::lock_guard lk(temp_mtx);
				needs_change = true;
			}

			m_temp_cv.notify_one();
		}
	});

	m_auto_temp_step = 0;
	bool first_step = true;

	while (true) {
		{
			std::unique_lock<std::mutex> lock(temp_mtx);

			m_temp_cv.wait(lock, [&] {
				return needs_change || !first_step || m_force_temp_change || m_quit;
			});

			if (m_quit)
				break;

			if (m_force_temp_change) {
				updateInterval();
				m_force_temp_change = false;
				first_step = true;
			}

			needs_change = false;
		}

		if (!cfg["temp_auto"].get<bool>())
			continue;

		const double adaptation_time_s = cfg["temp_auto_speed"].get<double>() * 60;

		//LOGV << "cur_time: " << std::asctime(std::localtime(&cur_time));
		//LOGV << "sunrise: " << std::asctime(std::localtime(&sunrise_time));
		//LOGV << "sunset: " << std::asctime(std::localtime(&sunset_time));

		cur_time = std::time(nullptr);

		bool daytime = cur_time >= sunrise_time && cur_time < sunset_time;
		int target_temp;
		long tmp;

		if (daytime) {
			target_temp = cfg["temp_auto_high"];
			tmp = sunrise_time;
		} else {
			target_temp = cfg["temp_auto_low"];
			tmp = sunset_time;
		}

		int time_since_start_s = std::abs(cur_time - tmp);

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
			//LOGV << "First step. Target temp: " << target_temp;
		} else {
			//LOGV << "Second step.";
			animation_s = adaptation_time_s - time_since_start_s;
			if (animation_s < 2)
				animation_s = 2;
		}

		int target_step = int(remap(target_temp, temp_k_max, temp_k_min, temp_steps_max, 0));

		if (m_auto_temp_step == target_step) {
			LOGV << "Temp step already at target " << target_step;
			first_step = true;
			continue;
		}

		const int FPS      = cfg["temp_auto_fps"];
		const int diff     = target_step - m_auto_temp_step;
		const double slice = 1. / FPS;

		LOGV << "Adjusting temp: " << m_auto_temp_step << " -> " << target_step;
		//LOGV << "Seconds since the start (clamped by temp_auto_speed): " << time_since_start_s;
		//LOGV << "Final adjustment duration: " << duration_s / 60 << " min";

		double time   = 0;
		int step      = -1;
		int prev_step = -1;

		while (step != target_step) {

			if (m_force_temp_change || !cfg["temp_auto"].get<bool>() || m_quit)
				break;

			time += slice;

			step = int(easeInOutQuad(time, m_auto_temp_step, diff, animation_s));

			if (step != prev_step) {

				for (size_t i = 0; i < m_monitors.size(); ++i) {

					if (cfg["screens"][i]["temp_auto"].get<bool>()) {

						cfg["screens"][i]["temp_step"] = step;
						m_server->setGamma(
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

		m_auto_temp_step = step;
		first_step = false;
	}

	clock_cv.notify_one();
	clock.join();
}
