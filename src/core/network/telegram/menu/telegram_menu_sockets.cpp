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

#include "core/network/telegram/telegram_menu.hpp"

#include <stdlib.h>
#include <string.h>

#include "controllers/socket_controller.hpp"
#include "core/network/stack/stack_cache.hpp"
#include "core/network/stack/stack_features.hpp"
#include "core/network/stack/stack_master.hpp"
#include "core/network/wifi_manager.hpp"
#include "core/rtc.hpp"
#include "plc/plc_control.hpp"
#include "utils/configs.hpp"
#include "utils/logger.hpp"

#include "core/network/telegram/menu/telegram_menu_sockets.hpp"

// ---- telegram menu extracted definitions ----
namespace
{
static constexpr uint32_t kTelegramSocketLockTimeoutMs = 300;
static constexpr uint8_t kTelegramSocketMenuVerifyRetries = 8;
}

bool TelegramMenuSockets::cmdSockets_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        (void)reply;
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        TelegramMenuSockets::sendSocketMenu_(*TelegramMenu::_self, u.chat_id, false);
        return true;
    }


    bool TelegramMenuSockets::cmdLights_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        (void)reply;
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        TelegramMenuSockets::sendSocketMenu_(*TelegramMenu::_self, u.chat_id, true);
        return true;
    }


    bool TelegramMenuSockets::cmdSocketList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
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


    bool TelegramMenuSockets::cmdSocketOn_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        return TelegramMenuSockets::startSocketAction_(*TelegramMenu::_self, bot, u, reply, 1);
    }


    bool TelegramMenuSockets::cmdSocketOff_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        return TelegramMenuSockets::startSocketAction_(*TelegramMenu::_self, bot, u, reply, 2);
    }


    bool TelegramMenuSockets::cmdSocketToggle_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        return TelegramMenuSockets::startSocketAction_(*TelegramMenu::_self, bot, u, reply, 3);
    }


    bool TelegramMenuSockets::startSocketAction_(TelegramMenu &self, TelegramBot &bot, const TelegramClient::Update &u, String &reply, uint8_t action)
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


