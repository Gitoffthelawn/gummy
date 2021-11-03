#ifndef SYSFS_H
#define SYSFS_H

#include <vector>
#include <filesystem>

class Device;
class Sysfs
{
public:
	static std::vector<Device> getDevices();
};

class Device
{
public:
	Device(std::string, std::string, std::string, int);
	Device(Device&&);
	void setBacklight(int);
private:
	std::string name;
	std::string path;
	std::string brt_file;
	int max_brt;
};

#endif // SYSFS_H
