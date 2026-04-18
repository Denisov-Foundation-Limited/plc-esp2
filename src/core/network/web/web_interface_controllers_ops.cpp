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

#include "core/network/web/web_interface_controllers_ops.hpp"

#include "core/network/network.hpp"
#include "core/network/web/web_interface.hpp"

#define _allowed_exts _web._allowed_exts
#define _auth_enabled _web._auth_enabled
#define _auth_pass _web._auth_pass
#define _auth_user _web._auth_user
#define _cli_auth _web._cli_auth
#define _cloud _web._cloud
#define _configs_manager _web._configs_manager
#define _controllers _web._controllers
#define _device_status _web._device_status
#define _ext _web._ext
#define _gsm _web._gsm
#define _gsm_status _web._gsm_status
#define _i2c _web._i2c
#define _last_status _web._last_status
#define _max_upload _web._max_upload
#define _ota_error _web._ota_error
#define _ota_in_progress _web._ota_in_progress
#define _ota_name _web._ota_name
#define _ota_ok _web._ota_ok
#define _ota_set_cookie _web._ota_set_cookie
#define _ota_size _web._ota_size
#define _ow _web._ow
#define _plc _web._plc
#define _session_user_idx _web._session_user_idx
#define _stack_status _web._stack_status
#define _upload _web._upload
#define _upload_error _web._upload_error
#define _upload_in_progress _web._upload_in_progress
#define _upload_name _web._upload_name
#define _upload_ok _web._upload_ok
#define _upload_set_cookie _web._upload_set_cookie
#define _upload_size _web._upload_size
#define _users _web._users
#define _wifi _web._wifi
#define _wifi_status _web._wifi_status
#define appendHtmlEscaped_ _web.appendHtmlEscaped_
#define appendJsonEscaped_ _web.appendJsonEscaped_
#define clearSession_ _web.clearSession_
#define extractSessionToken_ _web.extractSessionToken_
#define genApiKey_ _web.genApiKey_
#define issueSession_ _web.issueSession_
#define owAddrToHex_ _web.owAddrToHex_
#define owBusName_ _web.owBusName_
#define parseBasicAuth_ _web.parseBasicAuth_
#define refreshSession_ _web.refreshSession_
#define requestBasicAuth_ _web.requestBasicAuth_
#define safeHtmlValue_ _web.safeHtmlValue_
#define sanitizePath_ _web.sanitizePath_
#define sanitizeUploadName_ _web.sanitizeUploadName_
#define sanitizeUtf8_ _web.sanitizeUtf8_
#define sendRedirect_ _web.sendRedirect_
#define sendText_ _web.sendText_
#define sessionCookie_ _web.sessionCookie_
#define sessionPrincipalValid_ _web.sessionPrincipalValid_
#define sessionValid_ _web.sessionValid_
#define stackNodeIdHex_ _web.stackNodeIdHex_
#define extTypeName_ _web.extTypeName_
#define extDevTypeName_ _web.extDevTypeName_
#define portTypeName_ _web.portTypeName_
#define locationName_ _web.locationName_
#define fanStatusIcon_ _web.fanStatusIcon_
#define formatTemp_ _web.formatTemp_
#define uiPageHash_ _web.uiPageHash_

String WebInterfaceControllersOps::listPortsHtml_()
{
    String items;
    items.reserve(2048);
    for (uint16_t i = 0; i < PortIO::PORT_COUNT; ++i)
    {
        const auto &p = ActiveBoardProfile::PORTS[i];
        if (p.caps == Cap::None)
            continue;
        if (p.backend == PortIO::Backend::Extender)
        {
            if (!_ext)
                continue;
            const uint8_t dev = p.u.ext.dev;
            const auto *devs = _ext->devs();
            if (!devs || dev >= _ext->devCount())
                continue;
            if (devs[dev].type != Extender::Type::MCP23017)
                continue;
            if (!_ext->isPresent(dev))
                continue;
        }

        items += "<tr><td class=\"right\"><strong>";
        items += String(i);
        items += "</strong></td><td><strong>";
        items += (p.backend == PortIO::Backend::Extender) ? "Extender" : "Esp32";
        items += "</strong></td><td><strong>";
        items += locationName_(p.location);
        items += "</strong></td><td><strong>";
        items += portTypeName_(p.type);
        items += "</strong></td><td><strong>";
        items += p.allow_control ? "yes" : "no";
        items += "</strong></td><td class=\"right\"><strong>";

        const char *state = "n/a";
        bool state_value = false;
        if (_plc && _plc->portState((uint8_t)i, state_value))
            state = state_value ? "1" : "0";

        if (p.backend == PortIO::Backend::Extender)
        {
            items += String(p.u.ext.dev);
            items += "</strong></td><td class=\"right\"><strong>";
            items += String(p.u.ext.pin);
            items += "</strong></td><td><strong>";
            items += state;
            items += "</strong></td><td><strong>";
            items += extDevTypeName_(p.u.ext.dev);
        }
        else
        {
            items += "--</strong></td><td class=\"right\"><strong>";
            items += String(p.u.esp.gpio);
            items += "</strong></td><td><strong>";
            items += state;
            items += "</strong></td><td><strong>CPU";
        }
        items += "</strong></td></tr>";
    }
    if (items.length() == 0)
        items = "<tr><td colspan=\"9\" style=\"color:#94a3b8\"><strong>No ports</strong></td></tr>";
    return items;
}

String WebInterfaceControllersOps::listExtendersHtml_()
{
    if (!_ext)
        return "<tr><td colspan=\"4\" style=\"color:#94a3b8\"><strong>Extenders unavailable</strong></td></tr>";
    const auto *devs = _ext->devs();
    if (!devs)
        return "<tr><td colspan=\"4\" style=\"color:#94a3b8\"><strong>Extenders unavailable</strong></td></tr>";
    String items;
    items.reserve(512);
    for (uint8_t i = 0; i < _ext->devCount(); ++i)
    {
        const auto &d = devs[i];
        if (d.i2c_addr == 0 || d.type == Extender::Type::None)
            continue;
        if (!_ext->isPresent(i))
            continue;
        char addr_buf[8] = {};
        snprintf(addr_buf, sizeof(addr_buf), "0x%02X", (unsigned)d.i2c_addr);
        items += "<tr><td class=\"right\"><strong>";
        items += String((unsigned)i);
        items += "</strong></td><td class=\"right\"><strong>";
        items += String((unsigned)d.bus_num);
        items += "</strong></td><td><strong>";
        items += addr_buf;
        items += "</strong></td><td><strong>";
        items += extTypeName_(d.type);
        items += "</strong></td></tr>";
    }
    if (items.length() == 0)
        items = String("<tr><td colspan=\"4\" style=\"color:#94a3b8\"><strong>") +
                WebUiRu::WebCore::kExtendersAbsent + "</strong></td></tr>";
    return items;
}

String WebInterfaceControllersOps::listStackPortsHtml_(uint32_t node_id) const
{
    (void)node_id;
    return "<tr><td colspan=\"9\" style=\"color:#94a3b8\"><strong>not migrated</strong></td></tr>";
}

String WebInterfaceControllersOps::listStackExtendersHtml_(uint32_t node_id) const
{
    (void)node_id;
    return "<tr><td colspan=\"4\" style=\"color:#94a3b8\"><strong>not migrated</strong></td></tr>";
}

String WebInterfaceControllersOps::listI2cHtml_()
{
    if (!_i2c)
        return "<tr><td colspan=\"2\" style=\"color:#94a3b8\"><strong>none</strong></td></tr>";
    String items;
    items.reserve(1024);
    bool scanned[3] = {false, false, false};
    for (uint8_t i = 0; i < ActiveBoardProfile::I2C_COUNT; ++i)
    {
        const uint8_t bus = ActiveBoardProfile::I2CS[i].bus_num;
        if (bus < 3 && scanned[bus])
            continue;
        if (bus < 3)
            scanned[bus] = true;
        bool present[127] = {};
        if (!_i2c->scanDevices(bus, present))
            continue;
        for (uint8_t addr = 1; addr < 127; ++addr)
            if (present[addr])
            {
                char addr_buf[8] = {};
                snprintf(addr_buf, sizeof(addr_buf), "0x%02X", addr);
                items += "<tr><td class=\"right\"><strong>";
                items += String((unsigned)bus);
                items += "</strong></td><td><strong>";
                items += addr_buf;
                items += "</strong></td></tr>";
            }
    }
    if (items.length() == 0)
        items = "<tr><td colspan=\"2\" style=\"color:#94a3b8\"><strong>none</strong></td></tr>";
    return items;
}

