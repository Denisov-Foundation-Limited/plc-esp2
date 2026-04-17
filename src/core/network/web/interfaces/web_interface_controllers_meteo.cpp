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

#include "core/network/web/web_interface.hpp"

namespace
{
void loadLocalMeteoItems_(MeteoController &meteo, WebInterface::ScratchBuffer &scratch, size_t count)
{
    if (count == 0)
        return;
    auto guard = meteo.lockGuard();
    static constexpr size_t kDs18Max = sizeof(scratch.meteo_ds18_list) / sizeof(scratch.meteo_ds18_list[0]);
    meteo.listDs18b20Serials(scratch.meteo_ds18_list, kDs18Max, scratch.meteo_ds18_count);
    for (size_t i = 0; i < count; ++i)
    {
        scratch.meteo_valid[i] = false;
        const auto *cfg = meteo.configByIndex(i);
        const auto *st = meteo.stateByIndex(i);
        if (!cfg || !st)
            continue;
        scratch.meteo_valid[i] = true;
        scratch.meteo_cfg[i] = *cfg;
        scratch.meteo_st[i] = *st;
    }
}

size_t localMeteoRenderCount_(const WebInterface::ScratchBuffer &scratch, size_t count, bool can_view_disabled)
{
    if (count == 0)
        return 0;
    if (!can_view_disabled)
        return count;
    size_t last_enabled_idx = SIZE_MAX;
    for (size_t i = 0; i < count; ++i)
    {
        if (scratch.meteo_valid[i] && scratch.meteo_cfg[i].enabled)
            last_enabled_idx = i;
    }
    if (last_enabled_idx == SIZE_MAX)
        return count ? 1u : 0u;
    const size_t render_count = last_enabled_idx + 2u;
    return render_count > count ? count : render_count;
}

bool stackMeteoState_(const WebInterface &web, uint32_t node_id, StackUnitSnapshot::State &out)
{
    return web.network() && node_id != 0 && web.network()->stackIndexState(node_id, out);
}

bool stackMeteoCacheState_(const WebInterface &web, uint32_t node_id, StackUnitSnapshot::CacheState &out)
{
    return web.network() && node_id != 0 && web.network()->stackIndexCacheState(node_id, out);
}

bool stackMeteoRequestState_(const WebInterface &web, uint32_t node_id, StackUnitSnapshot::RequestState &out)
{
    return web.network() && node_id != 0 && web.network()->stackIndexRequestState(node_id, out);
}

bool waitForStackMeteoCache_(WebInterface &web, uint32_t node_id, uint32_t timeout_ms = 700u)
{
    if (!web.network() || node_id == 0)
        return false;
    const uint32_t started_ms = millis();
    while ((uint32_t)(millis() - started_ms) < timeout_ms)
    {
        StackUnitSnapshot::State snapshot{};
        StackUnitSnapshot::CacheState cache{};
        if (web.network()->stackIndexState(node_id, snapshot) && web.network()->stackIndexCacheState(node_id, cache) &&
            cache.meteo_count > 0)
            return true;
        delay(25);
    }
    return false;
}

void ensureStackMeteoSnapshot_(const WebInterface &web, uint32_t node_id, const StackUnitSnapshot::State *snapshot = nullptr,
                               const StackUnitSnapshot::CacheState *cache = nullptr)
{
    if (!web.network() || node_id == 0)
        return;
    StackUnitSnapshot::State state{};
    StackUnitSnapshot::CacheState state_cache{};
    const StackUnitSnapshot::State &ref = snapshot ? *snapshot : state;
    const StackUnitSnapshot::CacheState &cache_ref = cache ? *cache : state_cache;
    if (!snapshot)
    {
        if (!web.network()->stackIndexState(node_id, state) || !web.network()->stackIndexCacheState(node_id, state_cache))
        {
            web.network()->stackRoute().sendRequestSelected(web.stackPayloadMode(), node_id, "controllers", "summary_req",
                                                            nullptr, true);
            DynamicJsonDocument req(64);
            req["offset"] = 0;
            req["limit"] = StackUnitSnapshot::kPageSize;
            web.network()->stackRoute().sendRequestSelected(web.stackPayloadMode(), node_id, "meteo", "snapshot_req",
                                                            &req, true);
            return;
        }
    }
    if (ref.updated_ms == 0)
    {
        DynamicJsonDocument req(64);
        req["offset"] = 0;
        req["limit"] = StackUnitSnapshot::kPageSize;
        web.network()->stackRoute().sendRequestSelected(web.stackPayloadMode(), node_id, "meteo", "snapshot_req", &req,
                                                        true);
        return;
    }
    if (ref.meteo_enabled > cache_ref.meteo_count &&
        web.network()->prepareStackPageRequest(StackUnitSnapshot::PageKind::Meteo, node_id, millis(),
                                               cache_ref.meteo_count, 4000u))
    {
        DynamicJsonDocument req(64);
        req["offset"] = cache_ref.meteo_count;
        req["limit"] = StackUnitSnapshot::kPageSize;
        if (!web.network()->stackRoute().sendRequestSelected(web.stackPayloadMode(), node_id, "meteo", "snapshot_req",
                                                             &req, true))
            web.network()->clearStackPageRequest(StackUnitSnapshot::PageKind::Meteo, node_id);
    }
}

bool requestNextStackMeteoPage_(WebInterface &web, uint32_t node_id, uint16_t offset, uint16_t limit)
{
    if (!web.network() || node_id == 0 || limit == 0)
        return false;
    DynamicJsonDocument req(64);
    req["offset"] = offset;
    req["limit"] = limit;
    const bool sent = web.network()->stackRoute().sendRequestSelected(web.stackPayloadMode(), node_id, "meteo",
                                                                      "snapshot_req", &req, true);
    if (!sent)
        web.network()->clearStackPageRequest(StackUnitSnapshot::PageKind::Meteo, node_id);
    return sent;
}

size_t stackMeteoRenderCount_(const WebInterface &web, uint32_t node_id, uint8_t loaded_count, bool can_view_disabled)
{
    if (!can_view_disabled || !web.network())
        return loaded_count;
    size_t last_enabled_idx = SIZE_MAX;
    web.network()->forEachStackMeteo(node_id, loaded_count, [&](uint8_t index, const StackUnitSnapshot::MeteoItem &item) {
        if (item.enabled)
            last_enabled_idx = index;
    });
    if (last_enabled_idx == SIZE_MAX)
        return loaded_count ? 1u : 0u;
    const size_t rc = last_enabled_idx + 2u;
    return rc > loaded_count ? loaded_count : rc;
}

String stackMeteoSensorLabel_(const WebInterface &web, uint32_t node_id, const StackUnitSnapshot::MeteoItem &item)
{
    (void)web;
    (void)node_id;
    String out;
    out.reserve(64);
    out += String((unsigned)item.id);
    if (item.name[0] != '\0')
    {
        out += ": ";
        out += item.name;
    }
    return out;
}
}

size_t WebInterfaceControllersMeteoHelper::meteoLocalRenderCount_(const WebInterface &web) {
        if (!web._controllers)
            return 0;
        MeteoController &meteo = web._controllers->meteo();
        auto guard = meteo.lockGuard();
        size_t last_enabled_idx = SIZE_MAX;
        for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
        {
            const auto *cfg = meteo.configByIndex(i);
            if (cfg && cfg->enabled)
                last_enabled_idx = i;
        }
        if (last_enabled_idx == SIZE_MAX)
            return MeteoController::kSensorCount ? 1u : 0u;
        const size_t count = last_enabled_idx + 2u;
        return count > MeteoController::kSensorCount ? MeteoController::kSensorCount : count;
    }

