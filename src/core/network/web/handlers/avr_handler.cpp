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

#include "core/network/web/handlers/avr_handler.hpp"

#include "core/network/web/web_interface.hpp"

void AvrHandler::registerRoutes(WebInterface &web, AsyncWebServer &server) {
        server.on("/avr", HTTP_POST, [&web](AsyncWebServerRequest *request) { handleAvrSave(web, request); });
        server.on("/avr", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleAvr(web, request); });
    }

void AvrHandler::handleAvr(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        if (!web.requireWebAclController_(request, &set_cookie, UsersRegistry::AclController::Avr, node_id))
            return;
        if (!web.webAclCanViewItem_(UsersRegistry::AclController::Avr, 1, node_id))
        {
            web.sendText_(request, 403, "text/plain", "ACL deny", set_cookie);
            return;
        }
        const bool stack_view = isStackAvrView_(web, node_id);
        if (stack_view && web._stack_cache)
            web.stackCache().requestAvr(node_id);
        String page = FPSTR(kWebInterfaceAvrHtml);
        page.reserve(page.length() + 2048);
        page.replace("%NAV%", web.navHtml_());
        page.replace("%AVR_PAGE_TITLE%", WebUiRu::Avr::kPageTitle);
        page.replace("%AVR_CHK_ENABLED%", WebUiRu::Avr::kChkEnabled);
        page.replace("%AVR_CHK_AUTO_MODE%", WebUiRu::Avr::kChkAutoMode);
        page.replace("%AVR_CHK_PREFER_MAIN%", WebUiRu::Avr::kChkPreferMain);
        page.replace("%AVR_CHK_AUTO_RETURN_MAIN%", WebUiRu::Avr::kChkAutoReturnMain);
        page.replace("%AVR_GROUP_MANUAL_SOURCE%", WebUiRu::Avr::kGroupManualSource);
        page.replace("%AVR_LABEL_SOURCE%", WebUiRu::Avr::kLabelSource);
        page.replace("%AVR_RADIO_OFF%", WebUiRu::Avr::kRadioOff);
        page.replace("%AVR_RADIO_MAIN%", WebUiRu::Avr::kRadioMain);
        page.replace("%AVR_RADIO_RESERVE%", WebUiRu::Avr::kRadioReserve);
        page.replace("%AVR_GROUP_PORTS%", WebUiRu::Avr::kGroupPorts);
        page.replace("%AVR_PORT_MAIN_OK%", WebUiRu::Avr::kPortMainOk);
        page.replace("%AVR_PORT_FEEDBACK_MAIN%", WebUiRu::Avr::kPortFeedbackMain);
        page.replace("%AVR_PORT_RELAY_MAIN%", WebUiRu::Avr::kPortRelayMain);
        page.replace("%AVR_PORT_RESERVE_OK%", WebUiRu::Avr::kPortReserveOk);
        page.replace("%AVR_PORT_FEEDBACK_RESERVE%", WebUiRu::Avr::kPortFeedbackReserve);
        page.replace("%AVR_PORT_RELAY_RESERVE%", WebUiRu::Avr::kPortRelayReserve);
        page.replace("%AVR_GROUP_PORT_LOGIC%", WebUiRu::Avr::kGroupPortLogic);
        page.replace("%AVR_AL_MAIN_OK%", WebUiRu::Avr::kAlMainOk);
        page.replace("%AVR_AL_RESERVE_OK%", WebUiRu::Avr::kAlReserveOk);
        page.replace("%AVR_AL_FEEDBACK_MAIN%", WebUiRu::Avr::kAlFeedbackMain);
        page.replace("%AVR_AL_FEEDBACK_RESERVE%", WebUiRu::Avr::kAlFeedbackReserve);
        page.replace("%AVR_INV_RELAY_MAIN%", WebUiRu::Avr::kInvRelayMain);
        page.replace("%AVR_INV_RELAY_RESERVE%", WebUiRu::Avr::kInvRelayReserve);
        page.replace("%AVR_GROUP_TIMINGS%", WebUiRu::Avr::kGroupTimings);
        page.replace("%AVR_TIMING_DEBOUNCE%", WebUiRu::Avr::kTimingDebounce);
        page.replace("%AVR_TIMING_LOSS_DELAY%", WebUiRu::Avr::kTimingLossDelay);
        page.replace("%AVR_TIMING_RETURN_DELAY%", WebUiRu::Avr::kTimingReturnDelay);
        page.replace("%AVR_TIMING_BREAK%", WebUiRu::Avr::kTimingBreak);
        page.replace("%AVR_TIMING_WARMUP%", WebUiRu::Avr::kTimingWarmup);
        page.replace("%AVR_TIMING_TRANSFER_TIMEOUT%", WebUiRu::Avr::kTimingTransferTimeout);
        page.replace("%AVR_PORTS_HELP%", WebUiRu::Avr::kPortsHelp);
        page.replace("%AVR_FORM_ACTION%", avrRedirectPath_(node_id, stack_view));
        page.replace("%AVR_FAULT_FORM_ACTION%", avrRedirectPath_(node_id, stack_view));
        page.replace("%AVR_DEVICE_SELECT%", avrDeviceSelectHtml_(web, node_id, stack_view));
        page.replace("%AVR_SAVE_BTN%",
                     web.webSessionIsAdmin_() ? (String("<button type=\"submit\" form=\"avr-form\">") + WebUiRu::kSave + "</button>") : String(""));
        page.replace("%AVR_FAULT_BTN%", String("<button type=\"submit\" form=\"avr-fault-form\" class=\"btn-muted\">") + WebUiRu::Avr::kBtnResetFault + "</button>");

        if (!web._controllers)
        {
            page.replace("%AVR_STATUS%", WebUiRu::Common::kControllersUnavailable);
            page.replace("%AVR_STATE_TEXT%", buildStateIndicatorsUnknown_());
            fillDefaults_(page);
            web.sendHtml_(request, page, set_cookie);
            return;
        }

        if (stack_view)
        {
            if (!web._stack_cache)
            {
                page.replace("%AVR_STATUS%", WebUiRu::Avr::kStackCacheUnavailable);
                page.replace("%AVR_STATE_TEXT%", buildStateIndicatorsUnknown_());
                fillDefaults_(page);
            }
            else
            {
                const auto *cache = web.stackCache().avrCache(node_id);
                if (!cache || !cache->has_data)
                {
                    page.replace("%AVR_STATUS%", WebUiRu::Avr::kWaitingSlave);
                    page.replace("%AVR_STATE_TEXT%", buildStateIndicatorsUnknown_());
                    fillDefaults_(page);
                }
                else
                {
                    page.replace("%AVR_STATUS%", stackAvrStatusText_(web, node_id));
                    page.replace("%AVR_STATE_TEXT%", buildStateIndicators_(cache));
                    page.replace("%AVR_ENABLED_CHECKED%", cache->enabled ? "checked" : "");
                    page.replace("%AVR_AUTO_MODE_CHECKED%", cache->auto_mode ? "checked" : "");
                    page.replace("%AVR_PREFER_MAIN_CHECKED%", cache->prefer_main ? "checked" : "");
                    page.replace("%AVR_AUTO_RETURN_MAIN_CHECKED%", cache->auto_return_main ? "checked" : "");
                    page.replace("%AVR_MANUAL_OFF_SELECTED%", "checked");
                    page.replace("%AVR_MANUAL_MAIN_SELECTED%", "");
                    page.replace("%AVR_MANUAL_RESERVE_SELECTED%", "");
                    page.replace("%AVR_MAIN_OK_SELECTED%", portValue_(cache->main_ok_port));
                    page.replace("%AVR_RESERVE_OK_SELECTED%", portValue_(cache->reserve_ok_port));
                    page.replace("%AVR_RELAY_MAIN_SELECTED%", portValue_(cache->relay_main_port));
                    page.replace("%AVR_RELAY_RESERVE_SELECTED%", portValue_(cache->relay_reserve_port));
                    page.replace("%AVR_FB_MAIN_SELECTED%", portValue_(cache->feedback_main_port));
                    page.replace("%AVR_FB_RESERVE_SELECTED%", portValue_(cache->feedback_reserve_port));
                    page.replace("%AVR_MAIN_OK_AL_CHECKED%", "");
                    page.replace("%AVR_RESERVE_OK_AL_CHECKED%", "");
                    page.replace("%AVR_FB_MAIN_AL_CHECKED%", "");
                    page.replace("%AVR_FB_RESERVE_AL_CHECKED%", "");
                    page.replace("%AVR_RELAY_MAIN_INV_CHECKED%", "");
                    page.replace("%AVR_RELAY_RESERVE_INV_CHECKED%", "");
                    page.replace("%AVR_DEBOUNCE_MS%", "500");
                    page.replace("%AVR_LOSS_DELAY_MS%", "1500");
                    page.replace("%AVR_RETURN_DELAY_MS%", "5000");
                    page.replace("%AVR_BREAK_MS%", "250");
                    page.replace("%AVR_WARMUP_MS%", "1500");
                    page.replace("%AVR_TRANSFER_TIMEOUT_MS%", "15000");
                }
            }
            page.replace("%AVR_DINPUT_JSON%", web.socketPortOptionsJson_(PortIO::PinType::DInput));
            page.replace("%AVR_RELAY_JSON%", web.socketPortOptionsJson_(PortIO::PinType::Relay));
            page.replace("%AVR_DINPUT_USED_JSON%", "[]");
            page.replace("%AVR_RELAY_USED_JSON%", "[]");
        }
        else
        {
            AvrController &avr = web._controllers->avr();
            const AvrController::Config &cfg = avr.config();
            const AvrController::State &st = avr.state();

            page.replace("%AVR_STATUS%", web._avr_status);
            page.replace("%AVR_STATE_TEXT%", buildStateIndicators_(st));

            page.replace("%AVR_ENABLED_CHECKED%", cfg.enabled ? "checked" : "");
            page.replace("%AVR_AUTO_MODE_CHECKED%", cfg.auto_mode ? "checked" : "");
            page.replace("%AVR_PREFER_MAIN_CHECKED%", cfg.prefer_main ? "checked" : "");
            page.replace("%AVR_AUTO_RETURN_MAIN_CHECKED%", cfg.auto_return_main ? "checked" : "");

            page.replace("%AVR_MANUAL_OFF_SELECTED%", st.manual_source == AvrController::Source::Off ? "checked" : "");
            page.replace("%AVR_MANUAL_MAIN_SELECTED%", st.manual_source == AvrController::Source::Main ? "checked" : "");
            page.replace("%AVR_MANUAL_RESERVE_SELECTED%",
                         st.manual_source == AvrController::Source::Reserve ? "checked" : "");

            page.replace("%AVR_MAIN_OK_SELECTED%", portValue_(cfg.main_ok_port));
            page.replace("%AVR_RESERVE_OK_SELECTED%", portValue_(cfg.reserve_ok_port));
            page.replace("%AVR_RELAY_MAIN_SELECTED%", portValue_(cfg.relay_main_port));
            page.replace("%AVR_RELAY_RESERVE_SELECTED%", portValue_(cfg.relay_reserve_port));
            page.replace("%AVR_FB_MAIN_SELECTED%", portValue_(cfg.feedback_main_port));
            page.replace("%AVR_FB_RESERVE_SELECTED%", portValue_(cfg.feedback_reserve_port));

            page.replace("%AVR_MAIN_OK_AL_CHECKED%", cfg.main_ok_active_low ? "checked" : "");
            page.replace("%AVR_RESERVE_OK_AL_CHECKED%", cfg.reserve_ok_active_low ? "checked" : "");
            page.replace("%AVR_FB_MAIN_AL_CHECKED%", cfg.feedback_main_active_low ? "checked" : "");
            page.replace("%AVR_FB_RESERVE_AL_CHECKED%", cfg.feedback_reserve_active_low ? "checked" : "");
            page.replace("%AVR_RELAY_MAIN_INV_CHECKED%", cfg.relay_main_invert ? "checked" : "");
            page.replace("%AVR_RELAY_RESERVE_INV_CHECKED%", cfg.relay_reserve_invert ? "checked" : "");

            page.replace("%AVR_DEBOUNCE_MS%", String((unsigned long)cfg.debounce_ms));
            page.replace("%AVR_LOSS_DELAY_MS%", String((unsigned long)cfg.loss_delay_ms));
            page.replace("%AVR_RETURN_DELAY_MS%", String((unsigned long)cfg.return_delay_ms));
            page.replace("%AVR_BREAK_MS%", String((unsigned long)cfg.break_ms));
            page.replace("%AVR_WARMUP_MS%", String((unsigned long)cfg.warmup_ms));
            page.replace("%AVR_TRANSFER_TIMEOUT_MS%", String((unsigned long)cfg.transfer_timeout_ms));

            page.replace("%AVR_DINPUT_JSON%", web.socketPortOptionsJson_(PortIO::PinType::DInput));
            page.replace("%AVR_RELAY_JSON%", web.socketPortOptionsJson_(PortIO::PinType::Relay));
            page.replace("%AVR_DINPUT_USED_JSON%", web.globalUsedPortsJson_(PortIO::PinType::DInput));
            page.replace("%AVR_RELAY_USED_JSON%", web.globalUsedPortsJson_(PortIO::PinType::Relay));
        }
        web.sendHtml_(request, page, set_cookie);
    }