String WebInterfaceControllersOps::listStackI2cHtml_(uint32_t node_id) const
{
    (void)node_id;
    return "<tr><td colspan=\"2\" style=\"color:#94a3b8\"><strong>not migrated</strong></td></tr>";
}

String WebInterfaceControllersOps::stackNodesBlockHtml_() const
{
    String out;
    out.reserve(1024);
    out += "<div class=\"section\">";
    out += WebUiRu::Controllers::kText;
    out += "<table><thead><tr>";
    out += WebUiRu::Controllers::kUnitDevicenameNodeidIp;
    out += "</tr></thead><tbody id=\"stack-nodes-tbody\">";
    out += listStackNodesHtml_();
    out += "</tbody></table>";
    out += "</div>";
    return out;
}

String WebInterfaceControllersOps::listStackNodesStatusHtml_() const
{
    String html;
    html.reserve(4096);
    html += "<tr><th>";
    html += WebUiRu::Controllers::kIpRtcCpu;
    html += "</th></tr>";
    if (!_web._network)
        return WebUiRu::Controllers::kText3;
    const size_t count = _web._network->stackOnlineDeviceCount();
    if (count == 0)
        return WebUiRu::Controllers::kText4;
    for (size_t i = 0; i < count; ++i)
    {
        StackDeviceRegistry::DeviceInfo device{};
        if (!_web._network->stackDeviceSnapshotAt(i, device) || !device.online || device.node_id == 0)
            continue;
        StackUnitSnapshot::State snapshot{};
        const bool has_snapshot = _web._network->stackIndexState(device.node_id, snapshot) && snapshot.updated_ms != 0;
        html += "<tr><td><strong>";
        if (device.name[0])
            appendHtmlEscaped_(html, device.name);
        else
            html += stackNodeIdHex_(device.node_id);
        html += "</strong></td><td><strong>";
        appendHtmlEscaped_(html, device.ip);
        html += "</strong></td><td><strong>";
        html += has_snapshot ? safeHtmlValue_(snapshot.rtc_date, "n/a") : "n/a";
        html += "</strong></td><td><strong>";
        html += has_snapshot ? safeHtmlValue_(snapshot.rtc_time, "n/a") : "n/a";
        html += "</strong></td><td><strong>";
        html += has_snapshot ? formatTemp_(snapshot.rtc_temp) : "n/a";
        html += "</strong></td><td><strong>";
        html += has_snapshot ? formatTemp_(snapshot.board_temp) : "n/a";
        html += "</strong></td><td><strong>";
        html += "n/a";
        html += "</strong></td><td class=\"center\"><strong>";
        if (has_snapshot)
            html += fanStatusIcon_(snapshot.fan_on);
        else
            html += "n/a";
        html += "</strong></td></tr>";
    }
    return html;
}

String WebInterfaceControllersOps::listStackNodesHtml_() const
{
    if (!_web._network)
        return WebUiRu::Controllers::kText3;
    const size_t count = _web._network->stackOnlineDeviceCount();
    if (count == 0)
        return WebUiRu::Controllers::kText4;
    String items;
    items.reserve(1024);
    size_t unit_index = 0;
    for (size_t i = 0; i < count; ++i)
    {
        StackDeviceRegistry::DeviceInfo device;
        if (!_web._network->stackDeviceSnapshotAt(i, device) || !device.online || device.node_id == 0)
            continue;
        ++unit_index;
        items += "<tr data-node=\"";
        items += String((unsigned long)device.node_id);
        items += "\"><td><strong>";
        items += String((unsigned)unit_index);
        items += "</strong></td><td><strong>";
        if (device.name[0])
            appendHtmlEscaped_(items, device.name);
        else
            items += "-";
        items += "</strong></td><td><strong>";
        items += stackNodeIdHex_(device.node_id);
        items += "</strong></td><td><strong>";
        appendHtmlEscaped_(items, device.ip);
        items += "</strong></td><td><strong>";
        items += (device.caps & kStackCapController) ? WebUiRu::Controllers::kText5 : WebUiRu::Controllers::kText6;
        items += "</strong></td></tr>";
    }
    return items;
}

String WebInterfaceControllersOps::globalUsedPortsJson_(PortIO::PinType type) const
{
    String out;
    out.reserve(128);
    out += "[";
    if (!_controllers)
        return "[]";

    bool first = true;
    for (uint16_t i = 0; i < PortIO::PORT_COUNT; ++i)
    {
        if (!_controllers->gpioPortUsedByType((uint8_t)i, type))
            continue;
        if (!first)
            out += ",";
        out += String((unsigned)i);
        first = false;
    }
    out += "]";
    return out;
}


String WebInterfaceControllersOps::stackPortOptionsJson_(uint32_t node_id, PortIO::PinType type) const
{
        (void)node_id;
        String out;
        out.reserve(128);
        out += "[";
        bool first = true;
        for (uint16_t i = 0; i < PortIO::PORT_COUNT; ++i)
        {
            const auto &p = ActiveBoardProfile::PORTS[i];
            if (p.caps == Cap::None || p.type != type)
                continue;
            if (!first)
                out += ",";
            out += "{\"v\":";
            out += String((unsigned)i);
            out += ",\"l\":\"";
            out += String((unsigned)i);
            out += "\"}";
            first = false;
        }
        out += "]";
        return out;
    }