String WebInterfaceControllersMeteoHelper::meteoDeviceSelectHtml_(const WebInterface &web, uint32_t selected_node_id, bool stack_view) {
        if (!web.network() || web.network()->stackRole() != ConfigsManagerIface::StackRole::Master)
            return "";
        String html;
        html.reserve(512);
        html += "<div class=\"row\">";
        html += String("<span class=\"muted\">") + WebUiRu::kDevice + "</span>";
        html += "<select id=\"meteo-device\" class=\"field mini\">";
        html += "<option value=\"local\"";
        if (!stack_view)
            html += " selected";
        html += ">local</option>";
        const size_t count = web.network()->stackOnlineDeviceCount();
        for (size_t i = 0; i < count; ++i)
        {
            StackDeviceRegistry::DeviceInfo device{};
            if (!web.network()->stackDeviceSnapshotAt(i, device) || !device.online || device.node_id == 0)
                continue;
            const uint32_t id = device.node_id;
            html += "<option value=\"";
            html += String((unsigned long)id);
            html += "\"";
            if (stack_view && id == selected_node_id)
                html += " selected";
            html += ">";
            String name = device.name[0] ? String(device.name) : String();
            if (name.length() > 0)
                web.appendHtmlEscaped_(html, name.c_str());
            else
                html += web.stackNodeIdHex_(id);
            html += "</option>";
        }
        html += "</select></div>";
        return html;
    }

String WebInterfaceControllersMeteoHelper::stackMeteoStatusText_(const WebInterface &web, uint32_t node_id) {
            StackUnitSnapshot::State snapshot{};
            StackUnitSnapshot::RequestState request{};
            if (!stackMeteoState_(web, node_id, snapshot))
                return WebUiRu::kNoDataFromSlave;
            if (stackMeteoRequestState_(web, node_id, request) &&
                request.pending &&
                (uint32_t)(millis() - request.started_ms) > 15000u)
                return WebUiRu::Sockets::kText10;
            if (request.pending)
                return "";
            if (snapshot.updated_ms == 0)
                return WebUiRu::kNoDataFromSlave;
            return WebUiRu::kStatusOk;
    }

bool WebInterfaceControllersMeteoHelper::isStackMeteoView_(const WebInterface &web, uint32_t node_id) {
        if (node_id == 0 || !web.network() || web.network()->stackRole() != ConfigsManagerIface::StackRole::Master)
            return false;
        StackDeviceRegistry::DeviceInfo device{};
        return web.network()->stackDeviceSnapshotByNodeId(node_id, device) && device.online;
    }

bool WebInterfaceControllersMeteoHelper::requestStackMeteo_(WebInterface &web, uint32_t node_id) {
        if (!web.network() || node_id == 0)
            return false;
        const uint32_t now = millis();
        StackUnitSnapshot::State snapshot{};
        StackUnitSnapshot::CacheState cache{};
        StackUnitSnapshot::RequestState request{};
        const bool has_snapshot = web.network()->stackIndexState(node_id, snapshot);
        const bool has_cache = web.network()->stackIndexCacheState(node_id, cache);
        const bool has_request = web.network()->stackIndexRequestState(node_id, request);
        if (has_request && request.pending && (uint32_t)(now - request.started_ms) < 1500u)
            return true;
        if (!has_snapshot || snapshot.updated_ms == 0 ||
            (uint32_t)(now - snapshot.updated_ms) > 5000u ||
            !has_cache || cache.meteo_count == 0)
        {
            const bool refresh = web.requestStackIndexState_(node_id);
            web.network()->stackRoute().sendRequestSelected(web.stackPayloadMode(), node_id, "controllers", "summary_req",
                                                            nullptr, true);
            DynamicJsonDocument req(64);
            req["offset"] = 0;
            req["limit"] = StackUnitSnapshot::kPageSize;
            const bool meteo_req = web.network()->stackRoute().sendRequestSelected(web.stackPayloadMode(), node_id,
                                                                                   "meteo", "snapshot_req", &req, true);
            return refresh || meteo_req;
        }
        if (has_cache && snapshot.meteo_enabled > cache.meteo_count)
        {
            if (!web.network()->prepareStackPageRequest(StackUnitSnapshot::PageKind::Meteo, node_id, now,
                                                        cache.meteo_count, 4000u))
                return true;
            return requestNextStackMeteoPage_(web, node_id, cache.meteo_count, StackUnitSnapshot::kPageSize);
        }
        return true;
    }

size_t WebInterfaceControllersMeteoHelper::stackMeteoVisibleCount_(const WebInterface &web, uint32_t node_id) {
            StackUnitSnapshot::State snapshot{};
            StackUnitSnapshot::CacheState cache{};
            if (!stackMeteoState_(web, node_id, snapshot) || !stackMeteoCacheState_(web, node_id, cache))
            {
                const_cast<WebInterface &>(web).requestStackMeteo_(node_id);
                waitForStackMeteoCache_(const_cast<WebInterface &>(web), node_id);
            }
            if (!stackMeteoState_(web, node_id, snapshot) || !stackMeteoCacheState_(web, node_id, cache))
                return 0;
            if (cache.meteo_count == 0 && snapshot.meteo_enabled > 0)
            {
                const_cast<WebInterface &>(web).requestStackMeteo_(node_id);
                waitForStackMeteoCache_(const_cast<WebInterface &>(web), node_id);
                stackMeteoState_(web, node_id, snapshot);
                stackMeteoCacheState_(web, node_id, cache);
            }
            if (cache.meteo_count == 0)
                return 0;
            const bool can_view_disabled = web.webSessionIsAdmin_();
            const size_t render_count = stackMeteoRenderCount_(web, node_id, cache.meteo_count, can_view_disabled);
            size_t count = 0;
            web.network()->forEachStackMeteo(node_id, (uint8_t)render_count, [&](uint8_t, const StackUnitSnapshot::MeteoItem &item) {
                if (!web.webAclCanViewItem_(UsersRegistry::AclController::Meteo, item.id, node_id))
                    return;
                if (!can_view_disabled && !item.enabled)
                    return;
                ++count;
            });
            return count;
    }

