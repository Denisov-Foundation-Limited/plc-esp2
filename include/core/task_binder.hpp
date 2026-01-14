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
#include "plc/plc_control.hpp"

template <size_t N>
class TaskBinder
{
public:
    TaskBinder(TaskManager<N> &tm, WifiManager &wifi, PlcControl &plc, TelegramClient &tgbot)
        : _tm(tm), _wifi(wifi), _plc(plc), _tgbot(tgbot)
    {
    }

    void bindAll()
    {
        bindWiFiManager();
        bindPlcControl();
        bindTgbot();
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

    typename TaskManager<N>::Handle bindPlcControl()
    {
        typename TaskManager<N>::Options opt;
        opt.interval_ms = 1000;
        opt.priority = TaskManager<N>::Priority::Normal;
        return _tm.template add<&PlcControl::task>(_plc, opt);
    }

    typename TaskManager<N>::Handle bindTgbot()
    {
        typename TaskManager<N>::Options opt;
        opt.interval_ms = 500;
        opt.priority = TaskManager<N>::Priority::Low;
        return _tm.template add<&TaskBinder::tgbotTask_>(*this, opt);
    }

    TaskManager<N> &_tm;
    WifiManager &_wifi;
    PlcControl &_plc;
    TelegramClient &_tgbot;
    typename TaskManager<N>::Handle _ftest_task{};

    void tgbotTask_()
    {
        if (_wifi.isConnected())
            _tgbot.task();
    }
};
