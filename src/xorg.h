#ifndef XCB_H
#define XCB_H

#include <xcb/xcb.h>

class XCB
{
public:
	XCB();
	~XCB();
private:
	xcb_connection_t *conn;
};

#endif // XCB_H
