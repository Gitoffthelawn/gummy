#include "xorg.h"

#include <sys/ipc.h>
#include <sys/shm.h>

#include <xcb/shm.h>
#include <xcb/xcb_image.h>

#include "../commons/defs.h"
#include "../commons/utils.h"

#include <iostream>

XCB::XCB()
{
	conn = xcb_connect(nullptr, &pref_screen);

	xcb_screen_iterator_t iter = xcb_setup_roots_iterator(xcb_get_setup(conn));

	for (int i = 0; iter.rem > 0; --i) {
		if (i == pref_screen) {
			screen = iter.data;
			break;
		}
		xcb_screen_next(&iter);
	}

	auto scr_ck   = xcb_randr_get_screen_resources(conn, screen->root);
	auto *scr_rpl = xcb_randr_get_screen_resources_reply(conn, scr_ck, 0);

	crtc_count = scr_rpl->num_crtcs;

	xcb_randr_crtc_t *crtcs = xcb_randr_get_screen_resources_crtcs(scr_rpl);

	for (int i = 0; i < crtc_count; ++i) {

		Output o;
		o.crtc = crtcs[i];

		auto crtc_info_ck = xcb_randr_get_crtc_info(conn, o.crtc, 0);
		o.info            = xcb_randr_get_crtc_info_reply(conn, crtc_info_ck, nullptr);

		if (o.info->num_outputs == 0)
			continue;

		xcb_generic_error_t *err;

		o.shminfo.shmseg = xcb_generate_id(conn);
		auto csck = xcb_shm_create_segment(conn,
		                                   o.shminfo.shmseg,
		                                   o.info->width * o.info->height * 4,
		                                   0);
		xcb_shm_create_segment_reply(conn, csck, &err);

		if (err) {
			LOGF << "xcb_shm_create_segment err: " << err->error_code << '\n';
			exit(1);
		}

		o.pixmap_id = xcb_generate_id(conn);

		xcb_shm_create_pixmap_checked(conn,
		                              o.pixmap_id,
		                              screen->root,
		                              o.info->width,
		                              o.info->height,
		                              screen->root_depth,
		                              o.shminfo.shmseg,
		                              0);

		o.shminfo.shmid = shmget(IPC_PRIVATE, o.info->width * o.info->height * 4, IPC_CREAT | 0600);

		void *shm = shmat(o.shminfo.shmid, nullptr, SHM_RDONLY);

		if (shm == reinterpret_cast<void*>(-1)) {
			LOGF << "shmat failed";
			exit(1);
		}

		o.shminfo.shmaddr = reinterpret_cast<uint8_t*>(shm);

		auto void_ck = xcb_shm_attach_checked(conn, o.shminfo.shmseg, o.shminfo.shmid, 0);
		err = xcb_request_check(conn, void_ck);

		if (err) {
			LOGF << "xcb_shm_attach_checked err: " << int(err->error_code) << '\n';
			exit(1);
		}

		outputs.push_back(o);
	}

	free(scr_rpl);

	for (auto &o : outputs) {
		auto gamma_ck  = xcb_randr_get_crtc_gamma(conn, o.crtc);
		auto gamma_rpl = xcb_randr_get_crtc_gamma_reply(conn, gamma_ck, nullptr);
		o.ramp_sz = gamma_rpl->size;
		o.ramps.resize(3 * size_t(o.ramp_sz) * sizeof(uint16_t));
		free(gamma_rpl);
	}
}

int XCB::getScreenBrightness(const int scr_idx)
{
    Output *o = &outputs[scr_idx];
/*
    xcb_image_shm_get(
        conn,
        screen->root,
        nullptr,
        o->shminfo,
        o->info.x,
        o->info.y,
        XCB_GC_FOREGROUND
    );*/

    xcb_generic_error_t *err;
    auto ck = xcb_shm_get_image(conn, screen->root,
                      o->info->x,
                      o->info->y,
                      o->info->width,
                      o->info->height,
                      0,
                      XCB_IMAGE_FORMAT_Z_PIXMAP,
                      o->shminfo.shmseg,
                      0);

    auto reply = xcb_shm_get_image_reply(conn, ck, &err);

    if (err) {
        LOGF << "xcb_shm_get_image err: " << err->error_code;
        exit(1);
    }

    return calcBrightness(
        o->shminfo.shmaddr,
        o->info->width * o->info->height * 4,
        4,
        1024
     );
}

void XCB::setGamma(const int scr_idx, const int brt_step, const int temp_step)
{
	Output *o = &outputs[scr_idx];

	/**
	 * The ramp multiplier equals 32 when ramp_sz = 2048, 64 when 1024, etc.
	 * Assuming ramp_sz = 2048 and pure state (default brightness/temp)
	 * the RGB channels look like:
	 * [ 0, 32, 64, 96, ... UINT16_MAX - 32 ]
	 */
	uint16_t *r = &o->ramps[0 * o->ramp_sz];
	uint16_t *g = &o->ramps[1 * o->ramp_sz];
	uint16_t *b = &o->ramps[2 * o->ramp_sz];

	const double r_mult = interpTemp(temp_step, 0),
	             g_mult = interpTemp(temp_step, 1),
	             b_mult = interpTemp(temp_step, 2);

	const int    ramp_mult = (UINT16_MAX + 1) / o->ramp_sz;
	const double brt_mult  = normalize(brt_step, 0, brt_steps_max) * ramp_mult;

	for (int i = 0; i < o->ramp_sz; ++i) {
		const int val = std::clamp(int(i * brt_mult), 0, UINT16_MAX);
		r[i] = uint16_t(val * r_mult);
		g[i] = uint16_t(val * g_mult);
		b[i] = uint16_t(val * b_mult);
	}

	auto c = xcb_randr_set_crtc_gamma_checked(conn, o->crtc, o->ramp_sz, r, g, b);

	xcb_generic_error_t *error = xcb_request_check(conn, c);

	if (error) {
		LOGE << "randr set gamma error: " << error->error_code;
	}
}

int XCB::screenCount()
{
	return outputs.size();
}

XCB::~XCB()
{
	xcb_disconnect(conn);
}
