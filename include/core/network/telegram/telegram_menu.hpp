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
#include "controllers/socket_controller.hpp"
#include "controllers/meteo_controller.hpp"
#include "controllers/thermo_controller.hpp"
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
    void setSockets(SocketController &sockets) { _sockets = &sockets; }
    void setMeteo(MeteoController &meteo) { _meteo = &meteo; }
    void setThermo(ThermoController &thermo) { _thermo = &thermo; }

    struct AllowedUser
    {
        String username;
        int64_t chat_id = 0;
        bool is_admin = false;
        bool is_notify = false;
        bool enabled = true;
    };

    enum class AllowResult : uint8_t
    {
        Ok = 0,
        Invalid,
        Exists,
        Full
    };

    void setAllowedUsers(const std::vector<AllowedUser> &users)
    {
        _allowed_users.clear();
        for (size_t i = 0; i < users.size(); ++i)
        {
            if (_allowed_users.size() >= kMaxAllowedUsers)
                break;
            AllowedUser u = users[i];
            u.username = normalizeUsername_(u.username);
            if (u.username.length() == 0 && u.chat_id == 0)
                continue;
            if ((u.username.length() && hasAllowedUsername_(u.username)) ||
                (u.chat_id != 0 && hasAllowedUserChatId_(u.chat_id)))
                continue;
            _allowed_users.push_back(u);
        }
    }

    AllowResult addAllowedUser(const String &user)
    {
        AllowedUser u{};
        u.username = normalizeUsername_(user);
        u.is_admin = true;
        return addAllowedUser(u);
    }

    AllowResult addAllowedUser(const AllowedUser &user)
    {
        AllowedUser u = user;
        u.username = normalizeUsername_(u.username);
        if (u.username.length() == 0 && u.chat_id == 0)
            return AllowResult::Invalid;
        if ((u.username.length() && hasAllowedUsername_(u.username)) ||
            (u.chat_id != 0 && hasAllowedUserChatId_(u.chat_id)))
            return AllowResult::Exists;
        if (_allowed_users.size() >= kMaxAllowedUsers)
            return AllowResult::Full;
        _allowed_users.push_back(u);
        return AllowResult::Ok;
    }

    bool removeAllowedUser(const String &user)
    {
        String u = normalizeUsername_(user);
        if (u.length() == 0)
            return false;
        for (size_t i = 0; i < _allowed_users.size(); ++i)
        {
            if (_allowed_users[i].username == u)
            {
                _allowed_users.erase(_allowed_users.begin() + (int)i);
                return true;
            }
        }
        return false;
    }

    void clearAllowedUsers() { _allowed_users.clear(); }

    const std::vector<AllowedUser> &allowedUsers() const { return _allowed_users; }

    static constexpr size_t kMaxAllowedUsers = 10;