String WebInterfaceControllersOps::stackUsedPortsJson_(uint32_t node_id, PortIO::PinType type) const
{
        StackUnitSnapshot::State state{};
        if (!_web.network() || node_id == 0 || !_web.network()->stackIndexState(node_id, state) || !state.ports_state_valid)
            return "[]";
        const uint8_t *bits = nullptr;
        switch (type)
        {
        case PortIO::PinType::Relay:
            bits = state.relay_used_bits;
            break;
        case PortIO::PinType::Sensor:
            bits = state.sensor_used_bits;
            break;
        case PortIO::PinType::DInput:
        default:
            bits = state.dinput_used_bits;
            break;
        }
        String out;
        out.reserve(128);
        out += "[";
        bool first = true;
        for (uint16_t port = 0; port < PortIO::PORT_COUNT; ++port)
        {
            if ((bits[port >> 3] & (uint8_t)(1u << (port & 0x07u))) == 0)
                continue;
            const auto &p = ActiveBoardProfile::PORTS[port];
            if (p.caps == Cap::None || p.type != type)
                continue;
            if (!first)
                out += ",";
            out += String((unsigned)port);
            first = false;
        }
        out += "]";
        return out;
    }



    String WebInterfaceControllersOps::stackMeteoDs18OptionsJson_(uint32_t node_id) const
{
        (void)node_id;
        return "[]";
    }



    String WebInterfaceControllersOps::stackMeteoDs18UsedJson_(uint32_t node_id) const
{
        (void)node_id;
        return "[]";
    }



    String WebInterfaceControllersOps::listOwHtml_()
{
        if (!_ow)
            return "<tr><td colspan=\"3\" style=\"color:#94a3b8\"><strong>none</strong></td></tr>";
        String items;
        items.reserve(1024);
        for (uint8_t i = 0; i < ActiveBoardProfile::ONEWIRE_COUNT; ++i)
        {
            OneWireBus *bus = _ow->busPtrByIndex(i);
            if (!bus)
                continue;
            OneWireManager::ScopedBusLock lk(*_ow, i, 200);
            if (!lk.locked())
                continue;
            const auto &cfg = ActiveBoardProfile::ONEWIRES[i];
            uint8_t addr[8] = {};
            bus->reset_search();
            while (bus->search(addr))
            {
                if (OneWireBus::crc8(addr, 7) != addr[7])
                    continue;
                char hex[17] = {};
                owAddrToHex_(addr, hex);
                items += "<tr><td class=\"right\"><strong>";
                items += String((unsigned)i);
                items += "</strong></td><td><strong>";
                items += owBusName_(cfg.bus_id);
                items += "</strong></td><td><strong>";
                items += hex;
                items += "</strong></td></tr>";
            }
        }
        if (items.length() == 0)
            items = "<tr><td colspan=\"3\" style=\"color:#94a3b8\"><strong>none</strong></td></tr>";
        return items;
    }



    String WebInterfaceControllersOps::listStackOwHtml_(uint32_t node_id) const
{
        (void)node_id;
        return "<tr><td colspan=\"3\" style=\"color:#94a3b8\"><strong>not migrated</strong></td></tr>";
    }



    void WebInterfaceControllersOps::handleUpload_(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data,
                       size_t len, bool final)
{
        if (index == 0)
        {
            bool set_cookie = false;
            if (!checkAuth_(request, &set_cookie, true))
                return;
            if (!requireWebAdmin_(request, &set_cookie))
                return;
            if (_upload_in_progress && _upload)
                _upload.close();
            _upload_in_progress = true;
            request->onDisconnect([this]() {
                if (!_upload_in_progress)
                    return;
                if (_upload)
                    _upload.close();
                _upload_ok = false;
                _upload_error = "Upload disconnected";
                _upload_in_progress = false;
            });
            _upload_set_cookie = set_cookie;
            _upload_ok = true;
            _upload_error = "";
            _upload_name = filename;
            String path = sanitizeUploadName_(filename);
            if (!path.length())
            {
                _upload_ok = false;
                _upload_error = "Invalid file name";
                _upload_in_progress = false;
                return;
            }
            if (!isAllowedExt_(path))
            {
                _upload_ok = false;
                _upload_error = "File extension not allowed";
                _upload_in_progress = false;
                return;
            }
            _upload_size = 0;
            _upload = LittleFS.open(path, "w");
            if (!_upload)
            {
                _upload_ok = false;
                _upload_error = "Open failed";
                _upload_in_progress = false;
                return;
            }
        }
        if (!_upload_ok)
            return;
        _upload_size += len;
        if (_max_upload > 0 && _upload_size > _max_upload)
        {
            _upload_ok = false;
            _upload_error = "File too large";
            if (_upload)
                _upload.close();
            _upload_in_progress = false;
            return;
        }
        if (_upload)
            _upload.write(data, len);
        if (final)
        {
            if (_upload)
                _upload.close();
            _upload_in_progress = false;
        }
    }



    void WebInterfaceControllersOps::handleOta_(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len,
                    bool final)
{
        if (index == 0)
        {
            bool set_cookie = false;
            if (!checkAuth_(request, &set_cookie, true))
                return;
            if (!requireWebAdmin_(request, &set_cookie))
                return;
            _ota_set_cookie = set_cookie;
            if (_ota_in_progress)
                Update.abort();
            _ota_in_progress = true;
            request->onDisconnect([this]() {
                if (!_ota_in_progress)
                    return;
                Update.abort();
                _ota_ok = false;
                _ota_error = "OTA disconnected";
                _ota_in_progress = false;
            });
            _ota_ok = true;
            _ota_error = "";
            _ota_size = 0;
            _ota_name = filename;
            if (!Update.begin(UPDATE_SIZE_UNKNOWN))
            {
                _ota_ok = false;
                _ota_error = Update.errorString();
                _ota_in_progress = false;
            }
        }
        if (!_ota_ok)
            return;
        _ota_size += len;
        if (_max_upload > 0 && _ota_size > _max_upload)
        {
            _ota_ok = false;
            _ota_error = "Firmware image too large";
            Update.abort();
            _ota_in_progress = false;
            return;
        }
        if (Update.write(data, len) != len)
        {
            _ota_ok = false;
            _ota_error = Update.errorString();
            Update.abort();
            _ota_in_progress = false;
            return;
        }
        if (final)
        {
            if (!Update.end(true))
            {
                _ota_ok = false;
                _ota_error = Update.errorString();
            }
            _ota_in_progress = false;
        }
    }



    void WebInterfaceControllersOps::handleUploadDone_(AsyncWebServerRequest *request)
{
        if (!_upload_ok)
            _last_status = _upload_error.length() ? _upload_error : "Upload failed";
        else
            _last_status = "Upload complete";
sendRedirect_(request, "/status", _upload_set_cookie);
        _upload_set_cookie = false;
    }



    void WebInterfaceControllersOps::handleOtaDone_(AsyncWebServerRequest *request)
{
        if (!_ota_ok)
            _last_status = _ota_error.length() ? _ota_error : "Firmware update failed";
        else
            _last_status = "Firmware updated. Rebooting...";
sendRedirect_(request, "/status", _ota_set_cookie);
        _ota_set_cookie = false;
        if (_ota_ok)
        {
            delay(500);
            ESP.restart();
        }
    }



    void WebInterfaceControllersOps::handleWifiSave_(AsyncWebServerRequest *request)
{
        bool set_cookie = false;
        if (!checkAuth_(request, &set_cookie))
            return;
        if (!requireWebAdmin_(request, &set_cookie))
            return;
        bool changed = false;

        if (request->hasParam("mode", true))
        {
            String mode = request->getParam("mode", true)->value();
            WifiManager::Mode parsed = _wifi.mode();
            if (WifiManager::parseMode(mode, parsed) && parsed != _wifi.mode())
            {
                _wifi.setMode(parsed);
                changed = true;
            }
        }
        if (request->hasParam("ssid", true))
        {
            String ssid = request->getParam("ssid", true)->value();
            ssid.trim();
            if (ssid.length() > 0 && ssid != _wifi.ssid())
            {
                _wifi.setSsid(ssid);
                changed = true;
            }
        }
        if (request->hasParam("password", true))
        {
            String pass = request->getParam("password", true)->value();
            if (pass.length() > 0 && pass != _wifi.password())
            {
                _wifi.setPassword(pass);
                changed = true;
            }
        }
        if (request->hasParam("ap_ssid", true))
        {
            String ssid = request->getParam("ap_ssid", true)->value();
            ssid.trim();
            if (ssid.length() > 0 && ssid != _wifi.apSsid())
            {
                _wifi.setApSsid(ssid);
                changed = true;
            }
        }
        if (request->hasParam("ap_password", true))
        {
            String pass = request->getParam("ap_password", true)->value();
            if (pass.length() > 0 && pass != _wifi.apPassword())
            {
                _wifi.setApPassword(pass);
                changed = true;
            }
        }

        bool gsm_changed = false;
        bool gsm_ok = true;
        if (_gsm && ActiveBoardProfile::GSM.enabled)
        {
            const bool gsm_enabled = request->hasParam("gsm_enabled", true);
            gsm_changed = (gsm_enabled != _gsm->enabled());
            _gsm->setEnabled(gsm_enabled);
        }
        else if (_gsm)
        {
            _gsm->setEnabled(false);
        }

        bool wifi_ok = true;
        bool save_ok = true;
        if (changed)
            wifi_ok = _wifi.begin();
        if (changed || gsm_changed)
            save_ok = saveWifiConfig_();

        if (_gsm && _gsm->enabled() && !_gsm->started())
            gsm_ok = _gsm->begin(ActiveBoardProfile::GSM.uart_index);

        if (!changed)
            _wifi_status = "No changes";
        else if (!wifi_ok && !save_ok)
            _wifi_status = "Wi-Fi apply and save failed";
        else if (!wifi_ok)
            _wifi_status = "Wi-Fi apply failed";
        else if (!save_ok)
            _wifi_status = "Wi-Fi applied, but save failed";
        else
            _wifi_status = "Wi-Fi updated";

        if (!_gsm)
            _gsm_status = "GSM unavailable";
        else if (!ActiveBoardProfile::GSM.enabled)
            _gsm_status = "GSM disabled by board profile";
        else if (!gsm_changed)
            _gsm_status = "No changes";
        else if (!gsm_ok && !save_ok)
            _gsm_status = "GSM apply and save failed";
        else if (!gsm_ok)
            _gsm_status = "GSM apply failed";
        else if (!save_ok)
            _gsm_status = "GSM applied, but save failed";
        else
            _gsm_status = "GSM updated";
sendRedirect_(request, "/", set_cookie);
    }




    void WebInterfaceControllersOps::handleStackSave_(AsyncWebServerRequest *request)
{
        bool set_cookie = false;
        if (!checkAuth_(request, &set_cookie))
            return;
        if (!requireWebAdmin_(request, &set_cookie))
            return;
        if (!_configs_manager)
        {
            _stack_status = "Config manager missing";
            sendRedirect_(request, "/stack", set_cookie);
            return;
        }

        const String role_value = request->hasParam("role", true) ? request->getParam("role", true)->value() : "master";
        String role = role_value;
        role.trim();
        role.toLowerCase();
        _configs_manager->setStackRole(role == "slave" ? ConfigsManagerIface::StackRole::Slave
                                                        : ConfigsManagerIface::StackRole::Master);

        String host = request->hasParam("master_host", true) ? request->getParam("master_host", true)->value() : "";
        host.trim();
        _configs_manager->setStackMasterHost(host);

        String policy_value =
            request->hasParam("exchange_policy", true) ? request->getParam("exchange_policy", true)->value() : "direct";
        policy_value.trim();
        policy_value.toLowerCase();
        ConfigsManagerIface::StackExchangePolicy policy = ConfigsManagerIface::StackExchangePolicy::Direct;
        if (policy_value == "direct")
            policy = ConfigsManagerIface::StackExchangePolicy::Direct;
        else if (policy_value == "poll")
            policy = ConfigsManagerIface::StackExchangePolicy::Poll;
        _configs_manager->setStackExchangePolicy(policy);

        String transport_value =
            request->hasParam("transport", true) ? request->getParam("transport", true)->value() : "websocket";
        transport_value.trim();
        transport_value.toLowerCase();
        const ConfigsManagerIface::StackTransportKind transport_kind =
            (transport_value == "rs485") ? ConfigsManagerIface::StackTransportKind::Rs485
                                         : ConfigsManagerIface::StackTransportKind::WebSocket;
        _configs_manager->setStackTransport(transport_kind);

        String payload_value =
            request->hasParam("payload_mode", true) ? request->getParam("payload_mode", true)->value() : "json";
        payload_value.trim();
        payload_value.toLowerCase();
        ConfigsManagerIface::StackPayloadMode payload_mode = ConfigsManagerIface::StackPayloadMode::Json;
        if (payload_value == "binary")
            payload_mode = ConfigsManagerIface::StackPayloadMode::Binary;
        if (transport_kind == ConfigsManagerIface::StackTransportKind::Rs485)
            payload_mode = ConfigsManagerIface::StackPayloadMode::Binary;
        _configs_manager->setStackPayloadMode(payload_mode);

        _configs_manager->setStackFallbackEnabled(request->hasParam("fallback_enabled", true));

        String fallback_host =
            request->hasParam("fallback_host", true) ? request->getParam("fallback_host", true)->value() : "";
        fallback_host.trim();
        _configs_manager->setStackFallbackHost(fallback_host);

        _configs_manager->setStackSlaveController(request->hasParam("slave_controller", true));

        String api_key = request->hasParam("api_key", true) ? request->getParam("api_key", true)->value() : "";
        api_key.trim();
        if (api_key == WebInterface::maskSecretValue_(_configs_manager->stackApiKey()))
            api_key = _configs_manager->stackApiKey();
        _configs_manager->setStackApiKey(api_key);

        const bool saved = _configs_manager->save();
        _stack_status = saved ? "Stack updated" : "Stack applied, save failed";
        sendRedirect_(request, "/stack", set_cookie);
    }



    void WebInterfaceControllersOps::handleStackGenKey_(AsyncWebServerRequest *request)
{
        bool set_cookie = false;
        if (!checkAuth_(request, &set_cookie))
            return;
        if (!requireWebAdmin_(request, &set_cookie))
            return;
        sendText_(request, 200, "text/plain", genApiKey_(), set_cookie);
    }



    void WebInterfaceControllersOps::handleDeviceSave_(AsyncWebServerRequest *request)
{
        bool set_cookie = false;
        if (!checkAuth_(request, &set_cookie))
            return;
        if (!requireWebAdmin_(request, &set_cookie))
            return;
        if (!_plc)
        {
            _device_status = "PLC missing";
            sendRedirect_(request, "/", set_cookie);
            return;
        }
        if (!request->hasParam("device_name", true))
        {
            _device_status = "Missing name";
            sendRedirect_(request, "/", set_cookie);
            return;
        }
        String name = request->getParam("device_name", true)->value();
        name.trim();
        name = sanitizeUtf8_(name);
        bool changed = (name != _plc->deviceName());
        if (changed)
            _plc->setDeviceName(name);

        bool save_ok = true;
        if (changed)
            save_ok = saveWifiConfig_();

        if (!changed)
            _device_status = "No changes";
        else if (!save_ok)
            _device_status = "Save failed";
        else
            _device_status = "Saved";

        sendRedirect_(request, "/", set_cookie);
    }



    void WebInterfaceControllersOps::handleReboot_(AsyncWebServerRequest *request)
{
        bool set_cookie = false;
        if (!checkAuth_(request, &set_cookie))
            return;
        if (!requireWebAdmin_(request, &set_cookie))
            return;
        sendRedirect_(request, "/", set_cookie);
        delay(100);
        ESP.restart();
    }



    void WebInterfaceControllersOps::handleFileDownload_(AsyncWebServerRequest *request)
{
        bool set_cookie = false;
        if (!checkAuth_(request, &set_cookie))
            return;
        if (!requireWebAdmin_(request, &set_cookie))
            return;
        String path;
        if (request->hasParam("path"))
            path = request->getParam("path")->value();
        else
            path = request->url().substring(String("/files").length());
        path = sanitizePath_(path);
        if (!path.length())
        {
            sendText_(request, 400, "text/plain", "Invalid path", set_cookie);
            return;
        }
        if (!LittleFS.exists(path))
        {
            sendText_(request, 404, "text/plain", "File not found", set_cookie);
            return;
        }
        auto *response = request->beginResponse(LittleFS, path, "application/octet-stream");
        if (set_cookie)
            response->addHeader("Set-Cookie", sessionCookie_());
        request->send(response);
    }



    void WebInterfaceControllersOps::handleDelete_(AsyncWebServerRequest *request)
{
        bool set_cookie = false;
        if (!checkAuth_(request, &set_cookie, true))
            return;
        if (!requireWebAdmin_(request, &set_cookie))
            return;
        if (!request->hasParam("path"))
        {
            sendText_(request, 400, "text/plain", "Missing path", set_cookie);
            return;
        }
        String path = sanitizePath_(request->getParam("path")->value());
        if (!path.length())
        {
            sendText_(request, 400, "text/plain", "Invalid path", set_cookie);
            return;
        }
        if (!LittleFS.exists(path))
        {
            sendText_(request, 404, "text/plain", "File not found", set_cookie);
            return;
        }
        if (!LittleFS.remove(path))
        {
            sendText_(request, 500, "text/plain", "Delete failed", set_cookie);
            return;
        }
        sendRedirect_(request, "/manage", set_cookie);
    }



    void WebInterfaceControllersOps::handleUiHash_(AsyncWebServerRequest *request)
{
        bool set_cookie = false;
        if (!checkAuth_(request, &set_cookie))
            return;
        String path = "/";
        if (request->hasParam("path"))
        {
            path = request->getParam("path")->value();
            if (path.length() == 0)
                path = "/";
        }
        const uint32_t hash = uiPageHash_(path);
        char buf[9] = {};
        snprintf(buf, sizeof(buf), "%08lX", (unsigned long)hash);
        sendText_(request, 200, "text/plain", buf, set_cookie);
    }



    bool WebInterfaceControllersOps::hasUsersRegistryWebAuth_() const
{
        if (!_users)
            return false;
        for (size_t i = 0; i < _users->size(); ++i)
        {
            const auto &u = _users->user(i);
            if (!u.enabled)
                continue;
            if (u.username.length() == 0)
                continue;
            if (!u.hasWebPassword())
                continue;
            return true;
        }
        return false;
    }



    bool WebInterfaceControllersOps::checkLegacyAdminAuth_(const String &user, const String &pass) const
{
        if (_cli_auth)
        {
            String ulow = user;
            ulow.toLowerCase();
            if (ulow == CliConsole::kAdminUser && _cli_auth->adminPasswordSet() && _cli_auth->checkAdminPassword(pass))
                return true;
        }
        return false;
    }



    bool WebInterfaceControllersOps::findUsersRegistryAuth_(const String &user, const String &pass, size_t &user_idx) const
{
        user_idx = 0;
        if (!_users)
            return false;
        const String login = UsersRegistry::normalizeUsername(user);
        if (login.length() == 0)
            return false;
        for (size_t i = 0; i < _users->size(); ++i)
        {
            const auto &u = _users->user(i);
            if (!u.enabled)
                continue;
            if (u.username.length() == 0 || !u.hasWebPassword())
                continue;
            if (UsersRegistry::normalizeUsername(u.username) != login)
                continue;
            if (!u.checkWebPassword(pass))
                continue;
            user_idx = i;
            return true;
        }
        return false;
    }



    bool WebInterfaceControllersOps::checkUsersRegistryAuth_(const String &user, const String &pass) const
{
        size_t user_idx = 0;
        return findUsersRegistryAuth_(user, pass, user_idx);
    }



    bool WebInterfaceControllersOps::checkAuth_(AsyncWebServerRequest *request, bool *set_cookie, bool require_session )
{
        if (set_cookie)
            *set_cookie = false;
        const bool users_web_auth = hasUsersRegistryWebAuth_();

        String token;
        if (extractSessionToken_(request, token) && sessionValid_(token))
        {
            if (!sessionPrincipalValid_())
            {
                clearSession_();
            }
            else
            {
                refreshSession_();
                return true;
            }
        }
        if (require_session)
        {
            sendText_(request, 403, "text/plain", "Session required", false);
            return false;
        }

        String user;
        String pass;
        if (parseBasicAuth_(request, user, pass))
        {
            size_t user_idx = 0;
            if (findUsersRegistryAuth_(user, pass, user_idx))
            {
                issueSession_((int16_t)user_idx);
                if (set_cookie)
                    *set_cookie = true;
                return true;
            }
            if (!users_web_auth && checkLegacyAdminAuth_(user, pass))
            {
                issueSession_(-1);
                if (set_cookie)
                    *set_cookie = true;
                return true;
            }
        }
        if (users_web_auth)
        {
            requestBasicAuth_(request);
            return false;
        }
        if (_auth_enabled && request->authenticate(_auth_user.c_str(), _auth_pass.c_str()))
        {
            issueSession_(-1);
            if (set_cookie)
                *set_cookie = true;
            return true;
        }
        if (!_auth_enabled && !_cli_auth)
            return true;
        requestBasicAuth_(request);
        return false;
    }



    bool WebInterfaceControllersOps::checkAuthApi_(AsyncWebServerRequest *request, bool *set_cookie)
{
        if (set_cookie)
            *set_cookie = false;
        if (!request)
            return false;
        const bool users_web_auth = hasUsersRegistryWebAuth_();

        String token;
        if (extractSessionToken_(request, token) && sessionValid_(token))
        {
            if (!sessionPrincipalValid_())
            {
                clearSession_();
            }
            else
            {
                refreshSession_();
                return true;
            }
        }

        String user;
        String pass;
        if (parseBasicAuth_(request, user, pass))
        {
            size_t user_idx = 0;
            if (findUsersRegistryAuth_(user, pass, user_idx))
            {
                issueSession_((int16_t)user_idx);
                if (set_cookie)
                    *set_cookie = true;
                return true;
            }
            if (!users_web_auth && checkLegacyAdminAuth_(user, pass))
            {
                issueSession_(-1);
                if (set_cookie)
                    *set_cookie = true;
                return true;
            }
        }

        sendText_(request, 403, "application/json", "{\"ok\":false,\"err\":\"auth\"}", false);
        return false;
    }



    const UsersRegistry::User *WebInterfaceControllersOps::sessionUser_() const
{
        if (!_users || _session_user_idx < 0)
            return nullptr;
        const size_t idx = (size_t)_session_user_idx;
        if (idx >= _users->size())
            return nullptr;
        const auto &u = _users->user(idx);
        if (!u.enabled)
            return nullptr;
        return &u;
    }



    uint8_t WebInterfaceControllersOps::aclUnitByNodeId_(uint32_t node_id) const
{
        if (node_id == 0)
            return 0;
        if (!_web._network)
            return UsersRegistry::kAclUnitCount;
        const size_t count = _web._network->stackOnlineDeviceCount();
        for (size_t i = 0; i < count; ++i)
        {
            StackDeviceRegistry::DeviceInfo device{};
            if (!_web._network->stackDeviceSnapshotAt(i, device) || !device.online || device.node_id == 0)
                continue;
            if (device.node_id == node_id)
            {
                const size_t unit = i + 1u;
                if (unit >= (size_t)UsersRegistry::kAclUnitCount)
                    return UsersRegistry::kAclUnitCount;
                return (uint8_t)unit;
            }
        }
        return UsersRegistry::kAclUnitCount;
    }



    bool WebInterfaceControllersOps::webAclControllerAllowed_(UsersRegistry::AclController ctrl, uint32_t node_id ) const
{
        if (webSessionIsAdmin_())
            return true;
        const UsersRegistry::User *u = sessionUser_();
        if (!u)
            return false;
        const uint8_t unit = aclUnitByNodeId_(node_id);
        if (unit >= UsersRegistry::kAclUnitCount)
            return false;
        return u->controllerAllowed(unit, ctrl);
    }



    bool WebInterfaceControllersOps::webAclCanViewItem_(UsersRegistry::AclController ctrl, uint16_t item_id, uint32_t node_id ) const
{
        if (webSessionIsAdmin_())
            return true;
        const UsersRegistry::User *u = sessionUser_();
        if (!u)
            return false;
        const uint8_t unit = aclUnitByNodeId_(node_id);
        if (unit >= UsersRegistry::kAclUnitCount)
            return false;
        return u->canViewItem(unit, ctrl, item_id);
    }



    bool WebInterfaceControllersOps::webAclCanControlItem_(UsersRegistry::AclController ctrl, uint16_t item_id, uint32_t node_id ) const
{
        if (webSessionIsAdmin_())
            return true;
        const UsersRegistry::User *u = sessionUser_();
        if (!u)
            return false;
        const uint8_t unit = aclUnitByNodeId_(node_id);
        if (unit >= UsersRegistry::kAclUnitCount)
            return false;
        return u->canControlItem(unit, ctrl, item_id);
    }



    bool WebInterfaceControllersOps::webSessionIsAdmin_() const
{
        const UsersRegistry::User *u = sessionUser_();
        if (u)
            return u->tg_admin;
        if (!hasUsersRegistryWebAuth_() && _session_user_idx < 0)
            return sessionPrincipalValid_();
        return false;
    }



    bool WebInterfaceControllersOps::requireWebAdmin_(AsyncWebServerRequest *request, bool *set_cookie)
{
        if (webSessionIsAdmin_())
            return true;
        sendText_(request, 403, "text/plain", "Admin only", set_cookie ? *set_cookie : false);
        return false;
    }



    bool WebInterfaceControllersOps::requireWebAclController_(AsyncWebServerRequest *request, bool *set_cookie,
                                  UsersRegistry::AclController ctrl, uint32_t node_id )
{
        if (webAclControllerAllowed_(ctrl, node_id))
            return true;
        sendText_(request, 403, "text/plain", "ACL deny", set_cookie ? *set_cookie : false);
        return false;
    }



    String WebInterfaceControllersOps::requestIp_(AsyncWebServerRequest *request) const
{
        if (!request)
            return "unknown";
        auto *client = request->client();
        if (!client)
            return "unknown";
        return client->remoteIP().toString();
    }



    String WebInterfaceControllersOps::wifiIp_() const
{
        if (_wifi.staActive() && _wifi.apActive())
        {
            const bool sta_connected = WiFi.status() == WL_CONNECTED;
            const String sta_ip = sta_connected ? WiFi.localIP().toString() : String("disconnected");
            const String ap_ip = WiFi.softAPIP().toString();
            return String("STA: ") + sta_ip + " | AP: " + ap_ip;
        }
        if (_wifi.apActive())
            return WiFi.softAPIP().toString();
        if (_wifi.staActive() && WiFi.status() == WL_CONNECTED)
            return WiFi.localIP().toString();
        return "disconnected";
    }



    String WebInterfaceControllersOps::wifiStaSegment_() const
{
        if (!_wifi.staEnabled())
            return "";
        return String(" | STA: <strong>") + wifiStaStatus_() + "</strong>";
    }



    String WebInterfaceControllersOps::wifiStaStatus_() const
{
        switch (WiFi.status())
        {
        case WL_IDLE_STATUS:
            return "Idle";
        case WL_NO_SSID_AVAIL:
            return "SSID not found";
        case WL_SCAN_COMPLETED:
            return "Scan complete";
        case WL_CONNECTED:
            return "Connected";
        case WL_CONNECT_FAILED:
            return "Connect failed";
        case WL_CONNECTION_LOST:
            return "Connection lost";
        case WL_DISCONNECTED:
            return "Disconnected";
        default:
            return "Unknown";
        }
    }



