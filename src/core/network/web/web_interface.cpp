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

#include "core/network/web/web_interface.hpp"
#include "core/network/web/web_interface_controllers_ops.hpp"
#include "core/network/web/web_interface_stack_ops.hpp"
#include "core/network/web/web_interface_api_routes.hpp"
#include "core/network/web/web_interface_devices_routes.hpp"
#include "core/network/web/web_interface_stack_routes.hpp"
#include "core/network/web/web_interface_system_routes.hpp"
#include "core/network/web/interfaces/web_interface_controllers_ring.hpp"

void WebInterface::registerRoutes()
{
    WebInterfaceSystemRoutes::registerPrimary(*this, _server);
    WebInterfaceStackRoutes::registerRoutes(*this, _server);
    WebInterfaceSystemRoutes::registerSecondary(*this, _server);
    WebInterfaceDevicesRoutes::registerRoutes(*this, _server);
    WebInterfaceSystemRoutes::registerTail(*this, _server);
    WebInterfaceApiRoutes::registerRoutes(*this, _server);
    WebInterfaceSystemRoutes::registerDisplay(*this, _server);
    WebInterfaceApiRoutes::registerNotFound(*this, _server);
}

    WebInterface::WebInterface(AsyncWebServer &server, CliConsole &cli, WifiManager &wifi, Configs &configs, PlcControl &plc,
                 RTC &rtc, TelegramClient &tgbot, TelegramBot &tgbot_bot, TelegramMenu &tgbot_menu, Logger &logs,
                 Extender &ext,
                 I2CManager &i2c, OneWireManager &ow, Controllers &controllers, RulesController &rules)
        : _server(server),
          _cli_auth(&cli),
          _wifi(wifi),
          _configs(configs),
          _plc(&plc),
          _rtc(&rtc),
          _tgbot(&tgbot),
          _tgbot_bot(&tgbot_bot),
          _tgbot_menu(&tgbot_menu),
          _controllers(&controllers),
          _rules(&rules),
          _ext(&ext),
          _i2c(&i2c),
          _ow(&ow),
          _log(&logs),
          _controllers_ops(*this),
          _stack_ops(*this)
{
}



    bool WebInterface::begin(bool format_on_fail )
{
        return LittleFS.begin(format_on_fail, FsConfig::kBasePath, FsConfig::kMaxOpenFiles,
                              FsConfig::kPartitionLabel);
    }



    void WebInterface::setAuth(const String &user, const String &pass)
{
        _cli_auth = nullptr;
        _auth_user = user;
        _auth_pass = pass;
        _auth_enabled = (_auth_user.length() > 0);
    }



    void WebInterface::setMaxUploadBytes(size_t bytes)
{ _max_upload = bytes; }



    void WebInterface::setAllowedExtensions(const String &exts_csv)
{
        _allowed_exts = exts_csv;
        _allowed_exts.toLowerCase();
    }



    void WebInterface::setGsmModem(GsmModem &modem)
{ _gsm = &modem; }


    void WebInterface::setStackCache(StackCache &cache)
{
        _stack_cache = &cache;
        if (_log)
            _stack_cache->setLogger(_log);
        if (_configs_manager)
            _stack_cache->setConfigsManager(_configs_manager);
        if (_stack_master)
            _stack_cache->setStackMaster(_stack_master);
    }


    void WebInterface::setConfigsManager(ConfigsManagerIface &mgr)
{
        _configs_manager = &mgr;
        if (_stack_cache)
            _stack_cache->setConfigsManager(&mgr);
    }


    void WebInterface::setUsersRegistry(UsersRegistry &users)
{ _users = &users; }


    void WebInterface::setStackMaster(StackMaster &master)
{
        _stack_master = &master;
        if (_stack_cache)
            _stack_cache->setStackMaster(&master);
        _stack_master->setFrameHandlerSecondary(&WebInterface::onStackFrame_, this);
    }


    void WebInterface::setStackSlave(StackSlaveHandler *slave)
{ _stack_slave = slave; }


    void WebInterface::setCloudClient(CloudClient &client)
{ _cloud = &client; }


    void WebInterface::setRules(RulesController &rules)
{ _rules = &rules; }



    StackCache &WebInterface::stackCache()
{ return *_stack_cache; }


    const StackCache &WebInterface::stackCache() const
{ return *_stack_cache; }


    void WebInterface::logStackCacheAllocations()
{
        if (_stack_cache)
            _stack_cache->logAllocations();
    }


    void WebInterface::handleAdminSave_(AsyncWebServerRequest *request)
{
        bool set_cookie = false;
        if (!checkAuth_(request, &set_cookie))
            return;
        if (!requireWebAdmin_(request, &set_cookie))
            return;
        const bool has_rtc = request->hasParam("rtc_date", true) || request->hasParam("rtc_time", true);
        const bool has_buzzer = request->hasParam("buzzer_present", true);
        const bool has_eeprom = request->hasParam("eeprom_present", true);
        const bool has_system = has_buzzer || has_eeprom;

        if (has_rtc)
        {
            if (!_rtc)
            {
                sendText_(request, 500, "text/plain", "RTC unavailable", set_cookie);
                return;
            }
            if (!request->hasParam("rtc_date", true) || !request->hasParam("rtc_time", true))
            {
                sendText_(request, 400, "text/plain", "Missing RTC date/time", set_cookie);
                return;
            }
            String date = request->getParam("rtc_date", true)->value();
            String time = request->getParam("rtc_time", true)->value();
            date.trim();
            time.trim();
            if (!setRtc_(date, time))
            {
                sendText_(request, 400, "text/plain", "Invalid RTC datetime", set_cookie);
                return;
            }
        }

        if (has_buzzer)
        {
            if (!_plc)
            {
                sendText_(request, 500, "text/plain", "PLC unavailable", set_cookie);
                return;
            }
            const bool enabled = request->hasParam("buzzer_enabled", true);
            _plc->setBuzzerEnabled(enabled);
        }

        if (has_eeprom)
        {
            if (!_configs_manager)
            {
                sendText_(request, 500, "text/plain", "Config manager unavailable", set_cookie);
                return;
            }
            _configs_manager->setEepromSaveEnabled(request->hasParam("eeprom_save", true));
            _configs_manager->setEepromLoadEnabled(request->hasParam("eeprom_load", true));
        }

        if (has_system)
        {
            if (!_configs_manager || !_configs_manager->save())
            {
                sendText_(request, 500, "text/plain", "Save failed", set_cookie);
                return;
            }
        }

        if (!has_rtc && !has_system)
        {
            sendText_(request, 400, "text/plain", "Missing data", set_cookie);
            return;
        }

        sendRedirect_(request, "/admin", set_cookie);
    }



    String WebInterface::listFilesHtml_()
{
        String items;
        items.reserve(2048);
        File root = LittleFS.open("/");
        File file = root.openNextFile();
        while (file)
        {
            String name = file.name();
            String path = name;
            if (!path.startsWith("/"))
                path = "/" + path;
            items += "<tr><td><a href=\"/files";
            items += path;
            items += "\">";
            items += "<strong>";
            items += name;
            items += "</strong>";
            items += "</a></td><td class=\"right\">";
            items += "<strong>";
            items += String((unsigned)file.size());
            items += "</strong>";
            items += " B</td><td class=\"right\"><a class=\"del\" onclick=\"return confirm('Delete file ";
            items += name;
            items += "?')\" href=\"/delete?path=";
            items += path;
            items += "\"><strong>Delete</strong></a></td></tr>";
            file = root.openNextFile();
        }
        if (items.length() == 0)
            items = "<tr><td colspan=\"3\" style=\"color:#94a3b8\"><strong>No files</strong></td></tr>";
        return items;
    }

const char *WebInterface::extTypeName_(Extender::Type t)
{
        switch (t)
        {
        case Extender::Type::PCF8574:
            return "PCF8574";
        case Extender::Type::MCP23017:
            return "MCP23017";
        default:
            return "None";
        }
    }

const char *WebInterface::extDevTypeName_(uint8_t dev)
{
        if (dev >= ActiveBoardProfile::EXT_DEVS_COUNT)
            return "None";
        return extTypeName_(ActiveBoardProfile::EXT_DEVS[dev].type);
    }

const char *WebInterface::portTypeName_(PortIO::PinType t)
{
        switch (t)
        {
        case PortIO::PinType::System:
            return "System";
        case PortIO::PinType::Relay:
            return "Relay";
        case PortIO::PinType::Led:
            return "Led";
        case PortIO::PinType::Sensor:
            return "Sensor";
        case PortIO::PinType::Button:
            return "Button";
        case PortIO::PinType::DInput:
            return "DInput";
        case PortIO::PinType::Buzzer:
            return "Buzzer";
        case PortIO::PinType::Fan:
            return "Fan";
        default:
            return "Unknown";
        }
    }

const char *WebInterface::locationName_(PortIO::Location loc)
{
        switch (loc)
        {
        case PortIO::Location::Cpu:
            return "CPU";
        case PortIO::Location::Ext1:
            return "EXT_1";
        case PortIO::Location::Ext2:
            return "EXT_2";
        case PortIO::Location::Ext3:
            return "EXT_3";
        case PortIO::Location::Ext4:
            return "EXT_4";
        case PortIO::Location::Ext5:
            return "EXT_5";
        case PortIO::Location::Ext6:
            return "EXT_6";
        case PortIO::Location::Ext7:
            return "EXT_7";
        case PortIO::Location::Ext8:
            return "EXT_8";
        case PortIO::Location::Ext9:
            return "EXT_9";
        case PortIO::Location::Ext10:
            return "EXT_10";
        default:
            return "UNKNOWN";
        }
    }

uint8_t WebInterface::locationIndex_(PortIO::Location loc)
{
        switch (loc)
        {
        case PortIO::Location::Cpu:
            return 0;
        case PortIO::Location::Ext1:
            return 1;
        case PortIO::Location::Ext2:
            return 2;
        case PortIO::Location::Ext3:
            return 3;
        case PortIO::Location::Ext4:
            return 4;
        case PortIO::Location::Ext5:
            return 5;
        case PortIO::Location::Ext6:
            return 6;
        case PortIO::Location::Ext7:
            return 7;
        case PortIO::Location::Ext8:
            return 8;
        case PortIO::Location::Ext9:
            return 9;
        case PortIO::Location::Ext10:
            return 10;
        default:
            return 0;
        }
    }

void WebInterface::appendPortLabel_(String &out, PortIO::PinType type, const PortIO::PortDesc &p, uint8_t ui_id)
{
        const char *prefix = "p";
        if (type == PortIO::PinType::Relay)
            prefix = "rly";
        else if (type == PortIO::PinType::DInput)
            prefix = "in";
        else if (type == PortIO::PinType::Sensor)
            prefix = "sens";
        const uint8_t loc = locationIndex_(p.location);
        out += prefix;
        out += "-";
        if (type == PortIO::PinType::Sensor && loc == 0)
        {
            out += String((unsigned)ui_id);
            return;
        }
        out += String((unsigned)loc);
        out += "/";
        out += String((unsigned)ui_id);
    }

