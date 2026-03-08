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
#include "core/network/gsm_modem.hpp"
#include "core/network/cloud/cloud_client.hpp"
#include "core/network/wifi_manager.hpp"
#include "core/network/telegram/telegram_bot.hpp"
#include "core/display.hpp"
#include "core/stack/stack_runtime.hpp"
#include "hal/gpio/extender.hpp"
#include "controllers/controllers.hpp"
#include "utils/meteo_history.hpp"
#include "utils/logger.hpp"
#include "plc/plc_control.hpp"

#if defined(ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#endif

#ifndef TASK_BINDER_RTOS_DEBUG
#define TASK_BINDER_RTOS_DEBUG 0
#endif

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
        bindGsm_();
        bindCloud_();
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

    void setGsmModem(GsmModem &gsm) { _gsm = &gsm; }

    void setCloudClient(CloudClient &cloud) { _cloud = &cloud; }

    void bindStack(StackRuntime &stack)
    {
#if defined(ESP32)
        _stack_runtime = &stack;
        if (_stack_phase_mtx == nullptr)
            _stack_phase_mtx = xSemaphoreCreateMutex();
        if (_stack_evt_queue == nullptr)
            _stack_evt_queue = xQueueCreate(1, sizeof(uint8_t));
        if (_stack_evt_task == nullptr)
        {
            BaseType_t ok = xTaskCreatePinnedToCore(&TaskBinder::stackEventTaskEntry_, "stack_evt", 4096, this, 3,
                                                    &_stack_evt_task, tskNO_AFFINITY);
            if (ok != pdPASS)
                _logs.error(F("TASK"), F("Bind failed: stack_evt"));
        }
#else
        bindStackPost_(stack);
        bindStackFlush_(stack);
#endif
        if (_tm.used() == _tm.capacity())
            _logs.warn(F("TASK"), F("TaskManager is full: %u/%u"),
                       (unsigned)_tm.used(), (unsigned)_tm.capacity());
    }

    void runStackPre(StackRuntime &stack)
    {
#if defined(ESP32)
        _stack_runtime = &stack;
        if (_stack_phase_mtx)
            xSemaphoreTake(_stack_phase_mtx, portMAX_DELAY);
        stack.setTaskPhase(StackRuntime::TaskPhase::PreNetwork);
        stack.taskPre();
        stack.setTaskPhase(StackRuntime::TaskPhase::Idle);
        if (_stack_phase_mtx)
            xSemaphoreGive(_stack_phase_mtx);
#else
        stack.setTaskPhase(StackRuntime::TaskPhase::PreNetwork);
        stack.taskPre();
#endif
    }

    void notifyStackPostNetwork()
    {
#if defined(ESP32)
        if (_stack_evt_queue == nullptr)
            return;
        if (_stack_evt_pending)
            return;
        _stack_evt_pending = true;
        uint8_t evt = 1;
        xQueueOverwrite(_stack_evt_queue, &evt);
#endif
    }

