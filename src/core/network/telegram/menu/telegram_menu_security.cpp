#include "core/network/telegram/telegram_menu.hpp"

#include <stdlib.h>
#include <string.h>

#include "controllers/security_controller.hpp"
#include "core/network/stack/stack_cache.hpp"
#include "core/network/stack/stack_features.hpp"
#include "core/network/stack/stack_master.hpp"
#include "core/network/wifi_manager.hpp"
#include "core/rtc.hpp"
#include "plc/plc_control.hpp"
#include "utils/configs.hpp"
#include "utils/logger.hpp"

#include "core/network/telegram/menu/telegram_menu_security.hpp"

// ---- telegram menu extracted definitions ----
bool TelegramMenuSecurity::cmdSecurity_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        (void)reply;
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        if (TelegramMenu::_self->isLocalSelected_(u.chat_id) && !TelegramMenu::_self->_security)
        {
            reply = "Охрана недоступна";
            return true;
        }
        TelegramMenuSecurity::sendSecurityMenu_(*TelegramMenu::_self, u.chat_id);
        return true;
    }


    bool TelegramMenuSecurity::cmdSecurityStatus_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        if (TelegramMenu::_self->isLocalSelected_(u.chat_id) && !TelegramMenu::_self->_security)
        {
            reply = "Охрана недоступна";
            return true;
        }
        reply = TelegramMenuSecurity::securityStatusText_(*TelegramMenu::_self, u.chat_id);
        return true;
    }


    bool TelegramMenuSecurity::cmdSecurityList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        if (TelegramMenu::_self->isLocalSelected_(u.chat_id) && !TelegramMenu::_self->_security)
        {
            reply = "Охрана недоступна";
            return true;
        }
        const String text = TelegramMenuSecurity::securityListTextHtml_(*TelegramMenu::_self, u.chat_id);
        bot.sendText(u.chat_id, text, "", "HTML");
        return true;
    }


    bool TelegramMenuSecurity::cmdSecurityArm_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        if (TelegramMenu::_self->isLocalSelected_(u.chat_id) && !TelegramMenu::_self->_security)
        {
            reply = "Охрана недоступна";
            return true;
        }
        if (TelegramMenu::_self->isLocalSelected_(u.chat_id))
        {
            const String user = TelegramMenuSecurity::userFromChat_(*TelegramMenu::_self, u);
            if (TelegramMenu::_self->_security->armFrom("telegram", user))
                reply = "Охрана включена";
            else
                reply = "Контроллер охраны выключен";
        }
        else
        {
            const uint32_t node_id = TelegramMenu::_self->selectedNodeId_(u.chat_id);
            if (node_id == 0 || !TelegramMenu::_self->_stack_master)
            {
                reply = "Охрана недоступна";
                return true;
            }
            DynamicJsonDocument doc(256);
            doc["feature"] = (uint8_t)StackFeature::Security;
            doc["action"] = "set";
            JsonObject params = doc["params"].to<JsonObject>();
            params["armed"] = true;
            params["user"] = TelegramMenuSecurity::userFromChat_(*TelegramMenu::_self, u);
            char payload[256] = {};
            const size_t n = serializeJson(doc, payload, sizeof(payload));
            const bool ok = (n > 0) && TelegramMenu::_self->_stack_master->sendTo(
                                         node_id, (uint8_t)StackMsgType::CmdSet,
                                         reinterpret_cast<const uint8_t *>(payload), n);
            if (ok && TelegramMenu::_self->_stack_cache)
                TelegramMenu::_self->_stack_cache->requestSecurity(node_id);
            reply = ok ? "Команда отправлена" : "Не удалось";
        }
        return true;
    }


    bool TelegramMenuSecurity::cmdSecurityDisarm_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        if (TelegramMenu::_self->isLocalSelected_(u.chat_id) && !TelegramMenu::_self->_security)
        {
            reply = "Охрана недоступна";
            return true;
        }
        if (TelegramMenu::_self->isLocalSelected_(u.chat_id))
        {
            const String user = TelegramMenuSecurity::userFromChat_(*TelegramMenu::_self, u);
            TelegramMenu::_self->_security->disarmFrom("telegram", user);
            reply = "Охрана выключена";
        }
        else
        {
            const uint32_t node_id = TelegramMenu::_self->selectedNodeId_(u.chat_id);
            if (node_id == 0 || !TelegramMenu::_self->_stack_master)
            {
                reply = "Охрана недоступна";
                return true;
            }
            DynamicJsonDocument doc(256);
            doc["feature"] = (uint8_t)StackFeature::Security;
            doc["action"] = "set";
            JsonObject params = doc["params"].to<JsonObject>();
            params["armed"] = false;
            params["user"] = TelegramMenuSecurity::userFromChat_(*TelegramMenu::_self, u);
            char payload[256] = {};
            const size_t n = serializeJson(doc, payload, sizeof(payload));
            const bool ok = (n > 0) && TelegramMenu::_self->_stack_master->sendTo(
                                         node_id, (uint8_t)StackMsgType::CmdSet,
                                         reinterpret_cast<const uint8_t *>(payload), n);
            if (ok && TelegramMenu::_self->_stack_cache)
                TelegramMenu::_self->_stack_cache->requestSecurity(node_id);
            reply = ok ? "Команда отправлена" : "Не удалось";
        }
        return true;
    }


    bool TelegramMenuSecurity::cmdSecuritySilent_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        (void)bot;
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        if (!TelegramMenu::_self->_security)
        {
            reply = "Охрана недоступна";
            return true;
        }
        if (!TelegramMenu::_self->isLocalSelected_(u.chat_id))
        {
            reply = "Доступно только для локального устройства";
            return true;
        }
        const char *cmd = "/security_silent";
        String tail = u.text.substring(strlen(cmd));
        tail.trim();
        int space = tail.indexOf(' ');
        if (space <= 0)
        {
            reply = "Использование: /security_silent <id> <on|off>";
            return true;
        }
        String id_str = tail.substring(0, space);
        String val_str = tail.substring(space + 1);
        id_str.trim();
        val_str.trim();
        uint8_t id = 0;
        if (!TelegramMenuSecurity::parseSecurityId_(id_str, id))
        {
            reply = "Неверный ID охраны";
            return true;
        }
        bool silent = false;
        if (!TelegramMenu::parseOnOff_(val_str, silent))
        {
            reply = "Неверное значение (on/off)";
            return true;
        }
        if (!TelegramMenu::_self->_security->setSilent(id, silent))
        {
            reply = "Не удалось";
            return true;
        }
        reply = silent ? "Тихий режим включен" : "Тихий режим выключен";
        return true;
    }


    String TelegramMenuSecurity::securityStatusText_(TelegramMenu &self, int64_t chat_id)
    {
        if (!self.isLocalSelected_(chat_id))
        {
            if (!self._stack_cache)
                return "Охрана недоступна";
            const uint32_t node_id = self.selectedNodeId_(chat_id);
            if (node_id == 0)
                return "Охрана недоступна";
            const auto *cache = self._stack_cache->securityCache(node_id);
            if (!cache || !cache->has_data)
            {
                self._stack_cache->requestSecurity(node_id);
                return "Охрана:\n  обновление...";
            }
            String out = F("Охрана:\n");
            out += F("  Статус: ");
            out += cache->armed ? "🟢" : "⚪";
            out += F("\n  Тревога: ");
            out += cache->alarm ? "🔴" : "⚪";
            out += F("\n  Датчики:\n");
            for (size_t i = 0; i < cache->item_count; ++i)
            {
                const auto &it = cache->items[i];
                if (!it.enabled)
                    continue;
                out += F("    ");
                out += it.detect ? "🔴 " : "🟢 ";
                out += it.name[0] ? String(it.name) : String(F("датчик ")) + String((unsigned)it.id);
                String type_label;
                if (it.type[0])
                {
                    String t = String(it.type);
                    String tl = t;
                    tl.toLowerCase();
                    if (tl == "reed")
                        type_label = "Reed";
                    else if (tl == "pir")
                        type_label = "PIR";
                    else
                        type_label = t;
                }
                if (type_label.length())
                {
                    out += F(" [");
                    out += type_label;
                    out += F("]");
                }
                out += F("\n");
            }
            return out;
        }
        if (!self._security)
            return "Охрана недоступна";
        String out = F("Охрана:\n");
        out += F("  Статус: ");
        out += self._security->armed() ? "🟢" : "⚪";
        out += F("\n  Тревога: ");
        out += self._security->alarmOn() ? "🔴" : "⚪";
        out += F("\n  Датчики:\n");
        const bool has_groups = self.hasLocalGroups_();
        if (has_groups)
        {
            bool any = false;
            bool has_no_group = false;
            for (size_t gi = 0; gi < self._configs_manager->groupCount(); ++gi)
            {
                ConfigsManagerIface::GroupConfig g;
                if (!self._configs_manager->groupByIndex(gi, g) || g.id == 0)
                    continue;
                bool group_any = false;
                for (size_t i = 0; i < SecurityController::kSensorCount; ++i)
                {
                    const auto *cfg = self._security->configByIndex(i);
                    const auto *st = self._security->stateByIndex(i);
                    if (!cfg || !st || !cfg->enabled)
                        continue;
                    if (cfg->group_id == 0)
                        has_no_group = true;
                    if (cfg->group_id != g.id)
                        continue;
                    if (!group_any)
                    {
                        out += F("    [");
                        out += g.name;
                        out += F("]\n");
                        group_any = true;
                        any = true;
                    }
                    out += F("      ");
                    out += st->is_detect ? "рџ”ґ " : "рџџў ";
                    if (cfg->name.length())
                        out += cfg->name;
                    else
                        out += String(F("РґР°С‚С‡РёРє ")) + String((unsigned)cfg->id);
                    out += F(" [");
                    out += (cfg->type == SecurityController::SensorType::Reed) ? F("Reed") : F("PIR");
                    out += F("]\n");
                }
            }
            if (has_no_group)
            {
                out += F("    [Без группы]\n");
                any = true;
                for (size_t i = 0; i < SecurityController::kSensorCount; ++i)
                {
                    const auto *cfg = self._security->configByIndex(i);
                    const auto *st = self._security->stateByIndex(i);
                    if (!cfg || !st || !cfg->enabled || cfg->group_id != 0)
                        continue;
                    out += F("      ");
                    out += st->is_detect ? "рџ”ґ " : "рџџў ";
                    if (cfg->name.length())
                        out += cfg->name;
                    else
                        out += String(F("РґР°С‚С‡РёРє ")) + String((unsigned)cfg->id);
                    out += F(" [");
                    out += (cfg->type == SecurityController::SensorType::Reed) ? F("Reed") : F("PIR");
                    out += F("]\n");
                }
            }
            if (!any)
                out += F("    РїСѓСЃС‚Рѕ\n");
            return out;
        }
        for (size_t i = 0; i < SecurityController::kSensorCount; ++i)
        {
            const auto *cfg = self._security->configByIndex(i);
            const auto *st = self._security->stateByIndex(i);
            if (!cfg || !st || !cfg->enabled)
                continue;
            out += F("    ");
            out += st->is_detect ? "🔴 " : "🟢 ";
            if (cfg->name.length())
                out += cfg->name;
            else
                out += String(F("датчик ")) + String((unsigned)cfg->id);
            out += F(" [");
            out += (cfg->type == SecurityController::SensorType::Reed) ? F("Reed") : F("PIR");
            out += F("]");
            out += F("\n");
        }
        return out;
    }


    String TelegramMenuSecurity::securityListTextHtml_(TelegramMenu &self, int64_t chat_id)
    {
        if (!self.isLocalSelected_(chat_id))
        {
            if (!self._stack_cache)
                return "Охрана недоступна";
            const uint32_t node_id = self.selectedNodeId_(chat_id);
            if (node_id == 0)
                return "Охрана недоступна";
            const auto *cache = self._stack_cache->securityCache(node_id);
            if (!cache || !cache->has_data)
            {
                self._stack_cache->requestSecurity(node_id);
                return "Охрана: обновление...";
            }
            String out = F("<b>Охрана:</b>\n");
            out += F("ID  Type  Port  Silent  Detect  Name\n");
            out += F("-----------------------------------\n");
            for (size_t i = 0; i < cache->item_count; ++i)
            {
                const auto &it = cache->items[i];
                if (!it.enabled)
                    continue;
                out += String((unsigned)it.id);
                out += F("  ");
                out += it.type[0] ? String(it.type) : String("--");
                out += F("  ");
                out += (it.port != SecurityController::kInvalidPort) ? String((unsigned)it.port) : String("--");
                out += F("  ");
                out += it.silent ? "yes" : "no";
                out += F("  ");
                out += it.detect ? "yes" : "no";
                out += F("  ");
                if (it.name[0])
                    out += self.escapeHtml_(String(it.name));
                out += F("\n");
            }
            return out;
        }
        if (!self._security)
            return "Охрана недоступна";
        String out = F("<b>Охрана:</b>\n");
        const bool has_groups = self.hasLocalGroups_();
        if (has_groups)
        {
            bool any = false;
            bool has_no_group = false;
            for (size_t gi = 0; gi < self._configs_manager->groupCount(); ++gi)
            {
                ConfigsManagerIface::GroupConfig g;
                if (!self._configs_manager->groupByIndex(gi, g) || g.id == 0)
                    continue;
                bool group_any = false;
                for (size_t i = 0; i < SecurityController::kSensorCount; ++i)
                {
                    const auto *cfg = self._security->configByIndex(i);
                    const auto *st = self._security->stateByIndex(i);
                    if (!cfg || !st || !cfg->enabled)
                        continue;
                    if (cfg->group_id == 0)
                        has_no_group = true;
                    if (cfg->group_id != g.id)
                        continue;
                    if (!group_any)
                    {
                        out += F("\n<b>[");
                        out += self.escapeHtml_(g.name);
                        out += F("]</b>\n");
                        group_any = true;
                        any = true;
                    }
                    out += String((unsigned)cfg->id);
                    out += F("  ");
                    out += (cfg->type == SecurityController::SensorType::Reed) ? "reed" : "pir";
                    out += F("  ");
                    out += (cfg->port != SecurityController::kInvalidPort) ? String((unsigned)cfg->port) : String("--");
                    out += F("  ");
                    out += cfg->silent ? "yes" : "no";
                    out += F("  ");
                    out += st->is_detect ? "yes" : "no";
                    out += F("  ");
                    if (cfg->name.length())
                        out += self.escapeHtml_(cfg->name);
                    out += F("\n");
                }
            }
            if (has_no_group)
            {
                out += F("\n<b>[Без группы]</b>\n");
                any = true;
                for (size_t i = 0; i < SecurityController::kSensorCount; ++i)
                {
                    const auto *cfg = self._security->configByIndex(i);
                    const auto *st = self._security->stateByIndex(i);
                    if (!cfg || !st || !cfg->enabled || cfg->group_id != 0)
                        continue;
                    out += String((unsigned)cfg->id);
                    out += F("  ");
                    out += (cfg->type == SecurityController::SensorType::Reed) ? "reed" : "pir";
                    out += F("  ");
                    out += (cfg->port != SecurityController::kInvalidPort) ? String((unsigned)cfg->port) : String("--");
                    out += F("  ");
                    out += cfg->silent ? "yes" : "no";
                    out += F("  ");
                    out += st->is_detect ? "yes" : "no";
                    out += F("  ");
                    if (cfg->name.length())
                        out += self.escapeHtml_(cfg->name);
                    out += F("\n");
                }
            }
            if (!any)
                out += F("\nРїСѓСЃС‚Рѕ");
            return out;
        }
        out += F("ID  Type  Port  Silent  Detect  Name\n");
        out += F("-----------------------------------\n");
        for (size_t i = 0; i < SecurityController::kSensorCount; ++i)
        {
            const auto *cfg = self._security->configByIndex(i);
            const auto *st = self._security->stateByIndex(i);
            if (!cfg || !st || !cfg->enabled)
                continue;
            out += String((unsigned)cfg->id);
            out += F("  ");
            out += (cfg->type == SecurityController::SensorType::Reed) ? "reed" : "pir";
            out += F("  ");
            if (cfg->port != SecurityController::kInvalidPort)
                out += String((unsigned)cfg->port);
            else
                out += F("--");
            out += F("  ");
            out += cfg->silent ? "yes" : "no";
            out += F("  ");
            out += st->is_detect ? "yes" : "no";
            out += F("  ");
            if (cfg->name.length())
                out += self.escapeHtml_(cfg->name);
            out += F("\n");
        }
        return out;
    }


    String TelegramMenuSecurity::securityControlMarkup_()
    {
        std::vector<String> labels;
        labels.reserve(5);
        labels.push_back(F("Взять под охрану"));
        labels.push_back(F("Снять с охраны"));
        labels.push_back(F("Статус"));
        labels.push_back(F("Сбросить детекты"));
        labels.push_back(F("Назад"));
        return TelegramMenu::buildKeyboardMarkup_(labels);
    }


    void TelegramMenuSecurity::sendSecurityMenu_(TelegramMenu &self, int64_t chat_id)
    {
        if (!self._bot)
            return;
        if (self.isLocalSelected_(chat_id) && !self._security)
        {
            self._bot->sendText(chat_id, F("Охрана недоступна"));
            return;
        }
        const String markup = TelegramMenuSecurity::securityControlMarkup_();
        const String text = TelegramMenuSecurity::securityStatusText_(self, chat_id);
        self._bot->setMenu(chat_id, "security");
        self._bot->sendText(chat_id, text, markup);
    }


    bool TelegramMenuSecurity::handleSecuritySelection_(TelegramMenu &self, const TelegramClient::Update &u)
    {
        if (!self._bot)
            return false;
        const char *menu_id = self._bot->currentMenuId(u.chat_id);
        if (!menu_id || strcmp(menu_id, "security") != 0)
            return false;
        if (u.text.startsWith("/"))
            return false;
        if (u.text == F("Назад"))
        {
            self._bot->enterMenu(u.chat_id, "device");
            return true;
        }
        if (self.isLocalSelected_(u.chat_id) && !self._security)
        {
            self._bot->sendText(u.chat_id, F("Охрана недоступна"));
            return true;
        }
        if (u.text == F("Статус"))
        {
            TelegramMenuSecurity::sendSecurityMenu_(self, u.chat_id);
            return true;
        }
        if (u.text == F("Взять под охрану"))
        {
            if (self.isLocalSelected_(u.chat_id))
            {
                const String user = TelegramMenuSecurity::userFromChat_(self, u);
                self._security->armFrom("telegram", user);
            }
            else
            {
                const uint32_t node_id = self.selectedNodeId_(u.chat_id);
                if (node_id != 0 && self._stack_master)
                {
                    DynamicJsonDocument doc(256);
                    doc["feature"] = (uint8_t)StackFeature::Security;
                    doc["action"] = "set";
                    JsonObject p = doc["params"].to<JsonObject>();
                    p["armed"] = true;
                    p["user"] = TelegramMenuSecurity::userFromChat_(self, u);
                    char payload[256] = {};
                    const size_t n = serializeJson(doc, payload, sizeof(payload));
                    if (n > 0)
                        self._stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                                                   reinterpret_cast<const uint8_t *>(payload), n);
                    if (self._stack_cache)
                        self._stack_cache->requestSecurity(node_id);
                }
            }
            TelegramMenuSecurity::sendSecurityMenu_(self, u.chat_id);
            return true;
        }
        if (u.text == F("Снять с охраны"))
        {
            if (self.isLocalSelected_(u.chat_id))
            {
                const String user = TelegramMenuSecurity::userFromChat_(self, u);
                self._security->disarmFrom("telegram", user);
            }
            else
            {
                const uint32_t node_id = self.selectedNodeId_(u.chat_id);
                if (node_id != 0 && self._stack_master)
                {
                    DynamicJsonDocument doc(256);
                    doc["feature"] = (uint8_t)StackFeature::Security;
                    doc["action"] = "set";
                    JsonObject p = doc["params"].to<JsonObject>();
                    p["armed"] = false;
                    p["user"] = TelegramMenuSecurity::userFromChat_(self, u);
                    char payload[256] = {};
                    const size_t n = serializeJson(doc, payload, sizeof(payload));
                    if (n > 0)
                        self._stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                                                   reinterpret_cast<const uint8_t *>(payload), n);
                    if (self._stack_cache)
                        self._stack_cache->requestSecurity(node_id);
                }
            }
            TelegramMenuSecurity::sendSecurityMenu_(self, u.chat_id);
            return true;
        }
        if (u.text == F("Сбросить детекты"))
        {
            if (self.isLocalSelected_(u.chat_id))
            {
                self._security->clearDetect();
            }
            else
            {
                const uint32_t node_id = self.selectedNodeId_(u.chat_id);
                if (node_id != 0 && self._stack_master)
                {
                    DynamicJsonDocument doc(256);
                    doc["feature"] = (uint8_t)StackFeature::Security;
                    doc["action"] = "set";
                    JsonObject p = doc["params"].to<JsonObject>();
                    p["clear"] = true;
                    char payload[256] = {};
                    const size_t n = serializeJson(doc, payload, sizeof(payload));
                    if (n > 0)
                        self._stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                                                   reinterpret_cast<const uint8_t *>(payload), n);
                    if (self._stack_cache)
                        self._stack_cache->requestSecurity(node_id);
                }
            }
            TelegramMenuSecurity::sendSecurityMenu_(self, u.chat_id);
            return true;
        }
        return false;
    }


    String TelegramMenuSecurity::userFromChat_(TelegramMenu &self, const TelegramClient::Update &u)
    {
        String user = self.normalizeUsername_(u.from);
        const auto users = self.allowedUsers();
        size_t idx = 0;
        if (u.chat_id != 0 && self.findAllowedUserByChatId_(u.chat_id, idx))
        {
            if (idx < users.size && users[idx].username.length())
                user = users[idx].username;
        }
        return user;
    }


    bool TelegramMenuSecurity::parseSecurityId_(const String &text, uint8_t &out)
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
        if (v <= 0 || v > (int)SecurityController::kSensorCount)
            return false;
        out = (uint8_t)v;
        return true;
    }
