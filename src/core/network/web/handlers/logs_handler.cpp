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

#include "core/network/web/handlers/logs_handler.hpp"

#include "core/network/web/web_interface.hpp"

void LogsHandler::registerRoutes(WebInterface &web, AsyncWebServer &server) {
        server.on("/logs", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleLogs(web, request); });
    }

void LogsHandler::handleLogs(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (!web.requireWebAdmin_(request, &set_cookie))
            return;
        String page = FPSTR(kWebInterfaceLogsHtml);
        page.reserve(page.length() + 4096);
        page.replace("%NAV%", web.navHtml_());
        String lines;
        if (web._log)
        {
            const size_t count = web._log->recentCount();
            lines.reserve(count * 96 + 64);
            if (count == 0)
            {
                lines = "No logs";
            }
            else
            {
                char buf[LOGGER_BUFFER_SIZE] = {};
                for (size_t i = 0; i < count; ++i)
                {
                    if (web._log->getRecentLine(i, buf, sizeof(buf)))
                    {
                        web.appendHtmlEscaped_(lines, buf);
                        lines += "\n";
                    }
                }
            }
        }
        else
        {
            lines = "Logger unavailable";
        }
        page.replace("%LOG_LINES%", lines);
        web.sendHtml_(request, page, set_cookie);
    }
