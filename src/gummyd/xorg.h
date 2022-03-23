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

#ifndef XCB_H
#define XCB_H

#include <xcb/xcb.h>
#include <xcb/randr.h>
//#include <xcb/shm.h>
//#include <xcb/xcb_image.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>

#include <vector>

struct XLib
{
    XLib();
	~XLib();
	Display *dsp;
};

struct XCB
{
    XCB();
	~XCB();
	xcb_connection_t *conn;
	xcb_screen_t *screen;
	int pref_screen;
};

struct Output
{
    std::vector<uint16_t> ramps;
	xcb_randr_get_crtc_info_reply_t *info;
	xcb_randr_crtc_t crtc;
	XShmSegmentInfo shminfo;
	XImage *image;
	uint64_t image_len;
	int ramp_sz;
};

class Xorg
{
public:
    Xorg();
	int    get_screen_brightness(int scr_idx);
	void   set_gamma(int scr_idx, int brt, int temp);
	size_t scr_count() const;
private:
	void apply_gamma_ramp(Output &, int brt_step, int temp_step);
	std::vector<Output> outputs;
	XLib xlib;
	XCB  xcb;
};

#endif // XCB_H
