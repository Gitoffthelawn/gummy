#ifndef SCREENCTL_H
#define SCREENCTL_H

#include "xorg.h"
#include "sysfs.h"
#include "../commons/defs.h"

#include <thread>

class Monitor;

class ScreenCtl
{
public:
    ScreenCtl(Xorg *server);
    ~ScreenCtl();
private:
    Xorg *m_server;
    std::vector<Device> m_devices;
    std::vector<std::thread> m_threads;
    std::vector<Monitor> m_monitors;
    convar gamma_refresh_cv;
    bool m_quit = false;

    void reapplyGamma();
};

class Monitor
{
public:
    Monitor(Monitor&&);
    Monitor(Xorg* server, Device *device, int scr_idx);
    ~Monitor();
private:
    Xorg *m_server;
    Device *m_device;
    const int m_scr_idx;
    std::unique_ptr<std::thread> m_ss_thr;
    convar m_ss_cv;
    std::mutex m_brt_mtx;
    int m_ss_brt;
    bool m_brt_needs_change;
    bool m_quit = false;

    void capture();
    void adjust(convar&);
};

#endif // SCREENCTL_H
