#ifndef XCB_H
#define XCB_H

#include <xcb/xcb.h>
#include <vector>

class XCB
{
public:
	XCB();
	~XCB();

private:
	xcb_connection_t *conn;
	xcb_screen_t *scr;
	xcb_window_t root;
	int scr_num;
	int crtc_num;
	int ramp_sz;
	std::vector<uint16_t> init_ramp;
	bool initial_ramp_exists = false;

	xcb_screen_t* screenOfDisplay(int screen);
};

#endif // XCB_H
