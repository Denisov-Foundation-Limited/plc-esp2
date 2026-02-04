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
#include "core/network/telegram/telegram_bot.hpp"
#include "clients/rfid_reader.hpp"
#include "clients/ring_client.hpp"
#include "core/display.hpp"
#include "hal/gpio/extender.hpp"
#include "controllers/controllers.hpp"
#include "utils/meteo_history.hpp"

template <size_t N>
class TaskBinder
{
public:
    TaskBinder(TaskManager<N> &tm, WifiManager &wifi, TelegramBot &tgbot, Extender &ext,
               Controllers &controllers, MeteoHistory &meteo_history, RfidReader &rfid_reader,
               RingClient &ring_client, Display &display)
        : _tm(tm),
          _wifi(wifi),
          _tgbot(tgbot),
          _ext(ext),
          _controllers(controllers),
          _meteo_history(meteo_history),
          _rfid_reader(rfid_reader),
          _ring_client(ring_client),
          _display(display)
    {
    }

    void bindAll()
    {
        bindWiFiManager();
        bindTgbot();
        bindExtender();
        bindControllers();
        bindRfidReader_();
        bindRingClient_();
        bindMeteoHistory_();
        bindDisplay_();
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
    typename TaskManager<N>::Handle bindControllers()
    {
        typename TaskManager<N>::Options opt;
        opt.interval_ms = 50;
        opt.priority = TaskManager<N>::Priority::Normal;
        return _tm.template add<&Controllers::task>(_controllers, opt);
    }

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

    typename TaskManager<N>::Handle bindMeteoHistory_()
    {
        typename TaskManager<N>::Options opt;
        opt.interval_ms = 60000;
        opt.priority = TaskManager<N>::Priority::Low;
        return _tm.template add<&MeteoHistory::task>(_meteo_history, opt);
    }

    TaskManager<N> &_tm;
    WifiManager &_wifi;
    TelegramBot &_tgbot;
    Extender &_ext;
    Controllers &_controllers;
    MeteoHistory &_meteo_history;
    RfidReader &_rfid_reader;
    RingClient &_ring_client;
    Display &_display;
    typename TaskManager<N>::Handle _ftest_task{};
    typename TaskManager<N>::Handle _ext_task{};
    typename TaskManager<N>::Handle _rfid_task{};
    typename TaskManager<N>::Handle _ring_task{};
    typename TaskManager<N>::Handle _display_task{};

    void tgbotTask_()
    {
        if (_wifi.isConnected())
            _tgbot.task();
    }

    typename TaskManager<N>::Handle bindRfidReader_()
    {
        typename TaskManager<N>::Options opt;
        opt.interval_ms = 100;
        opt.priority = TaskManager<N>::Priority::Low;
        _rfid_task = _tm.template add<&RfidReader::task>(_rfid_reader, opt);
        return _rfid_task;
    }

    typename TaskManager<N>::Handle bindRingClient_()
    {
        typename TaskManager<N>::Options opt;
        opt.interval_ms = 50;
        opt.priority = TaskManager<N>::Priority::Low;
        _ring_task = _tm.template add<&RingClient::task>(_ring_client, opt);
        return _ring_task;
    }

    typename TaskManager<N>::Handle bindDisplay_()
    {
        typename TaskManager<N>::Options opt;
        opt.interval_ms = 5000;
        opt.priority = TaskManager<N>::Priority::Low;
        _display_task = _tm.template add<&Display::task>(_display, opt);
        return _display_task;
    }
};
