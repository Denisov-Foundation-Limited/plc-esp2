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
#include <LittleFS.h>

#include "core/network/telegram/telegram_allowed_users.hpp"
#include "core/network/telegram/telegram_bot.hpp"
#include "core/rtc.hpp"
#include "core/network/wifi_manager.hpp"
#include "plc/plc_control.hpp"
#include "controllers/socket_controller.hpp"
#include "controllers/meteo_controller.hpp"
#include "controllers/thermo_controller.hpp"
#include "controllers/tank_controller.hpp"
#include "controllers/septic_controller.hpp"
#include "controllers/security_controller.hpp"
#include "utils/meteo_history.hpp"
#include "utils/configs.hpp"
#include "utils/configs_manager_iface.hpp"
#include "utils/logger.hpp"
#include "core/network/stack/stack_master.hpp"

class TelegramMenuThermo;
class TelegramMenuSockets;
class TelegramMenuMeteo;
class TelegramMenuTanks;
class TelegramMenuSeptic;
class TelegramMenuSecurity;

class TelegramMenu : public TelegramAllowedUsersProvider
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
    void setTanks(TankController &tanks) { _tanks = &tanks; }
    void setSeptic(SepticController &septic) { _septic = &septic; }
    void setSecurity(SecurityController &security) { _security = &security; }

    using AllowedUser = TelegramAllowedUser;

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

    const std::vector<AllowedUser> &allowedUsers() const override { return _allowed_users; }

    static constexpr size_t kMaxAllowedUsers = 10;

