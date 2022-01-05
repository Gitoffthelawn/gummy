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

#include "xorg.h"
#include "cfg.h"
#include "../common/defs.h"
#include "../common/utils.h"

#include <sys/ipc.h>
#include <sys/shm.h>
#include <syslog.h>

Xorg::XLib::XLib()
{
	if (XInitThreads() == 0) {
		syslog(LOG_ERR, "XInitThreads failed");
		std::exit(1);
	}
	if (!(dsp = XOpenDisplay(nullptr))) {
		syslog(LOG_ERR, "XOpenDisplay failed");
		std::exit(1);
	}
	root   = DefaultRootWindow(dsp);
	screen = DefaultScreenOfDisplay(dsp);
	scr_no = DefaultScreen(dsp);
};

Xorg::XLib::~XLib()
{
    XCloseDisplay(dsp);
}

Xorg::XCB::XCB()
    : conn(xcb_connect(nullptr, &pref_screen))
{
	int err = xcb_connection_has_error(conn);
	if (err > 0) {
		syslog(LOG_ERR, "xcb_connect error %d", err);
		std::exit(1);
	}

	xcb_screen_iterator_t iter = xcb_setup_roots_iterator(xcb_get_setup(conn));
	for (int i = 0; iter.rem > 0; --i) {
		if (i == pref_screen) {
			screen = iter.data;
			break;
		}
		xcb_screen_next(&iter);
	}

	auto scr_ck      = xcb_randr_get_screen_resources(conn, screen->root);
	auto *scr_rpl    = xcb_randr_get_screen_resources_reply(conn, scr_ck, 0);
	randr_crtc_count = scr_rpl->num_crtcs;
	randr_crtcs      = xcb_randr_get_screen_resources_crtcs(scr_rpl);
	free(scr_rpl);
}

Xorg::XCB::~XCB()
{
    xcb_disconnect(conn);
}

Xorg::Output::Output(const XLib &xlib,
               const xcb_randr_crtc_t c,
               xcb_randr_get_crtc_info_reply_t *i)
    : crtc(c),
      _info(i),
      _image_len(_info->width * _info->height * 4)
{
	_image = XShmCreateImage(
	   xlib.dsp,
	   XDefaultVisual(xlib.dsp, xlib.scr_no),
	   DefaultDepth(xlib.dsp, xlib.scr_no),
	   ZPixmap,
	   nullptr,
	   &_shminfo,
	   _info->width,
	   _info->height
	);

	_shminfo.shmid = shmget(IPC_PRIVATE, _image_len, IPC_CREAT | 0600);
	void *shm = shmat(_shminfo.shmid, nullptr, SHM_RDONLY);

	if (shm == reinterpret_cast<void*>(-1)) {
		syslog(LOG_ERR, "shmat failed");
		std::exit(1);
	}

	_shminfo.shmaddr  = _image->data = reinterpret_cast<char*>(shm);
	_shminfo.readOnly = False;
	XShmAttach(xlib.dsp, &_shminfo);
}

void Xorg::Output::set_ramp_size(const int sz)
{
	_ramp_sz = sz;
	_ramps.resize(3 * size_t(sz) * sizeof(uint16_t));
}

int Xorg::Output::get_image_brightness(const XLib &xlib) const
{
	XShmGetImage(xlib.dsp, xlib.root, _image, _info->x, _info->y, AllPlanes);

	return calcBrightness(
	    reinterpret_cast<uint8_t*>(_image->data),
	    _image_len
	);
}

Xorg::Output::~Output()
{
	free(_info);
}

Xorg::Xorg()
{
	std::vector<xcb_randr_get_crtc_info_cookie_t> ick;
	for (int i = 0; i < _xcb.randr_crtc_count; ++i) {
		ick.push_back(xcb_randr_get_crtc_info(_xcb.conn, _xcb.randr_crtcs[i], 0));
	}
	for (int i = 0; i < _xcb.randr_crtc_count; ++i) {
		auto info = xcb_randr_get_crtc_info_reply(_xcb.conn, ick[i], nullptr);
		if (info->num_outputs > 0)
			_outputs.emplace_back(_xlib, _xcb.randr_crtcs[i], info);
	}

	std::vector<xcb_randr_get_crtc_gamma_cookie_t> gck;
	for (size_t i = 0; i < _outputs.size(); ++i) {
		gck.push_back(xcb_randr_get_crtc_gamma(_xcb.conn, _outputs[i].crtc));
	}
	for (size_t i = 0; i < _outputs.size(); ++i) {
		auto gamma_rpl = xcb_randr_get_crtc_gamma_reply(_xcb.conn, gck[i], nullptr);
		_outputs[i].set_ramp_size(gamma_rpl->size);
		free(gamma_rpl);
	}
}

int Xorg::get_screen_brightness(const int scr_idx) const
{
	return _outputs[scr_idx].get_image_brightness(_xlib);
}

void Xorg::set_gamma(const int scr_idx, const int brt_step, const int temp_step)
{
	_outputs[scr_idx].apply_gamma_ramp(_xcb, brt_step, temp_step);
}

void Xorg::Output::apply_gamma_ramp(const XCB &xcb, const int brt_step, const int temp_step)
{
	/**
	 * The ramp multiplier equals 32 when ramp_sz = 2048, 64 when 1024, etc.
	 * Assuming ramp_sz = 2048 and pure state (default brightness/temp)
	 * the RGB channels look like:
	 * [ 0, 32, 64, 96, ... UINT16_MAX - 32 ]
	 */
	uint16_t *r = &_ramps[0 * _ramp_sz];
	uint16_t *g = &_ramps[1 * _ramp_sz];
	uint16_t *b = &_ramps[2 * _ramp_sz];

	const double r_mult = stepToKelvin(temp_step, 0),
	             g_mult = stepToKelvin(temp_step, 1),
	             b_mult = stepToKelvin(temp_step, 2);

	const int    ramp_mult = (UINT16_MAX + 1) / _ramp_sz;
	const double brt_mult  = normalize(brt_step, 0, brt_steps_max) * ramp_mult;

	for (int i = 0; i < _ramp_sz; ++i) {
		const int val = std::clamp(int(i * brt_mult), 0, UINT16_MAX);
		r[i] = uint16_t(val * r_mult);
		g[i] = uint16_t(val * g_mult);
		b[i] = uint16_t(val * b_mult);
	}

	auto c  = xcb_randr_set_crtc_gamma_checked(xcb.conn, crtc, _ramp_sz, r, g, b);
	auto *e = xcb_request_check(xcb.conn, c);
	if (e) {
		syslog(LOG_ERR, "randr set gamma error: %d", int(e->error_code));
	}
}

size_t Xorg::scr_count() const
{
	return _outputs.size();
}
