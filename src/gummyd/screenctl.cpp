#include "screenctl.h"
#include "cfg.h"
#include "../commons/utils.h"
#include <mutex>

ScreenCtl::ScreenCtl(Xorg *server)
    : m_server(server),
      m_devices(Sysfs::getDevices())
{
	//m_server->setGamma();
	//m_threads.emplace_back([this] { reapplyGamma(); });

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
	gamma_refresh_cv.notify_one();
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
			gamma_refresh_cv.wait_until(lock, system_clock::now() + 5s, [&] {
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

