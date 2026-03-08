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

#include "core/network/web/handlers/stack_handler.hpp"

#include "core/network/web/web_interface.hpp"

void StackHandler::registerRoutes(WebInterface &web, AsyncWebServer &server) {
        server.on("/stack/nodes_tbody", HTTP_GET,
                  [&web](AsyncWebServerRequest *request) { handleNodesTbody(web, request); });
        server.on("/stack/slave_link", HTTP_GET,
                  [&web](AsyncWebServerRequest *request) { handleSlaveLinkStatus(web, request); });
        server.on("/stack/online_snapshot", HTTP_GET,
                  [&web](AsyncWebServerRequest *request) { handleOnlineSnapshot(web, request); });
        server.on("/stack", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleStack(web, request); });
    }

void StackHandler::handleStack(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (!web.requireWebAdmin_(request, &set_cookie))
            return;
        String page = FPSTR(kWebInterfaceStackHtml);
        page.reserve(page.length() + 4096);
        page.replace("%NAV%", web.navHtml_());
        const auto role = web.stackRole_();
        page.replace("%STACK_ROLE%", web.stackRoleName_(role));
        page.replace("%STACK_ROLE_MASTER_SEL%", role == ConfigsManagerIface::StackRole::Master ? "selected" : "");
        page.replace("%STACK_ROLE_SLAVE_SEL%", role == ConfigsManagerIface::StackRole::Slave ? "selected" : "");
        page.replace("%STACK_SLAVE_STYLE%", role == ConfigsManagerIface::StackRole::Slave ? "" : "display:none");
        page.replace("%STACK_PAGE_TITLE%", WebUiRu::StackPage::kPageTitle);
        page.replace("%STACK_TITLE%", WebUiRu::StackPage::kTitle);
        page.replace("%STACK_LABEL_ROLE%", WebUiRu::StackPage::kRole);
        page.replace("%STACK_LABEL_MASTER_HOST%", WebUiRu::StackPage::kMasterHost);
        page.replace("%STACK_LABEL_FALLBACK_MASTER%", WebUiRu::StackPage::kFallbackMaster);
        page.replace("%STACK_LABEL_ENABLE%", WebUiRu::StackPage::kEnable);
        page.replace("%STACK_LABEL_FALLBACK_HOST%", WebUiRu::StackPage::kFallbackHost);
        page.replace("%STACK_LABEL_SLAVE_CONTROLLER%", WebUiRu::StackPage::kSlaveController);
        page.replace("%STACK_LABEL_FULL_CONTROLLER%", WebUiRu::StackPage::kFullController);
        page.replace("%STACK_LABEL_API_KEY%", WebUiRu::StackPage::kApiKey);
        page.replace("%STACK_API_KEY_PLACEHOLDER%", WebUiRu::StackPage::kApiKeyPlaceholder);
        page.replace("%STACK_BTN_GEN_KEY%", WebUiRu::StackPage::kGenerate);
        auto linkDisconnectedText = [role]() -> const char * {
            return role == ConfigsManagerIface::StackRole::Slave
                       ? WebUiRu::StackPage::kMasterLinkDisconnected
                       : WebUiRu::StackPage::kSlaveLinkDisconnected;
        };
        auto linkConnectedText = [role]() -> const char * {
            return role == ConfigsManagerIface::StackRole::Slave
                       ? WebUiRu::StackPage::kMasterLinkConnected
                       : WebUiRu::StackPage::kSlaveLinkConnected;
        };
        auto linkWaitingHelloText = [role]() -> const char * {
            return role == ConfigsManagerIface::StackRole::Slave
                       ? WebUiRu::StackPage::kMasterLinkWaitingHello
                       : WebUiRu::StackPage::kSlaveLinkWaitingHello;
        };
        page.replace("%STACK_SLAVE_LINK_DISCONNECTED%", linkDisconnectedText());
        String slave_link_class = "bad";
        String slave_link_text = linkDisconnectedText();
        if (role == ConfigsManagerIface::StackRole::Slave && web._stack_slave)
        {
            if (web._stack_slave->linkReadyAfterHello())
            {
                slave_link_class = "ok";
                slave_link_text = linkConnectedText();
            }
            else if (web._stack_slave->nodeConnected())
            {
                slave_link_class = "bad";
                slave_link_text = linkWaitingHelloText();
            }
        }
        page.replace("%STACK_SLAVE_LINK_CLASS%", slave_link_class);
        page.replace("%STACK_SLAVE_LINK_TEXT%", slave_link_text);
        page.replace("%STACK_MASTER_HOST%", web.stackMasterHost_());
        page.replace("%STACK_FALLBACK_ENABLED_CHECKED%", web.stackFallbackEnabled_() ? "checked" : "");
        page.replace("%STACK_FALLBACK_HOST%", web.stackFallbackHost_());
        page.replace("%STACK_SLAVE_CONTROLLER_CHECKED%", web.stackSlaveController_() ? "checked" : "");
        page.replace("%STACK_API_KEY%", WebInterface::maskSecretValue_(web.stackApiKey_()));
        page.replace("%SAVE_TEXT%", WebUiRu::kSave);
        page.replace("%STACK_STATUS%", web._stack_status);
        if (role == ConfigsManagerIface::StackRole::Master)
        {
            String self = String("<p class=\"status\">") + WebUiRu::StackPage::kCurrentControllerPrefix + ": <strong>" +
                          web.deviceName_() + "</strong> | IP: <strong>" + web.wifiIp_() + "</strong></p>";
            page.replace("%STACK_SELF_BLOCK%", self);
            page.replace("%STACK_NODES_BLOCK%", web.stackNodesBlockHtml_());
        }
        else
        {
            page.replace("%STACK_SELF_BLOCK%", "");
            page.replace("%STACK_NODES_BLOCK%", "");
        }
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        web.sendHtml_(request, page, set_cookie);
    }

