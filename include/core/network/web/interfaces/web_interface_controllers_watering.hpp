#pragma once

#ifndef WEB_INTERFACE_CLASS_CONTEXT
class WebInterface;
class WebInterfaceControllersWateringHelper;
#else

class WebInterfaceControllersWateringHelper
{
public:
    static size_t wateringLocalRenderCount_(const WebInterface &web)
    {
        if (!web._controllers)
            return 0;
        WateringController &watering = web._controllers->watering();
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
    static String wateringDeviceSelectHtml_(const WebInterface &web, uint32_t selected_node_id, bool stack_view)
    {
        if (web.stackRole_() != ConfigsManagerIface::StackRole::Master || !web._stack_master)
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
    static String stackWateringStatusText_(const WebInterface &web, uint32_t node_id)
    {
            const auto *cache = web._stack_cache->wateringCache(node_id);
            if (!cache)
                return WebUiRu::kNoDataFromSlave;
            if (cache->pending)
                return "";
            if (!cache->last_ok && cache->last_error.length())
            {
                String msg = WebUiRu::kErrorPrefix;
                msg += cache->last_error;
                return msg;
            }
            if (!cache->has_data)
                return WebUiRu::kNoDataFromSlave;
            return WebUiRu::kStatusOk;
        
    }
    static bool isStackWateringView_(const WebInterface &web, uint32_t node_id)
    {
        return node_id != 0 && web._stack_master &&
               web.stackRole_() == ConfigsManagerIface::StackRole::Master;
    }
    static bool requestStackWatering_(WebInterface &web, uint32_t node_id)
    {
        return web._stack_cache && web._stack_cache->requestWatering(node_id);
    }
    static size_t stackWateringVisibleCount_(const WebInterface &web, uint32_t node_id)
    {
        const auto *cache = web._stack_cache ? web._stack_cache->wateringCache(node_id) : nullptr;
        if (!cache || !cache->has_data || !cache->items)
            return 0;
        const bool can_view_disabled = web.webSessionIsAdmin_();
        size_t render_count = cache->item_count;
        if (can_view_disabled)
        {
            size_t last_enabled_idx = SIZE_MAX;
            for (size_t i = 0; i < cache->item_count; ++i)
            {
                if (cache->items[i].enabled)
                    last_enabled_idx = i;
            }
            if (last_enabled_idx == SIZE_MAX)
                render_count = cache->item_count ? 1u : 0u;
            else
            {
                const size_t rc = last_enabled_idx + 2u;
                render_count = rc > cache->item_count ? cache->item_count : rc;
            }
        }
        size_t count = 0;
        for (size_t i = 0; i < render_count; ++i)
        {
            const auto &cfg = cache->items[i];
            if (!web.webAclCanViewItem_(UsersRegistry::AclController::Watering, cfg.id, node_id))
                continue;
            if (!can_view_disabled && !cfg.enabled)
                continue;
            ++count;
        }
        return count;
    }
    static String listStackWateringHtml_(WebInterface &web, uint32_t node_id, size_t offset, size_t limit)
    {
        const auto *cache = web._stack_cache->wateringCache(node_id);
        if (!cache || !cache->has_data)
            return WebUiRu::Watering::kText;
        if (cache->item_count == 0)
            return WebUiRu::Watering::kText2;

        String items;
        const size_t page_limit = (limit == 0) ? 1u : limit;
        size_t reserve = 2048u + page_limit * 920u;
        if (reserve < 8192u)
            reserve = 8192u;
        items.reserve(reserve);
        const bool can_view_disabled = web.webSessionIsAdmin_();
        size_t render_count = cache->item_count;
        if (can_view_disabled)
        {
            size_t last_enabled_idx = SIZE_MAX;
            for (size_t i = 0; i < cache->item_count; ++i)
            {
                if (cache->items[i].enabled)
                    last_enabled_idx = i;
            }
            if (last_enabled_idx == SIZE_MAX)
                render_count = cache->item_count ? 1u : 0u;
            else
            {
                const size_t rc = last_enabled_idx + 2u;
                render_count = rc > cache->item_count ? cache->item_count : rc;
            }
        }
        size_t rendered = 0;
        size_t visible_idx = 0;
        static const uint8_t kWeekdayMap[7] = {2, 3, 4, 5, 6, 7, 1};
        static const char *kWeekdayLabels[7] = {WebUiRu::Watering::kText14, WebUiRu::Watering::kText15, WebUiRu::Watering::kText16, WebUiRu::Watering::kText17, WebUiRu::Watering::kText18, WebUiRu::Watering::kText19, WebUiRu::Watering::kText20};

        for (size_t i = 0; i < render_count; ++i)
        {
            if (rendered >= page_limit)
                break;
            const auto &cfg = cache->items[i];
            if (!web.webAclCanViewItem_(UsersRegistry::AclController::Watering, cfg.id, node_id))
                continue;
            if (!can_view_disabled && !cfg.enabled)
                continue;
            if (visible_idx < offset)
            {
                ++visible_idx;
                continue;
            }
            ++visible_idx;
            const bool can_control = web.webAclCanControlItem_(UsersRegistry::AclController::Watering, cfg.id, node_id);
            const char *state_label = cfg.active ? WebUiRu::Watering::kText3
                                                 : (cfg.paused ? WebUiRu::Watering::kText4 : WebUiRu::Watering::kText5);

            items += "<div class=\"tile\" data-active=\"";
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
                web.appendHtmlEscaped_(items, String(cfg.name).c_str());
                items += "\"><div class=\"form-grid\">";

                items += WebUiRu::Watering::kInputTypeCheckboxNameW;
                items += String((unsigned)cfg.id);
                items += "_status\"";
                if (cfg.status)
                    items += " checked";
                items += "><span class=\"track\"><span class=\"knob\"></span></span></label></div>";

                items += WebUiRu::Watering::kText28;
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
            ++rendered;
        }
        if (items.length() == 0)
            items = WebUiRu::Watering::kText2;
        return items;
    }
    static String listWateringHtml_(WebInterface &web, size_t offset, size_t limit)
    {
        if (!web._controllers)
            return WebUiRu::Watering::kText27;
        String items;
        items.reserve(16384);
        WateringController &watering = web._controllers->watering();
        auto appendRule = [&](const WateringController::RuleConfig &cfg, const WateringController::RuleState &st)
        {
            const bool can_control = web.webAclCanControlItem_(UsersRegistry::AclController::Watering, cfg.id);
            const char *state_label = st.active ? WebUiRu::Watering::kText3 : (st.paused ? WebUiRu::Watering::kText4 : WebUiRu::Watering::kText5);
            items += "<div class=\"tile\" data-active=\"";
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
            const auto *cfg = watering.configByIndex(i);
            const auto *st = watering.stateByIndex(i);
            if (!cfg || !st)
                continue;
            if (!web.webAclCanViewItem_(UsersRegistry::AclController::Watering, cfg->id))
                continue;
            if (!can_view_disabled && !cfg->enabled)
                continue;
            if (visible_idx < offset)
            {
                ++visible_idx;
                continue;
            }
            ++visible_idx;
            appendRule(*cfg, *st);
            ++rendered;
        }
        return items;
    }
    static String listWateringHtml_(WebInterface &web)
    {
        return listWateringHtml_(web, 0u, SIZE_MAX);
    }
    static String wateringPortOptionsJson_(const WebInterface &web)
    {
        return web.socketPortOptionsJson_(PortIO::PinType::Relay);
    }
    static String wateringTankOptionsJson_(const WebInterface &web)
    {
        String out;
        out.reserve(256);
        out += "[";
        bool first = true;
        if (web._controllers)
        {
            const TankController &tanks = web._controllers->tanks();
            for (size_t i = 0; i < TankController::kTankCount; ++i)
            {
                const auto *cfg = tanks.configByIndex(i);
                if (!cfg || !cfg->enabled)
                    continue;
                if (!first)
                    out += ",";
                out += "{\"v\":";
                out += String((unsigned)cfg->id);
                out += ",\"l\":\"";
                if (cfg->name.length())
                    web.appendJsonEscaped_(out, cfg->name);
                else
                    out += String("Tank #") + String((unsigned)cfg->id);
                out += "\"}";
                first = false;
            }
        }
        out += "]";
        return out;
    }
    static String stackWateringTankOptionsJson_(const WebInterface &web, uint32_t node_id)
    {
        String out;
        out.reserve(256);
        out += "[";
        bool first = true;
        if (web._stack_cache)
        {
            const auto *cache = web._stack_cache->tanksCache(node_id);
            if (cache && cache->has_data && cache->items)
            {
                for (size_t i = 0; i < cache->item_count; ++i)
                {
                    const auto &cfg = cache->items[i];
                    if (!cfg.enabled)
                        continue;
                    if (!first)
                        out += ",";
                    out += "{\"v\":";
                    out += String((unsigned)cfg.id);
                    out += ",\"l\":\"";
                    if (cfg.name[0])
                        web.appendJsonEscaped_(out, cfg.name);
                    else
                        out += String("Tank #") + String((unsigned)cfg.id);
                    out += "\"}";
                    first = false;
                }
            }
        }
        out += "]";
        return out;
    }
};

    size_t wateringLocalRenderCount_() const
    {
        return WebInterfaceControllersWateringHelper::wateringLocalRenderCount_(*this);
    }

    String wateringDeviceSelectHtml_(uint32_t selected_node_id, bool stack_view) const
    {
        return WebInterfaceControllersWateringHelper::wateringDeviceSelectHtml_(*this, selected_node_id, stack_view);
    }

    String stackWateringStatusText_(uint32_t node_id) const
    {
        return WebInterfaceControllersWateringHelper::stackWateringStatusText_(*this, node_id);
    }

    bool isStackWateringView_(uint32_t node_id) const
    {
        return WebInterfaceControllersWateringHelper::isStackWateringView_(*this, node_id);
    }

    bool requestStackWatering_(uint32_t node_id)
    {
        return WebInterfaceControllersWateringHelper::requestStackWatering_(*this, node_id);
    }

    size_t stackWateringVisibleCount_(uint32_t node_id) const
    {
        return WebInterfaceControllersWateringHelper::stackWateringVisibleCount_(*this, node_id);
    }

    String listStackWateringHtml_(uint32_t node_id, size_t offset, size_t limit)
    {
        return WebInterfaceControllersWateringHelper::listStackWateringHtml_(*this, node_id, offset, limit);
    }

    String listWateringHtml_()
    {
        return WebInterfaceControllersWateringHelper::listWateringHtml_(*this);
    }
    String listWateringHtml_(size_t offset, size_t limit)
    {
        return WebInterfaceControllersWateringHelper::listWateringHtml_(*this, offset, limit);
    }

    String wateringPortOptionsJson_() const
    {
        return WebInterfaceControllersWateringHelper::wateringPortOptionsJson_(*this);
    }

    String wateringTankOptionsJson_() const
    {
        return WebInterfaceControllersWateringHelper::wateringTankOptionsJson_(*this);
    }
    String stackWateringTankOptionsJson_(uint32_t node_id) const
    {
        return WebInterfaceControllersWateringHelper::stackWateringTankOptionsJson_(*this, node_id);
    }

#endif