const char *WebInterface::owBusName_(OneWireCfg::OwType t)
{
        switch (t)
        {
        case OneWireCfg::OwType::iButton:
            return "iButton";
        case OneWireCfg::OwType::Temp:
            return "Temp";
        default:
            return "Unknown";
        }
    }

void WebInterface::owAddrToHex_(const uint8_t in[8], char out[17])
{
        static const char kHex[] = "0123456789ABCDEF";
        for (uint8_t i = 0; i < 8; ++i)
        {
            out[i * 2] = kHex[(in[i] >> 4) & 0x0F];
            out[i * 2 + 1] = kHex[in[i] & 0x0F];
        }
        out[16] = '\0';
    }



String WebInterface::listPortsHtml_()
{
    return _controllers_ops.listPortsHtml_();
}



String WebInterface::listExtendersHtml_()
{
    return _controllers_ops.listExtendersHtml_();
}



String WebInterface::listStackPortsHtml_(uint32_t node_id) const
{
    return _controllers_ops.listStackPortsHtml_(node_id);
}



String WebInterface::listStackExtendersHtml_(uint32_t node_id) const
{
    return _controllers_ops.listStackExtendersHtml_(node_id);
}



String WebInterface::indexDeviceSelectHtml_(uint32_t selected_node_id, bool stack_view) const
{
    return _stack_ops.indexDeviceSelectHtml_(selected_node_id, stack_view);
}

String WebInterface::busesDeviceSelectHtml_(uint32_t selected_node_id, bool stack_view) const
{
    return _stack_ops.busesDeviceSelectHtml_(selected_node_id, stack_view);
}

String WebInterface::portsDeviceSelectHtml_(uint32_t selected_node_id, bool stack_view) const
{
    return _stack_ops.portsDeviceSelectHtml_(selected_node_id, stack_view);
}

String WebInterface::stackBusesStatusText_(uint32_t node_id) const
{
    return _stack_ops.stackBusesStatusText_(node_id);
}

String WebInterface::stackPortsStatusText_(uint32_t node_id) const
{
    return _stack_ops.stackPortsStatusText_(node_id);
}

bool WebInterface::isStackBusesView_(uint32_t node_id) const
{
    return _stack_ops.isStackBusesView_(node_id);
}

bool WebInterface::isStackPortsView_(uint32_t node_id) const
{
    return _stack_ops.isStackPortsView_(node_id);
}

uint32_t WebInterface::parseStackNodeIdParam_(AsyncWebServerRequest *request) const
{
    return _stack_ops.parseStackNodeIdParam_(request);
}

void WebInterface::handleStackFrame_(uint32_t node_id, const StackFrame &frame)
{
    _stack_ops.handleStackFrame_(node_id, frame);
}

bool WebInterface::requestStackPorts_(uint32_t node_id)
{
    return _stack_ops.requestStackPorts_(node_id);
}

bool WebInterface::refreshStackPorts_(uint32_t node_id)
{
    return _stack_ops.refreshStackPorts_(node_id);
}

bool WebInterface::requestStackExtenders_(uint32_t node_id)
{
    return _stack_ops.requestStackExtenders_(node_id);
}

bool WebInterface::requestStackI2c_(uint32_t node_id, bool run)
{
    return _stack_ops.requestStackI2c_(node_id, run);
}

bool WebInterface::requestStackOw_(uint32_t node_id, bool run)
{
    return _stack_ops.requestStackOw_(node_id, run);
}

bool WebInterface::requestStackTempSensors_(uint32_t node_id)
{
    return _stack_ops.requestStackTempSensors_(node_id);
}

bool WebInterface::refreshStackTempSensors_(uint32_t node_id)
{
    return _stack_ops.refreshStackTempSensors_(node_id);
}

bool WebInterface::requestStackPlcStatus_(uint32_t node_id)
{
    return _stack_ops.requestStackPlcStatus_(node_id);
}

bool WebInterface::requestStackRtcStatus_(uint32_t node_id)
{
    return _stack_ops.requestStackRtcStatus_(node_id);
}

uint16_t WebInterface::nextStackCmdId_()
{
    return _stack_ops.nextStackCmdId_();
}

String WebInterface::ringDeviceSelectHtml_(uint32_t selected_node_id, bool stack_view) const
{
    return WebInterfaceControllersRingHelper::ringDeviceSelectHtml_(*this, selected_node_id, stack_view);
}

bool WebInterface::isStackRingView_(uint32_t node_id) const
{
    return WebInterfaceControllersRingHelper::isStackRingView_(*this, node_id);
}

bool WebInterface::sendStackRingCmd_(uint32_t node_id, bool set_state, bool state)
{
    return WebInterfaceControllersRingHelper::sendStackRingCmd_(*this, node_id, set_state, state);
}

bool WebInterface::sendStackRingCmdAll_(bool set_state, bool state)
{
    return WebInterfaceControllersRingHelper::sendStackRingCmdAll_(*this, set_state, state);
}

void WebInterface::onStackFrame_(void *ctx, uint32_t node_id, const StackFrame &frame)
{
    WebInterfaceControllersRingHelper::onStackFrame_(ctx, node_id, frame);
}

    String WebInterface::listI2cHtml_()
{
    return _controllers_ops.listI2cHtml_();
}



    String WebInterface::listStackI2cHtml_(uint32_t node_id) const
{
    return _controllers_ops.listStackI2cHtml_(node_id);
}



    String WebInterface::stackNodesBlockHtml_() const
{
    return _controllers_ops.stackNodesBlockHtml_();
}



    String WebInterface::listStackNodesStatusHtml_() const
{
    return _controllers_ops.listStackNodesStatusHtml_();
}



    String WebInterface::listStackNodesHtml_() const
{
    return _controllers_ops.listStackNodesHtml_();
}



    String WebInterface::globalUsedPortsJson_(PortIO::PinType type) const
{
    return _controllers_ops.globalUsedPortsJson_(type);
}

bool WebInterface::stackPortTypeMatch_(const StackCache::StackPortItem &it, PortIO::PinType type) const
{
    return _controllers_ops.stackPortTypeMatch_(it, type);
}



    String WebInterface::stackPortOptionsJson_(uint32_t node_id, PortIO::PinType type) const
{
    return _controllers_ops.stackPortOptionsJson_(node_id, type);
}



    String WebInterface::stackUsedPortsJson_(uint32_t node_id, PortIO::PinType type) const
{
    return _controllers_ops.stackUsedPortsJson_(node_id, type);
}



    String WebInterface::stackMeteoDs18OptionsJson_(uint32_t node_id) const
{
    return _controllers_ops.stackMeteoDs18OptionsJson_(node_id);
}



    String WebInterface::stackMeteoDs18UsedJson_(uint32_t node_id) const
{
    return _controllers_ops.stackMeteoDs18UsedJson_(node_id);
}



    String WebInterface::listOwHtml_()
{
    return _controllers_ops.listOwHtml_();
}



    String WebInterface::listStackOwHtml_(uint32_t node_id) const
{
    return _controllers_ops.listStackOwHtml_(node_id);
}



    void WebInterface::handleUpload_(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data,
                       size_t len, bool final)
{
    _controllers_ops.handleUpload_(request, filename, index, data, len, final);
}



    void WebInterface::handleOta_(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len,
                    bool final)
{
    _controllers_ops.handleOta_(request, filename, index, data, len, final);
}



    void WebInterface::handleUploadDone_(AsyncWebServerRequest *request)
{
    _controllers_ops.handleUploadDone_(request);
}



    void WebInterface::handleOtaDone_(AsyncWebServerRequest *request)
{
    _controllers_ops.handleOtaDone_(request);
}



    void WebInterface::handleWifiSave_(AsyncWebServerRequest *request)
{
    _controllers_ops.handleWifiSave_(request);
}




    void WebInterface::handleStackSave_(AsyncWebServerRequest *request)
{
    _controllers_ops.handleStackSave_(request);
}



    void WebInterface::handleStackGenKey_(AsyncWebServerRequest *request)
{
    _controllers_ops.handleStackGenKey_(request);
}



    void WebInterface::handleDeviceSave_(AsyncWebServerRequest *request)
{
    _controllers_ops.handleDeviceSave_(request);
}



    void WebInterface::handleReboot_(AsyncWebServerRequest *request)
{
    _controllers_ops.handleReboot_(request);
}



    void WebInterface::handleFileDownload_(AsyncWebServerRequest *request)
{
    _controllers_ops.handleFileDownload_(request);
}



    void WebInterface::handleDelete_(AsyncWebServerRequest *request)
{
    _controllers_ops.handleDelete_(request);
}



    void WebInterface::handleUiHash_(AsyncWebServerRequest *request)
{
    _controllers_ops.handleUiHash_(request);
}



    bool WebInterface::hasUsersRegistryWebAuth_() const
{
    return _controllers_ops.hasUsersRegistryWebAuth_();
}



    bool WebInterface::checkLegacyAdminAuth_(const String &user, const String &pass) const
{
    return _controllers_ops.checkLegacyAdminAuth_(user, pass);
}



    bool WebInterface::findUsersRegistryAuth_(const String &user, const String &pass, size_t &user_idx) const
{
    return _controllers_ops.findUsersRegistryAuth_(user, pass, user_idx);
}



    bool WebInterface::checkUsersRegistryAuth_(const String &user, const String &pass) const
{
    return _controllers_ops.checkUsersRegistryAuth_(user, pass);
}



    bool WebInterface::checkAuth_(AsyncWebServerRequest *request, bool *set_cookie, bool require_session)
{
    return _controllers_ops.checkAuth_(request, set_cookie, require_session);
}



    bool WebInterface::checkAuthApi_(AsyncWebServerRequest *request, bool *set_cookie)
{
    return _controllers_ops.checkAuthApi_(request, set_cookie);
}



    const UsersRegistry::User *WebInterface::sessionUser_() const
{
    return _controllers_ops.sessionUser_();
}



    uint8_t WebInterface::aclUnitByNodeId_(uint32_t node_id) const
{
    return _controllers_ops.aclUnitByNodeId_(node_id);
}



    bool WebInterface::webAclControllerAllowed_(UsersRegistry::AclController ctrl, uint32_t node_id) const
{
    return _controllers_ops.webAclControllerAllowed_(ctrl, node_id);
}



    bool WebInterface::webAclCanViewItem_(UsersRegistry::AclController ctrl, uint16_t item_id, uint32_t node_id) const
{
    return _controllers_ops.webAclCanViewItem_(ctrl, item_id, node_id);
}



    bool WebInterface::webAclCanControlItem_(UsersRegistry::AclController ctrl, uint16_t item_id, uint32_t node_id) const
{
    return _controllers_ops.webAclCanControlItem_(ctrl, item_id, node_id);
}



    bool WebInterface::webSessionIsAdmin_() const
{
    return _controllers_ops.webSessionIsAdmin_();
}



    bool WebInterface::requireWebAdmin_(AsyncWebServerRequest *request, bool *set_cookie)
{
    return _controllers_ops.requireWebAdmin_(request, set_cookie);
}



    bool WebInterface::requireWebAclController_(AsyncWebServerRequest *request, bool *set_cookie,
                                  UsersRegistry::AclController ctrl, uint32_t node_id)
{
    return _controllers_ops.requireWebAclController_(request, set_cookie, ctrl, node_id);
}



    String WebInterface::requestIp_(AsyncWebServerRequest *request) const
{
    return _controllers_ops.requestIp_(request);
}



    String WebInterface::wifiIp_() const
{
    return _controllers_ops.wifiIp_();
}



    String WebInterface::wifiStaSegment_() const
{
    return _controllers_ops.wifiStaSegment_();
}



    String WebInterface::wifiStaStatus_() const
{
    return _controllers_ops.wifiStaStatus_();
}



    String WebInterface::navHtml_() const
{
    return _controllers_ops.navHtml_();
}

    bool WebInterface::hasGroups_(uint32_t node_id) const
{
    if (node_id != 0)
    {
        const auto *cache = _stack_cache ? _stack_cache->groupsCache(node_id) : nullptr;
        return cache && cache->has_data && cache->items && cache->item_count > 0;
    }
    return _configs_manager && _configs_manager->groupCount() > 0;
}

    uint8_t WebInterface::firstGroupId_(uint32_t node_id) const
{
    if (node_id != 0)
    {
        const auto *cache = _stack_cache ? _stack_cache->groupsCache(node_id) : nullptr;
        if (!cache || !cache->has_data || !cache->items)
            return 0;
        for (size_t i = 0; i < cache->item_count; ++i)
        {
            const auto &g = cache->items[i];
            if (g.id != 0 && g.name[0] != '\0')
                return g.id;
        }
        return 0;
    }
    if (!_configs_manager)
        return 0;
    for (size_t i = 0; i < _configs_manager->groupCount(); ++i)
    {
        ConfigsManagerIface::GroupConfig g;
        if (_configs_manager->groupByIndex(i, g) && g.id != 0 && g.name.length() != 0)
            return g.id;
    }
    return 0;
}

    String WebInterface::groupVisibilityStyleAttr_(uint8_t group_id, uint32_t node_id) const
{
    if (!hasGroups_(node_id))
        return "";
    const uint8_t selected_group_id = firstGroupId_(node_id);
    if (selected_group_id == 0 || group_id == selected_group_id)
        return "";
    return " style=\"display:none\"";
}

    String WebInterface::groupOptionsHtml_(uint8_t selected_group_id, bool include_none, bool disabled_if_empty, uint32_t node_id) const
{
    String html;
    const bool has_groups = hasGroups_(node_id);
    if (include_none)
    {
        html += "<option value=\"0\"";
        if (selected_group_id == 0)
            html += " selected";
        html += ">";
        html += has_groups ? WebUiRu::GroupsPage::kNoGroup : WebUiRu::GroupsPage::kNoGroups;
        html += "</option>";
    }
    if (node_id != 0)
    {
        const auto *cache = _stack_cache ? _stack_cache->groupsCache(node_id) : nullptr;
        if (!cache || !cache->has_data || !cache->items)
            return html;
        for (size_t i = 0; i < cache->item_count; ++i)
        {
            const auto &g = cache->items[i];
            if (g.id == 0 || g.name[0] == '\0')
                continue;
            html += "<option value=\"";
            html += String((unsigned)g.id);
            html += "\"";
            if (selected_group_id == g.id)
                html += " selected";
            html += ">";
            appendHtmlEscaped_(html, g.name);
            html += "</option>";
        }
        if (!has_groups && disabled_if_empty && !include_none)
            html += String("<option value=\"0\" selected>") + WebUiRu::GroupsPage::kNoGroups + "</option>";
        return html;
    }
    if (!_configs_manager)
        return html;
    for (size_t i = 0; i < _configs_manager->groupCount(); ++i)
    {
        ConfigsManagerIface::GroupConfig g;
        if (!_configs_manager->groupByIndex(i, g) || g.id == 0 || g.name.length() == 0)
            continue;
        html += "<option value=\"";
        html += String((unsigned)g.id);
        html += "\"";
        if (selected_group_id == g.id)
            html += " selected";
        html += ">";
        appendHtmlEscaped_(html, g.name);
        html += "</option>";
    }
    if (!has_groups && disabled_if_empty && !include_none)
        html += String("<option value=\"0\" selected>") + WebUiRu::GroupsPage::kNoGroups + "</option>";
    return html;
}

