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
    using BackgroundHandler = void (*)(void *ctx);

    struct Command
    {
        const char *cmd;
        CommandHandler handler;
    };

    explicit TelegramBot(TelegramClient &client);

    TelegramClient &client();
    const TelegramClient &client() const;

    void setMenus(const Menu *menus, size_t count, const char *root_id);

    void setCommands(const Command *cmds, size_t count);

    void setTextHandler(TextHandler handler, void *ctx);
    void setBackgroundHandler(BackgroundHandler handler, void *ctx);

    void setMenuPrefixProvider(MenuPrefixProvider handler, void *ctx);

    void setMenuMarkupProvider(MenuMarkupProvider handler, void *ctx);

    void setMaxChats(size_t max_chats);

    bool processUpdates(const std::vector<TelegramClient::Update> &updates);

    void bind(TelegramClient &client);

    bool sendText(int64_t chat_id, const String &text, const String &reply_markup = "");

    bool sendText(int64_t chat_id, const String &text, const String &reply_markup, const String &parse_mode);

    void task();

    bool showMenu(int64_t chat_id, const Menu *menu, const String &prefix = "");

    bool goRoot(int64_t chat_id);

    bool enterMenu(int64_t chat_id, const char *menu_id, const String &prefix = "");

    bool setMenu(int64_t chat_id, const char *menu_id);

    const char *currentMenuId(int64_t chat_id) const;

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
    BackgroundHandler _bg_handler = nullptr;
    void *_bg_ctx = nullptr;
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
    static constexpr uint32_t kTraceSlowMs = 1000;

    bool handleUpdate_(const TelegramClient::Update &u);

    bool handleCommand_(const TelegramClient::Update &u);

    bool handleMenu_(const TelegramClient::Update &u, const Menu &menu);

    bool handleCommandText_(const TelegramClient::Update &u, const char *cmd);

    bool goParent_(int64_t chat_id);

    const Menu *findMenu_(const char *id) const;

    const Menu *currentMenu_(int64_t chat_id);

    const ChatState *findChat_(int64_t chat_id) const;

    ChatState *findChat_(int64_t chat_id);

    void setChatMenu_(int64_t chat_id, const char *menu_id);

    static bool commandMatch_(const String &text, const char *cmd);

    static String escapeJson_(const String &in);

    static String buildMessagePayload_(int64_t chat_id, const String &text, const String &reply_markup,
                                       const String &parse_mode = "");

    static String sanitizeUtf8_(const String &in);

    static bool isValidUtf8_(const String &in);

    static void appendUtf8_(String &out, uint16_t code);

    static String cp1251ToUtf8_(const String &in);

    static String buildMenuMarkup_(const Menu &menu);

    static void onUpdates_(void *ctx, const std::vector<TelegramClient::Update> &updates);

    bool enqueueText_(int64_t chat_id, const String &text, const String &reply_markup, const String &parse_mode);

    void processOutbox_();
};
