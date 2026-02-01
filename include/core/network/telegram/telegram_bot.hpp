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

    using MenuPrefixProvider = String (*)(void *ctx, int64_t chat_id, const Menu &menu);
    using MenuMarkupProvider = String (*)(void *ctx, int64_t chat_id, const Menu &menu);
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

    void setMenuPrefixProvider(MenuPrefixProvider handler, void *ctx)
    {
        _menu_prefix_handler = handler;
        _menu_prefix_ctx = ctx;
    }

    void setMenuMarkupProvider(MenuMarkupProvider handler, void *ctx)
    {
        _menu_markup_handler = handler;
        _menu_markup_ctx = ctx;
    }

    void setMaxChats(size_t max_chats)
    {
        _max_chats = max_chats;
        _chat_states.reserve(_max_chats);
    }

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
        return enqueueText_(chat_id, text, reply_markup, "");
    }

    bool sendText(int64_t chat_id, const String &text, const String &reply_markup, const String &parse_mode)
    {
        return enqueueText_(chat_id, text, reply_markup, parse_mode);
    }

    void task()
    {
        _client.task();
        processOutbox_();
        ++_send_tick;
    }

    bool showMenu(int64_t chat_id, const Menu *menu, const String &prefix = "")
    {
        if (!menu)
            return false;
        String text = menu->title ? String(menu->title) : String("Menu");
        if (_menu_prefix_handler)
        {
            const String menu_prefix = _menu_prefix_handler(_menu_prefix_ctx, chat_id, *menu);
            if (menu_prefix.length())
                text = menu_prefix + "\n" + text;
        }
        if (prefix.length())
            text = prefix + "\n" + text;
        String markup;
        if (_menu_markup_handler)
            markup = _menu_markup_handler(_menu_markup_ctx, chat_id, *menu);
        if (markup.length() == 0)
            markup = buildMenuMarkup_(*menu);
        return sendText(chat_id, text, markup, "HTML");
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

    bool setMenu(int64_t chat_id, const char *menu_id)
    {
        const Menu *menu = findMenu_(menu_id);
        if (!menu)
            return false;
        setChatMenu_(chat_id, menu->id);
        return true;
    }

    const char *currentMenuId(int64_t chat_id) const
    {
        const ChatState *st = findChat_(chat_id);
        if (!st || !st->menu_id)
            return _root_menu_id;
        return st->menu_id;
    }

private:
    struct ChatState
    {
        int64_t chat_id = 0;
        const char *menu_id = nullptr;
    };

    struct OutMsg
    {
        int64_t chat_id = 0;
        String text;
        String reply_markup;
        String parse_mode;
        uint32_t ready_tick = 0;
    };

    TelegramClient &_client;
    const Menu *_menus = nullptr;
    size_t _menu_count = 0;
    const char *_root_menu_id = nullptr;

    const Command *_cmds = nullptr;
    size_t _cmd_count = 0;
    TextHandler _text_handler = nullptr;
    void *_text_ctx = nullptr;
    MenuPrefixProvider _menu_prefix_handler = nullptr;
    void *_menu_prefix_ctx = nullptr;
    MenuMarkupProvider _menu_markup_handler = nullptr;
    void *_menu_markup_ctx = nullptr;

    size_t _max_chats = 8;
    std::vector<ChatState> _chat_states;
    std::vector<OutMsg> _out_queue;
    size_t _out_head = 0;
    uint32_t _send_tick = 0;

    static constexpr size_t kMaxOutQueue = 32;

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

    static String buildMessagePayload_(int64_t chat_id, const String &text, const String &reply_markup,
                                       const String &parse_mode = "")
    {
        const String clean_text = sanitizeUtf8_(text);
        const String clean_markup = sanitizeUtf8_(reply_markup);
        const String clean_mode = sanitizeUtf8_(parse_mode);
        String payload = F("{\"chat_id\":");
        payload.reserve(clean_text.length() + clean_markup.length() + clean_mode.length() + 64);
        payload += String((long long)chat_id);
        payload += F(",\"text\":\"");
        payload += escapeJson_(clean_text);
        payload += F("\"");
        if (clean_markup.length())
        {
            payload += F(",\"reply_markup\":");
            payload += clean_markup;
        }
        if (clean_mode.length())
        {
            payload += F(",\"parse_mode\":\"");
            payload += escapeJson_(clean_mode);
            payload += F("\"");
        }
        payload += F("}");
        return payload;
    }

    static String sanitizeUtf8_(const String &in)
    {
        if (isValidUtf8_(in))
            return in;
        return cp1251ToUtf8_(in);
    }

    static bool isValidUtf8_(const String &in)
    {
        size_t i = 0;
        while (i < (size_t)in.length())
        {
            const uint8_t c = (uint8_t)in[i];
            if (c < 0x80)
            {
                ++i;
                continue;
            }
            size_t need = 0;
            if ((c & 0xE0) == 0xC0)
            {
                if (c < 0xC2)
                    return false;
                need = 1;
            }
            else if ((c & 0xF0) == 0xE0)
            {
                need = 2;
            }
            else if ((c & 0xF8) == 0xF0)
            {
                if (c > 0xF4)
                    return false;
                need = 3;
            }
            else
            {
                return false;
            }

            if (i + need >= (size_t)in.length())
                return false;

            for (size_t j = 1; j <= need; ++j)
            {
                const uint8_t cc = (uint8_t)in[i + j];
                if ((cc & 0xC0) != 0x80)
                    return false;
            }
            i += need + 1;
        }
        return true;
    }

    static void appendUtf8_(String &out, uint16_t code)
    {
        if (code < 0x80)
        {
            out += (char)code;
            return;
        }
        if (code < 0x800)
        {
            out += (char)(0xC0 | (code >> 6));
            out += (char)(0x80 | (code & 0x3F));
            return;
        }
        out += (char)(0xE0 | (code >> 12));
        out += (char)(0x80 | ((code >> 6) & 0x3F));
        out += (char)(0x80 | (code & 0x3F));
    }

    static String cp1251ToUtf8_(const String &in)
    {
        String out;
        out.reserve(in.length() * 2);
        for (size_t i = 0; i < (size_t)in.length(); ++i)
        {
            const uint8_t c = (uint8_t)in[i];
            if (c < 0x80)
            {
                out += (char)c;
                continue;
            }
            uint16_t code = '?';
            if (c == 0xA8)
                code = 0x0401;
            else if (c == 0xB8)
                code = 0x0451;
            else if (c >= 0xC0 && c <= 0xFF)
                code = (uint16_t)(0x0410 + (c - 0xC0));
            else
                code = '?';
            appendUtf8_(out, code);
        }
        return out;
    }

    static String buildMenuMarkup_(const Menu &menu)
    {
        String out = F("{\"keyboard\":[");
        out.reserve(menu.count * 32 + 64);
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

    bool enqueueText_(int64_t chat_id, const String &text, const String &reply_markup, const String &parse_mode)
    {
        const size_t pending = (_out_queue.size() >= _out_head) ? (_out_queue.size() - _out_head) : 0;
        if (pending >= kMaxOutQueue)
            return false;
        OutMsg msg;
        msg.chat_id = chat_id;
        msg.text = text;
        msg.reply_markup = reply_markup;
        msg.parse_mode = parse_mode;
        msg.ready_tick = _send_tick + 1;
        _out_queue.push_back(msg);
        return true;
    }

    void processOutbox_()
    {
        if (_out_head >= _out_queue.size())
            return;
        OutMsg &msg = _out_queue[_out_head];
        if (msg.ready_tick > _send_tick)
            return;
        String payload = buildMessagePayload_(msg.chat_id, msg.text, msg.reply_markup, msg.parse_mode);
        _client.sendMessageRaw(payload);
        ++_out_head;
        if (_out_head >= _out_queue.size())
        {
            _out_queue.clear();
            _out_head = 0;
        }
        else if (_out_head >= 16 && _out_head * 2 >= _out_queue.size())
        {
            _out_queue.erase(_out_queue.begin(), _out_queue.begin() + (int)_out_head);
            _out_head = 0;
        }
    }
};
