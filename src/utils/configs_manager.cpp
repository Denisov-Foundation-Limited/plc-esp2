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
#include <string.h>
#include <vector>

using CfgMgrStackRole = ConfigsManagerIface::StackRole;

ConfigsManager::ConfigsManager(Configs &configs, WifiManager &wifi, Network &network, CliConsole &console, PlcControl &plc,
               Controllers &controllers, RulesController &rules, GsmModem &gsm, UsersRegistry &users)
    : _configs(configs),
      _wifi(wifi),
      _network(network),
      _console(console),
      _plc(plc),
      _controllers(controllers),
      _rules(rules),
      _gsm(gsm),
      _users(users){
}

CfgMgrStackRole ConfigsManager::stackRole() const{ return CfgMgrStackRole::Master; }

String ConfigsManager::stackMasterHost() const{ return ""; }

String ConfigsManager::stackApiKey() const{ return ""; }

bool ConfigsManager::stackFallbackEnabled() const{ return false; }

String ConfigsManager::stackFallbackHost() const{ return ""; }

bool ConfigsManager::stackSlaveController() const{ return true; }

bool ConfigsManager::cloudEnabled() const{ return _cloud_enabled; }

CloudTransportKind ConfigsManager::cloudTransport() const{ return _cloud_transport; }

String ConfigsManager::cloudHost() const{ return _cloud_host; }

uint16_t ConfigsManager::cloudPort() const{ return _cloud_port; }

String ConfigsManager::cloudPath() const{ return _cloud_path; }

bool ConfigsManager::cloudUseSsl() const{ return _cloud_use_ssl; }

uint32_t ConfigsManager::cloudReconnectMs() const{ return _cloud_reconnect_ms; }

uint32_t ConfigsManager::cloudEventIntervalMs() const{ return _cloud_event_ms; }

String ConfigsManager::cloudApiKey() const{ return _cloud_api_key; }

String ConfigsManager::cloudFirmwareVersion() const{ return _cloud_fw_version; }

bool ConfigsManager::eepromSaveEnabled() const{ return _eeprom_save_enabled; }

bool ConfigsManager::eepromLoadEnabled() const{ return _eeprom_load_enabled; }

size_t ConfigsManager::groupCount() const{
    size_t count = 0;
    for (const auto &g : _groups)
    {
        if (g.id != 0 && g.name.length())
            ++count;
    }
    return count;
}

bool ConfigsManager::groupByIndex(size_t idx, GroupConfig &out) const{
    bool used[kGroupCount] = {};
    for (size_t pos = 0; pos <= idx; ++pos)
    {
        size_t best = kGroupCount;
        for (size_t i = 0; i < kGroupCount; ++i)
        {
            const auto &g = _groups[i];
            if (used[i] || g.id == 0 || g.name.length() == 0)
                continue;
            if (best >= kGroupCount ||
                g.sort < _groups[best].sort ||
                (g.sort == _groups[best].sort && strcmp(g.name.c_str(), _groups[best].name.c_str()) < 0) ||
                (g.sort == _groups[best].sort && g.name == _groups[best].name && g.id < _groups[best].id))
            {
                best = i;
            }
        }
        if (best >= kGroupCount)
            return false;
        used[best] = true;
        if (pos == idx)
        {
            out = _groups[best];
            return true;
        }
    }
    return false;
}

bool ConfigsManager::setGroup(uint8_t id, const String &name, uint16_t sort){
    if (id == 0)
        return false;
    const String clean = sanitizeUtf8_(name);
    for (auto &g : _groups)
    {
        if (g.id == id)
        {
            g.name = clean;
            g.sort = sort;
            return true;
        }
    }
    for (auto &g : _groups)
    {
        if (g.id == 0 || g.name.length() == 0)
        {
            g.id = id;
            g.name = clean;
            g.sort = sort;
            return true;
        }
    }
    return false;
}

bool ConfigsManager::removeGroup(uint8_t id){
    if (id == 0)
        return false;
    for (auto &g : _groups)
    {
        if (g.id != id)
            continue;
        g = GroupConfig{};
        clearGroupRefs_(id);
        return true;
    }
    return false;
}

uint8_t ConfigsManager::allocateGroupId() const{
    for (unsigned id = 1; id <= 255; ++id)
    {
        bool used = false;
        for (const auto &g : _groups)
        {
            if (g.id == id)
            {
                used = true;
                break;
            }
        }
        if (!used)
            return (uint8_t)id;
    }
    return 0;
}