void AvrHandler::handleAvrSave(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (!web.requireWebAdmin_(request, &set_cookie))
            return;
        if (!web.requireWebAclController_(request, &set_cookie, UsersRegistry::AclController::Avr))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        const bool stack_view = isStackAvrView_(web, node_id);
        if (!web._controllers)
        {
            web.sendText_(request, 500, "text/plain", WebUiRu::Common::kControllersUnavailable, set_cookie);
            return;
        }
        if (!web.webAclCanControlItem_(UsersRegistry::AclController::Avr, 1, node_id))
        {
            web._avr_status = "ACL deny";
            web.sendRedirect_(request, avrRedirectPath_(node_id, stack_view), set_cookie);
            return;
        }

        AvrController &avr = web._controllers->avr();
        if (request->hasParam("avr_clear_fault", true))
        {
            if (stack_view)
            {
                if (sendStackAvrSet_(web, node_id, nullptr, true))
                    web._avr_status = WebUiRu::Avr::kCmdSent;
                else
                    web._avr_status = WebUiRu::Avr::kSendFailed;
            }
            else
            {
                avr.clearFault();
                web._avr_status = WebUiRu::Avr::kFaultCleared;
            }
            web.sendRedirect_(request, avrRedirectPath_(node_id, stack_view), set_cookie);
            return;
        }
        if (!request->hasParam("avr_save", true))
        {
            web.sendRedirect_(request, avrRedirectPath_(node_id, stack_view), set_cookie);
            return;
        }

        const AvrController::Config &cfg = avr.config();
        bool ok = true;
        String err;

        const bool enabled = request->hasParam("avr_enabled", true);
        const bool auto_mode = request->hasParam("avr_auto_mode", true);
        const bool prefer_main = request->hasParam("avr_prefer_main", true);
        const bool auto_return_main = request->hasParam("avr_auto_return_main", true);
        const bool main_ok_al = request->hasParam("avr_main_ok_active_low", true);
        const bool reserve_ok_al = request->hasParam("avr_reserve_ok_active_low", true);
        const bool fb_main_al = request->hasParam("avr_feedback_main_active_low", true);
        const bool fb_reserve_al = request->hasParam("avr_feedback_reserve_active_low", true);
        const bool relay_main_inv = request->hasParam("avr_relay_main_invert", true);
        const bool relay_reserve_inv = request->hasParam("avr_relay_reserve_invert", true);

        uint8_t main_ok = cfg.main_ok_port;
        uint8_t reserve_ok = cfg.reserve_ok_port;
        uint8_t relay_main = cfg.relay_main_port;
        uint8_t relay_reserve = cfg.relay_reserve_port;
        uint8_t fb_main = cfg.feedback_main_port;
        uint8_t fb_reserve = cfg.feedback_reserve_port;
        uint32_t debounce_ms = cfg.debounce_ms;
        uint32_t loss_delay_ms = cfg.loss_delay_ms;
        uint32_t return_delay_ms = cfg.return_delay_ms;
        uint32_t break_ms = cfg.break_ms;
        uint32_t warmup_ms = cfg.warmup_ms;
        uint32_t transfer_timeout_ms = cfg.transfer_timeout_ms;

        if (!web.parseSocketPort_(web.paramValue_(request, "avr_main_ok"), main_ok))
        {
            ok = false;
            err = WebUiRu::Avr::kInvalidMainOkPort;
        }
        if (ok && !web.parseSocketPort_(web.paramValue_(request, "avr_reserve_ok"), reserve_ok))
        {
            ok = false;
            err = WebUiRu::Avr::kInvalidReserveOkPort;
        }
        if (ok && !web.parseSocketPort_(web.paramValue_(request, "avr_relay_main"), relay_main))
        {
            ok = false;
            err = WebUiRu::Avr::kInvalidRelayMainPort;
        }
        if (ok && !web.parseSocketPort_(web.paramValue_(request, "avr_relay_reserve"), relay_reserve))
        {
            ok = false;
            err = WebUiRu::Avr::kInvalidRelayReservePort;
        }
        if (ok && !web.parseSocketPort_(web.paramValue_(request, "avr_feedback_main"), fb_main))
        {
            ok = false;
            err = WebUiRu::Avr::kInvalidFeedbackMainPort;
        }
        if (ok && !web.parseSocketPort_(web.paramValue_(request, "avr_feedback_reserve"), fb_reserve))
        {
            ok = false;
            err = WebUiRu::Avr::kInvalidFeedbackReservePort;
        }
        if (ok && !parseMs_(web, request, "avr_debounce_ms", debounce_ms))
        {
            ok = false;
            err = WebUiRu::Avr::kInvalidDebounceMs;
        }
        if (ok && !parseMs_(web, request, "avr_loss_delay_ms", loss_delay_ms))
        {
            ok = false;
            err = WebUiRu::Avr::kInvalidLossDelayMs;
        }
        if (ok && !parseMs_(web, request, "avr_return_delay_ms", return_delay_ms))
        {
            ok = false;
            err = WebUiRu::Avr::kInvalidReturnDelayMs;
        }
        if (ok && !parseMs_(web, request, "avr_break_ms", break_ms))
        {
            ok = false;
            err = WebUiRu::Avr::kInvalidBreakMs;
        }
        if (ok && !parseMs_(web, request, "avr_warmup_ms", warmup_ms))
        {
            ok = false;
            err = WebUiRu::Avr::kInvalidWarmupMs;
        }
        if (ok && !parseMs_(web, request, "avr_transfer_timeout_ms", transfer_timeout_ms))
        {
            ok = false;
            err = WebUiRu::Avr::kInvalidTransferTimeoutMs;
        }

        AvrController::Source manual = avr.manualSource();
        if (ok && !parseSource_(web.paramValue_(request, "avr_manual_source"), manual))
        {
            ok = false;
            err = WebUiRu::Avr::kInvalidManualSource;
        }

        if (!ok)
        {
            web._avr_status = err;
            web.sendRedirect_(request, avrRedirectPath_(node_id, stack_view), set_cookie);
            return;
        }

        const bool cfg_changed =
            cfg.enabled != enabled ||
            cfg.auto_mode != auto_mode ||
            cfg.prefer_main != prefer_main ||
            cfg.auto_return_main != auto_return_main ||
            cfg.main_ok_port != main_ok ||
            cfg.reserve_ok_port != reserve_ok ||
            cfg.relay_main_port != relay_main ||
            cfg.relay_reserve_port != relay_reserve ||
            cfg.feedback_main_port != fb_main ||
            cfg.feedback_reserve_port != fb_reserve ||
            cfg.main_ok_active_low != main_ok_al ||
            cfg.reserve_ok_active_low != reserve_ok_al ||
            cfg.feedback_main_active_low != fb_main_al ||
            cfg.feedback_reserve_active_low != fb_reserve_al ||
            cfg.relay_main_invert != relay_main_inv ||
            cfg.relay_reserve_invert != relay_reserve_inv ||
            cfg.debounce_ms != debounce_ms ||
            cfg.loss_delay_ms != loss_delay_ms ||
            cfg.return_delay_ms != return_delay_ms ||
            cfg.break_ms != break_ms ||
            cfg.warmup_ms != warmup_ms ||
            cfg.transfer_timeout_ms != transfer_timeout_ms;

        if (stack_view)
        {
            DynamicJsonDocument doc(1024);
            JsonObject obj = doc.to<JsonObject>();
            obj["enabled"] = enabled;
            obj["auto_mode"] = auto_mode;
            obj["prefer_main"] = prefer_main;
            obj["auto_return_main"] = auto_return_main;
            if (main_ok != AvrController::kInvalidPort)
                obj["main_ok"] = main_ok;
            if (reserve_ok != AvrController::kInvalidPort)
                obj["reserve_ok"] = reserve_ok;
            if (relay_main != AvrController::kInvalidPort)
                obj["relay_main"] = relay_main;
            if (relay_reserve != AvrController::kInvalidPort)
                obj["relay_reserve"] = relay_reserve;
            if (fb_main != AvrController::kInvalidPort)
                obj["feedback_main"] = fb_main;
            if (fb_reserve != AvrController::kInvalidPort)
                obj["feedback_reserve"] = fb_reserve;
            obj["main_ok_active_low"] = main_ok_al;
            obj["reserve_ok_active_low"] = reserve_ok_al;
            obj["feedback_main_active_low"] = fb_main_al;
            obj["feedback_reserve_active_low"] = fb_reserve_al;
            obj["relay_main_invert"] = relay_main_inv;
            obj["relay_reserve_invert"] = relay_reserve_inv;
            obj["debounce_ms"] = debounce_ms;
            obj["loss_delay_ms"] = loss_delay_ms;
            obj["return_delay_ms"] = return_delay_ms;
            obj["break_ms"] = break_ms;
            obj["warmup_ms"] = warmup_ms;
            obj["transfer_timeout_ms"] = transfer_timeout_ms;
            obj["manual_source"] = manualSourceName_(manual);
            if (sendStackAvrSet_(web, node_id, &obj, false))
                web._avr_status = WebUiRu::Avr::kCmdSent;
            else
                web._avr_status = WebUiRu::Avr::kSendFailed;
            web.sendRedirect_(request, avrRedirectPath_(node_id, true), set_cookie);
            return;
        }

        if (cfg_changed)
        {
            DynamicJsonDocument doc(1024);
            JsonObject obj = doc.to<JsonObject>();
            obj["enabled"] = enabled;
            obj["auto_mode"] = auto_mode;
            obj["prefer_main"] = prefer_main;
            obj["auto_return_main"] = auto_return_main;
            if (main_ok != AvrController::kInvalidPort)
                obj["main_ok"] = main_ok;
            if (reserve_ok != AvrController::kInvalidPort)
                obj["reserve_ok"] = reserve_ok;
            if (relay_main != AvrController::kInvalidPort)
                obj["relay_main"] = relay_main;
            if (relay_reserve != AvrController::kInvalidPort)
                obj["relay_reserve"] = relay_reserve;
            if (fb_main != AvrController::kInvalidPort)
                obj["feedback_main"] = fb_main;
            if (fb_reserve != AvrController::kInvalidPort)
                obj["feedback_reserve"] = fb_reserve;
            obj["main_ok_active_low"] = main_ok_al;
            obj["reserve_ok_active_low"] = reserve_ok_al;
            obj["feedback_main_active_low"] = fb_main_al;
            obj["feedback_reserve_active_low"] = fb_reserve_al;
            obj["relay_main_invert"] = relay_main_inv;
            obj["relay_reserve_invert"] = relay_reserve_inv;
            obj["debounce_ms"] = debounce_ms;
            obj["loss_delay_ms"] = loss_delay_ms;
            obj["return_delay_ms"] = return_delay_ms;
            obj["break_ms"] = break_ms;
            obj["warmup_ms"] = warmup_ms;
            obj["transfer_timeout_ms"] = transfer_timeout_ms;
            avr.applyConfig(doc.as<JsonObjectConst>());
        }

        const bool manual_changed = (avr.manualSource() != manual);
        if (manual_changed)
            avr.setManualSource(manual);

        bool saved = true;
        if (cfg_changed)
        {
            if (!web._configs_manager)
            {
                saved = false;
                web._avr_status = WebUiRu::Common::kConfigManagerUnavailable;
            }
            else if (!web._configs_manager->save())
            {
                saved = false;
                web._avr_status = WebUiRu::Common::kSaveFailed;
            }
        }

        if (saved)
            web._avr_status = (cfg_changed || manual_changed) ? WebUiRu::Common::kUpdated : WebUiRu::Common::kNoChanges;
        web.sendRedirect_(request, avrRedirectPath_(node_id, false), set_cookie);
    }

