#include "xorg.h"

XCB::XCB()
{
	conn = xcb_connect(nullptr, nullptr);
}

XCB::~XCB()
{
	xcb_disconnect(conn);
}
