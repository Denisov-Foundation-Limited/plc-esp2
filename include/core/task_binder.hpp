/**********************************************************************/
/*                                                                    */
/* Programmable Logic Controller for ESP microcontrollers             */
/*                                                                    */
/* Copyright (C) 2026 Denisov Foundation Limited                      */
/* License: GPLv3                                                     */
/* Written by Sergey Denisov aka LittleBuster                         */
/* Email: DenisovFoundationLtd@gmail.com                              */
/*                                                                    */
/**********************************************************************/

#pragma once

#include "core/task_manager.hpp"
#include "core/wifi_manager.hpp"

template <size_t N>
class TaskBinder
{
public:
    TaskBinder(TaskManager<N> &tm, WifiManager &wifi)
        : _tm(tm), _wifi(wifi)
    {
    }

    void bindAll()
    {
        bindWiFiManager();
    }

private:
    typename TaskManager<N>::Handle bindWiFiManager()
    {
        typename TaskManager<N>::Options opt;
        opt.interval_ms = 1000;
        opt.priority = TaskManager<N>::Priority::Normal;
        return _tm.template add<&WifiManager::tick>(_wifi, opt);
    }

    TaskManager<N> &_tm;
    WifiManager &_wifi;
};