bool AvrHandler::isStackAvrView_(WebInterface &web, uint32_t node_id) {
        return node_id != 0 && web._stack_master &&
               web.stackRole_() == ConfigsManagerIface::StackRole::Master;
    }

String AvrHandler::avrRedirectPath_(uint32_t node_id, bool stack_view) {
        if (!stack_view)
            return "/avr";
        String path = "/avr?node=";
        path += String((unsigned long)node_id);
        path += "&unit=stack";
        return path;
    }

const char * AvrHandler::manualSourceName_(AvrController::Source src) {
        switch (src)
        {
        case AvrController::Source::Main:
            return "main";
        case AvrController::Source::Reserve:
            return "reserve";
        default:
            return "off";
        }
    }

String AvrHandler::stackAvrStatusText_(WebInterface &web, uint32_t node_id) {
        if (!web._stack_cache)
            return WebUiRu::Avr::kStackCacheUnavailable;
        const auto *cache = web.stackCache().avrCache(node_id);
        if (!cache)
            return WebUiRu::Common::kNoDataFromSlave;
        if (cache->pending)
            return WebUiRu::Avr::kRequestingSlaveData;
        if (!cache->last_ok && cache->last_error.length())
        {
            String msg = WebUiRu::Common::kErrorPrefix;
            msg += cache->last_error;
            return msg;
        }
        if (!cache->has_data)
            return WebUiRu::Common::kNoDataFromSlave;
        return WebUiRu::Common::kStatusOk;
    }