String WebInterfaceControllersMeteoHelper::listStackMeteoHtml_(WebInterface &web, uint32_t node_id, size_t offset, size_t limit) {
            StackUnitSnapshot::State snapshot{};
            StackUnitSnapshot::CacheState cache{};
            if (!stackMeteoState_(web, node_id, snapshot) || !stackMeteoCacheState_(web, node_id, cache))
            {
                web.requestStackMeteo_(node_id);
                waitForStackMeteoCache_(web, node_id);
            }
            if (!stackMeteoState_(web, node_id, snapshot) || !stackMeteoCacheState_(web, node_id, cache))
                return WebUiRu::kNoDataFromSlave;
            if (cache.meteo_count == 0 && snapshot.meteo_enabled > 0)
            {
                web.requestStackMeteo_(node_id);
                waitForStackMeteoCache_(web, node_id);
                stackMeteoState_(web, node_id, snapshot);
                stackMeteoCacheState_(web, node_id, cache);
            }
            if (cache.meteo_count == 0)
                return WebUiRu::Meteo::kText2;
            String items;
            const size_t page_limit = (limit == 0) ? 1u
                                                   : ((limit == SIZE_MAX) ? cache.meteo_count : limit);
            size_t reserve = 2048u + page_limit * 700u;
            if (reserve < 8192u)
                reserve = 8192u;
            items.reserve(reserve);
            const bool can_view_disabled = web.webSessionIsAdmin_();
            char ds18_list[StackUnitSnapshot::kMeteoCount][17] = {};
            size_t ds18_count = 0;
            web.network()->forEachStackMeteo(node_id, cache.meteo_count, [&](uint8_t, const StackUnitSnapshot::MeteoItem &item) {
                if (!item.enabled || item.type != (uint8_t)MeteoController::SensorType::Ds18b20 || !item.ds18_addr_set)
                    return;
                char hex[17] = {};
                MeteoController::formatHexAddr(item.ds18_addr, hex);
                bool exists = false;
                for (size_t j = 0; j < ds18_count; ++j)
                {
                    if (strcmp(ds18_list[j], hex) == 0)
                    {
                        exists = true;
                        break;
                    }
                }
                if (!exists && ds18_count < StackUnitSnapshot::kMeteoCount)
                {
                    strncpy(ds18_list[ds18_count], hex, sizeof(ds18_list[ds18_count]) - 1);
                    ++ds18_count;
                }
            });
            const size_t render_count = stackMeteoRenderCount_(web, node_id, cache.meteo_count, can_view_disabled);
            size_t rendered = 0;
            size_t visible_idx = 0;
            web.network()->forEachStackMeteo(node_id, (uint8_t)render_count, [&](uint8_t, const StackUnitSnapshot::MeteoItem &cfg) {
                if (rendered >= page_limit)
                    return;
                if (!web.webAclCanViewItem_(UsersRegistry::AclController::Meteo, cfg.id, node_id))
                    return;
                if (!can_view_disabled && !cfg.enabled)
                    return;
                if (visible_idx < offset)
                {
                    ++visible_idx;
                    return;
                }
                ++visible_idx;
                const bool can_edit = web.webSessionIsAdmin_() &&
                                      web.webAclCanControlItem_(UsersRegistry::AclController::Meteo, cfg.id, node_id);
                char temp_buf[12] = {};
                char hum_buf[12] = {};
                char age_buf[16] = {};
                const char *temp = "--";
                const char *hum = "--";
                if (cfg.has_temp)
                {
                    dtostrf(cfg.temp_c, 0, 1, temp_buf);
                    temp = temp_buf;
                }
                if (cfg.has_humidity)
                {
                    dtostrf(cfg.humidity, 0, 1, hum_buf);
                    hum = hum_buf;
                }
                snprintf(age_buf, sizeof(age_buf), "%us", (unsigned)cfg.age_s);
                const bool show_hum = ((MeteoController::SensorType)cfg.type == MeteoController::SensorType::Dht22);
                String addr;
                if (cfg.ds18_addr_set)
                {
                    char hex[17] = {};
                    MeteoController::formatHexAddr(cfg.ds18_addr, hex);
                    addr = hex;
                }
                items += "<div class=\"tile js-group-item";
                if (!cfg.enabled)
                    items += " disabled";
                items += "\" data-group-id=\"";
                items += String((unsigned)cfg.group_id);
                items += "\"";
                items += web.groupVisibilityStyleAttr_(cfg.group_id, node_id);
                items += " data-sensor-id=\"";
                items += String((unsigned)cfg.id);
                items += "\">";
                items += "<div class=\"sensor-visual\">";
                items += "<span class=\"badge\">#";
                items += String((unsigned)cfg.id);
                items += "</span>";
                items += "<svg class=\"sensor-icon ";
                if (!cfg.ok)
                    items += "na";
                items += "\" viewBox=\"0 0 64 64\" aria-hidden=\"true\">";
                items += "<path fill=\"currentColor\" d=\"M32 6c-5.5 0-10 4.5-10 10v19.2c-2.6 2.4-4 5.7-4 9.3 0 7.2 5.8 13 13 13s13-5.8 13-13c0-3.6-1.4-6.9-4-9.3V16c0-5.5-4.5-10-10-10zm6 33.1V16c0-3.3-2.7-6-6-6s-6 2.7-6 6v23.1l-0.9 0.9c-1.8 1.7-2.8 3.9-2.8 6.4 0 4.9 4 9 9 9s9-4 9-9c0-2.5-1-4.8-2.8-6.4l-0.5-0.5z\"/>";
                items += "<rect x=\"30\" y=\"20\" width=\"4\" height=\"20\" rx=\"2\" fill=\"currentColor\"/>";
                items += "</svg><div class=\"sensor-readout\"><div class=\"sensor-value\"><span class=\"sensor-temp-value\">";
                items += temp;
                items += "</span></div><div class=\"sensor-unit\">°C</div>";
                if (show_hum)
                {
                    items += "<div class=\"sensor-hum\"><svg class=\"sensor-hum-icon\" viewBox=\"0 0 64 64\" aria-hidden=\"true\"><path fill=\"currentColor\" d=\"M32 6c7 12 16 22 16 34 0 8.8-7.2 16-16 16S16 48.8 16 40c0-12 9-22 16-34z\"/></svg><div class=\"sensor-value sensor-hum-value\">";
                    items += hum;
                    items += "</div><div class=\"sensor-unit\">%</div></div>";
                }
                items += "</div></div><div><div class=\"tile-head\"><strong>";
                items += WebUiRu::Meteo::kTitlePrefix;
                items += String((unsigned)cfg.id);
                items += "</strong><label class=\"switch\"><input type=\"checkbox\" class=\"meteo-enable\" name=\"m";
                items += String((unsigned)cfg.id);
                items += "_en\"";
                if (cfg.enabled)
                    items += " checked";
                if (!can_edit)
                    items += " disabled";
                items += "><span class=\"track\"><span class=\"knob\"></span></span></label></div>";
                items += WebUiRu::Meteo::kText12;
                items += web.meteoRemoteNodeOptionsHtml_(cfg.source_node_id);
                items += "</select></div>";
                items += WebUiRu::Meteo::kInputClassFieldNameMeteoNameType;
                items += String((unsigned)cfg.id);
                items += "_name\" value=\"";
                web.appendHtmlEscaped_(items, cfg.name);
                items += "\"";
                if (!can_edit)
                    items += " readonly";
                items += "></div>";
                items += String("<div class=\"form-row\" style=\"margin-top:8px;margin-bottom:8px\"><label>") + WebUiRu::GroupsPage::kLabel + "</label><select class=\"field\" name=\"m";
                items += String((unsigned)cfg.id);
                items += "_group\"";
                if (!can_edit || !web.hasGroups_(node_id))
                    items += " disabled";
                items += ">";
                items += web.groupOptionsHtml_(cfg.group_id, true, true, node_id);
                items += "</select></div>";
                items += WebUiRu::Meteo::kSelectClassFieldNameMeteoSourceName;
                items += String((unsigned)cfg.id);
                items += "_src\">";
                items += web.meteoRemoteSensorOptionsHtml_(cfg.source_sensor_id, cfg.source_node_id);
                items += "</select></div><div class=\"form-grid\">";
                items += WebUiRu::Meteo::kSelectClassFieldMeteoTypeNameM;
                items += String((unsigned)cfg.id);
                items += "_type\"";
                if (!can_edit)
                    items += " disabled";
                items += ">";
                const MeteoController::SensorType type = (MeteoController::SensorType)cfg.type;
                items += String("<option value=\"none\"") + (type == MeteoController::SensorType::None ? " selected" : "") + ">none</option>";
                items += String("<option value=\"ds18b20\"") + (type == MeteoController::SensorType::Ds18b20 ? " selected" : "") + ">ds18b20</option>";
                items += String("<option value=\"dht22\"") + (type == MeteoController::SensorType::Dht22 ? " selected" : "") + ">dht22</option>";
                items += "</select></div>";
                items += WebUiRu::Meteo::kSelectClassFieldMiniMeteoPinData;
                if (cfg.dht_pin != MeteoController::kInvalidPin)
                    items += String((unsigned)cfg.dht_pin);
                items += "\" name=\"m";
                items += String((unsigned)cfg.id);
                items += "_pin\">";
                if (cfg.dht_pin != MeteoController::kInvalidPin)
                {
                    items += "<option value=\"";
                    items += String((unsigned)cfg.dht_pin);
                    items += "\" selected>";
                    items += String((unsigned)cfg.dht_pin);
                    items += "</option>";
                }
                items += "</select></div>";
                items += WebUiRu::Meteo::kSelectClassFieldAddrMeteoAddrName;
                items += String((unsigned)cfg.id);
                items += "_addr\"";
                if (!can_edit)
                    items += " disabled";
                items += "><option value=\"\">-</option>";
                bool addr_found = false;
                for (size_t j = 0; j < ds18_count; ++j)
                {
                    items += "<option value=\"";
                    items += ds18_list[j];
                    items += "\"";
                    if (addr.length() && addr == ds18_list[j])
                    {
                        items += " selected";
                        addr_found = true;
                    }
                    items += ">";
                    items += ds18_list[j];
                    items += "</option>";
                }
                if (addr.length() && !addr_found)
                {
                    items += "<option value=\"";
                    items += addr;
                    items += "\" selected>";
                    items += addr;
                    items += "</option>";
                }
                items += "</select></div></div><div class=\"status-line\">";
                items += cfg.ok ? "<span class=\"status-dot sensor-status-dot status-ok\" title=\"OK\"></span>"
                                : "<span class=\"status-dot sensor-status-dot status-err\" title=\"ERR\"></span>";
                items += "<span class=\"meteo-age-text\">";
                items += WebUiRu::Meteo::kText13;
                items += cfg.has_read ? age_buf : "-";
                items += "</span></div></div></div>";
                ++rendered;
            });
            if (items.length() == 0)
                items = WebUiRu::Meteo::kText2;
            return items;
    }