String WebInterfaceControllersOps::navHtml_() const
{
        auto appendNavLink = [](String &out, const __FlashStringHelper *href, const __FlashStringHelper *label, bool primary = false) {
            out += F("<a href=\"");
            out += href;
            out += F("\" data-nav-path=\"");
            out += href;
            out += F("\" style=\"position:relative;display:inline-flex;align-items:center;justify-content:center;padding:");
            out += primary ? F("9px 12px") : F("9px 10px");
            out += F(";border-radius:12px;border:1px solid transparent;background:");
            out += primary ? F("rgba(56,189,248,.08)") : F("transparent");
            out += F(";color:");
            out += primary ? F("#d9f5ff") : F("rgba(186,230,253,.88)");
            out += F(";text-decoration:none;font-weight:");
            out += primary ? F("700") : F("500");
            out += F(";font-size:");
            out += primary ? F("15px") : F("14px");
            out += F(";letter-spacing:.01em;white-space:nowrap;transition:color .16s ease,background .16s ease,border-color .16s ease\">");
            out += label;
            out += F("</a>");
        };

        String nav;
        nav.reserve(4800);
        nav += F("<style>"
                 ".fc-nav{margin-bottom:16px;padding:10px 12px 12px;border-radius:18px;border:1px solid rgba(56,189,248,.12);"
                 "background:linear-gradient(180deg, rgba(7,17,36,.88) 0%, rgba(5,13,28,.74) 100%);"
                 "box-shadow:0 14px 36px rgba(0,0,0,.18),inset 0 1px 0 rgba(255,255,255,.04)}"
                 ".fc-nav__row{display:flex;flex-wrap:wrap;gap:6px 8px;align-items:center}"
                 ".fc-nav__row a:hover{color:#e0f2fe !important;background:rgba(56,189,248,.08) !important;border-color:rgba(56,189,248,.14) !important}"
                 ".fc-nav__row a.fc-nav__active{color:#e0f2fe !important;background:rgba(56,189,248,.12) !important;border-color:rgba(56,189,248,.22) !important;box-shadow:inset 0 -2px 0 rgba(56,189,248,.85)}"
                 ".fc-nav__brand{margin-right:4px}"
                 "@media (max-width:760px){.fc-nav{padding:10px}.fc-nav__row{gap:6px}.fc-nav__row a{font-size:13px !important;padding:8px 9px !important}}"
                 "</style>");
        nav += F("<div class=\"nav fc-nav\"><div class=\"fc-nav__row\">");
        appendNavLink(nav, F("/"), F("FCPLC"), true);
        appendNavLink(nav, F("/controllers"), F("Контроллеры"));
        if (webSessionIsAdmin_())
        {
            appendNavLink(nav, F("/wifi"), F("Сеть"));
            appendNavLink(nav, F("/manage"), F("Прошивка и файлы"));
            appendNavLink(nav, F("/ports"), F("Порты"));
            appendNavLink(nav, F("/buses"), F("Шины"));
            appendNavLink(nav, F("/users"), F("Пользователи"));
            appendNavLink(nav, F("/display"), F("Дисплей"));
            appendNavLink(nav, F("/rules"), F("Правила"));
            appendNavLink(nav, F("/groups"), F("Группы"));
            appendNavLink(nav, F("/cloud"), F("Облако"));
            appendNavLink(nav, F("/stack"), F("Стек"));
            appendNavLink(nav, F("/admin"), F("Система"));
            appendNavLink(nav, F("/logs"), F("Logs"));
        }
        nav += F("</div></div>");
        nav += F(R"HTML(
<script>
(function(){
  const currentPath = window.location.pathname || '/';
  document.querySelectorAll('.fc-nav [data-nav-path]').forEach(function(link){
    const href = link.getAttribute('data-nav-path') || '';
    if (!href) return;
    if ((href === '/' && currentPath === '/') || (href !== '/' && currentPath === href)) {
      link.classList.add('fc-nav__active');
    }
  });
})();
</script>
)HTML");
        return nav;
    }



    String WebInterfaceControllersOps::deviceName_() const
{
        if (_plc)
            return _plc->deviceName();
        return "";
    }



    ConfigsManagerIface::StackRole WebInterfaceControllersOps::stackRole_() const
{
        return _configs_manager ? _configs_manager->stackRole() : ConfigsManagerIface::StackRole::Master;
    }



    String WebInterfaceControllersOps::stackMasterHost_() const
{
        return _configs_manager ? _configs_manager->stackMasterHost() : String();
    }



    ConfigsManagerIface::StackExchangePolicy WebInterfaceControllersOps::stackExchangePolicy_() const
{
        return _configs_manager ? _configs_manager->stackExchangePolicy() : ConfigsManagerIface::StackExchangePolicy::Direct;
    }



    ConfigsManagerIface::StackTransportKind WebInterfaceControllersOps::stackTransport_() const
{
        return _configs_manager ? _configs_manager->stackTransport() : ConfigsManagerIface::StackTransportKind::WebSocket;
    }



