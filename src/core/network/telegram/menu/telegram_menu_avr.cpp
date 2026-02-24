#include "core/network/telegram/telegram_menu.hpp"

#include <stdlib.h>
#include <string.h>

#include "controllers/avr_controller.hpp"
#include "core/network/stack/stack_cache.hpp"
#include "core/network/stack/stack_features.hpp"
#include "core/network/stack/stack_master.hpp"
#include "core/network/wifi_manager.hpp"
#include "core/rtc.hpp"
#include "plc/plc_control.hpp"
#include "utils/configs.hpp"
#include "utils/logger.hpp"

#include "core/network/telegram/menu/telegram_menu_avr.hpp"

// ---- telegram menu extracted definitions ----
bool TelegramMenuAvr::cmdAvr_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        (void)reply;
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        if (TelegramMenu::_self->isLocalSelected_(u.chat_id) && !TelegramMenu::_self->_avr)
        {
            reply = "АВР недоступен";
            return true;
        }
        TelegramMenuAvr::sendAvrMenu_(*TelegramMenu::_self, u.chat_id);
        return true;
    }


    bool TelegramMenuAvr::cmdAvrStatus_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        (void)bot;
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        if (TelegramMenu::_self->isLocalSelected_(u.chat_id) && !TelegramMenu::_self->_avr)
        {
            reply = "АВР недоступен";
            return true;
        }
        reply = TelegramMenuAvr::avrStatusTextHtml_(*TelegramMenu::_self, u.chat_id);
        return true;
    }


    const char *TelegramMenuAvr::sourceRuByName_(const String &src)
    {
        String s = src;
        s.toLowerCase();
        if (s == "main")
            return "основной";
        if (s == "reserve")
            return "резерв";
        return "выкл";
    }


    const char *TelegramMenuAvr::sourceRu_(AvrController::Source src)
    {
        return sourceRuByName_(String(AvrController::sourceName(src)));
    }


    const char *TelegramMenuAvr::faultRuByName_(const String &fault)
    {
        String f = fault;
        f.toLowerCase();
        if (f == "none")
            return "нет";
        if (f == "no_source")
            return "нет источника";
        if (f == "transfer_timeout")
            return "таймаут переключения";
        if (f == "interlock")
            return "блокировка";
        if (f == "feedback_mismatch")
            return "ошибка обратной связи";
        return "неизвестно";
    }


    const char *TelegramMenuAvr::faultRu_(AvrController::Fault fault)
    {
        return faultRuByName_(String(AvrController::faultName(fault)));
    }


    String TelegramMenuAvr::avrStatusTextHtml_(TelegramMenu &self, int64_t chat_id)
    {
        String out = F("<b>АВР:</b>");
        out.reserve(640);
        if (self.isLocalSelected_(chat_id))
        {
            if (!self._avr)
                return F("АВР недоступен");
            const auto &cfg = self._avr->config();
            const auto &st = self._avr->state();
            out += F("\n  авто: <b>");
            out += cfg.auto_mode ? "🟢" : "⚪";
            out += F("</b>");
            out += F("\n  источник: <b>");
            out += TelegramMenuAvr::sourceRu_(st.active_source);
            out += F("</b>");
            out += F("\n  цель: <b>");
            out += TelegramMenuAvr::sourceRu_(st.target_source);
            out += F("</b>");
            out += F("\n  сеть основная: <b>");
            out += st.main_ok ? "🟢" : "⚪";
            out += F("</b>");
            out += F("\n  сеть резерв: <b>");
            out += st.reserve_ok ? "🟢" : "⚪";
            out += F("</b>");
            out += F("\n  авария: <b>");
            out += (st.fault == AvrController::Fault::None) ? "⚪" : "🔴";
            out += F("</b>");
            if (st.fault != AvrController::Fault::None)
            {
                out += F(" <b>(");
                out += TelegramMenuAvr::faultRu_(st.fault);
                out += F(")</b>");
            }
            return out;
        }
        if (!self._stack_cache)
            return F("АВР недоступен");
        const uint32_t node_id = self.selectedNodeId_(chat_id);
        if (node_id == 0)
            return F("АВР недоступен");
        const auto *cache = self._stack_cache->avrCache(node_id);
        if (!cache || !cache->has_data)
        {
            self._stack_cache->requestAvr(node_id);
            return F("<b>АВР:</b>\n  обновление...");
        }
        out += F("\n  авто: <b>");
        out += cache->auto_mode ? "🟢" : "⚪";
        out += F("</b>");
        out += F("\n  источник: <b>");
        out += cache->active_source[0] ? String(TelegramMenuAvr::sourceRuByName_(String(cache->active_source))) : String("-");
        out += F("</b>");
        out += F("\n  цель: <b>");
        out += cache->target_source[0] ? String(TelegramMenuAvr::sourceRuByName_(String(cache->target_source))) : String("-");
        out += F("</b>");
        out += F("\n  сеть основная: <b>");
        out += cache->main_ok ? "🟢" : "⚪";
        out += F("</b>");
        out += F("\n  сеть резерв: <b>");
        out += cache->reserve_ok ? "🟢" : "⚪";
        out += F("</b>");
        out += F("\n  авария: <b>");
        const String fault_raw = cache->fault[0] ? String(cache->fault) : String("none");
        const bool has_fault = (fault_raw != "none");
        out += has_fault ? "🔴" : "⚪";
        out += F("</b>");
        if (has_fault)
        {
            out += F(" <b>(");
            out += TelegramMenuAvr::faultRuByName_(fault_raw);
            out += F(")</b>");
        }
        return out;
    }


    String TelegramMenuAvr::avrControlMarkup_(TelegramMenu &self, int64_t chat_id)
    {
        std::vector<String> labels;
        labels.reserve(8);
        bool auto_mode = true;
        if (self.isLocalSelected_(chat_id))
        {
            if (self._avr)
                auto_mode = self._avr->config().auto_mode;
        }
        else if (self._stack_cache)
        {
            const uint32_t node_id = self.selectedNodeId_(chat_id);
            if (node_id != 0)
            {
                const auto *cache = self._stack_cache->avrCache(node_id);
                if (!cache || !cache->has_data)
                    self._stack_cache->requestAvr(node_id);
                else
                    auto_mode = cache->auto_mode;
            }
        }
        labels.push_back(F("Статус"));
        labels.push_back(auto_mode ? F("Авто ⚪") : F("Авто 🟢"));
        labels.push_back(F("Источник основной"));
        labels.push_back(F("Источник резерв"));
        labels.push_back(F("Источник выкл"));
        labels.push_back(F("Сброс аварии"));
        labels.push_back(F("Назад"));
        return TelegramMenu::buildKeyboardMarkup_(labels);
    }


    void TelegramMenuAvr::sendAvrMenu_(TelegramMenu &self, int64_t chat_id)
    {
        if (!self._bot)
            return;
        if (self.isLocalSelected_(chat_id) && !self._avr)
        {
            self._bot->sendText(chat_id, F("АВР недоступен"));
            return;
        }
        const String text = TelegramMenuAvr::avrStatusTextHtml_(self, chat_id);
        const String markup = TelegramMenuAvr::avrControlMarkup_(self, chat_id);
        self._bot->setMenu(chat_id, "avr");
        self._bot->sendText(chat_id, text, markup, "HTML");
    }


    bool TelegramMenuAvr::handleAvrSelection_(TelegramMenu &self, const TelegramClient::Update &u)
    {
        if (!self._bot)
            return false;
        const char *menu_id = self._bot->currentMenuId(u.chat_id);
        if (!menu_id || strcmp(menu_id, "avr") != 0)
            return false;
        if (u.text.startsWith("/"))
            return false;
        if (u.text == F("Назад"))
        {
            self._bot->enterMenu(u.chat_id, "device");
            return true;
        }
        if (self.isLocalSelected_(u.chat_id) && !self._avr)
        {
            self._bot->sendText(u.chat_id, F("АВР недоступен"));
            return true;
        }
        if (u.text == F("Статус"))
        {
            TelegramMenuAvr::sendAvrMenu_(self, u.chat_id);
            return true;
        }

        bool handled = false;
        bool ok = false;
        if (self.isLocalSelected_(u.chat_id))
        {
            handled = true;
            if (u.text == F("Авто 🟢"))
                ok = self._avr->setAutoMode(true);
            else if (u.text == F("Авто ⚪") || u.text == F("Авто 🔴"))
                ok = self._avr->setAutoMode(false);
            else if (u.text == F("Источник основной") || u.text == F("Источник Main"))
                ok = self._avr->setManualSource(AvrController::Source::Main);
            else if (u.text == F("Источник резерв") || u.text == F("Источник Reserve"))
                ok = self._avr->setManualSource(AvrController::Source::Reserve);
            else if (u.text == F("Источник выкл") || u.text == F("Источник Off"))
                ok = self._avr->setManualSource(AvrController::Source::Off);
            else if (u.text == F("Сброс аварии"))
            {
                self._avr->clearFault();
                ok = true;
            }
            else
            {
                handled = false;
            }
        }
        else
        {
            const uint32_t node_id = self.selectedNodeId_(u.chat_id);
            if (node_id != 0 && self._stack_master)
            {
                DynamicJsonDocument doc(384);
                doc["feature"] = (uint8_t)StackFeature::Avr;
                doc["action"] = "set";
                JsonObject params = doc["params"].to<JsonObject>();
                handled = true;
                if (u.text == F("Авто 🟢"))
                    params["auto_mode"] = true;
                else if (u.text == F("Авто ⚪") || u.text == F("Авто 🔴"))
                    params["auto_mode"] = false;
                else if (u.text == F("Источник основной") || u.text == F("Источник Main"))
                    params["manual_source"] = "main";
                else if (u.text == F("Источник резерв") || u.text == F("Источник Reserve"))
                    params["manual_source"] = "reserve";
                else if (u.text == F("Источник выкл") || u.text == F("Источник Off"))
                    params["manual_source"] = "off";
                else if (u.text == F("Сброс аварии"))
                    params["clear_fault"] = true;
                else
                    handled = false;
                if (handled)
                {
                    char payload[384] = {};
                    const size_t n = serializeJson(doc, payload, sizeof(payload));
                    ok = (n > 0) && self._stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                                                                reinterpret_cast<const uint8_t *>(payload), n);
                    if (ok && self._stack_cache)
                        self._stack_cache->requestAvr(node_id);
                }
            }
        }
        if (!handled)
            return false;
        if (!ok)
            self._bot->sendText(u.chat_id, F("Не удалось"));
        TelegramMenuAvr::sendAvrMenu_(self, u.chat_id);
        return true;
    }
