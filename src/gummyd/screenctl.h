/**
* gummy
* Copyright (C) 2022  Francesco Fusco
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef SCREENCTL_H
#define SCREENCTL_H

#include "xorg.h"
#include "sysfs.h"
#include "../common/defs.h"

#include <thread>
#include <condition_variable>
#include <sdbus-c++/ProxyInterfaces.h>

class Monitor;

class ScreenCtl
{
public:
    ScreenCtl(Xorg *server);
    ~ScreenCtl();
    void applyOptions(const std::string&);
private:
    void notifyTemp();
    void notifyMonitor(int scr_idx);
    int getAutoTempStep();

    Xorg *m_server;
    std::vector<Device> m_devices;
    std::vector<std::thread> m_threads;
    std::vector<Monitor> m_monitors;
    std::condition_variable m_gamma_refresh_cv;
    std::condition_variable m_temp_cv;
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
    std::condition_variable m_ss_cv;
    std::mutex m_brt_mtx;
    int m_ss_brt;
    bool m_brt_needs_change;
    bool m_quit = false;

    void capture();
    void adjustBrightness(std::condition_variable&);
};

#endif // SCREENCTL_H
