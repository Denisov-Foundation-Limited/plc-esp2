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

#include "utils/configs_manager.hpp"

#include <LittleFS.h>
#include <vector>

using CfgMgrStackRole = ConfigsManagerIface::StackRole;

ConfigsManager::ConfigsManager(Configs &configs, WifiManager &wifi, TelegramClient &telegram,
               Network &network, CliConsole &console, TelegramMenu &telegram_menu, PlcControl &plc,
               Controllers &controllers, RulesController &rules, GsmModem &gsm, UsersRegistry &users)
    : _configs(configs),
      _wifi(wifi),
      _telegram(telegram),
      _network(network),
      _console(console),
      _telegram_menu(telegram_menu),
      _plc(plc),
      _controllers(controllers),
      _rules(rules),
      _gsm(gsm),
      _users(users){
}

CfgMgrStackRole ConfigsManager::stackRole() const{ return _stack_role; }

String ConfigsManager::stackMasterHost() const{ return _stack_master_host; }

String ConfigsManager::stackApiKey() const{ return _stack_api_key; }

bool ConfigsManager::stackFallbackEnabled() const{ return _stack_fallback_enabled; }

String ConfigsManager::stackFallbackHost() const{ return _stack_fallback_host; }

bool ConfigsManager::stackSlaveController() const{ return _stack_slave_controller; }

bool ConfigsManager::cloudEnabled() const{ return _cloud_enabled; }

String ConfigsManager::cloudHost() const{ return _cloud_host; }

uint16_t ConfigsManager::cloudPort() const{ return _cloud_port; }

String ConfigsManager::cloudPath() const{ return _cloud_path; }

bool ConfigsManager::cloudUseSsl() const{ return _cloud_use_ssl; }

uint32_t ConfigsManager::cloudReconnectMs() const{ return _cloud_reconnect_ms; }

uint32_t ConfigsManager::cloudEventIntervalMs() const{ return _cloud_event_ms; }

String ConfigsManager::cloudApiKey() const{ return _cloud_api_key; }

String ConfigsManager::cloudFirmwareVersion() const{ return _cloud_fw_version; }

size_t ConfigsManager::displaySlotCount() const{ return kDisplaySlotCount; }

bool ConfigsManager::displaySlot(size_t idx, DisplaySlotConfig &out) const{
    if (idx >= kDisplaySlotCount)
        return false;
    out = _display_slots[idx];
    return true;
}

void ConfigsManager::setStackRole(StackRole role){ _stack_role = role; }

void ConfigsManager::setStackMasterHost(const String &host){ _stack_master_host = host; }

void ConfigsManager::setStackApiKey(const String &key){ _stack_api_key = key; }

void ConfigsManager::setStackFallbackEnabled(bool enabled){ _stack_fallback_enabled = enabled; }

void ConfigsManager::setStackFallbackHost(const String &host){ _stack_fallback_host = host; }

void ConfigsManager::setStackSlaveController(bool controller){ _stack_slave_controller = controller; }

void ConfigsManager::setCloudEnabled(bool enabled){
    if (enabled == _cloud_enabled)
        return;
    _cloud_enabled = enabled;
    _network.setCloudEnabled(enabled);
}

void ConfigsManager::setCloudHost(const String &host){
    if (host == _cloud_host)
        return;
    _cloud_host = host;
    applyCloudConfig_();
}

void ConfigsManager::setCloudPort(uint16_t port){
    if (port == _cloud_port)
        return;
    _cloud_port = port;
    applyCloudConfig_();
}

void ConfigsManager::setCloudPath(const String &path){
    if (path == _cloud_path)
        return;
    _cloud_path = path;
    applyCloudConfig_();
}

void ConfigsManager::setCloudUseSsl(bool use_ssl){
    if (use_ssl == _cloud_use_ssl)
        return;
    _cloud_use_ssl = use_ssl;
    applyCloudConfig_();
}

void ConfigsManager::setCloudReconnectMs(uint32_t ms){
    if (ms == _cloud_reconnect_ms)
        return;
    _cloud_reconnect_ms = ms;
    applyCloudConfig_();
}

void ConfigsManager::setCloudEventIntervalMs(uint32_t ms){
    if (ms == _cloud_event_ms)
        return;
    _cloud_event_ms = ms;
    _network.setCloudEventIntervalMs(ms);
}

