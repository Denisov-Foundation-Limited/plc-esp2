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
#include "core/network/wifi_manager.hpp"
#include "core/network/telegram/telegram.hpp"
#include "hal/gpio/extender.hpp"

template <size_t N>
class TaskBinder
{
public:
    TaskBinder(TaskManager<N> &tm, WifiManager &wifi, TelegramClient &tgbot, Extender &ext)
        : _tm(tm), _wifi(wifi), _tgbot(tgbot), _ext(ext)
    {
    }

    void bindAll()
    {
        bindWiFiManager();
        bindTgbot();
        bindExtender();
    }

    template <typename FtestT>
    typename TaskManager<N>::Handle bindFtest(FtestT &ftest)
    {
        typename TaskManager<N>::Options opt;
        opt.interval_ms = 500;
        opt.priority = TaskManager<N>::Priority::Normal;
        opt.enabled = false;
        _ftest_task = _tm.template add<&FtestT::task>(ftest, opt);
        return _ftest_task;
    }

    typename TaskManager<N>::Handle getFtestTask() const { return _ftest_task; }

private:
    typename TaskManager<N>::Handle bindWiFiManager()
    {
        typename TaskManager<N>::Options opt;
        opt.interval_ms = 1000;
        opt.priority = TaskManager<N>::Priority::Normal;
        return _tm.template add<&WifiManager::task>(_wifi, opt);
    }

    typename TaskManager<N>::Handle bindTgbot()
    {
        typename TaskManager<N>::Options opt;
        opt.interval_ms = 500;
        opt.priority = TaskManager<N>::Priority::Low;
        return _tm.template add<&TaskBinder::tgbotTask_>(*this, opt);
    }

    typename TaskManager<N>::Handle bindExtender()
    {
        typename TaskManager<N>::Options opt;
        opt.interval_ms = _ext.rescanIntervalMs();
        opt.priority = TaskManager<N>::Priority::Low;
        _ext_task = _tm.template add<&Extender::task>(_ext, opt);
        return _ext_task;
    }

    TaskManager<N> &_tm;
    WifiManager &_wifi;
    TelegramClient &_tgbot;
    Extender &_ext;
    typename TaskManager<N>::Handle _ftest_task{};
    typename TaskManager<N>::Handle _ext_task{};

    void tgbotTask_()
    {
        if (_wifi.isConnected())
            _tgbot.task();
    }
};
