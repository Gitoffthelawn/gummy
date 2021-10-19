#include "xorg.h"
#include <xcb/randr.h>
#include "defs.h"
#include "utils.h"

XCB::XCB()
{
	conn = xcb_connect(nullptr, nullptr);
	scr_num = 0; // @TODO: remove
	scr  = screenOfDisplay(scr_num);

	if (!scr) {
		LOGF << "Could not determine screen";
		exit(EXIT_FAILURE);
	}

	root = scr->root;

	auto scr_res_cookie = xcb_randr_get_screen_resources(conn, root);
	auto *scr_res_reply = xcb_randr_get_screen_resources_reply(conn , scr_res_cookie, 0);

	if (!scr_res_reply) {
		LOGE << "Failed to get screen information";
		exit(1);
	}


	xcb_randr_crtc_t *firstCrtc = xcb_randr_get_screen_resources_crtcs(scr_res_reply);
	crtc_num = *firstCrtc;

	xcb_randr_get_crtc_gamma_reply_t* gamma_reply =
	                xcb_randr_get_crtc_gamma_reply(conn, xcb_randr_get_crtc_gamma(conn, crtc_num), nullptr);

	if (!gamma_reply) {
		LOGE << "Failed to get gamma information";
		exit(EXIT_FAILURE);
	}

	ramp_sz = gamma_reply->size;

	if (ramp_sz == 0) {
		LOGE << "Invalid gamma ramp size";
		free(gamma_reply);
		exit(EXIT_FAILURE);
	}

	init_ramp.resize(3 * size_t(ramp_sz) * sizeof(uint16_t));
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
	}
}

void XCB::fillRamp(const int brt_step, const int temp_step)
{
	/**
	 * The ramp multiplier equals 32 when ramp_sz = 2048, 64 when 1024, etc.
	 * Assuming ramp_sz = 2048 and pure state (default brightness/temp)
	 * the RGB channels look like:
	 * [ 0, 32, 64, 96, ... UINT16_MAX - 32 ]
	 */
	uint16_t *r = &ramp[0 * ramp_sz];
	uint16_t *g = &ramp[1 * ramp_sz];
	uint16_t *b = &ramp[2 * ramp_sz];

	const double r_mult = interpTemp(temp_step, 0),
	             g_mult = interpTemp(temp_step, 1),
	             b_mult = interpTemp(temp_step, 2);

	const int    ramp_mult = (UINT16_MAX + 1) / ramp_sz;
	const double brt_mult  = normalize(brt_step, 0, brt_steps_max) * ramp_mult;

	for (int i = 0; i < ramp_sz; ++i) {
		const int val = std::clamp(int(i * brt_mult), 0, UINT16_MAX);
		r[i] = uint16_t(val * r_mult);
		g[i] = uint16_t(val * g_mult);
		b[i] = uint16_t(val * b_mult);
	}
}

void XCB::setGamma(const int brt, const int temp)
{
	fillRamp(brt, temp);
	xcb_randr_set_crtc_gamma(conn, crtc_num, ramp_sz, &ramp[0*ramp_sz], &ramp[1*ramp_sz], &ramp[2*ramp_sz]);
}

xcb_screen_t* XCB::screenOfDisplay(int screen)
{
	if (!conn)
		return nullptr;

	xcb_screen_iterator_t iter = xcb_setup_roots_iterator(xcb_get_setup(conn));

	for (;iter.rem; --screen, xcb_screen_next(&iter)) {
		if (screen == 0)
			return iter.data;
	}

	return nullptr;
}

XCB::~XCB()
{
	xcb_disconnect(conn);
}
