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

class WebInterface;
class AsyncWebServer;
class AsyncWebServerRequest;

class IndexHandler
{
public:
    static void registerRoutes(WebInterface &web, AsyncWebServer &server)
    {
        server.on("/", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleIndex(web, request); });
    }

    static void handleIndex(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        String page = FPSTR(kWebInterfaceIndexHtml);
        page.reserve(page.length() + 2048);
        page.replace("%NAV%", web.navHtml_());
        page.replace("%DEVICE_NAME%", web.deviceName_());
        page.replace("%DEVICE_STATUS%", web._device_status);
        const bool can_edit_device = web.webSessionIsAdmin_();
        page.replace("%DEVICE_BLOCK_STYLE%", can_edit_device ? "" : "display:none;");
        page.replace("%DEVICE_NAME_DISABLED%", can_edit_device ? "" : "disabled");
        page.replace("%DEVICE_SAVE_DISABLED%", can_edit_device ? "" : "disabled");
        const auto role = web.stackRole_();
        page.replace("%STACK_ROLE%", web.stackRoleName_(role));
        page.replace("%STACK_ROLE_MASTER_SEL%", role == ConfigsManagerIface::StackRole::Master ? "selected" : "");
        page.replace("%STACK_ROLE_SLAVE_SEL%", role == ConfigsManagerIface::StackRole::Slave ? "selected" : "");
        page.replace("%STACK_MASTER_HOST%", web.stackMasterHost_());
        page.replace("%STACK_API_KEY%", web.stackApiKey_());
        page.replace("%STACK_STATUS%", web._stack_status);
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        const bool stack_view = node_id != 0 && web._stack_master &&
                                web.stackRole_() == ConfigsManagerIface::StackRole::Master;
        page.replace("%INDEX_DEVICE_SELECT%", web.indexDeviceSelectHtml_(node_id, stack_view));
        String status_device_name = web.deviceName_();
        String rtc_date = web.rtcDateStr_();
        String rtc_time = web.rtcTimeOnlyStr_();
        String rtc_temp = web.formatTemp_(web.rtcTemp_());
        String board_temp = web.formatTemp_(web.boardTemp_());
        String cpu_temp = web.formatTemp_(web.cpuTemp_());
        String fan_icon = web.fanStatusIcon_();
        if (stack_view)
        {
            rtc_date = "n/a";
            rtc_time = "n/a";
            rtc_temp = "n/a";
            board_temp = "n/a";
            cpu_temp = "n/a";
            fan_icon = "n/a";
            if (web._stack_cache)
            {
                web._stack_cache->requestPlcStatus(node_id);
                web._stack_cache->requestRtcStatus(node_id);
                const auto *cache = web._stack_cache->statusCache(node_id);
                if (cache)
                {
                    const bool has_rtc = cache->has_rtc && cache->last_rtc_ok;
                    const bool has_plc = cache->has_plc && cache->last_plc_ok;
                    rtc_date = has_rtc ? cache->rtc_date : "n/a";
                    rtc_time = has_rtc ? cache->rtc_time : "n/a";
                    rtc_temp = has_rtc ? web.formatTemp_(cache->rtc_temp) : "n/a";
                    board_temp = has_plc ? web.formatTemp_(cache->board_temp) : "n/a";
                    cpu_temp = has_plc ? web.formatTemp_(cache->cpu_temp) : "n/a";
                    fan_icon = has_plc ? web.fanStatusIcon_(cache->fan_on) : "n/a";
                }
            }
            else
            {
                web.requestStackPlcStatus_(node_id);
                web.requestStackRtcStatus_(node_id);
                const auto *cache = web.findStackNodeStatusCache_(node_id, false);
                if (cache)
                {
                    const bool has_rtc = cache->has_rtc && cache->last_rtc_ok;
                    const bool has_plc = cache->has_plc && cache->last_plc_ok;
                    rtc_date = has_rtc ? cache->rtc_date : "n/a";
                    rtc_time = has_rtc ? cache->rtc_time : "n/a";
                    rtc_temp = has_rtc ? web.formatTemp_(cache->rtc_temp) : "n/a";
                    board_temp = has_plc ? web.formatTemp_(cache->board_temp) : "n/a";
                    cpu_temp = has_plc ? web.formatTemp_(cache->cpu_temp) : "n/a";
                    fan_icon = has_plc ? web.fanStatusIcon_(cache->fan_on) : "n/a";
                }
            }
            if (rtc_date == "" || rtc_time == "")
            {
                rtc_date = "n/a";
                rtc_time = "n/a";
            }
            if (web._stack_master)
            {
                const size_t count = web._stack_master->nodeCount();
                for (size_t i = 0; i < count; ++i)
                {
                    const uint32_t id = web._stack_master->nodeIdAt(i);
                    if (id != node_id)
                        continue;
                    String name = web._stack_master->nodeNameAt(i);
                    if (name.length() > 0)
                        status_device_name = name;
                    else
                        status_device_name = web.stackNodeIdHex_(id);
                    break;
                }
            }
        }
        page.replace("%STATUS_DEVICE_NAME%", status_device_name);
        page.replace("%BOARD_TEMP%", board_temp);
        page.replace("%CPU_TEMP%", cpu_temp);
        page.replace("%RTC_DATE%", rtc_date);
        page.replace("%RTC_TIME%", rtc_time);
        page.replace("%RTC_TEMP%", rtc_temp);
        page.replace("%FAN_STATUS_ICON%", fan_icon);
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        if (role == ConfigsManagerIface::StackRole::Master && web._stack_master)
        {
            const size_t count = web._stack_master->nodeCount();
            for (size_t i = 0; i < count; ++i)
            {
                const uint32_t id = web._stack_master->nodeIdAt(i);
                if (id == 0)
                    continue;
                if (web._stack_cache)
                {
                    web._stack_cache->requestPlcStatus(id);
                    web._stack_cache->requestRtcStatus(id);
                }
                else
                {
                    web.requestStackPlcStatus_(id);
                    web.requestStackRtcStatus_(id);
                }
            }
        }
        web.sendHtml_(request, page, set_cookie);
    }
};
