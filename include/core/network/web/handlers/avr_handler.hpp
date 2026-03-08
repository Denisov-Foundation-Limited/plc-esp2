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
#include "core/network/web/interfaces/web_interface_handler_base.hpp"

#include <ArduinoJson.h>

#include "controllers/avr_controller.hpp"
#include "core/network/stack/stack_cache.hpp"

class AvrHandler
{
public:
    static void registerRoutes(WebInterface &web, AsyncWebServer &server);

    static void handleAvr(WebInterface &web, AsyncWebServerRequest *request);

    static void handleAvrSave(WebInterface &web, AsyncWebServerRequest *request);

private:
    static bool isStackAvrView_(WebInterface &web, uint32_t node_id);

    static String avrRedirectPath_(uint32_t node_id, bool stack_view);

    static const char *manualSourceName_(AvrController::Source src);

    static String stackAvrStatusText_(WebInterface &web, uint32_t node_id);

    static String avrDeviceSelectHtml_(WebInterface &web, uint32_t selected_node_id, bool stack_view);

    static bool sendStackAvrSet_(WebInterface &web, uint32_t node_id, JsonObject *cfg, bool clear_fault);

    static void fillDefaults_(String &page);

    static String portValue_(uint8_t port);

    static bool parseMs_(WebInterface &web, AsyncWebServerRequest *request, const char *name, uint32_t &out);

    static bool parseSource_(const String &value, AvrController::Source &out);

    static String boolTxt_(bool value);

    static String buildStateIndicatorsItem_(const char *label, bool on, bool error);

    static String buildStateIndicatorsUnknown_();

    static String buildStateIndicators_(const AvrController::State &st);

    static String buildStateIndicators_(const StackCache::StackAvrCache *st);

    static String buildStateText_(const AvrController::State &st);

    static String buildStateText_(const StackCache::StackAvrCache *st);
};