void TelegramMenuSockets::buildSocketLabels_(TelegramMenu &self, std::vector<String> &out, bool lights_only )
    {
        out.clear();
        if (!self._sockets)
        {
            out.reserve(1);
        }
        else if (lights_only)
        {
            out.reserve(SocketController::kLightCount + 1);
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
            out.reserve(SocketController::kSocketCount + 1);
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
        out.push_back(F("Назад"));
    }


    String TelegramMenuSockets::socketListTextHtml_(TelegramMenu &self, int64_t chat_id, bool lights_only )
    {
        const char *menu_id = lights_only ? "lights" : "sockets";
        bool groups_enabled = self.isLocalSelected_(chat_id) ? (self._sockets && self.hasLocalGroups_())
                                                             : (self._stack_cache && self.hasGroups_(chat_id));
        const bool group_active = groups_enabled && self.groupFilterActive_(chat_id, menu_id);
        String out = lights_only ? F("<b>Свет:</b>") : F("<b>Розетки:</b>");
        out.reserve(512);
        if (self.isLocalSelected_(chat_id))
        {
            if (!self._sockets)
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
                    if (lights_only)
                    {
                        for (size_t i = 0; i < SocketController::kLightCount; ++i)
                        {
                            const auto *cfg = self._sockets->lightConfigByIndex(i);
                            const auto *st = self._sockets->lightStateByIndex(i);
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
                            out += st->relay_on ? F("🟡 ") : F("⚪ ");
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
                            out += st->relay_on ? F("🟢 ") : F("⚪ ");
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
                                out += "-";
                            }
                        }
                    }
                }
                if (has_no_group)
                {
                    out += F("\n  [Без группы]");
                    any = true;
                    if (lights_only)
                    {
                        for (size_t i = 0; i < SocketController::kLightCount; ++i)
                        {
                            const auto *cfg = self._sockets->lightConfigByIndex(i);
                            const auto *st = self._sockets->lightStateByIndex(i);
                            if (!cfg || !st || !cfg->enabled || cfg->group_id != 0)
                                continue;
                            out += F("\n    ");
                            out += st->relay_on ? F("🟡 ") : F("⚪ ");
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
                            if (!cfg || !st || !cfg->enabled || cfg->group_id != 0)
                                continue;
                            out += F("\n    ");
                            out += st->relay_on ? F("🟢 ") : F("⚪ ");
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
                                out += "-";
                            }
                        }
                    }
                }
                if (!any)
                    out += F("\n  пусто");
                return out;
            }
            if (groups_enabled && group_active)
            {
                out += F("\n  группа: <b>");
                out += self.escapeHtml_(self.groupLabelById_(chat_id, self.groupFilterId_(chat_id, menu_id)));
                out += F("</b>");
            }
            bool any = false;
            if (lights_only)
            {
                const uint8_t active_group_id = groups_enabled ? self.groupFilterId_(chat_id, menu_id) : 0;
                for (size_t i = 0; i < SocketController::kLightCount; ++i)
                {
                    const auto *cfg = self._sockets->lightConfigByIndex(i);
                    const auto *st = self._sockets->lightStateByIndex(i);
                    if (!cfg || !st || !cfg->enabled)
                        continue;
                    if (groups_enabled && cfg->group_id != active_group_id)
                        continue;
                    any = true;
                    out += "\n  ";
                    out += st->relay_on ? F("🟡 ") : F("⚪ ");
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
                const uint8_t active_group_id = groups_enabled ? self.groupFilterId_(chat_id, menu_id) : 0;
                for (size_t i = 0; i < SocketController::kSocketCount; ++i)
                {
                    const auto *cfg = self._sockets->configByIndex(i);
                    const auto *st = self._sockets->stateByIndex(i);
                    if (!cfg || !st || !cfg->enabled)
                        continue;
                    if (groups_enabled && cfg->group_id != active_group_id)
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
        if (groups_enabled && !group_active)
        {
            bool any_group = false;
            bool has_no_group = false;
            const auto *groups = self.selectedGroupsCache_(chat_id);
            if (!groups || !groups->has_data || !groups->items)
            {
                out += F("\n  РїСѓСЃС‚Рѕ");
                return out;
            }
            if (lights_only)
            {
                const auto *cache = self._stack_cache->lightsCache(node_id);
                if (!cache || !cache->has_data)
                {
                    self._stack_cache->requestLights(node_id);
                    out += F("\n  РѕР±РЅРѕРІР»РµРЅРёРµ...");
                    return out;
                }
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
                        out += F("\n    ");
                        out += it.state ? F("рџџЎ ") : F("вљЄ ");
                        out += String((unsigned)it.id);
                        out += F(": ");
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
                if (has_no_group)
                {
                    out += F("\n  [Р‘РµР· РіСЂСѓРїРїС‹]");
                    any_group = true;
                    for (size_t i = 0; i < cache->item_count; ++i)
                    {
                        const auto &it = cache->items[i];
                        if (!it.enabled || it.group_id != 0)
                            continue;
                        out += F("\n    ");
                        out += it.state ? F("рџџЎ ") : F("вљЄ ");
                        out += String((unsigned)it.id);
                        out += F(": ");
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
                    out += F("\n  РѕР±РЅРѕРІР»РµРЅРёРµ...");
                    return out;
                }
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
                        out += F("\n    ");
                        out += it.state ? F("рџџў ") : F("вљЄ ");
                        out += String((unsigned)it.id);
                        out += F(": ");
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
                if (has_no_group)
                {
                    out += F("\n  [Р‘РµР· РіСЂСѓРїРїС‹]");
                    any_group = true;
                    for (size_t i = 0; i < cache->item_count; ++i)
                    {
                        const auto &it = cache->items[i];
                        if (!it.enabled || it.group_id != 0)
                            continue;
                        out += F("\n    ");
                        out += it.state ? F("рџџў ") : F("вљЄ ");
                        out += String((unsigned)it.id);
                        out += F(": ");
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
            if (!any_group)
                out += F("\n  РїСѓСЃС‚Рѕ");
            return out;
        }
        if (groups_enabled && group_active)
        {
            out += F("\n  РіСЂСѓРїРїР°: <b>");
            out += self.escapeHtml_(self.groupLabelById_(chat_id, self.groupFilterId_(chat_id, menu_id)));
            out += F("</b>");
        }
        const uint8_t active_group_id = groups_enabled ? self.groupFilterId_(chat_id, menu_id) : 0;
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
                    if (groups_enabled && it.group_id != active_group_id)
                        continue;
                    any = true;
                    out += "\n  ";
                    out += it.state ? F("🟡 ") : F("⚪ ");
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
                    if (groups_enabled && it.group_id != active_group_id)
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


    void TelegramMenuSockets::sendSocketMenu_(TelegramMenu &self, int64_t chat_id, bool lights_only)
    {
        if (!self._bot)
            return;
        const uint32_t started = millis();
        const char *menu_id = lights_only ? "lights" : "sockets";
        TelegramMenu::ChatAuth *st = self.ensureAuth_(chat_id);
        if (st)
        {
            st->awaiting_socket = false;
            st->socket_action = 0;
        }
        std::vector<String> labels;
        if (self.isLocalSelected_(chat_id))
        {
            const bool groups_enabled = self._sockets && self.hasLocalGroups_();
            const bool group_active = groups_enabled && self.groupFilterActive_(chat_id, menu_id);
            if (groups_enabled && !group_active)
            {
                bool has_no_group = false;
                for (size_t gi = 0; gi < self._configs_manager->groupCount(); ++gi)
                {
                    ConfigsManagerIface::GroupConfig g;
                    if (!self._configs_manager->groupByIndex(gi, g) || g.id == 0)
                        continue;
                    bool present = false;
                    if (lights_only)
                    {
                        for (size_t i = 0; i < SocketController::kLightCount; ++i)
                        {
                            const auto *cfg = self._sockets->lightConfigByIndex(i);
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
                    }
                    else
                    {
                        for (size_t i = 0; i < SocketController::kSocketCount; ++i)
                        {
                            const auto *cfg = self._sockets->configByIndex(i);
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
                const uint8_t active_group_id = groups_enabled ? self.groupFilterId_(chat_id, menu_id) : 0;
                if (lights_only)
                {
                    for (size_t i = 0; i < SocketController::kLightCount; ++i)
                    {
                        const auto *cfg = self._sockets->lightConfigByIndex(i);
                        if (!cfg || !cfg->enabled)
                            continue;
                        if (groups_enabled && cfg->group_id != active_group_id)
                            continue;
                        String label = String((unsigned)cfg->id) + ": ";
                        label += cfg->name.length() ? cfg->name : String("Свет");
                        labels.push_back(label);
                    }
                }
                else
                {
                    for (size_t i = 0; i < SocketController::kSocketCount; ++i)
                    {
                        const auto *cfg = self._sockets->configByIndex(i);
                        if (!cfg || !cfg->enabled)
                            continue;
                        if (groups_enabled && cfg->group_id != active_group_id)
                            continue;
                        String label = String((unsigned)cfg->id) + ": ";
                        label += cfg->name.length() ? cfg->name : String("Розетка");
                        labels.push_back(label);
                    }
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
            const bool group_active = groups_enabled && self.groupFilterActive_(chat_id, menu_id);
            if (node_id != 0)
            {
                if (groups_enabled && !group_active)
                {
                    bool has_no_group = false;
                    const auto *groups = self.selectedGroupsCache_(chat_id);
                    if (groups && groups->has_data && groups->items)
                    {
                        for (size_t gi = 0; gi < groups->item_count; ++gi)
                        {
                            const auto &g = groups->items[gi];
                            if (g.id == 0 || !g.name[0])
                                continue;
                            bool present = false;
                            if (lights_only)
                            {
                                const auto *cache = self._stack_cache->lightsCache(node_id);
                                if (!cache || !cache->has_data)
                                {
                                    self._stack_cache->requestLights(node_id);
                                    break;
                                }
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
                            }
                            else
                            {
                                const auto *cache = self._stack_cache->socketsCache(node_id);
                                if (!cache || !cache->has_data)
                                {
                                    self._stack_cache->requestSockets(node_id);
                                    break;
                                }
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
                            }
                            if (present)
                                labels.push_back(String(g.name));
                        }
                    }
                    if (has_no_group)
                        labels.push_back(F("Р‘РµР· РіСЂСѓРїРїС‹"));
                }
                else if (lights_only)
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
                            if (groups_enabled && it.group_id != self.groupFilterId_(chat_id, menu_id))
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
                            if (groups_enabled && it.group_id != self.groupFilterId_(chat_id, menu_id))
                                continue;
                            String label;
                            label += String((unsigned)it.id);
                            label += ": ";
                            label += it.name[0] ? String(it.name) : String("Розетка");
                            labels.push_back(label);
                        }
                    }
                }
                if (groups_enabled)
                    labels.push_back(F("Р“СЂСѓРїРїС‹"));
            }
            labels.push_back(F("Назад"));
        }
        else
        {
            labels.push_back(F("Назад"));
        }
        const String markup = TelegramMenu::buildKeyboardMarkup_(labels);
        const String list = TelegramMenuSockets::socketListTextHtml_(self, chat_id, lights_only);
        const uint32_t built_ms = millis() - started;
        self._bot->setMenu(chat_id, lights_only ? "lights" : "sockets");
        const uint32_t send_started = millis();
        self._bot->sendText(chat_id, list, markup, "HTML");
        const uint32_t send_ms = millis() - send_started;
        if (self._logs && self._logs->ready() && (built_ms >= 200u || send_ms >= 200u))
        {
            self._logs->warn(F("TGBOT"),
                             F("Socket menu slow: build_ms: %lu send_ms: %lu local: %u lights: %u chat: %lld"),
                             (unsigned long)built_ms, (unsigned long)send_ms,
                             self.isLocalSelected_(chat_id) ? 1u : 0u,
                             lights_only ? 1u : 0u,
                             (long long)chat_id);
        }
    }


    bool TelegramMenuSockets::handleSocketToggleSelection_(TelegramMenu &self, const TelegramClient::Update &u)
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
            self.clearGroupFilter_(u.chat_id, lights_only ? "lights" : "sockets");
            self._bot->enterMenu(u.chat_id, "device");
            return true;
        }
        if (((self.isLocalSelected_(u.chat_id) && self._sockets) || (!self.isLocalSelected_(u.chat_id) && self._stack_cache)) &&
            self.hasGroups_(u.chat_id))
        {
            const char *group_menu_id = lights_only ? "lights" : "sockets";
            if (!self.groupFilterActive_(u.chat_id, group_menu_id))
            {
                uint8_t group_id = 0;
                if (!self.parseGroupLabel_(u.chat_id, u.text, group_id))
                {
                    self._bot->sendText(u.chat_id, F("Неизвестная группа"));
                    return true;
                }
                self.setGroupFilter_(u.chat_id, group_menu_id, group_id);
                TelegramMenuSockets::sendSocketMenu_(self, u.chat_id, lights_only);
                return true;
            }
            if (u.text == F("Группы"))
            {
                self.clearGroupFilter_(u.chat_id, group_menu_id);
                TelegramMenuSockets::sendSocketMenu_(self, u.chat_id, lights_only);
                return true;
            }
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
            ok = lights_only ? self._sockets->toggleLightRelayById(id, kTelegramSocketLockTimeoutMs)
                             : self._sockets->toggleRelayById(id, kTelegramSocketLockTimeoutMs);
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
            self._bot->sendText(u.chat_id,
                                self.isLocalSelected_(u.chat_id) ? F("Контроллер занят, повторите")
                                                                 : F("Не удалось"));
            return true;
        }
        self._bot->sendText(u.chat_id, F("OK"));
        return true;
    }


    bool TelegramMenuSockets::parseSocketIdFromText_(const String &text, uint8_t &out, uint8_t max_id)
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


    bool TelegramMenuSockets::parseSocketId_(const String &text, uint8_t &out, uint8_t max_id)
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


    bool TelegramMenuSockets::parseSocketLabel_(const String &text, uint8_t &out, uint8_t max_id)
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