String WebInterfaceControllersMeteoHelper::listMeteoHtml_(WebInterface &web, size_t offset, size_t limit) {
        if (!web._controllers)
            return WebUiRu::Meteo::kText11;
        String items;
        items.reserve(16384);
        auto scratch_guard = web.scratchLockGuard_();
        WebInterface::ScratchBuffer *scratch = (scratch_guard.locked() ? web.scratchBuffer_() : nullptr);
        if (!scratch)
            return WebUiRu::Meteo::kText11;
        MeteoController &meteo = web._controllers->meteo();
        const uint32_t now = millis();
        loadLocalMeteoItems_(meteo, *scratch, MeteoController::kSensorCount);
        memset(scratch->meteo_ds18_used, 0, sizeof(scratch->meteo_ds18_used));
        size_t ds18_used_count = 0;
        for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
        {
            if (!scratch->meteo_valid[i])
                continue;
            const auto &cfg = scratch->meteo_cfg[i];
            if (cfg.type != MeteoController::SensorType::Ds18b20 || !cfg.ds18_addr_set)
                continue;
            char hex[17] = {};
            MeteoController::formatHexAddr(cfg.ds18_addr, hex);
            bool exists = false;
            for (size_t j = 0; j < ds18_used_count; ++j)
            {
                if (strcmp(scratch->meteo_ds18_used[j], hex) == 0)
                {
                    exists = true;
                    break;
                }
            }
            if (!exists && ds18_used_count < MeteoController::kSensorCount)
            {
                strncpy(scratch->meteo_ds18_used[ds18_used_count], hex, sizeof(scratch->meteo_ds18_used[ds18_used_count]) - 1);
                ++ds18_used_count;
            }
        }
    
        auto appendTypeOption = [&](const char *value, const char *label, bool selected) {
            items += "<option value=\"";
            items += value;
            items += "\"";
            if (selected)
                items += " selected";
            items += ">";
            items += label;
            items += "</option>";
        };
    
        auto appendRow = [&](const MeteoController::SensorConfig &cfg, const MeteoController::SensorState &st,
                             bool enabled) {
            const bool can_edit = web.webSessionIsAdmin_() &&
                                  web.webAclCanControlItem_(UsersRegistry::AclController::Meteo, cfg.id);
            const bool has_read = st.last_read_ms != 0;
            char temp_buf[12] = {};
            char hum_buf[12] = {};
            char age_buf[16] = {};
            const char *temp = "--";
            const char *hum = "--";
            String ok = "<span class=\"status-dot sensor-status-dot status-na\" title=\"N/A\"></span>";
            const char *age = "-";
    
            if (st.has_temp)
            {
                dtostrf(st.temp_c, 0, 1, temp_buf);
                temp = temp_buf;
            }
            if (st.has_humidity)
            {
                dtostrf(st.humidity, 0, 1, hum_buf);
                hum = hum_buf;
            }
            if (has_read)
            {
                ok = st.ok ? "<span class=\"status-dot sensor-status-dot status-ok\" title=\"OK\"></span>"
                           : "<span class=\"status-dot sensor-status-dot status-err\" title=\"ERR\"></span>";
                const uint32_t age_s = (uint32_t)((now - st.last_read_ms) / 1000u);
                snprintf(age_buf, sizeof(age_buf), "%lus", (unsigned long)age_s);
                age = age_buf;
            }
    
            String pin;
            if (cfg.type == MeteoController::SensorType::Dht22 && cfg.dht_pin != MeteoController::kInvalidPin)
                pin = String((unsigned)cfg.dht_pin);
    
            String addr;
            if (cfg.type == MeteoController::SensorType::Ds18b20 && cfg.ds18_addr_set)
            {
                char hex[17] = {};
                MeteoController::formatHexAddr(cfg.ds18_addr, hex);
                addr = hex;
            }
    
            const bool has_remote = (cfg.source_node_id != 0 && cfg.source_sensor_id != 0);
            MeteoController::SensorType ui_type = cfg.type;
            if (has_remote)
            {
                MeteoController::SensorType remote_type = MeteoController::SensorType::None;
                if (web.meteoRemoteType_(cfg.source_node_id, cfg.source_sensor_id, remote_type) &&
                    remote_type != MeteoController::SensorType::None)
                    ui_type = remote_type;
            }
            const bool show_hum = (ui_type == MeteoController::SensorType::Dht22);
            const bool ok_on = has_read && st.ok;
    
            const bool has_groups = web.hasGroups_();
            items += "<div class=\"tile js-group-item";
            if (!enabled)
                items += " disabled";
            items += "\" data-group-id=\"";
            items += String((unsigned)cfg.group_id);
            items += "\"";
            items += web.groupVisibilityStyleAttr_(cfg.group_id);
            items += " data-sensor-id=\"";
            items += String((unsigned)cfg.id);
            items += "\">";
            items += "<div class=\"sensor-visual\">";
            items += "<span class=\"badge\">#";
            items += String((unsigned)cfg.id);
            items += "</span>";
            items += "<svg class=\"sensor-icon ";
            if (!ok_on)
                items += "na";
            items += "\" viewBox=\"0 0 64 64\" aria-hidden=\"true\">";
            items += "<path fill=\"currentColor\" d=\"M32 6c-5.5 0-10 4.5-10 10v19.2c-2.6 2.4-4 5.7-4 9.3 0 7.2 5.8 13 13 13s13-5.8 13-13c0-3.6-1.4-6.9-4-9.3V16c0-5.5-4.5-10-10-10zm6 33.1V16c0-3.3-2.7-6-6-6s-6 2.7-6 6v23.1l-0.9 0.9c-1.8 1.7-2.8 3.9-2.8 6.4 0 4.9 4 9 9 9s9-4 9-9c0-2.5-1-4.8-2.8-6.4l-0.5-0.5z\"/>";
            items += "<rect x=\"30\" y=\"20\" width=\"4\" height=\"20\" rx=\"2\" fill=\"currentColor\"/>";
            items += "</svg>";
            items += "<div class=\"sensor-readout\">";
            items += "<div class=\"sensor-value\"><span class=\"sensor-temp-value\">";
            items += temp;
            items += "</span></div><div class=\"sensor-unit\">°C</div>";
            if (show_hum)
            {
                items += "<div class=\"sensor-hum\"><svg class=\"sensor-hum-icon\" viewBox=\"0 0 64 64\" aria-hidden=\"true\">";
                items += "<path fill=\"currentColor\" d=\"M32 6c7 12 16 22 16 34 0 8.8-7.2 16-16 16S16 48.8 16 40c0-12 9-22 16-34z\"/>";
                items += "</svg><div class=\"sensor-value sensor-hum-value\">";
                items += hum;
                items += "</div><div class=\"sensor-unit\">%</div></div>";
            }
            items += "</div>";
            items += "</div>";
            items += "<div>";
            items += "<div class=\"tile-head\"><strong>";
            items += WebUiRu::Meteo::kTitlePrefix;
            items += String((unsigned)cfg.id);
            items += "</strong>";
            items += "<label class=\"switch\"><input type=\"checkbox\" class=\"meteo-enable\" name=\"m";
            items += String((unsigned)cfg.id);
            items += "_en\"";
            if (enabled)
                items += " checked";
            if (!can_edit)
                items += " disabled";
            items += "><span class=\"track\"><span class=\"knob\"></span></span></label></div>";
            items += WebUiRu::Meteo::kText12;
            items += web.meteoRemoteNodeOptionsHtml_(cfg.source_node_id);
            items += "</select></div>";
            items += WebUiRu::Meteo::kInputClassFieldNameMeteoNameType;
            items += String((unsigned)cfg.id);
            items += "_name\" value=\"";
            web.appendHtmlEscaped_(items, cfg.name.c_str());
            items += "\"";
            if (!can_edit)
                items += " readonly";
            items += "></div>";
            items += String("<div class=\"form-row\" style=\"margin-top:8px;margin-bottom:8px\"><label>") + WebUiRu::GroupsPage::kLabel + "</label><select class=\"field\" name=\"m";
            items += String((unsigned)cfg.id);
            items += "_group\"";
            if (!can_edit || !has_groups)
                items += " disabled";
            items += ">";
            items += web.groupOptionsHtml_(cfg.group_id, true, true);
            items += "</select></div>";
            items += WebUiRu::Meteo::kSelectClassFieldNameMeteoSourceName;
            items += String((unsigned)cfg.id);
            items += "_src\">";
            items += web.meteoRemoteSensorOptionsHtml_(cfg.source_sensor_id, cfg.source_node_id);
            items += "</select></div>";
            items += "<div class=\"form-grid\">";
            items += WebUiRu::Meteo::kSelectClassFieldMeteoTypeNameM;
            items += String((unsigned)cfg.id);
            items += "_type\"";
            if (!can_edit)
                items += " disabled";
            items += ">";
            appendTypeOption("none", "none", ui_type == MeteoController::SensorType::None);
            appendTypeOption("ds18b20", "ds18b20", ui_type == MeteoController::SensorType::Ds18b20);
            appendTypeOption("dht22", "dht22", ui_type == MeteoController::SensorType::Dht22);
            items += "</select></div>";
            items += WebUiRu::Meteo::kSelectClassFieldMiniMeteoPinData;
            items += pin;
            items += "\" name=\"m";
            items += String((unsigned)cfg.id);
            items += "_pin\"></select></div>";
            items += WebUiRu::Meteo::kSelectClassFieldAddrMeteoAddrName;
            items += String((unsigned)cfg.id);
            items += "_addr\"";
            if (!can_edit)
                items += " disabled";
            items += ">";
            items += "<option value=\"\">-</option>";
            bool addr_found = false;
            for (size_t i = 0; i < scratch->meteo_ds18_count; ++i)
            {
                bool used = false;
                for (size_t j = 0; j < ds18_used_count; ++j)
                {
                    if (strcmp(scratch->meteo_ds18_used[j], scratch->meteo_ds18_list[i]) == 0)
                    {
                        used = true;
                        break;
                    }
                }
                if (used && (!addr.length() || addr != scratch->meteo_ds18_list[i]))
                    continue;
                items += "<option value=\"";
                items += scratch->meteo_ds18_list[i];
                items += "\"";
                if (addr.length() && addr == scratch->meteo_ds18_list[i])
                {
                    items += " selected";
                    addr_found = true;
                }
                items += ">";
                items += scratch->meteo_ds18_list[i];
                items += "</option>";
            }
            if (addr.length() && !addr_found)
            {
                items += "<option value=\"";
                items += addr;
                items += "\" selected>";
                items += addr;
                items += "</option>";
            }
            items += "</select></div>";
            items += "</div>";
            items += "<div class=\"status-line\">";
            items += ok;
            items += "<span class=\"meteo-age-text\">";
            items += WebUiRu::Meteo::kText13;
            items += age;
            items += "</span></div>";
            items += "</div></div>";
        };
    
        const size_t page_limit = (limit == 0) ? 1u : limit;
        const bool can_view_disabled = web.webSessionIsAdmin_();
        const size_t render_count = localMeteoRenderCount_(*scratch, MeteoController::kSensorCount, can_view_disabled);
        size_t rendered = 0;
        size_t visible_idx = 0;
        for (size_t i = 0; i < render_count; ++i)
        {
            if (rendered >= page_limit)
                break;
            if (!scratch->meteo_valid[i])
                continue;
            if (!web.webAclCanViewItem_(UsersRegistry::AclController::Meteo, scratch->meteo_cfg[i].id))
                continue;
            if (!can_view_disabled && !scratch->meteo_cfg[i].enabled)
                continue;
            if (visible_idx < offset)
            {
                ++visible_idx;
                continue;
            }
            ++visible_idx;
            appendRow(scratch->meteo_cfg[i], scratch->meteo_st[i], scratch->meteo_cfg[i].enabled);
            ++rendered;
        }
        if (items.length() == 0)
            items = WebUiRu::Meteo::kText2;
        return items;
    }