ConfigsManagerIface::StackPayloadMode WebInterfaceControllersOps::stackPayloadMode_() const
{
        return _configs_manager ? _configs_manager->stackPayloadMode() : ConfigsManagerIface::StackPayloadMode::Json;
    }



    bool WebInterfaceControllersOps::stackFallbackEnabled_() const
{
        return _configs_manager ? _configs_manager->stackFallbackEnabled() : false;
    }



    String WebInterfaceControllersOps::stackFallbackHost_() const
{
        return _configs_manager ? _configs_manager->stackFallbackHost() : String();
    }



    bool WebInterfaceControllersOps::stackSlaveController_() const
{
        return _configs_manager ? _configs_manager->stackSlaveController() : true;
    }



    String WebInterfaceControllersOps::stackApiKey_() const
{
        return _configs_manager ? _configs_manager->stackApiKey() : String();
    }



    bool WebInterfaceControllersOps::cloudEnabled_() const
{
        if (_configs_manager)
            return _configs_manager->cloudEnabled();
        return false;
    }



    CloudTransportKind WebInterfaceControllersOps::cloudTransport_() const
{
        if (_configs_manager)
            return _configs_manager->cloudTransport();
        return CloudTransportKind::WebSocket;
    }



    String WebInterfaceControllersOps::cloudHost_() const
{
        if (_configs_manager)
            return _configs_manager->cloudHost();
        return "";
    }



    uint16_t WebInterfaceControllersOps::cloudPort_() const
{
        if (_configs_manager)
            return _configs_manager->cloudPort();
        return 0;
    }



    String WebInterfaceControllersOps::cloudPath_() const
{
        if (_configs_manager)
            return _configs_manager->cloudPath();
        return "/";
    }



    bool WebInterfaceControllersOps::cloudUseSsl_() const
{
        if (_configs_manager)
            return _configs_manager->cloudUseSsl();
        return false;
    }



    uint32_t WebInterfaceControllersOps::cloudReconnectMs_() const
{
        if (_configs_manager)
            return _configs_manager->cloudReconnectMs();
        return 0;
    }



    uint32_t WebInterfaceControllersOps::cloudEventMs_() const
{
        if (_configs_manager)
            return _configs_manager->cloudEventIntervalMs();
        return 0;
    }



    String WebInterfaceControllersOps::cloudApiKey_() const
{
        if (_configs_manager)
            return _configs_manager->cloudApiKey();
        return "";
    }



    String WebInterfaceControllersOps::cloudFwVersion_() const
{
        if (_configs_manager)
            return _configs_manager->cloudFirmwareVersion();
        return "";
    }


    uint32_t WebInterfaceControllersOps::cloudDeviceId_() const
{
        return (uint32_t)(ESP.getEfuseMac() & 0xFFFFFFFFu);
    }



    bool WebInterfaceControllersOps::cloudConnected_() const
{
        if (_cloud)
            return _cloud->isConnected();
        return false;
    }

