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

#ifndef DEFS_H
#define DEFS_H

#include <array>

constexpr const char* config_name = "gummyconf";
constexpr const char* fifo_name   = "/tmp/gummy.fifo";
constexpr const char* lock_name   = "/tmp/gummy.lock";

constexpr int brt_steps_max  = 500;
constexpr int temp_steps_max = 500;
constexpr int temp_k_min     = 6500;
constexpr int temp_k_max     = 2000;

/* Color ramp by Ingo Thies. From Redshift:
 * https://github.com/jonls/redshift/blob/master/README-colorramp */
constexpr std::array<double, 46 * 3> ingo_thies_table {
    1.00000000,  0.54360078,  0.08679949, // 2000K
    1.00000000,  0.56618736,  0.14065513,
    1.00000000,  0.58734976,  0.18362641,
    1.00000000,  0.60724493,  0.22137978,
    1.00000000,  0.62600248,  0.25591950,
    1.00000000,  0.64373109,  0.28819679,
    1.00000000,  0.66052319,  0.31873863,
    1.00000000,  0.67645822,  0.34786758,
    1.00000000,  0.69160518,  0.37579588,
    1.00000000,  0.70602449,  0.40267128,
    1.00000000,  0.71976951,  0.42860152,
    1.00000000,  0.73288760,  0.45366838,
    1.00000000,  0.74542112,  0.47793608,
    1.00000000,  0.75740814,  0.50145662,
    1.00000000,  0.76888303,  0.52427322,
    1.00000000,  0.77987699,  0.54642268,
    1.00000000,  0.79041843,  0.56793692,
    1.00000000,  0.80053332,  0.58884417,
    1.00000000,  0.81024551,  0.60916971,
    1.00000000,  0.81957693,  0.62893653,
    1.00000000,  0.82854786,  0.64816570,
    1.00000000,  0.83717703,  0.66687674,
    1.00000000,  0.84548188,  0.68508786,
    1.00000000,  0.85347859,  0.70281616,
    1.00000000,  0.86118227,  0.72007777,
    1.00000000,  0.86860704,  0.73688797,
    1.00000000,  0.87576611,  0.75326132,
    1.00000000,  0.88267187,  0.76921169,
    1.00000000,  0.88933596,  0.78475236,
    1.00000000,  0.89576933,  0.79989606,
    1.00000000,  0.90198230,  0.81465502,
    1.00000000,  0.90963069,  0.82838210,
    1.00000000,  0.91710889,  0.84190889,
    1.00000000,  0.92441842,  0.85523742,
    1.00000000,  0.93156127,  0.86836903,
    1.00000000,  0.93853986,  0.88130458,
    1.00000000,  0.94535695,  0.89404470,
    1.00000000,  0.95201559,  0.90658983,
    1.00000000,  0.95851906,  0.91894041,
    1.00000000,  0.96487079,  0.93109690,
    1.00000000,  0.97107439,  0.94305985,
    1.00000000,  0.97713351,  0.95482993,
    1.00000000,  0.98305189,  0.96640795,
    1.00000000,  0.98883326,  0.97779486,
    1.00000000,  0.99448139,  0.98899179,
    1.00000000,  1.00000000,  1.00000000 // 6500K - 137
};

#endif // DEFS_H
