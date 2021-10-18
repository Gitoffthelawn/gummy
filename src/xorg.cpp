#include "xorg.h"
#include <xcb/randr.h>
#include "defs.h"

XCB::XCB()
{
	conn = xcb_connect(nullptr, nullptr);
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
