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

class TelegramMenuThermo
{
public:
    static bool cmdThermo_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        (void)reply;
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        if (!TelegramMenu::_self->_thermo)
        {
            reply = "Термо недоступно";
            return true;
        }
        if (!TelegramMenu::_self->isLocalSelected_(u.chat_id))
        {
            reply = "Список доступен только для локального устройства";
            return true;
        }
        TelegramMenuThermo::sendThermoMenu_(*TelegramMenu::_self, u.chat_id);
        return true;
    }

    static bool cmdThermoList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        (void)bot;
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        if (!TelegramMenu::_self->_thermo)
        {
            reply = "Термо недоступно";
            return true;
        }
        if (!TelegramMenu::_self->isLocalSelected_(u.chat_id))
        {
            reply = "Список доступен только для локального устройства";
            return true;
        }
        reply = TelegramMenuThermo::thermoListTextHtml_(*TelegramMenu::_self);
        return true;
    }

    static bool cmdThermoShow_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        if (!TelegramMenu::_self->_thermo)
        {
            reply = "Термо недоступно";
            return true;
        }
        if (!TelegramMenu::_self->isLocalSelected_(u.chat_id))
        {
            reply = "Доступно только для локального устройства";
            return true;
        }
        const char *cmd = "/thermo_show";
        String tail = u.text.substring(strlen(cmd));
        tail.trim();
        if (tail.length() == 0)
        {
            reply = "Использование: /thermo_show <id>";
            return true;
        }
        uint8_t id = 0;
        if (!TelegramMenuThermo::parseThermoId_(tail, id))
        {
            reply = "Неверный ID термо";
            return true;
        }
        TelegramMenuThermo::sendThermoDevice_(*TelegramMenu::_self, u.chat_id, id);
        return true;
    }

    static void buildThermoLabels_(TelegramMenu &self, std::vector<String> &out)
    {
        out.clear();
        if (!self._thermo)
        {
            out.reserve(1);
            out.push_back(F("Назад"));
            return;
        }
        out.reserve(ThermoController::kDeviceCount + 1);
        for (size_t i = 0; i < ThermoController::kDeviceCount; ++i)
        {
            const auto *cfg = self._thermo->configByIndex(i);
            if (!cfg || !cfg->enabled)
                continue;
            String label;
            label += String((unsigned)cfg->id);
            label += ": ";
            if (cfg->name.length())
            {
                label += cfg->name;
            }
            out.push_back(label);
        }
        out.push_back(F("Назад"));
    }

    static String thermoListTextHtml_(TelegramMenu &self)
    {
        String out = F("<b>Термо:</b>");
        out.reserve(768);
        if (!self._thermo)
        {
            out += F("\n  недоступно");
            return out;
        }
        bool any = false;
        for (size_t i = 0; i < ThermoController::kDeviceCount; ++i)
        {
            const auto *cfg = self._thermo->configByIndex(i);
            const auto *st = self._thermo->stateByIndex(i);
            if (!cfg || !st || !cfg->enabled)
                continue;
            any = true;
            out += "\n  ";
            out += String((unsigned)cfg->id);
            out += ": ";
            if (cfg->name.length())
            {
                out += "<b>";
                out += self.escapeHtml_(cfg->name);
                out += "</b>";
            }
            else
            {
                out += "<b>-</b>";
            }
            out += "\n   режим: <b>";
            out += TelegramMenuThermo::thermoModeLabel_(cfg->mode);
            out += "</b>";
            out += "\n   питание: <b>";
            out += st->power_on ? F("🟢") : F("⚪");
            out += "</b>";
            out += "\n   статус: <b>";
            if (st->heat_on)
                out += F("🔥");
            else if (st->cool_on)
                out += F("❄️");
            else
                out += F("⏸");
            out += "</b>";
        }
        if (!any)
            out += F("\n  пусто");
        return out;
    }

    static String thermoDeviceTextHtml_(TelegramMenu &self, uint8_t id)
    {
        if (!self._thermo)
            return F("Термо недоступно");
        const auto *cfg = self._thermo->config(id);
        const auto *st = self._thermo->state(id);
        if (!cfg || !st)
            return F("Неверное устройство");
        String out = F("<b>Термо устройство:</b>");
        out.reserve(512);
        out += "\n  имя: ";
        if (cfg->name.length())
        {
            out += "<b>";
            out += self.escapeHtml_(cfg->name);
            out += "</b>";
        }
        else
        {
            out += "<b>-</b>";
        }
        out += "\n  включен: ";
        out += "<b>";
        out += cfg->enabled ? "1" : "0";
        out += "</b>";
        out += "\n  режим: ";
        out += "<b>";
        out += TelegramMenuThermo::thermoModeLabel_(cfg->mode);
        out += "</b>";
        out += "\n  темп: ";
        if (self._meteo && cfg->sensor_id)
        {
            const auto *st = self._meteo->state(cfg->sensor_id);
            if (st && st->has_temp)
            {
                char buf[10] = {};
                dtostrf(st->temp_c, 0, 1, buf);
                out += "<b>";
                out += buf;
                out += "°";
                out += "</b>";
            }
            else
            {
                out += "-";
            }
        }
        else
        {
            out += "-";
        }
        out += "\n  цель: ";
        out += "<b>";
        out += String(cfg->target_c, 1);
        out += "°";
        out += "</b>";
        out += "\n  гист: ";
        out += "<b>";
        out += String(cfg->hysteresis, 1);
        out += "°";
        out += "</b>";
        out += "\n  статус: ";
        out += "<b>";
        if (st->heat_on)
            out += F("🔥");
        else if (st->cool_on)
            out += F("❄️");
        else
            out += F("⏸");
        out += "</b>";
        out += "\n  питание: ";
        out += "<b>";
        out += st->power_on ? F("🟢") : F("⚪");
        out += "</b>";
        return out;
    }

    static const char *thermoModeLabel_(ThermoController::Mode mode)
    {
        switch (mode)
        {
        case ThermoController::Mode::Heat:
            return "Нагрев";
        case ThermoController::Mode::Cool:
            return "Охлаждение";
        case ThermoController::Mode::Auto:
            return "Авто";
        case ThermoController::Mode::Off:
        default:
            return "Выкл";
        }
    }

    static void sendThermoMenu_(TelegramMenu &self, int64_t chat_id)
    {
        if (!self._bot)
            return;
        if (!self.isLocalSelected_(chat_id))
        {
            self._bot->sendText(chat_id, F("Список доступен только для локального устройства"));
            return;
        }
        TelegramMenu::ChatAuth *st = self.ensureAuth_(chat_id);
        if (st)
        {
            st->awaiting_thermo = false;
            st->selected_thermo_id = 0;
        }
        std::vector<String> labels;
        TelegramMenuThermo::buildThermoLabels_(self, labels);
        const String markup = TelegramMenu::buildKeyboardMarkup_(labels);
        const String list = TelegramMenuThermo::thermoListTextHtml_(self);
        self._bot->setMenu(chat_id, "thermo");
        self._bot->sendText(chat_id, list, markup, "HTML");
    }

    static void sendThermoDevice_(TelegramMenu &self, int64_t chat_id, uint8_t id)
    {
        if (!self._bot)
            return;
        if (!self._thermo)
        {
            self._bot->sendText(chat_id, F("Термо недоступно"));
            return;
        }
        if (!self.isLocalSelected_(chat_id))
        {
            self._bot->sendText(chat_id, F("Доступно только для локального устройства"));
            return;
        }
        TelegramMenu::ChatAuth *st = self.ensureAuth_(chat_id);
        if (st)
        {
            st->awaiting_thermo = true;
            st->selected_thermo_id = id;
        }
        const String text = TelegramMenuThermo::thermoDeviceTextHtml_(self, id);
        const String markup = TelegramMenuThermo::thermoControlMarkup_();
        self._bot->sendText(chat_id, text, markup, "HTML");
    }

    static String thermoControlMarkup_()
    {
        std::vector<String> labels;
        labels.reserve(8);
        labels.push_back(F("Темп +"));
        labels.push_back(F("Темп -"));
        labels.push_back(F("Питание Вкл"));
        labels.push_back(F("Питание Выкл"));
        labels.push_back(F("Авто"));
        labels.push_back(F("Нагрев"));
        labels.push_back(F("Охлаждение"));
        labels.push_back(F("Назад"));
        return TelegramMenu::buildKeyboardMarkup_(labels);
    }

    static bool handleThermoAction_(TelegramMenu &self, const TelegramClient::Update &u)
    {
        TelegramMenu::ChatAuth *st = self.findAuth_(u.chat_id);
        if (!st || !st->awaiting_thermo)
            return false;
        if (u.text.startsWith("/"))
            return false;
        if (u.text == F("Назад"))
        {
            st->awaiting_thermo = false;
            st->selected_thermo_id = 0;
            TelegramMenuThermo::sendThermoMenu_(self, u.chat_id);
            return true;
        }
        if (!self._thermo)
        {
            self._bot->sendText(u.chat_id, F("Термо недоступно"));
            return true;
        }
        if (!self.isLocalSelected_(u.chat_id))
        {
            self._bot->sendText(u.chat_id, F("Доступно только для локального устройства"));
            return true;
        }
        const uint8_t id = st->selected_thermo_id;
        if (id == 0)
        {
            self._bot->sendText(u.chat_id, F("Не выбрано устройство"));
            return true;
        }
        bool handled = true;
        if (u.text == F("Темп +"))
        {
            const auto *cfg = self._thermo->config(id);
            if (cfg)
                self._thermo->setTarget(id, cfg->target_c + TelegramMenu::kThermoTargetStep);
        }
        else if (u.text == F("Темп -"))
        {
            const auto *cfg = self._thermo->config(id);
            if (cfg)
                self._thermo->setTarget(id, cfg->target_c - TelegramMenu::kThermoTargetStep);
        }
        else if (u.text == F("Питание Вкл"))
        {
            self._thermo->setPower(id, true, "tgbot");
        }
        else if (u.text == F("Питание Выкл"))
        {
            self._thermo->setPower(id, false, "tgbot");
        }
        else if (u.text == F("Авто"))
        {
            self._thermo->setMode(id, ThermoController::Mode::Auto);
        }
        else if (u.text == F("Нагрев"))
        {
            self._thermo->setMode(id, ThermoController::Mode::Heat);
        }
        else if (u.text == F("Охлаждение"))
        {
            self._thermo->setMode(id, ThermoController::Mode::Cool);
        }
        else
        {
            handled = false;
        }
        if (!handled)
        {
            self._bot->sendText(u.chat_id, F("Неизвестная команда"));
            return true;
        }
        TelegramMenuThermo::sendThermoDevice_(self, u.chat_id, id);
        return true;
    }

    static bool handleThermoSelection_(TelegramMenu &self, const TelegramClient::Update &u)
    {
        if (!self._bot)
            return false;
        const char *menu_id = self._bot->currentMenuId(u.chat_id);
        if (!menu_id || strcmp(menu_id, "thermo") != 0)
            return false;
        if (u.text.startsWith("/"))
            return false;
        if (u.text == F("Назад"))
        {
            self._bot->enterMenu(u.chat_id, "device");
            return true;
        }
        uint8_t id = 0;
        if (!TelegramMenuThermo::parseThermoLabel_(u.text, id))
        {
            self._bot->sendText(u.chat_id, F("Неизвестное устройство"));
            return true;
        }
        if (!self._thermo)
        {
            self._bot->sendText(u.chat_id, F("Термо недоступно"));
            return true;
        }
        if (!self.isLocalSelected_(u.chat_id))
        {
            self._bot->sendText(u.chat_id, F("Доступно только для локального устройства"));
            return true;
        }
        TelegramMenuThermo::sendThermoDevice_(self, u.chat_id, id);
        return true;
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
        return TelegramMenuThermo::parseThermoId_(num, out);
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
            return TelegramMenuThermo::parseThermoIdFromText_(head, out);
        }
        String low = t;
        low.toLowerCase();
        if (low.startsWith("thermo"))
        {
            String tail = t.substring(6);
            tail.trim();
            return TelegramMenuThermo::parseThermoIdFromText_(tail, out);
        }
        return TelegramMenuThermo::parseThermoIdFromText_(t, out);
    }
};