void ConfigsManager::setCloudApiKey(const String &key){
    if (key == _cloud_api_key)
        return;
    _cloud_api_key = key;
    _network.setCloudApiKey(key);
}

void ConfigsManager::setCloudFirmwareVersion(const String &ver){
    if (ver == _cloud_fw_version)
        return;
    _cloud_fw_version = ver;
    _network.setCloudFirmwareVersion(ver);
}

void ConfigsManager::setDisplaySlot(size_t idx, const DisplaySlotConfig &slot){
    if (idx >= kDisplaySlotCount)
        return;
    _display_slots[idx] = slot;
}

bool ConfigsManager::loadConfigs(){
    _doc.clear();
    if (!_configs.load(_doc))
    {
        if (_configs.lastError() == Configs::Error::OpenRead)
        {
            const bool loaded_users_fs = loadUsersFromFs_();
            if (!loaded_users_fs)
                ensureDefaultUsers_();
            const bool loaded_rules_fs = loadRulesFromFs_();
            if (!loaded_rules_fs)
                ensureDefaultRules_();
            return true;
        }
        return false;
    }
    applyConfig_(_doc);
    // Dedicated users file has higher priority than legacy "users" in startup-config.
    const bool has_legacy_users = _doc["users"].is<JsonArrayConst>();
    // Dedicated rules file has higher priority than legacy "rules" in startup-config.
    const bool has_legacy_rules = _doc["rules"].is<JsonArrayConst>();
    const bool loaded_users_fs = loadUsersFromFs_();
    const bool loaded_rules_fs = loadRulesFromFs_();
    if (!loaded_users_fs && has_legacy_users)
        saveUsersToFs_();
    else if (!loaded_users_fs)
        ensureDefaultUsers_();
    if (!hasAnyWebLoginUser_())
        ensureDefaultUsers_();
    if (!loaded_rules_fs && has_legacy_rules)
        saveRulesToFs_();
    else if (!loaded_rules_fs)
        ensureDefaultRules_();
    return true;
}

bool ConfigsManager::save(){
    _doc.clear();
    JsonObject w = _doc["wifi"].to<JsonObject>();
    w["ssid"] = _wifi.ssid();
    w["password"] = _wifi.password();
    w["ap"] = _wifi.ap();
    w["ap_ssid"] = _wifi.apSsid();
    w["ap_password"] = _wifi.apPassword();

    JsonObject t = _doc["telegram"].to<JsonObject>();
    t["token"] = _telegram.token();
    t["insecure"] = _telegram.insecure();
    t["client"] = _telegram.clientKindName();
    t["use_proxy"] = _telegram.useProxy();
    t["proxy_host"] = _telegram.proxyHost();
    t["proxy_port"] = (unsigned)_telegram.proxyPort();
    t["proxy_path"] = _telegram.proxyPath();

    if (_console.adminPasswordSet())
    {
        JsonObject a = _doc["admin"].to<JsonObject>();
        a["password_hash"] = _console.adminPasswordHashHex();
    }

    JsonObject plc = _doc["plc"].to<JsonObject>();
    plc["device_name"] = _plc.deviceName();
    plc["buzzer"] = _plc.buzzerEnabled();

    JsonObject s = _doc["stack"].to<JsonObject>();
    s["role"] = (_stack_role == StackRole::Master) ? "master" : "slave";
    s["master_host"] = _stack_master_host;
    s["api_key"] = _stack_api_key;
    s["fallback_enabled"] = _stack_fallback_enabled;
    s["fallback_host"] = _stack_fallback_host;
    s["slave_controller"] = _stack_slave_controller;

    JsonObject ctrl = _doc["controllers"].to<JsonObject>();
    _controllers.serialize(ctrl);

    JsonObject g = _doc["gsm"].to<JsonObject>();
    g["enabled"] = _gsm.enabled();

    JsonObject disp = _doc["display"].to<JsonObject>();
    JsonArray slots = disp["slots"].to<JsonArray>();
    for (size_t i = 0; i < kDisplaySlotCount; ++i)
    {
        const DisplaySlotConfig &slot = _display_slots[i];
        JsonObject obj = slots.add<JsonObject>();
        obj["kind"] = displayKindName_(slot.kind);
        if (slot.node_id)
            obj["node_id"] = (unsigned long)slot.node_id;
        if (slot.index)
            obj["index"] = (unsigned)slot.index;
        if (slot.field != DisplaySlotField::None)
            obj["field"] = displayFieldName_(slot.field);
        if (slot.kind == DisplaySlotKind::Text && slot.text[0])
            obj["text"] = slot.text;
    }

    JsonObject c = _doc["cloud"].to<JsonObject>();
    c["enabled"] = _cloud_enabled;
    c["host"] = _cloud_host;
    c["port"] = (unsigned)_cloud_port;
    c["path"] = _cloud_path;
    c["ssl"] = _cloud_use_ssl;
    if (_cloud_reconnect_ms)
        c["reconnect_ms"] = (unsigned)_cloud_reconnect_ms;
    if (_cloud_api_key.length())
        c["api_key"] = _cloud_api_key;
    if (_cloud_fw_version.length())
        c["fw_version"] = _cloud_fw_version;
    if (_cloud_event_ms)
        c["event_ms"] = (unsigned)_cloud_event_ms;

    if (_doc.overflowed())
        return false;

    if (!_configs.save(_doc))
        return false;
    if (!saveUsersToFs_())
        return false;
    return saveRulesToFs_();
}

