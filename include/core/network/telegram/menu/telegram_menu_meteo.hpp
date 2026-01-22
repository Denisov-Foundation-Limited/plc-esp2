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

class TelegramMenuMeteo
{
public:
    static bool cmdMeteo_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        (void)reply;
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        if (!TelegramMenu::_self->_meteo)
        {
            reply = "Метео недоступно";
            return true;
        }
        if (!TelegramMenu::_self->isLocalSelected_(u.chat_id))
        {
            reply = "Список доступен только для локального устройства";
            return true;
        }
        TelegramMenuMeteo::sendMeteoMenu_(*TelegramMenu::_self, u.chat_id);
        return true;
    }

    static bool cmdMeteoList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        if (!TelegramMenu::_self->_meteo)
        {
            reply = "Метео недоступно";
            return true;
        }
        if (!TelegramMenu::_self->isLocalSelected_(u.chat_id))
        {
            reply = "Список доступен только для локального устройства";
            return true;
        }
        const String text = TelegramMenuMeteo::meteoListTextHtml_(*TelegramMenu::_self);
        bot.sendText(u.chat_id, text, "", "HTML");
        return true;
    }

    static bool cmdMeteoShow_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        if (!TelegramMenu::_self->_meteo)
        {
            reply = "Метео недоступно";
            return true;
        }
        if (!TelegramMenu::_self->isLocalSelected_(u.chat_id))
        {
            reply = "Список доступен только для локального устройства";
            return true;
        }
        const char *cmd = "/meteo_show";
        String tail = u.text.substring(strlen(cmd));
        tail.trim();
        uint8_t id = 0;
        if (!TelegramMenuMeteo::parseMeteoId_(tail, id))
        {
            reply = "Использование: /meteo_show <id>";
            return true;
        }
        const String text = TelegramMenuMeteo::meteoSensorTextHtml_(*TelegramMenu::_self, id);
        bot.sendText(u.chat_id, text, "", "HTML");
        return true;
    }

    static void buildMeteoLabels_(TelegramMenu &self, std::vector<String> &out)
    {
        out.clear();
        if (!self._meteo)
        {
            out.reserve(1);
            out.push_back(F("Назад"));
            return;
        }
        out.reserve(MeteoController::kSensorCount + 1);
        for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
        {
            const auto *cfg = self._meteo->configByIndex(i);
            if (!cfg || !cfg->enabled)
                continue;
            String label;
            label += String((unsigned)cfg->id);
            label += ": ";
            if (cfg->name.length())
            {
                label += cfg->name;
            }
            else
            {
                label += F("Sensor");
                label += String((unsigned)cfg->id);
            }
            out.push_back(label);
        }
        out.push_back(F("Назад"));
    }

    static String meteoListTextHtml_(TelegramMenu &self)
    {
        String out = F("<b>Метео:</b>");
        out.reserve(768);
        if (!self._meteo)
        {
            out += F("\n  недоступно");
            return out;
        }
        bool any = false;
        for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
        {
            const auto *cfg = self._meteo->configByIndex(i);
            const auto *st = self._meteo->stateByIndex(i);
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
            if (st->has_temp)
            {
                char buf[10] = {};
                dtostrf(st->temp_c, 0, 1, buf);
                out += " Темп: ";
                out += "<b>";
                out += buf;
                out += "°";
                out += "</b>";
            }
            if (st->has_humidity)
            {
                char buf[10] = {};
                dtostrf(st->humidity, 0, 1, buf);
                out += " Влажн: ";
                out += "<b>";
                out += buf;
                out += "%";
                out += "</b>";
            }
            if (!st->has_temp && !st->has_humidity)
                out += " -";
        }
        if (!any)
            out += F("\n  пусто");
        return out;
    }

    static String meteoSensorTextHtml_(TelegramMenu &self, uint8_t id)
    {
        if (!self._meteo)
            return F("Метео недоступно");
        const auto *cfg = self._meteo->config(id);
        const auto *st = self._meteo->state(id);
        if (!cfg || !st)
            return F("Неверный датчик");
        String out = F("<b>Датчик метео:</b>");
        out.reserve(384);
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
        out += "\n  тип: ";
        out += "<b>";
        out += MeteoController::typeName(cfg->type);
        out += "</b>";
        if (cfg->type == MeteoController::SensorType::Ds18b20)
        {
            out += "\n  addr: ";
            if (cfg->ds18_addr_set)
            {
                char hex[17] = {};
                MeteoController::formatHexAddr(cfg->ds18_addr, hex);
                out += "<b>";
                out += hex;
                out += "</b>";
            }
            else
            {
                out += "<b>-</b>";
            }
        }
        out += "\n  темп: ";
        if (st->has_temp)
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
            out += "<b>-</b>";
        }
        out += "\n  влажн: ";
        if (st->has_humidity)
        {
            char buf[10] = {};
            dtostrf(st->humidity, 0, 1, buf);
            out += "<b>";
            out += buf;
            out += "%";
            out += "</b>";
        }
        else
        {
            out += "<b>-</b>";
        }
        out += "\n  статус: ";
        if (st->last_read_ms == 0)
            out += "<b>-</b>";
        else
        {
            out += "<b>";
            out += st->ok ? "OK" : "ERR";
            out += "</b>";
        }
        const String hist = TelegramMenuMeteo::meteoHistoryTextHtml_(self, id);
        if (hist.length())
            out += hist;
        return out;
    }

    static String meteoHistoryTextHtml_(TelegramMenu &self, uint8_t id)
    {
        Ds3231Mz::DateTime dt{};
        if (!self._rtc.Time(dt))
            return "";
        const uint32_t date = (uint32_t)dt.year * 10000u + (uint32_t)dt.month * 100u + (uint32_t)dt.day;
        if (!LittleFS.exists(MeteoHistory::kPath))
            return "";
        File f = LittleFS.open(MeteoHistory::kPath, "r");
        if (!f)
            return "";
        uint32_t magic = 0;
        uint32_t stored_date = 0;
        if (f.read(reinterpret_cast<uint8_t *>(&magic), sizeof(magic)) != sizeof(magic) ||
            f.read(reinterpret_cast<uint8_t *>(&stored_date), sizeof(stored_date)) != sizeof(stored_date))
        {
            f.close();
            return "";
        }
        if (magic != TelegramMenu::kHistoryMagic || stored_date != date)
        {
            f.close();
            return "";
        }
        if (id == 0 || id > MeteoController::kSensorCount)
        {
            f.close();
            return "";
        }
        const uint8_t sensor_index = (uint8_t)(id - 1);
        String out;
        bool any = false;
        for (uint8_t hour = 0; hour < 24; ++hour)
        {
            const size_t index = (size_t)sensor_index * 24u + hour;
            const size_t off = sizeof(uint32_t) + sizeof(uint32_t) + index * sizeof(int16_t) * 2;
            if (!f.seek(off, SeekSet))
                break;
            int16_t t10 = 0;
            int16_t h10 = 0;
            if (f.read(reinterpret_cast<uint8_t *>(&t10), sizeof(t10)) != sizeof(t10) ||
                f.read(reinterpret_cast<uint8_t *>(&h10), sizeof(h10)) != sizeof(h10))
                break;
            const bool has_temp = t10 != (int16_t)0x7FFF;
            const bool has_hum = h10 != (int16_t)0x7FFF;
            if (!has_temp && !has_hum)
                continue;
            if (!any)
                out += "\n  история (Темп/Влажн):";
            any = true;
            out += "\n   ";
            if (hour < 10)
                out += "0";
            out += String((unsigned)hour);
            out += ":00 ";
            if (has_temp)
            {
                const uint8_t bars = TelegramMenuMeteo::scaleBars_((float)t10 / 10.0f, 40.0f);
                out += "Т";
                out += TelegramMenuMeteo::barString_(bars);
                out += " <b>";
                out += String((float)t10 / 10.0f, 1);
                out += "°</b> ";
            }
            if (has_hum)
            {
                const uint8_t bars = TelegramMenuMeteo::scaleBars_((float)h10 / 10.0f, 100.0f);
                out += "| В";
                out += TelegramMenuMeteo::barString_(bars);
                out += " <b>";
                out += String((float)h10 / 10.0f, 1);
                out += "%</b>";
            }
        }
        f.close();
        return out;
    }

    static uint8_t scaleBars_(float v, float max_v)
    {
        if (max_v <= 0.0f)
            return 0;
        if (v < 0.0f)
            v = 0.0f;
        if (v > max_v)
            v = max_v;
        const float ratio = v / max_v;
        const uint8_t bars = (uint8_t)lroundf(ratio * 10.0f);
        return (bars > 10) ? 10 : bars;
    }

    static String barString_(uint8_t bars)
    {
        String out;
        out.reserve(30);
        for (uint8_t i = 0; i < 10; ++i)
            out += (i < bars) ? "█" : "░";
        return out;
    }

    static void sendMeteoMenu_(TelegramMenu &self, int64_t chat_id)
    {
        if (!self._bot)
            return;
        if (!self.isLocalSelected_(chat_id))
        {
            self._bot->sendText(chat_id, F("Список доступен только для локального устройства"));
            return;
        }
        std::vector<String> labels;
        TelegramMenuMeteo::buildMeteoLabels_(self, labels);
        const String markup = TelegramMenu::buildKeyboardMarkup_(labels);
        const String list = TelegramMenuMeteo::meteoListTextHtml_(self);
        self._bot->setMenu(chat_id, "meteo");
        self._bot->sendText(chat_id, list, markup, "HTML");
    }

    static bool handleMeteoSelection_(TelegramMenu &self, const TelegramClient::Update &u)
    {
        if (!self._bot)
            return false;
        const char *menu_id = self._bot->currentMenuId(u.chat_id);
        if (!menu_id || strcmp(menu_id, "meteo") != 0)
            return false;
        if (u.text.startsWith("/"))
            return false;
        if (u.text == F("Назад"))
        {
            self._bot->enterMenu(u.chat_id, "device", self.adminPrefix_(u.chat_id));
            return true;
        }
        uint8_t id = 0;
        if (!TelegramMenuMeteo::parseMeteoLabel_(u.text, id))
        {
            self._bot->sendText(u.chat_id, F("Неизвестный датчик"));
            return true;
        }
        if (!self._meteo)
        {
            self._bot->sendText(u.chat_id, F("Метео недоступно"));
            return true;
        }
        if (!self.isLocalSelected_(u.chat_id))
        {
            self._bot->sendText(u.chat_id, F("Доступно только для локального устройства"));
            return true;
        }
        const String text = TelegramMenuMeteo::meteoSensorTextHtml_(self, id);
        self._bot->sendText(u.chat_id, text, "", "HTML");
        return true;
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
        return TelegramMenuMeteo::parseMeteoId_(num, out);
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
            return TelegramMenuMeteo::parseMeteoIdFromText_(head, out);
        }
        String low = t;
        low.toLowerCase();
        if (low.startsWith("meteo"))
        {
            String tail = t.substring(5);
            tail.trim();
            return TelegramMenuMeteo::parseMeteoIdFromText_(tail, out);
        }
        if (low.startsWith("sensor"))
        {
            String tail = t.substring(6);
            tail.trim();
            return TelegramMenuMeteo::parseMeteoIdFromText_(tail, out);
        }
        return TelegramMenuMeteo::parseMeteoIdFromText_(t, out);
    }
};
