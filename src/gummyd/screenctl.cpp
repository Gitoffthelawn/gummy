#include "screenctl.h"
#include "cfg.h"
#include "../commons/utils.h"
#include <mutex>

ScreenCtl::ScreenCtl(Xorg *server) : m_server(server)
{
	m_server->setGamma();
	m_threads.emplace_back(std::thread([this] { reapplyGamma(); }));

	m_brt_controllers.reserve(m_server->screenCount());
	for (int i = 0; i < m_server->screenCount(); ++i) {
		m_brt_controllers.emplace_back(new BrtCtl(m_server, i));
	}
}

ScreenCtl::~ScreenCtl()
{
	stop();
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

void ScreenCtl::stop()
{
	m_quit = true;
	for (auto &x : m_brt_controllers)
		x->~BrtCtl();
	m_brt_controllers.clear();
	gamma_refresh_cv.notify_one();
	for (auto &t : m_threads)
		t.join();
}

BrtCtl::BrtCtl(Xorg* server, int scr_idx)
    : m_server(server),
      m_scr_idx(scr_idx),
      m_ss_thr(std::make_unique<std::thread>(std::thread([this] { captureScreen(); })))
{

}

BrtCtl::~BrtCtl()
{
    m_quit = true;
    m_ss_cv.notify_one();
    m_ss_thr->join();
}

void BrtCtl::captureScreen()
{
	convar      brt_cv;
	std::thread brt_thr([&] { adjustBrightness(brt_cv); });
	std::mutex  m;

	int img_delta = 0;
	bool force    = false;

	int
	prev_img_br = 0,
	prev_min    = 0,
	prev_max    = 0,
	prev_offset = 0;

	while (true) {
		{
			std::unique_lock<std::mutex> lock(m);

			m_ss_cv.wait(lock, [&] {
				return cfg["screens"].at(m_scr_idx)["brt_auto"].get<bool>() || m_quit;
			});
		}

		if (m_quit)
			break;

		if (cfg["screens"].at(m_scr_idx)["brt_auto"].get<bool>())
			force = true;
		else
			continue;

		while (cfg["screens"].at(m_scr_idx)["brt_auto"].get<bool>() && !m_quit) {

			const int img_br = m_server->getScreenBrightness(m_scr_idx);
			img_delta += abs(prev_img_br - img_br);

			if (img_delta > cfg["screens"].at(m_scr_idx)["brt_auto_threshold"].get<int>() || force) {

				img_delta = 0;
				force = false;

				{
					std::lock_guard lock(m_brt_mtx);
					m_ss_brt = img_br;
					m_brt_needs_change = true;
				}

				brt_cv.notify_one();
			}

			if (cfg["screens"].at(m_scr_idx)["brt_auto_min"] != prev_min
			        || cfg["screens"].at(m_scr_idx)["brt_auto_max"] != prev_max
			        || cfg["screens"].at(m_scr_idx)["brt_auto_offset"] != prev_offset)
				force = true;

			prev_img_br = img_br;
			prev_min    = cfg["screens"].at(m_scr_idx)["brt_auto_min"];
			prev_max    = cfg["screens"].at(m_scr_idx)["brt_auto_max"];
			prev_offset = cfg["screens"].at(m_scr_idx)["brt_auto_offset"];

			std::this_thread::sleep_for(std::chrono::milliseconds(cfg["screens"].at(m_scr_idx)["brt_auto_polling_rate"].get<int>()));
		}
	}

	{
		std::lock_guard<std::mutex> lock (m_brt_mtx);
		m_brt_needs_change = true;
	}

	brt_cv.notify_one();
	brt_thr.join();
}

void BrtCtl::adjustBrightness(convar &brt_cv)
{
	using namespace std::this_thread;
	using namespace std::chrono;

	while (true) {
		int img_br;

		{
			std::unique_lock<std::mutex> lock(m_brt_mtx);

			brt_cv.wait(lock, [&] {
				return m_brt_needs_change;
			});

			if (m_quit)
				break;

			m_brt_needs_change = false;
			img_br = m_ss_brt;
		}

		const int cur_step = cfg["screens"].at(m_scr_idx)["brt_step"];
		const int tmp = brt_steps_max
		                - int(remap(img_br, 0, 255, 0, brt_steps_max))
		                + int(remap(cfg["screens"].at(m_scr_idx)["brt_auto_offset"].get<int>(), 0, brt_steps_max, 0, cfg["screens"].at(m_scr_idx)["brt_auto_max"].get<int>()));
		const int target_step = std::clamp(tmp, cfg["screens"].at(m_scr_idx)["brt_auto_min"].get<int>(), cfg["screens"].at(m_scr_idx)["brt_auto_max"].get<int>());

		if (cur_step == target_step) {
			LOGV << "Brt already at target (" << target_step << ')';
			continue;
		}

		double time             = 0;
		const int FPS           = cfg["brt_auto_fps"];
		const double slice      = 1. / FPS;
		const double duration_s = cfg["brt_auto_speed"].get<double>() / 1000;
		const int diff          = target_step - cur_step;

		while (cfg["screens"].at(m_scr_idx)["brt_step"].get<int>() != target_step) {

			if (m_brt_needs_change || !cfg["screens"].at(m_scr_idx)["brt_auto"].get<bool>() || m_quit)
				break;

			time += slice;
			cfg["screens"].at(m_scr_idx)["brt_step"] = int(std::round(easeOutExpo(time, cur_step, diff, duration_s)));
			m_server->setGamma(m_scr_idx, cfg["screens"].at(m_scr_idx)["brt_step"], cfg["screens"].at(m_scr_idx)["temp_step"]);
			sleep_for(milliseconds(1000 / FPS));
		}
	}
}