String AvrHandler::avrDeviceSelectHtml_(WebInterface &web, uint32_t selected_node_id, bool stack_view) {
        if (web.stackRole_() != ConfigsManagerIface::StackRole::Master || !web._stack_master)
            return "";
        String html;
        html.reserve(512);
        html += "<div class=\"row\" style=\"margin-bottom:10px;\">";
        html += "<label>";
        html += WebUiRu::Common::kDevice;
        html += "</label>";
        html += "<select id=\"avr-device\" class=\"field\">";
        html += "<option value=\"local\"";
        if (!stack_view)
            html += " selected";
        html += ">local</option>";
        const size_t count = web._stack_master->nodeCount();
        for (size_t i = 0; i < count; ++i)
        {
            const uint32_t id = web._stack_master->nodeIdAt(i);
            html += "<option value=\"";
            html += String((unsigned long)id);
            html += "\"";
            if (stack_view && id == selected_node_id)
                html += " selected";
            html += ">";
            String name = web._stack_master->nodeNameAt(i);
            if (name.length() > 0)
                web.appendHtmlEscaped_(html, name.c_str());
            else
                html += web.stackNodeIdHex_(id);
            html += "</option>";
        }
        html += "</select></div>";
        return html;
    }

bool AvrHandler::sendStackAvrSet_(WebInterface &web, uint32_t node_id, JsonObject *cfg, bool clear_fault) {
        if (!web._stack_master || node_id == 0)
            return false;
        StaticJsonDocument<1536> doc;
        doc["cmd_id"] = web.nextStackCmdId_();
        doc["feature"] = (uint8_t)StackFeature::Avr;
        doc["action"] = "set";
        JsonObject params = doc["params"].to<JsonObject>();
        if (cfg)
        {
            for (JsonPair kv : *cfg)
                params[kv.key()] = kv.value();
        }
        if (clear_fault)
            params["clear_fault"] = true;
        char payload[1400] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        return web._stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                                         (const uint8_t *)payload, len);
    }