private:
    static inline TelegramMenu *_self = nullptr;

    PlcControl &_plc;
    WifiManager &_wifi;
    RTC &_rtc;
    String _admin_password;
    Configs &_configs;
    ConfigsManagerIface *_configs_manager = nullptr;
    std::vector<AllowedUser> _allowed_users;

    struct ChatAuth
    {
        int64_t chat_id = 0;
        String user_id;
        bool authorized = false;
        bool awaiting = false;
        bool awaiting_config = false;
        bool awaiting_device = false;
        bool awaiting_socket = false;
        uint8_t fail_count = 0;
        uint32_t lock_until_ms = 0;
        bool selected_local = true;
        uint32_t selected_node_id = 0;
        uint8_t socket_action = 0;
    };

    std::vector<ChatAuth> _auth;
    static constexpr size_t kMaxConfigBytes = 8192;
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

    static bool cmdSockets_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        (void)reply;
        if (!_self)
            return false;
        if (!requireAdmin_(*_self, bot, u, reply))
            return true;
        _self->sendSocketMenu_(u.chat_id);
        return true;
    }

    static bool cmdMeteo_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        (void)reply;
        if (!_self)
            return false;
        if (!requireAdmin_(*_self, bot, u, reply))
            return true;
        if (!_self->_meteo)
        {
            reply = "Метео недоступно";
            return true;
        }
        if (!_self->isLocalSelected_(u.chat_id))
        {
            reply = "Список доступен только для локального устройства";
            return true;
        }
        _self->sendMeteoMenu_(u.chat_id);
        return true;
    }

    static bool cmdThermo_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        (void)reply;
        if (!_self)
            return false;
        if (!requireAdmin_(*_self, bot, u, reply))
            return true;
        if (!_self->_thermo)
        {
            reply = "Термо недоступно";
            return true;
        }
        if (!_self->isLocalSelected_(u.chat_id))
        {
            reply = "Список доступен только для локального устройства";
            return true;
        }
        _self->sendThermoMenu_(u.chat_id);
        return true;
    }

    static bool cmdSocketList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        if (!_self)
            return false;
        if (!requireAdmin_(*_self, bot, u, reply))
            return true;
        if (!_self->_sockets)
        {
            reply = "Розетки недоступны";
            return true;
        }
        if (!_self->isLocalSelected_(u.chat_id))
        {
            reply = "Список доступен только для локального устройства";
            return true;
        }
        const String text = _self->socketListTextHtml_();
        bot.sendText(u.chat_id, text, "", "HTML");
        return true;
    }

    static bool cmdMeteoList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        if (!_self)
            return false;
        if (!requireAdmin_(*_self, bot, u, reply))
            return true;
        if (!_self->_meteo)
        {
            reply = "Метео недоступно";
            return true;
        }
        if (!_self->isLocalSelected_(u.chat_id))
        {
            reply = "Список доступен только для локального устройства";
            return true;
        }
        const String text = _self->meteoListTextHtml_();
        bot.sendText(u.chat_id, text, "", "HTML");
        return true;
    }

    static bool cmdMeteoShow_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        if (!_self)
            return false;
        if (!requireAdmin_(*_self, bot, u, reply))
            return true;
        if (!_self->_meteo)
        {
            reply = "Метео недоступно";
            return true;
        }
        if (!_self->isLocalSelected_(u.chat_id))
        {
            reply = "Список доступен только для локального устройства";
            return true;
        }
        const char *cmd = "/meteo_show";
        String tail = u.text.substring(strlen(cmd));
        tail.trim();
        uint8_t id = 0;
        if (!parseMeteoId_(tail, id))
        {
            reply = "Использование: /meteo_show <id>";
            return true;
        }
        const String text = _self->meteoSensorTextHtml_(id);
        bot.sendText(u.chat_id, text, "", "HTML");
        return true;
    }

    static bool cmdThermoList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        if (!_self)
            return false;
        if (!requireAdmin_(*_self, bot, u, reply))
            return true;
        if (!_self->_thermo)
        {
            reply = "Термо недоступно";
            return true;
        }
        if (!_self->isLocalSelected_(u.chat_id))
        {
            reply = "Список доступен только для локального устройства";
            return true;
        }
        const String text = _self->thermoListTextHtml_();
        bot.sendText(u.chat_id, text, "", "HTML");
        return true;
    }

    static bool cmdThermoShow_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        if (!_self)
            return false;
        if (!requireAdmin_(*_self, bot, u, reply))
            return true;
        if (!_self->_thermo)
        {
            reply = "Термо недоступно";
            return true;
        }
        if (!_self->isLocalSelected_(u.chat_id))
        {
            reply = "Список доступен только для локального устройства";
            return true;
        }
        const char *cmd = "/thermo_show";
        String tail = u.text.substring(strlen(cmd));
        tail.trim();
        uint8_t id = 0;
        if (!parseThermoId_(tail, id))
        {
            reply = "Использование: /thermo_show <id>";
            return true;
        }
        const String text = _self->thermoDeviceTextHtml_(id);
        bot.sendText(u.chat_id, text, "", "HTML");
        return true;
    }

    static bool cmdSocketOn_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        return startSocketAction_(bot, u, reply, 1);
    }

    static bool cmdSocketOff_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        return startSocketAction_(bot, u, reply, 2);
    }

    static bool cmdSocketToggle_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        return startSocketAction_(bot, u, reply, 3);
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
            reply = "Логгер недоступен";
            return true;
        }
        const size_t count = _self->_logs->recentCount();
        if (count == 0)
        {
            reply = "Логи пусты";
            return true;
        }
        reply = "Логи:\n";
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
            reply = "Список разрешенных пользователей пуст.";
            return true;
        }
        reply = "Разрешенные пользователи:";
        for (size_t i = 0; i < _self->_allowed_users.size(); ++i)
        {
            const auto &user = _self->_allowed_users[i];
            reply += "\n  ";
            reply += String((unsigned)(i + 1));
            reply += " ";
            reply += user.username.length() ? user.username : String("-");
            if (user.chat_id)
            {
                reply += " chat=";
                reply += String((long long)user.chat_id);
            }
            reply += " admin=";
            reply += user.is_admin ? "1" : "0";
            reply += " notify=";
            reply += user.is_notify ? "1" : "0";
            reply += " enabled=";
            reply += user.enabled ? "1" : "0";
        }
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
            reply = "Использование: /allow_add <username> [chat_id] [admin] [notify] [off]";
            return true;
        }
        AllowedUser user{};
        parseAllowUserSpec_(name, user);
        AllowResult res = _self->addAllowedUser(user);
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
        ChatAuth *st = self->ensureAuth_(u.chat_id);
        if (st)
            st->user_id = normalizeUsername_(u.from);
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
        st = self->findAuth_(u.chat_id);
        if (st && st->awaiting_socket)
        {
            uint8_t id = 0;
            if (!parseSocketId_(u.text, id))
            {
                String msg = String("Введите ID розетки (1..") + String(SocketController::kSocketCount) + ").";
                self->_bot->sendText(u.chat_id, msg);
                return true;
            }
            if (!self->_sockets)
            {
                self->_bot->sendText(u.chat_id, F("Розетки недоступны"));
                st->awaiting_socket = false;
                st->socket_action = 0;
                return true;
            }
            if (!self->isLocalSelected_(u.chat_id))
            {
                self->_bot->sendText(u.chat_id, F("Доступно только для локального устройства"));
                st->awaiting_socket = false;
                st->socket_action = 0;
                return true;
            }
            bool ok = false;
            if (st->socket_action == 1)
                ok = self->_sockets->setRelayById(id, true);
            else if (st->socket_action == 2)
                ok = self->_sockets->setRelayById(id, false);
            else if (st->socket_action == 3)
                ok = self->_sockets->toggleRelayById(id);
            st->awaiting_socket = false;
            st->socket_action = 0;
            if (!ok)
                self->_bot->sendText(u.chat_id, F("Не удалось"));
            else
                self->sendSocketMenu_(u.chat_id);
            return true;
        }
        if (self->handleSocketToggleSelection_(u))
            return true;
        if (self->handleMeteoSelection_(u))
            return true;
        if (self->handleThermoSelection_(u))
            return true;
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
        st->awaiting_socket = false;
        st->socket_action = 0;
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
        st->awaiting_socket = false;
        st->socket_action = 0;
    }

    TelegramBot *_bot = nullptr;
    Logger *_logs = nullptr;
    StackMaster *_stack_master = nullptr;
    SocketController *_sockets = nullptr;
    MeteoController *_meteo = nullptr;
    ThermoController *_thermo = nullptr;

        static inline const TelegramBot::MenuItem kRootItems[] = {};

    static inline const TelegramBot::MenuItem kDeviceItems[] = {
        { "Админка", "Админка", nullptr, nullptr },
        { "Розетки", "/sockets", nullptr, nullptr },
        { "Метео", "/meteo", nullptr, nullptr },
        { "Термо", "/thermo", nullptr, nullptr },
        { "Назад", "/back", nullptr, nullptr },
    };

    static inline const TelegramBot::MenuItem kSocketsItems[] = {};
    static inline const TelegramBot::MenuItem kMeteoItems[] = {};
    static inline const TelegramBot::MenuItem kThermoItems[] = {};

    static inline const TelegramBot::MenuItem kAdminItems[] = {
        { "ПЛК", nullptr, "plc", nullptr },
        { "Часы", nullptr, "rtc", nullptr },
        { "Wi-Fi", nullptr, "wifi", nullptr },
        { "Настройки", nullptr, "settings", nullptr },
        { "Логи", "/logs", nullptr, nullptr },
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
        { "device", "Меню устройства", kDeviceItems, 5, "root" },
        { "sockets", "Розетки", kSocketsItems, 0, "device" },
        { "meteo", "Метео", kMeteoItems, 0, "device" },
        { "thermo", "Термо", kThermoItems, 0, "device" },
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
        { "/sockets", &TelegramMenu::cmdSockets_ },
        { "/socket_list", &TelegramMenu::cmdSocketList_ },
        { "/socket_on", &TelegramMenu::cmdSocketOn_ },
        { "/socket_off", &TelegramMenu::cmdSocketOff_ },
        { "/socket_toggle", &TelegramMenu::cmdSocketToggle_ },
        { "/meteo", &TelegramMenu::cmdMeteo_ },
        { "/meteo_list", &TelegramMenu::cmdMeteoList_ },
        { "/meteo_show", &TelegramMenu::cmdMeteoShow_ },
        { "/thermo", &TelegramMenu::cmdThermo_ },
        { "/thermo_list", &TelegramMenu::cmdThermoList_ },
        { "/thermo_show", &TelegramMenu::cmdThermoShow_ },
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
    static String normalizeUsername_(String user)
    {
        user.trim();
        if (user.startsWith("@"))
            user.remove(0, 1);
        user.toLowerCase();
        return user;
    }

    bool hasAllowedUsername_(const String &user) const
    {
        for (size_t i = 0; i < _allowed_users.size(); ++i)
        {
            if (_allowed_users[i].username == user)
                return true;
        }
        return false;
    }

    bool hasAllowedUserChatId_(int64_t chat_id) const
    {
        for (size_t i = 0; i < _allowed_users.size(); ++i)
        {
            if (_allowed_users[i].chat_id != 0 && _allowed_users[i].chat_id == chat_id)
                return true;
        }
        return false;
    }

    bool isAllowedUser_(const TelegramClient::Update &u) const
    {
        if (_allowed_users.empty())
            return true;
        size_t idx = 0;
        if (u.chat_id != 0 && findAllowedUserByChatId_(u.chat_id, idx))
            return _allowed_users[idx].enabled;
        String user = normalizeUsername_(u.from);
        if (user.length() == 0)
            return false;
        if (findAllowedUserByName_(user, idx))
            return _allowed_users[idx].enabled;
        return false;
    }

    bool isAdminChat_(int64_t chat_id) const
    {
        if (_allowed_users.empty())
            return false;
        size_t idx = 0;
        if (chat_id != 0 && findAllowedUserByChatId_(chat_id, idx))
            return _allowed_users[idx].enabled && _allowed_users[idx].is_admin;
        const ChatAuth *st = findAuth_(chat_id);
        if (st && st->user_id.length() && findAllowedUserByName_(st->user_id, idx))
            return _allowed_users[idx].enabled && _allowed_users[idx].is_admin;
        return false;
    }

    bool findAllowedUserByName_(const String &name, size_t &out) const
    {
        for (size_t i = 0; i < _allowed_users.size(); ++i)
        {
            if (_allowed_users[i].username == name)
            {
                out = i;
                return true;
            }
        }
        return false;
    }

    bool findAllowedUserByChatId_(int64_t chat_id, size_t &out) const
    {
        for (size_t i = 0; i < _allowed_users.size(); ++i)
        {
            if (_allowed_users[i].chat_id != 0 && _allowed_users[i].chat_id == chat_id)
            {
                out = i;
                return true;
            }
        }
        return false;
    }

    static void parseAllowUserSpec_(const String &spec, AllowedUser &out)
    {
        out = AllowedUser{};
        String s = spec;
        s.trim();
        if (s.length() == 0)
            return;
        int start = 0;
        int part = 0;
        while (start < (int)s.length())
        {
            int space = s.indexOf(' ', start);
            if (space < 0)
                space = s.length();
            String token = s.substring(start, (size_t)space);
            token.trim();
            if (token.length())
            {
                if (part == 0)
                {
                    out.username = token;
                }
                else if (token == "admin")
                {
                    out.is_admin = true;
                }
                else if (token == "notify")
                {
                    out.is_notify = true;
                }
                else if (token == "off" || token == "disabled")
                {
                    out.enabled = false;
                }
                else
                {
                    const char *c = token.c_str();
                    bool numeric = true;
                    for (size_t i = 0; c[i]; ++i)
                    {
                        if (c[i] < '0' || c[i] > '9')
                        {
                            numeric = false;
                            break;
                        }
                    }
                    if (numeric)
                        out.chat_id = (int64_t)strtoll(c, nullptr, 10);
                }
            }
            start = space + 1;
            ++part;
        }
    }

    static bool parseSocketIdFromText_(const String &text, uint8_t &out)
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
        return parseSocketId_(num, out);
    }

    static bool parseSocketId_(const String &text, uint8_t &out)
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
        if (v <= 0 || v > (int)SocketController::kSocketCount)
            return false;
        out = (uint8_t)v;
        return true;
    }

    static bool parseSocketLabel_(const String &text, uint8_t &out)
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
            return parseSocketIdFromText_(head, out);
        }
        String low = t;
        low.toLowerCase();
        if (low.startsWith("socket"))
        {
            String tail = t.substring(6);
            tail.trim();
            return parseSocketIdFromText_(tail, out);
        }
        return parseSocketIdFromText_(t, out);
    }

    static bool parseMeteoIdFromText_(const String &text, uint8_t &out)
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
        return parseMeteoId_(num, out);
    }

    static bool parseMeteoId_(const String &text, uint8_t &out)
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
        if (v <= 0 || v > (int)MeteoController::kSensorCount)
            return false;
        out = (uint8_t)v;
        return true;
    }

    static bool parseMeteoLabel_(const String &text, uint8_t &out)
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
            return parseMeteoIdFromText_(head, out);
        }
        String low = t;
        low.toLowerCase();
        if (low.startsWith("meteo"))
        {
            String tail = t.substring(5);
            tail.trim();
            return parseMeteoIdFromText_(tail, out);
        }
        if (low.startsWith("sensor"))
        {
            String tail = t.substring(6);
            tail.trim();
            return parseMeteoIdFromText_(tail, out);
        }
        return parseMeteoIdFromText_(t, out);
    }

    static bool parseThermoIdFromText_(const String &text, uint8_t &out)
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
        return parseThermoId_(num, out);
    }

    static bool parseThermoId_(const String &text, uint8_t &out)
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
        if (v <= 0 || v > (int)ThermoController::kDeviceCount)
            return false;
        out = (uint8_t)v;
        return true;
    }

    static bool parseThermoLabel_(const String &text, uint8_t &out)
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
            return parseThermoIdFromText_(head, out);
        }
        String low = t;
        low.toLowerCase();
        if (low.startsWith("thermo"))
        {
            String tail = t.substring(6);
            tail.trim();
            return parseThermoIdFromText_(tail, out);
        }
        return parseThermoIdFromText_(t, out);
    }

    static bool startSocketAction_(TelegramBot &bot, const TelegramClient::Update &u, String &reply, uint8_t action)
    {
        if (!_self)
            return false;
        if (!requireAdmin_(*_self, bot, u, reply))
            return true;
        ChatAuth *st = _self->ensureAuth_(u.chat_id);
        if (!st)
            return false;
        st->awaiting_socket = true;
        st->socket_action = action;
        String msg = String("Введите ID розетки (1..") + String(SocketController::kSocketCount) + "):";
        bot.sendText(u.chat_id, msg);
        return true;
    }

    bool isLocalSelected_(int64_t chat_id) const
    {
        const ChatAuth *st = findAuth_(chat_id);
        if (!st)
            return true;
        return st->selected_local || st->selected_node_id == 0;
    }

    void buildSocketLabels_(std::vector<String> &out) const
    {
        out.clear();
        if (!_sockets)
            out.reserve(4);
        else
            out.reserve(16);
        if (_sockets)
        {
            for (size_t i = 0; i < SocketController::kSocketCount; ++i)
            {
                const auto *cfg = _sockets->configByIndex(i);
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
                    label += F("Socket ");
                    label += String((unsigned)cfg->id);
                }
                out.push_back(label);
            }
        }
        out.push_back(F("Назад"));
    }

    void buildMeteoLabels_(std::vector<String> &out) const
    {
        out.clear();
        if (!_meteo)
        {
            out.reserve(1);
            out.push_back(F("Назад"));
            return;
        }
        for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
        {
            const auto *cfg = _meteo->configByIndex(i);
            if (!cfg || !cfg->enabled)
                continue;
            String label;
            label += String((unsigned)cfg->id);
            label += ": ";
            label += MeteoController::typeName(cfg->type);
            out.push_back(label);
        }
        out.push_back(F("Назад"));
    }

    void buildThermoLabels_(std::vector<String> &out) const
    {
        out.clear();
        if (!_thermo)
        {
            out.reserve(1);
            out.push_back(F("Назад"));
            return;
        }
        for (size_t i = 0; i < ThermoController::kDeviceCount; ++i)
        {
            const auto *cfg = _thermo->configByIndex(i);
            if (!cfg || !cfg->enabled)
                continue;
            String label;
            label += String((unsigned)cfg->id);
            label += ": ";
            label += thermoModeLabel_(cfg->mode);
            out.push_back(label);
        }
        out.push_back(F("Назад"));
    }

    String socketListTextHtml_() const
    {
        String out = F("<b>Розетки:</b>");
        if (!_sockets)
        {
            out += F("\n  недоступны");
            return out;
        }
        bool any = false;
        for (size_t i = 0; i < SocketController::kSocketCount; ++i)
        {
            const auto *cfg = _sockets->configByIndex(i);
            const auto *st = _sockets->stateByIndex(i);
            if (!cfg || !st || !cfg->enabled)
                continue;
            any = true;
            out += "\n  ";
            out += st->relay_on ? F("?? ") : F("?? ");
            out += String((unsigned)cfg->id);
            out += ": ";
            if (cfg->name.length())
                out += escapeHtml_(cfg->name);
            else
                out += "-";
        }
        if (!any)
            out += F("\n  пусто");
        return out;
    }

    String meteoListTextHtml_() const
    {
        String out = F("<b>Метео:</b>");
        if (!_meteo)
        {
            out += F("\n  недоступно");
            return out;
        }
        bool any = false;
        for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
        {
            const auto *cfg = _meteo->configByIndex(i);
            const auto *st = _meteo->stateByIndex(i);
            if (!cfg || !st || !cfg->enabled)
                continue;
            any = true;
            out += "\n  ";
            out += String((unsigned)cfg->id);
            out += ": ";
            out += MeteoController::typeName(cfg->type);
            if (st->has_temp)
            {
                char buf[10] = {};
                dtostrf(st->temp_c, 0, 1, buf);
                out += " t=";
                out += buf;
            }
            if (st->has_humidity)
            {
                char buf[10] = {};
                dtostrf(st->humidity, 0, 1, buf);
                out += " h=";
                out += buf;
            }
            if (!st->has_temp && !st->has_humidity)
                out += " -";
        }
        if (!any)
            out += F("\n  пусто");
        return out;
    }

    String meteoSensorTextHtml_(uint8_t id) const
    {
        if (!_meteo)
            return F("Метео недоступно");
        const auto *cfg = _meteo->config(id);
        const auto *st = _meteo->state(id);
        if (!cfg || !st)
            return F("Неверный датчик");
        String out = F("<b>Датчик метео:</b>");
        out += "\n  id: ";
        out += String((unsigned)cfg->id);
        out += "\n  enabled: ";
        out += cfg->enabled ? "1" : "0";
        out += "\n  type: ";
        out += MeteoController::typeName(cfg->type);
        if (cfg->type == MeteoController::SensorType::Dht22)
        {
            out += "\n  pin: ";
            if (cfg->dht_pin != MeteoController::kInvalidPin)
                out += String((unsigned)cfg->dht_pin);
            else
                out += "-";
        }
        if (cfg->type == MeteoController::SensorType::Ds18b20)
        {
            out += "\n  addr: ";
            if (cfg->ds18_addr_set)
            {
                char hex[17] = {};
                MeteoController::formatHexAddr(cfg->ds18_addr, hex);
                out += hex;
            }
            else
            {
                out += "-";
            }
        }
        out += "\n  temp: ";
        if (st->has_temp)
        {
            char buf[10] = {};
            dtostrf(st->temp_c, 0, 2, buf);
            out += buf;
        }
        else
        {
            out += "-";
        }
        out += "\n  hum: ";
        if (st->has_humidity)
        {
            char buf[10] = {};
            dtostrf(st->humidity, 0, 1, buf);
            out += buf;
        }
        else
        {
            out += "-";
        }
        out += "\n  ok: ";
        if (st->last_read_ms == 0)
            out += "-";
        else
            out += st->ok ? "OK" : "ERR";
        return out;
    }

    String thermoListTextHtml_() const
    {
        String out = F("<b>Термо:</b>");
        if (!_thermo)
        {
            out += F("\n  недоступно");
            return out;
        }
        bool any = false;
        for (size_t i = 0; i < ThermoController::kDeviceCount; ++i)
        {
            const auto *cfg = _thermo->configByIndex(i);
            const auto *st = _thermo->stateByIndex(i);
            if (!cfg || !st || !cfg->enabled)
                continue;
            any = true;
            out += "\n  ";
            out += String((unsigned)cfg->id);
            out += ": ";
            out += thermoModeLabel_(cfg->mode);
            out += " питание=";
            out += st->power_on ? "вкл" : "выкл";
            out += " нагрев=";
            out += st->heat_on ? "вкл" : "выкл";
            out += " охлажд=";
            out += st->cool_on ? "вкл" : "выкл";
        }
        if (!any)
            out += F("\n  пусто");
        return out;
    }

    String thermoDeviceTextHtml_(uint8_t id) const
    {
        if (!_thermo)
            return F("Термо недоступно");
        const auto *cfg = _thermo->config(id);
        const auto *st = _thermo->state(id);
        if (!cfg || !st)
            return F("Неверное устройство");
        String out = F("<b>Термо устройство:</b>");
        out += "\n  id: ";
        out += String((unsigned)cfg->id);
        out += "\n  enabled: ";
        out += cfg->enabled ? "1" : "0";
        out += "\n  режим: ";
        out += thermoModeLabel_(cfg->mode);
        out += "\n  датчик: ";
        if (cfg->sensor_id)
            out += String((unsigned)cfg->sensor_id);
        else
            out += "-";
        out += "\n  цель: ";
        out += String(cfg->target_c, 2);
        out += "\n  гист: ";
        out += String(cfg->hysteresis, 2);
        out += "\n  порт_нагрева: ";
        if (cfg->heat_port != ThermoController::kInvalidPort)
            out += String((unsigned)cfg->heat_port);
        else
            out += "-";
        out += "\n  порт_охл: ";
        if (cfg->cool_port != ThermoController::kInvalidPort)
            out += String((unsigned)cfg->cool_port);
        else
            out += "-";
        out += "\n  кнопка: ";
        if (cfg->button_port != ThermoController::kInvalidPort)
            out += String((unsigned)cfg->button_port);
        else
            out += "-";
        out += "\n  питание: ";
        out += st->power_on ? "вкл" : "выкл";
        out += "\n  нагрев: ";
        out += st->heat_on ? "вкл" : "выкл";
        out += "\n  охлаждение: ";
        out += st->cool_on ? "вкл" : "выкл";
        return out;
    }

    static const char *thermoModeLabel_(ThermoController::Mode mode)
    {
        switch (mode)
        {
        case ThermoController::Mode::Heat:
            return "только нагрев";
        case ThermoController::Mode::Cool:
            return "только охлаждение";
        case ThermoController::Mode::Auto:
            return "авто";
        case ThermoController::Mode::Off:
        default:
            return "выкл";
        }
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

    static String escapeHtml_(const String &in)
    {
        String out;
        out.reserve(in.length() + 8);
        for (size_t i = 0; i < in.length(); ++i)
        {
            const char c = in.charAt(i);
            switch (c)
            {
            case '&':
                out += F("&amp;");
                break;
            case '<':
                out += F("&lt;");
                break;
            case '>':
                out += F("&gt;");
                break;
            case '"':
                out += F("&quot;");
                break;
            case '\'':
                out += F("&#39;");
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
        if (strcmp(menu.id, "root") == 0)
        {
            std::vector<DeviceEntry> devices;
            self->buildDeviceList_(devices);
            std::vector<String> labels;
            labels.reserve(devices.size());
            for (const auto &d : devices)
                labels.push_back(d.label);
            return buildKeyboardMarkup_(labels);
        }
        if (strcmp(menu.id, "sockets") == 0)
        {
            std::vector<String> labels;
            self->buildSocketLabels_(labels);
            return buildKeyboardMarkup_(labels);
        }
        if (strcmp(menu.id, "meteo") == 0)
        {
            std::vector<String> labels;
            self->buildMeteoLabels_(labels);
            return buildKeyboardMarkup_(labels);
        }
        if (strcmp(menu.id, "thermo") == 0)
        {
            std::vector<String> labels;
            self->buildThermoLabels_(labels);
            return buildKeyboardMarkup_(labels);
        }
        if (strcmp(menu.id, "device") == 0)
        {
            std::vector<String> labels;
            labels.reserve(5);
            if (self->isAdminChat_(chat_id))
                labels.push_back(F("Админка"));
            labels.push_back(F("Розетки"));
            labels.push_back(F("Метео"));
            labels.push_back(F("Термо"));
            labels.push_back(F("Назад"));
            return buildKeyboardMarkup_(labels);
        }
        return "";
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

    void sendSocketMenu_(int64_t chat_id)
    {
        if (!_bot)
            return;
        if (!isLocalSelected_(chat_id))
        {
            _bot->sendText(chat_id, F("Список доступен только для локального устройства"));
            return;
        }
        std::vector<String> labels;
        buildSocketLabels_(labels);
        const String markup = buildKeyboardMarkup_(labels);
        const String list = socketListTextHtml_();
        _bot->setMenu(chat_id, "sockets");
        _bot->sendText(chat_id, list, markup, "HTML");
    }

    void sendMeteoMenu_(int64_t chat_id)
    {
        if (!_bot)
            return;
        if (!isLocalSelected_(chat_id))
        {
            _bot->sendText(chat_id, F("Список доступен только для локального устройства"));
            return;
        }
        std::vector<String> labels;
        buildMeteoLabels_(labels);
        const String markup = buildKeyboardMarkup_(labels);
        const String list = meteoListTextHtml_();
        _bot->setMenu(chat_id, "meteo");
        _bot->sendText(chat_id, list, markup, "HTML");
    }

    void sendThermoMenu_(int64_t chat_id)
    {
        if (!_bot)
            return;
        if (!isLocalSelected_(chat_id))
        {
            _bot->sendText(chat_id, F("Список доступен только для локального устройства"));
            return;
        }
        std::vector<String> labels;
        buildThermoLabels_(labels);
        const String markup = buildKeyboardMarkup_(labels);
        const String list = thermoListTextHtml_();
        _bot->setMenu(chat_id, "thermo");
        _bot->sendText(chat_id, list, markup, "HTML");
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

    bool handleSocketToggleSelection_(const TelegramClient::Update &u)
    {
        if (!_bot)
            return false;
        const char *menu_id = _bot->currentMenuId(u.chat_id);
        if (!menu_id || strcmp(menu_id, "sockets") != 0)
            return false;
        if (u.text == F("Назад"))
        {
            _bot->enterMenu(u.chat_id, "device", adminPrefix_(u.chat_id));
            return true;
        }
        uint8_t id = 0;
        if (!parseSocketLabel_(u.text, id))
        {
            _bot->sendText(u.chat_id, F("Неизвестная розетка"));
            return true;
        }
        if (!_sockets)
        {
            _bot->sendText(u.chat_id, F("Розетки недоступны"));
            return true;
        }
        if (!isLocalSelected_(u.chat_id))
        {
            _bot->sendText(u.chat_id, F("Доступно только для локального устройства"));
            return true;
        }
        if (!_sockets->toggleRelayById(id))
        {
            _bot->sendText(u.chat_id, F("Не удалось"));
            return true;
        }
        sendSocketMenu_(u.chat_id);
        return true;
    }

    bool handleMeteoSelection_(const TelegramClient::Update &u)
    {
        if (!_bot)
            return false;
        const char *menu_id = _bot->currentMenuId(u.chat_id);
        if (!menu_id || strcmp(menu_id, "meteo") != 0)
            return false;
        if (u.text == F("Назад"))
        {
            _bot->enterMenu(u.chat_id, "device", adminPrefix_(u.chat_id));
            return true;
        }
        uint8_t id = 0;
        if (!parseMeteoLabel_(u.text, id))
        {
            _bot->sendText(u.chat_id, F("Неизвестный датчик"));
            return true;
        }
        if (!_meteo)
        {
            _bot->sendText(u.chat_id, F("Метео недоступно"));
            return true;
        }
        if (!isLocalSelected_(u.chat_id))
        {
            _bot->sendText(u.chat_id, F("Доступно только для локального устройства"));
            return true;
        }
        const String text = meteoSensorTextHtml_(id);
        _bot->sendText(u.chat_id, text, "", "HTML");
        return true;
    }

    bool handleThermoSelection_(const TelegramClient::Update &u)
    {
        if (!_bot)
            return false;
        const char *menu_id = _bot->currentMenuId(u.chat_id);
        if (!menu_id || strcmp(menu_id, "thermo") != 0)
            return false;
        if (u.text == F("Назад"))
        {
            _bot->enterMenu(u.chat_id, "device", adminPrefix_(u.chat_id));
            return true;
        }
        uint8_t id = 0;
        if (!parseThermoLabel_(u.text, id))
        {
            _bot->sendText(u.chat_id, F("Неизвестное устройство"));
            return true;
        }
        if (!_thermo)
        {
            _bot->sendText(u.chat_id, F("Термо недоступно"));
            return true;
        }
        if (!isLocalSelected_(u.chat_id))
        {
            _bot->sendText(u.chat_id, F("Доступно только для локального устройства"));
            return true;
        }
        const String text = thermoDeviceTextHtml_(id);
        _bot->sendText(u.chat_id, text, "", "HTML");
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
