#ifndef XCB_H
#define XCB_H

#include <xcb/xcb.h>
#include <xcb/randr.h>
#include <xcb/shm.h>
#include <xcb/xcb_image.h>
#include <vector>

struct Output;

class XCB
{
public:
	XCB();
	~XCB();
	int screenCount();
	void setGamma(const int scr_idx, const int brt, const int temp);
	int getScreenBrightness(const int scr_idx);
private:
	xcb_connection_t *conn;
	xcb_screen_t *screen;
	int pref_screen;
	int crtc_count;
	std::vector<Output> outputs;
};

struct Output {
	xcb_randr_crtc_t crtc;
	xcb_randr_get_crtc_info_reply_t *info;
	int ramp_sz;
	std::vector<uint16_t> ramps;
	xcb_pixmap_t pixmap_id;

	xcb_shm_segment_info_t shminfo;
};

#endif // XCB_H
