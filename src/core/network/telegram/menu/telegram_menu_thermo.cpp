#include "core/network/telegram/telegram_menu.hpp"

#include <stdlib.h>
#include <string.h>

#include "controllers/thermo_controller.hpp"
#include "core/network/stack/stack_cache.hpp"
#include "core/network/stack/stack_features.hpp"
#include "core/network/stack/stack_master.hpp"
#include "core/network/wifi_manager.hpp"
#include "core/rtc.hpp"
#include "plc/plc_control.hpp"
#include "utils/configs.hpp"
#include "utils/logger.hpp"

#include "core/network/telegram/menu/telegram_menu_thermo.hpp"


// ---- telegram menu extracted definitions ----
bool TelegramMenuThermo::cmdThermo_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        (void)reply;
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        if (TelegramMenu::_self->isLocalSelected_(u.chat_id) && !TelegramMenu::_self->_thermo)
        {
            reply = "Термо недоступно";
            return true;
        }
        TelegramMenuThermo::sendThermoMenu_(*TelegramMenu::_self, u.chat_id);
        return true;
    }


    bool TelegramMenuThermo::cmdThermoList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        (void)bot;
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        if (TelegramMenu::_self->isLocalSelected_(u.chat_id) && !TelegramMenu::_self->_thermo)
        {
            reply = "Термо недоступно";
            return true;
        }
        reply = TelegramMenuThermo::thermoListTextHtml_(*TelegramMenu::_self, u.chat_id);
        return true;
    }


    bool TelegramMenuThermo::cmdThermoShow_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        if (TelegramMenu::_self->isLocalSelected_(u.chat_id) && !TelegramMenu::_self->_thermo)
        {
            reply = "Термо недоступно";
            return true;
        }
        const char *cmd = "/thermo_show";
        String tail = u.text.substring(strlen(cmd));
        tail.trim();
        if (tail.length() == 0)
        {
            reply = "Использование: /thermo_show <id>";
            return true;
        }
        uint8_t id = 0;
        if (!TelegramMenuThermo::parseThermoId_(tail, id))
        {
            reply = "Неверный ID термо";
            return true;
        }
        TelegramMenuThermo::sendThermoDevice_(*TelegramMenu::_self, u.chat_id, id);
        return true;
    }


    void TelegramMenuThermo::buildThermoLabels_(TelegramMenu &self, std::vector<String> &out)
    {
        out.clear();
        if (!self._thermo)
        {
            out.reserve(1);
            out.push_back(F("Назад"));
            return;
        }
        out.reserve(ThermoController::kDeviceCount + 1);
        for (size_t i = 0; i < ThermoController::kDeviceCount; ++i)
        {
            const auto *cfg = self._thermo->configByIndex(i);
            if (!cfg || !cfg->enabled)
                continue;
            String label;
            label += String((unsigned)cfg->id);
            label += ": ";
            if (cfg->name.length())
            {
                label += cfg->name;
            }
            out.push_back(label);
        }
        out.push_back(F("Назад"));
    }


    String TelegramMenuThermo::thermoListTextHtml_(TelegramMenu &self, int64_t chat_id)
    {
        const bool groups_enabled = self.isLocalSelected_(chat_id) ? (self._thermo && self.hasLocalGroups_())
                                                                   : (self._stack_cache && self.hasGroups_(chat_id));
        const bool group_active = groups_enabled && self.groupFilterActive_(chat_id, "thermo");
        String out = F("<b>Термо:</b>");
        out.reserve(768);
        if (self.isLocalSelected_(chat_id))
        {
            if (!self._thermo)
            {
                out += F("\n  недоступно");
                return out;
            }
            if (groups_enabled && !group_active)
            {
                bool any = false;
                bool has_no_group = false;
                for (size_t gi = 0; gi < self._configs_manager->groupCount(); ++gi)
                {
                    ConfigsManagerIface::GroupConfig g;
                    if (!self._configs_manager->groupByIndex(gi, g) || g.id == 0)
                        continue;
                    bool group_any = false;
                    for (size_t i = 0; i < ThermoController::kDeviceCount; ++i)
                    {
                        const auto *cfg = self._thermo->configByIndex(i);
                        const auto *st = self._thermo->stateByIndex(i);
                        if (!cfg || !st || !cfg->enabled)
                            continue;
                        if (cfg->group_id == 0)
                            has_no_group = true;
                        if (cfg->group_id != g.id)
                            continue;
                        if (!group_any)
                        {
                            out += F("\n  [");
                            out += self.escapeHtml_(g.name);
                            out += F("]");
                            group_any = true;
                            any = true;
                        }
                        out += F("\n    ");
                        out += String((unsigned)cfg->id);
                        out += F(": ");
                        if (cfg->name.length())
                        {
                            out += "<b>";
                            out += self.escapeHtml_(cfg->name);
                            out += "</b>";
                        }
                        else
                        {
                            out += "<b>-</b>";
                        }
                        out += "\n      режим: <b>";
                        out += TelegramMenuThermo::thermoModeLabel_(cfg->mode);
                        out += "</b>";
                        out += "\n      питание: <b>";
                        out += st->power_on ? F("🟢") : F("⚪");
                        out += "</b>";
                        out += "\n      статус: <b>";
                        if (st->heat_on)
                            out += F("🔥");
                        else if (st->cool_on)
                            out += F("❄️");
                        else
                            out += F("⏳");
                        out += "</b>";
                    }
                }
                if (has_no_group)
                {
                    out += F("\n  [Без группы]");
                    any = true;
                    for (size_t i = 0; i < ThermoController::kDeviceCount; ++i)
                    {
                        const auto *cfg = self._thermo->configByIndex(i);
                        const auto *st = self._thermo->stateByIndex(i);
                        if (!cfg || !st || !cfg->enabled || cfg->group_id != 0)
                            continue;
                        out += F("\n    ");
                        out += String((unsigned)cfg->id);
                        out += F(": ");
                        if (cfg->name.length())
                        {
                            out += "<b>";
                            out += self.escapeHtml_(cfg->name);
                            out += "</b>";
                        }
                        else
                        {
                            out += "<b>-</b>";
                        }
                        out += "\n      режим: <b>";
                        out += TelegramMenuThermo::thermoModeLabel_(cfg->mode);
                        out += "</b>";
                        out += "\n      питание: <b>";
                        out += st->power_on ? F("🟢") : F("⚪");
                        out += "</b>";
                        out += "\n      статус: <b>";
                        if (st->heat_on)
                            out += F("🔥");
                        else if (st->cool_on)
                            out += F("❄️");
                        else
                            out += F("⏳");
                        out += "</b>";
                    }
                }
                if (!any)
                    out += F("\n  пусто");
                return out;
            }
            if (groups_enabled && group_active)
            {
                out += F("\n  группа: <b>");
                out += self.escapeHtml_(self.groupLabelById_(chat_id, self.groupFilterId_(chat_id, "thermo")));
                out += F("</b>");
            }
            bool any = false;
            const uint8_t active_group_id = groups_enabled ? self.groupFilterId_(chat_id, "thermo") : 0;
            for (size_t i = 0; i < ThermoController::kDeviceCount; ++i)
            {
                const auto *cfg = self._thermo->configByIndex(i);
                const auto *st = self._thermo->stateByIndex(i);
                if (!cfg || !st || !cfg->enabled)
                    continue;
                if (groups_enabled && cfg->group_id != active_group_id)
                    continue;
                any = true;
                out += "\n  ";
                out += String((unsigned)cfg->id);
                out += ": ";
                if (cfg->name.length())
                {
                    out += "<b>";
                    out += self.escapeHtml_(cfg->name);
                    out += "</b>";
                }
                else
                {
                    out += "<b>-</b>";
                }
                out += "\n   режим: <b>";
                out += TelegramMenuThermo::thermoModeLabel_(cfg->mode);
                out += "</b>";
                out += "\n   питание: <b>";
                out += st->power_on ? F("🟢") : F("⚪");
                out += "</b>";
                out += "\n   статус: <b>";
                if (st->heat_on)
                    out += F("🔥");
                else if (st->cool_on)
                    out += F("❄️");
                else
                    out += F("⏳");
                out += "</b>";
            }
            if (!any)
                out += F("\n  пусто");
            return out;
        }
        if (!self._stack_cache)
        {
            out += F("\n  недоступно");
            return out;
        }
        const uint32_t node_id = self.selectedNodeId_(chat_id);
        if (node_id == 0)
        {
            out += F("\n  недоступно");
            return out;
        }
        const auto *cache = self._stack_cache->thermoCache(node_id);
        if (!cache || !cache->has_data)
        {
            self._stack_cache->requestThermo(node_id);
            out += F("\n  обновление...");
            return out;
        }
        if (groups_enabled && !group_active)
        {
            bool any_group = false;
            bool has_no_group = false;
            const auto *groups = self.selectedGroupsCache_(chat_id);
            if (groups && groups->has_data && groups->items)
            {
                for (size_t gi = 0; gi < groups->item_count; ++gi)
                {
                    const auto &g = groups->items[gi];
                    if (g.id == 0 || !g.name[0])
                        continue;
                    bool section = false;
                    for (size_t i = 0; i < cache->item_count; ++i)
                    {
                        const auto &it = cache->items[i];
                        if (!it.enabled)
                            continue;
                        if (it.group_id == 0)
                            has_no_group = true;
                        if (it.group_id != g.id)
                            continue;
                        if (!section)
                        {
                            out += F("\n  [");
                            out += self.escapeHtml_(String(g.name));
                            out += F("]");
                            section = true;
                            any_group = true;
                        }
                        out += "\n  ";
                        out += String((unsigned)it.id);
                        out += ": ";
                        out += it.name[0] ? String("<b>") + self.escapeHtml_(String(it.name)) + "</b>" : String("<b>-</b>");
                        out += "\n   РїРёС‚Р°РЅРёРµ: <b>";
                        out += it.power_on ? "рџџў" : "вљЄ";
                        out += "</b>\n   СЃС‚Р°С‚СѓСЃ: <b>";
                        if (it.heat_on)
                            out += "рџ”Ґ";
                        else if (it.cool_on)
                            out += "вќ„пёЏ";
                        else
                            out += "вЏі";
                        out += "</b>";
                    }
                }
            }
            if (has_no_group)
            {
                out += F("\n  [Р‘РµР· РіСЂСѓРїРїС‹]");
                any_group = true;
                for (size_t i = 0; i < cache->item_count; ++i)
                {
                    const auto &it = cache->items[i];
                    if (!it.enabled || it.group_id != 0)
                        continue;
                    out += "\n  ";
                    out += String((unsigned)it.id);
                    out += ": ";
                    out += it.name[0] ? String("<b>") + self.escapeHtml_(String(it.name)) + "</b>" : String("<b>-</b>");
                    out += "\n   РїРёС‚Р°РЅРёРµ: <b>";
                    out += it.power_on ? "рџџў" : "вљЄ";
                    out += "</b>\n   СЃС‚Р°С‚СѓСЃ: <b>";
                    if (it.heat_on)
                        out += "рџ”Ґ";
                    else if (it.cool_on)
                        out += "вќ„пёЏ";
                    else
                        out += "вЏі";
                    out += "</b>";
                }
            }
            if (!any_group)
                out += F("\n  РїСѓСЃС‚Рѕ");
            return out;
        }
        if (groups_enabled && group_active)
        {
            out += F("\n  РіСЂСѓРїРїР°: <b>");
            out += self.escapeHtml_(self.groupLabelById_(chat_id, self.groupFilterId_(chat_id, "thermo")));
            out += F("</b>");
        }
        bool any = false;
        for (size_t i = 0; i < cache->item_count; ++i)
        {
            const auto &it = cache->items[i];
            if (!it.enabled)
                continue;
            if (groups_enabled && it.group_id != self.groupFilterId_(chat_id, "thermo"))
                continue;
            any = true;
            out += "\n  ";
            out += String((unsigned)it.id);
            out += ": ";
            out += it.name[0] ? String("<b>") + self.escapeHtml_(String(it.name)) + "</b>" : String("<b>-</b>");
            out += "\n   режим: <b>";
            if (strcmp(it.mode, "heat") == 0)
                out += "🔥";
            else if (strcmp(it.mode, "cool") == 0)
                out += "❄️";
            else if (strcmp(it.mode, "auto") == 0)
                out += "🤖";
            else
                out += "Выкл";
            out += "</b>";
            out += "\n   питание: <b>";
            out += it.power_on ? "🟢" : "⚪";
            out += "</b>";
            out += "\n   статус: <b>";
            if (it.heat_on)
                out += "🔥";
            else if (it.cool_on)
                out += "❄️";
            else
                out += "⏳";
            out += "</b>";
        }
        if (!any)
            out += F("\n  пусто");
        return out;
    }


    String TelegramMenuThermo::thermoDeviceTextHtml_(TelegramMenu &self, int64_t chat_id, uint8_t id)
    {
        if (self.isLocalSelected_(chat_id))
        {
            if (!self._thermo)
                return F("Термо недоступно");
            const auto *cfg = self._thermo->config(id);
            const auto *st = self._thermo->state(id);
            if (!cfg || !st)
                return F("Неверное устройство");
            String out = F("<b>Термо устройство:</b>");
            out.reserve(512);
            out += "\n  имя: ";
            if (cfg->name.length())
            {
                out += "<b>";
                out += self.escapeHtml_(cfg->name);
                out += "</b>";
            }
            else
            {
                out += "<b>-</b>";
            }
            out += "\n  режим: ";
            out += "<b>";
            out += TelegramMenuThermo::thermoModeLabel_(cfg->mode);
            out += "</b>";
            out += "\n  темп: ";
            if (self._meteo && cfg->sensor_id)
            {
                const auto *st = self._meteo->state(cfg->sensor_id);
                if (st && st->has_temp)
                {
                    char buf[10] = {};
                    dtostrf(st->temp_c, 0, 1, buf);
                    out += "<b>";
                    out += buf;
                    out += "°";
                    out += "</b>";
                }
                else
                {
                    out += "-";
                }
            }
            else
            {
                out += "-";
            }
            out += "\n  цель: ";
            out += "<b>";
            out += String(cfg->target_c, 1);
            out += "°";
            out += "</b>";
            out += "\n  гист: ";
            out += "<b>";
            out += String(cfg->hysteresis, 1);
            out += "°";
            out += "</b>";
            out += "\n  статус: ";
            out += "<b>";
            if (st->heat_on)
                out += F("🔥");
            else if (st->cool_on)
                out += F("❄️");
            else
                out += F("⏳");
            out += "</b>";
            out += "\n  питание: ";
            out += "<b>";
            out += st->power_on ? F("🟢") : F("⚪");
            out += "</b>";
            return out;
        }

        if (!self._stack_cache)
            return F("Термо недоступно");
        const uint32_t node_id = self.selectedNodeId_(chat_id);
        if (node_id == 0)
            return F("Термо недоступно");
        const auto *cache = self._stack_cache->thermoCache(node_id);
        if (!cache || !cache->has_data)
        {
            self._stack_cache->requestThermo(node_id);
            return F("Обновление данных...");
        }
        const StackCache::StackThermoItem *found = nullptr;
        for (size_t i = 0; i < cache->item_count; ++i)
            if (cache->items[i].id == id && cache->items[i].enabled)
            {
                found = &cache->items[i];
                break;
            }
        if (!found)
            return F("Неверное устройство");
        String out = F("<b>Термо устройство:</b>");
        out.reserve(420);
        out += "\n  имя: <b>";
        out += found->name[0] ? self.escapeHtml_(String(found->name)) : String("-");
        out += "</b>";
        out += "\n  режим: <b>";
        if (strcmp(found->mode, "heat") == 0)
            out += "🔥";
        else if (strcmp(found->mode, "cool") == 0)
            out += "❄️";
        else if (strcmp(found->mode, "auto") == 0)
            out += "🤖";
        else
            out += "Выкл";
        out += "</b>";
        out += "\n  темп: -";
        out += "\n  цель: ";
        out += "<b>";
        out += String(found->target, 1);
        out += "°";
        out += "</b>";
        out += "\n  гист: ";
        out += "<b>";
        out += String(found->hyst, 1);
        out += "°";
        out += "</b>";
        out += "\n  статус: ";
        out += "<b>";
        if (found->heat_on)
            out += F("🔥");
        else if (found->cool_on)
            out += F("❄️");
        else
            out += F("⏳");
        out += "</b>";
        out += "\n  питание: ";
        out += "<b>";
        out += found->power_on ? F("🟢") : F("⚪");
        out += "</b>";
        return out;
    }


    const char *TelegramMenuThermo::thermoModeLabel_(ThermoController::Mode mode)
    {
        switch (mode)
        {
        case ThermoController::Mode::Heat:
            return "🔥";
        case ThermoController::Mode::Cool:
            return "❄️";
        case ThermoController::Mode::Auto:
            return "🤖";
        case ThermoController::Mode::Off:
        default:
            return "Выкл";
        }
    }


    void TelegramMenuThermo::sendThermoMenu_(TelegramMenu &self, int64_t chat_id)
    {
        if (!self._bot)
            return;
        TelegramMenu::ChatAuth *st = self.ensureAuth_(chat_id);
        if (st)
        {
            st->awaiting_thermo = false;
            st->selected_thermo_id = 0;
        }
        std::vector<String> labels;
        if (self.isLocalSelected_(chat_id))
        {
            const bool groups_enabled = self._thermo && self.hasLocalGroups_();
            const bool group_active = groups_enabled && self.groupFilterActive_(chat_id, "thermo");
            if (groups_enabled && !group_active)
            {
                bool has_no_group = false;
                for (size_t gi = 0; gi < self._configs_manager->groupCount(); ++gi)
                {
                    ConfigsManagerIface::GroupConfig g;
                    if (!self._configs_manager->groupByIndex(gi, g) || g.id == 0)
                        continue;
                    bool present = false;
                    for (size_t i = 0; i < ThermoController::kDeviceCount; ++i)
                    {
                        const auto *cfg = self._thermo->configByIndex(i);
                        if (!cfg || !cfg->enabled)
                            continue;
                        if (cfg->group_id == g.id)
                        {
                            present = true;
                            break;
                        }
                        if (cfg->group_id == 0)
                            has_no_group = true;
                    }
                    if (present)
                        labels.push_back(g.name);
                }
                if (has_no_group)
                    labels.push_back(F("Без группы"));
                labels.push_back(F("Назад"));
            }
            else
            {
                const uint8_t active_group_id = groups_enabled ? self.groupFilterId_(chat_id, "thermo") : 0;
                for (size_t i = 0; i < ThermoController::kDeviceCount; ++i)
                {
                    const auto *cfg = self._thermo->configByIndex(i);
                    if (!cfg || !cfg->enabled)
                        continue;
                    if (groups_enabled && cfg->group_id != active_group_id)
                        continue;
                    String label = String((unsigned)cfg->id) + ": ";
                    label += cfg->name.length() ? cfg->name : String("-");
                    labels.push_back(label);
                }
                if (groups_enabled)
                    labels.push_back(F("Группы"));
                labels.push_back(F("Назад"));
            }
        }
        else if (self._stack_cache)
        {
            const uint32_t node_id = self.selectedNodeId_(chat_id);
            const bool groups_enabled = self.hasGroups_(chat_id);
            const bool group_active = groups_enabled && self.groupFilterActive_(chat_id, "thermo");
            if (node_id != 0)
            {
                const auto *cache = self._stack_cache->thermoCache(node_id);
                if (groups_enabled && !group_active)
                {
                    bool has_no_group = false;
                    const auto *groups = self.selectedGroupsCache_(chat_id);
                    if (!cache || !cache->has_data)
                    {
                        self._stack_cache->requestThermo(node_id);
                    }
                    else if (groups && groups->has_data && groups->items)
                    {
                        for (size_t gi = 0; gi < groups->item_count; ++gi)
                        {
                            const auto &g = groups->items[gi];
                            if (g.id == 0 || !g.name[0])
                                continue;
                            bool present = false;
                            for (size_t i = 0; i < cache->item_count; ++i)
                            {
                                const auto &it = cache->items[i];
                                if (!it.enabled)
                                    continue;
                                if (it.group_id == g.id)
                                {
                                    present = true;
                                    break;
                                }
                                if (it.group_id == 0)
                                    has_no_group = true;
                            }
                            if (present)
                                labels.push_back(String(g.name));
                        }
                    }
                    if (has_no_group)
                        labels.push_back(F("Р‘РµР· РіСЂСѓРїРїС‹"));
                }
                else if (!cache || !cache->has_data)
                {
                    self._stack_cache->requestThermo(node_id);
                }
                else
                {
                    const uint8_t active_group_id = groups_enabled ? self.groupFilterId_(chat_id, "thermo") : 0;
                    for (size_t i = 0; i < cache->item_count; ++i)
                    {
                        const auto &it = cache->items[i];
                        if (!it.enabled)
                            continue;
                        if (groups_enabled && it.group_id != active_group_id)
                            continue;
                        String label = String((unsigned)it.id) + ": ";
                        label += it.name[0] ? String(it.name) : String("-");
                        labels.push_back(label);
                    }
                    if (groups_enabled)
                        labels.push_back(F("Р“СЂСѓРїРїС‹"));
                }
            }
            labels.push_back(F("Назад"));
        }
        else
        {
            labels.push_back(F("Назад"));
        }
        const String markup = TelegramMenu::buildKeyboardMarkup_(labels);
        const String list = TelegramMenuThermo::thermoListTextHtml_(self, chat_id);
        self._bot->setMenu(chat_id, "thermo");
        self._bot->sendText(chat_id, list, markup, "HTML");
    }


    void TelegramMenuThermo::sendThermoDevice_(TelegramMenu &self, int64_t chat_id, uint8_t id)
    {
        if (!self._bot)
            return;
        if (self.isLocalSelected_(chat_id) && !self._thermo)
        {
            self._bot->sendText(chat_id, F("Термо недоступно"));
            return;
        }
        TelegramMenu::ChatAuth *st = self.ensureAuth_(chat_id);
        if (st)
        {
            st->awaiting_thermo = true;
            st->selected_thermo_id = id;
        }
        const String text = TelegramMenuThermo::thermoDeviceTextHtml_(self, chat_id, id);
        const String markup = TelegramMenuThermo::thermoControlMarkup_();
        self._bot->sendText(chat_id, text, markup, "HTML");
    }


    String TelegramMenuThermo::thermoControlMarkup_()
    {
        std::vector<String> labels;
        labels.reserve(10);
        labels.push_back(F("Темп +"));
        labels.push_back(F("Темп -"));
        labels.push_back(F("Гист +"));
        labels.push_back(F("Гист -"));
        labels.push_back(F("Питание 🟢"));
        labels.push_back(F("Питание ⚪"));
        labels.push_back(F("Режим 🤖"));
        labels.push_back(F("Режим 🔥"));
        labels.push_back(F("Режим ❄️"));
        labels.push_back(F("Назад"));
        return TelegramMenu::buildKeyboardMarkup_(labels);
    }


    bool TelegramMenuThermo::handleThermoAction_(TelegramMenu &self, const TelegramClient::Update &u)
    {
        TelegramMenu::ChatAuth *st = self.findAuth_(u.chat_id);
        if (!st || !st->awaiting_thermo)
            return false;
        if (u.text.startsWith("/"))
            return false;
        if (u.text == F("Назад"))
        {
            st->awaiting_thermo = false;
            st->selected_thermo_id = 0;
            TelegramMenuThermo::sendThermoMenu_(self, u.chat_id);
            return true;
        }
        if (self.isLocalSelected_(u.chat_id) && !self._thermo)
        {
            self._bot->sendText(u.chat_id, F("Термо недоступно"));
            return true;
        }
        const uint8_t id = st->selected_thermo_id;
        if (id == 0)
        {
            self._bot->sendText(u.chat_id, F("Не выбрано устройство"));
            return true;
        }
        bool handled = true;
        if (!self.isLocalSelected_(u.chat_id))
        {
            const uint32_t node_id = self.selectedNodeId_(u.chat_id);
            if (node_id == 0 || !self._stack_master)
                handled = false;
            else
            {
                float cur_target = 0.0f;
                float cur_hyst = 0.0f;
                bool has_cur = false;
                if (self._stack_cache)
                {
                    const auto *cache = self._stack_cache->thermoCache(node_id);
                    if (cache && cache->has_data)
                    {
                        for (size_t i = 0; i < cache->item_count; ++i)
                        {
                            const auto &it = cache->items[i];
                            if (it.id != id || !it.enabled)
                                continue;
                            cur_target = it.target;
                            cur_hyst = it.hyst;
                            has_cur = true;
                            break;
                        }
                    }
                }
                DynamicJsonDocument doc(256);
                doc["feature"] = (uint8_t)StackFeature::Thermo;
                doc["action"] = "set";
                JsonObject params = doc["params"].to<JsonObject>();
                JsonArray items = params["items"].to<JsonArray>();
                JsonObject it = items.add<JsonObject>();
                it["id"] = (unsigned)id;
                if (u.text == F("Питание 🟢") || u.text == F("Питание Вкл"))
                {
                    it["power"] = true;
                }
                else if (u.text == F("Питание ⚪") || u.text == F("Питание 🔴") || u.text == F("Питание Выкл"))
                {
                    it["power"] = false;
                }
                else if (u.text == F("Режим 🤖") || u.text == F("Авто"))
                {
                    it["mode"] = "auto";
                }
                else if (u.text == F("Режим 🔥") || u.text == F("Статус 🔥") || u.text == F("Нагрев"))
                {
                    it["mode"] = "heat";
                }
                else if (u.text == F("Режим ❄️") || u.text == F("Статус ❄️") || u.text == F("Охлаждение"))
                {
                    it["mode"] = "cool";
                }
                else if (u.text == F("Темп +"))
                {
                    if (!has_cur)
                    {
                        if (self._stack_cache)
                            self._stack_cache->requestThermo(node_id);
                        self._bot->sendText(u.chat_id, F("Нет актуальных данных. Повторите через пару секунд."));
                        return true;
                    }
                    it["target"] = cur_target + TelegramMenu::kThermoTargetStep;
                }
                else if (u.text == F("Темп -"))
                {
                    if (!has_cur)
                    {
                        if (self._stack_cache)
                            self._stack_cache->requestThermo(node_id);
                        self._bot->sendText(u.chat_id, F("Нет актуальных данных. Повторите через пару секунд."));
                        return true;
                    }
                    it["target"] = cur_target - TelegramMenu::kThermoTargetStep;
                }
                else if (u.text == F("Гист +"))
                {
                    if (!has_cur)
                    {
                        if (self._stack_cache)
                            self._stack_cache->requestThermo(node_id);
                        self._bot->sendText(u.chat_id, F("Нет актуальных данных. Повторите через пару секунд."));
                        return true;
                    }
                    it["hyst"] = cur_hyst + TelegramMenu::kThermoHystStep;
                }
                else if (u.text == F("Гист -"))
                {
                    if (!has_cur)
                    {
                        if (self._stack_cache)
                            self._stack_cache->requestThermo(node_id);
                        self._bot->sendText(u.chat_id, F("Нет актуальных данных. Повторите через пару секунд."));
                        return true;
                    }
                    float h = cur_hyst - TelegramMenu::kThermoHystStep;
                    if (h < 0.0f)
                        h = 0.0f;
                    it["hyst"] = h;
                }
                else
                {
                    handled = false;
                }
                if (handled)
                {
                    char payload[256] = {};
                    const size_t n = serializeJson(doc, payload, sizeof(payload));
                    handled = (n > 0) && self._stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                                                                     reinterpret_cast<const uint8_t *>(payload), n);
                    if (handled && self._stack_cache)
                        self._stack_cache->requestThermo(node_id);
                }
            }
        }
        else if (u.text == F("Темп +"))
        {
            const auto *cfg = self._thermo->config(id);
            if (cfg)
                self._thermo->setTarget(id, cfg->target_c + TelegramMenu::kThermoTargetStep);
        }
        else if (u.text == F("Темп -"))
        {
            const auto *cfg = self._thermo->config(id);
            if (cfg)
                self._thermo->setTarget(id, cfg->target_c - TelegramMenu::kThermoTargetStep);
        }
        else if (u.text == F("Гист +"))
        {
            const auto *cfg = self._thermo->config(id);
            if (cfg)
                self._thermo->setHysteresis(id, cfg->hysteresis + TelegramMenu::kThermoHystStep);
        }
        else if (u.text == F("Гист -"))
        {
            const auto *cfg = self._thermo->config(id);
            if (cfg)
            {
                float h = cfg->hysteresis - TelegramMenu::kThermoHystStep;
                if (h < 0.0f)
                    h = 0.0f;
                self._thermo->setHysteresis(id, h);
            }
        }
        else if (u.text == F("Питание 🟢") || u.text == F("Питание Вкл"))
        {
            self._thermo->setPower(id, true, "tgbot");
        }
        else if (u.text == F("Питание ⚪") || u.text == F("Питание 🔴") || u.text == F("Питание Выкл"))
        {
            self._thermo->setPower(id, false, "tgbot");
        }
        else if (u.text == F("Режим 🤖") || u.text == F("Авто"))
        {
            self._thermo->setMode(id, ThermoController::Mode::Auto);
        }
        else if (u.text == F("Режим 🔥") || u.text == F("Статус 🔥") || u.text == F("Нагрев"))
        {
            self._thermo->setMode(id, ThermoController::Mode::Heat);
        }
        else if (u.text == F("Режим ❄️") || u.text == F("Статус ❄️") || u.text == F("Охлаждение"))
        {
            self._thermo->setMode(id, ThermoController::Mode::Cool);
        }
        else
        {
            handled = false;
        }
        if (!handled)
        {
            self._bot->sendText(u.chat_id, F("Неизвестная команда"));
            return true;
        }
        TelegramMenuThermo::sendThermoDevice_(self, u.chat_id, id);
        return true;
    }


    bool TelegramMenuThermo::handleThermoSelection_(TelegramMenu &self, const TelegramClient::Update &u)
    {
        if (!self._bot)
            return false;
        const char *menu_id = self._bot->currentMenuId(u.chat_id);
        if (!menu_id || strcmp(menu_id, "thermo") != 0)
            return false;
        if (u.text.startsWith("/"))
            return false;
        if (u.text == F("Назад"))
        {
            self.clearGroupFilter_(u.chat_id, "thermo");
            self._bot->enterMenu(u.chat_id, "device");
            return true;
        }
        if (((self.isLocalSelected_(u.chat_id) && self._thermo) || (!self.isLocalSelected_(u.chat_id) && self._stack_cache)) &&
            self.hasGroups_(u.chat_id))
        {
            if (!self.groupFilterActive_(u.chat_id, "thermo"))
            {
                uint8_t group_id = 0;
                if (!self.parseGroupLabel_(u.chat_id, u.text, group_id))
                {
                    self._bot->sendText(u.chat_id, F("Неизвестная группа"));
                    return true;
                }
                self.setGroupFilter_(u.chat_id, "thermo", group_id);
                TelegramMenuThermo::sendThermoMenu_(self, u.chat_id);
                return true;
            }
            if (u.text == F("Группы"))
            {
                self.clearGroupFilter_(u.chat_id, "thermo");
                TelegramMenuThermo::sendThermoMenu_(self, u.chat_id);
                return true;
            }
        }
        uint8_t id = 0;
        if (!TelegramMenuThermo::parseThermoLabel_(u.text, id))
        {
            self._bot->sendText(u.chat_id, F("Неизвестное устройство"));
            return true;
        }
        if (self.isLocalSelected_(u.chat_id) && !self._thermo)
        {
            self._bot->sendText(u.chat_id, F("Термо недоступно"));
            return true;
        }
        TelegramMenuThermo::sendThermoDevice_(self, u.chat_id, id);
        return true;
    }


    bool TelegramMenuThermo::parseThermoIdFromText_(const String &text, uint8_t &out)
    {
        const size_t len = text.length();
        if (len == 0)
            return false;
        int start = -1;
        int end = -1;
        for (size_t i = 0; i < len; ++i)
        {
            const char c = text.charAt(i);
            if (c >= '0' && c <= '9')
            {
                if (start < 0)
                    start = (int)i;
                end = (int)i + 1;
            }
            else if (start >= 0)
            {
                break;
            }
        }
        if (start < 0 || end <= start)
            return false;
        String num = text.substring(start, end);
        return TelegramMenuThermo::parseThermoId_(num, out);
    }


    bool TelegramMenuThermo::parseThermoId_(const String &text, uint8_t &out)
    {
        String t = text;
        t.trim();
        if (t.length() == 0)
            return false;
        for (size_t i = 0; i < t.length(); ++i)
        {
            const char c = t[i];
            if (c < '0' || c > '9')
                return false;
        }
        const int v = t.toInt();
        if (v <= 0 || v > (int)ThermoController::kDeviceCount)
            return false;
        out = (uint8_t)v;
        return true;
    }


    bool TelegramMenuThermo::parseThermoLabel_(const String &text, uint8_t &out)
    {
        String t = text;
        t.trim();
        if (t.length() == 0)
            return false;
        int colon = t.indexOf(':');
        if (colon > 0)
        {
            String head = t.substring(0, colon);
            head.trim();
            return TelegramMenuThermo::parseThermoIdFromText_(head, out);
        }
        String low = t;
        low.toLowerCase();
        if (low.startsWith("thermo"))
        {
            String tail = t.substring(6);
            tail.trim();
            return TelegramMenuThermo::parseThermoIdFromText_(tail, out);
        }
        return TelegramMenuThermo::parseThermoIdFromText_(t, out);
    }