String WebInterface::groupFilterHtml_(const char *select_id, uint32_t node_id) const
{
    if (!hasGroups_(node_id) || !select_id)
        return "";
    const uint8_t selected_group_id = firstGroupId_(node_id);
    String html;
    html.reserve(640);
    html += "<div class=\"row\">";
    html += String("<span class=\"muted\">") + WebUiRu::GroupsPage::kLabel + "</span>";
    html += "<select id=\"";
    appendHtmlEscaped_(html, select_id);
    html += "\" class=\"field mini\">";
    html += groupOptionsHtml_(selected_group_id, false, false, node_id);
    html += "<option value=\"0\"";
    if (selected_group_id == 0)
        html += " selected";
    html += ">";
    html += WebUiRu::GroupsPage::kAll;
    html += "</option>";
    html += "</select></div>";
    html += "<script>(function(){const init=()=>{const sel=document.getElementById('";
    html += select_id;
    html += "');if(!sel)return;const apply=()=>{const v=String(sel.value||'0');document.querySelectorAll('.js-group-item').forEach((el)=>{const g=String(el.getAttribute('data-group-id')||'0');el.style.display=(v==='0'||g===v)?'':'none';});};sel.addEventListener('change',apply);apply();};if(document.readyState==='loading')document.addEventListener('DOMContentLoaded',init,{once:true});else init();})();</script>";
    return html;
}

String WebInterface::topFiltersBackHtml_() const
{
    return String("<div class=\"row\"><a href=\"/controllers\" class=\"field mini\" style=\"display:inline-flex;align-items:center;justify-content:center;flex:0 0 auto;width:auto;padding:6px 14px;text-decoration:none;font-weight:600;color:#d7f3ff;background:linear-gradient(180deg, rgba(14,165,233,.18) 0%, rgba(14,165,233,.10) 100%);border-color:rgba(56,189,248,.28)\">Назад</a></div>");
}

    String WebInterface::composeTopFiltersHtml_(const String &device_html, const String &group_html) const
{
    if (!device_html.length())
        return group_html;
    if (!group_html.length())
        return device_html;

    const String row_prefix = "<div class=\"row\">";
    const String row_suffix = "</div>";
    String device_inner = device_html;
    String group_inner = group_html;
    String group_tail;

    if (device_inner.startsWith(row_prefix) && device_inner.endsWith(row_suffix))
        device_inner = device_inner.substring((int)row_prefix.length(), (int)device_inner.length() - (int)row_suffix.length());

    if (group_inner.startsWith(row_prefix))
    {
        const int close_pos = group_inner.indexOf(row_suffix);
        if (close_pos >= 0)
        {
            group_tail = group_inner.substring(close_pos + (int)row_suffix.length());
            group_inner = group_inner.substring((int)row_prefix.length(), close_pos);
        }
    }

    String out;
    const String back_html = topFiltersBackHtml_();
    String back_inner = back_html;
    if (back_inner.startsWith(row_prefix) && back_inner.endsWith(row_suffix))
        back_inner = back_inner.substring((int)row_prefix.length(), (int)back_inner.length() - (int)row_suffix.length());

    out.reserve(device_html.length() + group_html.length() + back_html.length() + 48);
    out += row_prefix;
    out += device_inner;
    out += group_inner;
    out += back_inner;
    out += row_suffix;
    out += group_tail;
    return out;
}

    uint8_t WebInterface::parseGroupIdParam_(AsyncWebServerRequest *request, const String &name) const
{
    if (!request)
        return 0;
    const String value = paramValue_(request, name);
    if (!value.length())
        return 0;
    const int v = value.toInt();
    if (v < 0 || v > 255)
        return 0;
    return (uint8_t)v;
}



    String WebInterface::deviceName_() const
{
    return _controllers_ops.deviceName_();
}



    ConfigsManagerIface::StackRole WebInterface::stackRole_() const
{
    return _controllers_ops.stackRole_();
}



    String WebInterface::stackMasterHost_() const
{
    return _controllers_ops.stackMasterHost_();
}



    bool WebInterface::stackFallbackEnabled_() const
{
    return _controllers_ops.stackFallbackEnabled_();
}



    String WebInterface::stackFallbackHost_() const
{
    return _controllers_ops.stackFallbackHost_();
}



    bool WebInterface::stackSlaveController_() const
{
    return _controllers_ops.stackSlaveController_();
}



    String WebInterface::stackApiKey_() const
{
    return _controllers_ops.stackApiKey_();
}



    bool WebInterface::cloudEnabled_() const
{
    return _controllers_ops.cloudEnabled_();
}



    String WebInterface::cloudHost_() const
{
    return _controllers_ops.cloudHost_();
}



    uint16_t WebInterface::cloudPort_() const
{
    return _controllers_ops.cloudPort_();
}



    String WebInterface::cloudPath_() const
{
    return _controllers_ops.cloudPath_();
}



    bool WebInterface::cloudUseSsl_() const
{
    return _controllers_ops.cloudUseSsl_();
}



    uint32_t WebInterface::cloudReconnectMs_() const
{
    return _controllers_ops.cloudReconnectMs_();
}



    uint32_t WebInterface::cloudEventMs_() const
{
    return _controllers_ops.cloudEventMs_();
}



    String WebInterface::cloudApiKey_() const
{
    return _controllers_ops.cloudApiKey_();
}



    String WebInterface::cloudFwVersion_() const
{
    return _controllers_ops.cloudFwVersion_();
}


    uint32_t WebInterface::cloudDeviceId_() const
{
    return _controllers_ops.cloudDeviceId_();
}



    bool WebInterface::cloudConnected_() const
{
    return _controllers_ops.cloudConnected_();
}

const char *WebInterface::stackRoleName_(ConfigsManagerIface::StackRole role)
{
    return _controllers_ops.stackRoleName_(role);
}



    bool WebInterface::saveWifiConfig_()
{
    return _controllers_ops.saveWifiConfig_();
}



    bool WebInterface::isAllowedExt_(const String &path) const
{
    return _controllers_ops.isAllowedExt_(path);
}

bool WebInterface::parseSocketPort_(const String &input, uint8_t &out)
{
    return _controllers_ops.parseSocketPort_(input, out);
}

const char *WebInterface::displaySlotKindName_(DisplaySlotKind kind) const
{
    return _controllers_ops.displaySlotKindName_(kind);
}

const char *WebInterface::displaySlotFieldName_(DisplaySlotField field) const
{
    return _controllers_ops.displaySlotFieldName_(field);
}

