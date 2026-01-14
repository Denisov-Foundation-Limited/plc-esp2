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

#include <Arduino.h>
#include <vector>

#include "core/network/telegram/telegram.hpp"

class TelegramBot
{
public:
    struct MenuItem
    {
        const char *label;
        const char *command;
        const char *next_menu;
        const char *reply;
    };

    struct Menu
    {
        const char *id;
        const char *title;
        const MenuItem *items;
        size_t count;
        const char *parent_id;
    };

    using CommandHandler = bool (*)(TelegramBot &bot, const TelegramClient::Update &upd, String &reply);
    using TextHandler = bool (*)(void *ctx, const TelegramClient::Update &upd);

    struct Command
    {
        const char *cmd;
        CommandHandler handler;
    };

    explicit TelegramBot(TelegramClient &client)
        : _client(client)
    {
    }

    TelegramClient &client() { return _client; }
    const TelegramClient &client() const { return _client; }

    void setMenus(const Menu *menus, size_t count, const char *root_id)
    {
        _menus = menus;
        _menu_count = count;
        _root_menu_id = root_id;
    }

    void setCommands(const Command *cmds, size_t count)
    {
        _cmds = cmds;
        _cmd_count = count;
    }

    void setTextHandler(TextHandler handler, void *ctx)
    {
        _text_handler = handler;
        _text_ctx = ctx;
    }

    void setMaxChats(size_t max_chats) { _max_chats = max_chats; }

    bool processUpdates(const std::vector<TelegramClient::Update> &updates)
    {
        bool handled = false;
        for (const auto &u : updates)
        {
            if (u.text.length() == 0 && !u.hasDocument())
                continue;
            if (handleUpdate_(u))
                handled = true;
        }
        return handled;
    }

    void bind(TelegramClient &client)
    {
        client.setUpdateHandler(&TelegramBot::onUpdates_, this);
    }

    bool sendText(int64_t chat_id, const String &text, const String &reply_markup = "")
    {
        String payload = buildMessagePayload_(chat_id, text, reply_markup);
        return _client.sendMessageRaw(payload);
    }

    bool showMenu(int64_t chat_id, const Menu *menu, const String &prefix = "")
    {
        if (!menu)
            return false;
        String text = menu->title ? String(menu->title) : String("Menu");
        if (prefix.length())
            text = prefix + "\n" + text;
        String markup = buildMenuMarkup_(*menu);
        return sendText(chat_id, text, markup);
    }

    bool goRoot(int64_t chat_id)
    {
        const Menu *menu = findMenu_(_root_menu_id);
        if (!menu)
            return false;
        setChatMenu_(chat_id, menu->id);
        return showMenu(chat_id, menu);
    }

    bool enterMenu(int64_t chat_id, const char *menu_id, const String &prefix = "")
    {
        const Menu *menu = findMenu_(menu_id);
        if (!menu)
            return false;
        setChatMenu_(chat_id, menu->id);
        return showMenu(chat_id, menu, prefix);
    }

private:
    struct ChatState
    {
        int64_t chat_id = 0;
        const char *menu_id = nullptr;
    };

    TelegramClient &_client;
    const Menu *_menus = nullptr;
    size_t _menu_count = 0;
    const char *_root_menu_id = nullptr;

    const Command *_cmds = nullptr;
    size_t _cmd_count = 0;
    TextHandler _text_handler = nullptr;
    void *_text_ctx = nullptr;

    size_t _max_chats = 8;
    std::vector<ChatState> _chat_states;

    bool handleUpdate_(const TelegramClient::Update &u)
    {
        const String text = u.text;
        if (_text_handler && _text_handler(_text_ctx, u))
            return true;
        if (text == "/start")
            return goRoot(u.chat_id);
        if (text == "/back")
            return goParent_(u.chat_id);

        if (handleCommand_(u))
            return true;

        const Menu *menu = currentMenu_(u.chat_id);
        if (menu && handleMenu_(u, *menu))
            return true;

        if (menu)
            return showMenu(u.chat_id, menu, F("Unknown command"));
        return false;
    }

    bool handleCommand_(const TelegramClient::Update &u)
    {
        if (!_cmds || _cmd_count == 0)
            return false;
        for (size_t i = 0; i < _cmd_count; ++i)
        {
            const char *cmd = _cmds[i].cmd;
            if (!cmd || !cmd[0])
                continue;
            if (!commandMatch_(u.text, cmd))
                continue;
            String reply;
            if (_cmds[i].handler && _cmds[i].handler(*this, u, reply))
            {
                if (reply.length())
                    sendText(u.chat_id, reply);
                return true;
            }
        }
        return false;
    }

    bool handleMenu_(const TelegramClient::Update &u, const Menu &menu)
    {
        for (size_t i = 0; i < menu.count; ++i)
        {
            const MenuItem &it = menu.items[i];
            if (!it.label)
                continue;
            if (u.text != it.label)
                continue;
            if (it.command)
            {
                if (strcmp(it.command, "/back") == 0)
                    return goParent_(u.chat_id);
                TelegramClient::Update cmd_u = u;
                cmd_u.text = it.command;
                if (handleCommandText_(cmd_u, it.command))
                    return true;
            }
            if (it.next_menu)
            {
                const Menu *next = findMenu_(it.next_menu);
                if (!next)
                    return false;
                setChatMenu_(u.chat_id, next->id);
                if (it.reply && it.reply[0])
                    return showMenu(u.chat_id, next, it.reply);
                return showMenu(u.chat_id, next);
            }
            if (it.reply && it.reply[0])
                return sendText(u.chat_id, it.reply);
            return true;
        }
        return false;
    }