void AvrHandler::fillDefaults_(String &page) {
        page.replace("%AVR_ENABLED_CHECKED%", "");
        page.replace("%AVR_AUTO_MODE_CHECKED%", "");
        page.replace("%AVR_PREFER_MAIN_CHECKED%", "");
        page.replace("%AVR_AUTO_RETURN_MAIN_CHECKED%", "");
        page.replace("%AVR_MANUAL_OFF_SELECTED%", "checked");
        page.replace("%AVR_MANUAL_MAIN_SELECTED%", "");
        page.replace("%AVR_MANUAL_RESERVE_SELECTED%", "");
        page.replace("%AVR_MAIN_OK_SELECTED%", "");
        page.replace("%AVR_RESERVE_OK_SELECTED%", "");
        page.replace("%AVR_RELAY_MAIN_SELECTED%", "");
        page.replace("%AVR_RELAY_RESERVE_SELECTED%", "");
        page.replace("%AVR_FB_MAIN_SELECTED%", "");
        page.replace("%AVR_FB_RESERVE_SELECTED%", "");
        page.replace("%AVR_MAIN_OK_AL_CHECKED%", "");
        page.replace("%AVR_RESERVE_OK_AL_CHECKED%", "");
        page.replace("%AVR_FB_MAIN_AL_CHECKED%", "");
        page.replace("%AVR_FB_RESERVE_AL_CHECKED%", "");
        page.replace("%AVR_RELAY_MAIN_INV_CHECKED%", "");
        page.replace("%AVR_RELAY_RESERVE_INV_CHECKED%", "");
        page.replace("%AVR_DEBOUNCE_MS%", "500");
        page.replace("%AVR_LOSS_DELAY_MS%", "1500");
        page.replace("%AVR_RETURN_DELAY_MS%", "5000");
        page.replace("%AVR_BREAK_MS%", "250");
        page.replace("%AVR_WARMUP_MS%", "1500");
        page.replace("%AVR_TRANSFER_TIMEOUT_MS%", "15000");
        page.replace("%AVR_DINPUT_JSON%", "[]");
        page.replace("%AVR_RELAY_JSON%", "[]");
        page.replace("%AVR_DINPUT_USED_JSON%", "[]");
        page.replace("%AVR_RELAY_USED_JSON%", "[]");
    }