private:
    friend class TelegramMenuThermo;
    friend class TelegramMenuSockets;
    friend class TelegramMenuMeteo;
    friend class TelegramMenuTanks;
    friend class TelegramMenuSeptic;
    friend class TelegramMenuSecurity;
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
        bool awaiting_thermo = false;
        bool awaiting_tank = false;
        uint8_t fail_count = 0;
        uint32_t lock_until_ms = 0;
        bool selected_local = true;
        uint32_t selected_node_id = 0;
        uint8_t socket_action = 0;
        uint8_t selected_thermo_id = 0;
        uint8_t selected_tank_id = 0;
    };

    std::vector<ChatAuth> _auth;
    static constexpr size_t kMaxConfigBytes = 8192;
    static constexpr size_t kConfigDocCapacity = 12288;
    DynamicJsonDocument _cfg_doc{kConfigDocCapacity};
    static constexpr uint32_t kHistoryMagic = 0x4D544831u; // "MTH1"
    static constexpr float kThermoTargetStep = 1.0f;
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

    static bool cmdSockets_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdMeteo_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdThermo_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdTanks_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdSeptic_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdSecurity_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdSepticStatus_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdSepticList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdSepticMonitor_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdSecurityStatus_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdSecurityList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdSecurityArm_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdSecurityDisarm_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdSecuritySilent_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdSocketList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdMeteoList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdMeteoShow_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdThermoList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdTanksList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdThermoShow_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdTanksShow_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdSocketOn_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdSocketOff_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdSocketToggle_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

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
        self._cfg_doc.clear();
        DeserializationError err = deserializeJson(self._cfg_doc, json);
        if (err)
        {
            self._bot->sendText(u.chat_id, F("Ошибка разбора JSON."));
            return true;
        }
        const bool saved = self._configs_manager ? self._configs_manager->save(self._cfg_doc)
                                                 : self._configs.save(self._cfg_doc);
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
            if (!parseSocketId_(u.text, id));
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
        if (self->handleThermoAction_(u))
            return true;
        if (self->handleTankAction_(u))
            return true;
        if (self->handleSocketToggleSelection_(u))
            return true;
        if (self->handleMeteoSelection_(u))
            return true;
        if (self->handleThermoSelection_(u))
            return true;
        if (self->handleTankSelection_(u))
            return true;
        if (self->handleSepticSelection_(u))
            return true;
        if (self->handleSecuritySelection_(u))
            return true;
        if (self->handleRootDeviceSelection_(u))
            return true;
        if (st && st->awaiting_device);
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
            self->_cfg_doc.clear();
            DeserializationError err = deserializeJson(self->_cfg_doc, u.text);
            if (err)
            {
                if (self->_bot)
                    self->_bot->sendText(u.chat_id, F("Ошибка разбора JSON."));
                return true;
            }
            const bool saved = self->_configs_manager ? self->_configs_manager->save(self->_cfg_doc)
                                                      : self->_configs.save(self->_cfg_doc);
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
        st->awaiting_thermo = false;
        st->selected_thermo_id = 0;
        st->awaiting_tank = false;
        st->selected_tank_id = 0;
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
        st->awaiting_thermo = false;
        st->selected_thermo_id = 0;
        st->awaiting_tank = false;
        st->selected_tank_id = 0;
    }

    TelegramBot *_bot = nullptr;
    Logger *_logs = nullptr;
    StackMaster *_stack_master = nullptr;
    SocketController *_sockets = nullptr;
    MeteoController *_meteo = nullptr;
    ThermoController *_thermo = nullptr;
    TankController *_tanks = nullptr;
    SepticController *_septic = nullptr;
    SecurityController *_security = nullptr;

        static inline const TelegramBot::MenuItem kRootItems[] = {};

    static inline const TelegramBot::MenuItem kDeviceItems[] = {
        { "Админка", "Админка", nullptr, nullptr },
        { "Розетки", "/sockets", nullptr, nullptr },
        { "Метео", "/meteo", nullptr, nullptr },
        { "Термо", "/thermo", nullptr, nullptr },
        { "Баки", "/tanks", nullptr, nullptr },
        { "Септик", "/septic", nullptr, nullptr },
        { "Охрана", "/security", nullptr, nullptr },
        { "Назад", "/back", nullptr, nullptr },
    };

    static inline const TelegramBot::MenuItem kSocketsItems[] = {};
    static inline const TelegramBot::MenuItem kMeteoItems[] = {};
    static inline const TelegramBot::MenuItem kThermoItems[] = {};
    static inline const TelegramBot::MenuItem kTanksItems[] = {};
    static inline const TelegramBot::MenuItem kSepticItems[] = {};
    static inline const TelegramBot::MenuItem kSecurityItems[] = {};

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
        { "device", "Меню устройства", kDeviceItems, 7, "root" },
        { "sockets", "Розетки", kSocketsItems, 0, "device" },
        { "meteo", "Метео", kMeteoItems, 0, "device" },
        { "thermo", "Термо", kThermoItems, 0, "device" },
        { "tanks", "Баки", kTanksItems, 0, "device" },
        { "septic", "Септик", kSepticItems, 0, "device" },
        { "security", "Охрана", kSecurityItems, 0, "device" },
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
        { "/tanks", &TelegramMenu::cmdTanks_ },
        { "/tanks_list", &TelegramMenu::cmdTanksList_ },
        { "/tanks_show", &TelegramMenu::cmdTanksShow_ },
        { "/septic", &TelegramMenu::cmdSeptic_ },
        { "/septic_status", &TelegramMenu::cmdSepticStatus_ },
        { "/septic_list", &TelegramMenu::cmdSepticList_ },
        { "/septic_monitor", &TelegramMenu::cmdSepticMonitor_ },
        { "/security", &TelegramMenu::cmdSecurity_ },
        { "/security_status", &TelegramMenu::cmdSecurityStatus_ },
        { "/security_list", &TelegramMenu::cmdSecurityList_ },
        { "/security_arm", &TelegramMenu::cmdSecurityArm_ },
        { "/security_disarm", &TelegramMenu::cmdSecurityDisarm_ },
        { "/security_silent", &TelegramMenu::cmdSecuritySilent_ },
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

    static bool parseSocketIdFromText_(const String &text, uint8_t &out);

    static bool parseSocketId_(const String &text, uint8_t &out);

    static bool parseSocketLabel_(const String &text, uint8_t &out);

    static bool parseMeteoIdFromText_(const String &text, uint8_t &out);

    static bool parseMeteoId_(const String &text, uint8_t &out);

    static bool parseMeteoLabel_(const String &text, uint8_t &out);

    static bool parseThermoIdFromText_(const String &text, uint8_t &out);

    static bool parseThermoId_(const String &text, uint8_t &out);

    static bool parseThermoLabel_(const String &text, uint8_t &out);

    static bool parseSecurityId_(const String &text, uint8_t &out);

    static bool parseSepticId_(const String &text, uint8_t &out);

    static bool parseTankIdFromText_(const String &text, uint8_t &out);

    static bool parseTankId_(const String &text, uint8_t &out);

    static bool parseTankLabel_(const String &text, uint8_t &out);

    static bool parseOnOff_(const String &text, bool &out)
    {
        String t = text;
        t.trim();
        t.toLowerCase();
        if (t == "1" || t == "on" || t == "yes" || t == "true")
        {
            out = true;
            return true;
        }
        if (t == "0" || t == "off" || t == "no" || t == "false")
        {
            out = false;
            return true;
        }
        return false;
    }

    static bool startSocketAction_(TelegramBot &bot, const TelegramClient::Update &u, String &reply, uint8_t action);

    bool isLocalSelected_(int64_t chat_id) const
    {
        const ChatAuth *st = findAuth_(chat_id);
        if (!st)
            return true;
        return st->selected_local || st->selected_node_id == 0;
    }

    void buildSocketLabels_(std::vector<String> &out) const;

    void buildMeteoLabels_(std::vector<String> &out) const;

    void buildThermoLabels_(std::vector<String> &out) const;

    void buildTankLabels_(std::vector<String> &out) const;

    String socketListTextHtml_() const;

    String securityStatusText_() const;

    String septicStatusText_() const;

    String securityListTextHtml_() const;

    String septicListTextHtml_() const;

    String meteoListTextHtml_() const;

    String meteoSensorTextHtml_(uint8_t id) const;

    String meteoHistoryTextHtml_(uint8_t id) const;

    static uint8_t scaleBars_(float v, float max_v);

    static String barString_(uint8_t bars);

    String thermoListTextHtml_() const;

    String thermoDeviceTextHtml_(uint8_t id) const;

    static const char *tankLevelLabel_(const TankController::TankState &st);

    String tankListTextHtml_() const;

    String tankDeviceTextHtml_(uint8_t id) const;

    static const char *thermoModeLabel_(ThermoController::Mode mode);

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
        out.reserve(labels.size() * 32 + 64);
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
        if (strcmp(menu.id, "tanks") == 0)
        {
            std::vector<String> labels;
            self->buildTankLabels_(labels);
            return buildKeyboardMarkup_(labels);
        }
        if (strcmp(menu.id, "septic") == 0)
            return self->septicControlMarkup_();
        if (strcmp(menu.id, "security") == 0)
        {
            return securityControlMarkup_();
        }
        if (strcmp(menu.id, "device") == 0)
        {
            std::vector<String> labels;
            labels.reserve(7);
            if (self->isAdminChat_(chat_id))
                labels.push_back(F("Админка"));
            if (self->_sockets && self->_sockets->controllerEnabled())
                labels.push_back(F("Розетки"));
            if (self->_meteo && self->_meteo->controllerEnabled())
                labels.push_back(F("Метео"));
            if (self->_thermo && self->_thermo->controllerEnabled())
                labels.push_back(F("Термо"));
            if (self->_tanks && self->_tanks->controllerEnabled())
                labels.push_back(F("Баки"));
            if (self->_septic && self->_septic->controllerEnabled())
                labels.push_back(F("Септик"));
            if (self->_security && self->_security->controllerEnabled())
                labels.push_back(F("Охрана"));
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

    void sendSocketMenu_(int64_t chat_id);

    void sendMeteoMenu_(int64_t chat_id);

    void sendThermoMenu_(int64_t chat_id);

    void sendTanksMenu_(int64_t chat_id);

    void sendSepticMenu_(int64_t chat_id);

    void sendSecurityMenu_(int64_t chat_id);

    void sendThermoDevice_(int64_t chat_id, uint8_t id);

    void sendTankDevice_(int64_t chat_id, uint8_t id);

    static String thermoControlMarkup_();

    static String tankControlMarkup_();

    String septicControlMarkup_() const;

    static String securityControlMarkup_();

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

    bool handleSocketToggleSelection_(const TelegramClient::Update &u);

    bool handleMeteoSelection_(const TelegramClient::Update &u);

    bool handleThermoAction_(const TelegramClient::Update &u);

    bool handleTankAction_(const TelegramClient::Update &u);

    bool handleThermoSelection_(const TelegramClient::Update &u);

    bool handleTankSelection_(const TelegramClient::Update &u);

    bool handleSepticSelection_(const TelegramClient::Update &u);

    bool handleSecuritySelection_(const TelegramClient::Update &u);

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

#include "core/network/telegram/menu/telegram_menu_thermo.hpp"
#include "core/network/telegram/menu/telegram_menu_sockets.hpp"
#include "core/network/telegram/menu/telegram_menu_meteo.hpp"
#include "core/network/telegram/menu/telegram_menu_tanks.hpp"
#include "core/network/telegram/menu/telegram_menu_septic.hpp"
#include "core/network/telegram/menu/telegram_menu_security.hpp"

inline bool TelegramMenu::cmdSockets_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
{
    return TelegramMenuSockets::cmdSockets_(bot, u, reply);
}

inline bool TelegramMenu::cmdMeteo_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
{
    return TelegramMenuMeteo::cmdMeteo_(bot, u, reply);
}

inline bool TelegramMenu::cmdThermo_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
{
    return TelegramMenuThermo::cmdThermo_(bot, u, reply);
}

inline bool TelegramMenu::cmdTanks_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
{
    return TelegramMenuTanks::cmdTanks_(bot, u, reply);
}

inline bool TelegramMenu::cmdSeptic_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
{
    return TelegramMenuSeptic::cmdSeptic_(bot, u, reply);
}

inline bool TelegramMenu::cmdSepticStatus_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
{
    return TelegramMenuSeptic::cmdSepticStatus_(bot, u, reply);
}

inline bool TelegramMenu::cmdSepticList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
{
    return TelegramMenuSeptic::cmdSepticList_(bot, u, reply);
}

inline bool TelegramMenu::cmdSepticMonitor_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
{
    return TelegramMenuSeptic::cmdSepticMonitor_(bot, u, reply);
}

inline bool TelegramMenu::cmdSecurity_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
{
    return TelegramMenuSecurity::cmdSecurity_(bot, u, reply);
}

inline bool TelegramMenu::cmdSecurityStatus_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
{
    return TelegramMenuSecurity::cmdSecurityStatus_(bot, u, reply);
}

inline bool TelegramMenu::cmdSecurityList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
{
    return TelegramMenuSecurity::cmdSecurityList_(bot, u, reply);
}

inline bool TelegramMenu::cmdSecurityArm_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
{
    return TelegramMenuSecurity::cmdSecurityArm_(bot, u, reply);
}

inline bool TelegramMenu::cmdSecurityDisarm_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
{
    return TelegramMenuSecurity::cmdSecurityDisarm_(bot, u, reply);
}

inline bool TelegramMenu::cmdSecuritySilent_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
{
    return TelegramMenuSecurity::cmdSecuritySilent_(bot, u, reply);
}

inline bool TelegramMenu::cmdSocketList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
{
    return TelegramMenuSockets::cmdSocketList_(bot, u, reply);
}

inline bool TelegramMenu::cmdSocketOn_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
{
    return TelegramMenuSockets::cmdSocketOn_(bot, u, reply);
}

inline bool TelegramMenu::cmdSocketOff_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
{
    return TelegramMenuSockets::cmdSocketOff_(bot, u, reply);
}

inline bool TelegramMenu::cmdSocketToggle_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
{
    return TelegramMenuSockets::cmdSocketToggle_(bot, u, reply);
}

inline bool TelegramMenu::cmdMeteoList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
{
    return TelegramMenuMeteo::cmdMeteoList_(bot, u, reply);
}

inline bool TelegramMenu::cmdMeteoShow_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
{
    return TelegramMenuMeteo::cmdMeteoShow_(bot, u, reply);
}

inline bool TelegramMenu::cmdThermoList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
{
    return TelegramMenuThermo::cmdThermoList_(bot, u, reply);
}

inline bool TelegramMenu::cmdThermoShow_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
{
    return TelegramMenuThermo::cmdThermoShow_(bot, u, reply);
}

inline bool TelegramMenu::cmdTanksList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
{
    return TelegramMenuTanks::cmdTanksList_(bot, u, reply);
}

inline bool TelegramMenu::cmdTanksShow_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
{
    return TelegramMenuTanks::cmdTanksShow_(bot, u, reply);
}

inline bool TelegramMenu::startSocketAction_(TelegramBot &bot, const TelegramClient::Update &u, String &reply, uint8_t action)
{
    if (!_self)
        return false;
    return TelegramMenuSockets::startSocketAction_(*_self, bot, u, reply, action);
}

inline bool TelegramMenu::parseSocketIdFromText_(const String &text, uint8_t &out)
{
    return TelegramMenuSockets::parseSocketIdFromText_(text, out);
}

inline bool TelegramMenu::parseSocketId_(const String &text, uint8_t &out)
{
    return TelegramMenuSockets::parseSocketId_(text, out);
}

inline bool TelegramMenu::parseSocketLabel_(const String &text, uint8_t &out)
{
    return TelegramMenuSockets::parseSocketLabel_(text, out);
}

inline bool TelegramMenu::parseMeteoIdFromText_(const String &text, uint8_t &out)
{
    return TelegramMenuMeteo::parseMeteoIdFromText_(text, out);
}

inline bool TelegramMenu::parseMeteoId_(const String &text, uint8_t &out)
{
    return TelegramMenuMeteo::parseMeteoId_(text, out);
}

inline bool TelegramMenu::parseMeteoLabel_(const String &text, uint8_t &out)
{
    return TelegramMenuMeteo::parseMeteoLabel_(text, out);
}

inline bool TelegramMenu::parseThermoIdFromText_(const String &text, uint8_t &out)
{
    return TelegramMenuThermo::parseThermoIdFromText_(text, out);
}

inline bool TelegramMenu::parseThermoId_(const String &text, uint8_t &out)
{
    return TelegramMenuThermo::parseThermoId_(text, out);
}

inline bool TelegramMenu::parseThermoLabel_(const String &text, uint8_t &out)
{
    return TelegramMenuThermo::parseThermoLabel_(text, out);
}

inline bool TelegramMenu::parseTankIdFromText_(const String &text, uint8_t &out)
{
    return TelegramMenuTanks::parseTankIdFromText_(text, out);
}

inline bool TelegramMenu::parseTankId_(const String &text, uint8_t &out)
{
    return TelegramMenuTanks::parseTankId_(text, out);
}

inline bool TelegramMenu::parseTankLabel_(const String &text, uint8_t &out)
{
    return TelegramMenuTanks::parseTankLabel_(text, out);
}

inline bool TelegramMenu::parseSepticId_(const String &text, uint8_t &out)
{
    return TelegramMenuSeptic::parseSepticId_(text, out);
}

inline bool TelegramMenu::parseSecurityId_(const String &text, uint8_t &out)
{
    return TelegramMenuSecurity::parseSecurityId_(text, out);
}

inline void TelegramMenu::buildSocketLabels_(std::vector<String> &out) const
{
    TelegramMenuSockets::buildSocketLabels_(*const_cast<TelegramMenu *>(this), out);
}

inline void TelegramMenu::buildMeteoLabels_(std::vector<String> &out) const
{
    TelegramMenuMeteo::buildMeteoLabels_(*const_cast<TelegramMenu *>(this), out);
}

inline void TelegramMenu::buildThermoLabels_(std::vector<String> &out) const
{
    TelegramMenuThermo::buildThermoLabels_(*const_cast<TelegramMenu *>(this), out);
}

inline void TelegramMenu::buildTankLabels_(std::vector<String> &out) const
{
    TelegramMenuTanks::buildTankLabels_(*const_cast<TelegramMenu *>(this), out);
}

inline String TelegramMenu::socketListTextHtml_() const
{
    return TelegramMenuSockets::socketListTextHtml_(*const_cast<TelegramMenu *>(this));
}

inline String TelegramMenu::meteoListTextHtml_() const
{
    return TelegramMenuMeteo::meteoListTextHtml_(*const_cast<TelegramMenu *>(this));
}

inline String TelegramMenu::meteoSensorTextHtml_(uint8_t id) const
{
    return TelegramMenuMeteo::meteoSensorTextHtml_(*const_cast<TelegramMenu *>(this), id);
}

inline String TelegramMenu::meteoHistoryTextHtml_(uint8_t id) const
{
    return TelegramMenuMeteo::meteoHistoryTextHtml_(*const_cast<TelegramMenu *>(this), id);
}

inline uint8_t TelegramMenu::scaleBars_(float v, float max_v)
{
    return TelegramMenuMeteo::scaleBars_(v, max_v);
}

inline String TelegramMenu::barString_(uint8_t bars)
{
    return TelegramMenuMeteo::barString_(bars);
}

inline String TelegramMenu::thermoListTextHtml_() const
{
    return TelegramMenuThermo::thermoListTextHtml_(*const_cast<TelegramMenu *>(this));
}

inline String TelegramMenu::thermoDeviceTextHtml_(uint8_t id) const
{
    return TelegramMenuThermo::thermoDeviceTextHtml_(*const_cast<TelegramMenu *>(this), id);
}

inline const char *TelegramMenu::thermoModeLabel_(ThermoController::Mode mode)
{
    return TelegramMenuThermo::thermoModeLabel_(mode);
}

inline const char *TelegramMenu::tankLevelLabel_(const TankController::TankState &st)
{
    return TelegramMenuTanks::tankLevelLabel_(st);
}

inline String TelegramMenu::tankListTextHtml_() const
{
    return TelegramMenuTanks::tankListTextHtml_(*const_cast<TelegramMenu *>(this));
}

inline String TelegramMenu::tankDeviceTextHtml_(uint8_t id) const
{
    return TelegramMenuTanks::tankDeviceTextHtml_(*const_cast<TelegramMenu *>(this), id);
}

inline String TelegramMenu::septicStatusText_() const
{
    return TelegramMenuSeptic::septicStatusText_(*const_cast<TelegramMenu *>(this));
}

inline String TelegramMenu::septicListTextHtml_() const
{
    return TelegramMenuSeptic::septicListTextHtml_(*const_cast<TelegramMenu *>(this));
}

inline String TelegramMenu::securityStatusText_() const
{
    return TelegramMenuSecurity::securityStatusText_(*const_cast<TelegramMenu *>(this));
}

inline String TelegramMenu::securityListTextHtml_() const
{
    return TelegramMenuSecurity::securityListTextHtml_(*const_cast<TelegramMenu *>(this));
}

inline void TelegramMenu::sendSocketMenu_(int64_t chat_id)
{
    TelegramMenuSockets::sendSocketMenu_(*this, chat_id);
}

inline void TelegramMenu::sendMeteoMenu_(int64_t chat_id)
{
    TelegramMenuMeteo::sendMeteoMenu_(*this, chat_id);
}

inline void TelegramMenu::sendThermoMenu_(int64_t chat_id)
{
    TelegramMenuThermo::sendThermoMenu_(*this, chat_id);
}

inline void TelegramMenu::sendTanksMenu_(int64_t chat_id)
{
    TelegramMenuTanks::sendTanksMenu_(*this, chat_id);
}

inline void TelegramMenu::sendSepticMenu_(int64_t chat_id)
{
    TelegramMenuSeptic::sendSepticMenu_(*this, chat_id);
}

inline void TelegramMenu::sendSecurityMenu_(int64_t chat_id)
{
    TelegramMenuSecurity::sendSecurityMenu_(*this, chat_id);
}

inline void TelegramMenu::sendThermoDevice_(int64_t chat_id, uint8_t id)
{
    TelegramMenuThermo::sendThermoDevice_(*this, chat_id, id);
}

inline void TelegramMenu::sendTankDevice_(int64_t chat_id, uint8_t id)
{
    TelegramMenuTanks::sendTankDevice_(*this, chat_id, id);
}

inline String TelegramMenu::thermoControlMarkup_()
{
    return TelegramMenuThermo::thermoControlMarkup_();
}

inline String TelegramMenu::tankControlMarkup_()
{
    return TelegramMenuTanks::tankControlMarkup_();
}

inline String TelegramMenu::septicControlMarkup_() const
{
    return TelegramMenuSeptic::septicControlMarkup_(*const_cast<TelegramMenu *>(this));
}

inline String TelegramMenu::securityControlMarkup_()
{
    return TelegramMenuSecurity::securityControlMarkup_();
}

inline bool TelegramMenu::handleSocketToggleSelection_(const TelegramClient::Update &u)
{
    return TelegramMenuSockets::handleSocketToggleSelection_(*this, u);
}

inline bool TelegramMenu::handleMeteoSelection_(const TelegramClient::Update &u)
{
    return TelegramMenuMeteo::handleMeteoSelection_(*this, u);
}

inline bool TelegramMenu::handleThermoAction_(const TelegramClient::Update &u)
{
    return TelegramMenuThermo::handleThermoAction_(*this, u);
}

inline bool TelegramMenu::handleTankAction_(const TelegramClient::Update &u)
{
    return TelegramMenuTanks::handleTankAction_(*this, u);
}

inline bool TelegramMenu::handleThermoSelection_(const TelegramClient::Update &u)
{
    return TelegramMenuThermo::handleThermoSelection_(*this, u);
}

inline bool TelegramMenu::handleTankSelection_(const TelegramClient::Update &u)
{
    return TelegramMenuTanks::handleTankSelection_(*this, u);
}

inline bool TelegramMenu::handleSepticSelection_(const TelegramClient::Update &u)
{
    return TelegramMenuSeptic::handleSepticSelection_(*this, u);
}

inline bool TelegramMenu::handleSecuritySelection_(const TelegramClient::Update &u)
{
    return TelegramMenuSecurity::handleSecuritySelection_(*this, u);
}



