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
#include <ArduinoJson.h>
#include <string.h>
#include <vector>

#include "core/network/telegram/telegram_bot.hpp"
#include "core/rtc.hpp"
#include "core/network/wifi_manager.hpp"
#include "plc/plc_control.hpp"
#include "utils/configs.hpp"
#include "utils/configs_manager_iface.hpp"
#include "utils/logger.hpp"
#include "core/network/stack/stack_master.hpp"

class TelegramMenu
{
public:
    TelegramMenu(PlcControl &plc, WifiManager &wifi, RTC &rtc, TelegramBot &bot, Configs &configs, Logger &logs)
        : _plc(plc), _wifi(wifi), _rtc(rtc), _bot(&bot), _configs(configs), _logs(&logs)
    {
    }

    void begin()
    {
        _self = this;
        if (!_bot)
            return;
        _bot->setMenus(kMenus, kMenuCount, "root");
        _bot->setCommands(kCommands, kCommandCount);
        _bot->setTextHandler(&TelegramMenu::onText_, this);
        _bot->setMenuPrefixProvider(&TelegramMenu::menuPrefix_, this);
        _bot->setMenuMarkupProvider(&TelegramMenu::menuMarkup_, this);
    }

    void setAdminPassword(const String &password) { _admin_password = password; }
    const String &adminPassword() const { return _admin_password; }
    void setConfigsManager(ConfigsManagerIface &mgr) { _configs_manager = &mgr; }
    void setStackMaster(StackMaster &master) { _stack_master = &master; }

    enum class AllowResult : uint8_t
    {
        Ok = 0,
        Invalid,
        Exists,
        Full
    };

    void setAllowedUsers(const std::vector<String> &users)
    {
        _allowed_users.clear();
        for (size_t i = 0; i < users.size(); ++i)
        {
            if (_allowed_users.size() >= kMaxAllowedUsers)
                break;
            String u = normalizeUser_(users[i]);
            if (u.length() == 0 || hasAllowedUser_(u))
                continue;
            _allowed_users.push_back(u);
        }
    }

    AllowResult addAllowedUser(const String &user)
    {
        String u = normalizeUser_(user);
        if (u.length() == 0)
            return AllowResult::Invalid;
        if (hasAllowedUser_(u))
            return AllowResult::Exists;
        if (_allowed_users.size() >= kMaxAllowedUsers)
            return AllowResult::Full;
        _allowed_users.push_back(u);
        return AllowResult::Ok;
    }

    bool removeAllowedUser(const String &user)
    {
        String u = normalizeUser_(user);
        if (u.length() == 0)
            return false;
        for (size_t i = 0; i < _allowed_users.size(); ++i)
        {
            if (_allowed_users[i] == u)
            {
                _allowed_users.erase(_allowed_users.begin() + (int)i);
                return true;
            }
        }
        return false;
    }

    void clearAllowedUsers() { _allowed_users.clear(); }

    const std::vector<String> &allowedUsers() const { return _allowed_users; }

private:
    static inline TelegramMenu *_self = nullptr;

    PlcControl &_plc;
    WifiManager &_wifi;
    RTC &_rtc;
    String _admin_password;
    Configs &_configs;
    ConfigsManagerIface *_configs_manager = nullptr;
    std::vector<String> _allowed_users;

    struct ChatAuth
    {
        int64_t chat_id = 0;
        bool authorized = false;
        bool awaiting = false;
        bool awaiting_config = false;
        bool awaiting_device = false;
        uint8_t fail_count = 0;
        uint32_t lock_until_ms = 0;
        bool selected_local = true;
        uint32_t selected_node_id = 0;
    };

    std::vector<ChatAuth> _auth;
    static constexpr size_t kMaxConfigBytes = 8192;
    static constexpr size_t kMaxAllowedUsers = 10;

    static bool requireAdmin_(TelegramMenu &self, TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        (void)bot;
        if (self._admin_password.length() == 0)
            return true;
        ChatAuth *st = self.ensureAuth_(u.chat_id);
        if (!st || !st->authorized)
        {
            reply = "Нужна авторизация. Используйте /admin.";
            return false;
        }
        return true;
    }

