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
#include "core/network/web/interfaces/web_interface_handler_base.hpp"
#include <stdlib.h>
#include "utils/users_registry.hpp"

class UsersHandler
{
public:
    static void registerRoutes(WebInterface &web, AsyncWebServer &server);

    static void handleUsers(WebInterface &web, AsyncWebServerRequest *request);

    static void handleUsersSave(WebInterface &web, AsyncWebServerRequest *request);

    static void handleUsersAcl(WebInterface &web, AsyncWebServerRequest *request);

    static void handleUsersAclSave(WebInterface &web, AsyncWebServerRequest *request);

private:
    static int64_t parseChatId_(const String &value);

    static String usersCards_(WebInterface &web, bool read_only);

    template <typename FnT>
    static void forEachAclController_(FnT fn);

    static const char *aclControllerTitle_(UsersRegistry::AclController ctrl);

    static bool parseAclRouteParams_(WebInterface &web, AsyncWebServerRequest *request, size_t &user_idx, uint8_t &unit);

    static bool aclUnitNodeId_(WebInterface &web, uint8_t unit, uint32_t &node_id);

    static String aclItemLabel_(const String &prefix, uint16_t id, const String &name);

    template <typename FnT>
    static void forEachAclItem_(WebInterface &web, uint8_t unit, UsersRegistry::AclController ctrl, FnT fn, bool *loading = nullptr);

    static String aclTiles_(WebInterface &web, const UsersRegistry::User &u, uint8_t unit, bool read_only);

    static String ibuttonDatalist_(WebInterface &web);

    static String rfidDatalist_(WebInterface &web);
};
