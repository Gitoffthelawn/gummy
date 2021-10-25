#ifndef XCB_H
#define XCB_H

#include <xcb/xcb.h>
#include <xcb/randr.h>
#include <vector>

struct Output;

class XCB
{
public:
	XCB();
	~XCB();
	int screenCount();
	void setGamma(const int scr_idx, const int brt, const int temp);
private:
	xcb_connection_t *conn;
	xcb_screen_t *screen;
	int pref_screen;
	int crtc_count;
	std::vector<Output> outputs;
};

struct Output {
	xcb_randr_crtc_t crtc;
	xcb_randr_get_crtc_info_reply_t info;
	int ramp_sz;
	std::vector<uint16_t> ramps;
};

#endif // XCB_H