String WebInterfaceControllersMeteoHelper::listMeteoHtml_(WebInterface &web) {
        return listMeteoHtml_(web, 0u, SIZE_MAX);
    }

String WebInterfaceControllersMeteoHelper::meteoPortOptionsJson_(const WebInterface &web) {
        return web.socketPortOptionsJson_(PortIO::PinType::Sensor);
    }

String WebInterfaceControllersMeteoHelper::meteoUsedPinsJson_(const WebInterface &web) {
        String out;
        out.reserve(128);
        out += "[";
        bool first = true;
        if (web._controllers)
        {
            auto scratch_guard = web.scratchLockGuard_();
            WebInterface::ScratchBuffer *scratch = (scratch_guard.locked() ? web.scratchBuffer_() : nullptr);
            if (!scratch)
                return "[]";
            MeteoController &meteo = web._controllers->meteo();
            loadLocalMeteoItems_(meteo, *scratch, MeteoController::kSensorCount);
            bool used[PortIO::PORT_COUNT] = {};
            for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
            {
                if (!scratch->meteo_valid[i])
                    continue;
                const auto &cfg = scratch->meteo_cfg[i];
                if (cfg.type != MeteoController::SensorType::Dht22)
                    continue;
                const uint8_t pin = cfg.dht_pin;
                if (pin != MeteoController::kInvalidPin && pin < PortIO::PORT_COUNT)
                    used[pin] = true;
            }
            for (uint8_t i = 0; i < PortIO::PORT_COUNT; ++i)
            {
                if (!used[i])
                    continue;
                const auto &p = ActiveBoardProfile::PORTS[i];
                if (p.caps == Cap::None || p.type != PortIO::PinType::DInput)
                    continue;
                if (!first)
                    out += ",";
                out += String((unsigned)i);
                first = false;
            }
        }
        out += "]";
        return out;
    }

