#ifndef SCREENCTL_H
#define SCREENCTL_H

#include "xorg.h"
#include <thread>
#include "../commons/defs.h"

class ScreenCtl
{
public:
    ScreenCtl(Xorg *server);
    ~ScreenCtl();
private:
    Xorg *m_server;
    std::vector<std::thread> m_threads;
    convar gamma_refresh_cv;
    bool m_quit = false;
    void stop();

    void reapplyGamma();
};

#endif // SCREENCTL_H