const char *WebInterfaceControllersOps::stackRoleName_(ConfigsManagerIface::StackRole role)
{
        return (role == ConfigsManagerIface::StackRole::Master) ? "master" : "slave";
    }



    bool WebInterfaceControllersOps::saveWifiConfig_()
{
        if (_configs_manager)
            return _configs_manager->save();
        return false;
    }



    bool WebInterfaceControllersOps::isAllowedExt_(const String &path) const
{
        if (_allowed_exts.length() == 0)
            return true;
        int dot = path.lastIndexOf('.');
        if (dot < 0)
            return false;
        String ext = path.substring(dot + 1);
        ext.toLowerCase();
        String list = _allowed_exts;
        list.replace(" ", "");
        size_t start = 0;
        while (start < list.length())
        {
            int comma = list.indexOf(',', start);
            if (comma < 0)
                comma = list.length();
            String token = list.substring(start, comma);
            if (token == ext)
                return true;
            start = (size_t)comma + 1;
        }
        return false;
    }

bool WebInterfaceControllersOps::parseSocketPort_(const String &input, uint8_t &out)
{
        String t = input;
        t.trim();
        t.toLowerCase();
        if (t.length() == 0 || t == "-" || t == "none")
        {
            out = SocketController::kInvalidPort;
            return true;
        }
        for (size_t i = 0; i < t.length(); ++i)
            if (t[i] < '0' || t[i] > '9')
                return false;
        const unsigned long v = strtoul(t.c_str(), nullptr, 10);
        if (v > 255)
            return false;
        out = (uint8_t)v;
        return true;
    }

