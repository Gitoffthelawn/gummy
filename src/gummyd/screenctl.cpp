#include "screenctl.h"
#include "cfg.h"
#include <mutex>

ScreenCtl::ScreenCtl(Xorg *server) : m_server(server)
{
	m_server->setGamma();
	m_threads.emplace_back(std::thread([this] { reapplyGamma(); }));
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
	gamma_refresh_cv.notify_one();
	for (auto &t : m_threads)
		t.join();
}