String AvrHandler::portValue_(uint8_t port) {
        if (port == AvrController::kInvalidPort)
            return String();
        return String((unsigned)port);
    }

bool AvrHandler::parseMs_(WebInterface &web, AsyncWebServerRequest *request, const char *name, uint32_t &out) {
        const String value = web.paramValue_(request, name);
        if (value.length() == 0)
            return false;
        return web.parseUint_(value, out);
    }

bool AvrHandler::parseSource_(const String &value, AvrController::Source &out) {
        String t = value;
        t.trim();
        t.toLowerCase();
        if (t == "off" || t.length() == 0)
        {
            out = AvrController::Source::Off;
            return true;
        }
        if (t == "main")
        {
            out = AvrController::Source::Main;
            return true;
        }
        if (t == "reserve")
        {
            out = AvrController::Source::Reserve;
            return true;
        }
        return false;
    }

String AvrHandler::boolTxt_(bool value) {
        return value ? "1" : "0";
    }

String AvrHandler::buildStateIndicatorsItem_(const char *label, bool on, bool error) {
        String out;
        out.reserve(128);
        out += "<span class=\"state-item\"><span class=\"state-dot";
        if (on)
            out += error ? " err-on" : " net-on";
        out += "\"></span>";
        out += label;
        out += "</span>";
        return out;
    }

