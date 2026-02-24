#include "core/network/telegram/telegram_menu.hpp"

#include <stdlib.h>
#include <string.h>

#include "controllers/ring_controller.hpp"
#include "core/network/stack/stack_features.hpp"
#include "core/network/stack/stack_master.hpp"
#include "core/network/wifi_manager.hpp"
#include "core/rtc.hpp"
#include "plc/plc_control.hpp"
#include "utils/configs.hpp"
#include "utils/logger.hpp"

#include "core/network/telegram/menu/telegram_menu_ring.hpp"

// ---- telegram menu extracted definitions ----
bool TelegramMenuRing::cmdRing_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        (void)reply;
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        if (TelegramMenu::_self->isLocalSelected_(u.chat_id) && !TelegramMenu::_self->_ring)
        {
            reply = "Звонок недоступен";
            return true;
        }
        TelegramMenuRing::sendRingMenu_(*TelegramMenu::_self, u.chat_id);
        return true;
    }



    bool TelegramMenuRing::cmdRingOn_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        (void)bot;
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        const bool ok = TelegramMenuRing::setRingState_(*TelegramMenu::_self, u.chat_id, true);
        reply = ok ? "Команда отправлена" : "Не удалось";
        return true;
    }



    bool TelegramMenuRing::cmdRingOff_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    {
        (void)bot;
        if (!TelegramMenu::_self)
            return false;
        if (!TelegramMenu::requireAdmin_(*TelegramMenu::_self, bot, u, reply))
            return true;
        const bool ok = TelegramMenuRing::setRingState_(*TelegramMenu::_self, u.chat_id, false);
        reply = ok ? "Команда отправлена" : "Не удалось";
        return true;
    }



    String TelegramMenuRing::ringStatusText_(TelegramMenu &self, int64_t chat_id)
    {
        if (self.isLocalSelected_(chat_id))
        {
            if (!self._ring)
                return "Звонок недоступен";
            const auto &st = self._ring->state();
            String out = F("Звонок:\n  реле: ");
            out += st.relay_on ? "🟢" : "⚪";
            return out;
        }
        String out = F("Звонок:\n  удаленный узел\n  управление: Вкл/Выкл");
        return out;
    }



    String TelegramMenuRing::ringControlMarkup_(TelegramMenu &self, int64_t chat_id)
    {
        (void)self;
        (void)chat_id;
        std::vector<String> labels;
        labels.reserve(3);
        labels.push_back(F("Звонок 🟢"));
        labels.push_back(F("Звонок ⚪"));
        labels.push_back(F("Назад"));
        return TelegramMenu::buildKeyboardMarkup_(labels);
    }



    void TelegramMenuRing::sendRingMenu_(TelegramMenu &self, int64_t chat_id)
    {
        if (!self._bot)
            return;
        if (self.isLocalSelected_(chat_id) && !self._ring)
        {
            self._bot->sendText(chat_id, F("Звонок недоступен"));
            return;
        }
        const String text = TelegramMenuRing::ringStatusText_(self, chat_id);
        const String markup = TelegramMenuRing::ringControlMarkup_(self, chat_id);
        self._bot->setMenu(chat_id, "ring");
        self._bot->sendText(chat_id, text, markup);
    }



    bool TelegramMenuRing::handleRingSelection_(TelegramMenu &self, const TelegramClient::Update &u)
    {
        if (!self._bot)
            return false;
        const char *menu_id = self._bot->currentMenuId(u.chat_id);
        if (!menu_id || strcmp(menu_id, "ring") != 0)
            return false;
        if (u.text.startsWith("/"))
            return false;
        if (u.text == F("Назад"))
        {
            self._bot->enterMenu(u.chat_id, "device");
            return true;
        }
        if (u.text == F("Звонок 🟢") || u.text == F("Звонок Вкл"))
        {
            const bool ok = TelegramMenuRing::setRingState_(self, u.chat_id, true);
            if (!ok)
                self._bot->sendText(u.chat_id, F("Не удалось"));
            TelegramMenuRing::sendRingMenu_(self, u.chat_id);
            return true;
        }
        if (u.text == F("Звонок ⚪") || u.text == F("Звонок Выкл"))
        {
            const bool ok = TelegramMenuRing::setRingState_(self, u.chat_id, false);
            if (!ok)
                self._bot->sendText(u.chat_id, F("Не удалось"));
            TelegramMenuRing::sendRingMenu_(self, u.chat_id);
            return true;
        }
        return false;
    }

bool TelegramMenuRing::setRingState_(TelegramMenu &self, int64_t chat_id, bool on)
    {
        if (self.isLocalSelected_(chat_id))
        {
            if (!self._ring)
                return false;
            return self._ring->setHoldRelayWithSource(on, RingController::Source::Web);
        }
        const uint32_t node_id = self.selectedNodeId_(chat_id);
        if (node_id == 0 || !self._stack_master)
            return false;
        DynamicJsonDocument doc(256);
        doc["feature"] = (uint8_t)StackFeature::Ring;
        doc["action"] = "set";
        JsonObject params = doc["params"].to<JsonObject>();
        params["state"] = on;
        char payload[256] = {};
        const size_t n = serializeJson(doc, payload, sizeof(payload));
        return (n > 0) && self._stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                                                     reinterpret_cast<const uint8_t *>(payload), n);
    }
