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

class TelegramMenuTanks
{
public:
    static bool cmdTanks_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
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

    static bool cmdTanksList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
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

    static bool cmdTanksShow_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
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

    static void buildTankLabels_(TelegramMenu &self, std::vector<String> &out)
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

    static const char *tankLevelLabel_(const TankController::TankState &st)
    {
        if (st.level_full)
            return "99%";
        if (st.level_mid)
            return "66%";
        if (st.level_low)
            return "33%";
        return "0%";
    }

    static const char *tankLevelLabelRemote_(const StackCache::StackTankItem &st)
    {
        if (st.level_full)
            return "99%";
        if (st.level_mid)
            return "66%";
        if (st.level_low)
            return "33%";
        return "0%";
    }

    static uint8_t tankFillRows_(const TankController::TankState &st)
    {
        if (st.level_full)
            return 4;
        if (st.level_mid)
            return 3;
        if (st.level_low)
            return 2;
        return 0;
    }

    static uint8_t tankFillRowsRemote_(const StackCache::StackTankItem &st)
    {
        if (st.level_full)
            return 4;
        if (st.level_mid)
            return 3;
        if (st.level_low)
            return 2;
        return 0;
    }

    static String tankLevelArt_(uint8_t fill_rows)
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

    static String tankListTextHtml_(TelegramMenu &self, int64_t chat_id)
    {
        String out = F("<b>Баки:</b>");
        out.reserve(768);
        if (self.isLocalSelected_(chat_id))
        {
            if (!self._tanks)
            {
                out += F("\n  недоступны");
                return out;
            }
            bool any = false;
            for (size_t i = 0; i < TankController::kTankCount; ++i)
            {
                const auto *cfg = self._tanks->configByIndex(i);
                const auto *st = self._tanks->stateByIndex(i);
                if (!cfg || !st || !cfg->enabled)
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
        bool any = false;
        for (size_t i = 0; i < cache->item_count; ++i)
        {
            const auto &it = cache->items[i];
            if (!it.enabled)
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

    static String tankDeviceTextHtml_(TelegramMenu &self, int64_t chat_id, uint8_t id)
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

    static void sendTanksMenu_(TelegramMenu &self, int64_t chat_id)
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
            TelegramMenuTanks::buildTankLabels_(self, labels);
        }
        else if (self._stack_cache)
        {
            const uint32_t node_id = self.selectedNodeId_(chat_id);
            if (node_id != 0)
            {
                const auto *cache = self._stack_cache->tanksCache(node_id);
                if (!cache || !cache->has_data)
                {
                    self._stack_cache->requestTanks(node_id);
                }
                else
                {
                    for (size_t i = 0; i < cache->item_count; ++i)
                    {
                        const auto &it = cache->items[i];
                        if (!it.enabled)
                            continue;
                        String label = String((unsigned)it.id) + ": ";
                        label += it.name[0] ? String(it.name) : String("Tank");
                        labels.push_back(label);
                    }
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

    static void sendTankDevice_(TelegramMenu &self, int64_t chat_id, uint8_t id)
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

    static String tankControlMarkup_()
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

    static bool handleTankAction_(TelegramMenu &self, const TelegramClient::Update &u)
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

    static bool handleTankSelection_(TelegramMenu &self, const TelegramClient::Update &u)
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
            self._bot->enterMenu(u.chat_id, "device");
            return true;
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

    static bool parseTankIdFromText_(const String &text, uint8_t &out)
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

    static bool parseTankId_(const String &text, uint8_t &out)
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

    static bool parseTankLabel_(const String &text, uint8_t &out)
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
};
