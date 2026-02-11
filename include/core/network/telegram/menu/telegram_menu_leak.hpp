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

class TelegramMenuLeak
{
public:
    static bool cmdLeak_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        (void)reply;
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        if (TelegramMenu::_self->isLocalSelected_(u.chat_id) && !TelegramMenu::_self->_leak)
        {
            reply = "Leak недоступен";
            return true;
        }
        TelegramMenuLeak::sendLeakMenu_(*TelegramMenu::_self, u.chat_id);
        return true;
    }

    static bool cmdLeakList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        if (TelegramMenu::_self->isLocalSelected_(u.chat_id) && !TelegramMenu::_self->_leak)
        {
            reply = "Leak недоступен";
            return true;
        }
        const String text = TelegramMenuLeak::leakListTextHtml_(*TelegramMenu::_self, u.chat_id);
        bot.sendText(u.chat_id, text, "", "HTML");
        return true;
    }

    static bool cmdLeakShow_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        (void)bot;
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        if (TelegramMenu::_self->isLocalSelected_(u.chat_id) && !TelegramMenu::_self->_leak)
        {
            reply = "Leak недоступен";
            return true;
        }
        const char *cmd = "/leak_show";
        String tail = u.text.substring(strlen(cmd));
        tail.trim();
        uint8_t id = 0;
        if (!TelegramMenuLeak::parseLeakId_(tail, id))
        {
            reply = "Использование: /leak_show <id>";
            return true;
        }
        TelegramMenuLeak::sendLeakZone_(*TelegramMenu::_self, u.chat_id, id);
        return true;
    }

    static bool cmdLeakAck_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        (void)bot;
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        if (TelegramMenu::_self->isLocalSelected_(u.chat_id))
        {
            if (!TelegramMenu::_self->_leak)
            {
                reply = "Leak недоступен";
                return true;
            }
            reply = TelegramMenu::_self->_leak->ackAll() ? "Квитировано" : "Нет активных тревог";
            return true;
        }
        const uint32_t node_id = TelegramMenu::_self->selectedNodeId_(u.chat_id);
        if (node_id == 0 || !TelegramMenu::_self->_stack_master)
        {
            reply = "Leak недоступен";
            return true;
        }
        DynamicJsonDocument doc(256);
        doc["feature"] = (uint8_t)StackFeature::Leak;
        doc["action"] = "set";
        JsonObject params = doc["params"].to<JsonObject>();
        params["ack_all"] = true;
        char payload[256] = {};
        const size_t n = serializeJson(doc, payload, sizeof(payload));
        const bool ok = (n > 0) && TelegramMenu::_self->_stack_master->sendTo(
                                     node_id, (uint8_t)StackMsgType::CmdSet,
                                     reinterpret_cast<const uint8_t *>(payload), n);
        if (ok && TelegramMenu::_self->_stack_cache)
            TelegramMenu::_self->_stack_cache->requestLeak(node_id);
        reply = ok ? "Команда отправлена" : "Не удалось";
        return true;
    }

    static void buildLeakLabels_(TelegramMenu &self, int64_t chat_id, std::vector<String> &out)
    {
        out.clear();
        if (self.isLocalSelected_(chat_id))
        {
            if (!self._leak)
            {
                out.push_back(F("Сброс тревоги (всё)"));
                out.push_back(F("Назад"));
                return;
            }
            for (size_t i = 0; i < LeakController::kZoneCount; ++i)
            {
                const auto *cfg = self._leak->configByIndex(i);
                if (!cfg || !cfg->enabled)
                    continue;
                String label = String((unsigned)cfg->id) + ": ";
                label += cfg->name.length() ? cfg->name : String("Leak");
                out.push_back(label);
            }
            out.push_back(F("Сброс тревоги (всё)"));
            out.push_back(F("Назад"));
            return;
        }
        if (!self._stack_cache)
        {
            out.push_back(F("Сброс тревоги (всё)"));
            out.push_back(F("Назад"));
            return;
        }
        const uint32_t node_id = self.selectedNodeId_(chat_id);
        if (node_id == 0)
        {
            out.push_back(F("Сброс тревоги (всё)"));
            out.push_back(F("Назад"));
            return;
        }
        const auto *cache = self._stack_cache->leakCache(node_id);
        if (!cache || !cache->has_data)
        {
            self._stack_cache->requestLeak(node_id);
        }
        else
        {
            for (size_t i = 0; i < cache->item_count; ++i)
            {
                const auto &it = cache->items[i];
                if (!it.enabled)
                    continue;
                String label = String((unsigned)it.id) + ": ";
                label += it.name[0] ? String(it.name) : String("Leak");
                out.push_back(label);
            }
        }
        out.push_back(F("Сброс тревоги (всё)"));
        out.push_back(F("Назад"));
    }

    static String leakListTextHtml_(TelegramMenu &self, int64_t chat_id)
    {
        String out = F("<b>Leak:</b>");
        out.reserve(896);
        if (self.isLocalSelected_(chat_id))
        {
            if (!self._leak)
                return F("Leak недоступен");
            bool any = false;
            for (size_t i = 0; i < LeakController::kZoneCount; ++i)
            {
                const auto *cfg = self._leak->configByIndex(i);
                const auto *st = self._leak->stateByIndex(i);
                if (!cfg || !st || !cfg->enabled)
                    continue;
                any = true;
                out += F("\n  ");
                out += String((unsigned)cfg->id);
                out += F(": ");
                out += cfg->name.length() ? String("<b>") + self.escapeHtml_(cfg->name) + "</b>" : String("<b>-</b>");
                out += F("\n    питание: <b>");
                out += cfg->power_on ? "🟢" : "⚪";
                out += F("</b>\n    влага: <b>");
                out += st->wet ? "🔴" : "⚪";
                out += F("</b>\n    блокировка: <b>");
                out += st->alarm_latched ? "🔴" : "⚪";
                out += F("</b>");
            }
            if (!any)
                out += F("\n  пусто");
            return out;
        }
        if (!self._stack_cache)
            return F("Leak недоступен");
        const uint32_t node_id = self.selectedNodeId_(chat_id);
        if (node_id == 0)
            return F("Leak недоступен");
        const auto *cache = self._stack_cache->leakCache(node_id);
        if (!cache || !cache->has_data)
        {
            self._stack_cache->requestLeak(node_id);
            return F("<b>Leak:</b>\n  обновление...");
        }
        bool any = false;
        for (size_t i = 0; i < cache->item_count; ++i)
        {
            const auto &it = cache->items[i];
            if (!it.enabled)
                continue;
            any = true;
            out += F("\n  ");
            out += String((unsigned)it.id);
            out += F(": ");
            out += it.name[0] ? String("<b>") + self.escapeHtml_(String(it.name)) + "</b>" : String("<b>-</b>");
            out += F("\n    питание: <b>");
            out += it.power_on ? "🟢" : "⚪";
            out += F("</b>\n    влага: <b>");
            out += it.wet ? "🔴" : "⚪";
            out += F("</b>\n    блокировка: <b>");
            out += it.alarm_latched ? "🔴" : "⚪";
            out += F("</b>");
        }
        if (!any)
            out += F("\n  пусто");
        return out;
    }

    static String leakZoneTextHtml_(TelegramMenu &self, int64_t chat_id, uint8_t id)
    {
        if (self.isLocalSelected_(chat_id))
        {
            if (!self._leak)
                return F("Leak недоступен");
            const auto *cfg = self._leak->config(id);
            const auto *st = self._leak->state(id);
            if (!cfg || !st)
                return F("Неверная зона");
            String out = F("<b>Leak зона:</b>");
            out.reserve(420);
            out += F("\n  имя: <b>");
            out += cfg->name.length() ? self.escapeHtml_(cfg->name) : String("-");
            out += F("</b>\n  питание: <b>");
            out += cfg->power_on ? "🟢" : "⚪";
            out += F("</b>\n  влага: <b>");
            out += st->wet ? "🔴" : "⚪";
            out += F("</b>\n  блокировка: <b>");
            out += st->alarm_latched ? "🔴" : "⚪";
            out += F("</b>");
            return out;
        }
        if (!self._stack_cache)
            return F("Leak недоступен");
        const uint32_t node_id = self.selectedNodeId_(chat_id);
        if (node_id == 0)
            return F("Leak недоступен");
        const auto *cache = self._stack_cache->leakCache(node_id);
        if (!cache || !cache->has_data)
        {
            self._stack_cache->requestLeak(node_id);
            return F("Обновление данных...");
        }
        const StackCache::StackLeakItem *found = nullptr;
        for (size_t i = 0; i < cache->item_count; ++i)
            if (cache->items[i].id == id && cache->items[i].enabled)
            {
                found = &cache->items[i];
                break;
            }
        if (!found)
            return F("Неверная зона");
        String out = F("<b>Leak зона:</b>");
        out.reserve(420);
        out += F("\n  имя: <b>");
        out += found->name[0] ? self.escapeHtml_(String(found->name)) : String("-");
        out += F("</b>\n  питание: <b>");
        out += found->power_on ? "🟢" : "⚪";
        out += F("</b>\n  влага: <b>");
        out += found->wet ? "🔴" : "⚪";
        out += F("</b>\n  блокировка: <b>");
        out += found->alarm_latched ? "🔴" : "⚪";
        out += F("</b>");
        return out;
    }

    static String leakZoneControlMarkup_()
    {
        std::vector<String> labels;
        labels.reserve(4);
        labels.push_back(F("Питание 🟢"));
        labels.push_back(F("Питание ⚪"));
        labels.push_back(F("Сброс тревоги"));
        labels.push_back(F("Назад"));
        return TelegramMenu::buildKeyboardMarkup_(labels);
    }

    static void sendLeakMenu_(TelegramMenu &self, int64_t chat_id)
    {
        if (!self._bot)
            return;
        if (self.isLocalSelected_(chat_id) && !self._leak)
        {
            self._bot->sendText(chat_id, F("Leak недоступен"));
            return;
        }
        TelegramMenu::ChatAuth *st = self.ensureAuth_(chat_id);
        if (st)
        {
            st->awaiting_leak = false;
            st->selected_leak_id = 0;
        }
        std::vector<String> labels;
        TelegramMenuLeak::buildLeakLabels_(self, chat_id, labels);
        const String markup = TelegramMenu::buildKeyboardMarkup_(labels);
        const String text = TelegramMenuLeak::leakListTextHtml_(self, chat_id);
        self._bot->setMenu(chat_id, "leak");
        self._bot->sendText(chat_id, text, markup, "HTML");
    }

    static void sendLeakZone_(TelegramMenu &self, int64_t chat_id, uint8_t id)
    {
        if (!self._bot)
            return;
        if (self.isLocalSelected_(chat_id) && !self._leak)
        {
            self._bot->sendText(chat_id, F("Leak недоступен"));
            return;
        }
        TelegramMenu::ChatAuth *st = self.ensureAuth_(chat_id);
        if (st)
        {
            st->awaiting_leak = true;
            st->selected_leak_id = id;
        }
        const String text = TelegramMenuLeak::leakZoneTextHtml_(self, chat_id, id);
        self._bot->sendText(chat_id, text, TelegramMenuLeak::leakZoneControlMarkup_(), "HTML");
    }

    static bool handleLeakAction_(TelegramMenu &self, const TelegramClient::Update &u)
    {
        TelegramMenu::ChatAuth *st = self.findAuth_(u.chat_id);
        if (!st || !st->awaiting_leak)
            return false;
        if (u.text.startsWith("/"))
            return false;
        if (u.text == F("Назад"))
        {
            st->awaiting_leak = false;
            st->selected_leak_id = 0;
            TelegramMenuLeak::sendLeakMenu_(self, u.chat_id);
            return true;
        }
        if (self.isLocalSelected_(u.chat_id) && !self._leak)
        {
            self._bot->sendText(u.chat_id, F("Leak недоступен"));
            return true;
        }
        const uint8_t id = st->selected_leak_id;
        if (id == 0)
        {
            self._bot->sendText(u.chat_id, F("Зона не выбрана"));
            return true;
        }
        bool handled = true;
        bool ok = false;
        if (self.isLocalSelected_(u.chat_id))
        {
            if (u.text == F("Питание 🟢") || u.text == F("Питание Вкл"))
                ok = self._leak->setPower(id, true);
            else if (u.text == F("Питание ⚪") || u.text == F("Питание Выкл"))
                ok = self._leak->setPower(id, false);
            else if (u.text == F("Сброс тревоги") || u.text == F("Квитировать"))
                ok = self._leak->ack(id);
            else
                handled = false;
        }
        else
        {
            const uint32_t node_id = self.selectedNodeId_(u.chat_id);
            if (node_id == 0 || !self._stack_master)
                handled = false;
            else
            {
                DynamicJsonDocument doc(384);
                doc["feature"] = (uint8_t)StackFeature::Leak;
                doc["action"] = "set";
                JsonObject params = doc["params"].to<JsonObject>();
                if (u.text == F("Питание 🟢") || u.text == F("Питание Вкл"))
                {
                    JsonArray zones = params["zones"].to<JsonArray>();
                    JsonObject z = zones.add<JsonObject>();
                    z["id"] = (unsigned)id;
                    z["power_on"] = true;
                }
                else if (u.text == F("Питание ⚪") || u.text == F("Питание Выкл"))
                {
                    JsonArray zones = params["zones"].to<JsonArray>();
                    JsonObject z = zones.add<JsonObject>();
                    z["id"] = (unsigned)id;
                    z["power_on"] = false;
                }
                else if (u.text == F("Сброс тревоги") || u.text == F("Квитировать"))
                {
                    params["ack_all"] = true;
                }
                else
                    handled = false;
                if (handled)
                {
                    char payload[384] = {};
                    const size_t n = serializeJson(doc, payload, sizeof(payload));
                    ok = (n > 0) && self._stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                                                                reinterpret_cast<const uint8_t *>(payload), n);
                    if (ok && self._stack_cache)
                        self._stack_cache->requestLeak(node_id);
                }
            }
        }
        if (!handled)
            return false;
        if (!ok)
            self._bot->sendText(u.chat_id, F("Не удалось"));
        TelegramMenuLeak::sendLeakZone_(self, u.chat_id, id);
        return true;
    }

    static bool handleLeakSelection_(TelegramMenu &self, const TelegramClient::Update &u)
    {
        if (!self._bot)
            return false;
        const char *menu_id = self._bot->currentMenuId(u.chat_id);
        if (!menu_id || strcmp(menu_id, "leak") != 0)
            return false;
        if (u.text.startsWith("/"))
            return false;
        if (u.text == F("Назад"))
        {
            self._bot->enterMenu(u.chat_id, "device");
            return true;
        }
        if (u.text == F("Сброс тревоги (всё)") || u.text == F("Квитировать всё"))
        {
            String reply;
            TelegramMenuLeak::cmdLeakAck_(*self._bot, u, reply);
            if (reply.length())
                self._bot->sendText(u.chat_id, reply);
            TelegramMenuLeak::sendLeakMenu_(self, u.chat_id);
            return true;
        }
        uint8_t id = 0;
        if (!TelegramMenuLeak::parseLeakLabel_(u.text, id))
        {
            self._bot->sendText(u.chat_id, F("Неизвестная зона"));
            return true;
        }
        TelegramMenuLeak::sendLeakZone_(self, u.chat_id, id);
        return true;
    }

    static bool parseLeakIdFromText_(const String &text, uint8_t &out)
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
                break;
        }
        if (start < 0 || end <= start)
            return false;
        return TelegramMenuLeak::parseLeakId_(text.substring(start, end), out);
    }

    static bool parseLeakId_(const String &text, uint8_t &out)
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
        if (v <= 0 || v > (int)LeakController::kZoneCount)
            return false;
        out = (uint8_t)v;
        return true;
    }

    static bool parseLeakLabel_(const String &text, uint8_t &out)
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
            return TelegramMenuLeak::parseLeakIdFromText_(head, out);
        }
        return TelegramMenuLeak::parseLeakIdFromText_(t, out);
    }
};
