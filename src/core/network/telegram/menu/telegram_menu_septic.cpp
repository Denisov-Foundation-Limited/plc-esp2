#include "core/network/telegram/telegram_menu.hpp"

#include <stdlib.h>
#include <string.h>

#include "controllers/septic_controller.hpp"
#include "core/network/stack/stack_cache.hpp"
#include "core/network/stack/stack_features.hpp"
#include "core/network/stack/stack_master.hpp"
#include "core/network/wifi_manager.hpp"
#include "core/rtc.hpp"
#include "plc/plc_control.hpp"
#include "utils/configs.hpp"
#include "utils/logger.hpp"

#include "core/network/telegram/menu/telegram_menu_septic.hpp"

// ---- telegram menu extracted definitions ----
bool TelegramMenuSeptic::cmdSeptic_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        (void)reply;
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        if (TelegramMenu::_self->isLocalSelected_(u.chat_id) && !TelegramMenu::_self->_septic)
        {
            reply = "Септик недоступен";
            return true;
        }
        TelegramMenuSeptic::sendSepticMenu_(*TelegramMenu::_self, u.chat_id);
        return true;
    }


    bool TelegramMenuSeptic::cmdSepticStatus_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        if (TelegramMenu::_self->isLocalSelected_(u.chat_id) && !TelegramMenu::_self->_septic)
        {
            reply = "Септик недоступен";
            return true;
        }
        reply = TelegramMenuSeptic::septicStatusText_(*TelegramMenu::_self, u.chat_id);
        return true;
    }


    bool TelegramMenuSeptic::cmdSepticList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        if (TelegramMenu::_self->isLocalSelected_(u.chat_id) && !TelegramMenu::_self->_septic)
        {
            reply = "Септик недоступен";
            return true;
        }
        const String text = TelegramMenuSeptic::septicListTextHtml_(*TelegramMenu::_self, u.chat_id);
        bot.sendText(u.chat_id, text, "", "HTML");
        return true;
    }


    bool TelegramMenuSeptic::cmdSepticMonitor_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        (void)bot;
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        if (TelegramMenu::_self->isLocalSelected_(u.chat_id) && !TelegramMenu::_self->_septic)
        {
            reply = "Септик недоступен";
            return true;
        }
        const char *cmd = "/septic_monitor";
        String tail = u.text.substring(strlen(cmd));
        tail.trim();
        int space = tail.indexOf(' ');
        if (space <= 0)
        {
            reply = "Использование: /septic_monitor <id> <on|off>";
            return true;
        }
        String id_str = tail.substring(0, space);
        String val_str = tail.substring(space + 1);
        id_str.trim();
        val_str.trim();
        uint8_t id = 0;
        if (!TelegramMenuSeptic::parseSepticId_(id_str, id))
        {
            reply = "Неверный ID септика";
            return true;
        }
        bool on = false;
        if (!TelegramMenu::parseOnOff_(val_str, on))
        {
            reply = "Неверное значение (on/off)";
            return true;
        }
        if (!TelegramMenu::_self->isLocalSelected_(u.chat_id))
        {
            const uint32_t node_id = TelegramMenu::_self->selectedNodeId_(u.chat_id);
            if (node_id == 0 || !TelegramMenu::_self->_stack_master)
            {
                reply = "Септик недоступен";
                return true;
            }
            DynamicJsonDocument doc(256);
            doc["feature"] = (uint8_t)StackFeature::Septic;
            doc["action"] = "set";
            JsonObject params = doc["params"].to<JsonObject>();
            params["id"] = (unsigned)id;
            params["monitor"] = on;
            char payload[256] = {};
            const size_t n = serializeJson(doc, payload, sizeof(payload));
            const bool ok = (n > 0) && TelegramMenu::_self->_stack_master->sendTo(
                                         node_id, (uint8_t)StackMsgType::CmdSet,
                                         reinterpret_cast<const uint8_t *>(payload), n);
            if (ok && TelegramMenu::_self->_stack_cache)
                TelegramMenu::_self->_stack_cache->requestSeptic(node_id);
            reply = ok ? (on ? "Питание: 🟢" : "Питание: ⚪") : "Не удалось";
            return true;
        }
        if (!TelegramMenu::_self->_septic->setMonitoring(id, on))
        {
            reply = "Не удалось";
            return true;
        }
        reply = on ? "Питание: 🟢" : "Питание: ⚪";
        return true;
    }


    String TelegramMenuSeptic::septicStatusText_(TelegramMenu &self, int64_t chat_id)
    {
        if (!self.isLocalSelected_(chat_id))
        {
            if (!self._stack_cache)
                return "Септик недоступен";
            const uint32_t node_id = self.selectedNodeId_(chat_id);
            if (node_id == 0)
                return "Септик недоступен";
            const auto *cache = self._stack_cache->septicCache(node_id);
            if (!cache || !cache->has_data)
            {
                self._stack_cache->requestSeptic(node_id);
                return "Септик:\n  обновление...";
            }
            if (cache->item_count == 0)
                return "Септик:\n  пусто";
            const StackCache::StackSepticItem &it = cache->items[0];
            String out = F("Септик:\n");
            out += F("  Питание: ");
            out += it.monitor ? "🟢" : "⚪";
            out += F("\n  Предупреждение: ");
            out += it.warning ? "🟢" : "⚪";
            out += F("\n  Тревога: ");
            out += it.alarm ? "🟢" : "⚪";
            return out;
        }
        if (!self._septic)
            return "Септик недоступен";
        String out = F("Септик:\n");
        const auto *cfg = self._septic->configByIndex(0);
        const auto *st = self._septic->stateByIndex(0);
        if (cfg && st)
        {
            out += F("\n  Питание: ");
            out += cfg->monitoring_on ? "🟢" : "⚪";
            out += F("\n  Предупреждение: ");
            out += st->warning ? "🟢" : "⚪";
            out += F("\n  Тревога: ");
            out += st->alarm ? "🟢" : "⚪";
        }
        return out;
    }


    String TelegramMenuSeptic::septicListTextHtml_(TelegramMenu &self, int64_t chat_id)
    {
        if (!self.isLocalSelected_(chat_id))
        {
            if (!self._stack_cache)
                return "Септик недоступен";
            const uint32_t node_id = self.selectedNodeId_(chat_id);
            if (node_id == 0)
                return "Септик недоступен";
            const auto *cache = self._stack_cache->septicCache(node_id);
            if (!cache || !cache->has_data)
            {
                self._stack_cache->requestSeptic(node_id);
                return "<b>Септик:</b>\nобновление...";
            }
            String out = F("<b>Септик:</b>");
            bool any = false;
            for (size_t i = 0; i < cache->item_count; ++i)
            {
                const auto &it = cache->items[i];
                if (!it.enabled)
                    continue;
                any = true;
                out += F("\n  ");
                out += String((unsigned)it.id);
                out += F(": <b>Септик ");
                out += String((unsigned)it.id);
                out += F("</b>");
                out += F("\n    питание: <b>");
                out += it.monitor ? "🟢" : "⚪";
                out += F("</b>\n    предупреждение: <b>");
                out += it.warning ? "🟢" : "⚪";
                out += F("</b>\n    тревога: <b>");
                out += it.alarm ? "🟢" : "⚪";
                out += F("</b>");
            }
            if (!any)
                out += F("\n  пусто");
            return out;
        }
        if (!self._septic)
            return "Септик недоступен";
        String out = F("<b>Септик:</b>");
        bool any = false;
        for (size_t i = 0; i < SepticController::kSepticCount; ++i)
        {
            const auto *cfg = self._septic->configByIndex(i);
            const auto *st = self._septic->stateByIndex(i);
            if (!cfg || !st || !cfg->enabled)
                continue;
            any = true;
            out += F("\n  ");
            out += String((unsigned)cfg->id);
            out += F(": <b>");
            if (cfg->name.length())
                out += self.escapeHtml_(cfg->name);
            else
                out += F("-");
            out += F("</b>");
            out += F("\n    питание: <b>");
            out += cfg->monitoring_on ? "🟢" : "⚪";
            out += F("</b>\n    предупреждение: <b>");
            out += st->warning ? "🟢" : "⚪";
            out += F("</b>\n    тревога: <b>");
            out += st->alarm ? "🟢" : "⚪";
            out += F("</b>\n    w port: <b>");
            if (cfg->warning_port != SepticController::kInvalidPort)
                out += String((unsigned)cfg->warning_port);
            else
                out += F("none");
            out += F("</b>\n    a port: <b>");
            if (cfg->alarm_port != SepticController::kInvalidPort)
                out += String((unsigned)cfg->alarm_port);
            else
                out += F("none");
            out += F("</b>");
        }
        if (!any)
            out += F("\n  пусто");
        return out;
    }


    String TelegramMenuSeptic::septicControlMarkup_(TelegramMenu &self, int64_t chat_id)
    {
        (void)self;
        (void)chat_id;
        String out = F("{\"keyboard\":[[\"");
        out += TelegramMenu::escapeJson_(F("Питание 🟢"));
        out += F("\",\"");
        out += TelegramMenu::escapeJson_(F("Питание ⚪"));
        out += F("\"],[\"");
        out += TelegramMenu::escapeJson_(F("Статус"));
        out += F("\"],[\"");
        out += TelegramMenu::escapeJson_(F("Назад"));
        out += F("\"]],\"resize_keyboard\":true,\"one_time_keyboard\":false}");
        return out;
    }


    void TelegramMenuSeptic::sendSepticMenu_(TelegramMenu &self, int64_t chat_id)
    {
        if (!self._bot)
            return;
        if (self.isLocalSelected_(chat_id) && !self._septic)
        {
            self._bot->sendText(chat_id, F("Септик недоступен"));
            return;
        }
        const String markup = TelegramMenuSeptic::septicControlMarkup_(self, chat_id);
        const String text = TelegramMenuSeptic::septicStatusText_(self, chat_id);
        self._bot->setMenu(chat_id, "septic");
        self._bot->sendText(chat_id, text, markup);
    }


    bool TelegramMenuSeptic::handleSepticSelection_(TelegramMenu &self, const TelegramClient::Update &u)
    {
        if (!self._bot)
            return false;
        const char *menu_id = self._bot->currentMenuId(u.chat_id);
        if (!menu_id || strcmp(menu_id, "septic") != 0)
            return false;
        if (u.text.startsWith("/"))
            return false;
        if (u.text == F("Назад"))
        {
            self._bot->enterMenu(u.chat_id, "device");
            return true;
        }
        if (self.isLocalSelected_(u.chat_id) && !self._septic)
        {
            self._bot->sendText(u.chat_id, F("Септик недоступен"));
            return true;
        }
        if (u.text == F("Статус"))
        {
            TelegramMenuSeptic::sendSepticMenu_(self, u.chat_id);
            return true;
        }
        if (u.text == F("Питание 🟢") || u.text == F("Питание ⚪") ||
            u.text == F("Мониторинг 🟢") || u.text == F("Мониторинг ⚪") ||
            u.text == F("Мониторинг Вкл") || u.text == F("Мониторинг Выкл"))
        {
            const bool on = (u.text == F("Питание 🟢") ||
                             u.text == F("Мониторинг 🟢") || u.text == F("Мониторинг Вкл"));
            if (!self.isLocalSelected_(u.chat_id))
            {
                const uint32_t node_id = self.selectedNodeId_(u.chat_id);
                if (node_id == 0 || !self._stack_master)
                {
                    self._bot->sendText(u.chat_id, F("Септик недоступен"));
                    return true;
                }
                uint8_t id = 1;
                if (self._stack_cache)
                {
                    const auto *cache = self._stack_cache->septicCache(node_id);
                    if (!cache || !cache->has_data)
                    {
                        self._stack_cache->requestSeptic(node_id);
                    }
                    else if (cache->item_count > 0 && cache->items[0].id > 0)
                    {
                        id = cache->items[0].id;
                    }
                }
                DynamicJsonDocument doc(256);
                doc["feature"] = (uint8_t)StackFeature::Septic;
                doc["action"] = "set";
                JsonObject params = doc["params"].to<JsonObject>();
                params["id"] = (unsigned)id;
                params["monitor"] = on;
                char payload[256] = {};
                const size_t n = serializeJson(doc, payload, sizeof(payload));
                const bool ok = (n > 0) && self._stack_master->sendTo(
                                             node_id, (uint8_t)StackMsgType::CmdSet,
                                             reinterpret_cast<const uint8_t *>(payload), n);
                if (ok && self._stack_cache)
                    self._stack_cache->requestSeptic(node_id);
                if (!ok)
                    self._bot->sendText(u.chat_id, F("Не удалось"));
                TelegramMenuSeptic::sendSepticMenu_(self, u.chat_id);
                return true;
            }
            const auto *cfg = self._septic->configByIndex(0);
            if (!cfg)
            {
                self._bot->sendText(u.chat_id, F("Септик не настроен"));
                return true;
            }
            self._septic->setMonitoring(cfg->id, on);
            TelegramMenuSeptic::sendSepticMenu_(self, u.chat_id);
            return true;
        }
        return false;
    }


    bool TelegramMenuSeptic::parseSepticId_(const String &text, uint8_t &out)
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
        if (v <= 0 || v > (int)SepticController::kSepticCount)
            return false;
        out = (uint8_t)v;
        return true;
    }