String WebInterfaceControllersMeteoHelper::stackMeteoPortOptionsJson_(const WebInterface &web, uint32_t node_id) {
        (void)node_id;
        String out;
        out.reserve(256);
        out += "[";
        bool first = true;
        for (uint16_t i = 0; i < PortIO::PORT_COUNT; ++i)
        {
            const auto &p = ActiveBoardProfile::PORTS[i];
            if (p.caps == Cap::None || p.type != PortIO::PinType::Sensor)
                continue;
            if (!first)
                out += ",";
            out += "{\"v\":";
            out += String((unsigned)i);
            out += ",\"l\":\"";
            out += String((unsigned)i);
            out += "\"}";
            first = false;
        }
        out += "]";
        return out;
    }

String WebInterfaceControllersMeteoHelper::stackMeteoUsedPinsJson_(const WebInterface &web, uint32_t node_id) {
        String out;
        out.reserve(128);
        out += "[";
        if (!web.network() || node_id == 0)
            return "[]";
        StackUnitSnapshot::State snapshot{};
        StackUnitSnapshot::CacheState cache{};
        if (!web.network()->stackIndexState(node_id, snapshot) || !web.network()->stackIndexCacheState(node_id, cache))
        {
            const_cast<WebInterface &>(web).requestStackMeteo_(node_id);
            waitForStackMeteoCache_(const_cast<WebInterface &>(web), node_id);
        }
        if (!web.network()->stackIndexState(node_id, snapshot) || !web.network()->stackIndexCacheState(node_id, cache))
            return "[]";
        if (cache.meteo_count == 0 && snapshot.meteo_enabled > 0)
        {
            const_cast<WebInterface &>(web).requestStackMeteo_(node_id);
            waitForStackMeteoCache_(const_cast<WebInterface &>(web), node_id);
            web.network()->stackIndexState(node_id, snapshot);
            web.network()->stackIndexCacheState(node_id, cache);
        }
        bool used[PortIO::PORT_COUNT] = {};
        web.network()->forEachStackMeteo(node_id, cache.meteo_count, [&](uint8_t, const StackUnitSnapshot::MeteoItem &item) {
            if (!item.enabled || item.type != (uint8_t)MeteoController::SensorType::Dht22)
                return;
            const uint8_t pin = item.dht_pin;
            if (pin != MeteoController::kInvalidPin && pin < PortIO::PORT_COUNT)
                used[pin] = true;
        });
        bool first = true;
        for (uint8_t i = 0; i < PortIO::PORT_COUNT; ++i)
        {
            if (!used[i])
                continue;
            if (!first)
                out += ",";
            out += String((unsigned)i);
            first = false;
        }
        out += "]";
        return out;
    }

String WebInterfaceControllersMeteoHelper::stackMeteoDs18OptionsJson_(const WebInterface &web, uint32_t node_id) {
        String out;
        out.reserve(512);
        out += "[";
        if (!web.network() || node_id == 0)
            return "[]";
        StackUnitSnapshot::State snapshot{};
        StackUnitSnapshot::CacheState cache{};
        if (!web.network()->stackIndexState(node_id, snapshot) || !web.network()->stackIndexCacheState(node_id, cache))
        {
            const_cast<WebInterface &>(web).requestStackMeteo_(node_id);
            waitForStackMeteoCache_(const_cast<WebInterface &>(web), node_id);
        }
        if (!web.network()->stackIndexState(node_id, snapshot) || !web.network()->stackIndexCacheState(node_id, cache))
            return "[]";
        if (cache.meteo_count == 0 && snapshot.meteo_enabled > 0)
        {
            const_cast<WebInterface &>(web).requestStackMeteo_(node_id);
            waitForStackMeteoCache_(const_cast<WebInterface &>(web), node_id);
            web.network()->stackIndexState(node_id, snapshot);
            web.network()->stackIndexCacheState(node_id, cache);
        }
        bool first = true;
        web.network()->forEachStackMeteo(node_id, cache.meteo_count, [&](uint8_t, const StackUnitSnapshot::MeteoItem &item) {
            if (!item.enabled || item.type != (uint8_t)MeteoController::SensorType::Ds18b20 || !item.ds18_addr_set)
                return;
            char hex[17] = {};
            MeteoController::formatHexAddr(item.ds18_addr, hex);
            if (!first)
                out += ",";
            out += "\"";
            out += hex;
            out += "\"";
            first = false;
        });
        out += "]";
        return out;
    }

String WebInterfaceControllersMeteoHelper::stackMeteoDs18UsedJson_(const WebInterface &web, uint32_t node_id) {
        return stackMeteoDs18OptionsJson_(web, node_id);
    }

