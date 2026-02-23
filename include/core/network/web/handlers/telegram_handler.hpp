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

class WebInterface;
class AsyncWebServer;
class AsyncWebServerRequest;

class TelegramHandler
{
public:
    static void registerRoutes(WebInterface &web, AsyncWebServer &server)
    {
        server.on("/telegram", HTTP_POST, [&web](AsyncWebServerRequest *request) { handleTelegramSave(web, request); });
        server.on("/telegram", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleTelegram(web, request); });
    }

    static void handleTelegram(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (!web.requireWebAdmin_(request, &set_cookie))
            return;
        String page = FPSTR(kWebInterfaceTelegramHtml);
        page.reserve(page.length() + 4096);
        page.replace("%NAV%", web.navHtml_());
        page.replace("%TGBOT_TOKEN%", web._tgbot ? web._tgbot->token() : String(""));
        page.replace("%TGBOT_CHAT_ID%", web._tgbot ? String((long long)web._tgbot->chatId()) : String("0"));
        page.replace("%TGBOT_LAST_CHAT_ID%",
                     web._tgbot ? String((long long)web._tgbot->lastIncomingChatId()) : String("0"));
        page.replace("%TGBOT_INSECURE_CHECKED%", web._tgbot && web._tgbot->insecure() ? "checked" : "");
        page.replace("%TGBOT_CLIENT%", web._tgbot ? web._tgbot->clientKindName() : "none");
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

    static void handleTelegramSave(WebInterface &web, AsyncWebServerRequest *request)
    {
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
            if (token != web._tgbot->token())
            {
                web._tgbot->setToken(token);
                changed = true;
            }
        }
        if (web._tgbot && request->hasParam("chat_id", true))
        {
            String chat = request->getParam("chat_id", true)->value();
            chat.trim();
            if (chat.length() > 0)
            {
                int64_t chat_id = (int64_t)strtoll(chat.c_str(), nullptr, 10);
                if (chat_id != web._tgbot->chatId())
                {
                    web._tgbot->setChatId(chat_id);
                    changed = true;
                }
            }
        }
        if (web._tgbot)
        {
            const bool insecure = request->hasParam("insecure", true);
            if (insecure != web._tgbot->insecure())
            {
                web._tgbot->setInsecure(insecure);
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
};