size_t ConfigsManager::displaySlotCount() const{ return kDisplaySlotCount; }

bool ConfigsManager::displaySlot(size_t idx, DisplaySlotConfig &out) const{
    if (idx >= kDisplaySlotCount)
        return false;
    out = _display_slots[idx];
    return true;
}

void ConfigsManager::setStackRole(StackRole role){ (void)role; }

void ConfigsManager::setStackMasterHost(const String &host){ (void)host; }

void ConfigsManager::setStackApiKey(const String &key){ (void)key; }

void ConfigsManager::setStackFallbackEnabled(bool enabled){ (void)enabled; }

void ConfigsManager::setStackFallbackHost(const String &host){ (void)host; }

void ConfigsManager::setStackSlaveController(bool controller){ (void)controller; }

void ConfigsManager::setCloudEnabled(bool enabled){
    if (enabled == _cloud_enabled)
        return;
    _cloud_enabled = enabled;
    _network.setCloudEnabled(enabled);
}

void ConfigsManager::setCloudTransport(CloudTransportKind kind){
    if (kind == _cloud_transport)
        return;
    _cloud_transport = kind;
    applyCloudConfig_();
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

void ConfigsManager::setEepromSaveEnabled(bool enabled){
    if (enabled == _eeprom_save_enabled)
        return;
    _eeprom_save_enabled = enabled;
    _controllers.setEepromSaveEnabled(enabled);
}

void ConfigsManager::setEepromLoadEnabled(bool enabled){
    if (enabled == _eeprom_load_enabled)
        return;
    _eeprom_load_enabled = enabled;
    _controllers.setEepromLoadEnabled(enabled);
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
    w["mode"] = _wifi.modeName();
    w["ssid"] = _wifi.ssid();
    w["password"] = _wifi.password();
    w["ap"] = _wifi.apEnabled();
    w["ap_ssid"] = _wifi.apSsid();
    w["ap_password"] = _wifi.apPassword();

    if (_console.adminPasswordSet())
    {
        JsonObject a = _doc["admin"].to<JsonObject>();
        a["password_hash"] = _console.adminPasswordHashHex();
    }

    JsonObject plc = _doc["plc"].to<JsonObject>();
    plc["device_name"] = _plc.deviceName();
    plc["buzzer"] = _plc.buzzerEnabled();

    JsonObject eeprom = _doc["eeprom"].to<JsonObject>();
    eeprom["save"] = _eeprom_save_enabled;
    eeprom["load"] = _eeprom_load_enabled;

    JsonObject ctrl = _doc["controllers"].to<JsonObject>();
    _controllers.serialize(ctrl);

    JsonArray groups = _doc["groups"].to<JsonArray>();
    for (const auto &gcfg : _groups)
    {
        if (gcfg.id == 0 || gcfg.name.length() == 0)
            continue;
        JsonObject g = groups.add<JsonObject>();
        g["id"] = gcfg.id;
        g["name"] = gcfg.name;
        g["sort"] = gcfg.sort;
    }

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
    c["transport"] = (_cloud_transport == CloudTransportKind::Http) ? "http" : "ws";
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
    JsonObject eeprom = tmp["eeprom"].is<JsonObject>() ? tmp["eeprom"].as<JsonObject>() : tmp["eeprom"].to<JsonObject>();
    if (!eeprom["save"].is<bool>())
        eeprom["save"] = _eeprom_save_enabled;
    if (!eeprom["load"].is<bool>())
        eeprom["load"] = _eeprom_load_enabled;
    if (doc["users"].is<JsonArrayConst>())
    {
        _users.applyFromJson(doc["users"].as<JsonArrayConst>());
    }
    if (doc["rules"].is<JsonArrayConst>())
    {
        _rules.applyConfig(doc["rules"].as<JsonArrayConst>());
    }
    tmp.remove("stack");
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
    for (auto &g : _groups)
        g = GroupConfig{};

    if (doc["groups"].is<JsonArrayConst>())
    {
        size_t idx = 0;
        for (JsonVariantConst v : doc["groups"].as<JsonArrayConst>())
        {
            if (idx >= kGroupCount || !v.is<JsonObjectConst>())
                break;
            JsonObjectConst obj = v.as<JsonObjectConst>();
            GroupConfig &g = _groups[idx++];
            if (obj["id"].is<unsigned>())
            {
                const unsigned raw = obj["id"].as<unsigned>();
                if (raw <= 0xFFu)
                    g.id = (uint8_t)raw;
            }
            if (obj["sort"].is<unsigned>())
                g.sort = (uint16_t)obj["sort"].as<unsigned>();
            if (obj["name"].is<const char *>())
                g.name = sanitizeUtf8_(obj["name"].as<const char *>());
            if (g.id == 0 || g.name.length() == 0)
                g = GroupConfig{};
        }
    }

    if (doc["wifi"].is<JsonObjectConst>())
    {
        JsonObjectConst w = doc["wifi"].as<JsonObjectConst>();
        if (w["mode"].is<const char *>())
            _wifi.setModeByName(w["mode"].as<const char *>());
        else if (w["ap"].is<bool>())
            _wifi.setAp(w["ap"].as<bool>());
        if (w["ssid"].is<const char *>())
            _wifi.setSsid(w["ssid"].as<const char *>());
        if (w["password"].is<const char *>())
            _wifi.setPassword(w["password"].as<const char *>());
        if (w["ap_ssid"].is<const char *>())
            _wifi.setApSsid(w["ap_ssid"].as<const char *>());
        if (w["ap_password"].is<const char *>())
            _wifi.setApPassword(w["ap_password"].as<const char *>());
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

    if (doc["eeprom"].is<JsonObjectConst>())
    {
        JsonObjectConst eeprom = doc["eeprom"].as<JsonObjectConst>();
        if (eeprom["save"].is<bool>())
            _eeprom_save_enabled = eeprom["save"].as<bool>();
        if (eeprom["load"].is<bool>())
            _eeprom_load_enabled = eeprom["load"].as<bool>();
    }
    _controllers.setEepromSaveEnabled(_eeprom_save_enabled);
    _controllers.setEepromLoadEnabled(_eeprom_load_enabled);

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
        String transport = c["transport"] | "ws";
        transport.toLowerCase();
        _cloud_transport = (transport == "http") ? CloudTransportKind::Http : CloudTransportKind::WebSocket;
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

void ConfigsManager::clearGroupRefs_(uint8_t group_id){
    if (group_id == 0)
        return;
    for (size_t i = 0; i < SocketController::kSocketCount; ++i)
    {
        auto guard = _controllers.sockets().lockGuard();
        const auto *cfg = _controllers.sockets().configByIndex(i);
        if (cfg && cfg->group_id == group_id)
            _controllers.sockets().setGroupId(cfg->id, 0);
    }
    for (size_t i = 0; i < SocketController::kLightCount; ++i)
    {
        auto guard = _controllers.sockets().lockGuard();
        const auto *cfg = _controllers.sockets().lightConfigByIndex(i);
        if (cfg && cfg->group_id == group_id)
            _controllers.sockets().setLightGroupId(cfg->id, 0);
    }
    for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
    {
        auto guard = _controllers.meteo().lockGuard();
        const auto *cfg = _controllers.meteo().configByIndex(i);
        if (cfg && cfg->group_id == group_id)
            _controllers.meteo().setGroupId(cfg->id, 0);
    }
    for (size_t i = 0; i < ThermoController::kDeviceCount; ++i)
    {
        auto guard = _controllers.thermo().lockGuard();
        const auto *cfg = _controllers.thermo().configByIndex(i);
        if (cfg && cfg->group_id == group_id)
            _controllers.thermo().setGroupId(cfg->id, 0);
    }
    for (size_t i = 0; i < TankController::kTankCount; ++i)
    {
        auto guard = _controllers.tanks().lockGuard();
        const auto *cfg = _controllers.tanks().configByIndex(i);
        if (cfg && cfg->group_id == group_id)
            _controllers.tanks().setGroupId(cfg->id, 0);
    }
    for (size_t i = 0; i < SepticController::kSepticCount; ++i)
    {
        auto guard = _controllers.septic().lockGuard();
        const auto *cfg = _controllers.septic().configByIndex(i);
        if (cfg && cfg->group_id == group_id)
            _controllers.septic().setGroupId(cfg->id, 0);
    }
    for (size_t i = 0; i < SecurityController::kSensorCount; ++i)
    {
        auto guard = _controllers.security().lockGuard();
        const auto *cfg = _controllers.security().configByIndex(i);
        if (cfg && cfg->group_id == group_id)
            _controllers.security().setGroupId(cfg->id, 0);
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
    cfg.transport = _cloud_transport;
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
