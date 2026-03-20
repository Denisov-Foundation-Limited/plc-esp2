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

#include "core/network/gsm_modem.hpp"
#include "core/network/cloud/cloud_client.hpp"
#include "core/network/wifi_manager.hpp"
#include "core/network/network.hpp"
#include "core/display.hpp"
#include "core/plc_scan.hpp"
#include "core/runtime/app_runtime.hpp"
#include "hal/gpio/extender.hpp"
#include "controllers/controllers.hpp"
#include "utils/meteo_history.hpp"
#include "utils/logger.hpp"
#include "plc/plc_control.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <atomic>

#ifndef TASK_BINDER_RTOS_DEBUG
#define TASK_BINDER_RTOS_DEBUG 0
#endif

#ifndef TASK_BINDER_PLC_SCAN_TICK_MS
#define TASK_BINDER_PLC_SCAN_TICK_MS 1
#endif

#ifndef TASK_BINDER_NETWORK_LOOP_TICK_MS
#define TASK_BINDER_NETWORK_LOOP_TICK_MS 10
#endif

#ifndef TASK_BINDER_CONSOLE_LOOP_TICK_MS
#define TASK_BINDER_CONSOLE_LOOP_TICK_MS 10
#endif

#ifndef TASK_BINDER_TG_WIFI_RECOVER_STALL_MS
#define TASK_BINDER_TG_WIFI_RECOVER_STALL_MS 30000u
#endif

#ifndef TASK_BINDER_TG_WIFI_RECOVER_COOLDOWN_MS
#define TASK_BINDER_TG_WIFI_RECOVER_COOLDOWN_MS 120000u
#endif

#ifndef TASK_BINDER_TG_WIFI_RECOVER_FAIL_STREAK
#define TASK_BINDER_TG_WIFI_RECOVER_FAIL_STREAK 4u
#endif

class TaskBinder
{
public:
    using LoopCallback = void (*)(void *ctx);
    using FtestCallback = void (*)(void *ctx);

    TaskBinder(WifiManager &wifi, Extender &ext,
               Controllers &controllers, MeteoHistory &meteo_history,
               Display &display, PlcControl &plc, PlcScanLoop &plc_scan, Logger &logs)
        : _wifi(wifi),
          _ext(ext),
          _controllers(controllers),
          _meteo_history(meteo_history),
          _display(display),
          _plc(plc),
          _plc_scan(plc_scan),
          _logs(logs)
    {
    }

    void bindAll()
    {
        bindWiFiManager();
        bindGsm_();
        bindCloud_();
        bindExtender();
        bindPlcScan_();
        bindControllersStorage_();
        bindMeteoHistory_();
        bindNetworkLoop_();
        bindConsoleLoop_();
        bindDisplay_();
        bindPlc_();
    }

    template <typename FtestT>
    void bindFtest(FtestT &ftest)
    {
        _ftest_ctx = &ftest;
        _ftest_cb = [](void *ctx) {
            if (!ctx)
                return;
            static_cast<FtestT *>(ctx)->task();
        };
        if (_ftest_task == nullptr)
        {
            BaseType_t ok = xTaskCreatePinnedToCore(&TaskBinder::ftestTaskEntry_, "ftest", 4096, this, 1,
                                                    &_ftest_task, tskNO_AFFINITY);
            if (ok != pdPASS)
                _logs.error(F("TASK"), F("Bind failed: ftest"));
        }
        _ftest_enabled = false;
    }

    void enableFtest(bool enabled) { _ftest_enabled = enabled; }

    void setGsmModem(GsmModem &gsm) { _gsm = &gsm; }

    void setCloudClient(CloudClient &cloud) { _cloud = &cloud; }

    void setNetwork(Network &network) { _network = &network; }