bool ConfigsManager::save(const JsonDocument &doc){
    DynamicJsonDocument tmp(kConfigDocCapacity);
    tmp.set(doc);
    if (tmp.overflowed())
        return false;
    if (doc["users"].is<JsonArrayConst>())
    {
        _users.applyFromJson(doc["users"].as<JsonArrayConst>());
    }
    if (doc["rules"].is<JsonArrayConst>())
    {
        _rules.applyConfig(doc["rules"].as<JsonArrayConst>());
    }
    tmp.remove("users");
    tmp.remove("rules");
    if (!_configs.save(tmp))
        return false;
    if (!saveUsersToFs_())
        return false;
    return saveRulesToFs_();
}

bool ConfigsManager::hasAnyWebLoginUser_() const{
    for (size_t i = 0; i < _users.size(); ++i)
    {
        const auto &u = _users.user(i);
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

void ConfigsManager::ensureDefaultUsers_(){
    _users.clear();
    auto &u = _users.user(0);
    u.enabled = true;
    u.username = "admin";
    u.tg_admin = true;
    u.setWebPassword("1234");
    saveUsersToFs_();
}

void ConfigsManager::ensureDefaultRules_(){
    _rules.resetDefaults();
    saveRulesToFs_();
}

bool ConfigsManager::loadUsersFromFs_(){
    if (!LittleFS.exists(kUsersPath))
        return false;
    File f = LittleFS.open(kUsersPath, "r");
    if (!f)
        return false;
    DynamicJsonDocument doc(24576);
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err)
        return false;
    if (doc["users"].is<JsonArrayConst>())
    {
        _users.applyFromJson(doc["users"].as<JsonArrayConst>());
        if (!hasAnyWebLoginUser_())
            ensureDefaultUsers_();
        return true;
    }
    if (doc.is<JsonArrayConst>())
    {
        _users.applyFromJson(doc.as<JsonArrayConst>());
        if (!hasAnyWebLoginUser_())
            ensureDefaultUsers_();
        return true;
    }
    return false;
}

bool ConfigsManager::saveUsersToFs_(){
    DynamicJsonDocument doc(24576);
    JsonArray users = doc["users"].to<JsonArray>();
    _users.serializeToJson(users);
    File f = LittleFS.open(kUsersPath, "w");
    if (!f)
        return false;
    if (serializeJsonPretty(doc, f) == 0)
    {
        f.close();
        return false;
    }
    f.close();
    return true;
}

bool ConfigsManager::loadRulesFromFs_(){
    if (!LittleFS.exists(kRulesPath))
        return false;
    File f = LittleFS.open(kRulesPath, "r");
    if (!f)
        return false;
    DynamicJsonDocument doc(32768);
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err)
        return false;
    if (doc["rules"].is<JsonArrayConst>())
    {
        _rules.applyConfig(doc["rules"].as<JsonArrayConst>());
        return true;
    }
    if (doc.is<JsonArrayConst>())
    {
        _rules.applyConfig(doc.as<JsonArrayConst>());
        return true;
    }
    return false;
}

bool ConfigsManager::saveRulesToFs_(){
    DynamicJsonDocument doc(32768);
    JsonArray rules = doc["rules"].to<JsonArray>();
    _rules.serialize(rules);
    File f = LittleFS.open(kRulesPath, "w");
    if (!f)
        return false;
    if (serializeJsonPretty(doc, f) == 0)
    {
        f.close();
        return false;
    }
    f.close();
    return true;
}

