#ifndef SCREENCTL_H
#define SCREENCTL_H

#include "xorg.h"
#include <thread>
#include "../commons/defs.h"

class BrtCtl;

class ScreenCtl
{
public:
    ScreenCtl(Xorg *server);
    ~ScreenCtl();
private:
    Xorg *m_server;
    std::vector<std::thread> m_threads;
    std::vector<BrtCtl*> m_brt_controllers;
    convar gamma_refresh_cv;
    bool m_quit = false;

    void stop();
    void reapplyGamma();
};

class BrtCtl
{
public:
    BrtCtl(Xorg* server, int scr_idx);
    ~BrtCtl();
private:
    Xorg *m_server;
    const int m_scr_idx;
    std::unique_ptr<std::thread> m_ss_thr;
    convar m_ss_cv;
    std::mutex m_brt_mtx;
    int m_ss_brt;
    bool m_brt_needs_change;
    bool m_quit = false;

    void captureScreen();
    void adjustBrightness(convar&);
};

#endif // SCREENCTL_H