    void setConsoleLoop(LoopCallback cb, void *ctx)
    {
        _console_loop_cb = cb;
        _console_loop_ctx = ctx;
    }

public:
    void bindRuntime(AppRuntime &stack)
    {
        _app_runtime = &stack;
        if (_runtime_phase_mtx == nullptr)
            _runtime_phase_mtx = xSemaphoreCreateMutex();
        if (_runtime_evt_queue == nullptr)
            _runtime_evt_queue = xQueueCreate(1, sizeof(uint8_t));
        if (_runtime_evt_task == nullptr)
        {
            BaseType_t ok = xTaskCreatePinnedToCore(&TaskBinder::runtimeEventTaskEntry_, "runtime_evt", 4096, this, 3,
                                                    &_runtime_evt_task, tskNO_AFFINITY);
            if (ok != pdPASS)
                _logs.error(F("TASK"), F("Bind failed: runtime_evt"));
        }
    }

public:
    void runRuntimePre(AppRuntime &stack)
    {
        _app_runtime = &stack;
        if (_runtime_phase_mtx)
            xSemaphoreTake(_runtime_phase_mtx, portMAX_DELAY);
        stack.setTaskPhase(AppRuntime::TaskPhase::PreNetwork);
        stack.taskPre();
        stack.setTaskPhase(AppRuntime::TaskPhase::Idle);
        if (_runtime_phase_mtx)
            xSemaphoreGive(_runtime_phase_mtx);
    }

private:
    void notifyRuntimePostNetwork()
    {
        if (_runtime_evt_queue == nullptr)
            return;
        bool expected = false;
        if (!_runtime_evt_pending.compare_exchange_strong(expected, true))
            return;
        uint8_t evt = 1;
        xQueueOverwrite(_runtime_evt_queue, &evt);
    }

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

    void bindControllersStorage_()
    {
        if (_control_task == nullptr)
        {
            BaseType_t ok = xTaskCreatePinnedToCore(&TaskBinder::controlTaskEntry_, "control_loop", 8192, this, 2,
                                                    &_control_task, tskNO_AFFINITY);
            if (ok != pdPASS)
                _logs.error(F("TASK"), F("Bind failed: control_loop"));
        }
    }

    void bindPlcScan_()
    {
        if (_plc_scan_task == nullptr)
        {
            BaseType_t ok = xTaskCreatePinnedToCore(&TaskBinder::plcScanTaskEntry_, "plc_scan", 3072, this, 4,
                                                    &_plc_scan_task, tskNO_AFFINITY);
            if (ok != pdPASS)
                _logs.error(F("TASK"), F("Bind failed: plc_scan"));
        }
    }

    void bindWiFiManager()
    {
        if (_wifi_task == nullptr)
        {
            BaseType_t ok = xTaskCreatePinnedToCore(&TaskBinder::wifiTaskEntry_, "wifi_mgr", 3072, this, 2, &_wifi_task,
                                                    tskNO_AFFINITY);
            if (ok != pdPASS)
                _logs.error(F("TASK"), F("Bind failed: wifi"));
        }
    }

    void bindExtender()
    {
        if (_ext_task_rtos == nullptr)
        {
            BaseType_t ok = xTaskCreatePinnedToCore(&TaskBinder::extenderTaskEntry_, "extender", 3072, this, 1,
                                                    &_ext_task_rtos, tskNO_AFFINITY);
            if (ok != pdPASS)
                _logs.error(F("TASK"), F("Bind failed: extender"));
        }
    }

    void bindMeteoHistory_()
    {
        if (_meteo_history_task == nullptr)
        {
            BaseType_t ok = xTaskCreatePinnedToCore(&TaskBinder::meteoHistoryTaskEntry_, "meteo_hist", 4096, this, 1,
                                                    &_meteo_history_task, tskNO_AFFINITY);
            if (ok != pdPASS)
                _logs.error(F("TASK"), F("Bind failed: meteo_history"));
        }
    }

    void bindGsm_()
    {
        if (!_gsm)
            return;
        if (_gsm_task_rtos == nullptr)
        {
            BaseType_t ok = xTaskCreatePinnedToCore(&TaskBinder::gsmTaskEntry_, "gsm_modem", 4096, this, 3,
                                                    &_gsm_task_rtos, tskNO_AFFINITY);
            if (ok != pdPASS)
                _logs.error(F("TASK"), F("Bind failed: gsm_modem"));
        }
    }