String WebInterfaceControllersMeteoHelper::meteoSensorOptionsHtml_(const WebInterface &web, uint8_t selected_id, uint32_t selected_node_id,
                                          const uint8_t used_local[MeteoController::kSensorCount + 1],
                                          const uint32_t *used_remote, size_t used_remote_count) {
        (void)used_remote;
        (void)used_remote_count;
        String out;
        out += "<option value=\"\">-</option>";
        if (!web._controllers)
            return out;
        auto scratch_guard = web.scratchLockGuard_();
        WebInterface::ScratchBuffer *scratch = (scratch_guard.locked() ? web.scratchBuffer_() : nullptr);
        if (!scratch)
            return out;
        MeteoController &meteo = web._controllers->meteo();
        loadLocalMeteoItems_(meteo, *scratch, MeteoController::kSensorCount);
        for (uint8_t id = 1; id <= MeteoController::kSensorCount; ++id)
        {
            if (!scratch->meteo_valid[id - 1] || !scratch->meteo_cfg[id - 1].enabled)
                continue;
            const bool is_selected = (selected_node_id == 0 && id == selected_id);
            const bool is_used = (id <= MeteoController::kSensorCount) && (used_local[id] > 0) && !is_selected;
            if (is_used)
                continue;
            const auto &cfg = scratch->meteo_cfg[id - 1];
            out += "<option value=\"";
            out += String((unsigned)id);
            out += "\"";
            if (is_selected)
                out += " selected";
            out += ">";
            String label_name;
            if (cfg.name.length())
            {
                label_name = cfg.name;
            }
            else if (cfg.source_node_id && cfg.source_sensor_id)
            {
                label_name = web.meteoRemoteSensorName_(cfg.source_node_id, cfg.source_sensor_id);
            }
            out += String((unsigned)id);
            if (label_name.length())
            {
                out += ": ";
                web.appendHtmlEscaped_(out, label_name.c_str());
            }
            out += "</option>";
        }
        bool selected_remote_found = false;
        if (web.network())
        {
            const size_t count = web.network()->stackOnlineDeviceCount();
            for (size_t i = 0; i < count; ++i)
            {
                StackDeviceRegistry::DeviceInfo device{};
                if (!web.network()->stackDeviceSnapshotAt(i, device) || !device.online || device.node_id == 0)
                    continue;
                StackUnitSnapshot::State snapshot{};
                StackUnitSnapshot::CacheState cache{};
                if (!web.network()->stackIndexState(device.node_id, snapshot) ||
                    !web.network()->stackIndexCacheState(device.node_id, cache))
                {
                    ensureStackMeteoSnapshot_(web, device.node_id, nullptr);
                    waitForStackMeteoCache_(const_cast<WebInterface &>(web), device.node_id);
                    if (!web.network()->stackIndexState(device.node_id, snapshot) ||
                        !web.network()->stackIndexCacheState(device.node_id, cache))
                        continue;
                }
                ensureStackMeteoSnapshot_(web, device.node_id, &snapshot, &cache);
                if (cache.meteo_count == 0 && snapshot.meteo_enabled > 0)
                {
                    waitForStackMeteoCache_(const_cast<WebInterface &>(web), device.node_id);
                    web.network()->stackIndexState(device.node_id, snapshot);
                    web.network()->stackIndexCacheState(device.node_id, cache);
                }
                if (cache.meteo_count == 0)
                    continue;
                web.network()->forEachStackMeteo(device.node_id, cache.meteo_count, [&](uint8_t, const StackUnitSnapshot::MeteoItem &item) {
                    if (!item.enabled)
                        return;
                    const uint32_t remote_key = (device.node_id << 8) | item.id;
                    bool is_used = false;
                    for (size_t j = 0; j < used_remote_count; ++j)
                    {
                        if (used_remote[j] == remote_key &&
                            !(selected_node_id == device.node_id && selected_id == item.id))
                        {
                            is_used = true;
                            break;
                        }
                    }
                    if (is_used)
                        return;
                    out += "<option value=\"";
                    out += String((unsigned long)device.node_id);
                    out += ":";
                    out += String((unsigned)item.id);
                    out += "\"";
                    if (selected_node_id == device.node_id && selected_id == item.id)
                    {
                        out += " selected";
                        selected_remote_found = true;
                    }
                    out += ">";
                    const String label = web.meteoRemoteLabel_(device.node_id, item.id);
                    if (label.length())
                        web.appendHtmlEscaped_(out, label.c_str());
                    else
                        out += String((unsigned)item.id);
                    out += "</option>";
                });
            }
        }
        if (selected_node_id != 0 && selected_id != 0 && !selected_remote_found)
        {
            const String remote_name = web.meteoRemoteSensorName_(selected_node_id, selected_id);
            out += "<option value=\"";
            out += String((unsigned long)selected_node_id);
            out += ":";
            out += String((unsigned)selected_id);
            out += "\" selected>";
            if (remote_name.length())
                web.appendHtmlEscaped_(out, remote_name.c_str());
            else
                out += String((unsigned)selected_id);
            out += "</option>";
        }
        return out;
    }

String WebInterfaceControllersMeteoHelper::meteoRemoteSensorOptionsHtml_(const WebInterface &web, uint8_t selected_id, uint32_t selected_node_id) {
        String out;
        out += "<option value=\"\">-</option>";
        if (!web.network())
            return out;
        const size_t count = web.network()->stackOnlineDeviceCount();
        for (size_t i = 0; i < count; ++i)
        {
            StackDeviceRegistry::DeviceInfo device{};
            if (!web.network()->stackDeviceSnapshotAt(i, device) || !device.online || device.node_id == 0)
                continue;
            StackUnitSnapshot::State snapshot{};
            StackUnitSnapshot::CacheState cache{};
            if (!web.network()->stackIndexState(device.node_id, snapshot) ||
                !web.network()->stackIndexCacheState(device.node_id, cache))
            {
                ensureStackMeteoSnapshot_(web, device.node_id, nullptr);
                waitForStackMeteoCache_(const_cast<WebInterface &>(web), device.node_id);
                if (!web.network()->stackIndexState(device.node_id, snapshot) ||
                    !web.network()->stackIndexCacheState(device.node_id, cache))
                    continue;
            }
            ensureStackMeteoSnapshot_(web, device.node_id, &snapshot, &cache);
            if (cache.meteo_count == 0 && snapshot.meteo_enabled > 0)
            {
                waitForStackMeteoCache_(const_cast<WebInterface &>(web), device.node_id);
                web.network()->stackIndexState(device.node_id, snapshot);
                web.network()->stackIndexCacheState(device.node_id, cache);
            }
            if (cache.meteo_count == 0)
                continue;
            web.network()->forEachStackMeteo(device.node_id, cache.meteo_count, [&](uint8_t, const StackUnitSnapshot::MeteoItem &item) {
                if (!item.enabled)
                    return;
                out += "<option value=\"";
                out += String((unsigned long)device.node_id);
                out += ":";
                out += String((unsigned)item.id);
                out += "\" data-node=\"";
                out += String((unsigned long)device.node_id);
                out += "\"";
                if (selected_node_id == device.node_id && selected_id == item.id)
                    out += " selected";
                out += ">";
                const String label = web.meteoRemoteLabel_(device.node_id, item.id);
                if (label.length())
                    web.appendHtmlEscaped_(out, label.c_str());
                else
                    out += stackMeteoSensorLabel_(web, device.node_id, item);
                out += "</option>";
            });
        }
        return out;
    }

String WebInterfaceControllersMeteoHelper::meteoRemoteNodeOptionsHtml_(const WebInterface &web, uint32_t selected_node_id) {
        String out;
        out += "<option value=\"local\"";
        if (selected_node_id == 0)
            out += " selected";
        out += ">local</option>";
        if (!web.network())
            return out;
        const size_t count = web.network()->stackOnlineDeviceCount();
        for (size_t i = 0; i < count; ++i)
        {
            StackDeviceRegistry::DeviceInfo device{};
            if (!web.network()->stackDeviceSnapshotAt(i, device) || !device.online || device.node_id == 0)
                continue;
            out += "<option value=\"";
            out += String((unsigned long)device.node_id);
            out += "\"";
            if (selected_node_id == device.node_id)
                out += " selected";
            out += ">";
            if (device.name[0])
                web.appendHtmlEscaped_(out, device.name);
            else
                out += web.stackNodeIdHex_(device.node_id);
            out += "</option>";
        }
        return out;
    }

String WebInterfaceControllersMeteoHelper::meteoRemoteLabel_(const WebInterface &web, uint32_t node_id, uint8_t sensor_id) {
        String out;
        String node_name;
        if (web.network())
        {
            StackDeviceRegistry::DeviceInfo device{};
            if (web.network()->stackDeviceSnapshotByNodeId(node_id, device))
                node_name = device.name[0] ? String(device.name) : web.stackNodeIdHex_(node_id);
        }
        const String sensor_name = web.meteoRemoteSensorName_(node_id, sensor_id);
        if (node_name.length())
            out += node_name;
        if (sensor_name.length())
        {
            if (out.length())
                out += " / ";
            out += sensor_name;
        }
        return out;
    }

