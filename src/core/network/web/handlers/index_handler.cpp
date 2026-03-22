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

#include "core/network/web/handlers/index_handler.hpp"

#include <atomic>

#include "core/network/web/web_interface.hpp"

namespace
{
constexpr uint32_t kIndexMinIntervalMs = 350u;
constexpr uint32_t kIndexStateMinIntervalMs = 700u;
constexpr uint32_t kIndexLowHeapBytes = 20u * 1024u;
constexpr uint32_t kIndexPageCacheMs = 1500u;

std::atomic<uint32_t> g_last_index_request_ms{0};
std::atomic<uint32_t> g_last_index_state_request_ms{0};
std::atomic<bool> g_index_request_inflight{false};
std::atomic<bool> g_index_state_request_inflight{false};

String g_index_page_cache;
uint32_t g_index_page_cache_built_ms = 0;
uint32_t g_index_page_cache_node_id = 0;
bool g_index_page_cache_stack_view = false;
bool g_index_page_cache_can_edit = false;

bool requestTooFrequentIndex_(std::atomic<uint32_t> &stamp, uint32_t now_ms, uint32_t min_interval_ms)
{
    const uint32_t prev = stamp.load(std::memory_order_relaxed);
    if (prev != 0 && (uint32_t)(now_ms - prev) < min_interval_ms)
        return true;
    stamp.store(now_ms, std::memory_order_relaxed);
    return false;
}

bool canUseIndexPageCache_(uint32_t now_ms, uint32_t node_id, bool stack_view, bool can_edit)
{
    if (!g_index_page_cache.length())
        return false;
    if (g_index_page_cache_node_id != node_id || g_index_page_cache_stack_view != stack_view ||
        g_index_page_cache_can_edit != can_edit)
        return false;
    return (uint32_t)(now_ms - g_index_page_cache_built_ms) <= kIndexPageCacheMs;
}

bool isStackIndexView_(WebInterface &web, uint32_t node_id)
{
    if (node_id == 0 || !web.network() || web.network()->stackRole() != ConfigsManagerIface::StackRole::Master)
        return false;
    StackDeviceRegistry::DeviceInfo device{};
    return web.network()->stackDeviceSnapshotByNodeId(node_id, device) && device.online;
}
}

void IndexHandler::registerRoutes(WebInterface &web, AsyncWebServer &server) {
        server.on("/index/state", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleIndexState(web, request); });
        server.on("/", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleIndex(web, request); });
    }

void IndexHandler::handleIndex(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        const uint32_t now_ms = millis();
        const uint32_t free_heap = ESP.getFreeHeap();
        if (g_index_request_inflight.exchange(true, std::memory_order_acq_rel))
        {
            web.sendText_(request, 503, "text/plain", "WEB busy", set_cookie);
            return;
        }
        if (free_heap < kIndexLowHeapBytes ||
            requestTooFrequentIndex_(g_last_index_request_ms, now_ms, kIndexMinIntervalMs))
        {
            g_index_request_inflight.store(false, std::memory_order_release);
            web.sendText_(request, 503, "text/plain", "WEB busy", set_cookie);
            return;
        }
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        const bool stack_view = isStackIndexView_(web, node_id);
        const bool can_edit_device = web.webSessionIsAdmin_();
        String page;
        if (canUseIndexPageCache_(now_ms, node_id, stack_view, can_edit_device))
        {
            page = g_index_page_cache;
        }
        else
        {
            page = buildIndexPage_(web, node_id, stack_view, can_edit_device);
            g_index_page_cache = page;
            g_index_page_cache_built_ms = now_ms;
            g_index_page_cache_node_id = node_id;
            g_index_page_cache_stack_view = stack_view;
            g_index_page_cache_can_edit = can_edit_device;
        }
        g_index_request_inflight.store(false, std::memory_order_release);
        web.sendHtml_(request, page, set_cookie);
    }

void IndexHandler::handleIndexState(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuthApi_(request, &set_cookie))
            return;
        const uint32_t now_ms = millis();
        const uint32_t free_heap = ESP.getFreeHeap();
        if (g_index_state_request_inflight.exchange(true, std::memory_order_acq_rel))
        {
            web.sendText_(request, 503, "application/json",
                          "{\"busy\":true}",
                          set_cookie);
            return;
        }
        if (free_heap < kIndexLowHeapBytes ||
            requestTooFrequentIndex_(g_last_index_state_request_ms, now_ms, kIndexStateMinIntervalMs))
        {
            g_index_state_request_inflight.store(false, std::memory_order_release);
            web.sendText_(request, 503, "application/json",
                          "{\"busy\":true}",
                          set_cookie);
            return;
        }
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        const bool stack_view = isStackIndexView_(web, node_id);
        const IndexState st = collectIndexState_(web, node_id, stack_view);
        StaticJsonDocument<512> doc;
        doc["device_name"] = st.device_name;
        doc["rtc_date"] = st.rtc_date;
        doc["rtc_time"] = st.rtc_time;
        doc["rtc_temp"] = st.rtc_temp;
        doc["board_temp"] = st.board_temp;
        doc["fan_html"] = st.fan_html;
        String body;
        serializeJson(doc, body);
        g_index_state_request_inflight.store(false, std::memory_order_release);
        web.sendText_(request, 200, "application/json", body, set_cookie);
    }

