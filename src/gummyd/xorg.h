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
	Screen  *screen;
	Window  root;
	int scr_no;
};

struct XCB
{
	XCB();
	~XCB();
	xcb_connection_t *conn;
	xcb_screen_t *screen;
	int pref_screen;
	int randr_crtc_count;
	xcb_randr_crtc_t *randr_crtcs;
};

struct Output
{
	Output(const XLib &xlib,
	       const xcb_randr_crtc_t c,
	       xcb_randr_get_crtc_info_reply_t *i);
	~Output();

	int getImageBrightness(const XLib &xlib);

	xcb_randr_crtc_t crtc;
	xcb_randr_get_crtc_info_reply_t *info;
	std::vector<uint16_t> ramps;

	XShmSegmentInfo shminfo;
	XImage          *image;
	uint64_t        image_len;

	int ramp_sz;
};

class Xorg
{
public:
	Xorg();
	int  screenCount();
	void setGamma(const int scr_idx, const int brt, const int temp);
	void setGamma();
	int  getScreenBrightness(const int scr_idx);
private:
	void initOutputs();
	void applyGammaRamp(Output &, const int brt_step, const int temp_step);

	XLib _xlib;
	XCB  _xcb;
	std::vector<Output> _outputs;
};

#endif // XCB_H