String WebInterfaceControllersMeteoHelper::meteoRemoteSensorName_(const WebInterface &web, uint32_t node_id, uint8_t sensor_id) {
        if (node_id == 0)
        {
            if (!web._controllers)
                return "";
            auto scratch_guard = web.scratchLockGuard_();
            WebInterface::ScratchBuffer *scratch = (scratch_guard.locked() ? web.scratchBuffer_() : nullptr);
            if (!scratch)
                return "";
            MeteoController &meteo = web._controllers->meteo();
            loadLocalMeteoItems_(meteo, *scratch, MeteoController::kSensorCount);
            if (sensor_id == 0 || sensor_id > MeteoController::kSensorCount)
                return "";
            return scratch->meteo_valid[sensor_id - 1] ? scratch->meteo_cfg[sensor_id - 1].name : String("");
        }
        if (!web.network())
            return "";
        StackUnitSnapshot::MeteoItem item{};
        if (!web.network()->stackIndexMeteoById(node_id, sensor_id, item))
            return "";
        return item.name[0] ? String(item.name) : String("Sensor ") + String((unsigned)sensor_id);
    }

bool WebInterfaceControllersMeteoHelper::meteoRemoteType_(const WebInterface &web, uint32_t node_id, uint8_t sensor_id, MeteoController::SensorType &out) {
        out = MeteoController::SensorType::None;
        if (node_id == 0)
        {
            if (!web._controllers)
                return false;
            auto scratch_guard = web.scratchLockGuard_();
            WebInterface::ScratchBuffer *scratch = (scratch_guard.locked() ? web.scratchBuffer_() : nullptr);
            if (!scratch)
                return false;
            MeteoController &meteo = web._controllers->meteo();
            loadLocalMeteoItems_(meteo, *scratch, MeteoController::kSensorCount);
            if (sensor_id == 0 || sensor_id > MeteoController::kSensorCount)
                return false;
            if (!scratch->meteo_valid[sensor_id - 1])
                return false;
            out = scratch->meteo_cfg[sensor_id - 1].type;
            return true;
        }
        if (!web.network())
            return false;
        StackUnitSnapshot::MeteoItem item{};
        if (!web.network()->stackIndexMeteoById(node_id, sensor_id, item))
            return false;
        out = (MeteoController::SensorType)item.type;
        return out != MeteoController::SensorType::None;
    }

bool WebInterfaceControllersMeteoHelper::isMeteoSensorActive_(const WebInterface &web, uint8_t id) {
        if (!web._controllers)
            return false;
        auto scratch_guard = web.scratchLockGuard_();
        WebInterface::ScratchBuffer *scratch = (scratch_guard.locked() ? web.scratchBuffer_() : nullptr);
        if (!scratch)
            return false;
        MeteoController &meteo = web._controllers->meteo();
        loadLocalMeteoItems_(meteo, *scratch, MeteoController::kSensorCount);
        if (id == 0 || id > MeteoController::kSensorCount)
            return false;
        return scratch->meteo_valid[id - 1] && scratch->meteo_cfg[id - 1].enabled;
    }

bool WebInterfaceControllersMeteoHelper::isRemoteMeteoSensorActive_(const WebInterface &web, uint32_t node_id, uint8_t id) {
        if (node_id == 0 || !web.network() || id == 0)
            return false;
        StackUnitSnapshot::MeteoItem item{};
        return web.network()->stackIndexMeteoById(node_id, id, item) && item.enabled;
    }

size_t WebInterface::meteoLocalRenderCount_() const {
        return WebInterfaceControllersMeteoHelper::meteoLocalRenderCount_(*this);
    }

String WebInterface::meteoDeviceSelectHtml_(uint32_t selected_node_id, bool stack_view) const {
        return WebInterfaceControllersMeteoHelper::meteoDeviceSelectHtml_(*this, selected_node_id, stack_view);
    }

String WebInterface::stackMeteoStatusText_(uint32_t node_id) const {
        return WebInterfaceControllersMeteoHelper::stackMeteoStatusText_(*this, node_id);
    }

bool WebInterface::isStackMeteoView_(uint32_t node_id) const {
        return WebInterfaceControllersMeteoHelper::isStackMeteoView_(*this, node_id);
    }

bool WebInterface::requestStackMeteo_(uint32_t node_id) {
        return WebInterfaceControllersMeteoHelper::requestStackMeteo_(*this, node_id);
    }

size_t WebInterface::stackMeteoVisibleCount_(uint32_t node_id) const {
        return WebInterfaceControllersMeteoHelper::stackMeteoVisibleCount_(*this, node_id);
    }

String WebInterface::listStackMeteoHtml_(uint32_t node_id, size_t offset, size_t limit) {
        return WebInterfaceControllersMeteoHelper::listStackMeteoHtml_(*this, node_id, offset, limit);
    }

String WebInterface::listMeteoHtml_() {
        return WebInterfaceControllersMeteoHelper::listMeteoHtml_(*this);
    }

String WebInterface::listMeteoHtml_(size_t offset, size_t limit) {
        return WebInterfaceControllersMeteoHelper::listMeteoHtml_(*this, offset, limit);
    }

String WebInterface::meteoPortOptionsJson_() const {
        return WebInterfaceControllersMeteoHelper::meteoPortOptionsJson_(*this);
    }

String WebInterface::meteoUsedPinsJson_() const {
        return WebInterfaceControllersMeteoHelper::meteoUsedPinsJson_(*this);
    }

String WebInterface::stackMeteoPortOptionsJson_(uint32_t node_id) const {
        return WebInterfaceControllersMeteoHelper::stackMeteoPortOptionsJson_(*this, node_id);
    }

String WebInterface::stackMeteoUsedPinsJson_(uint32_t node_id) const {
        return WebInterfaceControllersMeteoHelper::stackMeteoUsedPinsJson_(*this, node_id);
    }

String WebInterface::meteoSensorOptionsHtml_(uint8_t selected_id, uint32_t selected_node_id,
                                   const uint8_t used_local[MeteoController::kSensorCount + 1],
                                   const uint32_t *used_remote, size_t used_remote_count) const {
        return WebInterfaceControllersMeteoHelper::meteoSensorOptionsHtml_(*this, selected_id, selected_node_id, used_local, used_remote, used_remote_count);
    }

String WebInterface::meteoRemoteSensorOptionsHtml_(uint8_t selected_id, uint32_t selected_node_id) const {
        return WebInterfaceControllersMeteoHelper::meteoRemoteSensorOptionsHtml_(*this, selected_id, selected_node_id);
    }

String WebInterface::meteoRemoteNodeOptionsHtml_(uint32_t selected_node_id) const {
        return WebInterfaceControllersMeteoHelper::meteoRemoteNodeOptionsHtml_(*this, selected_node_id);
    }

String WebInterface::meteoRemoteLabel_(uint32_t node_id, uint8_t sensor_id) const {
        return WebInterfaceControllersMeteoHelper::meteoRemoteLabel_(*this, node_id, sensor_id);
    }

String WebInterface::meteoRemoteSensorName_(uint32_t node_id, uint8_t sensor_id) const {
        return WebInterfaceControllersMeteoHelper::meteoRemoteSensorName_(*this, node_id, sensor_id);
    }

bool WebInterface::meteoRemoteType_(uint32_t node_id, uint8_t sensor_id, MeteoController::SensorType &out) const {
        return WebInterfaceControllersMeteoHelper::meteoRemoteType_(*this, node_id, sensor_id, out);
    }

bool WebInterface::isMeteoSensorActive_(uint8_t id) const {
        return WebInterfaceControllersMeteoHelper::isMeteoSensorActive_(*this, id);
    }

bool WebInterface::isRemoteMeteoSensorActive_(uint32_t node_id, uint8_t id) const {
        return WebInterfaceControllersMeteoHelper::isRemoteMeteoSensorActive_(*this, node_id, id);
    }