private:
    typename TaskManager<N>::Handle bindControllersStorage_()
    {
#if defined(ESP32)
        if (_control_task == nullptr)
        {
            BaseType_t ok = xTaskCreatePinnedToCore(&TaskBinder::controlTaskEntry_, "control_loop", 8192, this, 2,
                                                    &_control_task, tskNO_AFFINITY);
            if (ok != pdPASS)
                _logs.error(F("TASK"), F("Bind failed: control_loop"));
        }
        return {};
#else
        typename TaskManager<N>::Options opt;
        opt.interval_ms = 50;
        opt.priority = TaskManager<N>::Priority::Normal;
        return addChecked_<&Controllers::task>(_controllers, opt, "controllers_storage");
#endif
    }

    typename TaskManager<N>::Handle bindSockets_()
    {
#if defined(ESP32)
        return {};
#else
        typename TaskManager<N>::Options opt;
        opt.interval_ms = 50;
        opt.priority = TaskManager<N>::Priority::Normal;
        return addChecked_<&SocketController::task>(_controllers.sockets(), opt, "sockets");
#endif
    }

    typename TaskManager<N>::Handle bindMeteo_()
    {
#if defined(ESP32)
        return {};
#else
        typename TaskManager<N>::Options opt;
        opt.interval_ms = 50;
        opt.priority = TaskManager<N>::Priority::Normal;
        return addChecked_<&MeteoController::task>(_controllers.meteo(), opt, "meteo");
#endif
    }

    typename TaskManager<N>::Handle bindThermo_()
    {
#if defined(ESP32)
        return {};
#else
        typename TaskManager<N>::Options opt;
        opt.interval_ms = 50;
        opt.priority = TaskManager<N>::Priority::Normal;
        return addChecked_<&ThermoController::task>(_controllers.thermo(), opt, "thermo");
#endif
    }

    typename TaskManager<N>::Handle bindTanks_()
    {
#if defined(ESP32)
        return {};
#else
        typename TaskManager<N>::Options opt;
        opt.interval_ms = 50;
        opt.priority = TaskManager<N>::Priority::Normal;
        return addChecked_<&TankController::task>(_controllers.tanks(), opt, "tanks");
#endif
    }

    typename TaskManager<N>::Handle bindSeptic_()
    {
#if defined(ESP32)
        return {};
#else
        typename TaskManager<N>::Options opt;
        opt.interval_ms = 50;
        opt.priority = TaskManager<N>::Priority::Normal;
        return addChecked_<&SepticController::task>(_controllers.septic(), opt, "septic");
#endif
    }

    typename TaskManager<N>::Handle bindSecurity_()
    {
#if defined(ESP32)
        return {};
#else
        typename TaskManager<N>::Options opt;
        opt.interval_ms = 50;
        opt.priority = TaskManager<N>::Priority::Normal;
        return addChecked_<&SecurityController::task>(_controllers.security(), opt, "security");
#endif
    }

    typename TaskManager<N>::Handle bindRing_()
    {
#if defined(ESP32)
        return {};
#else
        typename TaskManager<N>::Options opt;
        opt.interval_ms = 50;
        opt.priority = TaskManager<N>::Priority::Normal;
        return addChecked_<&RingController::task>(_controllers.ring(), opt, "ring");
#endif
    }

    typename TaskManager<N>::Handle bindWatering_()
    {
#if defined(ESP32)
        return {};
#else
        typename TaskManager<N>::Options opt;
        opt.interval_ms = 50;
        opt.priority = TaskManager<N>::Priority::Normal;
        return addChecked_<&WateringController::task>(_controllers.watering(), opt, "watering");
#endif
    }

    typename TaskManager<N>::Handle bindAvr_()
    {
#if defined(ESP32)
        return {};
#else
        typename TaskManager<N>::Options opt;
        opt.interval_ms = 50;
        opt.priority = TaskManager<N>::Priority::Normal;
        return addChecked_<&AvrController::task>(_controllers.avr(), opt, "avr");
#endif
    }

    typename TaskManager<N>::Handle bindLeak_()
    {
#if defined(ESP32)
        return {};
#else
        typename TaskManager<N>::Options opt;
        opt.interval_ms = 50;
        opt.priority = TaskManager<N>::Priority::Normal;
        return addChecked_<&LeakController::task>(_controllers.leak(), opt, "leak");
#endif
    }

    typename TaskManager<N>::Handle bindWiFiManager()
    {
#if defined(ESP32)
        if (_wifi_task == nullptr)
        {
            BaseType_t ok = xTaskCreatePinnedToCore(&TaskBinder::wifiTaskEntry_, "wifi_mgr", 3072, this, 2, &_wifi_task,
                                                    tskNO_AFFINITY);
            if (ok != pdPASS)
                _logs.error(F("TASK"), F("Bind failed: wifi"));
        }
        return {};
#else
        typename TaskManager<N>::Options opt;
        opt.interval_ms = 1000;
        opt.priority = TaskManager<N>::Priority::Normal;
        return addChecked_<&WifiManager::task>(_wifi, opt, "wifi");
#endif
    }

    typename TaskManager<N>::Handle bindTgbot()
    {
#if defined(ESP32)
        if (_tgbot_task == nullptr)
        {
            BaseType_t ok = xTaskCreatePinnedToCore(&TaskBinder::telegramTaskEntry_, "tg_bot", 6144, this, 1,
                                                    &_tgbot_task, tskNO_AFFINITY);
            if (ok != pdPASS)
                _logs.error(F("TASK"), F("Bind failed: telegram_bot"));
        }
        return {};
#else
        typename TaskManager<N>::Options opt;
        opt.interval_ms = 200;
        opt.priority = TaskManager<N>::Priority::Low;
        return addChecked_<&TaskBinder::tgbotTask_>(*this, opt, "telegram_bot");
#endif
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
#if defined(ESP32)
        if (_meteo_history_task == nullptr)
        {
            BaseType_t ok = xTaskCreatePinnedToCore(&TaskBinder::meteoHistoryTaskEntry_, "meteo_hist", 4096, this, 1,
                                                    &_meteo_history_task, tskNO_AFFINITY);
            if (ok != pdPASS)
                _logs.error(F("TASK"), F("Bind failed: meteo_history"));
        }
        return {};
#else
        typename TaskManager<N>::Options opt;
        opt.interval_ms = 60000;
        opt.priority = TaskManager<N>::Priority::Low;
        return addChecked_<&MeteoHistory::task>(_meteo_history, opt, "meteo_history");
#endif
    }

    typename TaskManager<N>::Handle bindGsm_()
    {
        if (!_gsm)
            return {};
#if defined(ESP32)
        if (_gsm_task_rtos == nullptr)
        {
            BaseType_t ok = xTaskCreatePinnedToCore(&TaskBinder::gsmTaskEntry_, "gsm_modem", 4096, this, 3,
                                                    &_gsm_task_rtos, tskNO_AFFINITY);
            if (ok != pdPASS)
                _logs.error(F("TASK"), F("Bind failed: gsm_modem"));
        }
        return {};
#else
        typename TaskManager<N>::Options opt;
        opt.interval_ms = 50;
        opt.priority = TaskManager<N>::Priority::High;
        _gsm_task = addChecked_<&GsmModem::loop>(*_gsm, opt, "gsm_modem");
        return _gsm_task;
#endif
    }

    typename TaskManager<N>::Handle bindCloud_()
    {
        if (!_cloud)
            return {};
#if defined(ESP32)
        if (_cloud_task_rtos == nullptr)
        {
            BaseType_t ok = xTaskCreatePinnedToCore(&TaskBinder::cloudTaskEntry_, "cloud_cli", 8192, this, 3,
                                                    &_cloud_task_rtos, tskNO_AFFINITY);
            if (ok != pdPASS)
                _logs.error(F("TASK"), F("Bind failed: cloud_client"));
        }
        return {};
#else
        typename TaskManager<N>::Options opt;
        opt.interval_ms = 50;
        opt.priority = TaskManager<N>::Priority::High;
        _cloud_task = addChecked_<&CloudClient::loop>(*_cloud, opt, "cloud_client");
        return _cloud_task;
#endif
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
    GsmModem *_gsm = nullptr;
    CloudClient *_cloud = nullptr;
    typename TaskManager<N>::Handle _ftest_task{};
    typename TaskManager<N>::Handle _gsm_task{};
    typename TaskManager<N>::Handle _cloud_task{};
    typename TaskManager<N>::Handle _ext_task{};
    typename TaskManager<N>::Handle _display_task{};
    typename TaskManager<N>::Handle _stack_pre_task{};
    typename TaskManager<N>::Handle _stack_post_task{};
    typename TaskManager<N>::Handle _stack_flush_task{};
#if defined(ESP32)
    struct RtosDebugStats
    {
        uint32_t last_exec_us = 0;
        uint32_t max_exec_us = 0;
        uint64_t sum_exec_us = 0;
        uint32_t count = 0;
        uint32_t last_log_ms = 0;
        uint32_t window_start_ms = 0;
        uint32_t window_max_us = 0;
        uint64_t window_sum_exec_us = 0;
        uint32_t window_count = 0;
        UBaseType_t min_stack_hwm_words = 0;
    };

    TaskHandle_t _wifi_task = nullptr;
    TaskHandle_t _tgbot_task = nullptr;
    TaskHandle_t _meteo_history_task = nullptr;
    TaskHandle_t _control_task = nullptr;
    TaskHandle_t _gsm_task_rtos = nullptr;
    TaskHandle_t _cloud_task_rtos = nullptr;
    TaskHandle_t _stack_evt_task = nullptr;
    QueueHandle_t _stack_evt_queue = nullptr;
    SemaphoreHandle_t _stack_phase_mtx = nullptr;
    StackRuntime *_stack_runtime = nullptr;
    volatile bool _stack_evt_pending = false;
#if TASK_BINDER_RTOS_DEBUG
    RtosDebugStats _dbg_wifi{};
    RtosDebugStats _dbg_tg{};
    RtosDebugStats _dbg_meteo_history{};
    RtosDebugStats _dbg_control{};
    RtosDebugStats _dbg_gsm{};
    RtosDebugStats _dbg_cloud{};
    RtosDebugStats _dbg_stack_evt{};
#endif
#endif

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

#if defined(ESP32)
    void updateRtosDebug_(const char *task_name, uint32_t exec_us, UBaseType_t stack_hwm_words, RtosDebugStats &st)
    {
        const uint32_t now = millis();
        if (st.window_start_ms == 0)
            st.window_start_ms = now;
        if ((uint32_t)(now - st.window_start_ms) >= 60000u)
        {
            st.window_start_ms = now;
            st.window_max_us = 0;
            st.window_sum_exec_us = 0;
            st.window_count = 0;
        }

        st.last_exec_us = exec_us;
        if (exec_us > st.max_exec_us)
            st.max_exec_us = exec_us;
        if (exec_us > st.window_max_us)
            st.window_max_us = exec_us;
        st.sum_exec_us += exec_us;
        st.count += 1;
        st.window_sum_exec_us += exec_us;
        st.window_count += 1;
        if (st.min_stack_hwm_words == 0 || stack_hwm_words < st.min_stack_hwm_words)
            st.min_stack_hwm_words = stack_hwm_words;

        if ((uint32_t)(now - st.last_log_ms) < 10000u)
            return;
        st.last_log_ms = now;
        const uint32_t avg_us = st.count ? (uint32_t)(st.sum_exec_us / st.count) : 0;
        const uint32_t win_avg_us = st.window_count ? (uint32_t)(st.window_sum_exec_us / st.window_count) : 0;
        _logs.debug(F("RTOS"),
                    F("task: %s exec_us: %lu avg_us: %lu max_us: %lu wavg_us: %lu wmax_us: %lu cnt: %lu hwm: %u min_hwm: %u"),
                    task_name ? task_name : "-", (unsigned long)st.last_exec_us,
                    (unsigned long)avg_us, (unsigned long)st.max_exec_us,
                    (unsigned long)win_avg_us, (unsigned long)st.window_max_us,
                    (unsigned long)st.count, (unsigned)stack_hwm_words,
                    (unsigned)st.min_stack_hwm_words);
    }

    static void wifiTaskEntry_(void *arg)
    {
        auto *self = static_cast<TaskBinder *>(arg);
        TickType_t last = xTaskGetTickCount();
        for (;;)
        {
            const uint32_t t0 = micros();
            self->_wifi.task();
#if TASK_BINDER_RTOS_DEBUG
            const uint32_t dt = (uint32_t)(micros() - t0);
            const UBaseType_t hwm = uxTaskGetStackHighWaterMark(nullptr);
            self->updateRtosDebug_("wifi", dt, hwm, self->_dbg_wifi);
#endif
            vTaskDelayUntil(&last, pdMS_TO_TICKS(1000));
        }
    }

    static void telegramTaskEntry_(void *arg)
    {
        auto *self = static_cast<TaskBinder *>(arg);
        TickType_t last = xTaskGetTickCount();
        for (;;)
        {
            const uint32_t t0 = micros();
            self->tgbotTask_();
#if TASK_BINDER_RTOS_DEBUG
            const uint32_t dt = (uint32_t)(micros() - t0);
            const UBaseType_t hwm = uxTaskGetStackHighWaterMark(nullptr);
            self->updateRtosDebug_("telegram", dt, hwm, self->_dbg_tg);
#endif
            vTaskDelayUntil(&last, pdMS_TO_TICKS(200));
        }
    }

    static void meteoHistoryTaskEntry_(void *arg)
    {
        auto *self = static_cast<TaskBinder *>(arg);
        TickType_t last = xTaskGetTickCount();
        for (;;)
        {
            const uint32_t t0 = micros();
            self->_meteo_history.task();
#if TASK_BINDER_RTOS_DEBUG
            const uint32_t dt = (uint32_t)(micros() - t0);
            const UBaseType_t hwm = uxTaskGetStackHighWaterMark(nullptr);
            self->updateRtosDebug_("meteo_hist", dt, hwm, self->_dbg_meteo_history);
#endif
            vTaskDelayUntil(&last, pdMS_TO_TICKS(60000));
        }
    }

    static void gsmTaskEntry_(void *arg)
    {
        auto *self = static_cast<TaskBinder *>(arg);
        TickType_t last = xTaskGetTickCount();
        for (;;)
        {
            const uint32_t t0 = micros();
            if (self->_gsm)
                self->_gsm->loop();
#if TASK_BINDER_RTOS_DEBUG
            const uint32_t dt = (uint32_t)(micros() - t0);
            const UBaseType_t hwm = uxTaskGetStackHighWaterMark(nullptr);
            self->updateRtosDebug_("gsm", dt, hwm, self->_dbg_gsm);
#endif
            vTaskDelayUntil(&last, pdMS_TO_TICKS(50));
        }
    }

    static void controlTaskEntry_(void *arg)
    {
        auto *self = static_cast<TaskBinder *>(arg);
        TickType_t last = xTaskGetTickCount();
        for (;;)
        {
            const uint32_t t0 = micros();
            self->_controllers.task();
            self->_controllers.sockets().task();
            self->_controllers.meteo().task();
            self->_controllers.thermo().task();
            self->_controllers.tanks().task();
            self->_controllers.septic().task();
            self->_controllers.security().task();
            self->_controllers.ring().task();
            self->_controllers.watering().task();
            self->_controllers.avr().task();
            self->_controllers.leak().task();
#if TASK_BINDER_RTOS_DEBUG
            const uint32_t dt = (uint32_t)(micros() - t0);
            const UBaseType_t hwm = uxTaskGetStackHighWaterMark(nullptr);
            self->updateRtosDebug_("control", dt, hwm, self->_dbg_control);
#endif
            vTaskDelayUntil(&last, pdMS_TO_TICKS(50));
        }
    }

    static void cloudTaskEntry_(void *arg)
    {
        auto *self = static_cast<TaskBinder *>(arg);
        TickType_t last = xTaskGetTickCount();
        for (;;)
        {
            const uint32_t t0 = micros();
            if (self->_cloud)
                self->_cloud->loop();
#if TASK_BINDER_RTOS_DEBUG
            const uint32_t dt = (uint32_t)(micros() - t0);
            const UBaseType_t hwm = uxTaskGetStackHighWaterMark(nullptr);
            self->updateRtosDebug_("cloud", dt, hwm, self->_dbg_cloud);
#endif
            vTaskDelayUntil(&last, pdMS_TO_TICKS(50));
        }
    }

    static void stackEventTaskEntry_(void *arg)
    {
        auto *self = static_cast<TaskBinder *>(arg);
        for (;;)
        {
            uint8_t evt = 0;
            if (self->_stack_evt_queue == nullptr)
            {
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }
            if (xQueueReceive(self->_stack_evt_queue, &evt, portMAX_DELAY) != pdTRUE)
                continue;
            if (evt != 1 || self->_stack_runtime == nullptr)
                continue;
            self->_stack_evt_pending = false;

            const uint32_t t0 = micros();
            if (self->_stack_phase_mtx)
                xSemaphoreTake(self->_stack_phase_mtx, portMAX_DELAY);
            self->_stack_runtime->setTaskPhase(StackRuntime::TaskPhase::PostNetwork);
            self->_stack_runtime->taskPost();
            self->_stack_runtime->taskFlush();
            self->_stack_runtime->setTaskPhase(StackRuntime::TaskPhase::Idle);
            if (self->_stack_phase_mtx)
                xSemaphoreGive(self->_stack_phase_mtx);
#if TASK_BINDER_RTOS_DEBUG
            const uint32_t dt = (uint32_t)(micros() - t0);
            const UBaseType_t hwm = uxTaskGetStackHighWaterMark(nullptr);
            self->updateRtosDebug_("stack_evt", dt, hwm, self->_dbg_stack_evt);
#endif
        }
    }
#endif

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
