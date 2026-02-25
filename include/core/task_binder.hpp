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
#include "core/display.hpp"
#include "core/stack/stack_runtime.hpp"
#include "hal/gpio/extender.hpp"
#include "controllers/controllers.hpp"
#include "utils/meteo_history.hpp"
#include "utils/logger.hpp"
#include "plc/plc_control.hpp"

template <size_t N>
class TaskBinder
{
public:
    TaskBinder(TaskManager<N> &tm, WifiManager &wifi, TelegramBot &tgbot, Extender &ext,
               Controllers &controllers, MeteoHistory &meteo_history,
               Display &display, PlcControl &plc, Logger &logs)
        : _tm(tm),
          _wifi(wifi),
          _tgbot(tgbot),
          _ext(ext),
          _controllers(controllers),
          _meteo_history(meteo_history),
          _display(display),
          _plc(plc),
          _logs(logs)
    {
    }

    void bindAll()
    {
        bindWiFiManager();
        bindTgbot();
        bindExtender();
        bindControllersStorage_();
        bindSockets_();
        bindMeteo_();
        bindThermo_();
        bindTanks_();
        bindSeptic_();
        bindSecurity_();
        bindRing_();
        bindWatering_();
        bindAvr_();
        bindLeak_();
        bindMeteoHistory_();
        bindDisplay_();
        bindPlc_();
        if (_tm.used() == _tm.capacity())
            _logs.warn(F("TASK"), F("TaskManager is full: %u/%u"),
                       (unsigned)_tm.used(), (unsigned)_tm.capacity());
    }

    template <typename FtestT>
    typename TaskManager<N>::Handle bindFtest(FtestT &ftest)
    {
        typename TaskManager<N>::Options opt;
        opt.interval_ms = 500;
        opt.priority = TaskManager<N>::Priority::Normal;
        opt.enabled = false;
        _ftest_task = _tm.template add<&FtestT::task>(ftest, opt);
        if (!_ftest_task)
            _logs.error(F("TASK"), F("Bind failed: ftest"));
        return _ftest_task;
    }

    typename TaskManager<N>::Handle getFtestTask() const { return _ftest_task; }

    void bindStack(StackRuntime &stack)
    {
        bindStackPost_(stack);
        bindStackFlush_(stack);
        if (_tm.used() == _tm.capacity())
            _logs.warn(F("TASK"), F("TaskManager is full: %u/%u"),
                       (unsigned)_tm.used(), (unsigned)_tm.capacity());
    }

private:
    typename TaskManager<N>::Handle bindControllersStorage_()
    {
        typename TaskManager<N>::Options opt;
        opt.interval_ms = 50;
        opt.priority = TaskManager<N>::Priority::Normal;
        return addChecked_<&Controllers::task>(_controllers, opt, "controllers_storage");
    }

    typename TaskManager<N>::Handle bindSockets_()
    {
        typename TaskManager<N>::Options opt;
        opt.interval_ms = 50;
        opt.priority = TaskManager<N>::Priority::Normal;
        return addChecked_<&SocketController::task>(_controllers.sockets(), opt, "sockets");
    }

    typename TaskManager<N>::Handle bindMeteo_()
    {
        typename TaskManager<N>::Options opt;
        opt.interval_ms = 50;
        opt.priority = TaskManager<N>::Priority::Normal;
        return addChecked_<&MeteoController::task>(_controllers.meteo(), opt, "meteo");
    }

    typename TaskManager<N>::Handle bindThermo_()
    {
        typename TaskManager<N>::Options opt;
        opt.interval_ms = 50;
        opt.priority = TaskManager<N>::Priority::Normal;
        return addChecked_<&ThermoController::task>(_controllers.thermo(), opt, "thermo");
    }

    typename TaskManager<N>::Handle bindTanks_()
    {
        typename TaskManager<N>::Options opt;
        opt.interval_ms = 50;
        opt.priority = TaskManager<N>::Priority::Normal;
        return addChecked_<&TankController::task>(_controllers.tanks(), opt, "tanks");
    }

    typename TaskManager<N>::Handle bindSeptic_()
    {
        typename TaskManager<N>::Options opt;
        opt.interval_ms = 50;
        opt.priority = TaskManager<N>::Priority::Normal;
        return addChecked_<&SepticController::task>(_controllers.septic(), opt, "septic");
    }

    typename TaskManager<N>::Handle bindSecurity_()
    {
        typename TaskManager<N>::Options opt;
        opt.interval_ms = 50;
        opt.priority = TaskManager<N>::Priority::Normal;
        return addChecked_<&SecurityController::task>(_controllers.security(), opt, "security");
    }

    typename TaskManager<N>::Handle bindRing_()
    {
        typename TaskManager<N>::Options opt;
        opt.interval_ms = 50;
        opt.priority = TaskManager<N>::Priority::Normal;
        return addChecked_<&RingController::task>(_controllers.ring(), opt, "ring");
    }

