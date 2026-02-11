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

class TelegramMenuSockets
{
public:
    static bool cmdSockets_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        (void)reply;
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        TelegramMenuSockets::sendSocketMenu_(*TelegramMenu::_self, u.chat_id, false);
        return true;
    }

    static bool cmdLights_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        (void)reply;
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        TelegramMenuSockets::sendSocketMenu_(*TelegramMenu::_self, u.chat_id, true);
        return true;
    }

    static bool cmdSocketList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        if (TelegramMenu::_self->isLocalSelected_(u.chat_id) && !TelegramMenu::_self->_sockets)
        {
            reply = "Розетки недоступны";
            return true;
        }
        const String text = TelegramMenuSockets::socketListTextHtml_(*TelegramMenu::_self, u.chat_id, false);
        bot.sendText(u.chat_id, text, "", "HTML");
        return true;
    }

    static bool cmdSocketOn_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        return TelegramMenuSockets::startSocketAction_(*TelegramMenu::_self, bot, u, reply, 1);
    }

    static bool cmdSocketOff_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        return TelegramMenuSockets::startSocketAction_(*TelegramMenu::_self, bot, u, reply, 2);
    }

    static bool cmdSocketToggle_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        return TelegramMenuSockets::startSocketAction_(*TelegramMenu::_self, bot, u, reply, 3);
    }

    static bool startSocketAction_(TelegramMenu &self, TelegramBot &bot, const TelegramClient::Update &u, String &reply, uint8_t action)
    {
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(self, bot, u, reply))
            return true;
        TelegramMenu::ChatAuth *st = self.ensureAuth_(u.chat_id);
        if (!st)
            return false;
        st->awaiting_socket = true;
        st->socket_action = action;
        String msg = String("Введите ID розетки (1..") + String(SocketController::kSocketCount) + "):";
        bot.sendText(u.chat_id, msg);
        return true;
    }

    static void buildSocketLabels_(TelegramMenu &self, std::vector<String> &out, bool lights_only = false)
    {
        out.clear();
        if (!self._sockets)
        {
            out.reserve(1);
        }
        else if (lights_only)
        {
            out.reserve(SocketController::kLightCount + 1);
        }
        else
        {
            out.reserve(SocketController::kSocketCount + 1);
        }
        if (self._sockets)
        {
            if (lights_only)
            {
                for (size_t i = 0; i < SocketController::kLightCount; ++i)
                {
                    const auto *cfg = self._sockets->lightConfigByIndex(i);
                    if (!cfg || !cfg->enabled)
                        continue;
                    String label;
                    if (cfg->name.length())
                    {
                        label += String((unsigned)cfg->id);
                        label += ": ";
                        label += cfg->name;
                    }
                    else
                    {
                        label += F("Свет ");
                        label += String((unsigned)cfg->id);
                    }
                    out.push_back(label);
                }
            }
            else
            {
                for (size_t i = 0; i < SocketController::kSocketCount; ++i)
                {
                    const auto *cfg = self._sockets->configByIndex(i);
                    if (!cfg || !cfg->enabled)
                        continue;
                    String label;
                    if (cfg->name.length())
                    {
                        label += String((unsigned)cfg->id);
                        label += ": ";
                        label += cfg->name;
                    }
                    else
                    {
                        label += F("Розетка ");
                        label += String((unsigned)cfg->id);
                    }
                    out.push_back(label);
                }
            }
        }
        out.push_back(F("Назад"));
    }

    static String socketListTextHtml_(TelegramMenu &self, int64_t chat_id, bool lights_only = false)
    {
        String out = lights_only ? F("<b>Свет:</b>") : F("<b>Розетки:</b>");
        out.reserve(512);
        if (self.isLocalSelected_(chat_id))
        {
            if (!self._sockets)
            {
                out += F("\n  недоступны");
                return out;
            }
            bool any = false;
            if (lights_only)
            {
                for (size_t i = 0; i < SocketController::kLightCount; ++i)
                {
                    const auto *cfg = self._sockets->lightConfigByIndex(i);
                    const auto *st = self._sockets->lightStateByIndex(i);
                    if (!cfg || !st || !cfg->enabled)
                        continue;
                    any = true;
                    out += "\n  ";
                    out += st->relay_on ? F("💡 ") : F("⚪ ");
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
                        out += "-";
                    }
                }
            }
            else
            {
                for (size_t i = 0; i < SocketController::kSocketCount; ++i)
                {
                    const auto *cfg = self._sockets->configByIndex(i);
                    const auto *st = self._sockets->stateByIndex(i);
                    if (!cfg || !st || !cfg->enabled)
                        continue;
                    any = true;
                    out += "\n  ";
                    out += st->relay_on ? F("🟢 ") : F("⚪ ");
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
                        out += "-";
                    }
                }
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
        bool any = false;
        if (lights_only)
        {
            const auto *cache = self._stack_cache->lightsCache(node_id);
            if (!cache || !cache->has_data)
            {
                self._stack_cache->requestLights(node_id);
            }
            else
            {
                for (size_t i = 0; i < cache->item_count; ++i)
                {
                    const auto &it = cache->items[i];
                    if (!it.enabled)
                        continue;
                    any = true;
                    out += "\n  ";
                    out += it.state ? F("💡 ") : F("⚪ ");
                    out += String((unsigned)it.id);
                    out += ": ";
                    if (it.name[0])
                    {
                        out += "<b>";
                        out += self.escapeHtml_(String(it.name));
                        out += "</b>";
                    }
                    else
                    {
                        out += "-";
                    }
                }
            }
        }
        else
        {
            const auto *cache = self._stack_cache->socketsCache(node_id);
            if (!cache || !cache->has_data)
            {
                self._stack_cache->requestSockets(node_id);
            }
            else
            {
                for (size_t i = 0; i < cache->item_count; ++i)
                {
                    const auto &it = cache->items[i];
                    if (!it.enabled)
                        continue;
                    any = true;
                    out += "\n  ";
                    out += it.state ? F("🟢 ") : F("⚪ ");
                    out += String((unsigned)it.id);
                    out += ": ";
                    if (it.name[0])
                    {
                        out += "<b>";
                        out += self.escapeHtml_(String(it.name));
                        out += "</b>";
                    }
                    else
                    {
                        out += "-";
                    }
                }
            }
        }
        if (!any)
            out += F("\n  пусто");
        return out;
    }

    static void sendSocketMenu_(TelegramMenu &self, int64_t chat_id, bool lights_only)
    {
        if (!self._bot)
            return;
        TelegramMenu::ChatAuth *st = self.ensureAuth_(chat_id);
        if (st)
        {
            st->awaiting_socket = false;
            st->socket_action = 0;
        }
        std::vector<String> labels;
        if (self.isLocalSelected_(chat_id))
        {
            TelegramMenuSockets::buildSocketLabels_(self, labels, lights_only);
        }
        else if (self._stack_cache)
        {
            const uint32_t node_id = self.selectedNodeId_(chat_id);
            if (node_id != 0)
            {
                if (lights_only)
                {
                    const auto *cache = self._stack_cache->lightsCache(node_id);
                    if (!cache || !cache->has_data)
                    {
                        self._stack_cache->requestLights(node_id);
                    }
                    else
                    {
                        for (size_t i = 0; i < cache->item_count; ++i)
                        {
                            const auto &it = cache->items[i];
                            if (!it.enabled)
                                continue;
                            String label;
                            label += String((unsigned)it.id);
                            label += ": ";
                            label += it.name[0] ? String(it.name) : String("Свет");
                            labels.push_back(label);
                        }
                    }
                }
                else
                {
                    const auto *cache = self._stack_cache->socketsCache(node_id);
                    if (!cache || !cache->has_data)
                    {
                        self._stack_cache->requestSockets(node_id);
                    }
                    else
                    {
                        for (size_t i = 0; i < cache->item_count; ++i)
                        {
                            const auto &it = cache->items[i];
                            if (!it.enabled)
                                continue;
                            String label;
                            label += String((unsigned)it.id);
                            label += ": ";
                            label += it.name[0] ? String(it.name) : String("Розетка");
                            labels.push_back(label);
                        }
                    }
                }
            }
            labels.push_back(F("Назад"));
        }
        else
        {
            labels.push_back(F("Назад"));
        }
        const String markup = TelegramMenu::buildKeyboardMarkup_(labels);
        const String list = TelegramMenuSockets::socketListTextHtml_(self, chat_id, lights_only);
        self._bot->setMenu(chat_id, lights_only ? "lights" : "sockets");
        self._bot->sendText(chat_id, list, markup, "HTML");
    }

    static bool handleSocketToggleSelection_(TelegramMenu &self, const TelegramClient::Update &u)
    {
        if (!self._bot)
            return false;
        const char *menu_id = self._bot->currentMenuId(u.chat_id);
        if (!menu_id)
            return false;
        const bool lights_only = (strcmp(menu_id, "lights") == 0);
        if (!lights_only && strcmp(menu_id, "sockets") != 0)
            return false;
        if (u.text == F("Назад"))
        {
            self._bot->enterMenu(u.chat_id, "device");
            return true;
        }
        uint8_t id = 0;
        const uint8_t max_id = lights_only ? (uint8_t)SocketController::kLightCount
                                           : (uint8_t)SocketController::kSocketCount;
        if (!TelegramMenuSockets::parseSocketLabel_(u.text, id, max_id))
        {
            self._bot->sendText(u.chat_id, lights_only ? F("Неизвестный свет") : F("Неизвестная розетка"));
            return true;
        }
        if (self.isLocalSelected_(u.chat_id) && !self._sockets)
        {
            self._bot->sendText(u.chat_id, lights_only ? F("Свет недоступен") : F("Розетки недоступны"));
            return true;
        }
        bool ok = false;
        if (self.isLocalSelected_(u.chat_id))
        {
            if (lights_only)
            {
                const auto *cfg = self._sockets->lightConfig(id);
                if (!cfg)
                {
                    self._bot->sendText(u.chat_id, F("Неизвестный свет"));
                    return true;
                }
                ok = self._sockets->toggleLightRelayById(id);
            }
            else
            {
                const auto *cfg = self._sockets->config(id);
                if (!cfg)
                {
                    self._bot->sendText(u.chat_id, F("Неизвестная розетка"));
                    return true;
                }
                ok = self._sockets->toggleRelayById(id);
            }
        }
        else
        {
            const uint32_t node_id = self.selectedNodeId_(u.chat_id);
            if (node_id != 0 && self._stack_master)
            {
                DynamicJsonDocument doc(256);
                doc["feature"] = (uint8_t)StackFeature::Sockets;
                doc["action"] = lights_only ? "set_lights" : "set";
                JsonObject params = doc["params"].to<JsonObject>();
                JsonArray items = params["items"].to<JsonArray>();
                JsonObject item = items.add<JsonObject>();
                item["id"] = (unsigned)id;
                item["toggle"] = true;
                char payload[256] = {};
                const size_t n = serializeJson(doc, payload, sizeof(payload));
                if (n > 0)
                    ok = self._stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                                                    reinterpret_cast<const uint8_t *>(payload), n);
                if (ok && self._stack_cache)
                {
                    if (lights_only)
                        self._stack_cache->requestLights(node_id);
                    else
                        self._stack_cache->requestSockets(node_id);
                }
            }
        }
        if (!ok)
        {
            self._bot->sendText(u.chat_id, F("Не удалось"));
            return true;
        }
        TelegramMenuSockets::sendSocketMenu_(self, u.chat_id, lights_only);
        return true;
    }

    static bool parseSocketIdFromText_(const String &text, uint8_t &out, uint8_t max_id)
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
        return TelegramMenuSockets::parseSocketId_(num, out, max_id);
    }

    static bool parseSocketId_(const String &text, uint8_t &out, uint8_t max_id)
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
        if (v <= 0 || v > (int)max_id)
            return false;
        out = (uint8_t)v;
        return true;
    }

    static bool parseSocketLabel_(const String &text, uint8_t &out, uint8_t max_id)
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
            return TelegramMenuSockets::parseSocketIdFromText_(head, out, max_id);
        }
        String low = t;
        low.toLowerCase();
        if (low.startsWith("socket"))
        {
            String tail = t.substring(6);
            tail.trim();
            return TelegramMenuSockets::parseSocketIdFromText_(tail, out, max_id);
        }
        return TelegramMenuSockets::parseSocketIdFromText_(t, out, max_id);
    }
};
