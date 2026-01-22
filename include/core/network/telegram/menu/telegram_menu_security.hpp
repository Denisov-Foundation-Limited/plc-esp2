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

class TelegramMenuSecurity
{
public:
    static bool cmdSecurity_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        (void)reply;
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        if (!TelegramMenu::_self->_security)
        {
            reply = "Охрана недоступна";
            return true;
        }
        if (!TelegramMenu::_self->isLocalSelected_(u.chat_id))
        {
            reply = "Доступно только для локального устройства";
            return true;
        }
        TelegramMenuSecurity::sendSecurityMenu_(*TelegramMenu::_self, u.chat_id);
        return true;
    }

    static bool cmdSecurityStatus_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        if (!TelegramMenu::_self->_security)
        {
            reply = "Охрана недоступна";
            return true;
        }
        if (!TelegramMenu::_self->isLocalSelected_(u.chat_id))
        {
            reply = "Доступно только для локального устройства";
            return true;
        }
        reply = TelegramMenuSecurity::securityStatusText_(*TelegramMenu::_self);
        return true;
    }

    static bool cmdSecurityList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        if (!TelegramMenu::_self->_security)
        {
            reply = "Охрана недоступна";
            return true;
        }
        if (!TelegramMenu::_self->isLocalSelected_(u.chat_id))
        {
            reply = "Доступно только для локального устройства";
            return true;
        }
        const String text = TelegramMenuSecurity::securityListTextHtml_(*TelegramMenu::_self);
        bot.sendText(u.chat_id, text, "", "HTML");
        return true;
    }

    static bool cmdSecurityArm_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        if (!TelegramMenu::_self->_security)
        {
            reply = "Охрана недоступна";
            return true;
        }
        if (!TelegramMenu::_self->isLocalSelected_(u.chat_id))
        {
            reply = "Доступно только для локального устройства";
            return true;
        }
        if (TelegramMenu::_self->_security->arm())
            reply = "Охрана включена";
        else
            reply = "Контроллер охраны выключен";
        return true;
    }

    static bool cmdSecurityDisarm_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        if (!TelegramMenu::_self->_security)
        {
            reply = "Охрана недоступна";
            return true;
        }
        if (!TelegramMenu::_self->isLocalSelected_(u.chat_id))
        {
            reply = "Доступно только для локального устройства";
            return true;
        }
        TelegramMenu::_self->_security->disarm();
        reply = "Охрана выключена";
        return true;
    }

    static bool cmdSecuritySilent_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        (void)bot;
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        if (!TelegramMenu::_self->_security)
        {
            reply = "Охрана недоступна";
            return true;
        }
        if (!TelegramMenu::_self->isLocalSelected_(u.chat_id))
        {
            reply = "Доступно только для локального устройства";
            return true;
        }
        const char *cmd = "/security_silent";
        String tail = u.text.substring(strlen(cmd));
        tail.trim();
        int space = tail.indexOf(' ');
        if (space <= 0)
        {
            reply = "Использование: /security_silent <id> <on|off>";
            return true;
        }
        String id_str = tail.substring(0, space);
        String val_str = tail.substring(space + 1);
        id_str.trim();
        val_str.trim();
        uint8_t id = 0;
        if (!TelegramMenuSecurity::parseSecurityId_(id_str, id))
        {
            reply = "Неверный ID охраны";
            return true;
        }
        bool silent = false;
        if (!TelegramMenu::parseOnOff_(val_str, silent))
        {
            reply = "Неверное значение (on/off)";
            return true;
        }
        if (!TelegramMenu::_self->_security->setSilent(id, silent))
        {
            reply = "Не удалось";
            return true;
        }
        reply = silent ? "Тихий режим включен" : "Тихий режим выключен";
        return true;
    }

    static String securityStatusText_(TelegramMenu &self)
    {
        if (!self._security)
            return "Охрана недоступна";
        String out = F("Охрана:\n");
        out += F("  Контроллер: ");
        out += self._security->controllerEnabled() ? "включен" : "выключен";
        out += F("\n  Статус: ");
        out += self._security->armed() ? "под охраной" : "снято";
        out += F("\n  Тревога: ");
        out += self._security->alarmOn() ? "on" : "off";
        out += F("\n  Сирена: ");
        if (self._security->sirenPort() != SecurityController::kInvalidPort)
            out += String((unsigned)self._security->sirenPort());
        else
            out += "none";
        return out;
    }

    static String securityListTextHtml_(TelegramMenu &self)
    {
        if (!self._security)
            return "Охрана недоступна";
        String out = F("<b>Охрана:</b>\n");
        out += F("ID  Type  Port  Silent  Detect  Name\n");
        out += F("-----------------------------------\n");
        for (size_t i = 0; i < SecurityController::kSensorCount; ++i)
        {
            const auto *cfg = self._security->configByIndex(i);
            const auto *st = self._security->stateByIndex(i);
            if (!cfg || !st || !cfg->enabled)
                continue;
            out += String((unsigned)cfg->id);
            out += F("  ");
            out += (cfg->type == SecurityController::SensorType::Reed) ? "reed" : "pir";
            out += F("  ");
            if (cfg->port != SecurityController::kInvalidPort)
                out += String((unsigned)cfg->port);
            else
                out += F("--");
            out += F("  ");
            out += cfg->silent ? "yes" : "no";
            out += F("  ");
            out += st->is_detect ? "yes" : "no";
            out += F("  ");
            if (cfg->name.length())
                out += self.escapeHtml_(cfg->name);
            out += F("\n");
        }
        return out;
    }

    static String securityControlMarkup_()
    {
        std::vector<String> labels;
        labels.reserve(6);
        labels.push_back(F("Статус"));
        labels.push_back(F("Взять под охрану"));
        labels.push_back(F("Снять с охраны"));
        labels.push_back(F("Список датчиков"));
        labels.push_back(F("Сбросить детекты"));
        labels.push_back(F("Назад"));
        return TelegramMenu::buildKeyboardMarkup_(labels);
    }

    static void sendSecurityMenu_(TelegramMenu &self, int64_t chat_id)
    {
        if (!self._bot)
            return;
        if (!self.isLocalSelected_(chat_id))
        {
            self._bot->sendText(chat_id, F("Доступно только для локального устройства"));
            return;
        }
        if (!self._security)
        {
            self._bot->sendText(chat_id, F("Охрана недоступна"));
            return;
        }
        const String markup = TelegramMenuSecurity::securityControlMarkup_();
        const String text = TelegramMenuSecurity::securityStatusText_(self);
        self._bot->setMenu(chat_id, "security");
        self._bot->sendText(chat_id, text, markup);
    }

    static bool handleSecuritySelection_(TelegramMenu &self, const TelegramClient::Update &u)
    {
        if (!self._bot)
            return false;
        const char *menu_id = self._bot->currentMenuId(u.chat_id);
        if (!menu_id || strcmp(menu_id, "security") != 0)
            return false;
        if (u.text.startsWith("/"))
            return false;
        if (u.text == F("Назад"))
        {
            self._bot->enterMenu(u.chat_id, "device", self.adminPrefix_(u.chat_id));
            return true;
        }
        if (!self._security)
        {
            self._bot->sendText(u.chat_id, F("Охрана недоступна"));
            return true;
        }
        if (!self.isLocalSelected_(u.chat_id))
        {
            self._bot->sendText(u.chat_id, F("Доступно только для локального устройства"));
            return true;
        }
        if (u.text == F("Статус"))
        {
            TelegramMenuSecurity::sendSecurityMenu_(self, u.chat_id);
            return true;
        }
        if (u.text == F("Взять под охрану"))
        {
            self._security->arm();
            TelegramMenuSecurity::sendSecurityMenu_(self, u.chat_id);
            return true;
        }
        if (u.text == F("Снять с охраны"))
        {
            self._security->disarm();
            TelegramMenuSecurity::sendSecurityMenu_(self, u.chat_id);
            return true;
        }
        if (u.text == F("Список датчиков"))
        {
            const String text = TelegramMenuSecurity::securityListTextHtml_(self);
            self._bot->sendText(u.chat_id, text, "", "HTML");
            return true;
        }
        if (u.text == F("Сбросить детекты"))
        {
            self._security->clearDetect();
            TelegramMenuSecurity::sendSecurityMenu_(self, u.chat_id);
            return true;
        }
        return false;
    }

    static bool parseSecurityId_(const String &text, uint8_t &out)
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
        if (v <= 0 || v > (int)SecurityController::kSensorCount)
            return false;
        out = (uint8_t)v;
        return true;
    }
};
