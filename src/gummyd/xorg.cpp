#include "xorg.h"

#include <sys/ipc.h>
#include <sys/shm.h>

#include "cfg.h"
#include "../common/defs.h"
#include "../common/utils.h"

#include <plog/Log.h>
#include <iostream>

Xorg::Xorg()
{
	XInitThreads();
	m_conn = xcb_connect(nullptr, &m_pref_screen);

	xcb_screen_iterator_t iter = xcb_setup_roots_iterator(xcb_get_setup(m_conn));

	for (int i = 0; iter.rem > 0; --i) {
		if (i == m_pref_screen) {
			m_screen = iter.data;
			break;
		}
		xcb_screen_next(&iter);
	}

	auto scr_ck   = xcb_randr_get_screen_resources(m_conn, m_screen->root);
	auto *scr_rpl = xcb_randr_get_screen_resources_reply(m_conn, scr_ck, 0);
	m_crtc_count  = scr_rpl->num_crtcs;

	xcb_randr_crtc_t *crtcs = xcb_randr_get_screen_resources_crtcs(scr_rpl);

	m_dsp            = XOpenDisplay(nullptr);
	m_xlib_root      = DefaultRootWindow(m_dsp);
	m_xlib_screen    = DefaultScreenOfDisplay(m_dsp);
	int scr          = XDefaultScreen(m_dsp);

	for (int i = 0; i < m_crtc_count; ++i) {

		Output o;
		o.crtc = crtcs[i];

		auto crtc_info_ck = xcb_randr_get_crtc_info(m_conn, o.crtc, 0);
		o.info            = xcb_randr_get_crtc_info_reply(m_conn, crtc_info_ck, nullptr);

		if (o.info->num_outputs == 0)
			continue;

		m_outputs.push_back(o);
	}

	free(scr_rpl);

	for (auto &o : m_outputs) {

		// Gamma
		auto gamma_ck  = xcb_randr_get_crtc_gamma(m_conn, o.crtc);
		auto gamma_rpl = xcb_randr_get_crtc_gamma_reply(m_conn, gamma_ck, nullptr);
		o.ramp_sz = gamma_rpl->size;
		o.ramps.resize(3 * size_t(o.ramp_sz) * sizeof(uint16_t));
		free(gamma_rpl);

		// Shm
		o.image = XShmCreateImage(
		   m_dsp,
		   XDefaultVisual(m_dsp, scr),
		   DefaultDepth(m_dsp, scr),
		   ZPixmap,
		   nullptr,
		   &o.shminfo,
		   o.info->width,
		   o.info->height
		);

		o.image_len     = o.info->width * o.info->height * 4;
		o.shminfo.shmid = shmget(IPC_PRIVATE, o.image_len, IPC_CREAT | 0600);
		void *shm       = shmat(o.shminfo.shmid, nullptr, SHM_RDONLY);
		if (shm == reinterpret_cast<void*>(-1)) {
			LOGF << "shmat failed";
			exit(1);
		}
		o.shminfo.shmaddr  = o.image->data = reinterpret_cast<char*>(shm);
		o.shminfo.readOnly = False;
		XShmAttach(m_dsp, &o.shminfo);
	}
}

int Xorg::getScreenBrightness(const int scr_idx)
{
	Output *o = &m_outputs[scr_idx];

	XShmGetImage(m_dsp, m_xlib_root, o->image, o->info->x, o->info->y, AllPlanes);

	return calcBrightness(
	    reinterpret_cast<uint8_t*>(o->image->data),
	    o->image_len,
	    4,
	    1024
	 );
}

void Xorg::setGamma(const int scr_idx, const int brt_step, const int temp_step)
{
	//LOGV <<  "setGamma() scr " << scr_idx << ": "  << brt_step << " - " << temp_step;
	applyGammaRamp(m_outputs[scr_idx], brt_step, temp_step);
}

void Xorg::setGamma()
{
	for (size_t i = 0; i < m_outputs.size(); ++i)
		applyGammaRamp(
		    m_outputs[i],
		    cfg["screens"][i]["brt_step"],
		    cfg["screens"][i]["temp_step"]
		);
}

void Xorg::applyGammaRamp(Output &o, int brt_step, int temp_step)
{
	/**
	 * The ramp multiplier equals 32 when ramp_sz = 2048, 64 when 1024, etc.
	 * Assuming ramp_sz = 2048 and pure state (default brightness/temp)
	 * the RGB channels look like:
	 * [ 0, 32, 64, 96, ... UINT16_MAX - 32 ]
	 */
	uint16_t *r = &o.ramps[0 * o.ramp_sz];
	uint16_t *g = &o.ramps[1 * o.ramp_sz];
	uint16_t *b = &o.ramps[2 * o.ramp_sz];

	const double r_mult = stepToKelvin(temp_step, 0),
	             g_mult = stepToKelvin(temp_step, 1),
	             b_mult = stepToKelvin(temp_step, 2);

	const int    ramp_mult = (UINT16_MAX + 1) / o.ramp_sz;
	const double brt_mult  = normalize(brt_step, 0, brt_steps_max) * ramp_mult;

	for (int i = 0; i < o.ramp_sz; ++i) {
		const int val = std::clamp(int(i * brt_mult), 0, UINT16_MAX);
		r[i] = uint16_t(val * r_mult);
		g[i] = uint16_t(val * g_mult);
		b[i] = uint16_t(val * b_mult);
	}

	auto c = xcb_randr_set_crtc_gamma_checked(m_conn, o.crtc, o.ramp_sz, r, g, b);
	xcb_generic_error_t *e = xcb_request_check(m_conn, c);
	if (e) {
		LOGE << "randr set gamma error: " << int(e->error_code);
	}
}

int Xorg::screenCount()
{
	return m_outputs.size();
}

Xorg::~Xorg()
{
	xcb_disconnect(m_conn);
	XCloseDisplay(m_dsp);
}
