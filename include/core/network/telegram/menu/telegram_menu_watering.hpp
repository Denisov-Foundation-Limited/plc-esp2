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

class TelegramMenuWatering
{
public:
    static bool cmdWatering_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        (void)reply;
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        if (TelegramMenu::_self->isLocalSelected_(u.chat_id) && !TelegramMenu::_self->_watering)
        {
            reply = "Полив недоступен";
            return true;
        }
        TelegramMenuWatering::sendWateringMenu_(*TelegramMenu::_self, u.chat_id);
        return true;
    }

    static bool cmdWateringList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        if (TelegramMenu::_self->isLocalSelected_(u.chat_id) && !TelegramMenu::_self->_watering)
        {
            reply = "Полив недоступен";
            return true;
        }
        const String text = TelegramMenuWatering::wateringListTextHtml_(*TelegramMenu::_self, u.chat_id);
        bot.sendText(u.chat_id, text, "", "HTML");
        return true;
    }

    static bool cmdWateringShow_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        if (TelegramMenu::_self->isLocalSelected_(u.chat_id) && !TelegramMenu::_self->_watering)
        {
            reply = "Полив недоступен";
            return true;
        }
        const char *cmd = "/watering_show";
        String tail = u.text.substring(strlen(cmd));
        tail.trim();
        uint8_t id = 0;
        if (!TelegramMenuWatering::parseWateringId_(tail, id))
        {
            reply = "Использование: /watering_show <id>";
            return true;
        }
        TelegramMenuWatering::sendWateringRule_( *TelegramMenu::_self, u.chat_id, id);
        return true;
    }

    static void buildWateringLabels_(TelegramMenu &self, int64_t chat_id, std::vector<String> &out)
    {
        out.clear();
        if (self.isLocalSelected_(chat_id))
        {
            if (!self._watering)
            {
                out.push_back(F("Назад"));
                return;
            }
            for (size_t i = 0; i < WateringController::kRuleCount; ++i)
            {
                const auto *cfg = self._watering->configByIndex(i);
                if (!cfg || !cfg->enabled)
                    continue;
                String label = String((unsigned)cfg->id) + ": ";
                label += cfg->name.length() ? cfg->name : String("Rule");
                out.push_back(label);
            }
            out.push_back(F("Назад"));
            return;
        }
        if (!self._stack_cache)
        {
            out.push_back(F("Назад"));
            return;
        }
        const uint32_t node_id = self.selectedNodeId_(chat_id);
        if (node_id == 0)
        {
            out.push_back(F("Назад"));
            return;
        }
        const auto *cache = self._stack_cache->wateringCache(node_id);
        if (!cache || !cache->has_data)
            self._stack_cache->requestWatering(node_id);
        else
            for (size_t i = 0; i < cache->item_count; ++i)
            {
                const auto &it = cache->items[i];
                if (!it.enabled)
                    continue;
                String label = String((unsigned)it.id) + ": ";
                label += it.name[0] ? String(it.name) : String("Rule");
                out.push_back(label);
            }
        out.push_back(F("Назад"));
    }

    static String wateringControlMarkup_(TelegramMenu &self, int64_t chat_id)
    {
        std::vector<String> labels;
        TelegramMenuWatering::buildWateringLabels_(self, chat_id, labels);
        if (labels.empty())
            labels.push_back(F("Назад"));
        return TelegramMenu::buildKeyboardMarkup_(labels);
    }

    static String wateringListTextHtml_(TelegramMenu &self, int64_t chat_id)
    {
        String out = F("<b>Полив:</b>");
        out.reserve(960);
        if (self.isLocalSelected_(chat_id))
        {
            if (!self._watering)
                return F("Полив недоступен");
            bool any = false;
            for (size_t i = 0; i < WateringController::kRuleCount; ++i)
            {
                const auto *cfg = self._watering->configByIndex(i);
                const auto *st = self._watering->stateByIndex(i);
                if (!cfg || !st || !cfg->enabled)
                    continue;
                any = true;
                out += F("\n  ");
                out += String((unsigned)cfg->id);
                out += F(": ");
                out += cfg->name.length() ? String("<b>") + self.escapeHtml_(cfg->name) + "</b>" : String("<b>-</b>");
                out += F(" статус: <b>");
                out += st->status ? "🟢" : "⚪";
                out += F("</b>");
                if (st->active)
                    out += F(" <b>Полив</b>");
            }
            if (!any)
                out += F("\n  пусто");
            return out;
        }
        if (!self._stack_cache)
            return F("Полив недоступен");
        const uint32_t node_id = self.selectedNodeId_(chat_id);
        if (node_id == 0)
            return F("Полив недоступен");
        const auto *cache = self._stack_cache->wateringCache(node_id);
        if (!cache || !cache->has_data)
        {
            self._stack_cache->requestWatering(node_id);
            return F("<b>Полив:</b>\n  обновление...");
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
            out += F(" статус: <b>");
            out += it.status ? "🟢" : "⚪";
            out += F("</b>");
            if (it.active)
                out += F(" <b>Полив</b>");
        }
        if (!any)
            out += F("\n  пусто");
        return out;
    }

    static String wateringRuleTextHtml_(TelegramMenu &self, int64_t chat_id, uint8_t id)
    {
        if (self.isLocalSelected_(chat_id))
        {
            if (!self._watering)
                return F("Полив недоступен");
            const auto *cfg = self._watering->config(id);
            const auto *st = self._watering->state(id);
            if (!cfg || !st)
                return F("Неверное правило");
            String out = F("<b>Полив:</b>");
            out.reserve(480);
            out += F("\n  имя: <b>");
            out += cfg->name.length() ? self.escapeHtml_(cfg->name) : String("-");
            out += F("</b>\n  статус: <b>");
            out += st->status ? "🟢" : "⚪";
            out += F("</b>\n  активен: <b>");
            out += st->active ? "🟢" : "⚪";
            out += F("</b>\n  пауза: <b>");
            out += st->paused ? "🟡" : "⚪";
            out += F("</b>");
            return out;
        }
        if (!self._stack_cache)
            return F("Полив недоступен");
        const uint32_t node_id = self.selectedNodeId_(chat_id);
        if (node_id == 0)
            return F("Полив недоступен");
        const auto *cache = self._stack_cache->wateringCache(node_id);
        if (!cache || !cache->has_data)
        {
            self._stack_cache->requestWatering(node_id);
            return F("Обновление данных...");
        }
        const StackCache::StackWateringItem *found = nullptr;
        for (size_t i = 0; i < cache->item_count; ++i)
            if (cache->items[i].id == id && cache->items[i].enabled)
            {
                found = &cache->items[i];
                break;
            }
        if (!found)
            return F("Неверное правило");
        String out = F("<b>Полив:</b>");
        out.reserve(480);
        out += F("\n  имя: <b>");
        out += found->name[0] ? self.escapeHtml_(String(found->name)) : String("-");
        out += F("</b>\n  статус: <b>");
        out += found->status ? "🟢" : "⚪";
        out += F("</b>\n  активен: <b>");
        out += found->active ? "🟢" : "⚪";
        out += F("</b>\n  пауза: <b>");
        out += found->paused ? "🟡" : "⚪";
        out += F("</b>");
        return out;
    }

    static String wateringRuleControlMarkup_()
    {
        std::vector<String> labels;
        labels.reserve(3);
        labels.push_back(F("Полив 🟢"));
        labels.push_back(F("Полив ⚪"));
        labels.push_back(F("Назад"));
        return TelegramMenu::buildKeyboardMarkup_(labels);
    }

    static void sendWateringMenu_(TelegramMenu &self, int64_t chat_id)
    {
        if (!self._bot)
            return;
        if (self.isLocalSelected_(chat_id) && !self._watering)
        {
            self._bot->sendText(chat_id, F("Полив недоступен"));
            return;
        }
        TelegramMenu::ChatAuth *st = self.ensureAuth_(chat_id);
        if (st)
        {
            st->awaiting_watering = false;
            st->selected_watering_id = 0;
        }
        const String text = TelegramMenuWatering::wateringListTextHtml_(self, chat_id);
        const String markup = TelegramMenuWatering::wateringControlMarkup_(self, chat_id);
        self._bot->setMenu(chat_id, "watering");
        self._bot->sendText(chat_id, text, markup, "HTML");
    }

    static void sendWateringRule_(TelegramMenu &self, int64_t chat_id, uint8_t id)
    {
        if (!self._bot)
            return;
        if (self.isLocalSelected_(chat_id) && !self._watering)
        {
            self._bot->sendText(chat_id, F("Полив недоступен"));
            return;
        }
        TelegramMenu::ChatAuth *st = self.ensureAuth_(chat_id);
        if (st)
        {
            st->awaiting_watering = true;
            st->selected_watering_id = id;
        }
        const String text = TelegramMenuWatering::wateringRuleTextHtml_(self, chat_id, id);
        self._bot->sendText(chat_id, text, TelegramMenuWatering::wateringRuleControlMarkup_(), "HTML");
    }

    static bool handleWateringAction_(TelegramMenu &self, const TelegramClient::Update &u)
    {
        TelegramMenu::ChatAuth *st = self.findAuth_(u.chat_id);
        if (!st || !st->awaiting_watering)
            return false;
        if (u.text.startsWith("/"))
            return false;
        if (u.text == F("Назад"))
        {
            st->awaiting_watering = false;
            st->selected_watering_id = 0;
            TelegramMenuWatering::sendWateringMenu_(self, u.chat_id);
            return true;
        }
        const uint8_t id = st->selected_watering_id;
        if (id == 0)
        {
            self._bot->sendText(u.chat_id, F("Правило не выбрано"));
            return true;
        }
        bool handled = true;
        bool ok = false;
        if (self.isLocalSelected_(u.chat_id))
        {
            if (!self._watering)
                handled = false;
            else if (u.text == F("Полив 🟢") || u.text == F("Полив Вкл"))
                ok = self._watering->setStatus(id, true);
            else if (u.text == F("Полив ⚪") || u.text == F("Полив Выкл"))
                ok = self._watering->setStatus(id, false);
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
                DynamicJsonDocument doc(256);
                doc["feature"] = (uint8_t)StackFeature::Watering;
                doc["action"] = "set";
                JsonObject params = doc["params"].to<JsonObject>();
                params["id"] = (unsigned)id;
                if (u.text == F("Полив 🟢") || u.text == F("Полив Вкл"))
                    params["status"] = true;
                else if (u.text == F("Полив ⚪") || u.text == F("Полив Выкл"))
                    params["status"] = false;
                else
                    handled = false;
                if (handled)
                {
                    char payload[256] = {};
                    const size_t n = serializeJson(doc, payload, sizeof(payload));
                    ok = (n > 0) && self._stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                                                                reinterpret_cast<const uint8_t *>(payload), n);
                    if (ok && self._stack_cache)
                        self._stack_cache->requestWatering(node_id);
                }
            }
        }
        if (!handled)
            return false;
        if (!ok)
            self._bot->sendText(u.chat_id, F("Не удалось"));
        TelegramMenuWatering::sendWateringRule_(self, u.chat_id, id);
        return true;
    }

    static bool handleWateringSelection_(TelegramMenu &self, const TelegramClient::Update &u)
    {
        if (!self._bot)
            return false;
        const char *menu_id = self._bot->currentMenuId(u.chat_id);
        if (!menu_id || strcmp(menu_id, "watering") != 0)
            return false;
        if (u.text.startsWith("/"))
            return false;
        if (u.text == F("Назад"))
        {
            self._bot->enterMenu(u.chat_id, "device");
            return true;
        }
        uint8_t id = 0;
        if (!TelegramMenuWatering::parseWateringLabel_(u.text, id))
        {
            self._bot->sendText(u.chat_id, F("Неизвестное правило"));
            return true;
        }
        TelegramMenuWatering::sendWateringRule_(self, u.chat_id, id);
        return true;
    }

    static bool parseWateringIdFromText_(const String &text, uint8_t &out)
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
        String num = text.substring(start, end);
        return TelegramMenuWatering::parseWateringId_(num, out);
    }

    static bool parseWateringId_(const String &text, uint8_t &out)
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
        if (v <= 0 || v > (int)WateringController::kRuleCount)
            return false;
        out = (uint8_t)v;
        return true;
    }

    static bool parseWateringLabel_(const String &text, uint8_t &out)
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
            return TelegramMenuWatering::parseWateringIdFromText_(head, out);
        }
        return TelegramMenuWatering::parseWateringIdFromText_(t, out);
    }
};
