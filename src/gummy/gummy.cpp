#include <iostream>
#include <regex>
#include <unistd.h>
#include <fcntl.h>
#include <CLI11.hpp>
#include <json.hpp>
#include "../common/defs.h"
#include "../common/utils.h"

using std::cout;

int main(int argc, char **argv)
{
	CLI::App app{"Screen manager for X11."};

	bool running = alreadyRunning();

	app.add_subcommand("start", "Start the background process.")->callback([&] {
		if (running) {
			cout << "already started\n";
			std::exit(0);
		}

		cout << "starting gummy\n";
		pid_t pid = fork();

		if (pid == 0) {

			execl(CMAKE_INSTALL_DAEMON_PATH, "", nullptr);

			execl("./gummyd", "", nullptr);

			cout << "failed to start gummyd\n";
			std::exit(1);
		}

		std::exit(0);
	});

	app.add_subcommand("stop", "Stop the background process.")->callback([&] {

		if (!running) {
			cout << "already stopped\n";
			std::exit(0);
		}

		int fd = open(fifo_name, O_CREAT|O_WRONLY, 0600);

		std::string s("stop");
		write(fd, s.c_str(), s.size());
		close(fd);
		cout << "gummy stopped\n";
		std::exit(0);
	});

	app.add_subcommand("status", "Show app / screen status.")->callback([&] {
		cout << (running ? "running" : "not running") << "\n";
		std::exit(0);
	});

	int scr_no = -1;
	app.add_option("-s,--screen", scr_no, "Screen on which to act. If omitted, any changes will be applied on all screens.")->check(CLI::Range(0, 99));

	int bm = -1;
	app.add_option("-B,--brightness-mode", bm, "Brightness mode. 0 for manual, 1 for automatic.")->check(CLI::Range(0, 1));

	int brt_auto_min = -1;
	app.add_option("-N,--brightness-auto-min", brt_auto_min, "Set minimum automatic brightness.")->check(CLI::Range(5, 100));

	int brt_auto_max = -1;
	app.add_option("-M,--brightness-auto-max", brt_auto_max, "Set maximum automatic brightness.")->check(CLI::Range(5, 100));

	int brt_auto_offset = -1;
	app.add_option("-L,--brightness-auto-offset", brt_auto_offset, "Set automatic brightness offset. Higher = brighter image.")->check(CLI::Range(-100, 100));

	int tm = -1;
	app.add_option("-T,--temperature-mode", tm,
	               "Temperature mode. 0 for manual, 1 for automatic.")
	->check(CLI::Range(0, 1));

	int brt = -1;
	app.add_option("-b,--brightness", brt, "Set screen brightness percentage.")->check(CLI::Range(5, 100));

	int temp = -1;
	app.add_option("-t,--temperature", temp,
	               "Set screen temperature in kelvins.\nSetting this option will disable automatic temperature if enabled."
	)->check(CLI::Range(temp_k_max, temp_k_min));

	int temp_day = -1;
	app.add_option("-j,--temp-day", temp_day,
	               "Set day time temperature in kelvins.")
	->check(CLI::Range(temp_k_max, temp_k_min));

	int temp_night = -1;
	app.add_option("-k,--temp-night", temp_night,
	               "Set night time temperature in kelvins.")
	->check(CLI::Range(temp_k_max, temp_k_min));


	auto time_format_callback = [] (const std::string &s) {

		std::regex pattern("^\\d{2}:\\d{2}$");
		std::string err("option should match the 24h format.");

		if (!std::regex_search(s, pattern))
			return err;
		if (std::stoi(s.substr(0, 2)) > 23)
			return err;
		if (std::stoi(s.substr(3, 2)) > 59)
			return err;

		return std::string("");
	};

	std::string sunrise_time;
	app.add_option("-y,--sunrise-time", sunrise_time,
	               "Set sunrise time in 24h format, for example `06:00`.")
	->check(time_format_callback);

	std::string sunset_time;
	app.add_option("-u,--sunset-time", sunset_time,
	               "Set sunset time in 24h format, for example `16:30`.")
	->check(time_format_callback);

	int adapt_time = -1;
	app.add_option("-i,--temp-adaptation-time", adapt_time,
	               "Temperature adaptation time in minutes.\nFor example, if this option is set to 30 min. and the sunset time is at 16:30,\ntemperature starts adjusting at 16:00, going down gradually until 16:30.");

	CLI11_PARSE(app, argc, argv);

	if (!running) {
		cout << "gummy is not running.\nType: `gummy start`\n";
		std::exit(1);
	}

	nlohmann::json msg {
		{"scr_no", scr_no},
		{"brt_mode", bm},
		{"brt_auto_min", brt_auto_min},
		{"brt_auto_max", brt_auto_max},
		{"brt_auto_offset", brt_auto_offset},
		{"temp_mode", tm},
		{"brt_perc", brt},
		{"temp_k", temp},
		{"temp_day_k", temp_day},
		{"temp_night_k", temp_night},
		{"sunrise_time", sunrise_time},
		{"sunset_time", sunset_time},
		{"temp_adaptation_time", adapt_time}
	};

	std::string s(msg.dump());

	int fd = open(fifo_name, O_CREAT | O_WRONLY, 0600);
	write(fd, s.c_str(), s.size());
	close(fd);
	return 0;
}
