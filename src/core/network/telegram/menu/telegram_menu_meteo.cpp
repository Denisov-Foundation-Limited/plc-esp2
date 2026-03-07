#include "core/network/telegram/telegram_menu.hpp"

#include <stdlib.h>
#include <string.h>

#include "controllers/meteo_controller.hpp"
#include "core/network/stack/stack_cache.hpp"
#include "core/network/stack/stack_features.hpp"
#include "core/network/stack/stack_master.hpp"
#include "core/network/wifi_manager.hpp"
#include "core/rtc.hpp"
#include "plc/plc_control.hpp"
#include "utils/configs.hpp"
#include "utils/logger.hpp"

#include "core/network/telegram/menu/telegram_menu_meteo.hpp"

// ---- telegram menu extracted definitions ----
bool TelegramMenuMeteo::cmdMeteo_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        (void)reply;
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        if (TelegramMenu::_self->isLocalSelected_(u.chat_id) && !TelegramMenu::_self->_meteo)
        {
            reply = "Метео недоступно";
            return true;
        }
        TelegramMenuMeteo::sendMeteoMenu_(*TelegramMenu::_self, u.chat_id);
        return true;
    }


    bool TelegramMenuMeteo::cmdMeteoList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        if (TelegramMenu::_self->isLocalSelected_(u.chat_id) && !TelegramMenu::_self->_meteo)
        {
            reply = "Метео недоступно";
            return true;
        }
        const String text = TelegramMenuMeteo::meteoListTextHtml_(*TelegramMenu::_self, u.chat_id);
        bot.sendText(u.chat_id, text, "", "HTML");
        return true;
    }


    bool TelegramMenuMeteo::cmdMeteoShow_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        if (TelegramMenu::_self->isLocalSelected_(u.chat_id) && !TelegramMenu::_self->_meteo)
        {
            reply = "Метео недоступно";
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
        const String text = TelegramMenuMeteo::meteoSensorTextHtml_(*TelegramMenu::_self, u.chat_id, id);
        bot.sendText(u.chat_id, text, "", "HTML");
        return true;
    }


    void TelegramMenuMeteo::buildMeteoLabels_(TelegramMenu &self, std::vector<String> &out)
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
            String name;
            if (self._meteo->displayName(cfg->id, name) && name.length())
                label += name;
            else
                label += String("Sensor ") + String((unsigned)cfg->id);
            out.push_back(label);
        }
        out.push_back(F("Назад"));
    }


    String TelegramMenuMeteo::meteoListTextHtml_(TelegramMenu &self, int64_t chat_id)
    {
        const bool groups_enabled = self.isLocalSelected_(chat_id) ? (self._meteo && self.hasLocalGroups_())
                                                                   : (self._stack_cache && self.hasGroups_(chat_id));
        const bool group_active = groups_enabled && self.groupFilterActive_(chat_id, "meteo");
        String out = F("<b>Метео:</b>");
        out.reserve(768);
        if (self.isLocalSelected_(chat_id))
        {
            if (!self._meteo)
            {
                out += F("\n  недоступно");
                return out;
            }
            if (groups_enabled && !group_active)
            {
                bool any = false;
                bool has_no_group = false;
                for (size_t gi = 0; gi < self._configs_manager->groupCount(); ++gi)
                {
                    ConfigsManagerIface::GroupConfig g;
                    if (!self._configs_manager->groupByIndex(gi, g) || g.id == 0)
                        continue;
                    bool group_any = false;
                    for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
                    {
                        const auto *cfg = self._meteo->configByIndex(i);
                        const auto *st = self._meteo->stateByIndex(i);
                        if (!cfg || !st || !cfg->enabled)
                            continue;
                        if (cfg->group_id == 0)
                            has_no_group = true;
                        if (cfg->group_id != g.id)
                            continue;
                        if (!group_any)
                        {
                            out += F("\n  [");
                            out += self.escapeHtml_(g.name);
                            out += F("]");
                            group_any = true;
                            any = true;
                        }
                        out += F("\n    ");
                        out += String((unsigned)cfg->id);
                        out += F(": ");
                        String name;
                        if (self._meteo->displayName(cfg->id, name) && name.length())
                        {
                            out += "<b>";
                            out += self.escapeHtml_(name);
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
                            out += " Т: <b>";
                            out += buf;
                            out += "°</b>";
                        }
                        if (st->has_humidity)
                        {
                            char buf[10] = {};
                            dtostrf(st->humidity, 0, 1, buf);
                            out += " В: <b>";
                            out += buf;
                            out += "%</b>";
                        }
                        if (!st->has_temp && !st->has_humidity)
                            out += " -";
                    }
                }
                if (has_no_group)
                {
                    out += F("\n  [Без группы]");
                    any = true;
                    for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
                    {
                        const auto *cfg = self._meteo->configByIndex(i);
                        const auto *st = self._meteo->stateByIndex(i);
                        if (!cfg || !st || !cfg->enabled || cfg->group_id != 0)
                            continue;
                        out += F("\n    ");
                        out += String((unsigned)cfg->id);
                        out += F(": ");
                        String name;
                        if (self._meteo->displayName(cfg->id, name) && name.length())
                        {
                            out += "<b>";
                            out += self.escapeHtml_(name);
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
                            out += " Т: <b>";
                            out += buf;
                            out += "°</b>";
                        }
                        if (st->has_humidity)
                        {
                            char buf[10] = {};
                            dtostrf(st->humidity, 0, 1, buf);
                            out += " В: <b>";
                            out += buf;
                            out += "%</b>";
                        }
                        if (!st->has_temp && !st->has_humidity)
                            out += " -";
                    }
                }
                if (!any)
                    out += F("\n  пусто");
                return out;
            }
            if (groups_enabled && group_active)
            {
                out += F("\n  группа: <b>");
                out += self.escapeHtml_(self.groupLabelById_(chat_id, self.groupFilterId_(chat_id, "meteo")));
                out += F("</b>");
            }
            bool any = false;
            const uint8_t active_group_id = groups_enabled ? self.groupFilterId_(chat_id, "meteo") : 0;
            for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
            {
                const auto *cfg = self._meteo->configByIndex(i);
                const auto *st = self._meteo->stateByIndex(i);
                if (!cfg || !st || !cfg->enabled)
                    continue;
                if (groups_enabled && cfg->group_id != active_group_id)
                    continue;
                any = true;
                out += "\n  ";
                out += String((unsigned)cfg->id);
                out += ": ";
                String name;
                if (self._meteo->displayName(cfg->id, name) && name.length())
                {
                    out += "<b>";
                    out += self.escapeHtml_(name);
                    out += "</b>";
                }
                else
                    out += "<b>-</b>";
                if (st->has_temp)
                {
                    char buf[10] = {};
                    dtostrf(st->temp_c, 0, 1, buf);
                    out += " Т: ";
                    out += "<b>";
                    out += buf;
                    out += "°";
                    out += "</b>";
                }
                if (st->has_humidity)
                {
                    char buf[10] = {};
                    dtostrf(st->humidity, 0, 1, buf);
                    out += " В: ";
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

        if (!self._stack_cache)
        {
            out += F("\n  недоступно");
            return out;
        }
        const uint32_t node_id = self.selectedNodeId_(chat_id);
        if (node_id == 0)
        {
            out += F("\n  недоступно");
            return out;
        }
        const auto *cache = self._stack_cache->meteoCache(node_id);
        if (!cache || !cache->has_data)
        {
            self._stack_cache->requestMeteo(node_id);
            out += F("\n  обновление...");
            return out;
        }
        if (groups_enabled && !group_active)
        {
            bool any_group = false;
            bool has_no_group = false;
            const auto *groups = self.selectedGroupsCache_(chat_id);
            if (groups && groups->has_data && groups->items)
            {
                for (size_t gi = 0; gi < groups->item_count; ++gi)
                {
                    const auto &g = groups->items[gi];
                    if (g.id == 0 || !g.name[0])
                        continue;
                    bool section = false;
                    for (size_t i = 0; i < cache->item_count; ++i)
                    {
                        const auto &it = cache->items[i];
                        if (!it.enabled)
                            continue;
                        if (it.group_id == 0)
                            has_no_group = true;
                        if (it.group_id != g.id)
                            continue;
                        if (!section)
                        {
                            out += F("\n  [");
                            out += self.escapeHtml_(String(g.name));
                            out += F("]");
                            section = true;
                            any_group = true;
                        }
                        out += "\n    ";
                        out += String((unsigned)it.id);
                        out += F(": ");
                        out += it.name[0] ? String("<b>") + self.escapeHtml_(String(it.name)) + "</b>" : String("<b>-</b>");
                        if (it.has_temp)
                            out += String(" Рў: <b>") + String(it.temp_c, 1) + "В°</b>";
                        if (it.has_hum)
                            out += String(" Р’: <b>") + String(it.hum, 1) + "%</b>";
                        if (!it.has_temp && !it.has_hum)
                            out += " -";
                    }
                }
            }
            if (has_no_group)
            {
                out += F("\n  [Р‘РµР· РіСЂСѓРїРїС‹]");
                any_group = true;
                for (size_t i = 0; i < cache->item_count; ++i)
                {
                    const auto &it = cache->items[i];
                    if (!it.enabled || it.group_id != 0)
                        continue;
                    out += "\n    ";
                    out += String((unsigned)it.id);
                    out += F(": ");
                    out += it.name[0] ? String("<b>") + self.escapeHtml_(String(it.name)) + "</b>" : String("<b>-</b>");
                    if (it.has_temp)
                        out += String(" Рў: <b>") + String(it.temp_c, 1) + "В°</b>";
                    if (it.has_hum)
                        out += String(" Р’: <b>") + String(it.hum, 1) + "%</b>";
                    if (!it.has_temp && !it.has_hum)
                        out += " -";
                }
            }
            if (!any_group)
                out += F("\n  РїСѓСЃС‚Рѕ");
            return out;
        }
        if (groups_enabled && group_active)
        {
            out += F("\n  РіСЂСѓРїРїР°: <b>");
            out += self.escapeHtml_(self.groupLabelById_(chat_id, self.groupFilterId_(chat_id, "meteo")));
            out += F("</b>");
        }
        bool any = false;
        for (size_t i = 0; i < cache->item_count; ++i)
        {
            const auto &it = cache->items[i];
            if (!it.enabled)
                continue;
            if (groups_enabled && it.group_id != self.groupFilterId_(chat_id, "meteo"))
                continue;
            any = true;
            out += "\n  ";
            out += String((unsigned)it.id);
            out += ": ";
            if (it.name[0])
            {
                out += "<b>";
                out += self.escapeHtml_(String(it.name));
                out += "</b>";
            }
            else
            {
                out += "<b>-</b>";
            }
            if (it.has_temp)
            {
                out += " Т: <b>";
                out += String(it.temp_c, 1);
                out += "°</b>";
            }
            if (it.has_hum)
            {
                out += " В: <b>";
                out += String(it.hum, 1);
                out += "%</b>";
            }
            if (!it.has_temp && !it.has_hum)
                out += " -";
        }
        if (!any)
            out += F("\n  пусто");
        return out;
    }


    String TelegramMenuMeteo::meteoSensorTextHtml_(TelegramMenu &self, int64_t chat_id, uint8_t id)
    {
        if (self.isLocalSelected_(chat_id))
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
            String name;
            if (self._meteo->displayName(cfg->id, name) && name.length())
            {
                out += "<b>";
                out += self.escapeHtml_(name);
                out += "</b>";
            }
            else
                out += "<b>-</b>";
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

        if (!self._stack_cache)
            return F("Метео недоступно");
        const uint32_t node_id = self.selectedNodeId_(chat_id);
        if (node_id == 0)
            return F("Метео недоступно");
        const auto *cache = self._stack_cache->meteoCache(node_id);
        if (!cache || !cache->has_data)
        {
            self._stack_cache->requestMeteo(node_id);
            return F("Обновление данных...");
        }
        const StackCache::StackMeteoItem *found = nullptr;
        for (size_t i = 0; i < cache->item_count; ++i)
        {
            if (cache->items[i].id == id && cache->items[i].enabled)
            {
                found = &cache->items[i];
                break;
            }
        }
        if (!found)
            return F("Неверный датчик");
        String out = F("<b>Датчик метео:</b>");
        out.reserve(320);
        out += "\n  имя: <b>";
        out += found->name[0] ? self.escapeHtml_(String(found->name)) : String("-");
        out += "</b>";
        out += "\n  тип: <b>";
        out += found->type[0] ? String(found->type) : String("-");
        out += "</b>";
        if (found->addr[0])
        {
            out += "\n  addr: <b>";
            out += String(found->addr);
            out += "</b>";
        }
        out += "\n  темп: ";
        if (found->has_temp)
            out += String("<b>") + String(found->temp_c, 1) + "°</b>";
        else
            out += "<b>-</b>";
        out += "\n  влажн: ";
        if (found->has_hum)
            out += String("<b>") + String(found->hum, 1) + "%</b>";
        else
            out += "<b>-</b>";
        out += "\n  статус: <b>";
        out += found->ok ? "OK" : "ERR";
        out += "</b>";
        return out;
    }


    String TelegramMenuMeteo::meteoHistoryTextHtml_(TelegramMenu &self, uint8_t id)
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
        std::array<int16_t, 24> temp10{};
        std::array<int16_t, 24> hum10{};
        std::array<bool, 24> has_temp{};
        std::array<bool, 24> has_hum{};
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
            temp10[hour] = t10;
            hum10[hour] = h10;
            has_temp[hour] = (t10 != (int16_t)0x7FFF);
            has_hum[hour] = (h10 != (int16_t)0x7FFF);
        }

        String out;
        bool any_temp = false;
        bool any_hum = false;
        for (uint8_t hour = 0; hour < 24; ++hour)
        {
            if (!has_temp[hour])
                continue;
            if (!any_temp)
                out += "\n  история (Т):";
            any_temp = true;
            out += "\n   ";
            if (hour < 10)
                out += "0";
            out += String((unsigned)hour);
            out += ":00 ";
            const uint8_t bars = TelegramMenuMeteo::scaleBars_((float)temp10[hour] / 10.0f, 40.0f);
            out += "Т";
            out += TelegramMenuMeteo::barString_(bars);
            out += " <b>";
            out += String((float)temp10[hour] / 10.0f, 1);
            out += "°</b>";
        }
        for (uint8_t hour = 0; hour < 24; ++hour)
        {
            if (!has_hum[hour])
                continue;
            if (!any_hum)
                out += "\n  история (В):";
            any_hum = true;
            out += "\n   ";
            if (hour < 10)
                out += "0";
            out += String((unsigned)hour);
            out += ":00 ";
            const uint8_t bars = TelegramMenuMeteo::scaleBars_((float)hum10[hour] / 10.0f, 100.0f);
            out += "В";
            out += TelegramMenuMeteo::barString_(bars);
            out += " <b>";
            out += String((float)hum10[hour] / 10.0f, 1);
            out += "%</b>";
        }
        f.close();
        if (!any_temp && !any_hum)
            return "";
        return out;
    }


    uint8_t TelegramMenuMeteo::scaleBars_(float v, float max_v)
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


    String TelegramMenuMeteo::barString_(uint8_t bars)
    {
        String out;
        out.reserve(30);
        for (uint8_t i = 0; i < 10; ++i)
            out += (i < bars) ? "█" : "░";
        return out;
    }


    void TelegramMenuMeteo::sendMeteoMenu_(TelegramMenu &self, int64_t chat_id)
    {
        if (!self._bot)
            return;
        std::vector<String> labels;
        if (self.isLocalSelected_(chat_id))
        {
            const bool groups_enabled = self._meteo && self.hasLocalGroups_();
            const bool group_active = groups_enabled && self.groupFilterActive_(chat_id, "meteo");
            if (groups_enabled && !group_active)
            {
                bool has_no_group = false;
                for (size_t gi = 0; gi < self._configs_manager->groupCount(); ++gi)
                {
                    ConfigsManagerIface::GroupConfig g;
                    if (!self._configs_manager->groupByIndex(gi, g) || g.id == 0)
                        continue;
                    bool present = false;
                    for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
                    {
                        const auto *cfg = self._meteo->configByIndex(i);
                        if (!cfg || !cfg->enabled)
                            continue;
                        if (cfg->group_id == g.id)
                        {
                            present = true;
                            break;
                        }
                        if (cfg->group_id == 0)
                            has_no_group = true;
                    }
                    if (present)
                        labels.push_back(g.name);
                }
                if (has_no_group)
                    labels.push_back(F("Без группы"));
                labels.push_back(F("Назад"));
            }
            else
            {
                const uint8_t active_group_id = groups_enabled ? self.groupFilterId_(chat_id, "meteo") : 0;
                for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
                {
                    const auto *cfg = self._meteo->configByIndex(i);
                    if (!cfg || !cfg->enabled)
                        continue;
                    if (groups_enabled && cfg->group_id != active_group_id)
                        continue;
                    String label;
                    label += String((unsigned)cfg->id);
                    label += ": ";
                    String name;
                    if (self._meteo->displayName(cfg->id, name) && name.length())
                        label += name;
                    else
                        label += String("Sensor ") + String((unsigned)cfg->id);
                    labels.push_back(label);
                }
                if (groups_enabled)
                    labels.push_back(F("Группы"));
                labels.push_back(F("Назад"));
            }
        }
        else if (self._stack_cache)
        {
            const uint32_t node_id = self.selectedNodeId_(chat_id);
            const bool groups_enabled = self.hasGroups_(chat_id);
            const bool group_active = groups_enabled && self.groupFilterActive_(chat_id, "meteo");
            if (node_id != 0)
            {
                const auto *cache = self._stack_cache->meteoCache(node_id);
                if (groups_enabled && !group_active)
                {
                    bool has_no_group = false;
                    const auto *groups = self.selectedGroupsCache_(chat_id);
                    if (!cache || !cache->has_data)
                    {
                        self._stack_cache->requestMeteo(node_id);
                    }
                    else if (groups && groups->has_data && groups->items)
                    {
                        for (size_t gi = 0; gi < groups->item_count; ++gi)
                        {
                            const auto &g = groups->items[gi];
                            if (g.id == 0 || !g.name[0])
                                continue;
                            bool present = false;
                            for (size_t i = 0; i < cache->item_count; ++i)
                            {
                                const auto &it = cache->items[i];
                                if (!it.enabled)
                                    continue;
                                if (it.group_id == g.id)
                                {
                                    present = true;
                                    break;
                                }
                                if (it.group_id == 0)
                                    has_no_group = true;
                            }
                            if (present)
                                labels.push_back(String(g.name));
                        }
                    }
                    if (has_no_group)
                        labels.push_back(F("Р‘РµР· РіСЂСѓРїРїС‹"));
                }
                else if (!cache || !cache->has_data)
                {
                    self._stack_cache->requestMeteo(node_id);
                }
                else
                {
                    const uint8_t active_group_id = groups_enabled ? self.groupFilterId_(chat_id, "meteo") : 0;
                    for (size_t i = 0; i < cache->item_count; ++i)
                    {
                        const auto &it = cache->items[i];
                        if (!it.enabled)
                            continue;
                        if (groups_enabled && it.group_id != active_group_id)
                            continue;
                        String label = String((unsigned)it.id) + ": ";
                        label += it.name[0] ? String(it.name) : String("Sensor ") + String((unsigned)it.id);
                        labels.push_back(label);
                    }
                    if (groups_enabled)
                        labels.push_back(F("Р“СЂСѓРїРїС‹"));
                }
            }
            labels.push_back(F("Назад"));
        }
        else
        {
            labels.push_back(F("Назад"));
        }
        const String markup = TelegramMenu::buildKeyboardMarkup_(labels);
        const String list = TelegramMenuMeteo::meteoListTextHtml_(self, chat_id);
        self._bot->setMenu(chat_id, "meteo");
        self._bot->sendText(chat_id, list, markup, "HTML");
    }


    bool TelegramMenuMeteo::handleMeteoSelection_(TelegramMenu &self, const TelegramClient::Update &u)
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
            self.clearGroupFilter_(u.chat_id, "meteo");
            self._bot->enterMenu(u.chat_id, "device");
            return true;
        }
        if (((self.isLocalSelected_(u.chat_id) && self._meteo) || (!self.isLocalSelected_(u.chat_id) && self._stack_cache)) &&
            self.hasGroups_(u.chat_id))
        {
            if (!self.groupFilterActive_(u.chat_id, "meteo"))
            {
                uint8_t group_id = 0;
                if (!self.parseGroupLabel_(u.chat_id, u.text, group_id))
                {
                    self._bot->sendText(u.chat_id, F("Неизвестная группа"));
                    return true;
                }
                self.setGroupFilter_(u.chat_id, "meteo", group_id);
                TelegramMenuMeteo::sendMeteoMenu_(self, u.chat_id);
                return true;
            }
            if (u.text == F("Группы"))
            {
                self.clearGroupFilter_(u.chat_id, "meteo");
                TelegramMenuMeteo::sendMeteoMenu_(self, u.chat_id);
                return true;
            }
        }
        uint8_t id = 0;
        if (!TelegramMenuMeteo::parseMeteoLabel_(u.text, id))
        {
            self._bot->sendText(u.chat_id, F("Неизвестный датчик"));
            return true;
        }
        if (self.isLocalSelected_(u.chat_id) && !self._meteo)
        {
            self._bot->sendText(u.chat_id, F("Метео недоступно"));
            return true;
        }
        const String text = TelegramMenuMeteo::meteoSensorTextHtml_(self, u.chat_id, id);
        self._bot->sendText(u.chat_id, text, "", "HTML");
        return true;
    }


    bool TelegramMenuMeteo::parseMeteoIdFromText_(const String &text, uint8_t &out)
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


    bool TelegramMenuMeteo::parseMeteoId_(const String &text, uint8_t &out)
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


    bool TelegramMenuMeteo::parseMeteoLabel_(const String &text, uint8_t &out)
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