    void bindCloud_()
    {
        if (!_cloud)
            return;
        if (_cloud_task_rtos == nullptr)
        {
            BaseType_t ok = xTaskCreatePinnedToCore(&TaskBinder::cloudTaskEntry_, "cloud_cli", 8192, this, 3,
                                                    &_cloud_task_rtos, tskNO_AFFINITY);
            if (ok != pdPASS)
                _logs.error(F("TASK"), F("Bind failed: cloud_client"));
        }
    }

    void bindNetworkLoop_()
    {
        if (_network_task_rtos == nullptr)
        {
            BaseType_t ok = xTaskCreatePinnedToCore(&TaskBinder::networkTaskEntry_, "network_loop", 4096, this, 3,
                                                    &_network_task_rtos, tskNO_AFFINITY);
            if (ok != pdPASS)
                _logs.error(F("TASK"), F("Bind failed: network_loop"));
        }
    }

    void bindConsoleLoop_()
    {
        if (_console_task_rtos == nullptr)
        {
            BaseType_t ok = xTaskCreatePinnedToCore(&TaskBinder::consoleTaskEntry_, "console_loop", 4096, this, 1,
                                                    &_console_task_rtos, tskNO_AFFINITY);
            if (ok != pdPASS)
                _logs.error(F("TASK"), F("Bind failed: console_loop"));
        }
    }

    void bindDisplay_()
    {
        if (_display_task_rtos == nullptr)
        {
            BaseType_t ok = xTaskCreatePinnedToCore(&TaskBinder::displayTaskEntry_, "display", 3072, this, 1,
                                                    &_display_task_rtos, tskNO_AFFINITY);
            if (ok != pdPASS)
                _logs.error(F("TASK"), F("Bind failed: display"));
        }
    }

    void bindPlc_()
    {
        if (_plc_task_rtos == nullptr)
        {
            BaseType_t ok = xTaskCreatePinnedToCore(&TaskBinder::plcTaskEntry_, "plc", 4096, this, 2,
                                                    &_plc_task_rtos, tskNO_AFFINITY);
            if (ok != pdPASS)
                _logs.error(F("TASK"), F("Bind failed: plc"));
        }
    }

    void plcScanTask_()
    {
        _plc_scan.tick();
    }

    void networkLoopTask_()
    {
        if (_network)
            _network->loop();
    }

    void consoleLoopTask_()
    {
        if (_console_loop_cb)
            _console_loop_cb(_console_loop_ctx);
    }