void ConfigsManager::applyConfig_(const JsonDocument &doc){
    if (doc["wifi"].is<JsonObjectConst>())
    {
        JsonObjectConst w = doc["wifi"].as<JsonObjectConst>();
        if (w["ssid"].is<const char *>())
            _wifi.setSsid(w["ssid"].as<const char *>());
        if (w["password"].is<const char *>())
            _wifi.setPassword(w["password"].as<const char *>());
        if (w["ap"].is<bool>())
            _wifi.setAp(w["ap"].as<bool>());
        if (w["ap_ssid"].is<const char *>())
            _wifi.setApSsid(w["ap_ssid"].as<const char *>());
        if (w["ap_password"].is<const char *>())
            _wifi.setApPassword(w["ap_password"].as<const char *>());
    }

    if (doc["telegram"].is<JsonObjectConst>())
    {
        JsonObjectConst t = doc["telegram"].as<JsonObjectConst>();
        if (t["token"].is<const char *>())
            _telegram.setToken(t["token"].as<const char *>());
        if (t["insecure"].is<bool>())
            _telegram.setInsecure(t["insecure"].as<bool>());

        if (t["client"].is<const char *>())
        {
            String c = t["client"].as<const char *>();
            c.toLowerCase();
            if (c == "tinygsm")
                _network.setTelegramClientKind(TelegramNetCfg::ClientKind::TinyGsm);
            else if (c == "wifi" || c == "wifi_secure")
                _network.setTelegramClientKind(TelegramNetCfg::ClientKind::WifiSecure);
        }

        bool proxy_override = false;
        bool use_proxy = false;
        String host;
        uint16_t port = 0;
        String path;

        if (t["use_proxy"].is<bool>())
        {
            proxy_override = true;
            use_proxy = t["use_proxy"].as<bool>();
        }
        if (t["proxy_host"].is<const char *>())
        {
            proxy_override = true;
            host = t["proxy_host"].as<const char *>();
            if (!t["use_proxy"].is<bool>())
                use_proxy = true;
        }
        if (t["proxy_port"].is<unsigned>())
        {
            proxy_override = true;
            port = (uint16_t)t["proxy_port"].as<unsigned>();
            if (!t["use_proxy"].is<bool>())
                use_proxy = true;
        }
        if (t["proxy_path"].is<const char *>())
        {
            proxy_override = true;
            path = t["proxy_path"].as<const char *>();
            if (!t["use_proxy"].is<bool>())
                use_proxy = true;
        }

        if (proxy_override)
        {
            if (use_proxy && host.length() > 0)
                _network.setTelegramProxy(host, port, path);
            else
                _network.disableTelegramProxy();
        }
    }

    if (doc["users"].is<JsonArrayConst>())
    {
        _users.applyFromJson(doc["users"].as<JsonArrayConst>());
    }
    else if (doc["controllers"].is<JsonObjectConst>())
    {
        JsonObjectConst ctrl = doc["controllers"].as<JsonObjectConst>();
        if (ctrl["security_keys"].is<JsonArrayConst>())
        {
            JsonArrayConst arr = ctrl["security_keys"].as<JsonArrayConst>();
            size_t idx = 0;
            for (JsonVariantConst v : arr)
            {
                if (idx >= _users.size())
                    break;
                const char *serial = nullptr;
                bool enabled = true;
                if (v.is<const char *>())
                {
                    serial = v.as<const char *>();
                }
                else if (v.is<JsonObjectConst>())
                {
                    JsonObjectConst obj = v.as<JsonObjectConst>();
                    if (obj["serial"].is<const char *>())
                        serial = obj["serial"].as<const char *>();
                    else if (obj["addr"].is<const char *>())
                        serial = obj["addr"].as<const char *>();
                    if (obj["enabled"].is<bool>())
                        enabled = obj["enabled"].as<bool>();
                    if (obj["name"].is<const char *>())
                        _users.user(idx).username = obj["name"].as<const char *>();
                }
                if (serial && enabled)
                    _users.user(idx).ibutton_key = UsersRegistry::normalizeHex(serial, 16);
                ++idx;
            }
        }
        if (ctrl["security_rfid_keys"].is<JsonArrayConst>())
        {
            JsonArrayConst arr = ctrl["security_rfid_keys"].as<JsonArrayConst>();
            size_t idx = 0;
            for (JsonVariantConst v : arr)
            {
                if (idx >= _users.size())
                    break;
                const char *serial = nullptr;
                bool enabled = true;
                if (v.is<const char *>())
                {
                    serial = v.as<const char *>();
                }
                else if (v.is<JsonObjectConst>())
                {
                    JsonObjectConst obj = v.as<JsonObjectConst>();
                    if (obj["serial"].is<const char *>())
                        serial = obj["serial"].as<const char *>();
                    else if (obj["uid"].is<const char *>())
                        serial = obj["uid"].as<const char *>();
                    if (obj["enabled"].is<bool>())
                        enabled = obj["enabled"].as<bool>();
                    if (obj["name"].is<const char *>())
                        _users.user(idx).username = obj["name"].as<const char *>();
                }
                if (serial && enabled)
                    _users.user(idx).rfid_key = UsersRegistry::normalizeHex(serial, 20);
                ++idx;
            }
        }
    }
    if (doc["controllers"].is<JsonObjectConst>())
    {
        JsonObjectConst ctrl = doc["controllers"].as<JsonObjectConst>();
        if (ctrl["security_phones"].is<JsonArrayConst>())
        {
            JsonArrayConst arr = ctrl["security_phones"].as<JsonArrayConst>();
            size_t idx = 0;
            for (JsonVariantConst v : arr)
            {
                if (idx >= _users.size())
                    break;
                const char *number = nullptr;
                bool enabled = true;
                bool sms = false;
                bool call = false;
                if (v.is<const char *>())
                {
                    number = v.as<const char *>();
                }
                else if (v.is<JsonObjectConst>())
                {
                    JsonObjectConst obj = v.as<JsonObjectConst>();
                    if (obj["id"].is<unsigned>())
                    {
                        const unsigned raw = obj["id"].as<unsigned>();
                        if (raw >= 1 && raw <= _users.size())
                            idx = (size_t)(raw - 1);
                    }
                    if (obj["number"].is<const char *>())
                        number = obj["number"].as<const char *>();
                    if (obj["enabled"].is<bool>())
                        enabled = obj["enabled"].as<bool>();
                    if (obj["notify"].is<bool>())
                        sms = obj["notify"].as<bool>();
                    if (obj["call"].is<bool>())
                        call = obj["call"].as<bool>();
                    if (obj["name"].is<const char *>())
                    {
                        if (_users.user(idx).username.length() == 0)
                            _users.user(idx).username = obj["name"].as<const char *>();
                    }
                }
                auto &u = _users.user(idx);
                if (u.gsm_phone.length() == 0 && number && enabled)
                    u.gsm_phone = UsersRegistry::normalizePhone(number);
                if (!u.gsm_sms)
                    u.gsm_sms = sms;
                if (!u.gsm_call)
                    u.gsm_call = call;
                if (u.gsm_phone.length() && !u.enabled)
                    u.enabled = enabled;
                ++idx;
            }
        }
    }

    if (doc["gsm"].is<JsonObjectConst>())
    {
        JsonObjectConst g = doc["gsm"].as<JsonObjectConst>();
        if (g["enabled"].is<bool>())
            _gsm.setEnabled(g["enabled"].as<bool>());
    }

    if (doc["cloud"].is<JsonObjectConst>())
    {
        JsonObjectConst c = doc["cloud"].as<JsonObjectConst>();
        if (c["enabled"].is<bool>())
            _cloud_enabled = c["enabled"].as<bool>();
        _cloud_host = c["host"] | "";
        _cloud_port = (uint16_t)(c["port"] | 0u);
        _cloud_path = c["path"] | "/";
        _cloud_use_ssl = c["ssl"] | false;
        _cloud_reconnect_ms = (uint32_t)(c["reconnect_ms"] | _cloud_reconnect_ms);
        _cloud_api_key = c["api_key"] | "";
        _cloud_fw_version = c["fw_version"] | "";
        _cloud_event_ms = (uint32_t)(c["event_ms"] | _cloud_event_ms);

        if (_cloud_host.length())
            _network.setCloudConfig(buildCloudConfig_());
        _network.setCloudApiKey(_cloud_api_key);
        _network.setCloudFirmwareVersion(_cloud_fw_version);
        _network.setCloudEventIntervalMs(_cloud_event_ms);
        _network.setCloudEnabled(_cloud_enabled);
    }

    if (doc["admin"].is<JsonObjectConst>())
    {
        JsonObjectConst a = doc["admin"].as<JsonObjectConst>();
        if (a["password_hash"].is<const char *>())
        {
            _console.setAdminPasswordHashHex_(a["password_hash"].as<const char *>());
        }
        else if (a["password"].is<const char *>())
        {
            _console.setAdminPassword_(a["password"].as<const char *>());
        }
    }

    if (doc["plc"].is<JsonObjectConst>())
    {
        JsonObjectConst p = doc["plc"].as<JsonObjectConst>();
        if (p["device_name"].is<const char *>())
        {
            String name = p["device_name"].as<const char *>();
            name = sanitizeUtf8_(name);
            _plc.setDeviceName(name);
        }
        if (p["buzzer"].is<bool>())
            _plc.setBuzzerEnabled(p["buzzer"].as<bool>());
    }

    if (doc["stack"].is<JsonObjectConst>())
    {
        JsonObjectConst s = doc["stack"].as<JsonObjectConst>();
        if (s["role"].is<const char *>())
        {
            String role = s["role"].as<const char *>();
            role.toLowerCase();
            _stack_role = (role == "slave") ? StackRole::Slave : StackRole::Master;
        }
        if (s["master_host"].is<const char *>())
            _stack_master_host = s["master_host"].as<const char *>();
        if (s["api_key"].is<const char *>())
            _stack_api_key = s["api_key"].as<const char *>();
        if (s["fallback_enabled"].is<bool>())
            _stack_fallback_enabled = s["fallback_enabled"].as<bool>();
        if (s["fallback_host"].is<const char *>())
            _stack_fallback_host = s["fallback_host"].as<const char *>();
        if (s["slave_controller"].is<bool>())
            _stack_slave_controller = s["slave_controller"].as<bool>();
    }

    if (doc["display"].is<JsonObjectConst>())
    {
        JsonObjectConst disp = doc["display"].as<JsonObjectConst>();
        if (disp["slots"].is<JsonArrayConst>())
        {
            JsonArrayConst slots = disp["slots"].as<JsonArrayConst>();
            size_t idx = 0;
            for (JsonVariantConst v : slots)
            {
                if (idx >= kDisplaySlotCount)
                    break;
                if (!v.is<JsonObjectConst>())
                {
                    ++idx;
                    continue;
                }
                JsonObjectConst obj = v.as<JsonObjectConst>();
                DisplaySlotConfig slot{};
                slot.kind = parseDisplayKind_(obj["kind"]);
                slot.field = parseDisplayField_(obj["field"], slot.kind);
                if (obj["node_id"].is<unsigned long>())
                    slot.node_id = (uint32_t)obj["node_id"].as<unsigned long>();
                if (obj["index"].is<unsigned>())
                    slot.index = (uint8_t)obj["index"].as<unsigned>();
                if (obj["text"].is<const char *>())
                {
                    const char *txt = obj["text"].as<const char *>();
                    if (txt)
                    {
                        strncpy(slot.text, txt, sizeof(slot.text) - 1);
                        slot.text[sizeof(slot.text) - 1] = '\0';
                    }
                }
                _display_slots[idx] = slot;
                ++idx;
            }
        }
    }

    if (doc["controllers"].is<JsonObjectConst>())
    {
        _controllers.applyConfig(doc["controllers"].as<JsonObjectConst>());
    }
    if (doc["rules"].is<JsonArrayConst>())
    {
        _rules.applyConfig(doc["rules"].as<JsonArrayConst>());
    }
}