String IndexHandler::buildIndexPage_(WebInterface &web, uint32_t node_id, bool stack_view, bool can_edit_device) {
        String page = FPSTR(kWebInterfaceIndexHtml);
        page.reserve(page.length() + 2048);
        page.replace("%NAV%", web.navHtml_());
        page.replace("%DEVICE_NAME%", web.deviceName_());
        page.replace("%DEVICE_STATUS%", web._device_status);
        page.replace("%DEVICE_BLOCK_STYLE%", can_edit_device ? "" : "display:none;");
        page.replace("%DEVICE_NAME_DISABLED%", can_edit_device ? "" : "disabled");
        page.replace("%DEVICE_SAVE_DISABLED%", can_edit_device ? "" : "disabled");
        page.replace("%SAVE_TEXT%", WebUiRu::kSave);
        page.replace("%INDEX_LABEL_BOARD%", WebUiRu::Index::kBoard);
        page.replace("%INDEX_LABEL_DEVICE_NAME%", WebUiRu::Index::kDeviceName);
        page.replace("%INDEX_LABEL_STATUS%", WebUiRu::Index::kStatus);
        page.replace("%INDEX_LABEL_DATE%", WebUiRu::Index::kDate);
        page.replace("%INDEX_LABEL_TIME%", WebUiRu::Index::kTime);
        page.replace("%INDEX_LABEL_RTC_TEMP%", WebUiRu::Index::kRtcTemp);
        page.replace("%INDEX_LABEL_BOARD_TEMP%", WebUiRu::Index::kBoardTemp);
        page.replace("%INDEX_LABEL_FAN%", WebUiRu::Index::kFan);
        page.replace("%STACK_ROLE%", "");
        page.replace("%STACK_ROLE_MASTER_SEL%", "");
        page.replace("%STACK_ROLE_SLAVE_SEL%", "");
        page.replace("%STACK_MASTER_HOST%", "");
        page.replace("%STACK_API_KEY%", "");
        page.replace("%STACK_STATUS%", "");
        page.replace("%INDEX_DEVICE_SELECT%", web.indexDeviceSelectHtml_(node_id, stack_view));
        const String loading = "...";
        page.replace("%STATUS_DEVICE_NAME%", loading);
        page.replace("%BOARD_TEMP%", loading);
        page.replace("%RTC_DATE%", loading);
        page.replace("%RTC_TIME%", loading);
        page.replace("%RTC_TEMP%", loading);
        page.replace("%FAN_STATUS_ICON%", loading);
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        return page;
    }

IndexHandler::IndexState IndexHandler::collectIndexState_(WebInterface &web, uint32_t node_id, bool stack_view) {
        IndexState out{};
        out.device_name = web.deviceName_();
        out.rtc_date = web.rtcDateStr_();
        out.rtc_time = web.rtcTimeOnlyStr_();
        out.rtc_temp = web.formatTemp_(web.rtcTemp_());
        out.board_temp = web.formatTemp_(web.boardTemp_());
        out.fan_html = web.fanStatusIcon_();

        if (!stack_view)
            return out;

        out.rtc_date = "n/a";
        out.rtc_time = "n/a";
        out.rtc_temp = "n/a";
        out.board_temp = "n/a";
        out.fan_html = "n/a";
        if (web.network())
        {
            web.requestStackIndexState_(node_id);
            StackUnitSnapshot::Snapshot cache{};
            if (web.network()->stackIndexStateSnapshot(node_id, cache))
            {
                out.rtc_date = cache.has_rtc ? String(cache.rtc_date) : "n/a";
                out.rtc_time = cache.has_rtc ? String(cache.rtc_time) : "n/a";
                out.rtc_temp = (cache.has_rtc && cache.rtc_temp_ok) ? web.formatTemp_(cache.rtc_temp) : "n/a";
                out.board_temp = cache.has_plc ? web.formatTemp_(cache.board_temp) : "n/a";
                out.fan_html = cache.has_plc ? web.fanStatusIcon_(cache.fan_on) : "n/a";
            }
        }
        if (out.rtc_date == "" || out.rtc_time == "")
        {
            out.rtc_date = "n/a";
            out.rtc_time = "n/a";
        }
        if (web.network())
        {
            StackDeviceRegistry::DeviceInfo device{};
            if (web.network()->stackDeviceSnapshotByNodeId(node_id, device))
                out.device_name = device.name[0] ? String(device.name) : web.stackNodeIdHex_(node_id);
        }
        return out;
    }