    static bool cmdAdmin_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        (void)reply;
        if (!_self)
            return false;
        ChatAuth *st = _self->ensureAuth_(u.chat_id);
        if (!st)
            return false;
        if (_self->isLocked_(*st))
        {
            const uint32_t left_s = (_self->msLeft_(st->lock_until_ms) + 999) / 1000;
            String msg = "Блокировка ";
            msg += String((unsigned)left_s);
            msg += " сек";
            bot.sendText(u.chat_id, msg);
            return true;
        }
        if (_self->_admin_password.length() == 0)
        {
            bot.enterMenu(u.chat_id, "admin", _self->adminPrefix_(u.chat_id));
            return true;
        }

        if (st && st->authorized)
        {
            bot.enterMenu(u.chat_id, "admin", _self->adminPrefix_(u.chat_id));
            return true;
        }

        st->awaiting = true;
        st->authorized = false;
        bot.sendText(u.chat_id, F("Введите пароль администратора:"));
        return true;
    }

    static bool cmdDevice_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        if (!_self)
            return false;
        (void)reply;
        bot.goRoot(u.chat_id);
        return true;
    }

    static bool cmdStatus_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        (void)bot;
        if (!_self)
            return false;
        const float temp = _self->_plc.boardTemp();
        const bool fan = _self->_plc.fanStatus();
        char buf[96] = {};
        snprintf(buf, sizeof(buf), "Статус ПЛК:\n  temp_c: %.1f\n  fan: %s",
                 temp, fan ? "вкл" : "выкл");
        reply = buf;
        return true;
    }

    static bool cmdWifi_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        (void)bot;
        if (!_self)
            return false;
        reply = "Wi-Fi:\n  режим: ";
        reply += _self->_wifi.ap() ? "AP" : "STA";
        reply += "\n  ssid  : ";
        reply += _self->_wifi.ssid();
        reply += "\n  ap_ssid: ";
        reply += _self->_wifi.apSsid();
        return true;
    }

    static bool cmdStackList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        if (!_self)
            return false;
        if (!requireAdmin_(*_self, bot, u, reply))
            return true;
        if (!_self->_stack_master)
        {
            reply = "Стек недоступен";
            return true;
        }
        const size_t count = _self->_stack_master->nodeCount();
        if (count == 0)
        {
            reply = "Контроллеры: нет активных";
            return true;
        }
        reply = "Контроллеры:";
        for (size_t i = 0; i < count; ++i)
        {
            const uint32_t node_id = _self->_stack_master->nodeIdAt(i);
            String name = _self->_stack_master->nodeNameAt(i);
            if (name.length() == 0)
                name = fallbackNodeName_(node_id);
            const String ip = _self->_stack_master->nodeIpAt(i);
            reply += "\n  ";
            reply += name;
            reply += " (id=";
            reply += String(node_id);
            if (ip.length())
            {
                reply += ", ip=";
                reply += ip;
            }
            reply += ")";
        }
        return true;
    }

    static bool cmdWifiRestart_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        (void)bot;
        if (!_self)
            return false;
        if (_self->_wifi.begin())
            reply = "Wi-Fi перезапущен";
        else
            reply = "Не удалось перезапустить Wi-Fi";
        return true;
    }
    static bool cmdPlcRestart_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        (void)reply;
        if (!_self)
            return false;
        bot.sendText(u.chat_id, F("Перезапуск ПЛК"));
        delay(200);
        ESP.restart();
        return true;
    }

    static bool cmdWifiApOn_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        (void)bot;
        if (!_self)
            return false;
        _self->_wifi.setAp(true);
        if (_self->_wifi.begin())
            reply = "Wi-Fi AP включен";
        else
            reply = "Не удалось включить Wi-Fi AP";
        return true;
    }

    static bool cmdWifiApOff_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        (void)bot;
        if (!_self)
            return false;
        _self->_wifi.setAp(false);
        if (_self->_wifi.begin())
            reply = "Wi-Fi AP выключен";
        else
            reply = "Не удалось выключить Wi-Fi AP";
        return true;
    }

    static bool cmdLogs_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        if (!_self)
            return false;
        if (!requireAdmin_(*_self, bot, u, reply))
            return true;
        if (!_self->_logs)
        {
            reply = "Logger unavailable";
            return true;
        }
        const size_t count = _self->_logs->recentCount();
        if (count == 0)
        {
            reply = "Logs: empty";
            return true;
        }
        reply = "Logs:\n";
        char buf[LOGGER_BUFFER_SIZE] = {};
        static constexpr size_t kMaxReply = 3900;
        for (size_t i = 0; i < count; ++i)
        {
            if (!_self->_logs->getRecentLine(i, buf, sizeof(buf)))
                continue;
            const size_t need = strlen(buf) + 1;
            if (reply.length() + need > kMaxReply)
            {
                reply += "...";
                break;
            }
            reply += buf;
            reply += '\n';
        }
        return true;
    }

    static bool cmdConfigSet_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        (void)reply;
        if (!_self)
            return false;
        ChatAuth *st = _self->ensureAuth_(u.chat_id);
        if (!st)
            return false;
        if (_self->_admin_password.length() > 0 && !st->authorized)
        {
            bot.sendText(u.chat_id, F("Нужна авторизация. Используйте /admin."));
            return true;
        }
        st->awaiting_config = true;
        st->awaiting = false;
        bot.sendText(u.chat_id, F("Отправьте JSON для сохранения startup-config. /back - отмена."));
        return true;
    }

    static bool cmdAllowList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        if (!_self)
            return false;
        if (!requireAdmin_(*_self, bot, u, reply))
            return true;
        if (_self->_allowed_users.empty())
        {
            reply = "Список разрешенных username пуст.";
            return true;
        }
        reply = "Разрешенные username:";
        for (const auto &name : _self->_allowed_users)
            reply += "\n  " + name;
        return true;
    }

    static bool cmdAllowAdd_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        if (!_self)
            return false;
        if (!requireAdmin_(*_self, bot, u, reply))
            return true;
        const char *cmd = "/allow_add";
        String name = u.text.substring(strlen(cmd));
        name.trim();
        if (name.length() == 0)
        {
            reply = "Использование: /allow_add <username>";
            return true;
        }
        AllowResult res = _self->addAllowedUser(name);
        if (res == AllowResult::Ok)
            reply = "OK";
        else if (res == AllowResult::Exists)
            reply = "Уже в списке";
        else if (res == AllowResult::Full)
            reply = "Лимит 10";
        else
            reply = "Некорректный username";
        return true;
    }

    static bool cmdAllowDel_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        if (!_self)
            return false;
        if (!requireAdmin_(*_self, bot, u, reply))
            return true;
        const char *cmd = "/allow_del";
        String name = u.text.substring(strlen(cmd));
        name.trim();
        if (name.length() == 0)
        {
            reply = "Использование: /allow_del <username>";
            return true;
        }
        if (_self->removeAllowedUser(name))
            reply = "OK";
        else
            reply = "Не найден";
        return true;
    }

    static bool cmdAllowClear_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        if (!_self)
            return false;
        if (!requireAdmin_(*_self, bot, u, reply))
            return true;
        _self->clearAllowedUsers();
        reply = "OK";
        return true;
    }

    static bool cmdTime_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        (void)bot;
        if (!_self)
            return false;
        Ds3231Mz::DateTime dt;
        if (!_self->_rtc.Time(dt))
        {
            reply = "RTC: нет данных";
            return true;
        }
        char date_buf[16] = {};
        char time_buf[16] = {};
        snprintf(date_buf, sizeof(date_buf), "%04u-%02u-%02u",
                 (unsigned)dt.year, (unsigned)dt.month, (unsigned)dt.day);
        snprintf(time_buf, sizeof(time_buf), "%02u:%02u:%02u",
                 (unsigned)dt.hour, (unsigned)dt.minute, (unsigned)dt.second);
        reply = "RTC:\n  дата: ";
        reply += date_buf;
        reply += "\n  время: ";
        reply += time_buf;
        reply += "\n  день недели: ";
        reply += String((unsigned)dt.day_of_week);
        return true;
    }

    static bool handleDocument_(TelegramMenu &self, const TelegramClient::Update &u)
    {
        if (!u.hasDocument())
            return false;
        const bool is_config = (u.document_file_name == "startup-config.json");
        if (!is_config)
            return false;

        ChatAuth *st = self.ensureAuth_(u.chat_id);
        if (!st)
            return false;
        if (self._admin_password.length() > 0 && !st->authorized)
        {
            if (self._bot)
                self._bot->sendText(u.chat_id, F("Нужна авторизация. Используйте /admin."));
            return true;
        }
        st->awaiting_config = false;
        st->awaiting = false;
        if (is_config && u.document_size > 0 && u.document_size > kMaxConfigBytes)
        {
            if (self._bot)
                self._bot->sendText(u.chat_id, F("Слишком большой файл."));
            return true;
        }
        if (!self._bot)
            return true;

        TelegramClient &client = self._bot->client();
        FastBot2Client *fb = client.fastBot();
        if (!fb)
        {
            self._bot->sendText(u.chat_id, F("Telegram client not ready"));
            return true;
        }
        fb::Fetcher fetch = fb->downloadFile(u.document_file_id);
        if (!fetch)
        {
            String err = client.lastError();
            if (err.length() == 0)
                err = F("downloadFile failed");
            self._bot->sendText(u.chat_id, err);
            return true;
        }

        String json;
        if (u.document_size > 0)
            json.reserve((size_t)u.document_size + 16);
        struct StringWriter : public Print
        {
            explicit StringWriter(String &out) : _out(out) {}
            size_t write(uint8_t b) override
            {
                _out += static_cast<char>(b);
                return 1;
            }
            size_t write(const uint8_t *data, size_t len) override
            {
                if (!data || len == 0)
                    return 0;
                if (!_out.concat(reinterpret_cast<const char *>(data), len))
                    return 0;
                return len;
            }
            String &_out;
        };
        StringWriter writer(json);
        if (!fetch.writeTo(writer))
        {
            String err = client.lastError();
            if (err.length() == 0)
                err = F("download failed");
            self._bot->sendText(u.chat_id, err);
            return true;
        }
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, json);
        if (err)
        {
            self._bot->sendText(u.chat_id, F("Ошибка разбора JSON."));
            return true;
        }
        const bool saved = self._configs_manager ? self._configs_manager->save(doc) : self._configs.save(doc);
        if (!saved)
        {
            self._bot->sendText(u.chat_id, F("Не удалось сохранить конфиг."));
            return true;
        }
        self._bot->sendText(u.chat_id, F("Startup-config сохранен. Перезагрузите контроллер для применения."));
        return true;
    }

    static bool onText_(void *ctx, const TelegramClient::Update &u)
    {
        TelegramMenu *self = static_cast<TelegramMenu *>(ctx);
        if (!self)
            return false;
        if (!self->isAllowedUser_(u))
        {
            if (self->_bot)
                self->_bot->sendText(u.chat_id, F("Доступ запрещен"));
            return true;
        }
        if (handleDocument_(*self, u))
            return true;
        if (u.text == "/start")
        {
            self->resetAuth_(u.chat_id);
            return false;
        }
        if (u.text == "/back")
        {
            self->resetAwaiting_(u.chat_id);
            return false;
        }
        ChatAuth *st = self->findAuth_(u.chat_id);
        if (self->handleRootDeviceSelection_(u))
            return true;
        if (st && st->awaiting_device)
        {
            if (u.text == F("Назад"))
            {
                st->awaiting_device = false;
                if (self->_bot)
                    self->_bot->enterMenu(u.chat_id, "admin", self->adminPrefix_(u.chat_id));
                return true;
            }
            if (self->selectDevice_(u.chat_id, u.text))
            {
                st->awaiting_device = false;
                if (self->_bot)
                    self->_bot->enterMenu(u.chat_id, "admin", self->adminPrefix_(u.chat_id));
            }
            else
            {
                self->sendDeviceMenu_(u.chat_id, "Неизвестное устройство. Выберите из списка:");
            }
            return true;
        }
        if (st && st->awaiting_config)
        {
            if (self->_admin_password.length() > 0 && !st->authorized)
            {
                st->awaiting_config = false;
                if (self->_bot)
                    self->_bot->sendText(u.chat_id, F("Нужна авторизация. Используйте /admin."));
                return true;
            }
            st->awaiting_config = false;
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, u.text);
            if (err)
            {
                if (self->_bot)
                    self->_bot->sendText(u.chat_id, F("Ошибка разбора JSON."));
                return true;
            }
            const bool saved = self->_configs_manager ? self->_configs_manager->save(doc) : self->_configs.save(doc);
            if (!saved)
            {
                if (self->_bot)
                    self->_bot->sendText(u.chat_id, F("Не удалось сохранить конфиг."));
                return true;
            }
            if (self->_bot)
                self->_bot->sendText(u.chat_id, F("Startup-config сохранен. Перезагрузите контроллер для применения."));
            return true;
        }
        if (!st || !st->awaiting)
            return false;
        if (self->isLocked_(*st))
        {
            const uint32_t left_s = (self->msLeft_(st->lock_until_ms) + 999) / 1000;
            String msg = "Блокировка ";
            msg += String((unsigned)left_s);
            msg += " сек";
            self->_bot->sendText(u.chat_id, msg);
            return true;
        }
        const bool ok = (u.text == self->_admin_password);
        if (ok)
        {
            st->authorized = true;
            st->awaiting = false;
            st->fail_count = 0;
            st->lock_until_ms = 0;
            if (self->_bot)
                self->_bot->enterMenu(u.chat_id, "admin", F("Доступ разрешен"));
        }
        else
        {
            st->authorized = false;
            st->awaiting = true;
            self->onFail_(*st);
            if (self->_bot && !self->isLocked_(*st))
                self->_bot->sendText(u.chat_id, F("Неверный пароль\nВведите пароль администратора:"));
        }
        return true;
    }

    const ChatAuth *findAuth_(int64_t chat_id) const
    {
        for (size_t i = 0; i < _auth.size(); ++i)
        {
            if (_auth[i].chat_id == chat_id)
                return &_auth[i];
        }
        return nullptr;
    }

    ChatAuth *findAuth_(int64_t chat_id)
    {
        for (size_t i = 0; i < _auth.size(); ++i)
        {
            if (_auth[i].chat_id == chat_id)
                return &_auth[i];
        }
        return nullptr;
    }

    ChatAuth *ensureAuth_(int64_t chat_id)
    {
        ChatAuth *st = findAuth_(chat_id);
        if (st)
            return st;
        ChatAuth ns;
        ns.chat_id = chat_id;
        _auth.push_back(ns);
        return &_auth.back();
    }

    void resetAuth_(int64_t chat_id)
    {
        ChatAuth *st = findAuth_(chat_id);
        if (!st)
            return;
        st->authorized = false;
        st->awaiting = false;
        st->fail_count = 0;
        st->lock_until_ms = 0;
        st->awaiting_device = false;
        st->selected_local = true;
        st->selected_node_id = 0;
    }

    void resetAwaiting_(int64_t chat_id)
    {
        ChatAuth *st = findAuth_(chat_id);
        if (!st)
            return;
        st->awaiting = false;
        st->awaiting_config = false;
        st->awaiting_device = false;
    }

    TelegramBot *_bot = nullptr;
    Logger *_logs = nullptr;
    StackMaster *_stack_master = nullptr;

        static inline const TelegramBot::MenuItem kRootItems[] = {};

        static inline const TelegramBot::MenuItem kDeviceItems[] = {
        { "Админка", "Админка", nullptr, nullptr },
        { "Назад", "/back", nullptr, nullptr },
    };

        static inline const TelegramBot::MenuItem kAdminItems[] = {
        { "ПЛК", nullptr, "plc", nullptr },
        { "Часы", nullptr, "rtc", nullptr },
        { "Wi-Fi", nullptr, "wifi", nullptr },
        { "Настройки", nullptr, "settings", nullptr },
        { "Logs", "/logs", nullptr, nullptr },
        { "Назад", "/back", nullptr, nullptr },
    };

        static inline const TelegramBot::MenuItem kPlcItems[] = {
        { "Статус", "/status", nullptr, nullptr },
        { "Назад", "/back", nullptr, nullptr },
    };

        static inline const TelegramBot::MenuItem kRtcItems[] = {
        { "Время", "/time", nullptr, nullptr },
        { "Назад", "/back", nullptr, nullptr },
    };

        static inline const TelegramBot::MenuItem kWifiItems[] = {
        { "Состояние", "/wifi", nullptr, nullptr },
        { "Назад", "/back", nullptr, nullptr },
    };

        static inline const TelegramBot::MenuItem kSettingsItems[] = {
        { "Перезапуск Wi-Fi", "/wifi_restart", nullptr, nullptr },
        { "Перезапуск ПЛК", "/plc_restart", nullptr, nullptr },
        { "Wi-Fi AP Вкл", "/wifi_ap_on", nullptr, nullptr },
        { "Wi-Fi AP Выкл", "/wifi_ap_off", nullptr, nullptr },
        { "Startup-config", "/config_set", nullptr, nullptr },
        { "Назад", "/back", nullptr, nullptr },
    };

        static inline const TelegramBot::Menu kMenus[] = {
        { "root", "Выбор устройства", kRootItems, 0, nullptr },
        { "device", "Меню устройства", kDeviceItems, 3, "root" },
        { "admin", "Админка", kAdminItems, 6, "device" },
        { "plc", "ПЛК", kPlcItems, 2, "admin" },
        { "rtc", "Часы", kRtcItems, 2, "admin" },
        { "wifi", "Wi-Fi", kWifiItems, 2, "admin" },
        { "settings", "Настройки", kSettingsItems, 6, "admin" },
    };

    static inline const size_t kMenuCount = sizeof(kMenus) / sizeof(kMenus[0]);

    static inline const TelegramBot::Command kCommands[] = {
        { "Админка", &TelegramMenu::cmdAdmin_ },
        { "/status", &TelegramMenu::cmdStatus_ },
        { "/wifi", &TelegramMenu::cmdWifi_ },
        { "/time", &TelegramMenu::cmdTime_ },
        { "/wifi_restart", &TelegramMenu::cmdWifiRestart_ },
        { "/plc_restart", &TelegramMenu::cmdPlcRestart_ },
        { "/wifi_ap_on", &TelegramMenu::cmdWifiApOn_ },
        { "/wifi_ap_off", &TelegramMenu::cmdWifiApOff_ },
        { "/config_set", &TelegramMenu::cmdConfigSet_ },
        { "/allow_list", &TelegramMenu::cmdAllowList_ },
        { "/allow_add", &TelegramMenu::cmdAllowAdd_ },
        { "/allow_del", &TelegramMenu::cmdAllowDel_ },
        { "/allow_clear", &TelegramMenu::cmdAllowClear_ },
        { "/logs", &TelegramMenu::cmdLogs_ },
        { "/device", &TelegramMenu::cmdDevice_ },
        { "/stack_list", &TelegramMenu::cmdStackList_ },
    };

    static inline const size_t kCommandCount = sizeof(kCommands) / sizeof(kCommands[0]);

    static constexpr uint8_t kMaxFails = 3;
    static constexpr uint32_t kLockMs = 30000;

    bool isLocked_(const ChatAuth &st) const
    {
        return st.lock_until_ms != 0 && msLeft_(st.lock_until_ms) > 0;
    }

    static uint32_t msLeft_(uint32_t deadline_ms)
    {
        const int32_t diff = (int32_t)(deadline_ms - millis());
        return diff > 0 ? (uint32_t)diff : 0;
    }

    void onFail_(ChatAuth &st)
    {
        if (st.fail_count < 0xFF)
            st.fail_count++;
        if (st.fail_count >= kMaxFails)
        {
            st.lock_until_ms = millis() + kLockMs;
            st.fail_count = 0;
            st.awaiting = false;
            if (_bot)
                _bot->sendText(st.chat_id, F("Слишком много неверных попыток. Блокировка 30 сек"));
        }
    }
    static String normalizeUser_(String user)
    {
        user.trim();
        if (user.startsWith("@"))
            user.remove(0, 1);
        user.toLowerCase();
        return user;
    }

    bool hasAllowedUser_(const String &user) const
    {
        for (size_t i = 0; i < _allowed_users.size(); ++i)
        {
            if (_allowed_users[i] == user)
                return true;
        }
        return false;
    }

    bool isAllowedUser_(const TelegramClient::Update &u) const
    {
        if (_allowed_users.empty())
            return true;
        String user = normalizeUser_(u.from);
        if (user.length() == 0)
            return false;
        return hasAllowedUser_(user);
    }

    struct DeviceEntry
    {
        String label;
        bool local = true;
        uint32_t node_id = 0;
    };

    String adminPrefix_(int64_t chat_id) const
    {
        String prefix = F("Устройство: ");
        prefix += selectedDeviceLabel_(chat_id);
        return prefix;
    }

    String selectedDeviceLabel_(int64_t chat_id) const
    {
        const ChatAuth *st = findAuth_(chat_id);
        if (!st || st->selected_local || !_stack_master)
            return safePlcName_();
        String name = nodeNameById_(st->selected_node_id);
        if (name.length())
            return name;
        return fallbackNodeName_(st->selected_node_id);
    }

    String safePlcName_() const
    {
        const String name = _plc.deviceName();
        if (name.length())
            return name;
        return F("PLC");
    }

    static String fallbackNodeName_(uint32_t node_id)
    {
        String name = F("Node ");
        name += String(node_id);
        return name;
    }

    String nodeNameById_(uint32_t node_id) const
    {
        if (!_stack_master || node_id == 0)
            return "";
        const size_t count = _stack_master->nodeCount();
        for (size_t i = 0; i < count; ++i)
        {
            if (_stack_master->nodeIdAt(i) != node_id)
                continue;
            String name = _stack_master->nodeNameAt(i);
            if (name.length())
                return name;
            return fallbackNodeName_(node_id);
        }
        return "";
    }

    size_t buildDeviceList_(std::vector<DeviceEntry> &out) const
    {
        out.clear();
        DeviceEntry local{};
        local.label = safePlcName_();
        local.local = true;
        local.node_id = 0;
        out.push_back(local);
        if (!_stack_master)
            return out.size();
        const size_t count = _stack_master->nodeCount();
        for (size_t i = 0; i < count; ++i)
        {
            DeviceEntry entry{};
            entry.local = false;
            entry.node_id = _stack_master->nodeIdAt(i);
            entry.label = _stack_master->nodeNameAt(i);
            if (entry.label.length() == 0)
                entry.label = fallbackNodeName_(entry.node_id);
            out.push_back(entry);
        }
        return out.size();
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

    static String buildKeyboardMarkup_(const std::vector<String> &labels)
    {
        String out = F("{\"keyboard\":[");
        const size_t cols = 2;
        for (size_t i = 0; i < labels.size(); ++i)
        {
            if (i % cols == 0)
            {
                if (i > 0)
                    out += F(",");
                out += F("[");
            }
            out += F("\"");
            out += escapeJson_(labels[i]);
            out += F("\"");
            if ((i % cols) == cols - 1 || i + 1 == labels.size())
                out += F("]");
            else
                out += F(",");
        }
        out += F("],\"resize_keyboard\":true,\"one_time_keyboard\":false}");
        return out;
    }

    static String menuPrefix_(void *ctx, int64_t chat_id, const TelegramBot::Menu &menu)
    {
        TelegramMenu *self = static_cast<TelegramMenu *>(ctx);
        if (!self || !menu.id)
            return "";
        String prefix;
        if (strcmp(menu.id, "root") == 0)
            prefix = F("Текущее устройство: ");
        else
            prefix = F("Устройство: ");
        prefix += self->selectedDeviceLabel_(chat_id);
        return prefix;
    }

    static String menuMarkup_(void *ctx, int64_t chat_id, const TelegramBot::Menu &menu)
    {
        TelegramMenu *self = static_cast<TelegramMenu *>(ctx);
        if (!self || !menu.id)
            return "";
        if (strcmp(menu.id, "root") != 0)
            return "";
        std::vector<DeviceEntry> devices;
        self->buildDeviceList_(devices);
        std::vector<String> labels;
        labels.reserve(devices.size());
        for (const auto &d : devices)
            labels.push_back(d.label);
        return buildKeyboardMarkup_(labels);
    }

    void sendDeviceMenu_(int64_t chat_id, const String &prefix)
    {
        if (!_bot)
            return;
        std::vector<DeviceEntry> devices;
        buildDeviceList_(devices);
        std::vector<String> labels;
        labels.reserve(devices.size() + 1);
        for (const auto &d : devices)
            labels.push_back(d.label);
        labels.push_back(F("Назад"));
        String text = prefix;
        text += "\nТекущее: ";
        text += selectedDeviceLabel_(chat_id);
        const String markup = buildKeyboardMarkup_(labels);
        _bot->sendText(chat_id, text, markup);
    }

    bool handleRootDeviceSelection_(const TelegramClient::Update &u)
    {
        if (!_bot)
            return false;
        const char *menu_id = _bot->currentMenuId(u.chat_id);
        if (!menu_id || strcmp(menu_id, "root") != 0)
            return false;
        if (u.text.length() == 0 || u.text.startsWith("/"))
            return false;
        if (!selectDevice_(u.chat_id, u.text))
        {
            _bot->goRoot(u.chat_id);
            return true;
        }
        _bot->enterMenu(u.chat_id, "device");
        return true;
    }

    bool selectDevice_(int64_t chat_id, const String &label)
    {
        ChatAuth *st = ensureAuth_(chat_id);
        if (!st)
            return false;
        std::vector<DeviceEntry> devices;
        buildDeviceList_(devices);
        for (const auto &d : devices)
        {
            if (d.label != label)
                continue;
            st->selected_local = d.local;
            st->selected_node_id = d.local ? 0 : d.node_id;
            return true;
        }
        return false;
    }
};