bool WebInterface::parseDisplaySlotKind_(const String &input, DisplaySlotKind &out)
{
    return _controllers_ops.parseDisplaySlotKind_(input, out);
}

bool WebInterface::parseDisplaySlotField_(const String &input, DisplaySlotField &out)
{
    return _controllers_ops.parseDisplaySlotField_(input, out);
}

bool WebInterface::parseUint_(const String &input, uint16_t &out)
{
    return _controllers_ops.parseUint_(input, out);
}

bool WebInterface::parseUint_(const String &input, uint32_t &out)
{
    return _controllers_ops.parseUint_(input, out);
}

bool WebInterface::parseMeteoType_(const String &input, MeteoController::SensorType &out)
{
    return _controllers_ops.parseMeteoType_(input, out);
}

MeteoController::SensorType WebInterface::parseMeteoTypeName_(const char *input) const
{
    return _controllers_ops.parseMeteoTypeName_(input);
}

bool WebInterface::parseSecurityType_(const String &input, SecurityController::SensorType &out)
{
    return _controllers_ops.parseSecurityType_(input, out);
}

bool WebInterface::parseMeteoPin_(const String &input, uint8_t &out)
{
    return _controllers_ops.parseMeteoPin_(input, out);
}

bool WebInterface::parseMeteoAddr_(const String &input, uint8_t out[MeteoController::kAddrLen], bool &set)
{
    return _controllers_ops.parseMeteoAddr_(input, out, set);
}

bool WebInterface::parseSecurityKeyHex_(const String &s, uint8_t out[8])
{
    return _controllers_ops.parseSecurityKeyHex_(s, out);
}

bool WebInterface::parseThermoSensor_(const String &input, uint8_t &out, uint32_t &out_node)
{
    return _controllers_ops.parseThermoSensor_(input, out, out_node);
}

bool WebInterface::parseThermoMode_(const String &input, ThermoController::Mode &out)
{
    return _controllers_ops.parseThermoMode_(input, out);
}

bool WebInterface::parseThermoFloat_(const String &input, float &out)
{
    return _controllers_ops.parseThermoFloat_(input, out);
}


    String WebInterface::paramValue_(AsyncWebServerRequest *request, const String &name)
{
        if (!request || !request->hasParam(name, true))
            return "";
        return request->getParam(name, true)->value();
    }

String WebInterface::paramValueAny_(AsyncWebServerRequest *request, const String &name)
{
        if (!request)
            return "";
        if (request->hasParam(name, true))
            return request->getParam(name, true)->value();
        if (request->hasParam(name, false))
            return request->getParam(name, false)->value();
        if (request->hasParam(name))
            return request->getParam(name)->value();
        return "";
    }

void WebInterface::appendHtmlEscaped_(String &out, const char *in)
{
        if (!in)
            return;
        while (*in)
        {
            switch (*in)
            {
            case '&':
                out += "&amp;";
                break;
            case '<':
                out += "&lt;";
                break;
            case '>':
                out += "&gt;";
                break;
            case '"':
                out += "&quot;";
                break;
            case '\'':
                out += "&#39;";
                break;
            default:
                out += *in;
                break;
            }
            ++in;
        }
    }

void WebInterface::appendHtmlEscaped_(String &out, const String &in)
{
        appendHtmlEscaped_(out, in.c_str());
    }

void WebInterface::copyStr_(char *dst, size_t size, const char *src)
{
        if (!dst || size == 0)
            return;
        if (!src)
        {
            dst[0] = '\0';
            return;
        }
        strlcpy(dst, src, size);
    }

String WebInterface::maskSecretValue_(const String &value)
{
        if (value.length() == 0)
            return String("");
        String out;
        out.reserve(value.length());
        for (size_t i = 0; i < value.length(); ++i)
            out += '*';
        return out;
    }

bool WebInterface::isMaskedSecret_(const String &input, const String &actual)
{
        if (actual.length() == 0 || input.length() != actual.length())
            return false;
        for (size_t i = 0; i < input.length(); ++i)
            if (input[i] != '*')
                return false;
        return true;
    }

String WebInterface::safeHtmlValue_(const String &value, const char *fallback)
{
        if (value.length() == 0)
            return String(fallback);
        String out;
        out.reserve(value.length() + 8);
        appendHtmlEscaped_(out, value.c_str());
        return out;
    }

String WebInterface::sanitizeUtf8_(const String &in)
{
        if (isValidUtf8_(in))
            return in;
        return cp1251ToUtf8_(in);
    }

bool WebInterface::isValidUtf8_(const String &in)
{
        size_t i = 0;
        while (i < (size_t)in.length())
        {
            const uint8_t c = (uint8_t)in[i];
            if (c < 0x80)
            {
                ++i;
                continue;
            }
            size_t need = 0;
            if ((c & 0xE0) == 0xC0)
            {
                if (c < 0xC2)
                    return false;
                need = 1;
            }
            else if ((c & 0xF0) == 0xE0)
            {
                need = 2;
            }
            else if ((c & 0xF8) == 0xF0)
            {
                if (c > 0xF4)
                    return false;
                need = 3;
            }
            else
            {
                return false;
            }

            if (i + need >= (size_t)in.length())
                return false;

            for (size_t j = 1; j <= need; ++j)
            {
                const uint8_t cc = (uint8_t)in[i + j];
                if ((cc & 0xC0) != 0x80)
                    return false;
            }
            i += need + 1;
        }
        return true;
    }

void WebInterface::appendUtf8_(String &out, uint16_t code)
{
        if (code < 0x80)
        {
            out += (char)code;
            return;
        }
        if (code < 0x800)
        {
            out += (char)(0xC0 | (code >> 6));
            out += (char)(0x80 | (code & 0x3F));
            return;
        }
        out += (char)(0xE0 | (code >> 12));
        out += (char)(0x80 | ((code >> 6) & 0x3F));
        out += (char)(0x80 | (code & 0x3F));
    }

String WebInterface::cp1251ToUtf8_(const String &in)
{
        String out;
        out.reserve(in.length() * 2);
        for (size_t i = 0; i < (size_t)in.length(); ++i)
        {
            const uint8_t c = (uint8_t)in[i];
            if (c < 0x80)
            {
                out += (char)c;
                continue;
            }
            uint16_t code = '?';
            if (c == 0xA8)
                code = 0x0401;
            else if (c == 0xB8)
                code = 0x0451;
            else if (c >= 0xC0 && c <= 0xFF)
                code = (uint16_t)(0x0410 + (c - 0xC0));
            else
                code = '?';
            appendUtf8_(out, code);
        }
        return out;
    }

bool WebInterface::parseBasicAuth_(AsyncWebServerRequest *request, String &user, String &pass)
{
        if (!request || !request->hasHeader("Authorization"))
            return false;
        const AsyncWebHeader *h = request->getHeader("Authorization");
        if (!h)
            return false;
        String value = h->value();
        String vlow = value;
        vlow.toLowerCase();
        if (!vlow.startsWith("basic"))
            return false;
        int sp = -1;
        for (size_t i = 5; i < value.length(); ++i)
        {
            const char c = value.charAt(i);
            if (c == ' ' || c == '\t')
            {
                sp = (int)i;
                break;
            }
        }
        if (sp < 0)
            return false;
        String b64 = value.substring(sp + 1);
        b64.trim();
        String decoded;
        if (!decodeBase64_(b64, decoded))
            return false;
        const int colon = decoded.indexOf(':');
        if (colon < 0)
            return false;
        user = decoded.substring(0, colon);
        pass = decoded.substring(colon + 1);
        return true;
    }

int8_t WebInterface::b64Index_(char c)
{
        if (c >= 'A' && c <= 'Z')
            return (int8_t)(c - 'A');
        if (c >= 'a' && c <= 'z')
            return (int8_t)(26 + (c - 'a'));
        if (c >= '0' && c <= '9')
            return (int8_t)(52 + (c - '0'));
        if (c == '+' || c == '-')
            return 62;
        if (c == '/' || c == '_')
            return 63;
        return -1;
    }

bool WebInterface::decodeBase64_(const String &in, String &out)
{
        out = "";
        out.reserve((in.length() * 3) / 4 + 1);
        uint32_t acc = 0;
        int bits = 0;
        for (size_t i = 0; i < in.length(); ++i)
        {
            const char c = in.charAt(i);
            if (c == ' ' || c == '\r' || c == '\n' || c == '\t')
                continue;
            if (c == '=')
                break;
            const int8_t v = b64Index_(c);
            if (v < 0)
                return false;
            acc = (acc << 6) | (uint32_t)v;
            bits += 6;
            if (bits >= 8)
            {
                bits -= 8;
                const uint8_t b = (uint8_t)((acc >> bits) & 0xFFu);
                out += (char)b;
            }
        }
        return true;
    }

void WebInterface::requestBasicAuth_(AsyncWebServerRequest *request)
{
        if (!request)
            return;
        auto *response = request->beginResponse(401);
        response->addHeader("WWW-Authenticate", "Basic realm=\"FCPLC\"");
        request->send(response);
    }



    String WebInterface::sanitizeUploadName_(const String &filename) const
{
        String name = filename;
        name.trim();
        if (!name.length() || name.indexOf('/') >= 0 || name.indexOf('\\') >= 0 || name.indexOf("..") >= 0)
            return "";
        return String("/") + name;
    }



    String WebInterface::sanitizePath_(const String &path) const
{
        String out = path;
        out.trim();
        if (!out.length())
            return "";
        if (!out.startsWith("/"))
            out = "/" + out;
        if (out.indexOf("..") >= 0 || out.indexOf('\\') >= 0)
            return "";
        return out;
    }



    float WebInterface::boardTemp_() const
{
        return _plc ? _plc->boardTemp() : 0.0f;
    }



    float WebInterface::cpuTemp_() const
{
        return _plc ? _plc->cpuTemp() : 0.0f;
    }



    String WebInterface::formatTemp_(float temp_c) const
{
        char buf[16] = {};
        dtostrf(temp_c, 0, 1, buf);
        return String(buf) + " C";
    }



    String WebInterface::rtcTimeStr_() const
{
        if (!_rtc)
            return String("n/a");
        Ds3231Mz::DateTime dt{};
        if (!_rtc->Time(dt))
            return String("n/a");
        char buf[24] = {};
        snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u:%02u",
                 (unsigned)dt.year, (unsigned)dt.month, (unsigned)dt.day,
                 (unsigned)dt.hour, (unsigned)dt.minute, (unsigned)dt.second);
        return String(buf);
    }



    String WebInterface::rtcDateStr_() const
{
        if (!_rtc)
            return String("n/a");
        Ds3231Mz::DateTime dt{};
        if (!_rtc->Time(dt))
            return String("n/a");
        char buf[16] = {};
        snprintf(buf, sizeof(buf), "%04u-%02u-%02u",
                 (unsigned)dt.year, (unsigned)dt.month, (unsigned)dt.day);
        return String(buf);
    }



    String WebInterface::rtcTimeOnlyStr_() const
{
        if (!_rtc)
            return String("n/a");
        Ds3231Mz::DateTime dt{};
        if (!_rtc->Time(dt))
            return String("n/a");
        char buf[16] = {};
        snprintf(buf, sizeof(buf), "%02u:%02u:%02u",
                 (unsigned)dt.hour, (unsigned)dt.minute, (unsigned)dt.second);
        return String(buf);
    }



    bool WebInterface::setRtc_(const String &date, const String &time)
{
        if (!_rtc)
            return false;
        const int p1 = date.indexOf('-');
        const int p2 = (p1 >= 0) ? date.indexOf('-', p1 + 1) : -1;
        const int t1 = time.indexOf(':');
        const int t2 = (t1 >= 0) ? time.indexOf(':', t1 + 1) : -1;
        if (p1 <= 0 || p2 <= p1 || t1 <= 0 || t2 <= t1)
            return false;
        const uint16_t year = (uint16_t)date.substring(0, p1).toInt();
        const uint8_t month = (uint8_t)date.substring(p1 + 1, p2).toInt();
        const uint8_t day = (uint8_t)date.substring(p2 + 1).toInt();
        const uint8_t hour = (uint8_t)time.substring(0, t1).toInt();
        const uint8_t min = (uint8_t)time.substring(t1 + 1, t2).toInt();
        const uint8_t sec = (uint8_t)time.substring(t2 + 1).toInt();
        if (year < 2000 || year > 2099 || month < 1 || month > 12 || day < 1 || day > 31)
            return false;
        if (hour > 23 || min > 59 || sec > 59)
            return false;
        Ds3231Mz::DateTime dt{};
        dt.year = year;
        dt.month = month;
        dt.day = day;
        dt.day_of_week = calcDow_(year, month, day);
        dt.hour = hour;
        dt.minute = min;
        dt.second = sec;
        return _rtc->setTime(dt);
    }

