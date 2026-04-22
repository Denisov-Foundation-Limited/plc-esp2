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
void loadLocalWateringItems_(WateringController &watering, WebInterface::ScratchBuffer &scratch, size_t count)
{
    if (count == 0)
        return;
    auto guard = watering.lockGuard();
    for (size_t i = 0; i < count; ++i)
    {
        scratch.watering_valid[i] = false;
        const auto *cfg = watering.configByIndex(i);
        const auto *st = watering.stateByIndex(i);
        if (!cfg || !st)
            continue;
        scratch.watering_valid[i] = true;
        scratch.watering_cfg[i] = *cfg;
        scratch.watering_st[i] = *st;
    }
}

void loadLocalWateringTankItems_(const TankController &tanks, WebInterface::ScratchBuffer &scratch, size_t count)
{
    if (count == 0)
        return;
    auto guard = tanks.lockGuard();
    for (size_t i = 0; i < count; ++i)
    {
        scratch.tank_valid[i] = false;
        const auto *cfg = tanks.configByIndex(i);
        if (!cfg || !cfg->enabled)
            continue;
        scratch.tank_valid[i] = true;
        scratch.tank_cfg[i] = *cfg;
    }
}
}

size_t WebInterfaceControllersWateringHelper::wateringLocalRenderCount_(const WebInterface &web) {
        if (!web._controllers)
            return 0;
        WateringController &watering = web._controllers->watering();
        auto guard = watering.lockGuard();
        size_t last_enabled_idx = SIZE_MAX;
        for (size_t i = 0; i < WateringController::kRuleCount; ++i)
        {
            const auto *cfg = watering.configByIndex(i);
            if (cfg && cfg->enabled)
                last_enabled_idx = i;
        }
        if (last_enabled_idx == SIZE_MAX)
            return WateringController::kRuleCount ? 1u : 0u;
        const size_t count = last_enabled_idx + 2u;
        return count > WateringController::kRuleCount ? WateringController::kRuleCount : count;
    }

