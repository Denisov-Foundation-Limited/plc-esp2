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
#include <array>
#include <string.h>
#include <vector>
#include <stdlib.h>
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
#include "controllers/avr_controller.hpp"
#include "controllers/leak_controller.hpp"
#include "controllers/ring_controller.hpp"
#include "controllers/watering_controller.hpp"
#include "utils/meteo_history.hpp"
#include "utils/configs.hpp"
#include "utils/configs_manager_iface.hpp"
#include "utils/logger.hpp"
#include "utils/users_registry.hpp"
#include "core/network/stack/stack_master.hpp"
#include "core/network/stack/stack_cache.hpp"
#include "core/rules_controller.hpp"

class TelegramMenuThermo;
class TelegramMenuSockets;
class TelegramMenuMeteo;
class TelegramMenuTanks;
class TelegramMenuSeptic;
class TelegramMenuSecurity;
class TelegramMenuAvr;
class TelegramMenuLeak;
class TelegramMenuRing;
class TelegramMenuWatering;

class TelegramMenu : public TelegramAllowedUsersProvider
{
public:
    TelegramMenu(PlcControl &plc, WifiManager &wifi, RTC &rtc, TelegramBot &bot,
                 Configs &configs, Logger &logs, UsersRegistry &users)
        : _plc(plc), _wifi(wifi), _rtc(rtc), _bot(&bot), _configs(configs), _logs(&logs), _users(&users)
    {
    }

    void begin()
    {
        _self = this;
        if (!_bot)
        {
            if (_logs)
                _logs->warn(F("TGBOT"), F("Menu init skipped, bot missing"));
            return;
        }
        if (_logs)
            _logs->info(F("TGBOT"), F("Menu init"));
        _bot->setMenus(kMenus.data(), kMenus.size(), "root");
        _bot->setCommands(kCommands.data(), kCommands.size());
        _bot->setTextHandler(&TelegramMenu::onText_, this);
        _bot->setMenuPrefixProvider(&TelegramMenu::menuPrefix_, this);
        _bot->setMenuMarkupProvider(&TelegramMenu::menuMarkup_, this);
    }

    void setAdminPassword(const String &password) { _admin_password = password; }
    const String &adminPassword() const { return _admin_password; }
    void setConfigsManager(ConfigsManagerIface &mgr) { _configs_manager = &mgr; }
    void setStackMaster(StackMaster &master) { _stack_master = &master; }
    void setStackCache(StackCache &cache) { _stack_cache = &cache; }
    void setSockets(SocketController &sockets) { _sockets = &sockets; }
    void setMeteo(MeteoController &meteo) { _meteo = &meteo; }
    void setThermo(ThermoController &thermo) { _thermo = &thermo; }
    void setTanks(TankController &tanks) { _tanks = &tanks; }
    void setSeptic(SepticController &septic) { _septic = &septic; }
    void setSecurity(SecurityController &security) { _security = &security; }
    void setAvr(AvrController &avr) { _avr = &avr; }
    void setLeak(LeakController &leak) { _leak = &leak; }
    void setRing(RingController &ring) { _ring = &ring; }
    void setWatering(WateringController &watering) { _watering = &watering; }
    void setRules(RulesController &rules) { _rules = &rules; }

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
        if (!_users)
            return;
        for (size_t i = 0; i < _users->size(); ++i)
        {
            auto &dst = _users->user(i);
            dst.tg_username = "";
            dst.tg_chat_id = 0;
            dst.tg_admin = false;
            dst.tg_notify = false;
        }
        size_t next_slot = 0;
        for (size_t i = 0; i < users.size(); ++i)
        {
            if (next_slot >= _users->size())
                break;
            AllowedUser u = users[i];
            u.username = normalizeUsername_(u.username);
            if (u.username.length() == 0 && u.chat_id == 0)
                continue;
            if ((u.username.length() && hasAllowedUsername_(u.username)) ||
                (u.chat_id != 0 && hasAllowedUserChatId_(u.chat_id)))
                continue;
            auto &dst = _users->user(next_slot++);
            dst.enabled = u.enabled;
            dst.tg_username = u.username;
            dst.tg_chat_id = u.chat_id;
            dst.tg_admin = u.is_admin;
            dst.tg_notify = u.is_notify;
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
        if (!_users)
            return AllowResult::Invalid;
        AllowedUser u = user;
        u.username = normalizeUsername_(u.username);
        if (u.username.length() == 0 && u.chat_id == 0)
            return AllowResult::Invalid;
        if ((u.username.length() && hasAllowedUsername_(u.username)) ||
            (u.chat_id != 0 && hasAllowedUserChatId_(u.chat_id)))
            return AllowResult::Exists;
        for (size_t i = 0; i < _users->size(); ++i)
        {
            auto &dst = _users->user(i);
            if (dst.tg_username.length() || dst.tg_chat_id != 0)
                continue;
            dst.enabled = u.enabled;
            dst.tg_username = u.username;
            dst.tg_chat_id = u.chat_id;
            dst.tg_admin = u.is_admin;
            dst.tg_notify = u.is_notify;
            return AllowResult::Ok;
        }
        return AllowResult::Full;
    }

    bool removeAllowedUser(const String &user)
    {
        if (!_users)
            return false;
        String u = normalizeUsername_(user);
        if (u.length() == 0)
            return false;
        for (size_t i = 0; i < _users->size(); ++i)
        {
            auto &dst = _users->user(i);
            if (dst.tg_username == u)
            {
                dst.tg_username = "";
                dst.tg_chat_id = 0;
                dst.tg_admin = false;
                dst.tg_notify = false;
                return true;
            }
        }
        return false;
    }

    void clearAllowedUsers()
    {
        if (!_users)
            return;
        for (size_t i = 0; i < _users->size(); ++i)
        {
            auto &dst = _users->user(i);
            dst.tg_username = "";
            dst.tg_chat_id = 0;
            dst.tg_admin = false;
            dst.tg_notify = false;
        }
    }

    TelegramAllowedUsersView allowedUsers() const override
    {
        rebuildAllowedUsersCache_();
        return TelegramAllowedUsersView{_allowed_users_cache.data(), _allowed_users_cache_count};
    }

    static constexpr size_t kMaxAllowedUsers = UsersRegistry::kMaxUsers;

private:
    friend class TelegramMenuThermo;
    friend class TelegramMenuSockets;
    friend class TelegramMenuMeteo;
    friend class TelegramMenuTanks;
    friend class TelegramMenuSeptic;
    friend class TelegramMenuSecurity;
    friend class TelegramMenuAvr;
    friend class TelegramMenuLeak;
    friend class TelegramMenuRing;
    friend class TelegramMenuWatering;
    static inline TelegramMenu *_self = nullptr;

