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

#ifndef UTILS_H
#define UTILS_H

#include <cstddef>
#include <cstdint>

int set_lock();
int calc_brightness(uint8_t *buf,
                    uint64_t buf_sz,
                    int bytes_per_pixel = 4,
                    int stride = 1024);

double lerp(double x, double a, double b);
double normalize(double x, double a, double b);
double remap(double x, double a, double b, double ay, double by);
double step_to_kelvin(int step, size_t color_ch);

struct Animation
{
	double elapsed;
	double slice;
	double duration_s;
	int fps;
	int start_step;
	int diff;
};
double ease_out_expo(double t, double b , double c, double d);
double ease_in_out_quad(double t, double b, double c, double d);

#endif // UTILS_H
