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
int calc_brightness(const uint8_t *buf,
                    const uint64_t buf_sz,
                    const int bytes_per_pixel = 4,
                    const int stride = 1024);

double lerp(const double x, const double a, const double b);
double normalize(const double x, const double a, const double b);
double remap(const double x, const double a, const double b, const double ay, const double by);
double step_to_kelvin(const int step, const size_t color_ch);
double ease_out_expo(const double t, const double b , const double c, const double d);
double ease_in_out_quad(double t, const double b, const double c, const double d);

#endif // UTILS_H