String AvrHandler::buildStateIndicatorsUnknown_() {
        String out;
        out.reserve(256);
        out += "<div class=\"state-indicators\">";
        out += buildStateIndicatorsItem_(WebUiRu::Avr::kStateMain, false, false);
        out += buildStateIndicatorsItem_(WebUiRu::Avr::kStateReserve, false, false);
        out += buildStateIndicatorsItem_(WebUiRu::Avr::kStateFault, false, true);
        out += "</div>";
        return out;
    }

String AvrHandler::buildStateIndicators_(const AvrController::State &st) {
        String out;
        out.reserve(256);
        out += "<div class=\"state-indicators\">";
        out += buildStateIndicatorsItem_(WebUiRu::Avr::kStateMain, st.main_ok, false);
        out += buildStateIndicatorsItem_(WebUiRu::Avr::kStateReserve, st.reserve_ok, false);
        out += buildStateIndicatorsItem_(WebUiRu::Avr::kStateFault, st.fault != AvrController::Fault::None, true);
        out += "</div>";
        return out;
    }

String AvrHandler::buildStateIndicators_(const StackCache::StackAvrCache *st) {
        if (!st)
            return buildStateIndicatorsUnknown_();
        String fault = st->fault;
        fault.trim();
        fault.toLowerCase();
        const bool has_fault = fault.length() > 0 && fault != "none";
        String out;
        out.reserve(256);
        out += "<div class=\"state-indicators\">";
        out += buildStateIndicatorsItem_(WebUiRu::Avr::kStateMain, st->main_ok, false);
        out += buildStateIndicatorsItem_(WebUiRu::Avr::kStateReserve, st->reserve_ok, false);
        out += buildStateIndicatorsItem_(WebUiRu::Avr::kStateFault, has_fault, true);
        out += "</div>";
        return out;
    }

