#include "xorg.h"
#include <xcb/randr.h>
#include "defs.h"
#include "utils.h"

XCB::XCB()
{
	conn = xcb_connect(nullptr, nullptr);
	getDisplays();

	if (displays.empty()) {
		LOGF << "No screens found";
		exit(EXIT_FAILURE);
	}

	for (auto &dsp : displays) {
		auto scr_ck    = xcb_randr_get_screen_resources(conn, dsp.screen->root);
		auto *scr_rpl  = xcb_randr_get_screen_resources_reply(conn , scr_ck, 0);
		dsp.crtc_num   = *xcb_randr_get_screen_resources_crtcs(scr_rpl);
		auto gamma_ck  = xcb_randr_get_crtc_gamma(conn, dsp.crtc_num);
		auto gamma_rpl = xcb_randr_get_crtc_gamma_reply(conn, gamma_ck, nullptr);
		dsp.ramp_sz = gamma_rpl->size;
		dsp.ramps.resize(3 * size_t(dsp.ramp_sz) * sizeof(uint16_t));
	}

	/*init_ramp.resize(3 * size_t(ramp_sz) * sizeof(uint16_t));
	ramp.resize(3 * size_t(ramp_sz) * sizeof(uint16_t));

	uint16_t *d = init_ramp.data();
	uint16_t *r, *g, *b;

	r = xcb_randr_get_crtc_gamma_red(gamma_reply);
	g = xcb_randr_get_crtc_gamma_green(gamma_reply);
	b = xcb_randr_get_crtc_gamma_blue(gamma_reply);
	free(gamma_reply);

	if (!r || !g || !b) {
		LOGE << "Failed to get initial gamma ramp";
	} else {
		initial_ramp_exists = true;
		d[0 * ramp_sz] = *r;
		d[1 * ramp_sz] = *g;
		d[2 * ramp_sz] = *b;
	}*/
}

void XCB::setGamma(const int scr_idx, const int brt_step, const int temp_step)
{
	Display *dsp = &displays[scr_idx];

	/**
	 * The ramp multiplier equals 32 when ramp_sz = 2048, 64 when 1024, etc.
	 * Assuming ramp_sz = 2048 and pure state (default brightness/temp)
	 * the RGB channels look like:
	 * [ 0, 32, 64, 96, ... UINT16_MAX - 32 ]
	 */
	uint16_t *r = &dsp->ramps[0 * dsp->ramp_sz];
	uint16_t *g = &dsp->ramps[1 * dsp->ramp_sz];
	uint16_t *b = &dsp->ramps[2 * dsp->ramp_sz];

	const double r_mult = interpTemp(temp_step, 0),
	             g_mult = interpTemp(temp_step, 1),
	             b_mult = interpTemp(temp_step, 2);

	const int    ramp_mult = (UINT16_MAX + 1) / dsp->ramp_sz;
	const double brt_mult  = normalize(brt_step, 0, brt_steps_max) * ramp_mult;

	for (int i = 0; i < dsp->ramp_sz; ++i) {
		const int val = std::clamp(int(i * brt_mult), 0, UINT16_MAX);
		r[i] = uint16_t(val * r_mult);
		g[i] = uint16_t(val * g_mult);
		b[i] = uint16_t(val * b_mult);
	}

	auto c = xcb_randr_set_crtc_gamma_checked(conn, dsp->crtc_num, dsp->ramp_sz, r, g, b);

	xcb_generic_error_t *error = xcb_request_check(conn, c);
	if (error) {
		LOGE << "randr set gamma error: " << error->error_code;
	}
}

void XCB::getDisplays()
{
	if (!conn)
		return;

	xcb_screen_iterator_t iter = xcb_setup_roots_iterator(xcb_get_setup(conn));
	for (;iter.rem; xcb_screen_next(&iter)) {
		Display d;
		d.screen = iter.data;
		displays.push_back(d);
	}
}

int XCB::screensDetected()
{
	return displays.size();
}

XCB::~XCB()
{
	xcb_disconnect(conn);
}