void StackHandler::handleSlaveLinkStatus(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (!web.requireWebAdmin_(request, &set_cookie))
            return;
        const auto role = web.stackRole_();
        auto linkDisconnectedText = [role]() -> const char * {
            return role == ConfigsManagerIface::StackRole::Slave
                       ? WebUiRu::StackPage::kMasterLinkDisconnected
                       : WebUiRu::StackPage::kSlaveLinkDisconnected;
        };
        auto linkConnectedText = [role]() -> const char * {
            return role == ConfigsManagerIface::StackRole::Slave
                       ? WebUiRu::StackPage::kMasterLinkConnected
                       : WebUiRu::StackPage::kSlaveLinkConnected;
        };
        auto linkWaitingHelloText = [role]() -> const char * {
            return role == ConfigsManagerIface::StackRole::Slave
                       ? WebUiRu::StackPage::kMasterLinkWaitingHello
                       : WebUiRu::StackPage::kSlaveLinkWaitingHello;
        };
        String cls = "bad";
        String text = linkDisconnectedText();
        if (role == ConfigsManagerIface::StackRole::Slave && web._stack_slave)
        {
            if (web._stack_slave->linkReadyAfterHello())
            {
                cls = "ok";
                text = linkConnectedText();
            }
            else if (web._stack_slave->nodeConnected())
            {
                text = linkWaitingHelloText();
            }
        }
        String json;
        json.reserve(96);
        json += "{\"class\":\"";
        json += cls;
        json += "\",\"text\":\"";
        json += text;
        json += "\"}";
        web.sendText_(request, 200, "application/json", json, set_cookie);
    }

void StackHandler::handleNodesTbody(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (!web.requireWebAdmin_(request, &set_cookie))
            return;
        if (web.stackRole_() != ConfigsManagerIface::StackRole::Master)
        {
            web.sendText_(request, 200, "text/html; charset=utf-8", "", set_cookie);
            return;
        }
        web.sendText_(request, 200, "text/html; charset=utf-8", web.listStackNodesHtml_(), set_cookie);
    }

void StackHandler::handleOnlineSnapshot(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (web.stackRole_() != ConfigsManagerIface::StackRole::Master || !web._stack_master)
        {
            web.sendText_(request, 200, "application/json", "[]", set_cookie);
            return;
        }
        String out;
        out.reserve(512);
        out += "[";
        const size_t count = web._stack_master->nodeCount();
        bool first = true;
        for (size_t i = 0; i < count; ++i)
        {
            const uint32_t id = web._stack_master->nodeIdAt(i);
            if (id == 0)
                continue;
            if (!web._stack_master->nodeIsOnline(id, 6000))
                continue;
            String name = web._stack_master->nodeNameAt(i);
            bool sync_ready = false;
            if (web._stack_cache)
            {
                auto cacheReady = [](const auto *cache) -> bool {
                    return cache && (cache->has_data || cache->last_ok || cache->last_error.length());
                };
                const auto *sockets = web._stack_cache->socketsCache(id);
                const auto *lights = web._stack_cache->lightsCache(id);
                const auto *meteo = web._stack_cache->meteoCache(id);
                const auto *thermo = web._stack_cache->thermoCache(id);
                const auto *tanks = web._stack_cache->tanksCache(id);
                const auto *septic = web._stack_cache->septicCache(id);
                const auto *security = web._stack_cache->securityCache(id);
                const auto *watering = web._stack_cache->wateringCache(id);
                const auto *leak = web._stack_cache->leakCache(id);
                sync_ready = cacheReady(sockets) && cacheReady(lights) && cacheReady(meteo) &&
                             cacheReady(thermo) && cacheReady(tanks) && cacheReady(septic) &&
                             cacheReady(security) && cacheReady(watering) && cacheReady(leak);
            }
            if (!first)
                out += ",";
            out += "{\"id\":";
            out += String((unsigned long)id);
            out += ",\"name\":\"";
            if (name.length())
                web.appendJsonEscaped_(out, name.c_str());
            else
                out += web.stackNodeIdHex_(id);
            out += "\",\"sync\":";
            out += sync_ready ? "1" : "0";
            out += "}";
            first = false;
        }
        out += "]";
        web.sendText_(request, 200, "application/json", out, set_cookie);
    }