const char *WebInterfaceControllersOps::displaySlotKindName_(DisplaySlotKind kind) const
{
        switch (kind)
        {
        case DisplaySlotKind::Time:
            return "time";
        case DisplaySlotKind::Socket:
            return "socket";
        case DisplaySlotKind::Light:
            return "light";
        case DisplaySlotKind::Meteo:
            return "meteo";
        case DisplaySlotKind::Thermo:
            return "thermo";
        case DisplaySlotKind::Tank:
            return "tank";
        case DisplaySlotKind::Septic:
            return "septic";
        case DisplaySlotKind::Security:
            return "security";
        case DisplaySlotKind::Avr:
            return "avr";
        case DisplaySlotKind::Leak:
            return "leak";
        case DisplaySlotKind::Text:
            return "text";
        case DisplaySlotKind::None:
        default:
            return "none";
        }
    }

const char *WebInterfaceControllersOps::displaySlotFieldName_(DisplaySlotField field) const
{
        switch (field)
        {
        case DisplaySlotField::TimeHm:
            return "hm";
        case DisplaySlotField::TimeMin:
            return "min";
        case DisplaySlotField::SocketState:
            return "state";
        case DisplaySlotField::LightState:
            return "state";
        case DisplaySlotField::MeteoTemp:
            return "temp";
        case DisplaySlotField::MeteoHum:
            return "hum";
        case DisplaySlotField::ThermoState:
            return "state";
        case DisplaySlotField::TankLevel:
            return "level";
        case DisplaySlotField::SepticLevel:
            return "level";
        case DisplaySlotField::SecurityArmed:
            return "armed";
        case DisplaySlotField::AvrSource:
            return "avr_source";
        case DisplaySlotField::AvrMainOk:
            return "avr_main_ok";
        case DisplaySlotField::AvrReserveOk:
            return "avr_reserve_ok";
        case DisplaySlotField::LeakState:
            return "leak_state";
        case DisplaySlotField::Text:
            return "text";
        case DisplaySlotField::None:
        default:
            return "none";
        }
    }

bool WebInterfaceControllersOps::parseDisplaySlotKind_(const String &input, DisplaySlotKind &out)
{
        String t = input;
        t.trim();
        t.toLowerCase();
        if (t.length() == 0 || t == "none")
        {
            out = DisplaySlotKind::None;
            return true;
        }
        if (t == "time")
            out = DisplaySlotKind::Time;
        else if (t == "socket")
            out = DisplaySlotKind::Socket;
        else if (t == "light")
            out = DisplaySlotKind::Light;
        else if (t == "meteo")
            out = DisplaySlotKind::Meteo;
        else if (t == "thermo")
            out = DisplaySlotKind::Thermo;
        else if (t == "tank")
            out = DisplaySlotKind::Tank;
        else if (t == "septic")
            out = DisplaySlotKind::Septic;
        else if (t == "security")
            out = DisplaySlotKind::Security;
        else if (t == "avr")
            out = DisplaySlotKind::Avr;
        else if (t == "leak")
            out = DisplaySlotKind::Leak;
        else if (t == "text")
            out = DisplaySlotKind::Text;
        else
            return false;
        return true;
    }

bool WebInterfaceControllersOps::parseDisplaySlotField_(const String &input, DisplaySlotField &out)
{
        String t = input;
        t.trim();
        t.toLowerCase();
        if (t.length() == 0 || t == "none")
        {
            out = DisplaySlotField::None;
            return true;
        }
        if (t == "hm")
            out = DisplaySlotField::TimeHm;
        else if (t == "min")
            out = DisplaySlotField::TimeMin;
        else if (t == "state")
            out = DisplaySlotField::SocketState;
        else if (t == "temp")
            out = DisplaySlotField::MeteoTemp;
        else if (t == "hum")
            out = DisplaySlotField::MeteoHum;
        else if (t == "level")
            out = DisplaySlotField::TankLevel;
        else if (t == "armed")
            out = DisplaySlotField::SecurityArmed;
        else if (t == "avr_source")
            out = DisplaySlotField::AvrSource;
        else if (t == "avr_main_ok")
            out = DisplaySlotField::AvrMainOk;
        else if (t == "avr_reserve_ok")
            out = DisplaySlotField::AvrReserveOk;
        else if (t == "leak_state")
            out = DisplaySlotField::LeakState;
        else if (t == "text")
            out = DisplaySlotField::Text;
        else
            return false;
        return true;
    }

bool WebInterfaceControllersOps::parseUint_(const String &input, uint16_t &out)
{
        String t = input;
        t.trim();
        if (t.length() == 0)
            return false;
        for (size_t i = 0; i < t.length(); ++i)
            if (t[i] < '0' || t[i] > '9')
                return false;
        const unsigned long v = strtoul(t.c_str(), nullptr, 10);
        if (v > 0xFFFFu)
            return false;
        out = (uint16_t)v;
        return true;
    }

bool WebInterfaceControllersOps::parseUint_(const String &input, uint32_t &out)
{
        String t = input;
        t.trim();
        if (t.length() == 0)
            return false;
        for (size_t i = 0; i < t.length(); ++i)
            if (t[i] < '0' || t[i] > '9')
                return false;
        const unsigned long v = strtoul(t.c_str(), nullptr, 10);
        out = (uint32_t)v;
        return true;
    }