    void ftestTask_()
    {
        if (_ftest_enabled && _ftest_cb)
            _ftest_cb(_ftest_ctx);
    }

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
            self->_controllers.sockets().task();
            self->_controllers.task();
            self->_controllers.meteo().task();
            self->_controllers.tanks().task();
            self->_controllers.septic().task();
            self->_controllers.security().task();
            self->_controllers.watering().task();
            self->_controllers.avr().task();
            self->_controllers.leak().task();
            self->_controllers.thermo().task();
            self->_controllers.ring().task();
#if TASK_BINDER_RTOS_DEBUG
            const uint32_t dt = (uint32_t)(micros() - t0);
            const UBaseType_t hwm = uxTaskGetStackHighWaterMark(nullptr);
            self->updateRtosDebug_("control", dt, hwm, self->_dbg_control);
#endif
            vTaskDelayUntil(&last, pdMS_TO_TICKS(50));
        }
    }

    static void plcScanTaskEntry_(void *arg)
    {
        auto *self = static_cast<TaskBinder *>(arg);
        TickType_t last = xTaskGetTickCount();
        for (;;)
        {
            const uint32_t t0 = micros();
            self->plcScanTask_();
#if TASK_BINDER_RTOS_DEBUG
            const uint32_t dt = (uint32_t)(micros() - t0);
            const UBaseType_t hwm = uxTaskGetStackHighWaterMark(nullptr);
            self->updateRtosDebug_("plc_scan", dt, hwm, self->_dbg_plc_scan);
#endif
            vTaskDelayUntil(&last, pdMS_TO_TICKS(TASK_BINDER_PLC_SCAN_TICK_MS));
        }
    }

    static void extenderTaskEntry_(void *arg)
    {
        auto *self = static_cast<TaskBinder *>(arg);
        TickType_t last = xTaskGetTickCount();
        for (;;)
        {
            const uint32_t t0 = micros();
            self->_ext.task();
#if TASK_BINDER_RTOS_DEBUG
            const uint32_t dt = (uint32_t)(micros() - t0);
            const UBaseType_t hwm = uxTaskGetStackHighWaterMark(nullptr);
            self->updateRtosDebug_("extender", dt, hwm, self->_dbg_ext);
#endif
            vTaskDelayUntil(&last, pdMS_TO_TICKS(50));
        }
    }

    static void displayTaskEntry_(void *arg)
    {
        auto *self = static_cast<TaskBinder *>(arg);
        TickType_t last = xTaskGetTickCount();
        for (;;)
        {
            const uint32_t t0 = micros();
            self->_display.task();
#if TASK_BINDER_RTOS_DEBUG
            const uint32_t dt = (uint32_t)(micros() - t0);
            const UBaseType_t hwm = uxTaskGetStackHighWaterMark(nullptr);
            self->updateRtosDebug_("display", dt, hwm, self->_dbg_display);
#endif
            vTaskDelayUntil(&last, pdMS_TO_TICKS(250));
        }
    }

    static void plcTaskEntry_(void *arg)
    {
        auto *self = static_cast<TaskBinder *>(arg);
        TickType_t last = xTaskGetTickCount();
        for (;;)
        {
            const uint32_t t0 = micros();
            self->_plc.task();
#if TASK_BINDER_RTOS_DEBUG
            const uint32_t dt = (uint32_t)(micros() - t0);
            const UBaseType_t hwm = uxTaskGetStackHighWaterMark(nullptr);
            self->updateRtosDebug_("plc", dt, hwm, self->_dbg_plc);
#endif
            vTaskDelayUntil(&last, pdMS_TO_TICKS(100));
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

    static void networkTaskEntry_(void *arg)
    {
        auto *self = static_cast<TaskBinder *>(arg);
        TickType_t last = xTaskGetTickCount();
        for (;;)
        {
            const uint32_t t0 = micros();
            self->networkLoopTask_();
            self->notifyRuntimePostNetwork();
#if TASK_BINDER_RTOS_DEBUG
            const uint32_t dt = (uint32_t)(micros() - t0);
            const UBaseType_t hwm = uxTaskGetStackHighWaterMark(nullptr);
            self->updateRtosDebug_("network", dt, hwm, self->_dbg_network);
#endif
            vTaskDelayUntil(&last, pdMS_TO_TICKS(TASK_BINDER_NETWORK_LOOP_TICK_MS));
        }
    }

    static void consoleTaskEntry_(void *arg)
    {
        auto *self = static_cast<TaskBinder *>(arg);
        TickType_t last = xTaskGetTickCount();
        for (;;)
        {
            const uint32_t t0 = micros();
            self->consoleLoopTask_();
#if TASK_BINDER_RTOS_DEBUG
            const uint32_t dt = (uint32_t)(micros() - t0);
            const UBaseType_t hwm = uxTaskGetStackHighWaterMark(nullptr);
            self->updateRtosDebug_("console", dt, hwm, self->_dbg_console);
#endif
            vTaskDelayUntil(&last, pdMS_TO_TICKS(TASK_BINDER_CONSOLE_LOOP_TICK_MS));
        }
    }

    static void ftestTaskEntry_(void *arg)
    {
        auto *self = static_cast<TaskBinder *>(arg);
        TickType_t last = xTaskGetTickCount();
        for (;;)
        {
            const uint32_t t0 = micros();
            self->ftestTask_();
#if TASK_BINDER_RTOS_DEBUG
            const uint32_t dt = (uint32_t)(micros() - t0);
            const UBaseType_t hwm = uxTaskGetStackHighWaterMark(nullptr);
            self->updateRtosDebug_("ftest", dt, hwm, self->_dbg_ftest);
#endif
            vTaskDelayUntil(&last, pdMS_TO_TICKS(500));
        }
    }

    static void runtimeEventTaskEntry_(void *arg)
    {
        auto *self = static_cast<TaskBinder *>(arg);
        for (;;)
        {
            uint8_t evt = 0;
            if (self->_runtime_evt_queue == nullptr)
            {
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }
            if (xQueueReceive(self->_runtime_evt_queue, &evt, portMAX_DELAY) != pdTRUE)
                continue;
            if (evt != 1 || self->_app_runtime == nullptr)
                continue;
            self->_runtime_evt_pending.store(false);

            const uint32_t t0 = micros();
            if (self->_runtime_phase_mtx)
                xSemaphoreTake(self->_runtime_phase_mtx, portMAX_DELAY);
            self->_app_runtime->setTaskPhase(AppRuntime::TaskPhase::PostNetwork);
            self->_app_runtime->taskPost();
            self->_app_runtime->taskFlush();
            self->_app_runtime->setTaskPhase(AppRuntime::TaskPhase::Idle);
            if (self->_runtime_phase_mtx)
                xSemaphoreGive(self->_runtime_phase_mtx);
#if TASK_BINDER_RTOS_DEBUG
            const uint32_t dt = (uint32_t)(micros() - t0);
            const UBaseType_t hwm = uxTaskGetStackHighWaterMark(nullptr);
            self->updateRtosDebug_("runtime_evt", dt, hwm, self->_dbg_runtime_evt);
#endif
        }
    }

    WifiManager &_wifi;
    Extender &_ext;
    Controllers &_controllers;
    MeteoHistory &_meteo_history;
    Display &_display;
    PlcControl &_plc;
    PlcScanLoop &_plc_scan;
    Logger &_logs;

    GsmModem *_gsm = nullptr;
    CloudClient *_cloud = nullptr;
    Network *_network = nullptr;
    LoopCallback _console_loop_cb = nullptr;
    void *_console_loop_ctx = nullptr;
    FtestCallback _ftest_cb = nullptr;
    void *_ftest_ctx = nullptr;
    volatile bool _ftest_enabled = false;
    TaskHandle_t _wifi_task = nullptr;
    TaskHandle_t _meteo_history_task = nullptr;
    TaskHandle_t _control_task = nullptr;
    TaskHandle_t _plc_scan_task = nullptr;
    TaskHandle_t _ext_task_rtos = nullptr;
    TaskHandle_t _display_task_rtos = nullptr;
    TaskHandle_t _plc_task_rtos = nullptr;
    TaskHandle_t _gsm_task_rtos = nullptr;
    TaskHandle_t _cloud_task_rtos = nullptr;
    TaskHandle_t _runtime_evt_task = nullptr;
    TaskHandle_t _network_task_rtos = nullptr;
    TaskHandle_t _console_task_rtos = nullptr;
    TaskHandle_t _ftest_task = nullptr;
    QueueHandle_t _runtime_evt_queue = nullptr;
    SemaphoreHandle_t _runtime_phase_mtx = nullptr;
    AppRuntime *_app_runtime = nullptr;
    std::atomic<bool> _runtime_evt_pending{false};

#if TASK_BINDER_RTOS_DEBUG
    RtosDebugStats _dbg_wifi{};
    RtosDebugStats _dbg_meteo_history{};
    RtosDebugStats _dbg_control{};
    RtosDebugStats _dbg_plc_scan{};
    RtosDebugStats _dbg_ext{};
    RtosDebugStats _dbg_display{};
    RtosDebugStats _dbg_plc{};
    RtosDebugStats _dbg_gsm{};
    RtosDebugStats _dbg_cloud{};
    RtosDebugStats _dbg_runtime_evt{};
    RtosDebugStats _dbg_network{};
    RtosDebugStats _dbg_console{};
    RtosDebugStats _dbg_ftest{};
#endif
};
