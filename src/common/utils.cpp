/**
 * Copyright (C) Francesco Fusco. All rights reserved.
 * License: https://github.com/Fushko/gammy#license
 */

#include "utils.h"
#include "defs.h"
#include <cmath>

int calcBrightness(uint8_t *buf, uint64_t buf_sz, int bytes_per_pixel, int stride)
{
	uint64_t rgb[3] {};
	for (uint64_t i = 0, inc = stride * bytes_per_pixel; i < buf_sz; i += inc) {
		rgb[0] += buf[i + 2];
		rgb[1] += buf[i + 1];
		rgb[2] += buf[i];
	}

	return (rgb[0] * 0.2126 + rgb[1] * 0.7152 + rgb[2] * 0.0722) * stride / (buf_sz / bytes_per_pixel);
}

double lerp(double x, double a, double b)
{
	return (1 - x) * a + x * b;
}

double normalize(double x, double a, double b)
{
	return (x - a) / (b - a);
}

double remap(double x, double a, double b, double ay, double by)
{
	return lerp(normalize(x, a, b), ay, by);
}

double stepToKelvin(int temp_step, size_t color_ch)
{
	return remap(temp_step, 0, temp_steps_max, 1, ingo_thies_table[color_ch]);
};

double easeOutExpo(double t, double b , double c, double d)
{
	return (t == d) ? b + c : c * (-pow(2, -10 * t / d) + 1) + b;
}

double easeInOutQuad(double t, double b, double c, double d)
{
	if ((t /= d / 2) < 1)
		return c / 2 * t * t + b;
	else
		return -c / 2 * ((t - 1) * (t - 3) - 1) + b;
}

bool alreadyRunning()
{
	static int fd;
	struct flock fl;
	fl.l_type   = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start  = 0;
	fl.l_len    = 1;
	fd = open(lock_name, O_WRONLY | O_CREAT, 0666);
	return fd == -1 || fcntl(fd, F_SETLK, &fl) == -1;
}
