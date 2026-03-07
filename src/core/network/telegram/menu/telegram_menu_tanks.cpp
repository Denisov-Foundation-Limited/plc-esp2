#include "core/network/telegram/telegram_menu.hpp"

#include <stdlib.h>
#include <string.h>

#include "controllers/tank_controller.hpp"
#include "core/network/stack/stack_cache.hpp"
#include "core/network/stack/stack_features.hpp"
#include "core/network/stack/stack_master.hpp"
#include "core/network/wifi_manager.hpp"
#include "core/rtc.hpp"
#include "plc/plc_control.hpp"
#include "utils/configs.hpp"
#include "utils/logger.hpp"

#include "core/network/telegram/menu/telegram_menu_tanks.hpp"


// ---- telegram menu extracted definitions ----
bool TelegramMenuTanks::cmdTanks_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        (void)reply;
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        if (TelegramMenu::_self->isLocalSelected_(u.chat_id) && !TelegramMenu::_self->_tanks)
        {
            reply = "Баки недоступны";
            return true;
        }
        TelegramMenuTanks::sendTanksMenu_(*TelegramMenu::_self, u.chat_id);
        return true;
    }


    bool TelegramMenuTanks::cmdTanksList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        if (TelegramMenu::_self->isLocalSelected_(u.chat_id) && !TelegramMenu::_self->_tanks)
        {
            reply = "Баки недоступны";
            return true;
        }
        const String text = TelegramMenuTanks::tankListTextHtml_(*TelegramMenu::_self, u.chat_id);
        bot.sendText(u.chat_id, text, "", "HTML");
        return true;
    }


    bool TelegramMenuTanks::cmdTanksShow_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        if (TelegramMenu::_self->isLocalSelected_(u.chat_id) && !TelegramMenu::_self->_tanks)
        {
            reply = "Баки недоступны";
            return true;
        }
        const char *cmd = "/tanks_show";
        String tail = u.text.substring(strlen(cmd));
        tail.trim();
        uint8_t id = 0;
        if (!TelegramMenuTanks::parseTankId_(tail, id))
        {
            reply = "Использование: /tanks_show <id>";
            return true;
        }
        const String text = TelegramMenuTanks::tankDeviceTextHtml_(*TelegramMenu::_self, u.chat_id, id);
        bot.sendText(u.chat_id, text, "", "HTML");
        return true;
    }


    void TelegramMenuTanks::buildTankLabels_(TelegramMenu &self, std::vector<String> &out)
    {
        out.clear();
        if (!self._tanks)
        {
            out.reserve(1);
            out.push_back(F("Назад"));
            return;
        }
        out.reserve(TankController::kTankCount + 1);
        for (size_t i = 0; i < TankController::kTankCount; ++i)
        {
            const auto *cfg = self._tanks->configByIndex(i);
            if (!cfg || !cfg->enabled)
                continue;
            String label;
            label += String((unsigned)cfg->id);
            label += ": ";
            if (cfg->name.length())
                label += cfg->name;
            else
                label += F("Tank");
            out.push_back(label);
        }
        out.push_back(F("Назад"));
    }


    const char *TelegramMenuTanks::tankLevelLabel_(const TankController::TankState &st)
    {
        if (st.level_full)
            return "99%";
        if (st.level_mid)
            return "66%";
        if (st.level_low)
            return "33%";
        return "0%";
    }


    const char *TelegramMenuTanks::tankLevelLabelRemote_(const StackCache::StackTankItem &st)
    {
        if (st.level_full)
            return "99%";
        if (st.level_mid)
            return "66%";
        if (st.level_low)
            return "33%";
        return "0%";
    }


    uint8_t TelegramMenuTanks::tankFillRows_(const TankController::TankState &st)
    {
        if (st.level_full)
            return 4;
        if (st.level_mid)
            return 3;
        if (st.level_low)
            return 2;
        return 0;
    }


    uint8_t TelegramMenuTanks::tankFillRowsRemote_(const StackCache::StackTankItem &st)
    {
        if (st.level_full)
            return 4;
        if (st.level_mid)
            return 3;
        if (st.level_low)
            return 2;
        return 0;
    }


    String TelegramMenuTanks::tankLevelArt_(uint8_t fill_rows)
    {
        String out;
        out.reserve(128);
        out += "<pre>";
        out += "┌───────┐\n";
        for (uint8_t row = 0; row < 4; ++row)
        {
            const bool filled = (row >= (uint8_t)(4 - fill_rows));
            out += filled ? "│███████│" : "│       │";
            out += "\n";
        }
        out += "└───────┘\n";
        out += "</pre>";
        return out;
    }


    String TelegramMenuTanks::tankListTextHtml_(TelegramMenu &self, int64_t chat_id)
    {
        const bool groups_enabled = self.isLocalSelected_(chat_id) ? (self._tanks && self.hasLocalGroups_())
                                                                   : (self._stack_cache && self.hasGroups_(chat_id));
        const bool group_active = groups_enabled && self.groupFilterActive_(chat_id, "tanks");
        String out = F("<b>Баки:</b>");
        out.reserve(768);
        if (self.isLocalSelected_(chat_id))
        {
            if (!self._tanks)
            {
                out += F("\n  недоступны");
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
                    for (size_t i = 0; i < TankController::kTankCount; ++i)
                    {
                        const auto *cfg = self._tanks->configByIndex(i);
                        const auto *st = self._tanks->stateByIndex(i);
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
                        out += "\n      уровень: <b>";
                        out += TelegramMenuTanks::tankLevelLabel_(*st);
                        out += "</b>";
                        out += "\n      клапан: <b>";
                        out += st->valve_on ? F("🟢") : F("⚪");
                        out += "</b>";
                        out += "\n      насос: <b>";
                        out += st->pump_on ? F("🟢") : F("⚪");
                        out += "</b>";
                        out += "\n      питание: <b>";
                        out += cfg->power_on ? F("🟢") : F("⚪");
                        out += "</b>";
                    }
                }
                if (has_no_group)
                {
                    out += F("\n  [Без группы]");
                    any = true;
                    for (size_t i = 0; i < TankController::kTankCount; ++i)
                    {
                        const auto *cfg = self._tanks->configByIndex(i);
                        const auto *st = self._tanks->stateByIndex(i);
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
                        out += "\n      уровень: <b>";
                        out += TelegramMenuTanks::tankLevelLabel_(*st);
                        out += "</b>";
                        out += "\n      клапан: <b>";
                        out += st->valve_on ? F("🟢") : F("⚪");
                        out += "</b>";
                        out += "\n      насос: <b>";
                        out += st->pump_on ? F("🟢") : F("⚪");
                        out += "</b>";
                        out += "\n      питание: <b>";
                        out += cfg->power_on ? F("🟢") : F("⚪");
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
                out += self.escapeHtml_(self.groupLabelById_(chat_id, self.groupFilterId_(chat_id, "tanks")));
                out += F("</b>");
            }
            bool any = false;
            const uint8_t active_group_id = groups_enabled ? self.groupFilterId_(chat_id, "tanks") : 0;
            for (size_t i = 0; i < TankController::kTankCount; ++i)
            {
                const auto *cfg = self._tanks->configByIndex(i);
                const auto *st = self._tanks->stateByIndex(i);
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
                out += "\n    уровень: <b>";
                out += TelegramMenuTanks::tankLevelLabel_(*st);
                out += "</b>";
                out += "\n    клапан: <b>";
                out += st->valve_on ? F("🟢") : F("⚪");
                out += "</b>";
                out += "\n    насос: <b>";
                out += st->pump_on ? F("🟢") : F("⚪");
                out += "</b>";
                out += "\n    питание: <b>";
                out += cfg->power_on ? F("🟢") : F("⚪");
                out += "</b>";
            }
            if (!any)
                out += F("\n  пусто");
            return out;
        }
        if (!self._stack_cache)
        {
            out += F("\n  недоступны");
            return out;
        }
        const uint32_t node_id = self.selectedNodeId_(chat_id);
        if (node_id == 0)
        {
            out += F("\n  недоступны");
            return out;
        }
        const auto *cache = self._stack_cache->tanksCache(node_id);
        if (!cache || !cache->has_data)
        {
            self._stack_cache->requestTanks(node_id);
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
                        out += "\n    СѓСЂРѕРІРµРЅСЊ: <b>";
                        out += TelegramMenuTanks::tankLevelLabelRemote_(it);
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
                    out += "\n    СѓСЂРѕРІРµРЅСЊ: <b>";
                    out += TelegramMenuTanks::tankLevelLabelRemote_(it);
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
            out += self.escapeHtml_(self.groupLabelById_(chat_id, self.groupFilterId_(chat_id, "tanks")));
            out += F("</b>");
        }
        bool any = false;
        for (size_t i = 0; i < cache->item_count; ++i)
        {
            const auto &it = cache->items[i];
            if (!it.enabled)
                continue;
            if (groups_enabled && it.group_id != self.groupFilterId_(chat_id, "tanks"))
                continue;
            any = true;
            out += "\n  ";
            out += String((unsigned)it.id);
            out += ": ";
            out += it.name[0] ? String("<b>") + self.escapeHtml_(String(it.name)) + "</b>" : String("<b>-</b>");
            out += "\n    уровень: <b>";
            out += TelegramMenuTanks::tankLevelLabelRemote_(it);
            out += "</b>\n    клапан: <b>";
            out += it.valve_on ? "🟢" : "⚪";
            out += "</b>\n    насос: <b>";
            out += it.pump_on ? "🟢" : "⚪";
            out += "</b>\n    питание: <b>";
            out += it.power_on ? "🟢" : "⚪";
            out += "</b>";
        }
        if (!any)
            out += F("\n  пусто");
        return out;
    }


    String TelegramMenuTanks::tankDeviceTextHtml_(TelegramMenu &self, int64_t chat_id, uint8_t id)
    {
        if (self.isLocalSelected_(chat_id))
        {
            if (!self._tanks)
                return F("Баки недоступны");
            const auto *cfg = self._tanks->config(id);
            const auto *st = self._tanks->state(id);
            if (!cfg || !st)
                return F("Неверный бак");
            String out = F("<b>Бак:</b>");
            out.reserve(384);
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
            out += "\n  клапан: ";
            out += "<b>";
            out += st->valve_on ? F("🟢") : F("⚪");
            out += "</b>";
            out += "\n  насос: ";
            out += "<b>";
            out += st->pump_on ? F("🟢") : F("⚪");
            out += "</b>";
            out += "\n  питание: ";
            out += "<b>";
            out += cfg->power_on ? F("🟢") : F("⚪");
            out += "</b>";
            out += "\n  уровень: ";
            out += "<b>";
            out += TelegramMenuTanks::tankLevelLabel_(*st);
            out += "</b>";
            out += "\n";
            out += TelegramMenuTanks::tankLevelArt_(TelegramMenuTanks::tankFillRows_(*st));
            return out;
        }
        if (!self._stack_cache)
            return F("Баки недоступны");
        const uint32_t node_id = self.selectedNodeId_(chat_id);
        if (node_id == 0)
            return F("Баки недоступны");
        const auto *cache = self._stack_cache->tanksCache(node_id);
        if (!cache || !cache->has_data)
        {
            self._stack_cache->requestTanks(node_id);
            return F("Обновление данных...");
        }
        const StackCache::StackTankItem *found = nullptr;
        for (size_t i = 0; i < cache->item_count; ++i)
            if (cache->items[i].id == id && cache->items[i].enabled)
            {
                found = &cache->items[i];
                break;
            }
        if (!found)
            return F("Неверный бак");
        String out = F("<b>Бак:</b>");
        out.reserve(384);
        out += "\n  имя: <b>";
        out += found->name[0] ? self.escapeHtml_(String(found->name)) : String("-");
        out += "</b>";
        out += "\n  клапан: <b>";
        out += found->valve_on ? F("🟢") : F("⚪");
        out += "</b>";
        out += "\n  насос: <b>";
        out += found->pump_on ? F("🟢") : F("⚪");
        out += "</b>";
        out += "\n  питание: <b>";
        out += found->power_on ? F("🟢") : F("⚪");
        out += "</b>";
        out += "\n  уровень: <b>";
        out += TelegramMenuTanks::tankLevelLabelRemote_(*found);
        out += "</b>";
        out += "\n";
        out += TelegramMenuTanks::tankLevelArt_(TelegramMenuTanks::tankFillRowsRemote_(*found));
        out += "\n  авария: <b>";
        out += found->alarm_on ? F("🔴") : F("⚪");
        out += "</b>";
        if (!found->levels_ok)
        {
            out += "\n  уровни: <b>ERR</b>";
        }
        return out;
    }


    void TelegramMenuTanks::sendTanksMenu_(TelegramMenu &self, int64_t chat_id)
    {
        if (!self._bot)
            return;
        if (self.isLocalSelected_(chat_id) && !self._tanks)
        {
            self._bot->sendText(chat_id, F("Баки недоступны"));
            return;
        }
        TelegramMenu::ChatAuth *st = self.ensureAuth_(chat_id);
        if (st)
        {
            st->awaiting_tank = false;
            st->selected_tank_id = 0;
        }
        std::vector<String> labels;
        if (self.isLocalSelected_(chat_id))
        {
            const bool groups_enabled = self._tanks && self.hasLocalGroups_();
            const bool group_active = groups_enabled && self.groupFilterActive_(chat_id, "tanks");
            if (groups_enabled && !group_active)
            {
                bool has_no_group = false;
                for (size_t gi = 0; gi < self._configs_manager->groupCount(); ++gi)
                {
                    ConfigsManagerIface::GroupConfig g;
                    if (!self._configs_manager->groupByIndex(gi, g) || g.id == 0)
                        continue;
                    bool present = false;
                    for (size_t i = 0; i < TankController::kTankCount; ++i)
                    {
                        const auto *cfg = self._tanks->configByIndex(i);
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
                const uint8_t active_group_id = groups_enabled ? self.groupFilterId_(chat_id, "tanks") : 0;
                for (size_t i = 0; i < TankController::kTankCount; ++i)
                {
                    const auto *cfg = self._tanks->configByIndex(i);
                    if (!cfg || !cfg->enabled)
                        continue;
                    if (groups_enabled && cfg->group_id != active_group_id)
                        continue;
                    String label = String((unsigned)cfg->id) + ": ";
                    label += cfg->name.length() ? cfg->name : String("Tank");
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
            const bool group_active = groups_enabled && self.groupFilterActive_(chat_id, "tanks");
            if (node_id != 0)
            {
                const auto *cache = self._stack_cache->tanksCache(node_id);
                if (groups_enabled && !group_active)
                {
                    bool has_no_group = false;
                    const auto *groups = self.selectedGroupsCache_(chat_id);
                    if (!cache || !cache->has_data)
                    {
                        self._stack_cache->requestTanks(node_id);
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
                    self._stack_cache->requestTanks(node_id);
                }
                else
                {
                    const uint8_t active_group_id = groups_enabled ? self.groupFilterId_(chat_id, "tanks") : 0;
                    for (size_t i = 0; i < cache->item_count; ++i)
                    {
                        const auto &it = cache->items[i];
                        if (!it.enabled)
                            continue;
                        if (groups_enabled && it.group_id != active_group_id)
                            continue;
                        String label = String((unsigned)it.id) + ": ";
                        label += it.name[0] ? String(it.name) : String("Tank");
                        labels.push_back(label);
                    }
                    if (groups_enabled)
                        labels.push_back(F("Р“СЂСѓРїРїС‹"));
                }
            }
            labels.push_back(F("Назад"));
        }
        if (labels.empty())
            labels.push_back(F("Назад"));
        const String markup = TelegramMenu::buildKeyboardMarkup_(labels);
        const String list = TelegramMenuTanks::tankListTextHtml_(self, chat_id);
        self._bot->setMenu(chat_id, "tanks");
        self._bot->sendText(chat_id, list, markup, "HTML");
    }


    void TelegramMenuTanks::sendTankDevice_(TelegramMenu &self, int64_t chat_id, uint8_t id)
    {
        if (!self._bot)
            return;
        if (self.isLocalSelected_(chat_id) && !self._tanks)
        {
            self._bot->sendText(chat_id, F("Баки недоступны"));
            return;
        }
        TelegramMenu::ChatAuth *st = self.ensureAuth_(chat_id);
        if (st)
        {
            st->awaiting_tank = true;
            st->selected_tank_id = id;
        }
        const String text = TelegramMenuTanks::tankDeviceTextHtml_(self, chat_id, id);
        const String markup = TelegramMenuTanks::tankControlMarkup_();
        self._bot->sendText(chat_id, text, markup, "HTML");
    }


    String TelegramMenuTanks::tankControlMarkup_()
    {
        String out = F("{\"keyboard\":[[\"");
        out += TelegramMenu::escapeJson_(F("Питание 🟢"));
        out += F("\",\"");
        out += TelegramMenu::escapeJson_(F("Питание ⚪"));
        out += F("\"],[\"");
        out += TelegramMenu::escapeJson_(F("Назад"));
        out += F("\"]],\"resize_keyboard\":true,\"one_time_keyboard\":false}");
        return out;
    }


    bool TelegramMenuTanks::handleTankAction_(TelegramMenu &self, const TelegramClient::Update &u)
    {
        TelegramMenu::ChatAuth *st = self.findAuth_(u.chat_id);
        if (!st || !st->awaiting_tank)
            return false;
        if (u.text.startsWith("/"))
            return false;
        if (u.text == F("Назад"))
        {
            st->awaiting_tank = false;
            st->selected_tank_id = 0;
            TelegramMenuTanks::sendTanksMenu_(self, u.chat_id);
            return true;
        }
        if (self.isLocalSelected_(u.chat_id) && !self._tanks)
        {
            self._bot->sendText(u.chat_id, F("Баки недоступны"));
            return true;
        }
        const uint8_t id = st->selected_tank_id;
        if (id == 0)
        {
            self._bot->sendText(u.chat_id, F("Бак не выбран"));
            return true;
        }
        bool handled = true;
        if (!self.isLocalSelected_(u.chat_id))
        {
            const uint32_t node_id = self.selectedNodeId_(u.chat_id);
            if (node_id == 0 || !self._stack_master)
                handled = false;
            else if (u.text == F("Питание 🟢") || u.text == F("🟢"))
            {
                DynamicJsonDocument doc(256);
                doc["feature"] = (uint8_t)StackFeature::Tanks;
                doc["action"] = "set";
                JsonObject params = doc["params"].to<JsonObject>();
                JsonArray items = params["items"].to<JsonArray>();
                JsonObject item = items.add<JsonObject>();
                item["id"] = (unsigned)id;
                item["power_on"] = true;
                char payload[256] = {};
                const size_t n = serializeJson(doc, payload, sizeof(payload));
                handled = (n > 0) && self._stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                                                                 reinterpret_cast<const uint8_t *>(payload), n);
                if (handled && self._stack_cache)
                    self._stack_cache->requestTanks(node_id);
            }
            else if (u.text == F("Питание ⚪") || u.text == F("⚪"))
            {
                DynamicJsonDocument doc(256);
                doc["feature"] = (uint8_t)StackFeature::Tanks;
                doc["action"] = "set";
                JsonObject params = doc["params"].to<JsonObject>();
                JsonArray items = params["items"].to<JsonArray>();
                JsonObject item = items.add<JsonObject>();
                item["id"] = (unsigned)id;
                item["power_on"] = false;
                char payload[256] = {};
                const size_t n = serializeJson(doc, payload, sizeof(payload));
                handled = (n > 0) && self._stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                                                                 reinterpret_cast<const uint8_t *>(payload), n);
                if (handled && self._stack_cache)
                    self._stack_cache->requestTanks(node_id);
            }
            else
            {
                handled = false;
            }
        }
        else if (u.text == F("Питание 🟢") || u.text == F("🟢"))
        {
            self._tanks->setPower(id, true);
        }
        else if (u.text == F("Питание ⚪") || u.text == F("⚪"))
        {
            self._tanks->setPower(id, false);
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
        TelegramMenuTanks::sendTankDevice_(self, u.chat_id, id);
        return true;
    }


    bool TelegramMenuTanks::handleTankSelection_(TelegramMenu &self, const TelegramClient::Update &u)
    {
        if (!self._bot)
            return false;
        const char *menu_id = self._bot->currentMenuId(u.chat_id);
        if (!menu_id || strcmp(menu_id, "tanks") != 0)
            return false;
        if (u.text.startsWith("/"))
            return false;
        if (u.text == F("Назад"))
        {
            self.clearGroupFilter_(u.chat_id, "tanks");
            self._bot->enterMenu(u.chat_id, "device");
            return true;
        }
        if (((self.isLocalSelected_(u.chat_id) && self._tanks) || (!self.isLocalSelected_(u.chat_id) && self._stack_cache)) &&
            self.hasGroups_(u.chat_id))
        {
            if (!self.groupFilterActive_(u.chat_id, "tanks"))
            {
                uint8_t group_id = 0;
                if (!self.parseGroupLabel_(u.chat_id, u.text, group_id))
                {
                    self._bot->sendText(u.chat_id, F("Неизвестная группа"));
                    return true;
                }
                self.setGroupFilter_(u.chat_id, "tanks", group_id);
                TelegramMenuTanks::sendTanksMenu_(self, u.chat_id);
                return true;
            }
            if (u.text == F("Группы"))
            {
                self.clearGroupFilter_(u.chat_id, "tanks");
                TelegramMenuTanks::sendTanksMenu_(self, u.chat_id);
                return true;
            }
        }
        uint8_t id = 0;
        if (!TelegramMenuTanks::parseTankLabel_(u.text, id))
        {
            self._bot->sendText(u.chat_id, F("Неизвестный бак"));
            return true;
        }
        if (self.isLocalSelected_(u.chat_id) && !self._tanks)
        {
            self._bot->sendText(u.chat_id, F("Баки недоступны"));
            return true;
        }
        TelegramMenuTanks::sendTankDevice_(self, u.chat_id, id);
        return true;
    }


    bool TelegramMenuTanks::parseTankIdFromText_(const String &text, uint8_t &out)
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
        return TelegramMenuTanks::parseTankId_(num, out);
    }


    bool TelegramMenuTanks::parseTankId_(const String &text, uint8_t &out)
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
        if (v <= 0 || v > (int)TankController::kTankCount)
            return false;
        out = (uint8_t)v;
        return true;
    }


    bool TelegramMenuTanks::parseTankLabel_(const String &text, uint8_t &out)
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
            return TelegramMenuTanks::parseTankIdFromText_(head, out);
        }
        String low = t;
        low.toLowerCase();
        if (low.startsWith("tank"))
        {
            String tail = t.substring(4);
            tail.trim();
            return TelegramMenuTanks::parseTankIdFromText_(tail, out);
        }
        return TelegramMenuTanks::parseTankIdFromText_(t, out);
    }