String ConfigsManager::sanitizeUtf8_(const String &in){
    if (isValidUtf8_(in))
        return in;
    return cp1251ToUtf8_(in);
}

bool ConfigsManager::isValidUtf8_(const String &in){
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

void ConfigsManager::appendUtf8_(String &out, uint16_t code){
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

String ConfigsManager::cp1251ToUtf8_(const String &in){
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

CloudClient::Config ConfigsManager::buildCloudConfig_() const{
    CloudClient::Config cfg;
    cfg.host = _cloud_host;
    cfg.port = _cloud_port;
    cfg.path = _cloud_path;
    cfg.use_ssl = _cloud_use_ssl;
    cfg.reconnect_ms = _cloud_reconnect_ms;
    return cfg;
}

void ConfigsManager::applyCloudConfig_(){
    _network.setCloudConfig(buildCloudConfig_());
}

const char *ConfigsManager::displayKindName_(DisplaySlotKind kind){
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

const char *ConfigsManager::displayFieldName_(DisplaySlotField field){
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
    case DisplaySlotField::LeakState:
        return "leak_state";
    case DisplaySlotField::Text:
        return "text";
    case DisplaySlotField::None:
    default:
        return "none";
    }
}

DisplaySlotKind ConfigsManager::parseDisplayKind_(JsonVariantConst v){
    if (v.is<const char *>())
    {
        const char *raw = v.as<const char *>();
        if (!raw)
            return DisplaySlotKind::None;
        String s = raw;
        s.toLowerCase();
        if (s == "time")
            return DisplaySlotKind::Time;
        if (s == "socket")
            return DisplaySlotKind::Socket;
        if (s == "light")
            return DisplaySlotKind::Light;
        if (s == "meteo")
            return DisplaySlotKind::Meteo;
        if (s == "thermo")
            return DisplaySlotKind::Thermo;
        if (s == "tank")
            return DisplaySlotKind::Tank;
        if (s == "septic")
            return DisplaySlotKind::Septic;
        if (s == "security")
            return DisplaySlotKind::Security;
        if (s == "avr")
            return DisplaySlotKind::Avr;
        if (s == "leak")
            return DisplaySlotKind::Leak;
        if (s == "text")
            return DisplaySlotKind::Text;
        return DisplaySlotKind::None;
    }
    if (v.is<unsigned>())
    {
        const unsigned raw = v.as<unsigned>();
        if (raw <= (unsigned)DisplaySlotKind::Text)
            return (DisplaySlotKind)raw;
    }
    return DisplaySlotKind::None;
}

DisplaySlotField ConfigsManager::parseDisplayField_(JsonVariantConst v, DisplaySlotKind kind){
    if (v.is<const char *>())
    {
        const char *raw = v.as<const char *>();
        if (!raw)
            return DisplaySlotField::None;
        String s = raw;
        s.toLowerCase();
        if (s == "hm")
            return DisplaySlotField::TimeHm;
        if (s == "min")
            return DisplaySlotField::TimeMin;
        if (s == "state")
        {
            if (kind == DisplaySlotKind::Thermo)
                return DisplaySlotField::ThermoState;
            if (kind == DisplaySlotKind::Light)
                return DisplaySlotField::LightState;
            return DisplaySlotField::SocketState;
        }
        if (s == "temp")
            return DisplaySlotField::MeteoTemp;
        if (s == "hum")
            return DisplaySlotField::MeteoHum;
        if (s == "level")
        {
            if (kind == DisplaySlotKind::Septic)
                return DisplaySlotField::SepticLevel;
            return DisplaySlotField::TankLevel;
        }
        if (s == "armed")
            return DisplaySlotField::SecurityArmed;
        if (s == "avr_source")
            return DisplaySlotField::AvrSource;
        if (s == "leak_state")
            return DisplaySlotField::LeakState;
        if (s == "text")
            return DisplaySlotField::Text;
        return DisplaySlotField::None;
    }
    if (v.is<unsigned>())
    {
        const unsigned raw = v.as<unsigned>();
        if (raw <= (unsigned)DisplaySlotField::TimeMin)
            return (DisplaySlotField)raw;
    }
    if (kind == DisplaySlotKind::Text)
        return DisplaySlotField::Text;
    if (kind == DisplaySlotKind::Time)
        return DisplaySlotField::TimeHm;
    if (kind == DisplaySlotKind::Socket)
        return DisplaySlotField::SocketState;
    if (kind == DisplaySlotKind::Light)
        return DisplaySlotField::LightState;
    if (kind == DisplaySlotKind::Meteo)
        return DisplaySlotField::MeteoTemp;
    if (kind == DisplaySlotKind::Thermo)
        return DisplaySlotField::ThermoState;
    if (kind == DisplaySlotKind::Tank)
        return DisplaySlotField::TankLevel;
    if (kind == DisplaySlotKind::Septic)
        return DisplaySlotField::SepticLevel;
    if (kind == DisplaySlotKind::Security)
        return DisplaySlotField::SecurityArmed;
    if (kind == DisplaySlotKind::Avr)
        return DisplaySlotField::AvrSource;
    if (kind == DisplaySlotKind::Leak)
        return DisplaySlotField::LeakState;
    return DisplaySlotField::None;
}