    bool handleCommandText_(const TelegramClient::Update &u, const char *cmd)
    {
        if (!cmd || !cmd[0])
            return false;
        if (!commandMatch_(u.text, cmd))
            return false;
        String reply;
        for (size_t i = 0; i < _cmd_count; ++i)
        {
            if (_cmds[i].cmd && strcmp(_cmds[i].cmd, cmd) == 0)
            {
                if (_cmds[i].handler && _cmds[i].handler(*this, u, reply))
                {
                    if (reply.length())
                        sendText(u.chat_id, reply);
                    return true;
                }
            }
        }
        return false;
    }

    bool goParent_(int64_t chat_id)
    {
        const Menu *menu = currentMenu_(chat_id);
        if (!menu || !menu->parent_id)
            return goRoot(chat_id);
        const Menu *parent = findMenu_(menu->parent_id);
        if (!parent)
            return false;
        setChatMenu_(chat_id, parent->id);
        return showMenu(chat_id, parent);
    }

    const Menu *findMenu_(const char *id) const
    {
        if (!id || !_menus)
            return nullptr;
        for (size_t i = 0; i < _menu_count; ++i)
        {
            if (_menus[i].id && strcmp(_menus[i].id, id) == 0)
                return &_menus[i];
        }
        return nullptr;
    }

    const Menu *currentMenu_(int64_t chat_id)
    {
        const ChatState *st = findChat_(chat_id);
        if (!st || !st->menu_id)
            return findMenu_(_root_menu_id);
        return findMenu_(st->menu_id);
    }

    const ChatState *findChat_(int64_t chat_id) const
    {
        for (size_t i = 0; i < _chat_states.size(); ++i)
        {
            if (_chat_states[i].chat_id == chat_id)
                return &_chat_states[i];
        }
        return nullptr;
    }

    ChatState *findChat_(int64_t chat_id)
    {
        for (size_t i = 0; i < _chat_states.size(); ++i)
        {
            if (_chat_states[i].chat_id == chat_id)
                return &_chat_states[i];
        }
        return nullptr;
    }

    void setChatMenu_(int64_t chat_id, const char *menu_id)
    {
        ChatState *st = findChat_(chat_id);
        if (!st)
        {
            if (_chat_states.size() >= _max_chats)
                return;
            ChatState ns;
            ns.chat_id = chat_id;
            ns.menu_id = menu_id;
            _chat_states.push_back(ns);
            return;
        }
        st->menu_id = menu_id;
    }

    static bool commandMatch_(const String &text, const char *cmd)
    {
        if (!cmd)
            return false;
        const size_t len = strlen(cmd);
        if (len == 0)
            return false;
        if (!text.startsWith(cmd))
            return false;
        if (text.length() == len)
            return true;
        return text.charAt(len) == ' ';
    }

    static String escapeJson_(const String &in)
    {
        String out;
        out.reserve(in.length() + 8);
        for (size_t i = 0; i < in.length(); ++i)
        {
            const char c = in.charAt(i);
            switch (c)
            {
            case '\\':
                out += F("\\\\");
                break;
            case '"':
                out += F("\\\"");
                break;
            case '\n':
                out += F("\\n");
                break;
            case '\r':
                break;
            case '\t':
                out += F("\\t");
                break;
            default:
                out += c;
                break;
            }
        }
        return out;
    }

    static String buildMessagePayload_(int64_t chat_id, const String &text, const String &reply_markup)
    {
        String payload = F("{\"chat_id\":");
        payload += String((long long)chat_id);
        payload += F(",\"text\":\"");
        payload += escapeJson_(text);
        payload += F("\"");
        if (reply_markup.length())
        {
            payload += F(",\"reply_markup\":");
            payload += reply_markup;
        }
        payload += F("}");
        return payload;
    }

    static String buildMenuMarkup_(const Menu &menu)
    {
        String out = F("{\"keyboard\":[");
        const size_t cols = 2;
        for (size_t i = 0; i < menu.count; ++i)
        {
            if (i % cols == 0)
            {
                if (i > 0)
                    out += F(",");
                out += F("[");
            }
            out += F("\"");
            out += escapeJson_(menu.items[i].label ? String(menu.items[i].label) : String(""));
            out += F("\"");
            if ((i % cols) == cols - 1 || i + 1 == menu.count)
                out += F("]");
            else
                out += F(",");
        }
        out += F("],\"resize_keyboard\":true,\"one_time_keyboard\":false}");
        return out;
    }

    static void onUpdates_(void *ctx, const std::vector<TelegramClient::Update> &updates)
    {
        if (!ctx)
            return;
        TelegramBot *bot = static_cast<TelegramBot *>(ctx);
        bot->processUpdates(updates);
    }
};
