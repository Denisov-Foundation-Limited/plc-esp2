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

#include "core/network/web/handlers/telegram_handler.hpp"

#include "core/network/web/web_interface.hpp"

void TelegramHandler::registerRoutes(WebInterface &web, AsyncWebServer &server) {
        server.on("/telegram", HTTP_POST, [&web](AsyncWebServerRequest *request) { handleTelegramSave(web, request); });
        server.on("/telegram", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleTelegram(web, request); });
    }

void TelegramHandler::handleTelegram(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (!web.requireWebAdmin_(request, &set_cookie))
            return;
        String page = FPSTR(kWebInterfaceTelegramHtml);
        page.reserve(page.length() + 4096);
        page.replace("%NAV%", web.navHtml_());
        const String token_value = (web._tgbot && web._tgbot->token().length())
                                       ? WebInterface::maskSecretValue_(web._tgbot->token())
                                       : String("");
        page.replace("%TGBOT_TOKEN%", token_value);
        page.replace("%TGBOT_CHAT_ID%", web._tgbot ? String((long long)web._tgbot->chatId()) : String("0"));
        page.replace("%TGBOT_LAST_CHAT_ID%",
                     web._tgbot ? String((long long)web._tgbot->lastIncomingChatId()) : String("0"));
        page.replace("%TGBOT_INSECURE_CHECKED%", web._tgbot && web._tgbot->insecure() ? "checked" : "");
        page.replace("%TGBOT_CLIENT%", web._tgbot ? web._tgbot->clientKindName() : "none");
        page.replace("%TGBOT_CLIENT_WIFI_SELECTED%",
                     web._tgbot && web._tgbot->clientKind() == TelegramClient::ClientKind::WifiSecure ? "selected" : "");
        page.replace("%TGBOT_CLIENT_GSM_SELECTED%",
                     web._tgbot && web._tgbot->clientKind() == TelegramClient::ClientKind::TinyGsm ? "selected" : "");
        page.replace("%TGBOT_POLL_MODE_LONG_SELECTED%",
                     web._tgbot && web._tgbot->pollMode() == TelegramClient::PollMode::Long ? "selected" : "");
        page.replace("%TGBOT_POLL_MODE_SHORT_SELECTED%",
                     web._tgbot && web._tgbot->pollMode() == TelegramClient::PollMode::Short ? "selected" : "");
        page.replace("%TGBOT_USE_PROXY_CHECKED%", web._tgbot && web._tgbot->useProxy() ? "checked" : "");
        page.replace("%TGBOT_PROXY_HOST%", web._tgbot ? web._tgbot->proxyHost() : String(""));
        page.replace("%TGBOT_PROXY_PORT%", web._tgbot ? String((unsigned)web._tgbot->proxyPort()) : String("0"));
        page.replace("%TGBOT_PROXY_PATH%", web._tgbot ? web._tgbot->proxyPath() : String(""));
        page.replace("%TGBOT_CLIENT_LABEL%", WebUiRu::TelegramPage::kClient);
        page.replace("%TGBOT_ACCESS_TITLE%", WebUiRu::TelegramPage::kAccess);
        page.replace("%TGBOT_LAST_CHAT_ID_LABEL%", WebUiRu::TelegramPage::kLastChatId);
        page.replace("%TGBOT_USE_PROXY_LABEL%", WebUiRu::TelegramPage::kUseProxy);
        page.replace("%TGBOT_UNKNOWN_TEXT%", WebUiRu::TelegramPage::kUnknown);
        page.replace("%SAVE_TEXT%", WebUiRu::kSave);
        page.replace("%TGBOT_STATUS%", web._tgbot_status);
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        web.sendHtml_(request, page, set_cookie);
    }

void TelegramHandler::handleTelegramSave(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (!web.requireWebAdmin_(request, &set_cookie))
            return;
        bool changed = false;

        if (web._tgbot && request->hasParam("token", true))
        {
            String token = request->getParam("token", true)->value();
            token.trim();
            const bool masked_unchanged = WebInterface::isMaskedSecret_(token, web._tgbot->token());
            if (!masked_unchanged && token != web._tgbot->token())
            {
                web._tgbot->setToken(token);
                changed = true;
            }
        }
        if (web._tgbot)
        {
            if (request->hasParam("chat_id", true))
            {
                String chat_str = request->getParam("chat_id", true)->value();
                chat_str.trim();
                const int64_t chat_id = (int64_t)strtoll(chat_str.c_str(), nullptr, 10);
                if (chat_id != web._tgbot->chatId())
                {
                    web._tgbot->setChatId(chat_id);
                    changed = true;
                }
            }
            if (request->hasParam("client", true))
            {
                String client = request->getParam("client", true)->value();
                client.trim();
                client.toLowerCase();
                const TelegramClient::ClientKind new_kind =
                    (client == "gsm" || client == "tinygsm") ? TelegramClient::ClientKind::TinyGsm
                                                              : TelegramClient::ClientKind::WifiSecure;
                if (new_kind != web._tgbot->clientKind())
                {
                    web._tgbot->setClientKindHint(new_kind);
                    changed = true;
                }
            }
            const bool insecure = request->hasParam("insecure", true);
            if (insecure != web._tgbot->insecure())
            {
                web._tgbot->setInsecure(insecure);
                changed = true;
            }
            String poll_mode = request->hasParam("poll_mode", true) ? request->getParam("poll_mode", true)->value() : "long";
            poll_mode.trim();
            poll_mode.toLowerCase();
            const TelegramClient::PollMode new_mode =
                (poll_mode == "short") ? TelegramClient::PollMode::Short : TelegramClient::PollMode::Long;
            if (new_mode != web._tgbot->pollMode())
            {
                web._tgbot->setPollMode(new_mode);
                changed = true;
            }
        }

        if (web._tgbot)
        {
            const bool use_proxy = request->hasParam("use_proxy", true);
            String host = request->hasParam("proxy_host", true) ? request->getParam("proxy_host", true)->value() : "";
            String port_str = request->hasParam("proxy_port", true) ? request->getParam("proxy_port", true)->value() : "0";
            String path = request->hasParam("proxy_path", true) ? request->getParam("proxy_path", true)->value() : "";
            host.trim();
            path.trim();
            const uint16_t port = (uint16_t)strtoul(port_str.c_str(), nullptr, 10);

            if (use_proxy)
            {
                if (host != web._tgbot->proxyHost() || port != web._tgbot->proxyPort() ||
                    path != web._tgbot->proxyPath() || !web._tgbot->useProxy())
                {
                    web._tgbot->setProxy(host, port, path);
                    changed = true;
                }
            }
            else if (web._tgbot->useProxy())
            {
                web._tgbot->clearProxy();
                changed = true;
            }
        }

        bool save_ok = true;
        if (changed)
            save_ok = web.saveWifiConfig_();

        if (!changed)
            web._tgbot_status = WebUiRu::Common::kNoChangesAlt;
        else if (!save_ok)
            web._tgbot_status = WebUiRu::Common::kSaveFailed;
        else
            web._tgbot_status = WebUiRu::Common::kSaved;

        web.sendRedirect_(request, "/telegram", set_cookie);
    }
