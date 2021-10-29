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
	int getScreenBrightness(const int scr_idx);
private:
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
