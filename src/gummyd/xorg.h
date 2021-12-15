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

struct Output;

class Xorg
{
public:
	Xorg();
	~Xorg();
	int screenCount();
	void setGamma(const int scr_idx, const int brt, const int temp);
	void setGamma();
	int getScreenBrightness(const int scr_idx);
private:
	void applyGammaRamp(Output &, const int brt_step, const int temp_step);
	xcb_connection_t *m_conn;
	xcb_screen_t *m_screen;
	int m_pref_screen;
	int m_crtc_count;
	std::vector<Output> m_outputs;

	Display *m_dsp;
	Screen  *m_xlib_screen;
	Window  m_xlib_root;
};

struct Output {
	xcb_randr_crtc_t crtc;
	xcb_randr_get_crtc_info_reply_t *info;
	int ramp_sz;
	std::vector<uint16_t> ramps;

	XImage *image;
	uint64_t image_len;
	XShmSegmentInfo shminfo;
};

#endif // XCB_H
