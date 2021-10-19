#ifndef XCB_H
#define XCB_H

#include <xcb/xcb.h>
#include <vector>

struct Display;

class XCB
{
public:
	XCB();
	~XCB();
	int screensDetected();
	void setGamma(const int scr_idx, const int brt, const int temp);
private:
	xcb_connection_t *conn;
	std::vector<Display> displays;
	void getDisplays();
};

struct Display {
	xcb_screen_t *screen;
	int crtc_num;
	int ramp_sz;
	std::vector<uint16_t> ramps;
};

#endif // XCB_H