uint8_t WebInterface::calcDow_(uint16_t y, uint8_t m, uint8_t d)
{
        static const uint8_t t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
        if (m < 3)
            y -= 1;
        const uint8_t dow = (uint8_t)((y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7);
        return (uint8_t)(dow + 1);
    }



    void WebInterface::notifyRingPress_(bool stack_view, uint32_t node_id)
{
        String msg = WebUiRu::WebCore::kRingWebButton;
        if (stack_view)
        {
            msg += " (";
            msg += WebUiRu::WebCore::kStackShort;
            if (node_id)
            {
                msg += F(" ");
                msg += stackNodeIdHex_(node_id);
            }
            msg += ")";
        }
        else
        {
            msg += " (";
            msg += WebUiRu::WebCore::kLocalShort;
            msg += ")";
        }
        if (_log)
            _log->info(F("RING"), F("%s"), msg.c_str());
        sendTelegramNotify_(msg);
    }



    void WebInterface::sendTelegramNotify_(const String &msg)
{
        if (!_tgbot_bot || !_tgbot_menu)
            return;
        const auto users = _tgbot_menu->allowedUsers();
        if (users.empty())
            return;
        for (size_t i = 0; i < users.size; ++i)
        {
            const auto &user = users[i];
            if (!user.enabled || !user.is_notify || user.chat_id == 0)
                continue;
            _tgbot_bot->sendText(user.chat_id, msg);
        }
    }

void WebInterface::appendJsonEscaped_(String &out, const String &value)
{
        for (size_t i = 0; i < (size_t)value.length(); ++i)
        {
            const char c = value[i];
            switch (c)
            {
            case '\\':
                out += "\\\\";
                break;
            case '"':
                out += "\\\"";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if ((unsigned char)c < 0x20)
                    out += ' ';
                else
                    out += c;
                break;
            }
        }
    }



    float WebInterface::rtcTemp_() const
{
        if (!_rtc)
            return 0.0f;
        float temp_c = 0.0f;
        if (!_rtc->readTemp(temp_c))
            return 0.0f;
        return temp_c;
    }



    String WebInterface::fanStatusIcon_() const
{
        if (!_plc)
            return "n/a";
        const bool on = _plc->fanStatus();
        String out = "<span class=\"status-dot ";
        out += on ? "status-on" : "status-off";
        out += "\" title=\"";
        out += on ? WebUiRu::WebCore::kEnabled : WebUiRu::WebCore::kDisabled;
        out += "\"></span>";
        return out;
    }



    String WebInterface::fanStatusIcon_(bool on) const
{
        String out = "<span class=\"status-dot ";
        out += on ? "status-on" : "status-off";
        out += "\" title=\"";
        out += on ? WebUiRu::WebCore::kEnabled : WebUiRu::WebCore::kDisabled;
        out += "\"></span>";
        return out;
    }



    void WebInterface::sendHtml_(AsyncWebServerRequest *request, const String &page, bool set_cookie)
{
        String out = page;
        injectAutoRefresh_(out);
        auto *response = request->beginResponse(200, "text/html; charset=utf-8", out);
        if (set_cookie)
            response->addHeader("Set-Cookie", sessionCookie_());
        request->send(response);
    }



    void WebInterface::sendText_(AsyncWebServerRequest *request, int code, const char *type, const String &text, bool set_cookie)
{
        String content_type = type;
        if (content_type.startsWith("text/") && content_type.indexOf("charset=") < 0)
            content_type += "; charset=utf-8";
        auto *response = request->beginResponse(code, content_type, text);
        if (set_cookie)
            response->addHeader("Set-Cookie", sessionCookie_());
        request->send(response);
    }



    void WebInterface::sendHtmlRaw_(AsyncWebServerRequest *request, const String &page, bool set_cookie)
{
        auto *response = request->beginResponse(200, "text/html; charset=utf-8", page);
        if (set_cookie)
            response->addHeader("Set-Cookie", sessionCookie_());
        request->send(response);
    }



    void WebInterface::sendRedirect_(AsyncWebServerRequest *request, const char *path, bool set_cookie)
{
        auto *response = request->beginResponse(302);
        response->addHeader("Location", path);
        if (set_cookie)
            response->addHeader("Set-Cookie", sessionCookie_());
        request->send(response);
    }



    void WebInterface::sendRedirect_(AsyncWebServerRequest *request, const String &path, bool set_cookie)
{
        sendRedirect_(request, path.c_str(), set_cookie);
    }



    String WebInterface::sessionCookie_() const
{
        String cookie = String("plc_session=") + _session_token +
                        "; Max-Age=" + String(_session_ttl_ms / 1000) +
                        "; Path=/; HttpOnly; SameSite=Strict";
        return cookie;
    }



    void WebInterface::injectAutoRefresh_(String &page) const
{
        const int idx = page.lastIndexOf("</body>");
        if (idx < 0)
            return;
        String out;
        out.reserve(page.length() + 512);
        out += page.substring(0, idx);
        out += FPSTR(kWebAutoRefreshScript);
        out += page.substring(idx);
        page = out;
    }

uint32_t WebInterface::fnv1a_(uint32_t hash, const uint8_t *data, size_t len)
{
        for (size_t i = 0; i < len; ++i)
        {
            hash ^= data[i];
            hash *= 16777619u;
        }
        return hash;
    }

void WebInterface::hashAdd_(uint32_t &hash, const String &value)
{
        hash = fnv1a_(hash, reinterpret_cast<const uint8_t *>(value.c_str()), value.length());
    }

void WebInterface::hashAdd_(uint32_t &hash, const char *value)
{
        if (!value)
            return;
        hash = fnv1a_(hash, reinterpret_cast<const uint8_t *>(value), strlen(value));
    }

void WebInterface::hashAdd_(uint32_t &hash, uint32_t value)
{
        hash = fnv1a_(hash, reinterpret_cast<const uint8_t *>(&value), sizeof(value));
    }

void WebInterface::hashAdd_(uint32_t &hash, int32_t value)
{
        hash = fnv1a_(hash, reinterpret_cast<const uint8_t *>(&value), sizeof(value));
    }

void WebInterface::hashAdd_(uint32_t &hash, const uint8_t *data, size_t len)
{
        if (!data || !len)
            return;
        hash = fnv1a_(hash, data, len);
    }

int32_t WebInterface::scaled10_(float value)
{
        if (value >= 0.0f)
            return (int32_t)(value * 10.0f + 0.5f);
        return (int32_t)(value * 10.0f - 0.5f);
    }



    uint32_t WebInterface::uiPageHash_(const String &path)
{
        uint32_t hash = 2166136261u;
        hashAdd_(hash, path);
        hashAdd_(hash, _last_status);
        hashAdd_(hash, _wifi_status);
        hashAdd_(hash, _gsm_status);
        hashAdd_(hash, _tgbot_status);
        hashAdd_(hash, _cloud_status);
        hashAdd_(hash, _stack_status);
        hashAdd_(hash, _device_status);
        hashAdd_(hash, _sockets_status);
        hashAdd_(hash, _controllers_status);
        hashAdd_(hash, _lights_status);
        hashAdd_(hash, _meteo_status);
        hashAdd_(hash, _thermo_status);
        hashAdd_(hash, _tanks_status);
        hashAdd_(hash, _watering_status);
        hashAdd_(hash, _septic_status);
        hashAdd_(hash, _ring_status);
        hashAdd_(hash, _avr_status);
        hashAdd_(hash, _camera_status);
        hashAdd_(hash, _leak_status);
        hashAdd_(hash, _security_status);
        hashAdd_(hash, _rules_status);

        if (path == "/" || path == "/index")
        {
            hashAdd_(hash, deviceName_());
            hashAdd_(hash, stackRoleName_(stackRole_()));
            hashAdd_(hash, wifiIp_());
            hashAdd_(hash, listStackNodesStatusHtml_());
            return hash;
        }

        if (path == "/wifi")
        {
            hashAdd_(hash, _wifi.modeLabel());
            if (_wifi.staEnabled())
                hashAdd_(hash, _wifi.ssid());
            if (_wifi.apEnabled())
                hashAdd_(hash, _wifi.apSsid());
            hashAdd_(hash, wifiIp_());
            if (_gsm)
            {
                hashAdd_(hash, _gsm->imei());
                hashAdd_(hash, _gsm->imsi());
                hashAdd_(hash, _gsm->operatorName());
                hashAdd_(hash, _gsm->signalQuality());
                hashAdd_(hash, _gsm->regStatus());
                hashAdd_(hash, _gsm->lastError());
                hashAdd_(hash, _gsm->lastUrc());
                hashAdd_(hash, _gsm->lastCallNumber());
                hashAdd_(hash, _gsm->lastUssd());
            }
            return hash;
        }

        if (path == "/manage")
        {
            hashAdd_(hash, listFilesHtml_());
            return hash;
        }

        if (path == "/ports")
        {
            hashAdd_(hash, listPortsHtml_());
            hashAdd_(hash, listExtendersHtml_());
            return hash;
        }

        if (path == "/buses")
        {
            hashAdd_(hash, listI2cHtml_());
            hashAdd_(hash, listOwHtml_());
            return hash;
        }

        if (path == "/stack")
        {
            hashAdd_(hash, stackRoleName_(stackRole_()));
            hashAdd_(hash, stackMasterHost_());
            hashAdd_(hash, stackApiKey_());
            hashAdd_(hash, listStackNodesHtml_());
            if (stackRole_() == ConfigsManagerIface::StackRole::Slave && _stack_slave)
            {
                hashAdd_(hash, _stack_slave->nodeConnected() ? 1u : 0u);
                hashAdd_(hash, _stack_slave->linkReadyAfterHello() ? 1u : 0u);
            }
            return hash;
        }

        if (path == "/meteo")
        {
            if (_controllers)
            {
                MeteoController &meteo = _controllers->meteo();
                auto guard = meteo.lockGuard();
                for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
                {
                    const auto *cfg = meteo.configByIndex(i);
                    const auto *st = meteo.stateByIndex(i);
                    if (!cfg || !st)
                        continue;
                    hashAdd_(hash, (uint32_t)cfg->id);
                    hashAdd_(hash, cfg->enabled ? 1u : 0u);
                    hashAdd_(hash, cfg->name);
                    hashAdd_(hash, (uint32_t)cfg->type);
                    hashAdd_(hash, (uint32_t)cfg->dht_pin);
                    hashAdd_(hash, cfg->ds18_addr_set ? 1u : 0u);
                    if (cfg->ds18_addr_set)
                        hashAdd_(hash, cfg->ds18_addr, MeteoController::kAddrLen);
                    hashAdd_(hash, st->ok ? 1u : 0u);
                    hashAdd_(hash, st->has_temp ? 1u : 0u);
                    hashAdd_(hash, st->has_humidity ? 1u : 0u);
                    if (st->has_temp)
                        hashAdd_(hash, scaled10_(st->temp_c));
                    if (st->has_humidity)
                        hashAdd_(hash, scaled10_(st->humidity));
                    hashAdd_(hash, st->last_read_ms);
                }
            }
            if (_stack_cache && _stack_master)
            {
                const size_t count = _stack_master->nodeCount();
                for (size_t i = 0; i < count; ++i)
                {
                    const uint32_t node_id = _stack_master->nodeIdAt(i);
                    const auto *cache = _stack_cache->meteoCache(node_id);
                    if (!cache)
                        continue;
                    hashAdd_(hash, node_id);
                    hashAdd_(hash, cache->has_data ? 1u : 0u);
                    hashAdd_(hash, cache->pending ? 1u : 0u);
                    hashAdd_(hash, cache->last_ok ? 1u : 0u);
                    hashAdd_(hash, cache->last_error);
                    hashAdd_(hash, (uint32_t)cache->item_count);
                    for (size_t j = 0; j < cache->item_count; ++j)
                    {
                        const auto &it = cache->items[j];
                        hashAdd_(hash, (uint32_t)it.id);
                        hashAdd_(hash, it.enabled ? 1u : 0u);
                        hashAdd_(hash, it.ok ? 1u : 0u);
                        hashAdd_(hash, it.has_temp ? 1u : 0u);
                        hashAdd_(hash, it.has_hum ? 1u : 0u);
                        if (it.has_temp)
                            hashAdd_(hash, scaled10_(it.temp_c));
                        if (it.has_hum)
                            hashAdd_(hash, scaled10_(it.hum));
                    }
                }
            }
            return hash;
        }

        if (path == "/thermo")
        {
            if (_controllers)
            {
                ThermoController &thermo = _controllers->thermo();
                auto guard = thermo.lockGuard();
                for (size_t i = 0; i < ThermoController::kDeviceCount; ++i)
                {
                    const auto *cfg = thermo.configByIndex(i);
                    const auto *st = thermo.stateByIndex(i);
                    if (!cfg || !st)
                        continue;
                    hashAdd_(hash, (uint32_t)cfg->id);
                    hashAdd_(hash, cfg->enabled ? 1u : 0u);
                    hashAdd_(hash, (uint32_t)cfg->sensor_id);
                    hashAdd_(hash, cfg->sensor_node_id);
                    hashAdd_(hash, (uint32_t)cfg->heat_port);
                    hashAdd_(hash, (uint32_t)cfg->cool_port);
                    hashAdd_(hash, (uint32_t)cfg->button_port);
                    hashAdd_(hash, cfg->name);
                    hashAdd_(hash, (uint32_t)cfg->mode);
                    hashAdd_(hash, scaled10_(cfg->target_c));
                    hashAdd_(hash, scaled10_(cfg->hysteresis));
                    hashAdd_(hash, st->power_on ? 1u : 0u);
                    hashAdd_(hash, st->heat_on ? 1u : 0u);
                    hashAdd_(hash, st->cool_on ? 1u : 0u);
                }
            }
            if (_stack_cache && _stack_master)
            {
                const size_t count = _stack_master->nodeCount();
                for (size_t i = 0; i < count; ++i)
                {
                    const uint32_t node_id = _stack_master->nodeIdAt(i);
                    const auto *cache = _stack_cache->thermoCache(node_id);
                    if (!cache)
                        continue;
                    hashAdd_(hash, node_id);
                    hashAdd_(hash, cache->has_data ? 1u : 0u);
                    hashAdd_(hash, cache->pending ? 1u : 0u);
                    hashAdd_(hash, cache->last_ok ? 1u : 0u);
                    hashAdd_(hash, cache->last_error);
                    hashAdd_(hash, (uint32_t)cache->item_count);
                    for (size_t j = 0; j < cache->item_count; ++j)
                    {
                        const auto &it = cache->items[j];
                        hashAdd_(hash, (uint32_t)it.id);
                        hashAdd_(hash, it.enabled ? 1u : 0u);
                        hashAdd_(hash, it.power_on ? 1u : 0u);
                        hashAdd_(hash, it.heat_on ? 1u : 0u);
                        hashAdd_(hash, it.cool_on ? 1u : 0u);
                        hashAdd_(hash, (uint32_t)it.sensor);
                        hashAdd_(hash, it.sensor_node);
                        hashAdd_(hash, scaled10_(it.target));
                        hashAdd_(hash, scaled10_(it.hyst));
                        hashAdd_(hash, (uint32_t)it.heat);
                        hashAdd_(hash, (uint32_t)it.cool);
                        hashAdd_(hash, (uint32_t)it.button);
                        hashAdd_(hash, it.name);
                        hashAdd_(hash, it.mode);
                    }
                }
            }
            return hash;
        }

        if (path == "/tanks")
        {
            if (_controllers)
            {
                TankController &tanks = _controllers->tanks();
                auto guard = tanks.lockGuard();
                for (size_t i = 0; i < TankController::kTankCount; ++i)
                {
                    const auto *cfg = tanks.configByIndex(i);
                    const auto *st = tanks.stateByIndex(i);
                    if (!cfg || !st)
                        continue;
                    hashAdd_(hash, (uint32_t)cfg->id);
                    hashAdd_(hash, cfg->enabled ? 1u : 0u);
                    hashAdd_(hash, cfg->power_on ? 1u : 0u);
                    hashAdd_(hash, (uint32_t)cfg->level_low);
                    hashAdd_(hash, (uint32_t)cfg->level_mid);
                    hashAdd_(hash, (uint32_t)cfg->level_full);
                    hashAdd_(hash, (uint32_t)cfg->relay_valve);
                    hashAdd_(hash, (uint32_t)cfg->relay_pump);
                    hashAdd_(hash, (uint32_t)cfg->relay_alarm);
                    hashAdd_(hash, cfg->name);
                    hashAdd_(hash, st->level_low ? 1u : 0u);
                    hashAdd_(hash, st->level_mid ? 1u : 0u);
                    hashAdd_(hash, st->level_full ? 1u : 0u);
                    hashAdd_(hash, st->levels_ok ? 1u : 0u);
                    hashAdd_(hash, st->valve_on ? 1u : 0u);
                    hashAdd_(hash, st->pump_on ? 1u : 0u);
                    hashAdd_(hash, st->alarm_on ? 1u : 0u);
                }
            }
            if (_stack_cache && _stack_master)
            {
                const size_t count = _stack_master->nodeCount();
                for (size_t i = 0; i < count; ++i)
                {
                    const uint32_t node_id = _stack_master->nodeIdAt(i);
                    const auto *cache = _stack_cache->tanksCache(node_id);
                    if (!cache)
                        continue;
                    hashAdd_(hash, node_id);
                    hashAdd_(hash, cache->has_data ? 1u : 0u);
                    hashAdd_(hash, cache->pending ? 1u : 0u);
                    hashAdd_(hash, cache->last_ok ? 1u : 0u);
                    hashAdd_(hash, cache->last_error);
                    hashAdd_(hash, (uint32_t)cache->item_count);
                    for (size_t j = 0; j < cache->item_count; ++j)
                    {
                        const auto &it = cache->items[j];
                        hashAdd_(hash, (uint32_t)it.id);
                        hashAdd_(hash, it.enabled ? 1u : 0u);
                        hashAdd_(hash, it.power_on ? 1u : 0u);
                        hashAdd_(hash, (uint32_t)it.low);
                        hashAdd_(hash, (uint32_t)it.mid);
                        hashAdd_(hash, (uint32_t)it.full);
                        hashAdd_(hash, (uint32_t)it.valve);
                        hashAdd_(hash, (uint32_t)it.pump);
                        hashAdd_(hash, (uint32_t)it.alarm);
                        hashAdd_(hash, it.level_low ? 1u : 0u);
                        hashAdd_(hash, it.level_mid ? 1u : 0u);
                        hashAdd_(hash, it.level_full ? 1u : 0u);
                        hashAdd_(hash, it.levels_ok ? 1u : 0u);
                        hashAdd_(hash, it.valve_on ? 1u : 0u);
                        hashAdd_(hash, it.pump_on ? 1u : 0u);
                        hashAdd_(hash, it.alarm_on ? 1u : 0u);
                        hashAdd_(hash, it.name);
                    }
                }
            }
            return hash;
        }

        if (path == "/septic")
        {
            if (_controllers)
            {
                SepticController &septic = _controllers->septic();
                auto guard = septic.lockGuard();
                for (size_t i = 0; i < SepticController::kSepticCount; ++i)
                {
                    const auto *cfg = septic.configByIndex(i);
                    const auto *st = septic.stateByIndex(i);
                    if (!cfg || !st)
                        continue;
                    hashAdd_(hash, (uint32_t)cfg->id);
                    hashAdd_(hash, cfg->enabled ? 1u : 0u);
                    hashAdd_(hash, cfg->monitoring_on ? 1u : 0u);
                    hashAdd_(hash, (uint32_t)cfg->warning_port);
                    hashAdd_(hash, (uint32_t)cfg->alarm_port);
                    hashAdd_(hash, (uint32_t)cfg->relay_warning);
                    hashAdd_(hash, (uint32_t)cfg->relay_alarm);
                    hashAdd_(hash, cfg->name);
                    hashAdd_(hash, st->warning ? 1u : 0u);
                    hashAdd_(hash, st->alarm ? 1u : 0u);
                    hashAdd_(hash, st->relay_warning ? 1u : 0u);
                    hashAdd_(hash, st->relay_alarm ? 1u : 0u);
                }
            }
            if (_stack_cache && _stack_master)
            {
                const size_t count = _stack_master->nodeCount();
                for (size_t i = 0; i < count; ++i)
                {
                    const uint32_t node_id = _stack_master->nodeIdAt(i);
                    const auto *cache = _stack_cache->septicCache(node_id);
                    if (!cache)
                        continue;
                    hashAdd_(hash, node_id);
                    hashAdd_(hash, cache->has_data ? 1u : 0u);
                    hashAdd_(hash, cache->pending ? 1u : 0u);
                    hashAdd_(hash, cache->last_ok ? 1u : 0u);
                    hashAdd_(hash, cache->last_error);
                    hashAdd_(hash, (uint32_t)cache->item_count);
                    for (size_t j = 0; j < cache->item_count; ++j)
                    {
                        const auto &it = cache->items[j];
                        hashAdd_(hash, (uint32_t)it.id);
                        hashAdd_(hash, it.enabled ? 1u : 0u);
                        hashAdd_(hash, it.monitor ? 1u : 0u);
                        hashAdd_(hash, (uint32_t)it.warning_port);
                        hashAdd_(hash, (uint32_t)it.alarm_port);
                        hashAdd_(hash, (uint32_t)it.relay_warning);
                        hashAdd_(hash, (uint32_t)it.relay_alarm);
                        hashAdd_(hash, it.warning ? 1u : 0u);
                        hashAdd_(hash, it.alarm ? 1u : 0u);
                    }
                }
            }
            return hash;
        }

        if (path == "/watering")
        {
            if (_controllers)
            {
                WateringController &watering = _controllers->watering();
                auto guard = watering.lockGuard();
                for (size_t i = 0; i < WateringController::kRuleCount; ++i)
                {
                    const auto *cfg = watering.configByIndex(i);
                    const auto *st = watering.stateByIndex(i);
                    if (!cfg || !st)
                        continue;
                    hashAdd_(hash, (uint32_t)cfg->id);
                    hashAdd_(hash, cfg->enabled ? 1u : 0u);
                    hashAdd_(hash, (uint32_t)cfg->port);
                    hashAdd_(hash, (uint32_t)cfg->tank_id);
                    hashAdd_(hash, (uint32_t)cfg->weekdays_mask);
                    hashAdd_(hash, (uint32_t)cfg->hour);
                    hashAdd_(hash, (uint32_t)cfg->minute);
                    hashAdd_(hash, (uint32_t)cfg->duration_sec);
                    hashAdd_(hash, (uint32_t)cfg->hour2);
                    hashAdd_(hash, (uint32_t)cfg->minute2);
                    hashAdd_(hash, (uint32_t)cfg->duration2_sec);
                    hashAdd_(hash, (uint32_t)cfg->hour3);
                    hashAdd_(hash, (uint32_t)cfg->minute3);
                    hashAdd_(hash, (uint32_t)cfg->duration3_sec);
                    hashAdd_(hash, cfg->resume_after_refill ? 1u : 0u);
                    hashAdd_(hash, (uint32_t)cfg->resume_level);
                    hashAdd_(hash, cfg->name);
                    hashAdd_(hash, st->status ? 1u : 0u);
                    hashAdd_(hash, st->active ? 1u : 0u);
                    hashAdd_(hash, st->paused ? 1u : 0u);
                    hashAdd_(hash, (uint32_t)st->remaining_ms);
                }
            }
            if (_stack_cache && _stack_master)
            {
                const size_t count = _stack_master->nodeCount();
                for (size_t i = 0; i < count; ++i)
                {
                    const uint32_t node_id = _stack_master->nodeIdAt(i);
                    const auto *cache = _stack_cache->wateringCache(node_id);
                    if (!cache)
                        continue;
                    hashAdd_(hash, node_id);
                    hashAdd_(hash, cache->has_data ? 1u : 0u);
                    hashAdd_(hash, cache->pending ? 1u : 0u);
                    hashAdd_(hash, cache->last_ok ? 1u : 0u);
                    hashAdd_(hash, cache->last_error);
                    hashAdd_(hash, (uint32_t)cache->item_count);
                    for (size_t j = 0; j < cache->item_count; ++j)
                    {
                        const auto &it = cache->items[j];
                        hashAdd_(hash, (uint32_t)it.id);
                        hashAdd_(hash, it.enabled ? 1u : 0u);
                        hashAdd_(hash, it.status ? 1u : 0u);
                        hashAdd_(hash, (uint32_t)it.port);
                        hashAdd_(hash, (uint32_t)it.tank_id);
                        hashAdd_(hash, (uint32_t)it.weekdays_mask);
                        hashAdd_(hash, (uint32_t)it.hour);
                        hashAdd_(hash, (uint32_t)it.minute);
                        hashAdd_(hash, (uint32_t)it.duration_sec);
                        hashAdd_(hash, (uint32_t)it.hour2);
                        hashAdd_(hash, (uint32_t)it.minute2);
                        hashAdd_(hash, (uint32_t)it.duration2_sec);
                        hashAdd_(hash, (uint32_t)it.hour3);
                        hashAdd_(hash, (uint32_t)it.minute3);
                        hashAdd_(hash, (uint32_t)it.duration3_sec);
                        hashAdd_(hash, it.resume_after_refill ? 1u : 0u);
                        hashAdd_(hash, (uint32_t)it.resume_level);
                        hashAdd_(hash, it.active ? 1u : 0u);
                        hashAdd_(hash, it.paused ? 1u : 0u);
                        hashAdd_(hash, (uint32_t)it.remaining_ms);
                        hashAdd_(hash, it.name);
                    }
                }
            }
            return hash;
        }

        if (path == "/security")
        {
            if (_controllers)
            {
                SecurityController &sec = _controllers->security();
                auto guard = sec.lockGuard();
                hashAdd_(hash, sec.controllerEnabled() ? 1u : 0u);
                hashAdd_(hash, sec.armed() ? 1u : 0u);
                hashAdd_(hash, sec.alarmOn() ? 1u : 0u);
                hashAdd_(hash, static_cast<uint32_t>(sec.sirenPort()));
                for (size_t i = 0; i < SecurityController::kSensorCount; ++i)
                {
                    const auto *cfg = sec.configByIndex(i);
                    const auto *st = sec.stateByIndex(i);
                    if (!cfg || !st)
                        continue;
                    hashAdd_(hash, (uint32_t)cfg->id);
                    hashAdd_(hash, cfg->enabled ? 1u : 0u);
                    hashAdd_(hash, (uint32_t)cfg->type);
                    hashAdd_(hash, (uint32_t)cfg->port);
                    hashAdd_(hash, cfg->silent ? 1u : 0u);
                    hashAdd_(hash, cfg->name);
                    hashAdd_(hash, st->raw ? 1u : 0u);
                    hashAdd_(hash, st->is_detect ? 1u : 0u);
                }
                for (size_t i = 0; i < SecurityController::kKeyCount; ++i)
                {
                    uint8_t addr[8] = {};
                    bool enabled = false;
                    if (sec.keySlot(i, addr, enabled))
                    {
                        hashAdd_(hash, enabled ? 1u : 0u);
                        if (enabled)
                            hashAdd_(hash, addr, sizeof(addr));
                    }
                }
            }
            if (_stack_cache && _stack_master)
            {
                const size_t count = _stack_master->nodeCount();
                for (size_t i = 0; i < count; ++i)
                {
                    const uint32_t node_id = _stack_master->nodeIdAt(i);
                    const auto *cache = _stack_cache->securityCache(node_id);
                    if (!cache)
                        continue;
                    hashAdd_(hash, node_id);
                    hashAdd_(hash, cache->has_data ? 1u : 0u);
                    hashAdd_(hash, cache->pending ? 1u : 0u);
                    hashAdd_(hash, cache->last_ok ? 1u : 0u);
                    hashAdd_(hash, cache->last_error);
                    hashAdd_(hash, cache->enabled ? 1u : 0u);
                    hashAdd_(hash, cache->armed ? 1u : 0u);
                    hashAdd_(hash, cache->alarm ? 1u : 0u);
                    hashAdd_(hash, (uint32_t)cache->item_count);
                    for (size_t j = 0; j < cache->item_count; ++j)
                    {
                        const auto &it = cache->items[j];
                        hashAdd_(hash, (uint32_t)it.id);
                        hashAdd_(hash, it.enabled ? 1u : 0u);
                        hashAdd_(hash, it.detect ? 1u : 0u);
                        hashAdd_(hash, it.silent ? 1u : 0u);
                        hashAdd_(hash, (uint32_t)it.port);
                        hashAdd_(hash, it.type);
                        hashAdd_(hash, it.name);
                    }
                }
            }
            return hash;
        }

        if (path == "/ring")
        {
            if (_controllers)
            {
                auto guard = _controllers->ring().lockGuard();
                const auto &cfg = _controllers->ring().config();
                hashAdd_(hash, cfg.enabled ? 1u : 0u);
                hashAdd_(hash, (uint32_t)cfg.button_port);
                hashAdd_(hash, (uint32_t)cfg.relay_port);
            }
            hashAdd_(hash, _ring_status);
            return hash;
        }

        if (path == "/avr")
        {
            if (_controllers)
            {
                auto guard = _controllers->avr().lockGuard();
                const auto &cfg = _controllers->avr().config();
                const auto &st = _controllers->avr().state();
                hashAdd_(hash, cfg.enabled ? 1u : 0u);
                hashAdd_(hash, cfg.auto_mode ? 1u : 0u);
                hashAdd_(hash, cfg.prefer_main ? 1u : 0u);
                hashAdd_(hash, cfg.auto_return_main ? 1u : 0u);
                hashAdd_(hash, (uint32_t)cfg.main_ok_port);
                hashAdd_(hash, (uint32_t)cfg.reserve_ok_port);
                hashAdd_(hash, (uint32_t)cfg.relay_main_port);
                hashAdd_(hash, (uint32_t)cfg.relay_reserve_port);
                hashAdd_(hash, (uint32_t)cfg.feedback_main_port);
                hashAdd_(hash, (uint32_t)cfg.feedback_reserve_port);
                hashAdd_(hash, cfg.main_ok_active_low ? 1u : 0u);
                hashAdd_(hash, cfg.reserve_ok_active_low ? 1u : 0u);
                hashAdd_(hash, cfg.feedback_main_active_low ? 1u : 0u);
                hashAdd_(hash, cfg.feedback_reserve_active_low ? 1u : 0u);
                hashAdd_(hash, cfg.relay_main_invert ? 1u : 0u);
                hashAdd_(hash, cfg.relay_reserve_invert ? 1u : 0u);
                hashAdd_(hash, cfg.debounce_ms);
                hashAdd_(hash, cfg.loss_delay_ms);
                hashAdd_(hash, cfg.return_delay_ms);
                hashAdd_(hash, cfg.break_ms);
                hashAdd_(hash, cfg.warmup_ms);
                hashAdd_(hash, cfg.transfer_timeout_ms);
                hashAdd_(hash, (uint32_t)st.active_source);
                hashAdd_(hash, (uint32_t)st.target_source);
                hashAdd_(hash, (uint32_t)st.manual_source);
                hashAdd_(hash, (uint32_t)st.fault);
                hashAdd_(hash, st.transfer_in_progress ? 1u : 0u);
                hashAdd_(hash, st.main_ok ? 1u : 0u);
                hashAdd_(hash, st.reserve_ok ? 1u : 0u);
            }
            if (_stack_cache && _stack_master)
            {
                const size_t count = _stack_master->nodeCount();
                for (size_t i = 0; i < count; ++i)
                {
                    const uint32_t node_id = _stack_master->nodeIdAt(i);
                    const auto *cache = _stack_cache->avrCache(node_id);
                    if (!cache)
                        continue;
                    hashAdd_(hash, node_id);
                    hashAdd_(hash, cache->has_data ? 1u : 0u);
                    hashAdd_(hash, cache->pending ? 1u : 0u);
                    hashAdd_(hash, cache->last_ok ? 1u : 0u);
                    hashAdd_(hash, cache->last_error);
                    hashAdd_(hash, cache->enabled ? 1u : 0u);
                    hashAdd_(hash, cache->auto_mode ? 1u : 0u);
                    hashAdd_(hash, cache->prefer_main ? 1u : 0u);
                    hashAdd_(hash, cache->auto_return_main ? 1u : 0u);
                    hashAdd_(hash, cache->main_ok ? 1u : 0u);
                    hashAdd_(hash, cache->reserve_ok ? 1u : 0u);
                    hashAdd_(hash, cache->relay_main_on ? 1u : 0u);
                    hashAdd_(hash, cache->relay_reserve_on ? 1u : 0u);
                    hashAdd_(hash, cache->transfer ? 1u : 0u);
                    hashAdd_(hash, (uint32_t)cache->main_ok_port);
                    hashAdd_(hash, (uint32_t)cache->reserve_ok_port);
                    hashAdd_(hash, (uint32_t)cache->relay_main_port);
                    hashAdd_(hash, (uint32_t)cache->relay_reserve_port);
                    hashAdd_(hash, (uint32_t)cache->feedback_main_port);
                    hashAdd_(hash, (uint32_t)cache->feedback_reserve_port);
                    hashAdd_(hash, cache->active_source);
                    hashAdd_(hash, cache->target_source);
                    hashAdd_(hash, cache->fault);
                }
            }
            hashAdd_(hash, _avr_status);
            return hash;
        }

        if (path == "/leak")
        {
            if (_controllers)
            {
                LeakController &leak = _controllers->leak();
                auto guard = leak.lockGuard();
                hashAdd_(hash, leak.controllerEnabled() ? 1u : 0u);
                for (size_t i = 0; i < LeakController::kZoneCount; ++i)
                {
                    const auto *cfg = leak.configByIndex(i);
                    const auto *st = leak.stateByIndex(i);
                    if (!cfg || !st)
                        continue;
                    hashAdd_(hash, (uint32_t)cfg->id);
                    hashAdd_(hash, cfg->enabled ? 1u : 0u);
                    hashAdd_(hash, cfg->power_on ? 1u : 0u);
                    hashAdd_(hash, cfg->sensor_active_low ? 1u : 0u);
                    hashAdd_(hash, cfg->valve_open_on_power ? 1u : 0u);
                    hashAdd_(hash, (uint32_t)cfg->sensor_port);
                    hashAdd_(hash, (uint32_t)cfg->valve_port);
                    hashAdd_(hash, (uint32_t)cfg->alarm_port);
                    hashAdd_(hash, cfg->name);
                    hashAdd_(hash, st->wet ? 1u : 0u);
                    hashAdd_(hash, st->alarm_latched ? 1u : 0u);
                }
            }
            if (_stack_cache && _stack_master)
            {
                const size_t count = _stack_master->nodeCount();
                for (size_t i = 0; i < count; ++i)
                {
                    const uint32_t node_id = _stack_master->nodeIdAt(i);
                    const auto *cache = _stack_cache->leakCache(node_id);
                    if (!cache)
                        continue;
                    hashAdd_(hash, node_id);
                    hashAdd_(hash, cache->has_data ? 1u : 0u);
                    hashAdd_(hash, cache->pending ? 1u : 0u);
                    hashAdd_(hash, cache->last_ok ? 1u : 0u);
                    hashAdd_(hash, cache->last_error);
                    hashAdd_(hash, (uint32_t)cache->item_count);
                    for (size_t j = 0; j < cache->item_count; ++j)
                    {
                        const auto &it = cache->items[j];
                        hashAdd_(hash, (uint32_t)it.id);
                        hashAdd_(hash, it.enabled ? 1u : 0u);
                        hashAdd_(hash, it.power_on ? 1u : 0u);
                        hashAdd_(hash, it.sensor_active_low ? 1u : 0u);
                        hashAdd_(hash, (uint32_t)it.sensor);
                        hashAdd_(hash, (uint32_t)it.valve);
                        hashAdd_(hash, (uint32_t)it.alarm);
                        hashAdd_(hash, it.wet ? 1u : 0u);
                        hashAdd_(hash, it.alarm_latched ? 1u : 0u);
                        hashAdd_(hash, it.name);
                    }
                }
            }
            hashAdd_(hash, _leak_status);
            return hash;
        }

        if (path == "/admin")
        {
            hashAdd_(hash, rtcDateStr_());
            hashAdd_(hash, rtcTimeOnlyStr_());
            hashAdd_(hash, (_plc && _plc->buzzerEnabled()) ? 1u : 0u);
            return hash;
        }

        if (path == "/logs")
        {
            if (_log)
            {
                const size_t count = _log->recentCount();
                hashAdd_(hash, static_cast<uint32_t>(count));
                if (count > 0)
                {
                    char buf[LOGGER_BUFFER_SIZE] = {};
                    if (_log->getRecentLine(count - 1, buf, sizeof(buf)))
                        hashAdd_(hash, buf);
                }
            }
            return hash;
        }

        if (path == "/telegram")
        {
            if (_tgbot)
            {
                hashAdd_(hash, _tgbot->token());
                hashAdd_(hash, String((long long)_tgbot->chatId()));
                hashAdd_(hash, _tgbot->clientKindName());
                hashAdd_(hash, _tgbot->pollMode() == TelegramClient::PollMode::Long ? "long" : "short");
                hashAdd_(hash, _tgbot->useProxy() ? "1" : "0");
                hashAdd_(hash, _tgbot->proxyHost());
                hashAdd_(hash, String((unsigned)_tgbot->proxyPort()));
                hashAdd_(hash, _tgbot->proxyPath());
            }
            return hash;
        }

        if (path == "/cloud")
        {
            hashAdd_(hash, cloudEnabled_() ? 1u : 0u);
            hashAdd_(hash, cloudHost_());
            hashAdd_(hash, (uint32_t)cloudPort_());
            hashAdd_(hash, cloudPath_());
            hashAdd_(hash, cloudUseSsl_() ? 1u : 0u);
            hashAdd_(hash, cloudReconnectMs_());
            hashAdd_(hash, cloudEventMs_());
            hashAdd_(hash, cloudApiKey_());
            hashAdd_(hash, cloudFwVersion_());
            hashAdd_(hash, cloudDeviceId_());
            hashAdd_(hash, cloudConnected_() ? 1u : 0u);
            return hash;
        }

        return hash;
    }

void WebInterface::appendHex_(String &out, uint32_t value)
{
        char buf[9] = {};
        snprintf(buf, sizeof(buf), "%08lX", (unsigned long)value);
        out += buf;
    }

String WebInterface::stackNodeIdHex_(uint32_t value)
{
        String out = "0x";
        appendHex_(out, value);
        return out;
    }

String WebInterface::genApiKey_()
{
        char buf[33] = {};
        static const char kHex[] = "0123456789abcdef";
        for (size_t i = 0; i < 16; ++i)
        {
            const uint8_t v = (uint8_t)random(0, 256);
            buf[i * 2] = kHex[(v >> 4) & 0x0F];
            buf[i * 2 + 1] = kHex[v & 0x0F];
        }
        buf[32] = '\0';
        return String(buf);
    }

uint32_t WebInterface::rand32_()
{
#if defined(ESP32)
        return esp_random();
#else
        uint32_t r = (uint32_t)random(0x7FFFFFFF);
        r = (r << 1) ^ (uint32_t)micros();
        return r;
#endif
    }



    String WebInterface::makeSessionToken_() const
{
        String out;
        out.reserve(32);
        for (uint8_t i = 0; i < 4; ++i)
            appendHex_(out, rand32_());
        return out;
    }



    void WebInterface::issueSession_(int16_t user_idx )
{
        _session_token = makeSessionToken_();
        const uint32_t now = millis();
        _session_expire_ms = now + _session_ttl_ms;
        _session_user_idx = user_idx;
    }



    void WebInterface::clearSession_()
{
        _session_token = "";
        _session_expire_ms = 0;
        _session_user_idx = -1;
    }



    void WebInterface::refreshSession_()
{
        const uint32_t now = millis();
        _session_expire_ms = now + _session_ttl_ms;
    }



    bool WebInterface::sessionValid_(const String &token) const
{
        if (_session_token.length() == 0)
            return false;
        const uint32_t now = millis();
        if ((int32_t)(now - _session_expire_ms) >= 0)
            return false;
        return token == _session_token;
    }



    bool WebInterface::extractSessionToken_(AsyncWebServerRequest *request, String &out) const
{
        if (!request)
            return false;
        if (!request->hasHeader("Cookie"))
            return false;
        const AsyncWebHeader *hdr = request->getHeader("Cookie");
        if (!hdr)
            return false;
        String cookies = hdr->value();
        const String key = "plc_session=";
        int pos = cookies.indexOf(key);
        if (pos < 0)
            return false;
        int start = pos + key.length();
        int end = cookies.indexOf(';', start);
        if (end < 0)
            end = cookies.length();
        out = cookies.substring(start, end);
        out.trim();
        return out.length() > 0;
    }



    bool WebInterface::sessionPrincipalValid_() const
{
        if (_session_user_idx < 0)
        {
            if (_cli_auth && _cli_auth->adminPasswordSet())
                return true;
            if (_auth_enabled)
                return true;
            return false;
        }
        if (!_users)
            return false;
        const size_t idx = (size_t)_session_user_idx;
        if (idx >= _users->size())
            return false;
        const auto &u = _users->user(idx);
        return u.enabled;
    }



    String WebInterface::gsmStatusLabel_() const
{
        if (!_gsm)
            return WebUiRu::WebCore::kUnavailable;
        if (!_gsm->started())
            return "off";
        const String &err = _gsm->lastError();
        if (err.length())
            return "error";
        const String &reg = _gsm->regStatus();
        if (!reg.length())
            return "no reg";
        return "ok";
    }