String AvrHandler::buildStateText_(const AvrController::State &st) {
        String out;
        out.reserve(192);
        out += WebUiRu::Avr::kActive;
        out += AvrController::sourceName(st.active_source);
        out += WebUiRu::Avr::kTarget;
        out += AvrController::sourceName(st.target_source);
        out += WebUiRu::Avr::kFault;
        out += AvrController::faultName(st.fault);
        out += WebUiRu::Avr::kSwitching;
        out += boolTxt_(st.transfer_in_progress);
        out += " main_ok:";
        out += boolTxt_(st.main_ok);
        out += " reserve_ok:";
        out += boolTxt_(st.reserve_ok);
        out += " relay_main:";
        out += boolTxt_(st.relay_main_on);
        out += " relay_reserve:";
        out += boolTxt_(st.relay_reserve_on);
        return out;
    }

String AvrHandler::buildStateText_(const StackCache::StackAvrCache *st) {
        if (!st)
            return String("n/a");
        String out;
        out.reserve(192);
        out += WebUiRu::Avr::kActive;
        out += st->active_source;
        out += WebUiRu::Avr::kTarget;
        out += st->target_source;
        out += WebUiRu::Avr::kFault;
        out += st->fault;
        out += WebUiRu::Avr::kSwitching;
        out += boolTxt_(st->transfer);
        out += " main_ok:";
        out += boolTxt_(st->main_ok);
        out += " reserve_ok:";
        out += boolTxt_(st->reserve_ok);
        out += " relay_main:";
        out += boolTxt_(st->relay_main_on);
        out += " relay_reserve:";
        out += boolTxt_(st->relay_reserve_on);
        return out;
    }