    PlcControl &_plc;
    WifiManager &_wifi;
    RTC &_rtc;
    String _admin_password;
    Configs &_configs;
    ConfigsManagerIface *_configs_manager = nullptr;
    UsersRegistry *_users = nullptr;
    mutable std::array<AllowedUser, kMaxAllowedUsers> _allowed_users_cache{};
    mutable size_t _allowed_users_cache_count = 0;

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
        bool awaiting_leak = false;
        bool awaiting_watering = false;
        uint8_t fail_count = 0;
        uint32_t lock_until_ms = 0;
        bool selected_local = true;
        uint32_t selected_node_id = 0;
        uint8_t socket_action = 0;
        uint8_t selected_thermo_id = 0;
        uint8_t selected_tank_id = 0;
        uint8_t selected_leak_id = 0;
        uint8_t selected_watering_id = 0;
    };

    static constexpr size_t kMaxAuth = 16;
    std::array<ChatAuth, kMaxAuth> _auth{};
    size_t _auth_count = 0;
    static constexpr size_t kMaxConfigBytes = 8192;
    static constexpr size_t kConfigDocCapacity = 12288;
    DynamicJsonDocument _cfg_doc{kConfigDocCapacity};
    static constexpr uint32_t kHistoryMagic = 0x4D544831u; // "MTH1"
    static constexpr float kThermoTargetStep = 1.0f;
    static constexpr float kThermoHystStep = 0.5f;
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
            bot.enterMenu(u.chat_id, "admin");
            return true;
        }

        if (st && st->authorized)
        {
            bot.enterMenu(u.chat_id, "admin");
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
        (void)reply;
        if (!_self)
            return false;
        const float temp = _self->_plc.boardTemp();
        const bool fan = _self->_plc.fanStatus();
        char buf[96] = {};
        snprintf(buf, sizeof(buf), "%.1f", temp);
        String out = F("Статус ПЛК:\n  temp_c: <b>");
        out += buf;
        out += F("</b>\n  fan: <b>");
        out += fan ? F("вкл") : F("выкл");
        out += F("</b>");
        bot.sendText(u.chat_id, out, "", "HTML");
        return true;
    }

    static bool cmdWifi_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        (void)reply;
        if (!_self)
            return false;
        String out = F("Wi-Fi:\n  режим: <b>");
        out += _self->_wifi.ap() ? F("AP") : F("STA");
        out += F("</b>\n  ssid: <b>");
        out += _self->escapeHtml_(_self->_wifi.ssid());
        out += F("</b>\n  ap_ssid: <b>");
        out += _self->escapeHtml_(_self->_wifi.apSsid());
        out += F("</b>");
        bot.sendText(u.chat_id, out, "", "HTML");
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
        size_t ctrl_count = 0;
        for (size_t i = 0; i < count; ++i)
            if (_self->_stack_master->nodeIsControllerAt(i))
                ++ctrl_count;
        if (ctrl_count == 0)
        {
            reply = "Контроллеры: нет активных";
            return true;
        }
        String out = F("Контроллеры:");
        for (size_t i = 0; i < count; ++i)
        {
            if (!_self->_stack_master->nodeIsControllerAt(i))
                continue;
            const uint32_t node_id = _self->_stack_master->nodeIdAt(i);
            String name = _self->_stack_master->nodeNameAt(i);
            if (name.length() == 0)
                name = fallbackNodeName_(node_id);
            const String ip = _self->_stack_master->nodeIpAt(i);
            out += F("\n  <b>");
            out += _self->escapeHtml_(name);
            out += F("</b> (id=<b>");
            out += String(node_id);
            out += F("</b>");
            if (ip.length())
            {
                out += F(", ip=<b>");
                out += _self->escapeHtml_(ip);
                out += F("</b>");
            }
            out += F(")");
        }
        bot.sendText(u.chat_id, out, "", "HTML");
        return true;
    }

    static bool cmdSockets_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdLights_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdMeteo_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdThermo_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdTanks_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdSeptic_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdSecurity_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdAvr_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdAvrStatus_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdLeak_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdLeakList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdLeakShow_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdLeakAck_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdRing_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdRingOn_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdRingOff_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdWatering_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdWateringList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdWateringShow_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

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
        const auto users = _self->allowedUsers();
        if (users.size == 0)
        {
            reply = "Список разрешенных пользователей пуст.";
            return true;
        }
        String out = F("Разрешенные пользователи:");
        for (size_t i = 0; i < users.size; ++i)
        {
            const auto &user = users[i];
            out += F("\n  <b>");
            out += String((unsigned)(i + 1));
            out += F("</b> ");
            out += user.username.length() ? _self->escapeHtml_(user.username) : String("-");
            if (user.chat_id)
            {
                out += F(" chat=<b>");
                out += String((long long)user.chat_id);
                out += F("</b>");
            }
            out += F(" admin=<b>");
            out += user.is_admin ? "1" : "0";
            out += F("</b> notify=<b>");
            out += user.is_notify ? "1" : "0";
            out += F("</b> enabled=<b>");
            out += user.enabled ? "1" : "0";
            out += F("</b>");
        }
        bot.sendText(u.chat_id, out, "", "HTML");
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
            reply = "Лимит 20";
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
        (void)reply;
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
        String out = F("RTC:\n  дата: <b>");
        out += date_buf;
        out += F("</b>\n  время: <b>");
        out += time_buf;
        out += F("</b>\n  день недели: <b>");
        out += String((unsigned)dt.day_of_week);
        out += F("</b>");
        bot.sendText(u.chat_id, out, "", "HTML");
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
            if (!parseSocketId_(u.text, id))
            {
                if (self->_bot)
                    self->_bot->sendText(u.chat_id, F("Неверный ID розетки"));
                st->awaiting_socket = false;
                st->socket_action = 0;
                return true;
            }
            if (!self->_sockets)
            {
                self->_bot->sendText(u.chat_id, F("Розетки недоступны"));
                st->awaiting_socket = false;
                st->socket_action = 0;
                return true;
            }
            bool ok = false;
            if (self->isLocalSelected_(u.chat_id))
            {
                if (st->socket_action == 1)
                    ok = self->_sockets->setRelayById(id, true);
                else if (st->socket_action == 2)
                    ok = self->_sockets->setRelayById(id, false);
                else if (st->socket_action == 3)
                    ok = self->_sockets->toggleRelayById(id);
            }
            else
            {
                const uint32_t node_id = self->selectedNodeId_(u.chat_id);
                if (node_id != 0 && self->_stack_master)
                {
                    DynamicJsonDocument doc(256);
                    doc["feature"] = (uint8_t)StackFeature::Sockets;
                    doc["action"] = "set";
                    JsonObject params = doc["params"].to<JsonObject>();
                    JsonArray items = params["items"].to<JsonArray>();
                    JsonObject item = items.add<JsonObject>();
                    item["id"] = (unsigned)id;
                    if (st->socket_action == 3)
                        item["toggle"] = true;
                    else
                        item["state"] = (st->socket_action == 1);
                    char payload[256] = {};
                    const size_t n = serializeJson(doc, payload, sizeof(payload));
                    if (n > 0)
                        ok = self->_stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                                                         reinterpret_cast<const uint8_t *>(payload), n);
                    if (ok && self->_stack_cache)
                        self->_stack_cache->requestSockets(node_id);
                }
            }
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
        if (self->handleLeakAction_(u))
            return true;
        if (self->handleWateringAction_(u))
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
        if (self->handleAvrSelection_(u))
            return true;
        if (self->handleLeakSelection_(u))
            return true;
        if (self->handleRingSelection_(u))
            return true;
        if (self->handleWateringSelection_(u))
            return true;
        if (self->handleRootDeviceSelection_(u))
            return true;
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
        for (size_t i = 0; i < _auth_count; ++i)
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
        if (_auth_count >= kMaxAuth)
            return nullptr;
        ChatAuth ns;
        ns.chat_id = chat_id;
        _auth[_auth_count++] = ns;
        return &_auth[_auth_count - 1];
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
        st->awaiting_leak = false;
        st->selected_leak_id = 0;
        st->awaiting_watering = false;
        st->selected_watering_id = 0;
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
        st->awaiting_leak = false;
        st->selected_leak_id = 0;
        st->awaiting_watering = false;
        st->selected_watering_id = 0;
    }

    TelegramBot *_bot = nullptr;
    Logger *_logs = nullptr;
    StackMaster *_stack_master = nullptr;
    StackCache *_stack_cache = nullptr;
    SocketController *_sockets = nullptr;
    MeteoController *_meteo = nullptr;
    ThermoController *_thermo = nullptr;
    TankController *_tanks = nullptr;
    SepticController *_septic = nullptr;
    SecurityController *_security = nullptr;
    AvrController *_avr = nullptr;
    LeakController *_leak = nullptr;
    RingController *_ring = nullptr;
    WateringController *_watering = nullptr;
    RulesController *_rules = nullptr;

    static inline const std::array<TelegramBot::MenuItem, 0> kRootItems = {};

    static inline const std::array<TelegramBot::MenuItem, 13> kDeviceItems = {{
        { "Админка", "Админка", nullptr, nullptr },
        { "Розетки", "/sockets", nullptr, nullptr },
        { "Свет", "/lights", nullptr, nullptr },
        { "Метео", "/meteo", nullptr, nullptr },
        { "Термо", "/thermo", nullptr, nullptr },
        { "Баки", "/tanks", nullptr, nullptr },
        { "Септик", "/septic", nullptr, nullptr },
        { "Охрана", "/security", nullptr, nullptr },
        { "АВР", "/avr", nullptr, nullptr },
        { "Leak", "/leak", nullptr, nullptr },
        { "Звонок", "/ring", nullptr, nullptr },
        { "Полив", "/watering", nullptr, nullptr },
        { "Назад", "/back", nullptr, nullptr },
    }};

    static inline const std::array<TelegramBot::MenuItem, 0> kSocketsItems = {};
    static inline const std::array<TelegramBot::MenuItem, 0> kMeteoItems = {};
    static inline const std::array<TelegramBot::MenuItem, 0> kThermoItems = {};
    static inline const std::array<TelegramBot::MenuItem, 0> kTanksItems = {};
    static inline const std::array<TelegramBot::MenuItem, 0> kSepticItems = {};
    static inline const std::array<TelegramBot::MenuItem, 0> kSecurityItems = {};
    static inline const std::array<TelegramBot::MenuItem, 0> kAvrItems = {};
    static inline const std::array<TelegramBot::MenuItem, 0> kLeakItems = {};
    static inline const std::array<TelegramBot::MenuItem, 0> kRingItems = {};
    static inline const std::array<TelegramBot::MenuItem, 0> kWateringItems = {};

    static inline const std::array<TelegramBot::MenuItem, 6> kAdminItems = {{
        { "ПЛК", nullptr, "plc", nullptr },
        { "Часы", nullptr, "rtc", nullptr },
        { "Wi-Fi", nullptr, "wifi", nullptr },
        { "Настройки", nullptr, "settings", nullptr },
        { "Логи", "/logs", nullptr, nullptr },
        { "Назад", "/back", nullptr, nullptr },
    }};

    static inline const std::array<TelegramBot::MenuItem, 2> kPlcItems = {{
        { "Статус", "/status", nullptr, nullptr },
        { "Назад", "/back", nullptr, nullptr },
    }};

    static inline const std::array<TelegramBot::MenuItem, 2> kRtcItems = {{
        { "Время", "/time", nullptr, nullptr },
        { "Назад", "/back", nullptr, nullptr },
    }};

    static inline const std::array<TelegramBot::MenuItem, 2> kWifiItems = {{
        { "Состояние", "/wifi", nullptr, nullptr },
        { "Назад", "/back", nullptr, nullptr },
    }};

    static inline const std::array<TelegramBot::MenuItem, 6> kSettingsItems = {{
        { "Перезапуск Wi-Fi", "/wifi_restart", nullptr, nullptr },
        { "Перезапуск ПЛК", "/plc_restart", nullptr, nullptr },
        { "Wi-Fi AP Вкл", "/wifi_ap_on", nullptr, nullptr },
        { "Wi-Fi AP Выкл", "/wifi_ap_off", nullptr, nullptr },
        { "Startup-config", "/config_set", nullptr, nullptr },
        { "Назад", "/back", nullptr, nullptr },
    }};

    static inline const std::array<TelegramBot::Menu, 18> kMenus = {{
        { "root", "Выбор устройства", kRootItems.data(), kRootItems.size(), nullptr },
        { "device", "Меню устройства", kDeviceItems.data(), kDeviceItems.size(), "root" },
        { "sockets", "Розетки", kSocketsItems.data(), kSocketsItems.size(), "device" },
        { "lights", "Свет", kSocketsItems.data(), kSocketsItems.size(), "device" },
        { "meteo", "Метео", kMeteoItems.data(), kMeteoItems.size(), "device" },
        { "thermo", "Термо", kThermoItems.data(), kThermoItems.size(), "device" },
        { "tanks", "Баки", kTanksItems.data(), kTanksItems.size(), "device" },
        { "septic", "Септик", kSepticItems.data(), kSepticItems.size(), "device" },
        { "security", "Охрана", kSecurityItems.data(), kSecurityItems.size(), "device" },
        { "avr", "АВР", kAvrItems.data(), kAvrItems.size(), "device" },
        { "leak", "Leak", kLeakItems.data(), kLeakItems.size(), "device" },
        { "ring", "Звонок", kRingItems.data(), kRingItems.size(), "device" },
        { "watering", "Полив", kWateringItems.data(), kWateringItems.size(), "device" },
        { "admin", "Админка", kAdminItems.data(), kAdminItems.size(), "device" },
        { "plc", "ПЛК", kPlcItems.data(), kPlcItems.size(), "admin" },
        { "rtc", "Часы", kRtcItems.data(), kRtcItems.size(), "admin" },
        { "wifi", "Wi-Fi", kWifiItems.data(), kWifiItems.size(), "admin" },
        { "settings", "Настройки", kSettingsItems.data(), kSettingsItems.size(), "admin" },
    }};

    static inline const std::array<TelegramBot::Command, 53> kCommands = {{
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
        { "/lights", &TelegramMenu::cmdLights_ },
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
        { "/avr", &TelegramMenu::cmdAvr_ },
        { "/avr_status", &TelegramMenu::cmdAvrStatus_ },
        { "/leak", &TelegramMenu::cmdLeak_ },
        { "/leak_list", &TelegramMenu::cmdLeakList_ },
        { "/leak_show", &TelegramMenu::cmdLeakShow_ },
        { "/leak_ack", &TelegramMenu::cmdLeakAck_ },
        { "/ring", &TelegramMenu::cmdRing_ },
        { "/ring_on", &TelegramMenu::cmdRingOn_ },
        { "/ring_off", &TelegramMenu::cmdRingOff_ },
        { "/watering", &TelegramMenu::cmdWatering_ },
        { "/watering_list", &TelegramMenu::cmdWateringList_ },
        { "/watering_show", &TelegramMenu::cmdWateringShow_ },
    }};

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

    void rebuildAllowedUsersCache_() const
    {
        _allowed_users_cache_count = 0;
        if (!_users)
            return;
        for (size_t i = 0; i < _users->size(); ++i)
        {
            const auto &src = _users->user(i);
            if (src.tg_username.length() == 0 && src.tg_chat_id == 0)
                continue;
            if (_allowed_users_cache_count >= _allowed_users_cache.size())
                break;
            AllowedUser &dst = _allowed_users_cache[_allowed_users_cache_count++];
            dst.username = src.tg_username;
            dst.chat_id = src.tg_chat_id;
            dst.is_admin = src.tg_admin;
            dst.is_notify = src.tg_notify;
            dst.enabled = src.enabled;
        }
    }

    bool hasAllowedUsername_(const String &user) const
    {
        if (!_users)
            return false;
        for (size_t i = 0; i < _users->size(); ++i)
        {
            const auto &u = _users->user(i);
            if (u.tg_username == user)
                return true;
        }
        return false;
    }

    bool hasAllowedUserChatId_(int64_t chat_id) const
    {
        if (!_users)
            return false;
        for (size_t i = 0; i < _users->size(); ++i)
        {
            const auto &u = _users->user(i);
            if (u.tg_chat_id != 0 && u.tg_chat_id == chat_id)
                return true;
        }
        return false;
    }

    bool isAllowedUser_(const TelegramClient::Update &u) const
    {
        const auto users = allowedUsers();
        if (users.size == 0)
            return true;
        size_t idx = 0;
        if (u.chat_id != 0 && findAllowedUserByChatId_(u.chat_id, idx))
            return users[idx].enabled;
        String user = normalizeUsername_(u.from);
        if (user.length() == 0)
            return false;
        if (findAllowedUserByName_(user, idx))
            return users[idx].enabled;
        return false;
    }

    bool isAdminChat_(int64_t chat_id) const
    {
        const auto users = allowedUsers();
        if (users.size == 0)
            return false;
        size_t idx = 0;
        if (chat_id != 0 && findAllowedUserByChatId_(chat_id, idx))
            return users[idx].enabled && users[idx].is_admin;
        const ChatAuth *st = findAuth_(chat_id);
        if (st && st->user_id.length() && findAllowedUserByName_(st->user_id, idx))
            return users[idx].enabled && users[idx].is_admin;
        return false;
    }

    bool findAllowedUserByName_(const String &name, size_t &out) const
    {
        const auto users = allowedUsers();
        for (size_t i = 0; i < users.size; ++i)
        {
            if (users[i].username == name)
            {
                out = i;
                return true;
            }
        }
        return false;
    }

    bool findAllowedUserByChatId_(int64_t chat_id, size_t &out) const
    {
        const auto users = allowedUsers();
        for (size_t i = 0; i < users.size; ++i)
        {
            if (users[i].chat_id != 0 && users[i].chat_id == chat_id)
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

    uint32_t selectedNodeId_(int64_t chat_id) const
    {
        const ChatAuth *st = findAuth_(chat_id);
        if (!st || st->selected_local)
            return 0;
        return st->selected_node_id;
    }

    void buildSocketLabels_(std::vector<String> &out) const;
    void buildSocketLabels_(std::vector<String> &out, bool lights_only) const;

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
            if (!_stack_master->nodeIsControllerAt(i))
                continue;
            const uint32_t node_id = _stack_master->nodeIdAt(i);
            if (node_id == 0)
                continue;
            DeviceEntry entry{};
            entry.local = false;
            entry.node_id = node_id;
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

    static String buildRootKeyboardMarkup_(const std::vector<DeviceEntry> &devices)
    {
        String out = F("{\"keyboard\":[[\"Я дома\",\"Собираюсь\",\"Ушёл\"]");
        out.reserve(devices.size() * 32 + 128);
        const size_t cols = 2;
        for (size_t i = 0; i < devices.size(); ++i)
        {
            if (i % cols == 0)
                out += F(",[");
            out += F("\"");
            out += escapeJson_(devices[i].label);
            out += F("\"");
            if ((i % cols) == cols - 1 || i + 1 == devices.size())
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
            prefix = F("Текущее устройство: <b>");
        else
            prefix = F("Устройство: <b>");
        prefix += self->escapeHtml_(self->selectedDeviceLabel_(chat_id));
        prefix += F("</b>");
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
            return buildRootKeyboardMarkup_(devices);
        }
        if (strcmp(menu.id, "sockets") == 0)
        {
            std::vector<String> labels;
            if (self->isLocalSelected_(chat_id))
            {
                self->buildSocketLabels_(labels, false);
            }
            else if (self->_stack_cache)
            {
                const uint32_t node_id = self->selectedNodeId_(chat_id);
                if (node_id != 0)
                {
                    const auto *cache = self->_stack_cache->socketsCache(node_id);
                    if (!cache || !cache->has_data)
                    {
                        self->_stack_cache->requestSockets(node_id);
                    }
                    else
                    {
                        for (size_t i = 0; i < cache->item_count; ++i)
                        {
                            const auto &it = cache->items[i];
                            if (!it.enabled)
                                continue;
                            String label = String((unsigned)it.id) + ": ";
                            label += it.name[0] ? String(it.name) : String("Розетка");
                            labels.push_back(label);
                        }
                    }
                }
                labels.push_back(F("Назад"));
            }
            if (labels.empty())
                labels.push_back(F("Назад"));
            return buildKeyboardMarkup_(labels);
        }
        if (strcmp(menu.id, "lights") == 0)
        {
            std::vector<String> labels;
            if (self->isLocalSelected_(chat_id))
            {
                self->buildSocketLabels_(labels, true);
            }
            else if (self->_stack_cache)
            {
                const uint32_t node_id = self->selectedNodeId_(chat_id);
                if (node_id != 0)
                {
                    const auto *cache = self->_stack_cache->lightsCache(node_id);
                    if (!cache || !cache->has_data)
                    {
                        self->_stack_cache->requestLights(node_id);
                    }
                    else
                    {
                        for (size_t i = 0; i < cache->item_count; ++i)
                        {
                            const auto &it = cache->items[i];
                            if (!it.enabled)
                                continue;
                            String label = String((unsigned)it.id) + ": ";
                            label += it.name[0] ? String(it.name) : String("Свет");
                            labels.push_back(label);
                        }
                    }
                }
                labels.push_back(F("Назад"));
            }
            if (labels.empty())
                labels.push_back(F("Назад"));
            return buildKeyboardMarkup_(labels);
        }
        if (strcmp(menu.id, "meteo") == 0)
        {
            std::vector<String> labels;
            if (self->isLocalSelected_(chat_id))
            {
                self->buildMeteoLabels_(labels);
            }
            else if (self->_stack_cache)
            {
                const uint32_t node_id = self->selectedNodeId_(chat_id);
                if (node_id != 0)
                {
                    const auto *cache = self->_stack_cache->meteoCache(node_id);
                    if (!cache || !cache->has_data)
                    {
                        self->_stack_cache->requestMeteo(node_id);
                    }
                    else
                    {
                        for (size_t i = 0; i < cache->item_count; ++i)
                        {
                            const auto &it = cache->items[i];
                            if (!it.enabled)
                                continue;
                            String label = String((unsigned)it.id) + ": ";
                            label += it.name[0] ? String(it.name) : String("Sensor");
                            labels.push_back(label);
                        }
                    }
                }
                labels.push_back(F("Назад"));
            }
            if (labels.empty())
                labels.push_back(F("Назад"));
            return buildKeyboardMarkup_(labels);
        }
        if (strcmp(menu.id, "thermo") == 0)
        {
            std::vector<String> labels;
            if (self->isLocalSelected_(chat_id))
            {
                self->buildThermoLabels_(labels);
            }
            else if (self->_stack_cache)
            {
                const uint32_t node_id = self->selectedNodeId_(chat_id);
                if (node_id != 0)
                {
                    const auto *cache = self->_stack_cache->thermoCache(node_id);
                    if (!cache || !cache->has_data)
                    {
                        self->_stack_cache->requestThermo(node_id);
                    }
                    else
                    {
                        for (size_t i = 0; i < cache->item_count; ++i)
                        {
                            const auto &it = cache->items[i];
                            if (!it.enabled)
                                continue;
                            String label = String((unsigned)it.id) + ": ";
                            label += it.name[0] ? String(it.name) : String("-");
                            labels.push_back(label);
                        }
                    }
                }
                labels.push_back(F("Назад"));
            }
            if (labels.empty())
                labels.push_back(F("Назад"));
            return buildKeyboardMarkup_(labels);
        }
        if (strcmp(menu.id, "tanks") == 0)
        {
            std::vector<String> labels;
            if (self->isLocalSelected_(chat_id))
            {
                self->buildTankLabels_(labels);
            }
            else if (self->_stack_cache)
            {
                const uint32_t node_id = self->selectedNodeId_(chat_id);
                if (node_id != 0)
                {
                    const auto *cache = self->_stack_cache->tanksCache(node_id);
                    if (!cache || !cache->has_data)
                    {
                        self->_stack_cache->requestTanks(node_id);
                    }
                    else
                    {
                        for (size_t i = 0; i < cache->item_count; ++i)
                        {
                            const auto &it = cache->items[i];
                            if (!it.enabled)
                                continue;
                            String label = String((unsigned)it.id) + ": ";
                            label += it.name[0] ? String(it.name) : String("Tank");
                            labels.push_back(label);
                        }
                    }
                }
                labels.push_back(F("Назад"));
            }
            if (labels.empty())
                labels.push_back(F("Назад"));
            return buildKeyboardMarkup_(labels);
        }
        if (strcmp(menu.id, "septic") == 0)
            return self->septicControlMarkup_(chat_id);
        if (strcmp(menu.id, "security") == 0)
        {
            return securityControlMarkup_();
        }
        if (strcmp(menu.id, "avr") == 0)
        {
            return self->avrControlMarkup_(chat_id);
        }
        if (strcmp(menu.id, "leak") == 0)
        {
            return self->leakControlMarkup_(chat_id);
        }
        if (strcmp(menu.id, "ring") == 0)
        {
            return self->ringControlMarkup_(chat_id);
        }
        if (strcmp(menu.id, "watering") == 0)
        {
            return self->wateringControlMarkup_(chat_id);
        }
        if (strcmp(menu.id, "device") == 0)
        {
            std::vector<String> labels;
            labels.reserve(13);
            if (self->isAdminChat_(chat_id))
                labels.push_back(F("Админка"));
            if (self->isLocalSelected_(chat_id))
            {
                if (self->_sockets && self->_sockets->controllerEnabled())
                    labels.push_back(F("Розетки"));
                if (self->_sockets && self->_sockets->controllerEnabled())
                    labels.push_back(F("Свет"));
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
                if (self->_avr && self->_avr->controllerEnabled())
                    labels.push_back(F("АВР"));
                if (self->_leak && self->_leak->controllerEnabled())
                    labels.push_back(F("Leak"));
                if (self->_ring && self->_ring->controllerEnabled())
                    labels.push_back(F("Звонок"));
                if (self->_watering && self->_watering->controllerEnabled())
                    labels.push_back(F("Полив"));
            }
            else
            {
                const ChatAuth *st = self->findAuth_(chat_id);
                const uint32_t node_id = st ? st->selected_node_id : 0;
                if (node_id != 0 && self->_stack_cache)
                {
                    bool sockets_enabled = false;
                    bool lights_enabled = false;
                    bool meteo_enabled = false;
                    bool thermo_enabled = false;
                    bool tanks_enabled = false;
                    bool septic_enabled = false;
                    bool security_enabled = false;
                    bool avr_enabled = false;
                    bool leak_enabled = false;
                    bool ring_enabled = true;
                    bool watering_enabled = false;

                    const auto *sc = self->_stack_cache->socketsCache(node_id);
                    if (!sc || !sc->has_data)
                        self->_stack_cache->requestSockets(node_id);
                    else
                        for (size_t i = 0; i < sc->item_count; ++i)
                            if (sc->items[i].enabled)
                            {
                                sockets_enabled = true;
                                break;
                            }

                    const auto *lc = self->_stack_cache->lightsCache(node_id);
                    if (!lc || !lc->has_data)
                        self->_stack_cache->requestLights(node_id);
                    else
                        for (size_t i = 0; i < lc->item_count; ++i)
                            if (lc->items[i].enabled)
                            {
                                lights_enabled = true;
                                break;
                            }

                    const auto *mc = self->_stack_cache->meteoCache(node_id);
                    if (!mc || !mc->has_data)
                        self->_stack_cache->requestMeteo(node_id);
                    else
                        for (size_t i = 0; i < mc->item_count; ++i)
                            if (mc->items[i].enabled)
                            {
                                meteo_enabled = true;
                                break;
                            }

                    const auto *tc = self->_stack_cache->thermoCache(node_id);
                    if (!tc || !tc->has_data)
                        self->_stack_cache->requestThermo(node_id);
                    else
                        for (size_t i = 0; i < tc->item_count; ++i)
                            if (tc->items[i].enabled)
                            {
                                thermo_enabled = true;
                                break;
                            }

                    const auto *tac = self->_stack_cache->tanksCache(node_id);
                    if (!tac || !tac->has_data)
                        self->_stack_cache->requestTanks(node_id);
                    else
                        for (size_t i = 0; i < tac->item_count; ++i)
                            if (tac->items[i].enabled)
                            {
                                tanks_enabled = true;
                                break;
                            }

                    const auto *sec = self->_stack_cache->septicCache(node_id);
                    if (!sec || !sec->has_data)
                        self->_stack_cache->requestSeptic(node_id);
                    else
                        for (size_t i = 0; i < sec->item_count; ++i)
                            if (sec->items[i].enabled)
                            {
                                septic_enabled = true;
                                break;
                            }

                    const auto *sg = self->_stack_cache->securityCache(node_id);
                    if (!sg || !sg->has_data)
                        self->_stack_cache->requestSecurity(node_id);
                    else
                        security_enabled = sg->enabled;

                    const auto *ac = self->_stack_cache->avrCache(node_id);
                    if (!ac || !ac->has_data)
                        self->_stack_cache->requestAvr(node_id);
                    else
                        avr_enabled = ac->enabled;

                    const auto *lk = self->_stack_cache->leakCache(node_id);
                    if (!lk || !lk->has_data)
                        self->_stack_cache->requestLeak(node_id);
                    else
                        for (size_t i = 0; i < lk->item_count; ++i)
                            if (lk->items[i].enabled)
                            {
                                leak_enabled = true;
                                break;
                            }

                    const auto *wc = self->_stack_cache->wateringCache(node_id);
                    if (!wc || !wc->has_data)
                        self->_stack_cache->requestWatering(node_id);
                    else
                        for (size_t i = 0; i < wc->item_count; ++i)
                            if (wc->items[i].enabled)
                            {
                                watering_enabled = true;
                                break;
                            }

                    if (sockets_enabled)
                        labels.push_back(F("Розетки"));
                    if (lights_enabled)
                        labels.push_back(F("Свет"));
                    if (meteo_enabled)
                        labels.push_back(F("Метео"));
                    if (thermo_enabled)
                        labels.push_back(F("Термо"));
                    if (tanks_enabled)
                        labels.push_back(F("Баки"));
                    if (septic_enabled)
                        labels.push_back(F("Септик"));
                    if (security_enabled)
                        labels.push_back(F("Охрана"));
                    if (avr_enabled)
                        labels.push_back(F("АВР"));
                    if (leak_enabled)
                        labels.push_back(F("Leak"));
                    if (ring_enabled)
                        labels.push_back(F("Звонок"));
                    if (watering_enabled)
                        labels.push_back(F("Полив"));
                }
            }
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

    void sendAvrMenu_(int64_t chat_id);

    void sendLeakMenu_(int64_t chat_id);

    void sendRingMenu_(int64_t chat_id);

    void sendWateringMenu_(int64_t chat_id);

    void sendThermoDevice_(int64_t chat_id, uint8_t id);

    void sendTankDevice_(int64_t chat_id, uint8_t id);

    static String thermoControlMarkup_();

    static String tankControlMarkup_();

    String septicControlMarkup_() const;
    String septicControlMarkup_(int64_t chat_id) const;
    String avrControlMarkup_(int64_t chat_id) const;
    String leakControlMarkup_(int64_t chat_id) const;
    String ringControlMarkup_(int64_t chat_id) const;
    String wateringControlMarkup_(int64_t chat_id) const;

    static String securityControlMarkup_();

    String ruleActor_(int64_t chat_id) const
    {
        const ChatAuth *st = findAuth_(chat_id);
        if (st && st->user_id.length())
            return st->user_id;
        return String("telegram");
    }

    static bool parseRuleId_(const String &value, uint8_t &out)
    {
        String s = value;
        s.trim();
        if (!s.length())
            return false;
        for (size_t i = 0; i < (size_t)s.length(); ++i)
        {
            const char c = s[i];
            if (c < '0' || c > '9')
                return false;
        }
        const int v = s.toInt();
        if (v <= 0 || v > 255)
            return false;
        out = (uint8_t)v;
        return true;
    }

    static bool parseRuleOnOff_(const String &value, bool &on)
    {
        String s = value;
        s.trim();
        s.toLowerCase();
        if (s == "on" || s == "1" || s == "true")
        {
            on = true;
            return true;
        }
        if (s == "off" || s == "0" || s == "false")
        {
            on = false;
            return true;
        }
        return false;
    }

    static String normalizeCondToken_(const String &value)
    {
        String s = value;
        s.trim();
        s.toLowerCase();
        if (s == "1" || s == "true")
            return "on";
        if (s == "0" || s == "false")
            return "off";
        return s;
    }

    static bool parseCondNumber_(const String &value, double &out)
    {
        String s = value;
        s.trim();
        if (!s.length())
            return false;
        char buf[32] = {};
        const size_t n = s.length() < (sizeof(buf) - 1) ? s.length() : (sizeof(buf) - 1);
        for (size_t i = 0; i < n; ++i)
            buf[i] = s[i];
        char *end = nullptr;
        const double v = strtod(buf, &end);
        if (!end || *end != '\0')
            return false;
        out = v;
        return true;
    }

    static bool evalCondOp_(const String &actual, const String &op_raw, const String &expected)
    {
        String op = op_raw;
        op.trim();
        if (!op.length())
            op = "==";
        if (op == "eq")
            op = "==";
        else if (op == "ne")
            op = "!=";
        else if (op == "gt")
            op = ">";
        else if (op == "lt")
            op = "<";
        else if (op == "ge")
            op = ">=";
        else if (op == "le")
            op = "<=";

        double av = 0.0;
        double ev = 0.0;
        const bool an = parseCondNumber_(actual, av);
        const bool en = parseCondNumber_(expected, ev);
        if (an && en)
        {
            if (op == "==")
                return av == ev;
            if (op == "!=")
                return av != ev;
            if (op == ">")
                return av > ev;
            if (op == "<")
                return av < ev;
            if (op == ">=")
                return av >= ev;
            if (op == "<=")
                return av <= ev;
            return false;
        }

        const String a = normalizeCondToken_(actual);
        const String e = normalizeCondToken_(expected);
        if (op == "==")
            return a == e;
        if (op == "!=")
            return a != e;
        return false;
    }

    bool tryReadSepticConditionValue_(uint8_t item_id, const String &param, String &out) const
    {
        if (!_septic || !_septic->controllerEnabled())
            return false;
        for (size_t i = 0; i < SepticController::kSepticCount; ++i)
        {
            const auto *cfg = _septic->configByIndex(i);
            const auto *st = _septic->stateByIndex(i);
            if (!cfg || !st || cfg->id != item_id)
                continue;
            if (param == "status")
            {
                if (st->alarm)
                    out = "alarm";
                else if (st->warning)
                    out = "warn";
                else
                    out = "ok";
                return true;
            }
            if (param == "warning")
            {
                out = st->warning ? "on" : "off";
                return true;
            }
            if (param == "alarm")
            {
                out = st->alarm ? "on" : "off";
                return true;
            }
            return false;
        }
        return false;
    }

    bool tryReadRuleConditionActual_(const RulesController::Rule &rule, String &out) const
    {
        String ctrl = rule.condition_controller;
        String param = rule.condition_parameter;
        ctrl.trim();
        ctrl.toLowerCase();
        param.trim();
        param.toLowerCase();
        const uint8_t item_id = rule.condition_item_id;
        const uint32_t node_id = rule.condition_node_id;

        if (node_id != 0)
        {
            if (!_stack_cache)
                return false;
            if (ctrl == "sockets")
            {
                if (item_id == 0)
                    return false;
                const auto *cache = _stack_cache->socketsCache(node_id);
                if (!cache || !cache->has_data)
                {
                    _stack_cache->requestSockets(node_id);
                    return false;
                }
                if (param != "relay_on")
                    return false;
                for (size_t i = 0; i < cache->item_count; ++i)
                {
                    const auto &it = cache->items[i];
                    if (it.id != item_id)
                        continue;
                    out = it.state ? "on" : "off";
                    return true;
                }
                return false;
            }
            if (ctrl == "lights")
            {
                if (item_id == 0)
                    return false;
                const auto *cache = _stack_cache->lightsCache(node_id);
                if (!cache || !cache->has_data)
                {
                    _stack_cache->requestLights(node_id);
                    return false;
                }
                if (param != "relay_on")
                    return false;
                for (size_t i = 0; i < cache->item_count; ++i)
                {
                    const auto &it = cache->items[i];
                    if (it.id != item_id)
                        continue;
                    out = it.state ? "on" : "off";
                    return true;
                }
                return false;
            }
            if (ctrl == "meteo")
            {
                if (item_id == 0)
                    return false;
                const auto *cache = _stack_cache->meteoCache(node_id);
                if (!cache || !cache->has_data)
                {
                    _stack_cache->requestMeteo(node_id);
                    return false;
                }
                for (size_t i = 0; i < cache->item_count; ++i)
                {
                    const auto &it = cache->items[i];
                    if (it.id != item_id)
                        continue;
                    if (param == "ok")
                    {
                        out = it.ok ? "on" : "off";
                        return true;
                    }
                    if (param == "temp_c" && it.has_temp)
                    {
                        out = String(it.temp_c, 1);
                        return true;
                    }
                    if (param == "humidity" && it.has_hum)
                    {
                        out = String(it.hum, 1);
                        return true;
                    }
                    return false;
                }
                return false;
            }
            if (ctrl == "security")
            {
                const auto *cache = _stack_cache->securityCache(node_id);
                if (!cache || !cache->has_data)
                {
                    _stack_cache->requestSecurity(node_id);
                    return false;
                }
                if (param == "armed")
                {
                    out = cache->armed ? "on" : "off";
                    return true;
                }
                if (param == "alarm_on")
                {
                    out = cache->alarm ? "on" : "off";
                    return true;
                }
                if (param == "detect" && item_id != 0)
                {
                    for (size_t i = 0; i < cache->item_count; ++i)
                    {
                        const auto &it = cache->items[i];
                        if (it.id != item_id)
                            continue;
                        out = it.detect ? "on" : "off";
                        return true;
                    }
                }
                return false;
            }
            if (ctrl == "tanks")
            {
                if (item_id == 0)
                    return false;
                const auto *cache = _stack_cache->tanksCache(node_id);
                if (!cache || !cache->has_data)
                {
                    _stack_cache->requestTanks(node_id);
                    return false;
                }
                for (size_t i = 0; i < cache->item_count; ++i)
                {
                    const auto &it = cache->items[i];
                    if (it.id != item_id)
                        continue;
                    if (param == "level")
                    {
                        if (it.level_full)
                            out = "full";
                        else if (it.level_mid)
                            out = "mid";
                        else if (it.level_low)
                            out = "low";
                        else
                            out = "empty";
                        return true;
                    }
                    if (param == "empty")
                    {
                        out = (!it.level_low && !it.level_mid && !it.level_full) ? "on" : "off";
                        return true;
                    }
                    if (param == "valve_on")
                    {
                        out = it.valve_on ? "on" : "off";
                        return true;
                    }
                    if (param == "pump_on")
                    {
                        out = it.pump_on ? "on" : "off";
                        return true;
                    }
                    if (param == "alarm_on")
                    {
                        out = it.alarm_on ? "on" : "off";
                        return true;
                    }
                    return false;
                }
                return false;
            }
            if (ctrl == "septic")
            {
                if (item_id == 0)
                    return false;
                const auto *cache = _stack_cache->septicCache(node_id);
                if (!cache || !cache->has_data)
                {
                    _stack_cache->requestSeptic(node_id);
                    return false;
                }
                for (size_t i = 0; i < cache->item_count; ++i)
                {
                    const auto &it = cache->items[i];
                    if (it.id != item_id)
                        continue;
                    if (param == "status")
                    {
                        if (it.alarm)
                            out = "alarm";
                        else if (it.warning)
                            out = "warn";
                        else
                            out = "ok";
                        return true;
                    }
                    if (param == "warning")
                    {
                        out = it.warning ? "on" : "off";
                        return true;
                    }
                    if (param == "alarm")
                    {
                        out = it.alarm ? "on" : "off";
                        return true;
                    }
                    return false;
                }
                return false;
            }
            return false;
        }

        if (ctrl == "sockets")
        {
            if (!_sockets || !_sockets->controllerEnabled() || item_id == 0)
                return false;
            bool st = false;
            if (param == "relay_on" && _sockets->relayStateById(item_id, st))
            {
                out = st ? "on" : "off";
                return true;
            }
            return false;
        }
        if (ctrl == "lights")
        {
            if (!_sockets || !_sockets->lightsEnabled() || item_id == 0)
                return false;
            bool st = false;
            if (param == "relay_on" && _sockets->lightRelayStateById(item_id, st))
            {
                out = st ? "on" : "off";
                return true;
            }
            return false;
        }
        if (ctrl == "meteo")
        {
            if (!_meteo || !_meteo->controllerEnabled() || item_id == 0)
                return false;
            const auto *st = _meteo->state(item_id);
            if (!st)
                return false;
            if (param == "ok")
            {
                out = st->ok ? "on" : "off";
                return true;
            }
            if (param == "temp_c" && st->has_temp)
            {
                out = String(st->temp_c, 1);
                return true;
            }
            if (param == "humidity" && st->has_humidity)
            {
                out = String(st->humidity, 1);
                return true;
            }
            return false;
        }
        if (ctrl == "security")
        {
            if (!_security || !_security->controllerEnabled())
                return false;
            if (param == "armed")
            {
                out = _security->armed() ? "on" : "off";
                return true;
            }
            if (param == "alarm_on")
            {
                out = _security->alarmOn() ? "on" : "off";
                return true;
            }
            if (param == "detect" && item_id != 0)
            {
                const auto *st = _security->state(item_id);
                if (!st)
                    return false;
                out = st->is_detect ? "on" : "off";
                return true;
            }
            return false;
        }
        if (ctrl == "tanks")
        {
            if (!_tanks || !_tanks->controllerEnabled() || item_id == 0)
                return false;
            const auto *st = _tanks->state(item_id);
            if (!st)
                return false;
            if (param == "level")
            {
                if (st->level_full)
                    out = "full";
                else if (st->level_mid)
                    out = "mid";
                else if (st->level_low)
                    out = "low";
                else
                    out = "empty";
                return true;
            }
            if (param == "empty")
            {
                out = (!st->level_low && !st->level_mid && !st->level_full) ? "on" : "off";
                return true;
            }
            if (param == "valve_on")
            {
                out = st->valve_on ? "on" : "off";
                return true;
            }
            if (param == "pump_on")
            {
                out = st->pump_on ? "on" : "off";
                return true;
            }
            if (param == "alarm_on")
            {
                out = st->alarm_on ? "on" : "off";
                return true;
            }
            return false;
        }
        if (ctrl == "septic")
            return item_id ? tryReadSepticConditionValue_(item_id, param, out) : false;
        return false;
    }

    bool checkRuleCondition_(const RulesController::Rule &rule) const
    {
        if (!rule.condition_enabled)
            return true;
        if (!rule.condition_controller.length() || !rule.condition_parameter.length())
            return false;
        String actual;
        if (!tryReadRuleConditionActual_(rule, actual))
            return false;
        return evalCondOp_(actual, rule.condition_op, rule.condition_value);
    }

    bool runControllerRuleAction_(int64_t chat_id, const RulesController::RuleAction &a)
    {
        String controller = a.controller;
        String param = a.parameter;
        String value = a.value;
        controller.trim();
        param.trim();
        value.trim();
        controller.toLowerCase();
        param.toLowerCase();
        const String actor = ruleActor_(chat_id);
        const uint32_t node_id = a.node_id;

        if (node_id != 0)
        {
            if (!_stack_master)
                return false;
            DynamicJsonDocument doc(256);
            if (controller == "security")
            {
                doc["feature"] = (uint8_t)StackFeature::Security;
                doc["action"] = "set";
                JsonObject params = doc["params"].to<JsonObject>();
                params["user"] = actor;
                if (param == "arm")
                    params["armed"] = true;
                else if (param == "disarm")
                    params["armed"] = false;
                else if (param == "clear")
                    params["clear"] = true;
                else
                    return false;
                char payload[256] = {};
                const size_t n = serializeJson(doc, payload, sizeof(payload));
                if (n == 0)
                    return false;
                const bool ok = _stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                                                      reinterpret_cast<const uint8_t *>(payload), n);
                if (ok && _stack_cache)
                    _stack_cache->requestSecurity(node_id);
                return ok;
            }
            if (controller == "sockets" || controller == "lights")
            {
                uint8_t id = 0;
                bool on = false;
                bool has_state = false;
                if (param == "toggle")
                {
                    if (!parseRuleId_(value, id))
                        return false;
                }
                else if (param == "set")
                {
                    const int sep = value.indexOf(',');
                    if (sep < 0)
                        return false;
                    if (!parseRuleId_(value.substring(0, sep), id))
                        return false;
                    if (!parseRuleOnOff_(value.substring(sep + 1), on))
                        return false;
                    has_state = true;
                }
                else
                    return false;

                doc["feature"] = (uint8_t)StackFeature::Sockets;
                doc["action"] = (controller == "lights") ? "set_lights" : "set";
                JsonObject params = doc["params"].to<JsonObject>();
                JsonArray items = params["items"].to<JsonArray>();
                JsonObject item = items.add<JsonObject>();
                item["id"] = (unsigned)id;
                if (param == "toggle")
                    item["toggle"] = true;
                else if (has_state)
                    item["state"] = on;

                char payload[256] = {};
                const size_t n = serializeJson(doc, payload, sizeof(payload));
                if (n == 0)
                    return false;
                const bool ok = _stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                                                      reinterpret_cast<const uint8_t *>(payload), n);
                if (ok && _stack_cache)
                {
                    if (controller == "lights")
                        _stack_cache->requestLights(node_id);
                    else
                        _stack_cache->requestSockets(node_id);
                }
                return ok;
            }
            return false;
        }

        if (controller == "security")
        {
            if (!_security)
                return false;
            if (param == "arm")
                return _security->armFrom("telegram_rule", actor);
            if (param == "disarm")
                return _security->disarmFrom("telegram_rule", actor);
            if (param == "clear")
            {
                _security->clearDetect();
                return true;
            }
            if (param == "rfid")
                return value.length() ? _security->processRfidUidString(value.c_str(), "telegram_rule") : false;
            if (param == "ibutton")
                return value.length() ? _security->processIButtonSerialString(value.c_str(), "telegram_rule") : false;
            return false;
        }

        if (controller == "sockets" || controller == "lights")
        {
            if (!_sockets)
                return false;
            if (param == "toggle")
            {
                uint8_t id = 0;
                if (!parseRuleId_(value, id))
                    return false;
                return (controller == "lights") ? _sockets->toggleLightRelayById(id) : _sockets->toggleRelayById(id);
            }
            if (param == "set")
            {
                const int sep = value.indexOf(',');
                if (sep < 0)
                    return false;
                uint8_t id = 0;
                const String id_s = value.substring(0, sep);
                if (!parseRuleId_(id_s, id))
                    return false;
                const String st = value.substring(sep + 1);
                bool on = false;
                if (!parseRuleOnOff_(st, on))
                    return false;
                return (controller == "lights") ? _sockets->setLightRelayById(id, on) : _sockets->setRelayById(id, on);
            }
            return false;
        }

        return false;
    }

    bool runQuickRule_(int64_t chat_id, uint8_t rule_id)
    {
        if (!_rules || !_bot)
            return false;
        const RulesController::Rule *rule = _rules->rule(rule_id);
        if (!rule || !rule->enabled)
            return false;
        if (!checkRuleCondition_(*rule))
            return false;
        if (!_rules->triggerRule(rule_id))
            return false;

        for (size_t i = 1; i <= RulesController::kActionCount; ++i)
        {
            const RulesController::RuleAction *a = _rules->action(rule_id, i);
            if (!a || !a->enabled)
                continue;
            if (a->delay_ms)
                delay(a->delay_ms);
            if (a->kind == RulesController::ActionKind::Telegram)
            {
                String msg = a->value.length() ? a->value : a->parameter;
                if (!msg.length())
                    continue;
                _bot->sendText(chat_id, msg);
                continue;
            }
            if (a->kind == RulesController::ActionKind::Pause)
                continue;
            if (a->kind == RulesController::ActionKind::Controller)
            {
                const bool ok = runControllerRuleAction_(chat_id, *a);
                (void)ok;
            }
        }
        return true;
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
        if (u.text == F("Я дома"))
        {
            const bool ok = runQuickRule_(u.chat_id, 1);
            if (!ok)
                _bot->sendText(u.chat_id, F("Правило Я дома отключено или недоступно"));
            return true;
        }
        if (u.text == F("Собираюсь"))
        {
            const bool ok = runQuickRule_(u.chat_id, 2);
            if (!ok)
                _bot->sendText(u.chat_id, F("Правило Собираюсь отключено или недоступно"));
            return true;
        }
        if (u.text == F("Ушёл"))
        {
            const bool ok = runQuickRule_(u.chat_id, 3);
            if (!ok)
                _bot->sendText(u.chat_id, F("Правило Ушёл отключено или недоступно"));
            return true;
        }
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

    bool handleAvrSelection_(const TelegramClient::Update &u);

    bool handleLeakSelection_(const TelegramClient::Update &u);

    bool handleLeakAction_(const TelegramClient::Update &u);

    bool handleRingSelection_(const TelegramClient::Update &u);

    bool handleWateringSelection_(const TelegramClient::Update &u);

    bool handleWateringAction_(const TelegramClient::Update &u);

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
#include "core/network/telegram/menu/telegram_menu_avr.hpp"
#include "core/network/telegram/menu/telegram_menu_leak.hpp"
#include "core/network/telegram/menu/telegram_menu_ring.hpp"
#include "core/network/telegram/menu/telegram_menu_watering.hpp"

inline bool TelegramMenu::cmdSockets_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
{
    return TelegramMenuSockets::cmdSockets_(bot, u, reply);
}

inline bool TelegramMenu::cmdLights_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
{
    return TelegramMenuSockets::cmdLights_(bot, u, reply);
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

inline bool TelegramMenu::cmdAvr_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
{
    return TelegramMenuAvr::cmdAvr_(bot, u, reply);
}

inline bool TelegramMenu::cmdAvrStatus_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
{
    return TelegramMenuAvr::cmdAvrStatus_(bot, u, reply);
}

inline bool TelegramMenu::cmdLeak_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
{
    return TelegramMenuLeak::cmdLeak_(bot, u, reply);
}

inline bool TelegramMenu::cmdLeakList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
{
    return TelegramMenuLeak::cmdLeakList_(bot, u, reply);
}

inline bool TelegramMenu::cmdLeakShow_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
{
    return TelegramMenuLeak::cmdLeakShow_(bot, u, reply);
}

inline bool TelegramMenu::cmdLeakAck_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
{
    return TelegramMenuLeak::cmdLeakAck_(bot, u, reply);
}

inline bool TelegramMenu::cmdRing_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
{
    return TelegramMenuRing::cmdRing_(bot, u, reply);
}

inline bool TelegramMenu::cmdRingOn_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
{
    return TelegramMenuRing::cmdRingOn_(bot, u, reply);
}

inline bool TelegramMenu::cmdRingOff_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
{
    return TelegramMenuRing::cmdRingOff_(bot, u, reply);
}

inline bool TelegramMenu::cmdWatering_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
{
    return TelegramMenuWatering::cmdWatering_(bot, u, reply);
}

inline bool TelegramMenu::cmdWateringList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
{
    return TelegramMenuWatering::cmdWateringList_(bot, u, reply);
}

inline bool TelegramMenu::cmdWateringShow_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
{
    return TelegramMenuWatering::cmdWateringShow_(bot, u, reply);
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
    return TelegramMenuSockets::parseSocketIdFromText_(text, out, (uint8_t)SocketController::kSocketCount);
}

inline bool TelegramMenu::parseSocketId_(const String &text, uint8_t &out)
{
    return TelegramMenuSockets::parseSocketId_(text, out, (uint8_t)SocketController::kSocketCount);
}

inline bool TelegramMenu::parseSocketLabel_(const String &text, uint8_t &out)
{
    return TelegramMenuSockets::parseSocketLabel_(text, out, (uint8_t)SocketController::kSocketCount);
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
    TelegramMenuSockets::buildSocketLabels_(*const_cast<TelegramMenu *>(this), out, false);
}

inline void TelegramMenu::buildSocketLabels_(std::vector<String> &out, bool lights_only) const
{
    TelegramMenuSockets::buildSocketLabels_(*const_cast<TelegramMenu *>(this), out, lights_only);
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
    return TelegramMenuSockets::socketListTextHtml_(*const_cast<TelegramMenu *>(this), 0, false);
}

inline String TelegramMenu::meteoListTextHtml_() const
{
    return TelegramMenuMeteo::meteoListTextHtml_(*const_cast<TelegramMenu *>(this), 0);
}

inline String TelegramMenu::meteoSensorTextHtml_(uint8_t id) const
{
    return TelegramMenuMeteo::meteoSensorTextHtml_(*const_cast<TelegramMenu *>(this), 0, id);
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
    return TelegramMenuThermo::thermoListTextHtml_(*const_cast<TelegramMenu *>(this), 0);
}

inline String TelegramMenu::thermoDeviceTextHtml_(uint8_t id) const
{
    return TelegramMenuThermo::thermoDeviceTextHtml_(*const_cast<TelegramMenu *>(this), 0, id);
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
    return TelegramMenuTanks::tankListTextHtml_(*const_cast<TelegramMenu *>(this), 0);
}

inline String TelegramMenu::tankDeviceTextHtml_(uint8_t id) const
{
    return TelegramMenuTanks::tankDeviceTextHtml_(*const_cast<TelegramMenu *>(this), 0, id);
}

inline String TelegramMenu::septicStatusText_() const
{
    return TelegramMenuSeptic::septicStatusText_(*const_cast<TelegramMenu *>(this), 0);
}

inline String TelegramMenu::septicListTextHtml_() const
{
    return TelegramMenuSeptic::septicListTextHtml_(*const_cast<TelegramMenu *>(this), 0);
}

inline String TelegramMenu::securityStatusText_() const
{
    return TelegramMenuSecurity::securityStatusText_(*const_cast<TelegramMenu *>(this), 0);
}

inline String TelegramMenu::securityListTextHtml_() const
{
    return TelegramMenuSecurity::securityListTextHtml_(*const_cast<TelegramMenu *>(this), 0);
}

inline void TelegramMenu::sendSocketMenu_(int64_t chat_id)
{
    TelegramMenuSockets::sendSocketMenu_(*this, chat_id, false);
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

inline void TelegramMenu::sendAvrMenu_(int64_t chat_id)
{
    TelegramMenuAvr::sendAvrMenu_(*this, chat_id);
}

inline void TelegramMenu::sendLeakMenu_(int64_t chat_id)
{
    TelegramMenuLeak::sendLeakMenu_(*this, chat_id);
}

inline void TelegramMenu::sendRingMenu_(int64_t chat_id)
{
    TelegramMenuRing::sendRingMenu_(*this, chat_id);
}

inline void TelegramMenu::sendWateringMenu_(int64_t chat_id)
{
    TelegramMenuWatering::sendWateringMenu_(*this, chat_id);
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
    return TelegramMenuSeptic::septicControlMarkup_(*const_cast<TelegramMenu *>(this), 0);
}

inline String TelegramMenu::septicControlMarkup_(int64_t chat_id) const
{
    return TelegramMenuSeptic::septicControlMarkup_(*const_cast<TelegramMenu *>(this), chat_id);
}

inline String TelegramMenu::avrControlMarkup_(int64_t chat_id) const
{
    return TelegramMenuAvr::avrControlMarkup_(*const_cast<TelegramMenu *>(this), chat_id);
}

inline String TelegramMenu::leakControlMarkup_(int64_t chat_id) const
{
    std::vector<String> labels;
    TelegramMenuLeak::buildLeakLabels_(*const_cast<TelegramMenu *>(this), chat_id, labels);
    if (labels.empty())
        labels.push_back(F("Назад"));
    return buildKeyboardMarkup_(labels);
}

inline String TelegramMenu::ringControlMarkup_(int64_t chat_id) const
{
    return TelegramMenuRing::ringControlMarkup_(*const_cast<TelegramMenu *>(this), chat_id);
}

inline String TelegramMenu::wateringControlMarkup_(int64_t chat_id) const
{
    return TelegramMenuWatering::wateringControlMarkup_(*const_cast<TelegramMenu *>(this), chat_id);
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

inline bool TelegramMenu::handleAvrSelection_(const TelegramClient::Update &u)
{
    return TelegramMenuAvr::handleAvrSelection_(*this, u);
}

inline bool TelegramMenu::handleLeakSelection_(const TelegramClient::Update &u)
{
    return TelegramMenuLeak::handleLeakSelection_(*this, u);
}

inline bool TelegramMenu::handleLeakAction_(const TelegramClient::Update &u)
{
    return TelegramMenuLeak::handleLeakAction_(*this, u);
}

inline bool TelegramMenu::handleRingSelection_(const TelegramClient::Update &u)
{
    return TelegramMenuRing::handleRingSelection_(*this, u);
}

inline bool TelegramMenu::handleWateringSelection_(const TelegramClient::Update &u)
{
    return TelegramMenuWatering::handleWateringSelection_(*this, u);
}

inline bool TelegramMenu::handleWateringAction_(const TelegramClient::Update &u)
{
    return TelegramMenuWatering::handleWateringAction_(*this, u);
}