String WebInterfaceControllersWateringHelper::wateringDeviceSelectHtml_(const WebInterface &web, uint32_t selected_node_id, bool stack_view) {
        if (!web.network() || web.network()->stackRole() != ConfigsManagerIface::StackRole::Master)
            return "";
        String html;
        html.reserve(512);
        html += "<div class=\"row\">";
        html += String("<span class=\"muted\">") + WebUiRu::kDevice + "</span>";
        html += "<select id=\"watering-device\" class=\"field mini\">";
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

String WebInterfaceControllersWateringHelper::stackWateringStatusText_(const WebInterface &web, uint32_t node_id) {
        if (!web.network() || node_id == 0)
            return WebUiRu::kNoDataFromSlave;
        StackUnitSnapshot::State snapshot{};
        StackUnitSnapshot::CacheState cache{};
        if (!web.network()->stackIndexState(node_id, snapshot) || !web.network()->stackIndexCacheState(node_id, cache))
            return WebUiRu::kNoDataFromSlave;
        String out;
        out.reserve(64);
        out += WebUiRu::Watering::kText6;
        out += String((unsigned)snapshot.watering_enabled);
        out += " · ";
        out += WebUiRu::Watering::kText9;
        out += String((unsigned)snapshot.watering_active);
        if (cache.watering_count < snapshot.watering_enabled)
        {
            out += " · ";
            out += "загрузка";
        }
        return out;
    }

bool WebInterfaceControllersWateringHelper::isStackWateringView_(const WebInterface &web, uint32_t node_id) {
        if (node_id == 0 || !web.network() || web.network()->stackRole() != ConfigsManagerIface::StackRole::Master)
            return false;
        StackDeviceRegistry::DeviceInfo device{};
        return web.network()->stackDeviceSnapshotByNodeId(node_id, device) && device.online;
    }

bool WebInterfaceControllersWateringHelper::requestStackWatering_(WebInterface &web, uint32_t node_id) {
        if (!web.network() || node_id == 0)
            return false;
        const uint32_t now = millis();
        StackUnitSnapshot::State snapshot{};
        StackUnitSnapshot::CacheState cache{};
        const bool has_snapshot = web.network()->stackIndexState(node_id, snapshot);
        const bool has_cache = web.network()->stackIndexCacheState(node_id, cache);
        if (!has_snapshot || snapshot.updated_ms == 0 || (uint32_t)(now - snapshot.updated_ms) > 5000u ||
            !has_cache || (cache.watering_count == 0 && snapshot.watering_enabled > 0))
        {
            if (!web.network()->prepareStackPageRequest(StackUnitSnapshot::PageKind::Watering, node_id, now, 0, 4000u))
                return false;
            DynamicJsonDocument req(64);
            req["offset"] = 0;
            req["limit"] = StackUnitSnapshot::kPageSize;
            return web.network()->stackRoute().sendRequestSelected(web.stackPayloadMode(), node_id, "watering",
                                                                   "snapshot_req", &req, true);
        }
        if (snapshot.watering_enabled > 0 && cache.watering_count < snapshot.watering_enabled)
        {
            const uint16_t next_offset = cache.watering_count;
            if (!web.network()->prepareStackPageRequest(StackUnitSnapshot::PageKind::Watering, node_id, now, next_offset,
                                                        4000u))
                return true;
            DynamicJsonDocument req(64);
            req["offset"] = next_offset;
            req["limit"] = StackUnitSnapshot::kPageSize;
            return web.network()->stackRoute().sendRequestSelected(web.stackPayloadMode(), node_id, "watering",
                                                                   "snapshot_req", &req, true);
        }
        return true;
    }

size_t WebInterfaceControllersWateringHelper::stackWateringVisibleCount_(const WebInterface &web, uint32_t node_id) {
        if (!web.network() || node_id == 0)
            return 0;
        StackUnitSnapshot::State snapshot{};
        StackUnitSnapshot::CacheState cache{};
        if (!web.network()->stackIndexState(node_id, snapshot) || !web.network()->stackIndexCacheState(node_id, cache))
            return 0;
        return cache.watering_count > 0 ? cache.watering_count : snapshot.watering_enabled;
    }

String WebInterfaceControllersWateringHelper::listStackWateringHtml_(WebInterface &web, uint32_t node_id, size_t offset, size_t limit) {
        if (!web.network() || node_id == 0)
            return WebUiRu::kNoDataFromSlave;
        StackUnitSnapshot::State snapshot{};
        StackUnitSnapshot::CacheState cache{};
        if (!web.network()->stackIndexState(node_id, snapshot) || !web.network()->stackIndexCacheState(node_id, cache))
            return WebUiRu::kNoDataFromSlave;
        if (cache.watering_count == 0)
            return snapshot.watering_enabled > 0 ? String("Данные загружаются") : String(WebUiRu::Watering::kText);

        String items;
        items.reserve(16384);
        const size_t page_limit = (limit == 0) ? SIZE_MAX : limit;
        const bool can_view_disabled = web.webSessionIsAdmin_();
        size_t rendered = 0;
        size_t visible_idx = 0;
        web.network()->forEachStackWatering(node_id, cache.watering_count, [&](uint8_t, const StackUnitSnapshot::WateringItem &cfg) {
            if (rendered >= page_limit)
                return;
            if (!web.webAclCanViewItem_(UsersRegistry::AclController::Watering, cfg.id, node_id))
                return;
            if (!can_view_disabled && !cfg.enabled)
                return;
            if (visible_idx < offset)
            {
                ++visible_idx;
                return;
            }
            ++visible_idx;

            const bool can_control = web.webAclCanControlItem_(UsersRegistry::AclController::Watering, cfg.id, node_id);
            const char *state_label = cfg.active ? WebUiRu::Watering::kText3
                                                 : (cfg.paused ? WebUiRu::Watering::kText4 : WebUiRu::Watering::kText5);
            items += "<div class=\"tile\" data-rule-id=\"";
            items += String((unsigned)cfg.id);
            items += "\" data-active=\"";
            items += cfg.active ? "1\">" : "0\">";
            items += WebUiRu::Watering::kText6;
            items += String((unsigned)cfg.id);
            items += "</strong><span class=\"badge\">";
            items += cfg.enabled ? WebUiRu::Watering::kText7 : WebUiRu::Watering::kText8;
            items += WebUiRu::Watering::kText9;
            items += state_label;
            items += "</span></div></div>";
            items += "<div><div class=\"tile-head\"><strong>";
            if (cfg.name[0])
                web.appendHtmlEscaped_(items, cfg.name);
            else
                items += WebUiRu::Watering::kText10;
            items += "</strong>";
            if (can_control)
            {
                items += "<input type=\"hidden\" name=\"w";
                items += String((unsigned)cfg.id);
                items += "_en\" value=\"0\"><label class=\"switch\"><input type=\"checkbox\" value=\"1\" name=\"w";
                items += String((unsigned)cfg.id);
                items += "_en\"";
                if (cfg.enabled)
                    items += " checked";
                items += "><span class=\"track\"><span class=\"knob\"></span></span></label>";
            }
            items += "</div>";
            if (can_control)
            {
                items += "<input class=\"field name\" type=\"text\" name=\"w";
                items += String((unsigned)cfg.id);
                items += "_name\" value=\"";
                web.appendHtmlEscaped_(items, cfg.name);
                items += "\"><div class=\"form-grid\">";
                items += WebUiRu::Watering::kInputTypeCheckboxNameW;
                items += String((unsigned)cfg.id);
                items += "_status\"";
                if (cfg.status)
                    items += " checked";
                items += "><span class=\"track\"><span class=\"knob\"></span></span></label></div>";
                items += WebUiRu::Watering::kSelectClassFieldMiniWateringSelectData;
                if (cfg.port != WateringController::kInvalidPort)
                    items += String((unsigned)cfg.port);
                items += "\" name=\"w";
                items += String((unsigned)cfg.id);
                items += "_port\"></select></div>";
                items += WebUiRu::Watering::kText28;
            }
            static const uint8_t kWeekdayMap[7] = {2, 3, 4, 5, 6, 7, 1};
            static const char *kWeekdayLabels[7] = {WebUiRu::Watering::kText14, WebUiRu::Watering::kText15,
                                                    WebUiRu::Watering::kText16, WebUiRu::Watering::kText17,
                                                    WebUiRu::Watering::kText18, WebUiRu::Watering::kText19,
                                                    WebUiRu::Watering::kText20};
            if (can_control)
            {
                for (size_t wi = 0; wi < 7; ++wi)
                {
                    const uint8_t dow = kWeekdayMap[wi];
                    items += "<label class=\"weekday-item\"><input type=\"checkbox\" name=\"w";
                    items += String((unsigned)cfg.id);
                    items += "_d";
                    items += String((unsigned)dow);
                    items += "\"";
                    if (cfg.weekdays_mask & (uint8_t)(1u << (dow - 1u)))
                        items += " checked";
                    items += "><span>";
                    items += kWeekdayLabels[wi];
                    items += "</span></label>";
                }
                items += "</div></div>";

                auto appendSlot = [&](uint8_t slot_index, bool slot_enabled, uint8_t hour, uint8_t minute,
                                      uint32_t duration_sec) {
                    const char *slot_label = slot_index == 0 ? " Слот 1" : (slot_index == 1 ? " Слот 2" : " Слот 3");
                    const char *time_name = slot_index == 0 ? "_time" : (slot_index == 1 ? "_time2" : "_time3");
                    const char *dur_name = slot_index == 0 ? "_dur" : (slot_index == 1 ? "_dur2" : "_dur3");
                    const char *en_name = slot_index == 0 ? "_time_en" : (slot_index == 1 ? "_time2_en" : "_time3_en");
                    items += "<div class=\"form-row\"><label><input type=\"hidden\" name=\"w";
                    items += String((unsigned)cfg.id);
                    items += en_name;
                    items += "\" value=\"0\"><input type=\"checkbox\" name=\"w";
                    items += String((unsigned)cfg.id);
                    items += en_name;
                    items += "\" value=\"1\"";
                    if (slot_enabled)
                        items += " checked";
                    items += ">";
                    items += slot_label;
                    items += "</label></div>";
                    items += WebUiRu::Watering::kInputClassFieldMiniTypeTimeName;
                    items += String((unsigned)cfg.id);
                    items += time_name;
                    items += "\" value=\"";
                    if (hour <= 23 && minute <= 59)
                    {
                        char buf[8] = {};
                        snprintf(buf, sizeof(buf), "%02u:%02u", (unsigned)hour, (unsigned)minute);
                        items += buf;
                    }
                    items += "\"></div>";
                    items += WebUiRu::Watering::kInputClassFieldMiniTypeNumberMin;
                    items += String((unsigned)cfg.id);
                    items += dur_name;
                    items += "\" value=\"";
                    if (duration_sec)
                        items += String((unsigned long)((duration_sec + 59) / 60));
                    items += "\"></div>";
                };

                appendSlot(0, cfg.slot1_enabled, cfg.hour, cfg.minute, cfg.duration_sec);
                appendSlot(1, cfg.slot2_enabled, cfg.hour2, cfg.minute2, cfg.duration2_sec);
                appendSlot(2, cfg.slot3_enabled, cfg.hour3, cfg.minute3, cfg.duration3_sec);

                items += WebUiRu::Watering::kSelectClassFieldMiniWateringSelectData2;
                if (cfg.tank_id)
                    items += String((unsigned)cfg.tank_id);
                items += "\" name=\"w";
                items += String((unsigned)cfg.id);
                items += "_tank\"></select></div>";
                items += WebUiRu::Watering::kInputTypeCheckboxNameW2;
                items += String((unsigned)cfg.id);
                items += "_resume\"";
                if (cfg.resume_after_refill)
                    items += " checked";
                items += "><span class=\"track\"><span class=\"knob\"></span></span></label></div>";
                items += WebUiRu::Watering::kGeSelectClassFieldMiniNameW;
                items += String((unsigned)cfg.id);
                items += "_resume_level\"><option value=\"low\"";
                if (cfg.resume_level == 0)
                    items += " selected";
                items += ">low</option><option value=\"mid\"";
                if (cfg.resume_level == 1)
                    items += " selected";
                items += ">mid</option><option value=\"full\"";
                if (cfg.resume_level == 2)
                    items += " selected";
                items += ">full</option></select></div>";
            }
            items += "</div></div></div>";
            ++rendered;
        });
        return items.length() ? items : WebUiRu::Watering::kText;
    }

String WebInterfaceControllersWateringHelper::listWateringHtml_(WebInterface &web, size_t offset, size_t limit) {
        if (!web._controllers)
            return WebUiRu::Watering::kText27;
        String items;
        items.reserve(16384);
        auto scratch_guard = web.scratchLockGuard_();
        WebInterface::ScratchBuffer *scratch = (scratch_guard.locked() ? web.scratchBuffer_() : nullptr);
        if (!scratch)
            return "";
        WateringController &watering = web._controllers->watering();
        loadLocalWateringItems_(watering, *scratch, WateringController::kRuleCount);
        auto appendRule = [&](const WateringController::RuleConfig &cfg, const WateringController::RuleState &st)
        {
            const bool can_control = web.webAclCanControlItem_(UsersRegistry::AclController::Watering, cfg.id);
            const char *state_label = st.active ? WebUiRu::Watering::kText3 : (st.paused ? WebUiRu::Watering::kText4 : WebUiRu::Watering::kText5);
            items += "<div class=\"tile\" data-rule-id=\"";
            items += String((unsigned)cfg.id);
            items += "\" data-active=\"";
            items += st.active ? "1\">" : "0\">";
            items += WebUiRu::Watering::kText6;
            items += String((unsigned)cfg.id);
            items += "</strong><span class=\"badge\">";
            items += cfg.enabled ? WebUiRu::Watering::kText7 : WebUiRu::Watering::kText8;
            items += WebUiRu::Watering::kText9;
            items += state_label;
            items += "</span></div></div>";
            items += "<div><div class=\"tile-head\"><strong>";
            if (cfg.name.length())
                web.appendHtmlEscaped_(items, cfg.name.c_str());
            else
                items += WebUiRu::Watering::kText10;
            items += "</strong>";
            if (can_control)
            {
                items += "<input type=\"hidden\" name=\"w";
                items += String((unsigned)cfg.id);
                items += "_en\" value=\"0\"><label class=\"switch\"><input type=\"checkbox\" value=\"1\" name=\"w";
                items += String((unsigned)cfg.id);
                items += "_en\"";
                if (cfg.enabled)
                    items += " checked";
                items += "><span class=\"track\"><span class=\"knob\"></span></span></label>";
            }
            items += "</div>";
            if (can_control)
            {
                items += "<input class=\"field name\" type=\"text\" name=\"w";
                items += String((unsigned)cfg.id);
                items += "_name\" value=\"";
                web.appendHtmlEscaped_(items, cfg.name.c_str());
                items += "\"><div class=\"form-grid\">";
                items += WebUiRu::Watering::kInputTypeCheckboxNameW;
                items += String((unsigned)cfg.id);
                items += "_status\"";
                if (st.status)
                    items += " checked";
                items += "><span class=\"track\"><span class=\"knob\"></span></span></label></div>";
                items += WebUiRu::Watering::kSelectClassFieldMiniWateringSelectData;
                if (cfg.port != WateringController::kInvalidPort)
                    items += String((unsigned)cfg.port);
                items += "\" name=\"w";
                items += String((unsigned)cfg.id);
                items += "_port\"></select></div>";
                items += WebUiRu::Watering::kText28;
            }
            static const uint8_t kWeekdayMap[7] = {2, 3, 4, 5, 6, 7, 1};
            static const char *kWeekdayLabels[7] = {WebUiRu::Watering::kText14, WebUiRu::Watering::kText15, WebUiRu::Watering::kText16, WebUiRu::Watering::kText17, WebUiRu::Watering::kText18, WebUiRu::Watering::kText19, WebUiRu::Watering::kText20};
            if (can_control)
            {
                for (size_t wi = 0; wi < 7; ++wi)
                {
                    const uint8_t dow = kWeekdayMap[wi];
                    items += "<label class=\"weekday-item\"><input type=\"checkbox\" name=\"w";
                    items += String((unsigned)cfg.id);
                    items += "_d";
                    items += String((unsigned)dow);
                    items += "\"";
                    if (cfg.weekdays_mask & (uint8_t)(1u << (dow - 1u)))
                        items += " checked";
                    items += "><span>";
                    items += kWeekdayLabels[wi];
                    items += "</span></label>";
                }
                items += "</div></div>";

                items += "<div class=\"form-row\"><label><input type=\"hidden\" name=\"w";
                items += String((unsigned)cfg.id);
                items += "_time_en\" value=\"0\"><input type=\"checkbox\" name=\"w";
                items += String((unsigned)cfg.id);
                items += "_time_en\" value=\"1\"";
                if (cfg.slot1_enabled)
                    items += " checked";
                items += "> Слот 1</label></div>";
                items += WebUiRu::Watering::kInputClassFieldMiniTypeTimeName;
                items += String((unsigned)cfg.id);
                items += "_time\" value=\"";
                if (cfg.weekdays_mask && cfg.duration_sec && cfg.hour <= 23 && cfg.minute <= 59)
                {
                    char buf[8] = {};
                    snprintf(buf, sizeof(buf), "%02u:%02u", (unsigned)cfg.hour, (unsigned)cfg.minute);
                    items += buf;
                }
                items += "\"></div>";
                items += WebUiRu::Watering::kInputClassFieldMiniTypeNumberMin;
                items += String((unsigned)cfg.id);
                items += "_dur\" value=\"";
                if (cfg.duration_sec)
                    items += String((unsigned long)((cfg.duration_sec + 59) / 60));
                items += "\"></div>";
                items += "<div class=\"form-row\"><label><input type=\"hidden\" name=\"w";
                items += String((unsigned)cfg.id);
                items += "_time2_en\" value=\"0\"><input type=\"checkbox\" name=\"w";
                items += String((unsigned)cfg.id);
                items += "_time2_en\" value=\"1\"";
                if (cfg.slot2_enabled)
                    items += " checked";
                items += "> Слот 2</label></div>";
                items += WebUiRu::Watering::kText2InputClassFieldMiniTypeTime;
                items += String((unsigned)cfg.id);
                items += "_time2\" value=\"";
                if (cfg.weekdays_mask && cfg.duration2_sec && cfg.hour2 <= 23 && cfg.minute2 <= 59)
                {
                    char buf2[8] = {};
                    snprintf(buf2, sizeof(buf2), "%02u:%02u", (unsigned)cfg.hour2, (unsigned)cfg.minute2);
                    items += buf2;
                }
                items += "\"></div>";
                items += WebUiRu::Watering::kText2InputClassFieldMiniTypeNumber;
                items += String((unsigned)cfg.id);
                items += "_dur2\" value=\"";
                if (cfg.duration2_sec)
                    items += String((unsigned long)((cfg.duration2_sec + 59) / 60));
                items += "\"></div>";
                items += "<div class=\"form-row\"><label><input type=\"hidden\" name=\"w";
                items += String((unsigned)cfg.id);
                items += "_time3_en\" value=\"0\"><input type=\"checkbox\" name=\"w";
                items += String((unsigned)cfg.id);
                items += "_time3_en\" value=\"1\"";
                if (cfg.slot3_enabled)
                    items += " checked";
                items += "> Слот 3</label></div>";
                items += WebUiRu::Watering::kText3InputClassFieldMiniTypeTime;
                items += String((unsigned)cfg.id);
                items += "_time3\" value=\"";
                if (cfg.weekdays_mask && cfg.duration3_sec && cfg.hour3 <= 23 && cfg.minute3 <= 59)
                {
                    char buf3[8] = {};
                    snprintf(buf3, sizeof(buf3), "%02u:%02u", (unsigned)cfg.hour3, (unsigned)cfg.minute3);
                    items += buf3;
                }
                items += "\"></div>";
                items += WebUiRu::Watering::kText3InputClassFieldMiniTypeNumber;
                items += String((unsigned)cfg.id);
                items += "_dur3\" value=\"";
                if (cfg.duration3_sec)
                    items += String((unsigned long)((cfg.duration3_sec + 59) / 60));
                items += "\"></div>";
                items += WebUiRu::Watering::kSelectClassFieldMiniWateringSelectData2;
                if (cfg.tank_id)
                    items += String((unsigned)cfg.tank_id);
                items += "\" name=\"w";
                items += String((unsigned)cfg.id);
                items += "_tank\"></select></div>";
                items += WebUiRu::Watering::kInputTypeCheckboxNameW2;
                items += String((unsigned)cfg.id);
                items += "_resume\"";
                if (cfg.resume_after_refill)
                    items += " checked";
                items += "><span class=\"track\"><span class=\"knob\"></span></span></label></div>";
                items += WebUiRu::Watering::kGeSelectClassFieldMiniNameW;
                items += String((unsigned)cfg.id);
                items += "_resume_level\"><option value=\"low\"";
                if (cfg.resume_level == 0)
                    items += " selected";
                items += ">low</option><option value=\"mid\"";
                if (cfg.resume_level == 1)
                    items += " selected";
                items += ">mid</option><option value=\"full\"";
                if (cfg.resume_level == 2)
                    items += " selected";
                items += ">full</option></select></div>";
            }
            items += "</div></div></div>";
        };
    
        const size_t render_count = web.wateringLocalRenderCount_();
        const size_t page_limit = (limit == 0) ? 1u : limit;
        const bool can_view_disabled = web.webSessionIsAdmin_();
        size_t rendered = 0;
        size_t visible_idx = 0;
        for (size_t i = 0; i < render_count; ++i)
        {
            if (rendered >= page_limit)
                break;
            if (!scratch->watering_valid[i])
                continue;
            if (!web.webAclCanViewItem_(UsersRegistry::AclController::Watering, scratch->watering_cfg[i].id))
                continue;
            if (!can_view_disabled && !scratch->watering_cfg[i].enabled)
                continue;
            if (visible_idx < offset)
            {
                ++visible_idx;
                continue;
            }
            ++visible_idx;
            appendRule(scratch->watering_cfg[i], scratch->watering_st[i]);
            ++rendered;
        }
        return items;
    }

String WebInterfaceControllersWateringHelper::listWateringHtml_(WebInterface &web) {
        return listWateringHtml_(web, 0u, SIZE_MAX);
    }

String WebInterfaceControllersWateringHelper::wateringPortOptionsJson_(const WebInterface &web) {
        return web.socketPortOptionsJson_(PortIO::PinType::Relay);
    }

String WebInterfaceControllersWateringHelper::wateringTankOptionsJson_(const WebInterface &web) {
        String out;
        out.reserve(256);
        out += "[";
        bool first = true;
        if (web._controllers)
        {
            auto scratch_guard = web.scratchLockGuard_();
            WebInterface::ScratchBuffer *scratch = (scratch_guard.locked() ? web.scratchBuffer_() : nullptr);
            if (!scratch)
                return "[]";
            const TankController &tanks = web._controllers->tanks();
            loadLocalWateringTankItems_(tanks, *scratch, TankController::kTankCount);
            for (size_t i = 0; i < TankController::kTankCount; ++i)
            {
                if (!scratch->tank_valid[i])
                    continue;
                const auto &cfg = scratch->tank_cfg[i];
                if (!first)
                    out += ",";
                out += "{\"v\":";
                out += String((unsigned)cfg.id);
                out += ",\"l\":\"";
                if (cfg.name.length())
                    web.appendJsonEscaped_(out, cfg.name);
                else
                    out += String("Tank #") + String((unsigned)cfg.id);
                out += "\"}";
                first = false;
            }
        }
        out += "]";
        return out;
    }

String WebInterfaceControllersWateringHelper::stackWateringTankOptionsJson_(const WebInterface &web, uint32_t node_id) {
        if (!web.network() || node_id == 0)
            return "[]";
        StackUnitSnapshot::CacheState cache{};
        if (!web.network()->stackIndexCacheState(node_id, cache) || cache.tank_count == 0)
            return "[]";
        String out;
        out.reserve(256);
        out += "[";
        bool first = true;
        web.network()->forEachStackTank(node_id, cache.tank_count, [&](uint8_t, const StackUnitSnapshot::TankItem &cfg) {
            if (!cfg.enabled)
                return;
            if (!first)
                out += ",";
            out += "{\"v\":";
            out += String((unsigned)cfg.id);
            out += ",\"l\":\"";
            if (cfg.name[0])
                web.appendJsonEscaped_(out, String(cfg.name));
            else
                out += String("Tank #") + String((unsigned)cfg.id);
            out += "\"}";
            first = false;
        });
        out += "]";
        return out;
    }

size_t WebInterface::wateringLocalRenderCount_() const {
        return WebInterfaceControllersWateringHelper::wateringLocalRenderCount_(*this);
    }

String WebInterface::wateringDeviceSelectHtml_(uint32_t selected_node_id, bool stack_view) const {
        return WebInterfaceControllersWateringHelper::wateringDeviceSelectHtml_(*this, selected_node_id, stack_view);
    }

String WebInterface::stackWateringStatusText_(uint32_t node_id) const {
        return WebInterfaceControllersWateringHelper::stackWateringStatusText_(*this, node_id);
    }

bool WebInterface::isStackWateringView_(uint32_t node_id) const {
        return WebInterfaceControllersWateringHelper::isStackWateringView_(*this, node_id);
    }

bool WebInterface::requestStackWatering_(uint32_t node_id) {
        return WebInterfaceControllersWateringHelper::requestStackWatering_(*this, node_id);
    }

size_t WebInterface::stackWateringVisibleCount_(uint32_t node_id) const {
        return WebInterfaceControllersWateringHelper::stackWateringVisibleCount_(*this, node_id);
    }

String WebInterface::listStackWateringHtml_(uint32_t node_id, size_t offset, size_t limit) {
        return WebInterfaceControllersWateringHelper::listStackWateringHtml_(*this, node_id, offset, limit);
    }

String WebInterface::listWateringHtml_() {
        return WebInterfaceControllersWateringHelper::listWateringHtml_(*this);
    }

String WebInterface::listWateringHtml_(size_t offset, size_t limit) {
        return WebInterfaceControllersWateringHelper::listWateringHtml_(*this, offset, limit);
    }

String WebInterface::wateringPortOptionsJson_() const {
        return WebInterfaceControllersWateringHelper::wateringPortOptionsJson_(*this);
    }

String WebInterface::wateringTankOptionsJson_() const {
        return WebInterfaceControllersWateringHelper::wateringTankOptionsJson_(*this);
    }

String WebInterface::stackWateringTankOptionsJson_(uint32_t node_id) const {
        return WebInterfaceControllersWateringHelper::stackWateringTankOptionsJson_(*this, node_id);
    }


