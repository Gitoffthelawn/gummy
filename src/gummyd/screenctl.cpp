#include "screenctl.h"
#include "cfg.h"
#include "../commons/utils.h"
#include <mutex>
#include <ctime>

ScreenCtl::ScreenCtl(Xorg *server)
    : m_server(server),
      m_devices(Sysfs::getDevices())
{
	//m_server->setGamma();
	//m_threads.emplace_back([this] { reapplyGamma(); });

	m_threads.emplace_back([this] { adjustTemperature(); });

	m_monitors.reserve(m_server->screenCount());
	for (int i = 0; i < m_server->screenCount(); ++i) {

		Device *dev = nullptr;
		if (m_devices.size() > size_t(i))
			dev = &m_devices[i];

		m_monitors.emplace_back(m_server, dev, i);
	}
}

ScreenCtl::~ScreenCtl()
{
	m_quit = true;
	m_temp_cv.notify_one();
	m_gamma_refresh_cv.notify_one();
	for (auto &t : m_threads)
		t.join();
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

// This won't ever be called as the storage is static
Monitor::Monitor(Monitor &&o)
    : m_server(o.m_server),
      m_scr_idx(o.m_scr_idx)
{
    m_ss_thr.swap(o.m_ss_thr);
}

Monitor::Monitor(Xorg* server, Device* device, int scr_idx)
    : m_server(server),
      m_device(device),
      m_scr_idx(scr_idx),
      m_ss_thr(std::make_unique<std::thread>([this] { capture(); }))
{

}

Monitor::~Monitor()
{
    m_quit = true;
    m_ss_cv.notify_one();
    m_ss_thr->join();
}

void Monitor::capture()
{
	using namespace std::this_thread;
	using namespace std::chrono;

	convar      brt_cv;
	std::thread brt_thr([&] { adjust(brt_cv); });
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

void Monitor::adjust(convar &brt_cv)
{
	using namespace std::this_thread;
	using namespace std::chrono;
	using namespace std::chrono_literals;

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

		json scr = cfg["screens"][m_scr_idx];

		const int cur_step = scr["brt_step"];
		const int ss_step  = ss_brt * brt_steps_max / 255;

		// Offset relative to the max brightness. Only relevant on max brightness > 100%.
		const int offset = scr["brt_auto_offset"].get<int>() * brt_steps_max / scr["brt_auto_max"].get<int>();

		const int target_step = std::clamp(
		    brt_steps_max - ss_step + offset,
		    scr["brt_auto_min"].get<int>(),
		    scr["brt_auto_max"].get<int>()
		);

		if (cur_step == target_step) {
			//LOGV << "Brt already at target for screen " << m_scr_idx << " (" << target_step << ')';
			continue;
		}

		if (m_device) {
			const int x = target_step * 255 / brt_steps_max;
			m_device->setBacklight(x);
		} else {

			const int    FPS         = cfg["brt_auto_fps"];
			const double slice       = 1. / FPS;
			const double duration_s  = cfg["brt_auto_speed"].get<double>() / 1000;
			const int    diff        = target_step - cur_step;

			double time   = 0;
			int prev_step = 0;

			//LOGV << "scr " << m_scr_idx << " target_step: " << target_step;

			while (cfg["screens"][m_scr_idx]["brt_step"].get<int>() != target_step) {

				if (m_brt_needs_change || !cfg["screens"][m_scr_idx]["brt_auto"].get<bool>() || m_quit)
					break;

				time += slice;

				cfg["screens"][m_scr_idx]["brt_step"] = int(std::round(
				    easeOutExpo(time, cur_step, diff, duration_s))
				);

				// Avoid expensive syscalls
				if (prev_step != cfg["screens"][m_scr_idx]["brt_step"]) {
					m_server->setGamma(
					    m_scr_idx,
					    cfg["screens"][m_scr_idx]["brt_step"],
					    cfg["screens"][m_scr_idx]["temp_step"]
					);
				}

				prev_step = cfg["screens"][m_scr_idx]["brt_step"];

				sleep_for(milliseconds(1000 / FPS));
			}
		}
	}
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

	std::time_t start_datetime;
	std::time_t end_datetime;

	const auto updateInterval = [&start_datetime, &end_datetime] {

		// Get current timestamp
		std::time_t cur_ts = std::time(nullptr);

		// Get tm struct from it
		std::tm *curtime = std::localtime(&cur_ts);
		curtime->tm_sec  = 0;

		// Set hour and min for start
		std::string t_start(cfg["temp_auto_sunset"]);
		curtime->tm_hour = std::stoi(t_start.substr(0, 2));
		curtime->tm_min  = std::stoi(t_start.substr(3, 2));

		// Subtract adaptation time from sunset time
		curtime->tm_sec -= int(cfg["temp_auto_speed"].get<double>() * 60);

		start_datetime = std::mktime(curtime);

		// Set hour and min for end
		std::string t_end(cfg["temp_auto_sunrise"]);
		curtime->tm_hour = std::stoi(t_end.substr(0, 2));
		curtime->tm_min  = std::stoi(t_end.substr(3, 2));

		// Assume end time is tomorrow
		curtime->tm_mday++;

		end_datetime = std::mktime(curtime);
	};

	updateInterval();

	bool needs_change = cfg["temp_auto"].get<bool>();

	convar     clock_cv;
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
				needs_change = true; // @TODO: Should be false if the state hasn't changed
			}

			m_temp_cv.notify_one();
		}
	});

	bool first_step_done = false;

	while (true) {
		{
			std::unique_lock<std::mutex> lock(temp_mtx);

			m_temp_cv.wait(lock, [&] {
				return needs_change || first_step_done || m_force_temp_change || m_quit;
			});

			if (m_quit)
				break;

			if (m_force_temp_change) {
				updateInterval();
				m_force_temp_change = false;
				first_step_done = false;
			}

			needs_change = false;
		}

		if (!cfg["temp_auto"].get<bool>())
			continue;

		// Temperature target in Kelvin
		int target_temp = cfg["temp_auto_low"];

		// Seconds it takes to reach it
		double duration_s  = 60;

		const double adapt_time_s = cfg["temp_auto_speed"].get<double>() * 60;

		std::time_t cur_datetime = std::time(nullptr);

		if (cur_datetime < end_datetime && end_datetime < start_datetime) {
			LOGV << "Start was yesterday";
			std::tm *x = std::localtime(&end_datetime);
			x->tm_wday--;
			start_datetime = std::mktime(x);
		}

		LOGV << "cur: " << std::asctime(std::localtime(&cur_datetime));
		LOGV << "start: " << std::asctime(std::localtime(&start_datetime)) << ", adaptation m: " << adapt_time_s / 60;
		LOGV << "end: " << std::asctime(std::localtime(&end_datetime));

		if (cur_datetime >= start_datetime && cur_datetime < end_datetime) {

			int secs_from_start = cur_datetime - start_datetime;

			LOGV << "mins_from_start: " << secs_from_start / 60 << " adapt_time_s: " << adapt_time_s;

			if (secs_from_start > adapt_time_s)
				secs_from_start = adapt_time_s;

			if (!first_step_done) {
				target_temp = remap(
				    secs_from_start,
				    0, adapt_time_s,
				    cfg["temp_auto_high"], cfg["temp_auto_low"]
				);
			} else {
				duration_s = adapt_time_s - secs_from_start;
				if (duration_s < 2)
					duration_s = 2;
			}
		} else {
			target_temp = cfg["temp_auto_high"];
		}

		LOGV << "Temp duration: " << duration_s / 60 << " min";

		int cur_step    = cfg["temp_auto_step"];
		int target_step = int(remap(target_temp, temp_k_max, temp_k_min, temp_steps_max, 0));

		if (cur_step == target_step) {
			LOGV << "Temp step already at target (" << target_step << ")";
			first_step_done = false;
			continue;
		}

		const int FPS      = cfg["temp_auto_fps"];
		const int diff     = target_step - cur_step;
		const double slice = 1. / FPS;

		double time = 0;

		int prev_step = 0;

		LOGV << "Adjusting temperature";

		while (cfg["temp_auto_step"].get<int>() != target_step) {

			if (m_force_temp_change || !cfg["temp_auto"].get<bool>() || m_quit)
				break;

			time += slice;

			const int step = int(easeInOutQuad(time, cur_step, diff, duration_s));

			cfg["temp_auto_step"] = step;

			//LOGV << "step: " << step << " / " << target_step;

			for (size_t i = 0; i < m_monitors.size(); ++i) {
				if (cfg["screens"][i]["temp_auto"].get<bool>())
					cfg["screens"][i]["temp_step"] = step;
			}

			if (step != prev_step)
				m_server->setGamma();

			prev_step = step;

			sleep_for(milliseconds(1000 / FPS));
		}

		first_step_done = true;
	}

	clock_cv.notify_one();
	clock.join();
}
