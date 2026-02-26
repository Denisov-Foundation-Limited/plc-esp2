#include "core/network/web/web_interface_stack_routes.hpp"

#include "core/network/web/web_interface.hpp"
#include "core/network/web/interfaces/web_interface_handlers.hpp"

void WebInterfaceStackRoutes::registerRoutes(WebInterface &web, AsyncWebServer &server)
{
    server.on("/stack/gen_key", HTTP_POST, [&web](AsyncWebServerRequest *request) { web.handleStackGenKey_(request); });
    StackHandler::registerRoutes(web, server);
    server.on("/stack", HTTP_POST, [&web](AsyncWebServerRequest *request) { web.handleStackSave_(request); });
}