    typename TaskManager<N>::Handle bindWatering_()
    {
        typename TaskManager<N>::Options opt;
        opt.interval_ms = 50;
        opt.priority = TaskManager<N>::Priority::Normal;
        return addChecked_<&WateringController::task>(_controllers.watering(), opt, "watering");
    }

    typename TaskManager<N>::Handle bindAvr_()
    {
        typename TaskManager<N>::Options opt;
        opt.interval_ms = 50;
        opt.priority = TaskManager<N>::Priority::Normal;
        return addChecked_<&AvrController::task>(_controllers.avr(), opt, "avr");
    }

    typename TaskManager<N>::Handle bindLeak_()
    {
        typename TaskManager<N>::Options opt;
        opt.interval_ms = 50;
        opt.priority = TaskManager<N>::Priority::Normal;
        return addChecked_<&LeakController::task>(_controllers.leak(), opt, "leak");
    }

    typename TaskManager<N>::Handle bindWiFiManager()
    {
        typename TaskManager<N>::Options opt;
        opt.interval_ms = 1000;
        opt.priority = TaskManager<N>::Priority::Normal;
        return addChecked_<&WifiManager::task>(_wifi, opt, "wifi");
    }

    typename TaskManager<N>::Handle bindTgbot()
    {
        typename TaskManager<N>::Options opt;
        opt.interval_ms = 200;
        opt.priority = TaskManager<N>::Priority::Low;
        return addChecked_<&TaskBinder::tgbotTask_>(*this, opt, "telegram_bot");
    }

    typename TaskManager<N>::Handle bindExtender()
    {
        typename TaskManager<N>::Options opt;
        opt.interval_ms = 50;
        opt.priority = TaskManager<N>::Priority::Low;
        _ext_task = addChecked_<&Extender::task>(_ext, opt, "extender");
        return _ext_task;
    }

    typename TaskManager<N>::Handle bindMeteoHistory_()
    {
        typename TaskManager<N>::Options opt;
        opt.interval_ms = 60000;
        opt.priority = TaskManager<N>::Priority::Low;
        return addChecked_<&MeteoHistory::task>(_meteo_history, opt, "meteo_history");
    }

    TaskManager<N> &_tm;
    WifiManager &_wifi;
    TelegramBot &_tgbot;
    Extender &_ext;
    Controllers &_controllers;
    MeteoHistory &_meteo_history;
    Display &_display;
    PlcControl &_plc;
    Logger &_logs;
    typename TaskManager<N>::Handle _ftest_task{};
    typename TaskManager<N>::Handle _ext_task{};
    typename TaskManager<N>::Handle _display_task{};
    typename TaskManager<N>::Handle _stack_pre_task{};
    typename TaskManager<N>::Handle _stack_post_task{};
    typename TaskManager<N>::Handle _stack_flush_task{};

    void tgbotTask_()
    {
        if (_wifi.isConnected())
            _tgbot.task();
    }

    typename TaskManager<N>::Handle bindDisplay_()
    {
        typename TaskManager<N>::Options opt;
        opt.interval_ms = 250;
        opt.priority = TaskManager<N>::Priority::Low;
        _display_task = addChecked_<&Display::task>(_display, opt, "display");
        return _display_task;
    }

    typename TaskManager<N>::Handle bindPlc_()
    {
        typename TaskManager<N>::Options opt;
        opt.interval_ms = 100;
        opt.priority = TaskManager<N>::Priority::Normal;
        return addChecked_<&PlcControl::task>(_plc, opt, "plc");
    }

    typename TaskManager<N>::Handle bindStackPre_(StackRuntime &stack)
    {
        typename TaskManager<N>::Options opt;
        opt.interval_ms = 1;
        opt.priority = TaskManager<N>::Priority::Highest;
        _stack_pre_task = addChecked_<&StackRuntime::taskPre>(stack, opt, "stack_pre");
        return _stack_pre_task;
    }

    typename TaskManager<N>::Handle bindStackPost_(StackRuntime &stack)
    {
        typename TaskManager<N>::Options opt;
        opt.interval_ms = 1;
        opt.priority = TaskManager<N>::Priority::High;
        _stack_post_task = addChecked_<&StackRuntime::taskPost>(stack, opt, "stack_post");
        return _stack_post_task;
    }

    typename TaskManager<N>::Handle bindStackFlush_(StackRuntime &stack)
    {
        typename TaskManager<N>::Options opt;
        opt.interval_ms = 1;
        opt.priority = TaskManager<N>::Priority::Lowest;
        _stack_flush_task = addChecked_<&StackRuntime::taskFlush>(stack, opt, "stack_flush");
        return _stack_flush_task;
    }

    template <auto Method, typename T>
    typename TaskManager<N>::Handle addChecked_(T &obj, const typename TaskManager<N>::Options &opt,
                                                const char *name)
    {
        const auto h = _tm.template add<Method>(obj, opt);
        if (!h)
            _logs.error(F("TASK"), F("Bind failed: %s"), name ? name : "-");
        return h;
    }
};
