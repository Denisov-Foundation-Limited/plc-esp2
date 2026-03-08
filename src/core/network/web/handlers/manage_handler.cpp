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

#include "core/network/web/handlers/manage_handler.hpp"

#include "core/network/web/web_interface.hpp"

void ManageHandler::registerRoutes(WebInterface &web, AsyncWebServer &server) {
        server.on("/manage", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleManage(web, request); });
    }

void ManageHandler::handleManage(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (!web.requireWebAdmin_(request, &set_cookie))
            return;
        String page = FPSTR(kWebInterfaceManageHtml);
        page.reserve(page.length() + 4096);
        page.replace("%NAV%", web.navHtml_());
        page.replace("%MANAGE_PAGE_TITLE%", WebUiRu::ManagePage::kPageTitle);
        page.replace("%MANAGE_TITLE%", WebUiRu::ManagePage::kTitle);
        page.replace("%MANAGE_FW_SECTION%", WebUiRu::ManagePage::kFwSection);
        page.replace("%MANAGE_FW_HINT%", WebUiRu::ManagePage::kFwHint);
        page.replace("%MANAGE_UPLOAD_FW%", WebUiRu::ManagePage::kUploadFw);
        page.replace("%MANAGE_FILES_SECTION%", WebUiRu::ManagePage::kFilesSection);
        page.replace("%MANAGE_UPLOAD_FILE%", WebUiRu::ManagePage::kUploadFile);
        page.replace("%MANAGE_STATUS_LINK%", WebUiRu::ManagePage::kStatusLink);
        page.replace("%MANAGE_FILE_LIST%", WebUiRu::ManagePage::kFileList);
        page.replace("%MANAGE_COL_NAME%", WebUiRu::ManagePage::kName);
        page.replace("%MANAGE_COL_SIZE%", WebUiRu::ManagePage::kSize);
        page.replace("%MANAGE_COL_DELETE%", WebUiRu::ManagePage::kDelete);
        page.replace("%FILES%", web.listFilesHtml_());
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        web.sendHtml_(request, page, set_cookie);
    }
