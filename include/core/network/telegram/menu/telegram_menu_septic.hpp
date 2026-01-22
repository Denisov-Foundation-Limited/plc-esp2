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

class TelegramMenuSeptic
{
public:
    static bool cmdSeptic_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        (void)reply;
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        if (!TelegramMenu::_self->_septic)
        {
            reply = "Септик недоступен";
            return true;
        }
        if (!TelegramMenu::_self->isLocalSelected_(u.chat_id))
        {
            reply = "Список доступен только для локального устройства";
            return true;
        }
        TelegramMenuSeptic::sendSepticMenu_(*TelegramMenu::_self, u.chat_id);
        return true;
    }

    static bool cmdSepticStatus_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        if (!TelegramMenu::_self->_septic)
        {
            reply = "Септик недоступен";
            return true;
        }
        if (!TelegramMenu::_self->isLocalSelected_(u.chat_id))
        {
            reply = "Доступно только для локального устройства";
            return true;
        }
        reply = TelegramMenuSeptic::septicStatusText_(*TelegramMenu::_self);
        return true;
    }

    static bool cmdSepticList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        if (!TelegramMenu::_self->_septic)
        {
            reply = "Септик недоступен";
            return true;
        }
        if (!TelegramMenu::_self->isLocalSelected_(u.chat_id))
        {
            reply = "Список доступен только для локального устройства";
            return true;
        }
        const String text = TelegramMenuSeptic::septicListTextHtml_(*TelegramMenu::_self);
        bot.sendText(u.chat_id, text, "", "HTML");
        return true;
    }

    static bool cmdSepticMonitor_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        (void)bot;
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        if (!TelegramMenu::_self->_septic)
        {
            reply = "Септик недоступен";
            return true;
        }
        if (!TelegramMenu::_self->isLocalSelected_(u.chat_id))
        {
            reply = "Доступно только для локального устройства";
            return true;
        }
        const char *cmd = "/septic_monitor";
        String tail = u.text.substring(strlen(cmd));
        tail.trim();
        int space = tail.indexOf(' ');
        if (space <= 0)
        {
            reply = "Использование: /septic_monitor <id> <on|off>";
            return true;
        }
        String id_str = tail.substring(0, space);
        String val_str = tail.substring(space + 1);
        id_str.trim();
        val_str.trim();
        uint8_t id = 0;
        if (!TelegramMenuSeptic::parseSepticId_(id_str, id))
        {
            reply = "Неверный ID септика";
            return true;
        }
        bool on = false;
        if (!TelegramMenu::parseOnOff_(val_str, on))
        {
            reply = "Неверное значение (on/off)";
            return true;
        }
        if (!TelegramMenu::_self->_septic->setMonitoring(id, on))
        {
            reply = "Не удалось";
            return true;
        }
        reply = on ? "Мониторинг включен" : "Мониторинг выключен";
        return true;
    }

    static String septicStatusText_(TelegramMenu &self)
    {
        if (!self._septic)
            return "Септик недоступен";
        String out = F("Септик:\n");
        out += F("  Контроллер: ");
        out += self._septic->controllerEnabled() ? "включен" : "выключен";
        const auto *cfg = self._septic->configByIndex(0);
        const auto *st = self._septic->stateByIndex(0);
        if (cfg && st)
        {
            out += F("\n  Мониторинг: ");
            out += cfg->monitoring_on ? "вкл" : "выкл";
            out += F("\n  Warning: ");
            out += st->warning ? "on" : "off";
            out += F("\n  Alarm: ");
            out += st->alarm ? "on" : "off";
            out += F("\n  W port: ");
            if (cfg->warning_port != SepticController::kInvalidPort)
                out += String((unsigned)cfg->warning_port);
            else
                out += "none";
            out += F("\n  A port: ");
            if (cfg->alarm_port != SepticController::kInvalidPort)
                out += String((unsigned)cfg->alarm_port);
            else
                out += "none";
        }
        return out;
    }

    static String septicListTextHtml_(TelegramMenu &self)
    {
        if (!self._septic)
            return "Септик недоступен";
        String out = F("<b>Септик:</b>\n");
        out += F("ID  Mon  Warn  Alarm  W.Port  A.Port  Name\n");
        out += F("-----------------------------------------\n");
        for (size_t i = 0; i < SepticController::kSepticCount; ++i)
        {
            const auto *cfg = self._septic->configByIndex(i);
            const auto *st = self._septic->stateByIndex(i);
            if (!cfg || !st || !cfg->enabled)
                continue;
            out += String((unsigned)cfg->id);
            out += F("  ");
            out += cfg->monitoring_on ? "on" : "off";
            out += F("  ");
            out += st->warning ? "on" : "off";
            out += F("  ");
            out += st->alarm ? "on" : "off";
            out += F("  ");
            if (cfg->warning_port != SepticController::kInvalidPort)
                out += String((unsigned)cfg->warning_port);
            else
                out += F("--");
            out += F("  ");
            if (cfg->alarm_port != SepticController::kInvalidPort)
                out += String((unsigned)cfg->alarm_port);
            else
                out += F("--");
            out += F("  ");
            if (cfg->name.length())
                out += self.escapeHtml_(cfg->name);
            out += F("\n");
        }
        return out;
    }

    static String septicControlMarkup_(TelegramMenu &self)
    {
        std::vector<String> labels;
        labels.reserve(4);
        bool monitoring_on = true;
        if (self._septic)
        {
            const auto *cfg = self._septic->configByIndex(0);
            if (cfg)
                monitoring_on = cfg->monitoring_on;
        }
        labels.push_back(F("Статус"));
        labels.push_back(F("Список"));
        labels.push_back(monitoring_on ? F("Мониторинг Выкл") : F("Мониторинг Вкл"));
        labels.push_back(F("Назад"));
        return TelegramMenu::buildKeyboardMarkup_(labels);
    }

    static void sendSepticMenu_(TelegramMenu &self, int64_t chat_id)
    {
        if (!self._bot)
            return;
        if (!self.isLocalSelected_(chat_id))
        {
            self._bot->sendText(chat_id, F("Список доступен только для локального устройства"));
            return;
        }
        if (!self._septic)
        {
            self._bot->sendText(chat_id, F("Септик недоступен"));
            return;
        }
        const String markup = TelegramMenuSeptic::septicControlMarkup_(self);
        const String text = TelegramMenuSeptic::septicStatusText_(self);
        self._bot->setMenu(chat_id, "septic");
        self._bot->sendText(chat_id, text, markup);
    }

    static bool handleSepticSelection_(TelegramMenu &self, const TelegramClient::Update &u)
    {
        if (!self._bot)
            return false;
        const char *menu_id = self._bot->currentMenuId(u.chat_id);
        if (!menu_id || strcmp(menu_id, "septic") != 0)
            return false;
        if (u.text.startsWith("/"))
            return false;
        if (u.text == F("Назад"))
        {
            self._bot->enterMenu(u.chat_id, "device", self.adminPrefix_(u.chat_id));
            return true;
        }
        if (!self._septic)
        {
            self._bot->sendText(u.chat_id, F("Септик недоступен"));
            return true;
        }
        if (!self.isLocalSelected_(u.chat_id))
        {
            self._bot->sendText(u.chat_id, F("Доступно только для локального устройства"));
            return true;
        }
        if (u.text == F("Статус"))
        {
            TelegramMenuSeptic::sendSepticMenu_(self, u.chat_id);
            return true;
        }
        if (u.text == F("Список"))
        {
            const String text = TelegramMenuSeptic::septicListTextHtml_(self);
            self._bot->sendText(u.chat_id, text, "", "HTML");
            return true;
        }
        if (u.text == F("Мониторинг Вкл") || u.text == F("Мониторинг Выкл"))
        {
            const auto *cfg = self._septic->configByIndex(0);
            if (!cfg)
            {
                self._bot->sendText(u.chat_id, F("Септик не настроен"));
                return true;
            }
            const bool on = (u.text == F("Мониторинг Вкл"));
            self._septic->setMonitoring(cfg->id, on);
            TelegramMenuSeptic::sendSepticMenu_(self, u.chat_id);
            return true;
        }
        return false;
    }

    static bool parseSepticId_(const String &text, uint8_t &out)
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
        if (v <= 0 || v > (int)SepticController::kSepticCount)
            return false;
        out = (uint8_t)v;
        return true;
    }
};