bool WebInterfaceControllersOps::parseMeteoType_(const String &input, MeteoController::SensorType &out)
{
        String t = input;
        t.trim();
        t.toLowerCase();
        if (t.length() == 0 || t == "none")
        {
            out = MeteoController::SensorType::None;
            return true;
        }
        if (t == "ds18b20")
        {
            out = MeteoController::SensorType::Ds18b20;
            return true;
        }
        if (t == "dht22")
        {
            out = MeteoController::SensorType::Dht22;
            return true;
        }
        return false;
    }

MeteoController::SensorType WebInterfaceControllersOps::parseMeteoTypeName_(const char *input) const
{
        if (!input || !input[0])
            return MeteoController::SensorType::None;
        String t = input;
        t.toLowerCase();
        if (t == "dht22")
            return MeteoController::SensorType::Dht22;
        if (t == "ds18b20")
            return MeteoController::SensorType::Ds18b20;
        return MeteoController::SensorType::None;
    }

bool WebInterfaceControllersOps::parseSecurityType_(const String &input, SecurityController::SensorType &out)
{
        String t = input;
        t.trim();
        t.toLowerCase();
        if (t.length() == 0 || t == "pir")
        {
            out = SecurityController::SensorType::Pir;
            return true;
        }
        if (t == "reed")
        {
            out = SecurityController::SensorType::Reed;
            return true;
        }
        return false;
    }

bool WebInterfaceControllersOps::parseMeteoPin_(const String &input, uint8_t &out)
{
        String t = input;
        t.trim();
        t.toLowerCase();
        if (t.length() == 0 || t == "-" || t == "none")
        {
            out = MeteoController::kInvalidPin;
            return true;
        }
        for (size_t i = 0; i < t.length(); ++i)
            if (t[i] < '0' || t[i] > '9')
                return false;
        const unsigned long v = strtoul(t.c_str(), nullptr, 10);
        if (v > 255)
            return false;
        out = (uint8_t)v;
        return true;
    }

bool WebInterfaceControllersOps::parseMeteoAddr_(const String &input, uint8_t out[MeteoController::kAddrLen], bool &set)
{
        String t = input;
        t.trim();
        t.toLowerCase();
        if (t.length() == 0 || t == "-" || t == "none")
        {
            set = false;
            return true;
        }
        set = MeteoController::parseHexAddr(t.c_str(), out);
        return set;
    }

bool WebInterfaceControllersOps::parseSecurityKeyHex_(const String &s, uint8_t out[8])
{
        if (s.length() != 16)
            return false;
        for (uint8_t i = 0; i < 8; ++i)
        {
            const char hi_c = s[i * 2];
            const char lo_c = s[i * 2 + 1];
            auto nibble = [](char c) -> int {
                if (c >= '0' && c <= '9')
                    return c - '0';
                if (c >= 'a' && c <= 'f')
                    return 10 + (c - 'a');
                if (c >= 'A' && c <= 'F')
                    return 10 + (c - 'A');
                return -1;
            };
            const int hi = nibble(hi_c);
            const int lo = nibble(lo_c);
            if (hi < 0 || lo < 0)
                return false;
            out[i] = (uint8_t)((hi << 4) | lo);
        }
        return true;
    }

bool WebInterfaceControllersOps::parseThermoSensor_(const String &input, uint8_t &out, uint32_t &out_node)
{
        String t = input;
        t.trim();
        t.toLowerCase();
        out_node = 0;
        if (t.length() == 0 || t == "-" || t == "none")
        {
            out = ThermoController::kInvalidSensor;
            return true;
        }
        int sep = t.indexOf(':');
        String node_str;
        String sensor_str;
        if (sep >= 0)
        {
            node_str = t.substring(0, sep);
            sensor_str = t.substring(sep + 1);
        }
        else
        {
            sensor_str = t;
        }
        if (node_str.length())
        {
            for (size_t i = 0; i < (size_t)node_str.length(); ++i)
                if (node_str[i] < '0' || node_str[i] > '9')
                    return false;
            const unsigned long node_v = strtoul(node_str.c_str(), nullptr, 10);
            out_node = (uint32_t)node_v;
        }
        for (size_t i = 0; i < (size_t)sensor_str.length(); ++i)
            if (sensor_str[i] < '0' || sensor_str[i] > '9')
                return false;
        const unsigned long v = strtoul(sensor_str.c_str(), nullptr, 10);
        if (v > MeteoController::kSensorCount)
            return false;
        out = (uint8_t)v;
        return true;
    }

bool WebInterfaceControllersOps::parseThermoMode_(const String &input, ThermoController::Mode &out)
{
        String t = input;
        t.trim();
        t.toLowerCase();
        if (t.length() == 0 || t == "off" || t == "none")
        {
            out = ThermoController::Mode::Off;
            return true;
        }
        if (t == "heat" || t == "heat_only" || t == "only_heat")
        {
            out = ThermoController::Mode::Heat;
            return true;
        }
        if (t == "cool" || t == "cool_only" || t == "only_cool")
        {
            out = ThermoController::Mode::Cool;
            return true;
        }
        if (t == "auto")
        {
            out = ThermoController::Mode::Auto;
            return true;
        }
        return false;
    }

bool WebInterfaceControllersOps::parseThermoFloat_(const String &input, float &out)
{
        String t = input;
        t.trim();
        if (t.length() == 0)
            return false;
        const char *c = t.c_str();
        char *end = nullptr;
        const float v = strtof(c, &end);
        if (end == c)
            return false;
        out = v;
        return true;
    }

#undef webSessionIsAdmin_
#undef uiPageHash_
#undef stackNodeIdHex_
#undef sessionUser_
#undef sessionCookie_
#undef sendRedirect_
#undef sanitizeUtf8_
#undef sanitizePath_
#undef requireWebAdmin_
#undef refreshSession_
#undef owBusName_
#undef listStackNodesHtml_
#undef isAllowedExt_
#undef genApiKey_
#undef findUsersRegistryAuth_
#undef extractSessionToken_
#undef checkLegacyAdminAuth_
#undef appendJsonEscaped_
#undef aclUnitByNodeId_
#undef _wifi
#undef _upload_size
#undef _upload_ok
#undef _upload_in_progress
#undef _upload
#undef _session_user_idx
#undef _ow
#undef _ota_set_cookie
#undef _ota_name
#undef _ota_error
#undef _last_status
#undef _gsm_status
#undef _device_status
#undef _configs_manager
#undef _cli_auth
#undef _auth_pass
#undef _allowed_exts

#undef uiPageHash_
#undef sessionValid_
#undef sessionPrincipalValid_
#undef sessionCookie_
#undef sendText_
#undef sendRedirect_
#undef sanitizeUtf8_
#undef sanitizeUploadName_
#undef sanitizePath_
#undef safeHtmlValue_
#undef requestBasicAuth_
#undef refreshSession_
#undef parseBasicAuth_
#undef owBusName_
#undef owAddrToHex_
#undef issueSession_
#undef genApiKey_
#undef stackNodeIdHex_
#undef fanStatusIcon_
#undef formatTemp_
#undef extractSessionToken_
#undef clearSession_
#undef appendJsonEscaped_
#undef appendHtmlEscaped_
#undef _wifi_status
#undef _wifi
#undef _users
#undef _upload_size
#undef _upload_set_cookie
#undef _upload_ok
#undef _upload_name
#undef _upload_in_progress
#undef _upload_error
#undef _upload
#undef _stack_status
#undef _session_user_idx
#undef _plc
#undef _ow
#undef _ota_size
#undef _ota_set_cookie
#undef _ota_ok
#undef _ota_name
#undef _ota_in_progress
#undef _ota_error
#undef _max_upload
#undef _last_status
#undef _i2c
#undef _gsm_status
#undef _gsm
#undef _device_status
#undef _controllers
#undef _configs_manager
#undef _cloud
#undef _cli_auth
#undef _auth_user
#undef _auth_pass
#undef _auth_enabled
#undef _allowed_exts
