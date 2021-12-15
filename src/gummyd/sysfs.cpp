#include "sysfs.h"
#include "../common/defs.h"

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <syslog.h>

std::vector<Device> Sysfs::getDevices()
{
	namespace fs = std::filesystem;

	std::vector<Device> devices;

	for (const auto &entry : fs::directory_iterator("/sys/class/backlight")) {

		const auto path         = entry.path();
		const auto path_str     = path.generic_string();
		const auto max_brt_file = path_str + "/max_brightness";

		std::ifstream stream(max_brt_file);
		if (!stream.is_open()) {
			syslog(LOG_ERR, "Unable to open %s", max_brt_file.c_str());
			exit(1);
		}

		std::string max_brt;
		stream.read(max_brt.data(), sizeof(max_brt));

		devices.emplace_back(
		    path.filename(),
		    path,
		    path_str + "/brightness",
		    std::stoi(max_brt)
		);
	}

	return devices;
}

Device::Device(std::string name, std::string path, std::string brt_file, int max_brt)
    : name(name),
      path(path),
      brt_file(brt_file),
      max_brt(max_brt)
{

}

Device::Device(Device&& d)
    : name(d.name),
      path(d.path),
      brt_file(d.brt_file),
      max_brt(d.max_brt)
{

}

void Device::setBacklight(int brt)
{
    brt = std::clamp(brt, 0, max_brt);
    std::string out(std::to_string(brt) + '\n');

    std::ofstream stream(brt_file);
    stream.write(out.c_str(), out.size());
}
