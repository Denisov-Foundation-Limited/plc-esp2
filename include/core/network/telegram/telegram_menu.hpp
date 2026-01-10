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
#include <vector>

#if defined(ESP32)
#include <Update.h>
#endif

#include "core/network/telegram/telegram_bot.hpp"
#include "core/rtc.hpp"
#include "core/network/wifi_manager.hpp"
#include "plc/plc_control.hpp"
#include "utils/configs.hpp"

class TelegramMenu
{
public:
    TelegramMenu(PlcControl &plc, WifiManager &wifi, RTC &rtc, TelegramBot &bot, Configs &configs)
        : _plc(plc), _wifi(wifi), _rtc(rtc), _bot(&bot), _configs(configs)
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
    }

    void setAdminPassword(const String &password) { _admin_password = password; }
    const String &adminPassword() const { return _admin_password; }

private:
    static inline TelegramMenu *_self = nullptr;

    PlcControl &_plc;
    WifiManager &_wifi;
    RTC &_rtc;
    String _admin_password;
    Configs &_configs;

    struct ChatAuth
    {
        int64_t chat_id = 0;
        bool authorized = false;
        bool awaiting = false;
        bool awaiting_config = false;
        uint8_t fail_count = 0;
        uint32_t lock_until_ms = 0;
    };

    std::vector<ChatAuth> _auth;
    static constexpr size_t kMaxConfigBytes = 8192;

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
        bot.sendText(u.chat_id, F("Введите пароль Админки:"));
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
        reply = "Wi-Fi:\n  режим : ";
        reply += _self->_wifi.ap() ? "AP" : "STA";
        reply += "\n  ssid  : ";
        reply += _self->_wifi.ssid();
        reply += "\n  ap_ssid: ";
        reply += _self->_wifi.apSsid();
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
            reply = "Перезапуск Wi-Fi не удался";
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
        bot.sendText(u.chat_id, F("Отправьте JSON для сохранения startup-config. /back — отмена."));
        return true;
    }

    static bool cmdFirmware_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        if (!_self)
            return false;
        ChatAuth *st = _self->ensureAuth_(u.chat_id);
        if (!st)
            return false;
        if (_self->_admin_password.length() > 0 && !st->authorized)
        {
            reply = "Нужна авторизация. Используйте /admin.";
            return true;
        }
        reply = "Отправьте файл firmware.bin для обновления прошивки.";
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
            reply = "RTC: ошибка чтения";
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
        const bool is_firmware = (u.document_file_name == "firmware.bin");
        if (!is_config && !is_firmware)
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
        if (is_config && u.document_size > 0 && u.document_size > kMaxConfigBytes)
        {
            if (self._bot)
                self._bot->sendText(u.chat_id, F("Конфиг слишком большой."));
            return true;
        }
        if (!self._bot)
            return true;

        TelegramClient &client = self._bot->client();
        String file_path;
        if (!client.getFilePath(u.document_file_id, file_path))
        {
            self._bot->sendText(u.chat_id, String("getFile: ") + client.lastError());
            return true;
        }

        if (is_config)
        {
            String json;
            if (!client.downloadFile(file_path, json))
            {
                self._bot->sendText(u.chat_id, String("Скачивание не удалось: ") + client.lastError());
                return true;
            }
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, json);
            if (err)
            {
                self._bot->sendText(u.chat_id, F("Ошибка разбора JSON."));
                return true;
            }
            if (!self._configs.save(doc))
            {
                self._bot->sendText(u.chat_id, F("Не удалось сохранить конфиг."));
                return true;
            }
            self._bot->sendText(u.chat_id, F("Startup-config сохранен. Перезагрузите контроллер для применения."));
            return true;
        }

#if defined(ESP32)
        self._bot->sendText(u.chat_id, F("Начинаю обновление прошивки..."));
        size_t bytes = 0;
        const size_t size = u.document_size > 0 ? (size_t)u.document_size : UPDATE_SIZE_UNKNOWN;
        if (!Update.begin(size))
        {
            self._bot->sendText(u.chat_id, F("Не удалось начать обновление."));
            return true;
        }
        auto writer = [](void *ctx, const uint8_t *data, size_t len) -> bool
        {
            (void)ctx;
            return Update.write(data, len) == len;
        };
        if (!client.downloadFile(file_path, writer, nullptr, bytes))
        {
            Update.abort();
            self._bot->sendText(u.chat_id, String("Скачивание не удалось: ") + client.lastError());
            return true;
        }
        if (!Update.end(true))
        {
            self._bot->sendText(u.chat_id, F("Обновление прошивки не удалось."));
            return true;
        }
        self._bot->sendText(u.chat_id, F("Прошивка обновлена. Перезагрузка..."));
        ESP.restart();
#else
        self._bot->sendText(u.chat_id, F("Обновление прошивки не поддерживается."));
#endif
        return true;
    }

    static bool onText_(void *ctx, const TelegramClient::Update &u)
    {
        TelegramMenu *self = static_cast<TelegramMenu *>(ctx);
        if (!self)
            return false;
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
            if (!self->_configs.save(doc))
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
                self->_bot->sendText(u.chat_id, F("Неверный пароль\nВведите пароль Админки:"));
        }
        return true;
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
    }

    void resetAwaiting_(int64_t chat_id)
    {
        ChatAuth *st = findAuth_(chat_id);
        if (!st)
            return;
        st->awaiting = false;
        st->awaiting_config = false;
    }

    TelegramBot *_bot = nullptr;

    static inline const TelegramBot::MenuItem kRootItems[] = {
        { "Админка", "Админка", nullptr, nullptr },
    };

    static inline const TelegramBot::MenuItem kAdminItems[] = {
        { "ПЛК", nullptr, "plc", nullptr },
        { "Часы", nullptr, "rtc", nullptr },
        { "Wi-Fi", nullptr, "wifi", nullptr },
        { "Настройки", nullptr, "settings", nullptr },
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
        { "Показать", "/wifi", nullptr, nullptr },
        { "Назад", "/back", nullptr, nullptr },
    };

    static inline const TelegramBot::MenuItem kSettingsItems[] = {
        { "Перезапуск Wi-Fi", "/wifi_restart", nullptr, nullptr },
        { "Wi-Fi AP вкл", "/wifi_ap_on", nullptr, nullptr },
        { "Wi-Fi AP выкл", "/wifi_ap_off", nullptr, nullptr },
        { "Startup-config", "/config_set", nullptr, nullptr },
        { "Прошивка", "/fw_update", nullptr, nullptr },
        { "Назад", "/back", nullptr, nullptr },
    };

    static inline const TelegramBot::Menu kMenus[] = {
        { "root", "Главное меню", kRootItems, 1, nullptr },
        { "admin", "Админка", kAdminItems, 5, "root" },
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
        { "/wifi_ap_on", &TelegramMenu::cmdWifiApOn_ },
        { "/wifi_ap_off", &TelegramMenu::cmdWifiApOff_ },
        { "/config_set", &TelegramMenu::cmdConfigSet_ },
        { "/fw_update", &TelegramMenu::cmdFirmware_ },
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
                _bot->sendText(st.chat_id, F("Слишком много попыток. Блокировка 30 сек"));
        }
    }
};
