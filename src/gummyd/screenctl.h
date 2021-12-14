#ifndef SCREENCTL_H
#define SCREENCTL_H

#include "xorg.h"
#include "sysfs.h"
#include "../common/defs.h"

#include <thread>
#include <sdbus-c++/ProxyInterfaces.h>

class Monitor;

class ScreenCtl
{
public:
    ScreenCtl(Xorg *server);
    void notifyTemp();
    void notifyMonitor(int scr_idx);
    int getAutoTempStep();
    ~ScreenCtl();
private:
    Xorg *m_server;
    std::vector<Device> m_devices;
    std::vector<std::thread> m_threads;
    std::vector<Monitor> m_monitors;
    convar m_gamma_refresh_cv;
    convar m_temp_cv;
    int m_auto_temp_step;
    bool m_quit = false;
    bool m_force_temp_change;

    void reapplyGamma();
    void adjustTemperature();

    std::unique_ptr<sdbus::IProxy> m_dbus_proxy;
    bool listenWakeupSignal();
};

class Monitor
{
public:
    Monitor(Monitor&&);
    Monitor(Xorg* server, Device *device, int scr_idx);

    void notify();
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
    void adjustBrightness(convar&);
};

#endif // SCREENCTL_H
