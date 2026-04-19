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

#include "core/network/stack/stack_data_binary_codec.hpp"

#include <Arduino.h>
#include <math.h>
#include <string.h>

#include <vector>

#include "core/network/stack/stack_unit_snapshot.hpp"

namespace
{
constexpr uint8_t kSchemaVersion = 1;

enum SecuritySetMask : uint8_t
{
    kSecuritySetArmed = 0x01,
    kSecuritySetForce = 0x02,
    kSecuritySetAlarm = 0x04,
    kSecuritySetClear = 0x08,
    kSecuritySetBeep = 0x10,
};

enum SecurityAlarmMask : uint8_t
{
    kSecurityAlarmActive = 0x01,
    kSecurityAlarmSilent = 0x02,
    kSecurityAlarmHasName = 0x04,
};

enum SepticSetMask : uint8_t
{
    kSepticSetMonitor = 0x01,
    kSepticSetEnabled = 0x02,
};

enum SepticLevelMask : uint8_t
{
    kSepticLevelAlarm = 0x01,
    kSepticLevelHasLevel = 0x02,
    kSepticLevelHasName = 0x04,
};

enum TankEmptyMask : uint8_t
{
    kTankEmptyActive = 0x01,
    kTankEmptyHasName = 0x02,
};

enum WateringEventMask : uint8_t
{
    kWateringHasName = 0x01,
    kWateringHasReason = 0x02,
};

enum WateringSetMask : uint32_t
{
    kWateringSetEnabled = 0x0001,
    kWateringSetName = 0x0002,
    kWateringSetPort = 0x0004,
    kWateringSetStatus = 0x0008,
    kWateringSetWeekdays = 0x0010,
    kWateringSetTank = 0x0020,
    kWateringSetResume = 0x0040,
    kWateringSetResumeLevel = 0x0080,
    kWateringSetSlot1Time = 0x0100,
    kWateringSetSlot1Duration = 0x0200,
    kWateringSetSlot1Enabled = 0x0400,
    kWateringSetSlot2Time = 0x0800,
    kWateringSetSlot2Duration = 0x1000,
    kWateringSetSlot2Enabled = 0x2000,
    kWateringSetSlot3Time = 0x4000,
    kWateringSetSlot3Duration = 0x8000,
    kWateringSetSlot3Enabled = 0x00010000,
    kWateringSetForce = 0x00020000,
};

enum SecurityResultMask : uint8_t
{
    kSecurityResultMatch = 0x01,
    kSecurityResultArmed = 0x02,
};

enum SocketSetMask : uint16_t
{
    kSocketSetEnabled = 0x0001,
    kSocketSetName = 0x0002,
    kSocketSetButton = 0x0004,
    kSocketSetRelay = 0x0008,
    kSocketSetGroup = 0x0010,
    kSocketSetState = 0x0020,
    kSocketSetToggle = 0x0040,
};

enum MeteoSetMask : uint16_t
{
    kMeteoSetEnabled = 0x0001,
    kMeteoSetName = 0x0002,
    kMeteoSetGroup = 0x0004,
    kMeteoSetType = 0x0008,
    kMeteoSetPin = 0x0010,
    kMeteoSetAddr = 0x0020,
    kMeteoSetSource = 0x0040,
};

enum ThermoSetMask : uint16_t
{
    kThermoSetEnabled = 0x0001,
    kThermoSetName = 0x0002,
    kThermoSetGroup = 0x0004,
    kThermoSetSensor = 0x0008,
    kThermoSetMode = 0x0010,
    kThermoSetTarget = 0x0020,
    kThermoSetHyst = 0x0040,
    kThermoSetHeatPort = 0x0080,
    kThermoSetCoolPort = 0x0100,
    kThermoSetButtonPort = 0x0200,
    kThermoSetPower = 0x0400,
    kThermoSetToggle = 0x0800,
};

enum TanksSetMask : uint16_t
{
    kTankSetEnabled = 0x0001,
    kTankSetPower = 0x0002,
    kTankSetToggle = 0x0004,
    kTankSetName = 0x0008,
    kTankSetGroup = 0x0010,
    kTankSetLow = 0x0020,
    kTankSetMid = 0x0040,
    kTankSetFull = 0x0080,
    kTankSetValve = 0x0100,
    kTankSetPump = 0x0200,
    kTankSetAlarm = 0x0400,
};

enum SnapshotItemFlags : uint8_t
{
    kItemEnabled = 0x01,
    kItemState = 0x02,
    kItemBool3 = 0x04,
    kItemBool4 = 0x08,
    kItemBool5 = 0x10,
    kItemBool6 = 0x20,
    kItemBool7 = 0x40,
    kItemBool8 = 0x80,
};

void appendU8_(std::vector<uint8_t> &out, uint8_t v)
{
    out.push_back(v);
}

void appendBool_(std::vector<uint8_t> &out, bool v)
{
    appendU8_(out, v ? 1u : 0u);
}

void appendU16_(std::vector<uint8_t> &out, uint16_t v)
{
    out.push_back((uint8_t)(v & 0xFFu));
    out.push_back((uint8_t)((v >> 8) & 0xFFu));
}

void appendI16_(std::vector<uint8_t> &out, int16_t v)
{
    appendU16_(out, (uint16_t)v);
}

void appendU32_(std::vector<uint8_t> &out, uint32_t v)
{
    out.push_back((uint8_t)(v & 0xFFu));
    out.push_back((uint8_t)((v >> 8) & 0xFFu));
    out.push_back((uint8_t)((v >> 16) & 0xFFu));
    out.push_back((uint8_t)((v >> 24) & 0xFFu));
}

void appendI32_(std::vector<uint8_t> &out, int32_t v)
{
    appendU32_(out, (uint32_t)v);
}

void appendFloat_(std::vector<uint8_t> &out, float v)
{
    uint32_t bits = 0;
    memcpy(&bits, &v, sizeof(bits));
    appendU32_(out, bits);
}

void appendBytes_(std::vector<uint8_t> &out, const uint8_t *data, size_t size)
{
    if (!data || size == 0)
        return;
    out.insert(out.end(), data, data + size);
}

void appendString_(std::vector<uint8_t> &out, const char *text)
{
    const size_t len = text ? strlen(text) : 0u;
    appendU16_(out, (uint16_t)((len > 0xFFFFu) ? 0xFFFFu : len));
    appendBytes_(out, (const uint8_t *)text, (len > 0xFFFFu) ? 0xFFFFu : len);
}

void appendString_(std::vector<uint8_t> &out, const String &text)
{
    appendString_(out, text.c_str());
}

bool readU8_(const uint8_t *&p, size_t &left, uint8_t &out)
{
    if (!p || left < 1u)
        return false;
    out = *p++;
    --left;
    return true;
}

bool readBool_(const uint8_t *&p, size_t &left, bool &out)
{
    uint8_t v = 0;
    if (!readU8_(p, left, v))
        return false;
    out = (v != 0);
    return true;
}

bool readU16_(const uint8_t *&p, size_t &left, uint16_t &out)
{
    if (!p || left < 2u)
        return false;
    out = (uint16_t)p[0] | ((uint16_t)p[1] << 8);
    p += 2;
    left -= 2;
    return true;
}

bool readI16_(const uint8_t *&p, size_t &left, int16_t &out)
{
    uint16_t raw = 0;
    if (!readU16_(p, left, raw))
        return false;
    out = (int16_t)raw;
    return true;
}

bool readU32_(const uint8_t *&p, size_t &left, uint32_t &out)
{
    if (!p || left < 4u)
        return false;
    out = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    p += 4;
    left -= 4;
    return true;
}

bool readI32_(const uint8_t *&p, size_t &left, int32_t &out)
{
    uint32_t raw = 0;
    if (!readU32_(p, left, raw))
        return false;
    out = (int32_t)raw;
    return true;
}

bool readFloat_(const uint8_t *&p, size_t &left, float &out)
{
    uint32_t bits = 0;
    if (!readU32_(p, left, bits))
        return false;
    memcpy(&out, &bits, sizeof(out));
    return true;
}

bool readBytes_(const uint8_t *&p, size_t &left, const uint8_t *&data, size_t size)
{
    if (!p || left < size)
        return false;
    data = p;
    p += size;
    left -= size;
    return true;
}

bool readString_(const uint8_t *&p, size_t &left, String &out)
{
    uint16_t len = 0;
    if (!readU16_(p, left, len))
        return false;
    const uint8_t *data = nullptr;
    if (!readBytes_(p, left, data, len))
        return false;
    out.reserve(len);
    out = "";
    for (uint16_t i = 0; i < len; ++i)
        out += (char)data[i];
    return true;
}

bool readVersion_(const uint8_t *&p, size_t &left)
{
    uint8_t version = 0;
    return readU8_(p, left, version) && version == kSchemaVersion;
}

void addSummaryCounts_(JsonObject summary_obj, const char *name, uint16_t a, const char *a_key, uint16_t b, const char *b_key)
{
    JsonObject obj = summary_obj[name].to<JsonObject>();
    obj[a_key] = a;
    obj[b_key] = b;
}

bool encodePageRequest_(JsonVariantConst value, std::vector<uint8_t> &out)
{
    out.clear();
    appendU8_(out, kSchemaVersion);
    appendU16_(out, (uint16_t)(value["offset"] | 0));
    appendU16_(out, (uint16_t)(value["limit"] | 0));
    return true;
}

bool decodePageRequest_(const uint8_t *data, size_t size, DynamicJsonDocument &out)
{
    const uint8_t *p = data;
    size_t left = size;
    uint16_t offset = 0;
    uint16_t limit = 0;
    if (!readVersion_(p, left) || !readU16_(p, left, offset) || !readU16_(p, left, limit) || left != 0)
        return false;
    out.clear();
    out["offset"] = offset;
    out["limit"] = limit;
    return true;
}

bool encodeSystemSnapshot_(JsonVariantConst value, std::vector<uint8_t> &out)
{
    out.clear();
    appendU8_(out, kSchemaVersion);
    appendString_(out, value["device_name"] | "");
    appendString_(out, value["rtc_date"] | "");
    appendString_(out, value["rtc_time"] | "");
    appendBool_(out, value["rtc_temp_ok"] | false);
    appendFloat_(out, value["rtc_temp"].is<float>() ? value["rtc_temp"].as<float>() : (float)(value["rtc_temp"] | 0.0));
    appendFloat_(out, value["board_temp"].is<float>() ? value["board_temp"].as<float>() : (float)(value["board_temp"] | 0.0));
    appendBool_(out, value["fan_on"] | false);
    JsonVariantConst wifi = value["wifi"];
    appendString_(out, wifi["mode"] | "");
    appendString_(out, wifi["ssid"] | "");
    appendString_(out, wifi["ap_ssid"] | "");
    appendString_(out, wifi["ip"] | "");
    appendString_(out, wifi["mac"] | "");
    JsonVariantConst gsm = value["gsm"];
    appendBool_(out, gsm["enabled"] | false);
    appendBool_(out, gsm["started"] | false);
    appendString_(out, gsm["imei"] | "");
    appendString_(out, gsm["imsi"] | "");
    appendString_(out, gsm["operator"] | "");
    appendString_(out, gsm["signal"] | "");
    appendString_(out, gsm["reg_status"] | "");
    appendString_(out, gsm["last_error"] | "");
    appendString_(out, gsm["last_urc"] | "");
    appendString_(out, gsm["last_call"] | "");
    appendString_(out, gsm["last_ussd"] | "");
    appendI32_(out, gsm["last_http_status"].is<int>() ? gsm["last_http_status"].as<int>() : (int32_t)(gsm["last_http_status"] | -1));
    appendI32_(out, gsm["last_http_len"].is<int>() ? gsm["last_http_len"].as<int>() : (int32_t)(gsm["last_http_len"] | -1));
    return true;
}

bool decodeSystemSnapshot_(const uint8_t *data, size_t size, DynamicJsonDocument &out)
{
    auto fill_common = [&](const String &device_name, const String &rtc_date, const String &rtc_time, bool rtc_temp_ok,
                           float rtc_temp, float board_temp, bool fan_on) {
        out.clear();
        out["device_name"] = device_name;
        out["rtc_date"] = rtc_date;
        out["rtc_time"] = rtc_time;
        out["rtc_temp_ok"] = rtc_temp_ok;
        out["rtc_temp"] = rtc_temp;
        out["board_temp"] = board_temp;
        out["fan_on"] = fan_on;
    };

    {
        const uint8_t *p = data;
        size_t left = size;
        String device_name;
        String rtc_date;
        String rtc_time;
        bool rtc_temp_ok = false;
        float rtc_temp = 0.0f;
        float board_temp = 0.0f;
        bool fan_on = false;
        String wifi_mode;
        String wifi_ssid;
        String wifi_ap_ssid;
        String wifi_ip;
        String wifi_mac;
        bool gsm_enabled = false;
        bool gsm_started = false;
        String gsm_imei;
        String gsm_imsi;
        String gsm_operator;
        String gsm_signal;
        String gsm_reg_status;
        String gsm_last_error;
        String gsm_last_urc;
        String gsm_last_call;
        String gsm_last_ussd;
        int32_t gsm_last_http_status = -1;
        int32_t gsm_last_http_len = -1;
        if (readVersion_(p, left) && readString_(p, left, device_name) && readString_(p, left, rtc_date) &&
            readString_(p, left, rtc_time) && readBool_(p, left, rtc_temp_ok) && readFloat_(p, left, rtc_temp) &&
            readFloat_(p, left, board_temp) && readBool_(p, left, fan_on) &&
            readString_(p, left, wifi_mode) && readString_(p, left, wifi_ssid) &&
            readString_(p, left, wifi_ap_ssid) && readString_(p, left, wifi_ip) &&
            readString_(p, left, wifi_mac) && readBool_(p, left, gsm_enabled) &&
            readBool_(p, left, gsm_started) && readString_(p, left, gsm_imei) &&
            readString_(p, left, gsm_imsi) && readString_(p, left, gsm_operator) &&
            readString_(p, left, gsm_signal) && readString_(p, left, gsm_reg_status) &&
            readString_(p, left, gsm_last_error) && readString_(p, left, gsm_last_urc) &&
            readString_(p, left, gsm_last_call) && readString_(p, left, gsm_last_ussd) &&
            readI32_(p, left, gsm_last_http_status) && readI32_(p, left, gsm_last_http_len) && left == 0)
        {
            fill_common(device_name, rtc_date, rtc_time, rtc_temp_ok, rtc_temp, board_temp, fan_on);
            JsonObject wifi = out["wifi"].to<JsonObject>();
            wifi["mode"] = wifi_mode;
            wifi["ssid"] = wifi_ssid;
            wifi["ap_ssid"] = wifi_ap_ssid;
            wifi["ip"] = wifi_ip;
            wifi["mac"] = wifi_mac;
            JsonObject gsm = out["gsm"].to<JsonObject>();
            gsm["enabled"] = gsm_enabled;
            gsm["started"] = gsm_started;
            gsm["imei"] = gsm_imei;
            gsm["imsi"] = gsm_imsi;
            gsm["operator"] = gsm_operator;
            gsm["signal"] = gsm_signal;
            gsm["reg_status"] = gsm_reg_status;
            gsm["last_error"] = gsm_last_error;
            gsm["last_urc"] = gsm_last_urc;
            gsm["last_call"] = gsm_last_call;
            gsm["last_ussd"] = gsm_last_ussd;
            if (gsm_last_http_status >= 0)
                gsm["last_http_status"] = gsm_last_http_status;
            if (gsm_last_http_len >= 0)
                gsm["last_http_len"] = gsm_last_http_len;
            return true;
        }
    }

    {
        const uint8_t *p = data;
        size_t left = size;
        String device_name;
        String rtc_date;
        String rtc_time;
        bool rtc_temp_ok = false;
        float rtc_temp = 0.0f;
        float board_temp = 0.0f;
        bool fan_on = false;
        if (!readVersion_(p, left) || !readString_(p, left, device_name) || !readString_(p, left, rtc_date) ||
            !readString_(p, left, rtc_time) || !readBool_(p, left, rtc_temp_ok) || !readFloat_(p, left, rtc_temp) ||
            !readFloat_(p, left, board_temp) || !readBool_(p, left, fan_on) || left != 0)
            return false;
        fill_common(device_name, rtc_date, rtc_time, rtc_temp_ok, rtc_temp, board_temp, fan_on);
        return true;
    }
}

bool encodeControllersSummary_(JsonVariantConst value, std::vector<uint8_t> &out)
{
    JsonVariantConst summary = value["summary"];
    JsonVariantConst security = summary["security"];
    JsonArrayConst detected_items = security["detected_items"].as<JsonArrayConst>();
    out.clear();
    appendU8_(out, kSchemaVersion);
    appendU16_(out, (uint16_t)(summary["sockets"]["enabled"] | 0));
    appendU16_(out, (uint16_t)(summary["sockets"]["on"] | 0));
    appendU16_(out, (uint16_t)(summary["lights"]["enabled"] | 0));
    appendU16_(out, (uint16_t)(summary["lights"]["on"] | 0));
    appendU16_(out, (uint16_t)(summary["meteo"]["enabled"] | 0));
    appendU16_(out, (uint16_t)(summary["meteo"]["ok"] | 0));
    appendU16_(out, (uint16_t)(summary["thermo"]["enabled"] | 0));
    appendU16_(out, (uint16_t)(summary["thermo"]["active"] | 0));
    appendU16_(out, (uint16_t)(summary["tanks"]["enabled"] | 0));
    appendU16_(out, (uint16_t)(summary["tanks"]["alert"] | 0));
    appendU16_(out, (uint16_t)(summary["septic"]["enabled"] | 0));
    appendU16_(out, (uint16_t)(summary["septic"]["warning"] | 0));
    appendU16_(out, (uint16_t)(summary["septic"]["alert"] | 0));
    appendBool_(out, summary["septic"]["monitor"] | false);
    appendBool_(out, summary["septic"]["relay_warning_on"] | false);
    appendBool_(out, summary["septic"]["relay_alarm_on"] | false);
    appendU8_(out, (uint8_t)(summary["septic"]["group_id"] | 0));
    appendU8_(out, (uint8_t)(summary["septic"]["warning_port"] | 0xFF));
    appendU8_(out, (uint8_t)(summary["septic"]["alarm_port"] | 0xFF));
    appendU8_(out, (uint8_t)(summary["septic"]["relay_warning_port"] | 0xFF));
    appendU8_(out, (uint8_t)(summary["septic"]["relay_alarm_port"] | 0xFF));
    appendString_(out, summary["septic"]["name"] | "");
    appendU16_(out, (uint16_t)(summary["watering"]["enabled"] | 0));
    appendU16_(out, (uint16_t)(summary["watering"]["active"] | 0));
    JsonVariantConst rules = summary["rules"];
    JsonArrayConst rule_items = rules["items"].as<JsonArrayConst>();
    appendU16_(out, (uint16_t)(rules["enabled"] | 0));
    const uint8_t rule_count = rule_items.isNull() ? 0u : (uint8_t)min((size_t)255u, rule_items.size());
    appendU8_(out, rule_count);
    if (!rule_items.isNull())
    {
        for (JsonObjectConst item : rule_items)
        {
            appendU8_(out, (uint8_t)(item["id"] | 0));
            appendBool_(out, item["enabled"] | false);
            appendString_(out, item["name"] | "");
        }
    }
    appendBool_(out, security["enabled"] | false);
    appendU16_(out, (uint16_t)(security["sensors_enabled"] | 0));
    appendU16_(out, (uint16_t)(security["detected"] | 0));
    uint8_t preview_count = 0;
    if (!detected_items.isNull())
        preview_count = (uint8_t)min((size_t)255u, detected_items.size());
    appendU8_(out, preview_count);
    if (!detected_items.isNull())
    {
        for (JsonObjectConst item : detected_items)
        {
            appendU8_(out, (uint8_t)(item["id"] | 0));
            appendString_(out, item["name"] | "");
        }
    }
    appendBool_(out, security["armed"] | false);
    appendBool_(out, security["alarm"] | false);
    appendBool_(out, summary["ring"]["enabled"] | false);
    appendBool_(out, summary["ring"]["on"] | false);
    appendBool_(out, summary["avr"]["enabled"] | false);
    appendBool_(out, summary["avr"]["main_ok"] | false);
    appendBool_(out, summary["avr"]["reserve_ok"] | false);
    appendBool_(out, summary["avr"]["fault"] | false);
    appendU8_(out, (uint8_t)(summary["avr"]["active_source_id"] | 0));
    appendU16_(out, (uint16_t)(summary["leak"]["enabled"] | 0));
    appendU16_(out, (uint16_t)(summary["leak"]["alert"] | 0));
    return true;
}

bool decodeControllersSummary_(const uint8_t *data, size_t size, DynamicJsonDocument &out)
{
    const uint8_t *p = data;
    size_t left = size;
    if (!readVersion_(p, left))
        return false;
    out.clear();
    JsonObject summary = out["summary"].to<JsonObject>();
    uint16_t a = 0;
    uint16_t b = 0;
    if (!readU16_(p, left, a) || !readU16_(p, left, b))
        return false;
    addSummaryCounts_(summary, "sockets", a, "enabled", b, "on");
    if (!readU16_(p, left, a) || !readU16_(p, left, b))
        return false;
    addSummaryCounts_(summary, "lights", a, "enabled", b, "on");
    if (!readU16_(p, left, a) || !readU16_(p, left, b))
        return false;
    addSummaryCounts_(summary, "meteo", a, "enabled", b, "ok");
    if (!readU16_(p, left, a) || !readU16_(p, left, b))
        return false;
    addSummaryCounts_(summary, "thermo", a, "enabled", b, "active");
    if (!readU16_(p, left, a) || !readU16_(p, left, b))
        return false;
    addSummaryCounts_(summary, "tanks", a, "enabled", b, "alert");
    JsonObject septic = summary["septic"].to<JsonObject>();
    if (!readU16_(p, left, a) || !readU16_(p, left, b))
        return false;
    septic["enabled"] = a;
    septic["warning"] = b;
    if (!readU16_(p, left, a))
        return false;
    septic["alert"] = a;
    bool boolv = false;
    uint8_t u8 = 0;
    String text;
    if (!readBool_(p, left, boolv))
        return false;
    septic["monitor"] = boolv;
    if (!readBool_(p, left, boolv))
        return false;
    septic["relay_warning_on"] = boolv;
    if (!readBool_(p, left, boolv))
        return false;
    septic["relay_alarm_on"] = boolv;
    if (!readU8_(p, left, u8))
        return false;
    septic["group_id"] = u8;
    if (!readU8_(p, left, u8))
        return false;
    septic["warning_port"] = u8;
    if (!readU8_(p, left, u8))
        return false;
    septic["alarm_port"] = u8;
    if (!readU8_(p, left, u8))
        return false;
    septic["relay_warning_port"] = u8;
    if (!readU8_(p, left, u8))
        return false;
    septic["relay_alarm_port"] = u8;
    if (!readString_(p, left, text))
        return false;
    septic["name"] = text;

    if (!readU16_(p, left, a) || !readU16_(p, left, b))
        return false;
    addSummaryCounts_(summary, "watering", a, "enabled", b, "active");

    JsonObject rules = summary["rules"].to<JsonObject>();
    uint16_t rules_enabled = 0;
    uint8_t rules_count = 0;
    if (!readU16_(p, left, rules_enabled) || !readU8_(p, left, rules_count) ||
        rules_count > StackUnitSnapshot::kRuleCount)
        return false;
    rules["enabled"] = rules_enabled;
    JsonArray rule_items = rules["items"].to<JsonArray>();
    for (uint8_t i = 0; i < rules_count; ++i)
    {
        uint8_t id = 0;
        bool enabled = false;
        if (!readU8_(p, left, id) || !readBool_(p, left, enabled) || !readString_(p, left, text))
            return false;
        JsonObject item = rule_items.add<JsonObject>();
        item["id"] = id;
        item["enabled"] = enabled;
        item["name"] = text;
    }

    JsonObject security = summary["security"].to<JsonObject>();
    if (!readBool_(p, left, boolv))
        return false;
    security["enabled"] = boolv;
    if (!readU16_(p, left, a) || !readU16_(p, left, b))
        return false;
    security["sensors_enabled"] = a;
    security["detected"] = b;
    JsonArray detected_items = security["detected_items"].to<JsonArray>();
    if (!readU8_(p, left, u8))
        return false;
    for (uint8_t i = 0; i < u8; ++i)
    {
        uint8_t id = 0;
        if (!readU8_(p, left, id) || !readString_(p, left, text))
            return false;
        JsonObject item = detected_items.add<JsonObject>();
        item["id"] = id;
        item["name"] = text;
    }
    if (!readBool_(p, left, boolv))
        return false;
    security["armed"] = boolv;
    if (!readBool_(p, left, boolv))
        return false;
    security["alarm"] = boolv;

    JsonObject ring = summary["ring"].to<JsonObject>();
    if (!readBool_(p, left, boolv))
        return false;
    ring["enabled"] = boolv;
    if (!readBool_(p, left, boolv))
        return false;
    ring["on"] = boolv;

    JsonObject avr = summary["avr"].to<JsonObject>();
    if (!readBool_(p, left, boolv))
        return false;
    avr["enabled"] = boolv;
    if (!readBool_(p, left, boolv))
        return false;
    avr["main_ok"] = boolv;
    if (!readBool_(p, left, boolv))
        return false;
    avr["reserve_ok"] = boolv;
    if (!readBool_(p, left, boolv))
        return false;
    avr["fault"] = boolv;
    if (!readU8_(p, left, u8))
        return false;
    avr["active_source_id"] = u8;

    if (!readU16_(p, left, a) || !readU16_(p, left, b) || left != 0)
        return false;
    addSummaryCounts_(summary, "leak", a, "enabled", b, "alert");
    return true;
}

bool encodeSocketLikeSnapshot_(JsonVariantConst value, const char *summary_name, const char *items_name, std::vector<uint8_t> &out)
{
    JsonVariantConst summary = value["summary"][summary_name];
    JsonArrayConst items = value["controllers"][items_name].as<JsonArrayConst>();
    out.clear();
    appendU8_(out, kSchemaVersion);
    appendU16_(out, (uint16_t)(value["offset"] | 0));
    appendU16_(out, (uint16_t)(value["limit"] | 0));
    appendU16_(out, (uint16_t)(value["total"] | 0));
    appendU16_(out, (uint16_t)(summary["enabled"] | 0));
    appendU16_(out, (uint16_t)(summary["on"] | 0));
    const uint8_t count = items.isNull() ? 0u : (uint8_t)min((size_t)255u, items.size());
    appendU8_(out, count);
    if (!items.isNull())
    {
        for (JsonObjectConst item : items)
        {
            appendU8_(out, (uint8_t)(item["id"] | 0));
            uint8_t flags = 0;
            if (item["enabled"] | false)
                flags |= kItemEnabled;
            if (item["state"] | false)
                flags |= kItemState;
            appendU8_(out, flags);
            appendU8_(out, (uint8_t)(item["button"] | 0xFF));
            appendU8_(out, (uint8_t)(item["relay"] | 0xFF));
            appendU8_(out, (uint8_t)(item["group_id"] | 0));
            appendString_(out, item["name"] | "");
        }
    }
    return true;
}

bool decodeSocketLikeSnapshot_(const uint8_t *data, size_t size, const char *summary_name, const char *items_name, DynamicJsonDocument &out)
{
    const uint8_t *p = data;
    size_t left = size;
    uint16_t offset = 0;
    uint16_t limit = 0;
    uint16_t total = 0;
    uint16_t enabled = 0;
    uint16_t on = 0;
    uint8_t count = 0;
    if (!readVersion_(p, left) || !readU16_(p, left, offset) || !readU16_(p, left, limit) || !readU16_(p, left, total) ||
        !readU16_(p, left, enabled) || !readU16_(p, left, on) || !readU8_(p, left, count))
        return false;
    out.clear();
    out["offset"] = offset;
    out["limit"] = limit;
    out["total"] = total;
    JsonObject summary = out["summary"][summary_name].to<JsonObject>();
    summary["enabled"] = enabled;
    summary["on"] = on;
    JsonArray items = out["controllers"][items_name].to<JsonArray>();
    for (uint8_t i = 0; i < count; ++i)
    {
        uint8_t id = 0;
        uint8_t flags = 0;
        uint8_t button = 0;
        uint8_t relay = 0;
        uint8_t group_id = 0;
        String name;
        if (!readU8_(p, left, id) || !readU8_(p, left, flags) || !readU8_(p, left, button) || !readU8_(p, left, relay) ||
            !readU8_(p, left, group_id) || !readString_(p, left, name))
            return false;
        JsonObject item = items.add<JsonObject>();
        item["id"] = id;
        item["enabled"] = (flags & kItemEnabled) != 0;
        item["state"] = (flags & kItemState) != 0;
        item["button"] = button;
        item["relay"] = relay;
        item["group_id"] = group_id;
        item["name"] = name;
    }
    return left == 0;
}

bool encodeMeteoSnapshot_(JsonVariantConst value, std::vector<uint8_t> &out)
{
    JsonVariantConst summary = value["summary"]["meteo"];
    JsonArrayConst items = value["controllers"]["meteo"].as<JsonArrayConst>();
    out.clear();
    appendU8_(out, kSchemaVersion);
    appendU16_(out, (uint16_t)(value["offset"] | 0));
    appendU16_(out, (uint16_t)(value["limit"] | 0));
    appendU16_(out, (uint16_t)(value["total"] | 0));
    appendU16_(out, (uint16_t)(summary["enabled"] | 0));
    appendU16_(out, (uint16_t)(summary["ok"] | 0));
    const uint8_t count = items.isNull() ? 0u : (uint8_t)min((size_t)255u, items.size());
    appendU8_(out, count);
    if (!items.isNull())
    {
        for (JsonObjectConst item : items)
        {
            appendU8_(out, (uint8_t)(item["id"] | 0));
            uint8_t flags = 0;
            if (item["enabled"] | false)
                flags |= kItemEnabled;
            if (item["has_read"] | false)
                flags |= kItemState;
            if (item["ok"] | false)
                flags |= kItemBool3;
            if (item["has_temp"] | false)
                flags |= kItemBool4;
            if (item["has_hum"] | false)
                flags |= kItemBool5;
            if (item["addr_set"] | false)
                flags |= kItemBool6;
            appendU8_(out, flags);
            appendU8_(out, (uint8_t)(item["group_id"] | 0));
            appendU8_(out, (uint8_t)(item["type_id"] | 0));
            appendU8_(out, (uint8_t)(item["pin"] | 0xFF));
            const char *addr = item["addr"] | "";
            appendString_(out, addr);
            appendU32_(out, (uint32_t)(item["src_node"] | 0u));
            appendU8_(out, (uint8_t)(item["src_sensor"] | 0));
            appendFloat_(out, item["temp_c"].is<float>() ? item["temp_c"].as<float>() : (float)(item["temp_c"] | 0.0));
            appendFloat_(out, item["hum"].is<float>() ? item["hum"].as<float>() : (float)(item["hum"] | 0.0));
            appendU16_(out, (uint16_t)(item["age_s"] | 0));
            appendString_(out, item["name"] | "");
        }
    }
    return true;
}

bool decodeMeteoSnapshot_(const uint8_t *data, size_t size, DynamicJsonDocument &out)
{
    const uint8_t *p = data;
    size_t left = size;
    uint16_t offset = 0, limit = 0, total = 0, enabled = 0, ok = 0;
    uint8_t count = 0;
    if (!readVersion_(p, left) || !readU16_(p, left, offset) || !readU16_(p, left, limit) || !readU16_(p, left, total) ||
        !readU16_(p, left, enabled) || !readU16_(p, left, ok) || !readU8_(p, left, count))
        return false;
    out.clear();
    out["offset"] = offset;
    out["limit"] = limit;
    out["total"] = total;
    JsonObject summary = out["summary"]["meteo"].to<JsonObject>();
    summary["enabled"] = enabled;
    summary["ok"] = ok;
    JsonArray items = out["controllers"]["meteo"].to<JsonArray>();
    for (uint8_t i = 0; i < count; ++i)
    {
        uint8_t id = 0, flags = 0, group_id = 0, type_id = 0, pin = 0, src_sensor = 0;
        uint32_t src_node = 0;
        uint16_t age_s = 0;
        float temp_c = 0.0f;
        float hum = 0.0f;
        String addr;
        String name;
        if (!readU8_(p, left, id) || !readU8_(p, left, flags) || !readU8_(p, left, group_id) || !readU8_(p, left, type_id) ||
            !readU8_(p, left, pin) || !readString_(p, left, addr) || !readU32_(p, left, src_node) ||
            !readU8_(p, left, src_sensor) || !readFloat_(p, left, temp_c) || !readFloat_(p, left, hum) ||
            !readU16_(p, left, age_s) || !readString_(p, left, name))
            return false;
        JsonObject item = items.add<JsonObject>();
        item["id"] = id;
        item["enabled"] = (flags & kItemEnabled) != 0;
        item["has_read"] = (flags & kItemState) != 0;
        item["ok"] = (flags & kItemBool3) != 0;
        item["has_temp"] = (flags & kItemBool4) != 0;
        item["has_hum"] = (flags & kItemBool5) != 0;
        item["addr_set"] = (flags & kItemBool6) != 0;
        item["group_id"] = group_id;
        item["type_id"] = type_id;
        item["pin"] = pin;
        item["addr"] = addr;
        item["src_node"] = src_node;
        item["src_sensor"] = src_sensor;
        item["temp_c"] = temp_c;
        item["hum"] = hum;
        item["age_s"] = age_s;
        item["name"] = name;
    }
    return left == 0;
}

bool encodeThermoSnapshot_(JsonVariantConst value, std::vector<uint8_t> &out)
{
    JsonVariantConst summary = value["summary"]["thermo"];
    JsonArrayConst items = value["controllers"]["thermo"].as<JsonArrayConst>();
    out.clear();
    appendU8_(out, kSchemaVersion);
    appendU16_(out, (uint16_t)(value["offset"] | 0));
    appendU16_(out, (uint16_t)(value["limit"] | 0));
    appendU16_(out, (uint16_t)(value["total"] | 0));
    appendU16_(out, (uint16_t)(summary["enabled"] | 0));
    appendU16_(out, (uint16_t)(summary["active"] | 0));
    const uint8_t count = items.isNull() ? 0u : (uint8_t)min((size_t)255u, items.size());
    appendU8_(out, count);
    if (!items.isNull())
    {
        for (JsonObjectConst item : items)
        {
            appendU8_(out, (uint8_t)(item["id"] | 0));
            uint8_t flags = 0;
            if (item["enabled"] | false)
                flags |= kItemEnabled;
            if (item["power_on"] | false)
                flags |= kItemState;
            if (item["heat_on"] | false)
                flags |= kItemBool3;
            if (item["cool_on"] | false)
                flags |= kItemBool4;
            appendU8_(out, flags);
            appendU8_(out, (uint8_t)(item["group_id"] | 0));
            appendU8_(out, (uint8_t)(item["sensor_id"] | 0));
            appendU32_(out, (uint32_t)(item["sensor_node_id"] | 0u));
            appendU8_(out, (uint8_t)(item["heat_port"] | 0xFF));
            appendU8_(out, (uint8_t)(item["cool_port"] | 0xFF));
            appendU8_(out, (uint8_t)(item["button_port"] | 0xFF));
            appendU8_(out, (uint8_t)(item["mode_id"] | 0));
            appendI16_(out, (int16_t)(item["target_c"] | 0));
            appendFloat_(out, item["hyst"].is<float>() ? item["hyst"].as<float>() : (float)(item["hyst"] | 0.0));
            appendString_(out, item["name"] | "");
        }
    }
    return true;
}

bool decodeThermoSnapshot_(const uint8_t *data, size_t size, DynamicJsonDocument &out)
{
    const uint8_t *p = data;
    size_t left = size;
    uint16_t offset = 0, limit = 0, total = 0, enabled = 0, active = 0;
    uint8_t count = 0;
    if (!readVersion_(p, left) || !readU16_(p, left, offset) || !readU16_(p, left, limit) || !readU16_(p, left, total) ||
        !readU16_(p, left, enabled) || !readU16_(p, left, active) || !readU8_(p, left, count))
        return false;
    out.clear();
    out["offset"] = offset;
    out["limit"] = limit;
    out["total"] = total;
    JsonObject summary = out["summary"]["thermo"].to<JsonObject>();
    summary["enabled"] = enabled;
    summary["active"] = active;
    JsonArray items = out["controllers"]["thermo"].to<JsonArray>();
    for (uint8_t i = 0; i < count; ++i)
    {
        uint8_t id = 0, flags = 0, group_id = 0, sensor_id = 0, heat_port = 0, cool_port = 0, button_port = 0, mode_id = 0;
        uint32_t sensor_node_id = 0;
        int16_t target_c = 0;
        float hyst = 0.0f;
        String name;
        const uint8_t *item_p0 = p;
        const size_t item_left0 = left;
        if (!readU8_(p, left, id) || !readU8_(p, left, flags) || !readU8_(p, left, group_id) || !readU8_(p, left, sensor_id) ||
            !readU32_(p, left, sensor_node_id) || !readU8_(p, left, heat_port) || !readU8_(p, left, cool_port) ||
            !readU8_(p, left, button_port) || !readU8_(p, left, mode_id))
            return false;
        if (!readI16_(p, left, target_c) || !readFloat_(p, left, hyst) || !readString_(p, left, name))
        {
            p = item_p0;
            left = item_left0;
            return false;
        }
        JsonObject item = items.add<JsonObject>();
        item["id"] = id;
        item["enabled"] = (flags & kItemEnabled) != 0;
        item["power_on"] = (flags & kItemState) != 0;
        item["heat_on"] = (flags & kItemBool3) != 0;
        item["cool_on"] = (flags & kItemBool4) != 0;
        item["group_id"] = group_id;
        item["sensor_id"] = sensor_id;
        item["sensor_node_id"] = sensor_node_id;
        item["heat_port"] = heat_port;
        item["cool_port"] = cool_port;
        item["button_port"] = button_port;
        item["mode_id"] = mode_id;
        item["target_c"] = target_c;
        item["hyst"] = hyst;
        item["name"] = name;
    }
    return left == 0;
}

bool encodeTankSnapshot_(JsonVariantConst value, std::vector<uint8_t> &out)
{
    JsonVariantConst summary = value["summary"]["tanks"];
    JsonArrayConst items = value["controllers"]["tanks"].as<JsonArrayConst>();
    out.clear();
    appendU8_(out, kSchemaVersion);
    appendU16_(out, (uint16_t)(value["offset"] | 0));
    appendU16_(out, (uint16_t)(value["limit"] | 0));
    appendU16_(out, (uint16_t)(value["total"] | 0));
    appendU16_(out, (uint16_t)(summary["enabled"] | 0));
    appendU16_(out, (uint16_t)(summary["alert"] | 0));
    const uint8_t count = items.isNull() ? 0u : (uint8_t)min((size_t)255u, items.size());
    appendU8_(out, count);
    if (!items.isNull())
    {
        for (JsonObjectConst item : items)
        {
            appendU8_(out, (uint8_t)(item["id"] | 0));
            uint8_t flags1 = 0;
            if (item["enabled"] | false)
                flags1 |= kItemEnabled;
            if (item["power_on"] | false)
                flags1 |= kItemState;
            if (item["level_low"] | false)
                flags1 |= kItemBool3;
            if (item["level_mid"] | false)
                flags1 |= kItemBool4;
            if (item["level_full"] | false)
                flags1 |= kItemBool5;
            if (item["levels_ok"] | false)
                flags1 |= kItemBool6;
            if (item["valve_on"] | false)
                flags1 |= kItemBool7;
            if (item["pump_on"] | false)
                flags1 |= kItemBool8;
            appendU8_(out, flags1);
            appendU8_(out, (uint8_t)(item["group_id"] | 0));
            appendU8_(out, (uint8_t)(item["low"] | 0xFF));
            appendU8_(out, (uint8_t)(item["mid"] | 0xFF));
            appendU8_(out, (uint8_t)(item["full"] | 0xFF));
            appendU8_(out, (uint8_t)(item["valve"] | 0xFF));
            appendU8_(out, (uint8_t)(item["pump"] | 0xFF));
            appendU8_(out, (uint8_t)(item["alarm"] | 0xFF));
            appendBool_(out, item["alarm_on"] | false);
            appendString_(out, item["name"] | "");
        }
    }
    return true;
}

bool decodeTankSnapshot_(const uint8_t *data, size_t size, DynamicJsonDocument &out)
{
    const uint8_t *p = data;
    size_t left = size;
    uint16_t offset = 0, limit = 0, total = 0, enabled = 0, alert = 0;
    uint8_t count = 0;
    if (!readVersion_(p, left) || !readU16_(p, left, offset) || !readU16_(p, left, limit) || !readU16_(p, left, total) ||
        !readU16_(p, left, enabled) || !readU16_(p, left, alert) || !readU8_(p, left, count))
        return false;
    out.clear();
    out["offset"] = offset;
    out["limit"] = limit;
    out["total"] = total;
    JsonObject summary = out["summary"]["tanks"].to<JsonObject>();
    summary["enabled"] = enabled;
    summary["alert"] = alert;
    JsonArray items = out["controllers"]["tanks"].to<JsonArray>();
    for (uint8_t i = 0; i < count; ++i)
    {
        uint8_t id = 0, flags = 0, group_id = 0, low = 0, mid = 0, full = 0, valve = 0, pump = 0, alarm_port = 0;
        bool alarm_on = false;
        String name;
        if (!readU8_(p, left, id) || !readU8_(p, left, flags) || !readU8_(p, left, group_id) || !readU8_(p, left, low) ||
            !readU8_(p, left, mid) || !readU8_(p, left, full) || !readU8_(p, left, valve) || !readU8_(p, left, pump) ||
            !readU8_(p, left, alarm_port) || !readBool_(p, left, alarm_on) || !readString_(p, left, name))
            return false;
        JsonObject item = items.add<JsonObject>();
        item["id"] = id;
        item["enabled"] = (flags & kItemEnabled) != 0;
        item["power_on"] = (flags & kItemState) != 0;
        item["level_low"] = (flags & kItemBool3) != 0;
        item["level_mid"] = (flags & kItemBool4) != 0;
        item["level_full"] = (flags & kItemBool5) != 0;
        item["levels_ok"] = (flags & kItemBool6) != 0;
        item["valve_on"] = (flags & kItemBool7) != 0;
        item["pump_on"] = (flags & kItemBool8) != 0;
        item["group_id"] = group_id;
        item["low"] = low;
        item["mid"] = mid;
        item["full"] = full;
        item["valve"] = valve;
        item["pump"] = pump;
        item["alarm"] = alarm_port;
        item["alarm_on"] = alarm_on;
        item["name"] = name;
    }
    return left == 0;
}

bool encodeLeakSnapshot_(JsonVariantConst value, std::vector<uint8_t> &out)
{
    JsonVariantConst summary = value["summary"]["leak"];
    JsonArrayConst items = value["controllers"]["leak"].as<JsonArrayConst>();
    out.clear();
    appendU8_(out, kSchemaVersion);
    appendU16_(out, (uint16_t)(value["offset"] | 0));
    appendU16_(out, (uint16_t)(value["limit"] | 0));
    appendU16_(out, (uint16_t)(value["total"] | 0));
    appendU16_(out, (uint16_t)(summary["enabled"] | 0));
    appendU16_(out, (uint16_t)(summary["alert"] | 0));
    const uint8_t count = items.isNull() ? 0u : (uint8_t)min((size_t)255u, items.size());
    appendU8_(out, count);
    if (!items.isNull())
    {
        for (JsonObjectConst item : items)
        {
            appendU8_(out, (uint8_t)(item["id"] | 0));
            uint8_t flags = 0;
            if (item["enabled"] | false)
                flags |= kItemEnabled;
            if (item["power_on"] | false)
                flags |= kItemState;
            if (item["sensor_active_low"] | false)
                flags |= kItemBool3;
            if (item["wet"] | false)
                flags |= kItemBool4;
            if (item["alarm_latched"] | false)
                flags |= kItemBool5;
            if (item["valve_closed"] | false)
                flags |= kItemBool6;
            if (item["alarm_on"] | false)
                flags |= kItemBool7;
            appendU8_(out, flags);
            appendU8_(out, (uint8_t)(item["sensor"] | 0xFF));
            appendU8_(out, (uint8_t)(item["valve"] | 0xFF));
            appendU8_(out, (uint8_t)(item["alarm"] | 0xFF));
            appendString_(out, item["name"] | "");
        }
    }
    return true;
}

bool decodeLeakSnapshot_(const uint8_t *data, size_t size, DynamicJsonDocument &out)
{
    const uint8_t *p = data;
    size_t left = size;
    uint16_t offset = 0, limit = 0, total = 0, enabled = 0, alert = 0;
    uint8_t count = 0;
    if (!readVersion_(p, left) || !readU16_(p, left, offset) || !readU16_(p, left, limit) || !readU16_(p, left, total) ||
        !readU16_(p, left, enabled) || !readU16_(p, left, alert) || !readU8_(p, left, count))
        return false;
    out.clear();
    out["offset"] = offset;
    out["limit"] = limit;
    out["total"] = total;
    JsonObject summary = out["summary"]["leak"].to<JsonObject>();
    summary["enabled"] = enabled;
    summary["alert"] = alert;
    JsonArray items = out["controllers"]["leak"].to<JsonArray>();
    for (uint8_t i = 0; i < count; ++i)
    {
        uint8_t id = 0, flags = 0, sensor = 0, valve = 0, alarm_port = 0;
        String name;
        if (!readU8_(p, left, id) || !readU8_(p, left, flags) || !readU8_(p, left, sensor) || !readU8_(p, left, valve) ||
            !readU8_(p, left, alarm_port) || !readString_(p, left, name))
            return false;
        JsonObject item = items.add<JsonObject>();
        item["id"] = id;
        item["enabled"] = (flags & kItemEnabled) != 0;
        item["power_on"] = (flags & kItemState) != 0;
        item["sensor_active_low"] = (flags & kItemBool3) != 0;
        item["wet"] = (flags & kItemBool4) != 0;
        item["alarm_latched"] = (flags & kItemBool5) != 0;
        item["valve_closed"] = (flags & kItemBool6) != 0;
        item["alarm_on"] = (flags & kItemBool7) != 0;
        item["sensor"] = sensor;
        item["valve"] = valve;
        item["alarm"] = alarm_port;
        item["name"] = name;
    }
    return left == 0;
}

bool encodeSocketSet_(JsonVariantConst value, std::vector<uint8_t> &out)
{
    JsonArrayConst items = value["items"].as<JsonArrayConst>();
    out.clear();
    appendU8_(out, kSchemaVersion);
    appendString_(out, value["source"] | "");
    appendString_(out, value["source_user"] | "");
    const uint8_t count = items.isNull() ? 0u : (uint8_t)min((size_t)255u, items.size());
    appendU8_(out, count);
    if (!items.isNull())
    {
        for (JsonObjectConst item : items)
        {
            uint16_t mask = 0;
            if (item.containsKey("enabled"))
                mask |= kSocketSetEnabled;
            if (item.containsKey("name"))
                mask |= kSocketSetName;
            if (item.containsKey("button"))
                mask |= kSocketSetButton;
            if (item.containsKey("relay"))
                mask |= kSocketSetRelay;
            if (item.containsKey("group_id"))
                mask |= kSocketSetGroup;
            if (item.containsKey("state"))
                mask |= kSocketSetState;
            if (item.containsKey("toggle"))
                mask |= kSocketSetToggle;
            appendU8_(out, (uint8_t)(item["id"] | 0));
            appendU16_(out, mask);
            if (mask & kSocketSetEnabled)
                appendBool_(out, item["enabled"] | false);
            if (mask & kSocketSetName)
                appendString_(out, item["name"] | "");
            if (mask & kSocketSetButton)
                appendU8_(out, (uint8_t)(item["button"] | 0xFF));
            if (mask & kSocketSetRelay)
                appendU8_(out, (uint8_t)(item["relay"] | 0xFF));
            if (mask & kSocketSetGroup)
                appendU8_(out, (uint8_t)(item["group_id"] | 0));
            if (mask & kSocketSetState)
                appendBool_(out, item["state"] | false);
            if (mask & kSocketSetToggle)
                appendBool_(out, item["toggle"] | false);
        }
    }
    return true;
}

bool decodeSocketSet_(const uint8_t *data, size_t size, DynamicJsonDocument &out)
{
    const uint8_t *p = data;
    size_t left = size;
    String source;
    String source_user;
    uint8_t count = 0;
    if (!readVersion_(p, left) || !readString_(p, left, source) || !readString_(p, left, source_user) || !readU8_(p, left, count))
        return false;
    out.clear();
    if (source.length())
        out["source"] = source;
    if (source_user.length())
        out["source_user"] = source_user;
    JsonArray items = out["items"].to<JsonArray>();
    for (uint8_t i = 0; i < count; ++i)
    {
        uint8_t id = 0;
        uint16_t mask = 0;
        if (!readU8_(p, left, id) || !readU16_(p, left, mask))
            return false;
        JsonObject item = items.add<JsonObject>();
        item["id"] = id;
        bool boolv = false;
        uint8_t u8 = 0;
        String text;
        if (mask & kSocketSetEnabled)
        {
            if (!readBool_(p, left, boolv))
                return false;
            item["enabled"] = boolv;
        }
        if (mask & kSocketSetName)
        {
            if (!readString_(p, left, text))
                return false;
            item["name"] = text;
        }
        if (mask & kSocketSetButton)
        {
            if (!readU8_(p, left, u8))
                return false;
            item["button"] = u8;
        }
        if (mask & kSocketSetRelay)
        {
            if (!readU8_(p, left, u8))
                return false;
            item["relay"] = u8;
        }
        if (mask & kSocketSetGroup)
        {
            if (!readU8_(p, left, u8))
                return false;
            item["group_id"] = u8;
        }
        if (mask & kSocketSetState)
        {
            if (!readBool_(p, left, boolv))
                return false;
            item["state"] = boolv;
        }
        if (mask & kSocketSetToggle)
        {
            if (!readBool_(p, left, boolv))
                return false;
            item["toggle"] = boolv;
        }
    }
    return left == 0;
}

bool encodeWateringSnapshot_(JsonVariantConst value, std::vector<uint8_t> &out)
{
    JsonObjectConst summary = value["summary"]["watering"].as<JsonObjectConst>();
    JsonArrayConst items = value["controllers"]["watering"].as<JsonArrayConst>();
    out.clear();
    appendU8_(out, kSchemaVersion);
    appendU16_(out, (uint16_t)(value["offset"] | 0));
    appendU16_(out, (uint16_t)(value["limit"] | StackUnitSnapshot::kPageSize));
    appendU16_(out, (uint16_t)(value["total"] | 0));
    appendU16_(out, (uint16_t)(summary["enabled"] | 0));
    appendU16_(out, (uint16_t)(summary["active"] | 0));
    const uint8_t count = items.isNull() ? 0u : (uint8_t)min((size_t)StackUnitSnapshot::kPageSize, items.size());
    appendU8_(out, count);
    if (!items.isNull())
    {
        uint8_t emitted = 0;
        for (JsonObjectConst item : items)
        {
            if (emitted >= count)
                break;
            appendU8_(out, (uint8_t)(item["id"] | 0));
            uint8_t flags = 0;
            if (item["enabled"] | false)
                flags |= kItemEnabled;
            if (item["status"] | false)
                flags |= kItemState;
            if (item["active"] | false)
                flags |= kItemBool3;
            if (item["paused"] | false)
                flags |= kItemBool4;
            if (item["slot1_enabled"] | false)
                flags |= kItemBool5;
            if (item["slot2_enabled"] | false)
                flags |= kItemBool6;
            if (item["slot3_enabled"] | false)
                flags |= kItemBool7;
            if (item["resume"] | false)
                flags |= kItemBool8;
            appendU8_(out, flags);
            appendU8_(out, (uint8_t)(item["port"] | 0xFF));
            appendU8_(out, (uint8_t)(item["tank"] | 0));
            appendU8_(out, (uint8_t)(item["weekdays_mask"] | 0));
            appendU8_(out, (uint8_t)(item["hour"] | 0xFF));
            appendU8_(out, (uint8_t)(item["minute"] | 0xFF));
            appendU32_(out, (uint32_t)(item["duration_s"] | 0u));
            appendU8_(out, (uint8_t)(item["hour2"] | 0xFF));
            appendU8_(out, (uint8_t)(item["minute2"] | 0xFF));
            appendU32_(out, (uint32_t)(item["duration2_s"] | 0u));
            appendU8_(out, (uint8_t)(item["hour3"] | 0xFF));
            appendU8_(out, (uint8_t)(item["minute3"] | 0xFF));
            appendU32_(out, (uint32_t)(item["duration3_s"] | 0u));
            appendU8_(out, (uint8_t)(item["resume_level"] | 0));
            const bool force = item["force"] | false;
            const uint32_t remaining_ms = force ? 0xFFFFFFFFu : (uint32_t)(item["remaining_ms"] | 0u);
            appendU32_(out, remaining_ms);
            appendString_(out, item["name"] | "");
            ++emitted;
        }
    }
    return true;
}

bool decodeWateringSnapshot_(const uint8_t *data, size_t size, DynamicJsonDocument &out)
{
    const uint8_t *p = data;
    size_t left = size;
    uint16_t offset = 0, limit = 0, total = 0, enabled_total = 0, active_total = 0;
    uint8_t count = 0;
    if (!readVersion_(p, left) || !readU16_(p, left, offset) || !readU16_(p, left, limit) || !readU16_(p, left, total) ||
        !readU16_(p, left, enabled_total) || !readU16_(p, left, active_total) || !readU8_(p, left, count))
        return false;
    out.clear();
    out["offset"] = offset;
    out["limit"] = limit;
    out["total"] = total;
    JsonObject summary = out["summary"].to<JsonObject>();
    JsonObject watering_summary = summary["watering"].to<JsonObject>();
    watering_summary["enabled"] = enabled_total;
    watering_summary["active"] = active_total;
    JsonObject controllers = out["controllers"].to<JsonObject>();
    JsonArray items = controllers["watering"].to<JsonArray>();
    for (uint8_t i = 0; i < count; ++i)
    {
        uint8_t id = 0;
        uint8_t flags = 0;
        uint8_t port = 0xFF;
        uint8_t tank = 0;
        uint8_t weekdays_mask = 0;
        uint8_t hour = 0xFF;
        uint8_t minute = 0xFF;
        uint32_t duration_s = 0;
        uint8_t hour2 = 0xFF;
        uint8_t minute2 = 0xFF;
        uint32_t duration2_s = 0;
        uint8_t hour3 = 0xFF;
        uint8_t minute3 = 0xFF;
        uint32_t duration3_s = 0;
        uint8_t resume_level = 0;
        uint32_t remaining_ms = 0;
        String name;
        if (!readU8_(p, left, id) || !readU8_(p, left, flags) || !readU8_(p, left, port) || !readU8_(p, left, tank) ||
            !readU8_(p, left, weekdays_mask) || !readU8_(p, left, hour) || !readU8_(p, left, minute) ||
            !readU32_(p, left, duration_s) || !readU8_(p, left, hour2) || !readU8_(p, left, minute2) ||
            !readU32_(p, left, duration2_s) || !readU8_(p, left, hour3) || !readU8_(p, left, minute3) ||
            !readU32_(p, left, duration3_s) || !readU8_(p, left, resume_level) || !readU32_(p, left, remaining_ms) ||
            !readString_(p, left, name))
            return false;
        JsonObject item = items.add<JsonObject>();
        item["id"] = id;
        item["enabled"] = (flags & kItemEnabled) != 0;
        item["status"] = (flags & kItemState) != 0;
        item["active"] = (flags & kItemBool3) != 0;
        item["paused"] = (flags & kItemBool4) != 0;
        item["slot1_enabled"] = (flags & kItemBool5) != 0;
        item["slot2_enabled"] = (flags & kItemBool6) != 0;
        item["slot3_enabled"] = (flags & kItemBool7) != 0;
        item["resume"] = (flags & kItemBool8) != 0;
        item["port"] = port;
        item["tank"] = tank;
        item["weekdays_mask"] = weekdays_mask;
        item["hour"] = hour;
        item["minute"] = minute;
        item["duration_s"] = duration_s;
        item["hour2"] = hour2;
        item["minute2"] = minute2;
        item["duration2_s"] = duration2_s;
        item["hour3"] = hour3;
        item["minute3"] = minute3;
        item["duration3_s"] = duration3_s;
        item["resume_level"] = resume_level;
        item["force"] = remaining_ms == 0xFFFFFFFFu;
        if (remaining_ms != 0xFFFFFFFFu)
            item["remaining_ms"] = remaining_ms;
        if (name.length())
            item["name"] = name;
    }
    return left == 0;
}

bool encodeMeteoSet_(JsonVariantConst value, std::vector<uint8_t> &out)
{
    JsonArrayConst items = value["items"].as<JsonArrayConst>();
    out.clear();
    appendU8_(out, kSchemaVersion);
    const uint8_t count = items.isNull() ? 0u : (uint8_t)min((size_t)255u, items.size());
    appendU8_(out, count);
    if (!items.isNull())
    {
        for (JsonObjectConst item : items)
        {
            uint16_t mask = 0;
            if (item.containsKey("enabled"))
                mask |= kMeteoSetEnabled;
            if (item.containsKey("name"))
                mask |= kMeteoSetName;
            if (item.containsKey("group_id"))
                mask |= kMeteoSetGroup;
            if (item.containsKey("type_id"))
                mask |= kMeteoSetType;
            if (item.containsKey("pin"))
                mask |= kMeteoSetPin;
            if (item.containsKey("addr_set") || item.containsKey("addr"))
                mask |= kMeteoSetAddr;
            if (item.containsKey("src_node") || item.containsKey("src_sensor"))
                mask |= kMeteoSetSource;
            appendU8_(out, (uint8_t)(item["id"] | 0));
            appendU16_(out, mask);
            if (mask & kMeteoSetEnabled)
                appendBool_(out, item["enabled"] | false);
            if (mask & kMeteoSetName)
                appendString_(out, item["name"] | "");
            if (mask & kMeteoSetGroup)
                appendU8_(out, (uint8_t)(item["group_id"] | 0));
            if (mask & kMeteoSetType)
                appendU8_(out, (uint8_t)(item["type_id"] | 0));
            if (mask & kMeteoSetPin)
                appendU8_(out, (uint8_t)(item["pin"] | 0xFF));
            if (mask & kMeteoSetAddr)
            {
                appendBool_(out, item["addr_set"] | false);
                appendString_(out, item["addr"] | "");
            }
            if (mask & kMeteoSetSource)
            {
                appendU32_(out, (uint32_t)(item["src_node"] | 0u));
                appendU8_(out, (uint8_t)(item["src_sensor"] | 0));
            }
        }
    }
    return true;
}

bool decodeMeteoSet_(const uint8_t *data, size_t size, DynamicJsonDocument &out)
{
    const uint8_t *p = data;
    size_t left = size;
    uint8_t count = 0;
    if (!readVersion_(p, left) || !readU8_(p, left, count))
        return false;
    out.clear();
    JsonArray items = out["items"].to<JsonArray>();
    for (uint8_t i = 0; i < count; ++i)
    {
        uint8_t id = 0;
        uint16_t mask = 0;
        if (!readU8_(p, left, id) || !readU16_(p, left, mask))
            return false;
        JsonObject item = items.add<JsonObject>();
        item["id"] = id;
        bool boolv = false;
        uint8_t u8 = 0;
        uint32_t u32 = 0;
        String text;
        if (mask & kMeteoSetEnabled)
        {
            if (!readBool_(p, left, boolv))
                return false;
            item["enabled"] = boolv;
        }
        if (mask & kMeteoSetName)
        {
            if (!readString_(p, left, text))
                return false;
            item["name"] = text;
        }
        if (mask & kMeteoSetGroup)
        {
            if (!readU8_(p, left, u8))
                return false;
            item["group_id"] = u8;
        }
        if (mask & kMeteoSetType)
        {
            if (!readU8_(p, left, u8))
                return false;
            item["type_id"] = u8;
        }
        if (mask & kMeteoSetPin)
        {
            if (!readU8_(p, left, u8))
                return false;
            item["pin"] = u8;
        }
        if (mask & kMeteoSetAddr)
        {
            if (!readBool_(p, left, boolv) || !readString_(p, left, text))
                return false;
            item["addr_set"] = boolv;
            item["addr"] = text;
        }
        if (mask & kMeteoSetSource)
        {
            if (!readU32_(p, left, u32) || !readU8_(p, left, u8))
                return false;
            item["src_node"] = u32;
            item["src_sensor"] = u8;
        }
    }
    return left == 0;
}

bool encodeThermoSet_(JsonVariantConst value, std::vector<uint8_t> &out)
{
    JsonArrayConst items = value["items"].as<JsonArrayConst>();
    out.clear();
    appendU8_(out, kSchemaVersion);
    appendString_(out, value["source"] | "");
    const uint8_t count = items.isNull() ? 0u : (uint8_t)min((size_t)255u, items.size());
    appendU8_(out, count);
    if (!items.isNull())
    {
        for (JsonObjectConst item : items)
        {
            uint16_t mask = 0;
            if (item.containsKey("enabled"))
                mask |= kThermoSetEnabled;
            if (item.containsKey("name"))
                mask |= kThermoSetName;
            if (item.containsKey("group_id"))
                mask |= kThermoSetGroup;
            if (item.containsKey("sensor_node_id") || item.containsKey("sensor_id"))
                mask |= kThermoSetSensor;
            if (item.containsKey("mode_id"))
                mask |= kThermoSetMode;
            if (item.containsKey("target_c"))
                mask |= kThermoSetTarget;
            if (item.containsKey("hyst"))
                mask |= kThermoSetHyst;
            if (item.containsKey("heat_port"))
                mask |= kThermoSetHeatPort;
            if (item.containsKey("cool_port"))
                mask |= kThermoSetCoolPort;
            if (item.containsKey("button_port"))
                mask |= kThermoSetButtonPort;
            if (item.containsKey("power_on"))
                mask |= kThermoSetPower;
            if (item.containsKey("toggle"))
                mask |= kThermoSetToggle;
            appendU8_(out, (uint8_t)(item["id"] | 0));
            appendU16_(out, mask);
            if (mask & kThermoSetEnabled)
                appendBool_(out, item["enabled"] | false);
            if (mask & kThermoSetName)
                appendString_(out, item["name"] | "");
            if (mask & kThermoSetGroup)
                appendU8_(out, (uint8_t)(item["group_id"] | 0));
            if (mask & kThermoSetSensor)
            {
                appendU32_(out, (uint32_t)(item["sensor_node_id"] | 0u));
                appendU8_(out, (uint8_t)(item["sensor_id"] | 0));
            }
            if (mask & kThermoSetMode)
                appendU8_(out, (uint8_t)(item["mode_id"] | 0));
            if (mask & kThermoSetTarget)
                appendI16_(out, (int16_t)(item["target_c"] | 0));
            if (mask & kThermoSetHyst)
                appendFloat_(out, item["hyst"].is<float>() ? item["hyst"].as<float>() : (float)(item["hyst"] | 0.0));
            if (mask & kThermoSetHeatPort)
                appendU8_(out, (uint8_t)(item["heat_port"] | 0xFF));
            if (mask & kThermoSetCoolPort)
                appendU8_(out, (uint8_t)(item["cool_port"] | 0xFF));
            if (mask & kThermoSetButtonPort)
                appendU8_(out, (uint8_t)(item["button_port"] | 0xFF));
            if (mask & kThermoSetPower)
                appendBool_(out, item["power_on"] | false);
            if (mask & kThermoSetToggle)
                appendBool_(out, item["toggle"] | false);
        }
    }
    return true;
}

bool decodeThermoSet_(const uint8_t *data, size_t size, DynamicJsonDocument &out)
{
    const uint8_t *p = data;
    size_t left = size;
    String source;
    uint8_t count = 0;
    if (!readVersion_(p, left) || !readString_(p, left, source) || !readU8_(p, left, count))
        return false;
    out.clear();
    if (source.length())
        out["source"] = source;
    JsonArray items = out["items"].to<JsonArray>();
    for (uint8_t i = 0; i < count; ++i)
    {
        uint8_t id = 0;
        uint16_t mask = 0;
        if (!readU8_(p, left, id) || !readU16_(p, left, mask))
            return false;
        JsonObject item = items.add<JsonObject>();
        item["id"] = id;
        bool boolv = false;
        uint8_t u8 = 0;
        uint32_t u32 = 0;
        float fv = 0.0f;
        int16_t i16v = 0;
        String text;
        if (mask & kThermoSetEnabled)
        {
            if (!readBool_(p, left, boolv))
                return false;
            item["enabled"] = boolv;
        }
        if (mask & kThermoSetName)
        {
            if (!readString_(p, left, text))
                return false;
            item["name"] = text;
        }
        if (mask & kThermoSetGroup)
        {
            if (!readU8_(p, left, u8))
                return false;
            item["group_id"] = u8;
        }
        if (mask & kThermoSetSensor)
        {
            if (!readU32_(p, left, u32) || !readU8_(p, left, u8))
                return false;
            item["sensor_node_id"] = u32;
            item["sensor_id"] = u8;
        }
        if (mask & kThermoSetMode)
        {
            if (!readU8_(p, left, u8))
                return false;
            item["mode_id"] = u8;
        }
        if (mask & kThermoSetTarget)
        {
            if (!readI16_(p, left, i16v))
                return false;
            item["target_c"] = i16v;
        }
        if (mask & kThermoSetHyst)
        {
            if (!readFloat_(p, left, fv))
                return false;
            item["hyst"] = fv;
        }
        if (mask & kThermoSetHeatPort)
        {
            if (!readU8_(p, left, u8))
                return false;
            item["heat_port"] = u8;
        }
        if (mask & kThermoSetCoolPort)
        {
            if (!readU8_(p, left, u8))
                return false;
            item["cool_port"] = u8;
        }
        if (mask & kThermoSetButtonPort)
        {
            if (!readU8_(p, left, u8))
                return false;
            item["button_port"] = u8;
        }
        if (mask & kThermoSetPower)
        {
            if (!readBool_(p, left, boolv))
                return false;
            item["power_on"] = boolv;
        }
        if (mask & kThermoSetToggle)
        {
            if (!readBool_(p, left, boolv))
                return false;
            item["toggle"] = boolv;
        }
    }
    return left == 0;
}

bool encodeTanksSet_(JsonVariantConst value, std::vector<uint8_t> &out)
{
    JsonArrayConst items = value["items"].as<JsonArrayConst>();
    out.clear();
    appendU8_(out, kSchemaVersion);
    const uint8_t count = items.isNull() ? 0u : (uint8_t)min((size_t)255u, items.size());
    appendU8_(out, count);
    if (!items.isNull())
    {
        for (JsonObjectConst item : items)
        {
            uint16_t mask = 0;
            if (item.containsKey("enabled"))
                mask |= kTankSetEnabled;
            if (item.containsKey("power_on"))
                mask |= kTankSetPower;
            if (item.containsKey("toggle"))
                mask |= kTankSetToggle;
            if (item.containsKey("name"))
                mask |= kTankSetName;
            if (item.containsKey("group_id"))
                mask |= kTankSetGroup;
            if (item.containsKey("low"))
                mask |= kTankSetLow;
            if (item.containsKey("mid"))
                mask |= kTankSetMid;
            if (item.containsKey("full"))
                mask |= kTankSetFull;
            if (item.containsKey("valve"))
                mask |= kTankSetValve;
            if (item.containsKey("pump"))
                mask |= kTankSetPump;
            if (item.containsKey("alarm"))
                mask |= kTankSetAlarm;
            appendU8_(out, (uint8_t)(item["id"] | 0));
            appendU16_(out, mask);
            if (mask & kTankSetEnabled)
                appendBool_(out, item["enabled"] | false);
            if (mask & kTankSetPower)
                appendBool_(out, item["power_on"] | false);
            if (mask & kTankSetToggle)
                appendBool_(out, item["toggle"] | false);
            if (mask & kTankSetName)
                appendString_(out, item["name"] | "");
            if (mask & kTankSetGroup)
                appendU8_(out, (uint8_t)(item["group_id"] | 0));
            if (mask & kTankSetLow)
                appendU8_(out, (uint8_t)(item["low"] | 0xFF));
            if (mask & kTankSetMid)
                appendU8_(out, (uint8_t)(item["mid"] | 0xFF));
            if (mask & kTankSetFull)
                appendU8_(out, (uint8_t)(item["full"] | 0xFF));
            if (mask & kTankSetValve)
                appendU8_(out, (uint8_t)(item["valve"] | 0xFF));
            if (mask & kTankSetPump)
                appendU8_(out, (uint8_t)(item["pump"] | 0xFF));
            if (mask & kTankSetAlarm)
                appendU8_(out, (uint8_t)(item["alarm"] | 0xFF));
        }
    }
    return true;
}

bool decodeTanksSet_(const uint8_t *data, size_t size, DynamicJsonDocument &out)
{
    const uint8_t *p = data;
    size_t left = size;
    uint8_t count = 0;
    if (!readVersion_(p, left) || !readU8_(p, left, count))
        return false;
    out.clear();
    JsonArray items = out["items"].to<JsonArray>();
    for (uint8_t i = 0; i < count; ++i)
    {
        uint8_t id = 0;
        uint16_t mask = 0;
        if (!readU8_(p, left, id) || !readU16_(p, left, mask))
            return false;
        JsonObject item = items.add<JsonObject>();
        item["id"] = id;
        bool boolv = false;
        uint8_t u8 = 0;
        String text;
        if (mask & kTankSetEnabled)
        {
            if (!readBool_(p, left, boolv))
                return false;
            item["enabled"] = boolv;
        }
        if (mask & kTankSetPower)
        {
            if (!readBool_(p, left, boolv))
                return false;
            item["power_on"] = boolv;
        }
        if (mask & kTankSetToggle)
        {
            if (!readBool_(p, left, boolv))
                return false;
            item["toggle"] = boolv;
        }
        if (mask & kTankSetName)
        {
            if (!readString_(p, left, text))
                return false;
            item["name"] = text;
        }
        if (mask & kTankSetGroup)
        {
            if (!readU8_(p, left, u8))
                return false;
            item["group_id"] = u8;
        }
        if (mask & kTankSetLow)
        {
            if (!readU8_(p, left, u8))
                return false;
            item["low"] = u8;
        }
        if (mask & kTankSetMid)
        {
            if (!readU8_(p, left, u8))
                return false;
            item["mid"] = u8;
        }
        if (mask & kTankSetFull)
        {
            if (!readU8_(p, left, u8))
                return false;
            item["full"] = u8;
        }
        if (mask & kTankSetValve)
        {
            if (!readU8_(p, left, u8))
                return false;
            item["valve"] = u8;
        }
        if (mask & kTankSetPump)
        {
            if (!readU8_(p, left, u8))
                return false;
            item["pump"] = u8;
        }
        if (mask & kTankSetAlarm)
        {
            if (!readU8_(p, left, u8))
                return false;
            item["alarm"] = u8;
        }
    }
    return left == 0;
}

bool encodeRingState_(JsonVariantConst value, const char *key, std::vector<uint8_t> &out)
{
    out.clear();
    appendU8_(out, kSchemaVersion);
    appendBool_(out, value[key] | false);
    return true;
}

bool decodeRingState_(const uint8_t *data, size_t size, const char *key, DynamicJsonDocument &out)
{
    const uint8_t *p = data;
    size_t left = size;
    bool v = false;
    if (!readVersion_(p, left) || !readBool_(p, left, v) || left != 0)
        return false;
    out.clear();
    out[key] = v;
    return true;
}

bool encodeSepticSet_(JsonVariantConst value, std::vector<uint8_t> &out)
{
    uint8_t mask = 0;
    if (value.containsKey("monitor"))
        mask |= kSepticSetMonitor;
    if (value.containsKey("enabled"))
        mask |= kSepticSetEnabled;
    out.clear();
    appendU8_(out, kSchemaVersion);
    appendU8_(out, (uint8_t)(value["id"] | 1));
    appendU8_(out, mask);
    if (mask & kSepticSetMonitor)
        appendBool_(out, value["monitor"] | false);
    if (mask & kSepticSetEnabled)
        appendBool_(out, value["enabled"] | false);
    return true;
}

bool decodeSepticSet_(const uint8_t *data, size_t size, DynamicJsonDocument &out)
{
    const uint8_t *p = data;
    size_t left = size;
    uint8_t id = 0;
    uint8_t mask = 0;
    if (!readVersion_(p, left) || !readU8_(p, left, id) || !readU8_(p, left, mask))
        return false;
    out.clear();
    out["id"] = id;
    bool v = false;
    if (mask & kSepticSetMonitor)
    {
        if (!readBool_(p, left, v))
            return false;
        out["monitor"] = v;
    }
    if (mask & kSepticSetEnabled)
    {
        if (!readBool_(p, left, v))
            return false;
        out["enabled"] = v;
    }
    return left == 0;
}

bool encodeSepticLevel_(JsonVariantConst value, std::vector<uint8_t> &out)
{
    uint8_t mask = 0;
    if (value["alarm"] | false)
        mask |= kSepticLevelAlarm;
    if (!value["level"].isNull())
        mask |= kSepticLevelHasLevel;
    if (!value["name"].isNull())
        mask |= kSepticLevelHasName;
    out.clear();
    appendU8_(out, kSchemaVersion);
    appendU8_(out, (uint8_t)(value["id"] | 0));
    appendU8_(out, mask);
    if (mask & kSepticLevelHasLevel)
        appendString_(out, value["level"] | "");
    if (mask & kSepticLevelHasName)
        appendString_(out, value["name"] | "");
    return true;
}

bool decodeSepticLevel_(const uint8_t *data, size_t size, DynamicJsonDocument &out)
{
    const uint8_t *p = data;
    size_t left = size;
    uint8_t id = 0;
    uint8_t mask = 0;
    if (!readVersion_(p, left) || !readU8_(p, left, id) || !readU8_(p, left, mask))
        return false;
    out.clear();
    out["id"] = id;
    out["alarm"] = (mask & kSepticLevelAlarm) != 0;
    String text;
    if (mask & kSepticLevelHasLevel)
    {
        if (!readString_(p, left, text))
            return false;
        out["level"] = text;
    }
    if (mask & kSepticLevelHasName)
    {
        if (!readString_(p, left, text))
            return false;
        out["name"] = text;
    }
    return left == 0;
}

bool encodeTankEmpty_(JsonVariantConst value, std::vector<uint8_t> &out)
{
    uint8_t mask = 0;
    if (value["empty"] | false)
        mask |= kTankEmptyActive;
    if (!value["name"].isNull())
        mask |= kTankEmptyHasName;
    out.clear();
    appendU8_(out, kSchemaVersion);
    appendU8_(out, (uint8_t)(value["id"] | 0));
    appendU8_(out, mask);
    if (mask & kTankEmptyHasName)
        appendString_(out, value["name"] | "");
    return true;
}

bool decodeTankEmpty_(const uint8_t *data, size_t size, DynamicJsonDocument &out)
{
    const uint8_t *p = data;
    size_t left = size;
    uint8_t id = 0;
    uint8_t mask = 0;
    if (!readVersion_(p, left) || !readU8_(p, left, id) || !readU8_(p, left, mask))
        return false;
    out.clear();
    out["id"] = id;
    out["empty"] = (mask & kTankEmptyActive) != 0;
    if (mask & kTankEmptyHasName)
    {
        String name;
        if (!readString_(p, left, name))
            return false;
        out["name"] = name;
    }
    return left == 0;
}

bool encodeWateringEvent_(JsonVariantConst value, std::vector<uint8_t> &out)
{
    uint8_t mask = 0;
    if (!value["name"].isNull())
        mask |= kWateringHasName;
    if (!value["reason"].isNull())
        mask |= kWateringHasReason;
    out.clear();
    appendU8_(out, kSchemaVersion);
    appendString_(out, value["event"] | "");
    appendU8_(out, (uint8_t)(value["id"] | 0));
    appendU8_(out, (uint8_t)(value["port"] | 0xFF));
    appendU8_(out, (uint8_t)(value["tank"] | 0));
    appendU8_(out, mask);
    appendU32_(out, (uint32_t)(value["remaining_ms"] | 0u));
    appendU8_(out, (uint8_t)(value["resume_level"] | 0));
    if (mask & kWateringHasName)
        appendString_(out, value["name"] | "");
    if (mask & kWateringHasReason)
        appendString_(out, value["reason"] | "");
    return true;
}

bool decodeWateringEvent_(const uint8_t *data, size_t size, DynamicJsonDocument &out)
{
    const uint8_t *p = data;
    size_t left = size;
    String event;
    uint8_t id = 0, port = 0, tank = 0, mask = 0, resume_level = 0;
    uint32_t remaining_ms = 0;
    if (!readVersion_(p, left) || !readString_(p, left, event) || !readU8_(p, left, id) || !readU8_(p, left, port) ||
        !readU8_(p, left, tank) || !readU8_(p, left, mask) || !readU32_(p, left, remaining_ms) ||
        !readU8_(p, left, resume_level))
        return false;
    out.clear();
    out["event"] = event;
    out["id"] = id;
    out["port"] = port;
    out["tank"] = tank;
    out["remaining_ms"] = remaining_ms;
    out["resume_level"] = resume_level;
    if (mask & kWateringHasName)
    {
        String text;
        if (!readString_(p, left, text))
            return false;
        out["name"] = text;
    }
    if (mask & kWateringHasReason)
    {
        String text;
        if (!readString_(p, left, text))
            return false;
        out["reason"] = text;
    }
    return left == 0;
}

bool encodeWateringSet_(JsonVariantConst value, std::vector<uint8_t> &out)
{
    JsonArrayConst items = value["items"].as<JsonArrayConst>();
    out.clear();
    appendU8_(out, kSchemaVersion);
    const uint8_t count = items.isNull() ? 0u : (uint8_t)min((size_t)255u, items.size());
    appendU8_(out, count);
    if (!items.isNull())
    {
        for (JsonObjectConst item : items)
        {
            uint32_t mask = 0;
            if (item.containsKey("enabled"))
                mask |= kWateringSetEnabled;
            if (item.containsKey("name"))
                mask |= kWateringSetName;
            if (item.containsKey("port"))
                mask |= kWateringSetPort;
            if (item.containsKey("status"))
                mask |= kWateringSetStatus;
            if (item.containsKey("weekdays_mask"))
                mask |= kWateringSetWeekdays;
            if (item.containsKey("tank"))
                mask |= kWateringSetTank;
            if (item.containsKey("resume"))
                mask |= kWateringSetResume;
            if (item.containsKey("resume_level"))
                mask |= kWateringSetResumeLevel;
            if (item.containsKey("hour") || item.containsKey("minute"))
                mask |= kWateringSetSlot1Time;
            if (item.containsKey("duration_s"))
                mask |= kWateringSetSlot1Duration;
            if (item.containsKey("slot1_enabled"))
                mask |= kWateringSetSlot1Enabled;
            if (item.containsKey("hour2") || item.containsKey("minute2"))
                mask |= kWateringSetSlot2Time;
            if (item.containsKey("duration2_s"))
                mask |= kWateringSetSlot2Duration;
            if (item.containsKey("slot2_enabled"))
                mask |= kWateringSetSlot2Enabled;
            if (item.containsKey("hour3") || item.containsKey("minute3"))
                mask |= kWateringSetSlot3Time;
            if (item.containsKey("duration3_s"))
                mask |= kWateringSetSlot3Duration;
            if (item.containsKey("slot3_enabled"))
                mask |= kWateringSetSlot3Enabled;
            if (item.containsKey("force"))
                mask |= kWateringSetForce;
            appendU8_(out, (uint8_t)(item["id"] | 0));
            appendU32_(out, mask);
            if (mask & kWateringSetEnabled)
                appendBool_(out, item["enabled"] | false);
            if (mask & kWateringSetName)
                appendString_(out, item["name"] | "");
            if (mask & kWateringSetPort)
                appendU8_(out, (uint8_t)(item["port"] | 0xFF));
            if (mask & kWateringSetStatus)
                appendBool_(out, item["status"] | false);
            if (mask & kWateringSetWeekdays)
                appendU8_(out, (uint8_t)(item["weekdays_mask"] | 0));
            if (mask & kWateringSetTank)
                appendU8_(out, (uint8_t)(item["tank"] | 0));
            if (mask & kWateringSetResume)
                appendBool_(out, item["resume"] | false);
            if (mask & kWateringSetResumeLevel)
                appendU8_(out, (uint8_t)(item["resume_level"] | 0));
            if (mask & kWateringSetSlot1Time)
            {
                appendU8_(out, (uint8_t)(item["hour"] | 0xFF));
                appendU8_(out, (uint8_t)(item["minute"] | 0xFF));
            }
            if (mask & kWateringSetSlot1Duration)
                appendU32_(out, (uint32_t)(item["duration_s"] | 0u));
            if (mask & kWateringSetSlot1Enabled)
                appendBool_(out, item["slot1_enabled"] | false);
            if (mask & kWateringSetSlot2Time)
            {
                appendU8_(out, (uint8_t)(item["hour2"] | 0xFF));
                appendU8_(out, (uint8_t)(item["minute2"] | 0xFF));
            }
            if (mask & kWateringSetSlot2Duration)
                appendU32_(out, (uint32_t)(item["duration2_s"] | 0u));
            if (mask & kWateringSetSlot2Enabled)
                appendBool_(out, item["slot2_enabled"] | false);
            if (mask & kWateringSetSlot3Time)
            {
                appendU8_(out, (uint8_t)(item["hour3"] | 0xFF));
                appendU8_(out, (uint8_t)(item["minute3"] | 0xFF));
            }
            if (mask & kWateringSetSlot3Duration)
                appendU32_(out, (uint32_t)(item["duration3_s"] | 0u));
            if (mask & kWateringSetSlot3Enabled)
                appendBool_(out, item["slot3_enabled"] | false);
            if (mask & kWateringSetForce)
                appendBool_(out, item["force"] | false);
        }
    }
    return true;
}

bool decodeWateringSet_(const uint8_t *data, size_t size, DynamicJsonDocument &out)
{
    const uint8_t *p = data;
    size_t left = size;
    uint8_t count = 0;
    if (!readVersion_(p, left) || !readU8_(p, left, count))
        return false;
    out.clear();
    JsonArray items = out["items"].to<JsonArray>();
    for (uint8_t i = 0; i < count; ++i)
    {
        uint8_t id = 0;
        uint32_t mask = 0;
        if (!readU8_(p, left, id) || !readU32_(p, left, mask))
            return false;
        JsonObject item = items.add<JsonObject>();
        item["id"] = id;
        bool boolv = false;
        uint8_t u8 = 0;
        uint32_t u32 = 0;
        String text;
        if (mask & kWateringSetEnabled)
        {
            if (!readBool_(p, left, boolv))
                return false;
            item["enabled"] = boolv;
        }
        if (mask & kWateringSetName)
        {
            if (!readString_(p, left, text))
                return false;
            item["name"] = text;
        }
        if (mask & kWateringSetPort)
        {
            if (!readU8_(p, left, u8))
                return false;
            item["port"] = u8;
        }
        if (mask & kWateringSetStatus)
        {
            if (!readBool_(p, left, boolv))
                return false;
            item["status"] = boolv;
        }
        if (mask & kWateringSetWeekdays)
        {
            if (!readU8_(p, left, u8))
                return false;
            item["weekdays_mask"] = u8;
        }
        if (mask & kWateringSetTank)
        {
            if (!readU8_(p, left, u8))
                return false;
            item["tank"] = u8;
        }
        if (mask & kWateringSetResume)
        {
            if (!readBool_(p, left, boolv))
                return false;
            item["resume"] = boolv;
        }
        if (mask & kWateringSetResumeLevel)
        {
            if (!readU8_(p, left, u8))
                return false;
            item["resume_level"] = u8;
        }
        if (mask & kWateringSetSlot1Time)
        {
            if (!readU8_(p, left, u8))
                return false;
            item["hour"] = u8;
            if (!readU8_(p, left, u8))
                return false;
            item["minute"] = u8;
        }
        if (mask & kWateringSetSlot1Duration)
        {
            if (!readU32_(p, left, u32))
                return false;
            item["duration_s"] = u32;
        }
        if (mask & kWateringSetSlot1Enabled)
        {
            if (!readBool_(p, left, boolv))
                return false;
            item["slot1_enabled"] = boolv;
        }
        if (mask & kWateringSetSlot2Time)
        {
            if (!readU8_(p, left, u8))
                return false;
            item["hour2"] = u8;
            if (!readU8_(p, left, u8))
                return false;
            item["minute2"] = u8;
        }
        if (mask & kWateringSetSlot2Duration)
        {
            if (!readU32_(p, left, u32))
                return false;
            item["duration2_s"] = u32;
        }
        if (mask & kWateringSetSlot2Enabled)
        {
            if (!readBool_(p, left, boolv))
                return false;
            item["slot2_enabled"] = boolv;
        }
        if (mask & kWateringSetSlot3Time)
        {
            if (!readU8_(p, left, u8))
                return false;
            item["hour3"] = u8;
            if (!readU8_(p, left, u8))
                return false;
            item["minute3"] = u8;
        }
        if (mask & kWateringSetSlot3Duration)
        {
            if (!readU32_(p, left, u32))
                return false;
            item["duration3_s"] = u32;
        }
        if (mask & kWateringSetSlot3Enabled)
        {
            if (!readBool_(p, left, boolv))
                return false;
            item["slot3_enabled"] = boolv;
        }
        if (mask & kWateringSetForce)
        {
            if (!readBool_(p, left, boolv))
                return false;
            item["force"] = boolv;
        }
    }
    return left == 0;
}

bool encodeSecuritySet_(JsonVariantConst value, std::vector<uint8_t> &out)
{
    uint8_t mask = 0;
    if (value.containsKey("armed"))
        mask |= kSecuritySetArmed;
    if (value.containsKey("force"))
        mask |= kSecuritySetForce;
    if (value.containsKey("alarm"))
        mask |= kSecuritySetAlarm;
    if (value.containsKey("clear"))
        mask |= kSecuritySetClear;
    if (value.containsKey("beep"))
        mask |= kSecuritySetBeep;
    out.clear();
    appendU8_(out, kSchemaVersion);
    appendU8_(out, mask);
    if (mask & kSecuritySetArmed)
        appendBool_(out, value["armed"] | false);
    if (mask & kSecuritySetForce)
        appendBool_(out, value["force"] | false);
    if (mask & kSecuritySetAlarm)
        appendBool_(out, value["alarm"] | false);
    if (mask & kSecuritySetClear)
        appendBool_(out, value["clear"] | false);
    if (mask & kSecuritySetBeep)
        appendString_(out, value["beep"] | "");
    return true;
}

bool decodeSecuritySet_(const uint8_t *data, size_t size, DynamicJsonDocument &out)
{
    const uint8_t *p = data;
    size_t left = size;
    uint8_t mask = 0;
    if (!readVersion_(p, left) || !readU8_(p, left, mask))
        return false;
    out.clear();
    bool boolv = false;
    if (mask & kSecuritySetArmed)
    {
        if (!readBool_(p, left, boolv))
            return false;
        out["armed"] = boolv;
    }
    if (mask & kSecuritySetForce)
    {
        if (!readBool_(p, left, boolv))
            return false;
        out["force"] = boolv;
    }
    if (mask & kSecuritySetAlarm)
    {
        if (!readBool_(p, left, boolv))
            return false;
        out["alarm"] = boolv;
    }
    if (mask & kSecuritySetClear)
    {
        if (!readBool_(p, left, boolv))
            return false;
        out["clear"] = boolv;
    }
    if (mask & kSecuritySetBeep)
    {
        String text;
        if (!readString_(p, left, text))
            return false;
        out["beep"] = text;
    }
    return left == 0;
}

bool encodeSecurityAlarm_(JsonVariantConst value, std::vector<uint8_t> &out)
{
    uint8_t mask = 0;
    if (value["alarm"] | false)
        mask |= kSecurityAlarmActive;
    if (value["silent"] | false)
        mask |= kSecurityAlarmSilent;
    if (!value["name"].isNull())
        mask |= kSecurityAlarmHasName;
    out.clear();
    appendU8_(out, kSchemaVersion);
    appendU8_(out, mask);
    appendU8_(out, (uint8_t)(value["sensor_id"] | 0));
    if (mask & kSecurityAlarmHasName)
        appendString_(out, value["name"] | "");
    return true;
}

bool decodeSecurityAlarm_(const uint8_t *data, size_t size, DynamicJsonDocument &out)
{
    const uint8_t *p = data;
    size_t left = size;
    uint8_t mask = 0;
    uint8_t sensor_id = 0;
    if (!readVersion_(p, left) || !readU8_(p, left, mask) || !readU8_(p, left, sensor_id))
        return false;
    out.clear();
    out["alarm"] = (mask & kSecurityAlarmActive) != 0;
    out["silent"] = (mask & kSecurityAlarmSilent) != 0;
    out["sensor_id"] = sensor_id;
    if (mask & kSecurityAlarmHasName)
    {
        String text;
        if (!readString_(p, left, text))
            return false;
        out["name"] = text;
    }
    return left == 0;
}

bool encodeSecuritySimplePair_(JsonVariantConst value, const char *id_key, const char *name_key, std::vector<uint8_t> &out)
{
    out.clear();
    appendU8_(out, kSchemaVersion);
    appendString_(out, value[id_key] | "");
    appendString_(out, value[name_key] | "");
    return true;
}

bool decodeSecuritySimplePair_(const uint8_t *data, size_t size, const char *id_key, const char *name_key, DynamicJsonDocument &out)
{
    const uint8_t *p = data;
    size_t left = size;
    String id;
    String name;
    if (!readVersion_(p, left) || !readString_(p, left, id) || !readString_(p, left, name) || left != 0)
        return false;
    out.clear();
    out[id_key] = id;
    if (name.length())
        out[name_key] = name;
    return true;
}

bool encodeQuickActionRun_(JsonVariantConst value, std::vector<uint8_t> &out)
{
    out.clear();
    appendU8_(out, kSchemaVersion);
    appendString_(out, value["preset"] | "");
    return true;
}

bool decodeQuickActionRun_(const uint8_t *data, size_t size, DynamicJsonDocument &out)
{
    const uint8_t *p = data;
    size_t left = size;
    String preset;
    if (!readVersion_(p, left) || !readString_(p, left, preset) || left != 0)
        return false;
    out.clear();
    out["preset"] = preset;
    return true;
}

bool encodeRuleRun_(JsonVariantConst value, std::vector<uint8_t> &out)
{
    out.clear();
    appendU8_(out, kSchemaVersion);
    appendU8_(out, (uint8_t)(value["id"] | 0));
    return true;
}

bool decodeRuleRun_(const uint8_t *data, size_t size, DynamicJsonDocument &out)
{
    const uint8_t *p = data;
    size_t left = size;
    uint8_t id = 0;
    if (!readVersion_(p, left) || !readU8_(p, left, id) || left != 0)
        return false;
    out.clear();
    out["id"] = id;
    return true;
}

bool encodeSecurityPrearmBlocked_(JsonVariantConst value, std::vector<uint8_t> &out)
{
    out.clear();
    appendU8_(out, kSchemaVersion);
    appendString_(out, value["details"] | "");
    return true;
}

bool decodeSecurityPrearmBlocked_(const uint8_t *data, size_t size, DynamicJsonDocument &out)
{
    const uint8_t *p = data;
    size_t left = size;
    String details;
    if (!readVersion_(p, left) || !readString_(p, left, details) || left != 0)
        return false;
    out.clear();
    out["details"] = details;
    return true;
}

bool encodeSecurityResult_(JsonVariantConst value, const char *id_key, std::vector<uint8_t> &out)
{
    uint8_t mask = 0;
    if (value["match"] | false)
        mask |= kSecurityResultMatch;
    if (value["armed"] | false)
        mask |= kSecurityResultArmed;
    out.clear();
    appendU8_(out, kSchemaVersion);
    appendU8_(out, mask);
    appendString_(out, value[id_key] | "");
    appendString_(out, value["result"] | "");
    return true;
}

bool decodeSecurityResult_(const uint8_t *data, size_t size, const char *id_key, DynamicJsonDocument &out)
{
    const uint8_t *p = data;
    size_t left = size;
    uint8_t mask = 0;
    String id;
    String result;
    if (!readVersion_(p, left) || !readU8_(p, left, mask) || !readString_(p, left, id) || !readString_(p, left, result) ||
        left != 0)
        return false;
    out.clear();
    out[id_key] = id;
    out["match"] = (mask & kSecurityResultMatch) != 0;
    out["armed"] = (mask & kSecurityResultArmed) != 0;
    out["result"] = result;
    return true;
}

bool encodeWebIndexState_(JsonVariantConst value, std::vector<uint8_t> &out)
{
    std::vector<uint8_t> system_part;
    std::vector<uint8_t> summary_part;
    std::vector<uint8_t> sockets_part;
    std::vector<uint8_t> lights_part;
    if (!encodeSystemSnapshot_(value, system_part) || !encodeControllersSummary_(value, summary_part) ||
        !encodeSocketLikeSnapshot_(value, "sockets", "sockets", sockets_part) ||
        !encodeSocketLikeSnapshot_(value, "lights", "lights", lights_part))
        return false;
    out.clear();
    appendU8_(out, kSchemaVersion);
    appendU16_(out, (uint16_t)system_part.size());
    appendBytes_(out, system_part.data(), system_part.size());
    appendU16_(out, (uint16_t)summary_part.size());
    appendBytes_(out, summary_part.data(), summary_part.size());
    appendU16_(out, (uint16_t)sockets_part.size());
    appendBytes_(out, sockets_part.data(), sockets_part.size());
    appendU16_(out, (uint16_t)lights_part.size());
    appendBytes_(out, lights_part.data(), lights_part.size());
    const char *relay_bits = value["relay_used_bits"] | "";
    const char *dinput_bits = value["dinput_used_bits"] | "";
    const char *sensor_bits = value["sensor_used_bits"] | "";
    for (size_t i = 0; i < StackUnitSnapshot::kPortMaskBytes; ++i)
    {
        auto hexByte = [](const char *s, size_t idx) -> uint8_t {
            auto nibble = [](char c) -> uint8_t {
                if (c >= '0' && c <= '9')
                    return (uint8_t)(c - '0');
                if (c >= 'a' && c <= 'f')
                    return (uint8_t)(10 + (c - 'a'));
                if (c >= 'A' && c <= 'F')
                    return (uint8_t)(10 + (c - 'A'));
                return 0;
            };
            const size_t off = idx * 2u;
            if (!s || strlen(s) < off + 2u)
                return 0;
            return (uint8_t)((nibble(s[off]) << 4) | nibble(s[off + 1u]));
        };
        appendU8_(out, hexByte(relay_bits, i));
    }
    for (size_t i = 0; i < StackUnitSnapshot::kPortMaskBytes; ++i)
    {
        auto hexByte = [](const char *s, size_t idx) -> uint8_t {
            auto nibble = [](char c) -> uint8_t {
                if (c >= '0' && c <= '9')
                    return (uint8_t)(c - '0');
                if (c >= 'a' && c <= 'f')
                    return (uint8_t)(10 + (c - 'a'));
                if (c >= 'A' && c <= 'F')
                    return (uint8_t)(10 + (c - 'A'));
                return 0;
            };
            const size_t off = idx * 2u;
            if (!s || strlen(s) < off + 2u)
                return 0;
            return (uint8_t)((nibble(s[off]) << 4) | nibble(s[off + 1u]));
        };
        appendU8_(out, hexByte(dinput_bits, i));
    }
    for (size_t i = 0; i < StackUnitSnapshot::kPortMaskBytes; ++i)
    {
        auto hexByte = [](const char *s, size_t idx) -> uint8_t {
            auto nibble = [](char c) -> uint8_t {
                if (c >= '0' && c <= '9')
                    return (uint8_t)(c - '0');
                if (c >= 'a' && c <= 'f')
                    return (uint8_t)(10 + (c - 'a'));
                if (c >= 'A' && c <= 'F')
                    return (uint8_t)(10 + (c - 'A'));
                return 0;
            };
            const size_t off = idx * 2u;
            if (!s || strlen(s) < off + 2u)
                return 0;
            return (uint8_t)((nibble(s[off]) << 4) | nibble(s[off + 1u]));
        };
        appendU8_(out, hexByte(sensor_bits, i));
    }
    return true;
}

bool decodeWebIndexState_(const uint8_t *data, size_t size, DynamicJsonDocument &out)
{
    const uint8_t *p = data;
    size_t left = size;
    uint16_t len = 0;
    if (!readVersion_(p, left))
        return false;
    out.clear();
    auto merge_part = [&](uint16_t part_len, bool (*fn)(const uint8_t *, size_t, DynamicJsonDocument &)) -> bool {
        const uint8_t *part = nullptr;
        if (!readBytes_(p, left, part, part_len))
            return false;
        DynamicJsonDocument tmp(max((size_t)512u, (size_t)part_len * 4u));
        if (!fn(part, part_len, tmp))
            return false;
        for (JsonPair kv : tmp.as<JsonObject>())
            out[kv.key()] = kv.value();
        return true;
    };
    if (!readU16_(p, left, len) || !merge_part(len, decodeSystemSnapshot_))
        return false;
    if (!readU16_(p, left, len) || !merge_part(len, decodeControllersSummary_))
        return false;
    if (!readU16_(p, left, len))
        return false;
    {
        const uint8_t *part = nullptr;
        if (!readBytes_(p, left, part, len))
            return false;
        DynamicJsonDocument tmp(max((size_t)512u, (size_t)len * 4u));
        if (!decodeSocketLikeSnapshot_(part, len, "sockets", "sockets", tmp))
            return false;
        for (JsonPair kv : tmp.as<JsonObject>())
            out[kv.key()] = kv.value();
    }
    if (!readU16_(p, left, len))
        return false;
    {
        const uint8_t *part = nullptr;
        if (!readBytes_(p, left, part, len))
            return false;
        DynamicJsonDocument tmp(max((size_t)512u, (size_t)len * 4u));
        if (!decodeSocketLikeSnapshot_(part, len, "lights", "lights", tmp))
            return false;
        out["summary"]["lights"] = tmp["summary"]["lights"];
        out["controllers"]["lights"] = tmp["controllers"]["lights"];
    }
    auto appendHexString = [&](const char *key) -> bool {
        String hex;
        hex.reserve(StackUnitSnapshot::kPortMaskBytes * 2u);
        for (size_t i = 0; i < StackUnitSnapshot::kPortMaskBytes; ++i)
        {
            uint8_t value = 0;
            if (!readU8_(p, left, value))
                return false;
            char buf[3] = {};
            snprintf(buf, sizeof(buf), "%02X", (unsigned)value);
            hex += buf;
        }
        out[key] = hex;
        return true;
    };
    if (left == StackUnitSnapshot::kPortMaskBytes * 3u)
    {
        if (!appendHexString("relay_used_bits") || !appendHexString("dinput_used_bits") ||
            !appendHexString("sensor_used_bits"))
            return false;
    }
    return left == 0;
}
} // namespace

bool StackDataBinaryCodec::encode(const char *feature, const char *action, JsonVariantConst value, std::vector<uint8_t> &out)
{
    if (!feature || !action)
        return false;
    if (strcmp(feature, "system") == 0)
    {
        if (strcmp(action, "snapshot") == 0)
            return encodeSystemSnapshot_(value, out);
        if (strcmp(action, "snapshot_req") == 0)
            return encodePageRequest_(value, out);
    }
    if (strcmp(feature, "controllers") == 0)
    {
        if (strcmp(action, "summary") == 0)
            return encodeControllersSummary_(value, out);
        if (strcmp(action, "summary_req") == 0)
        {
            out.clear();
            appendU8_(out, kSchemaVersion);
            return true;
        }
    }
    if (strcmp(feature, "web") == 0)
    {
        if (strcmp(action, "index_state") == 0)
            return encodeWebIndexState_(value, out);
        if (strcmp(action, "index_state_req") == 0)
        {
            out.clear();
            appendU8_(out, kSchemaVersion);
            return true;
        }
    }
    if (strcmp(feature, "ring") == 0)
    {
        if (strcmp(action, "button") == 0)
            return encodeRingState_(value, "pressed", out);
        if (strcmp(action, "set") == 0)
            return encodeRingState_(value, "state", out);
    }
    if (strcmp(feature, "sockets") == 0)
    {
        if (strcmp(action, "snapshot_req") == 0)
            return encodePageRequest_(value, out);
        if (strcmp(action, "snapshot") == 0)
            return encodeSocketLikeSnapshot_(value, "sockets", "sockets", out);
        if (strcmp(action, "set") == 0 || strcmp(action, "set_lights") == 0)
            return encodeSocketSet_(value, out);
    }
    if (strcmp(feature, "lights") == 0)
    {
        if (strcmp(action, "snapshot_req") == 0)
            return encodePageRequest_(value, out);
        if (strcmp(action, "snapshot") == 0)
            return encodeSocketLikeSnapshot_(value, "lights", "lights", out);
    }
    if (strcmp(feature, "meteo") == 0)
    {
        if (strcmp(action, "snapshot_req") == 0)
            return encodePageRequest_(value, out);
        if (strcmp(action, "snapshot") == 0)
            return encodeMeteoSnapshot_(value, out);
        if (strcmp(action, "set") == 0)
            return encodeMeteoSet_(value, out);
    }
    if (strcmp(feature, "thermo") == 0)
    {
        if (strcmp(action, "snapshot_req") == 0)
            return encodePageRequest_(value, out);
        if (strcmp(action, "snapshot") == 0)
            return encodeThermoSnapshot_(value, out);
        if (strcmp(action, "set") == 0)
            return encodeThermoSet_(value, out);
    }
    if (strcmp(feature, "tanks") == 0)
    {
        if (strcmp(action, "snapshot_req") == 0)
            return encodePageRequest_(value, out);
        if (strcmp(action, "snapshot") == 0)
            return encodeTankSnapshot_(value, out);
        if (strcmp(action, "set") == 0)
            return encodeTanksSet_(value, out);
        if (strcmp(action, "empty") == 0)
            return encodeTankEmpty_(value, out);
    }
    if (strcmp(feature, "leak") == 0)
    {
        if (strcmp(action, "snapshot_req") == 0)
            return encodePageRequest_(value, out);
        if (strcmp(action, "snapshot") == 0)
            return encodeLeakSnapshot_(value, out);
    }
    if (strcmp(feature, "septic") == 0)
    {
        if (strcmp(action, "set") == 0)
            return encodeSepticSet_(value, out);
        if (strcmp(action, "level") == 0)
            return encodeSepticLevel_(value, out);
    }
    if (strcmp(feature, "watering") == 0)
    {
        if (strcmp(action, "snapshot_req") == 0)
            return encodePageRequest_(value, out);
        if (strcmp(action, "snapshot") == 0)
            return encodeWateringSnapshot_(value, out);
        if (strcmp(action, "set") == 0)
            return encodeWateringSet_(value, out);
        if (strcmp(action, "event") == 0)
            return encodeWateringEvent_(value, out);
    }
    if (strcmp(feature, "security") == 0)
    {
        if (strcmp(action, "set") == 0)
            return encodeSecuritySet_(value, out);
        if (strcmp(action, "alarm") == 0)
            return encodeSecurityAlarm_(value, out);
        if (strcmp(action, "rfid") == 0)
            return encodeSecuritySimplePair_(value, "uid", "name", out);
        if (strcmp(action, "ibutton") == 0)
            return encodeSecuritySimplePair_(value, "serial", "name", out);
        if (strcmp(action, "status_req") == 0)
        {
            out.clear();
            appendU8_(out, kSchemaVersion);
            return true;
        }
        if (strcmp(action, "prearm_blocked") == 0)
            return encodeSecurityPrearmBlocked_(value, out);
        if (strcmp(action, "rfid_result") == 0)
            return encodeSecurityResult_(value, "uid", out);
        if (strcmp(action, "ibutton_result") == 0)
            return encodeSecurityResult_(value, "serial", out);
    }
    if (strcmp(feature, "quick_actions") == 0)
    {
        if (strcmp(action, "run") == 0)
            return encodeQuickActionRun_(value, out);
    }
    if (strcmp(feature, "rules") == 0)
    {
        if (strcmp(action, "run") == 0)
            return encodeRuleRun_(value, out);
    }
    return false;
}

bool StackDataBinaryCodec::decode(const char *feature, const char *action, const uint8_t *data, size_t size,
                                  DynamicJsonDocument &out)
{
    if (!feature || !action || !data || size == 0)
        return false;
    if (strcmp(feature, "system") == 0)
    {
        if (strcmp(action, "snapshot") == 0)
            return decodeSystemSnapshot_(data, size, out);
        if (strcmp(action, "snapshot_req") == 0)
            return decodePageRequest_(data, size, out);
    }
    if (strcmp(feature, "controllers") == 0)
    {
        if (strcmp(action, "summary") == 0)
            return decodeControllersSummary_(data, size, out);
        if (strcmp(action, "summary_req") == 0)
        {
            const uint8_t *p = data;
            size_t left = size;
            return readVersion_(p, left) && left == 0 && (out.clear(), true);
        }
    }
    if (strcmp(feature, "web") == 0)
    {
        if (strcmp(action, "index_state") == 0)
            return decodeWebIndexState_(data, size, out);
        if (strcmp(action, "index_state_req") == 0)
        {
            const uint8_t *p = data;
            size_t left = size;
            return readVersion_(p, left) && left == 0 && (out.clear(), true);
        }
    }
    if (strcmp(feature, "ring") == 0)
    {
        if (strcmp(action, "button") == 0)
            return decodeRingState_(data, size, "pressed", out);
        if (strcmp(action, "set") == 0)
            return decodeRingState_(data, size, "state", out);
    }
    if (strcmp(feature, "sockets") == 0)
    {
        if (strcmp(action, "snapshot_req") == 0)
            return decodePageRequest_(data, size, out);
        if (strcmp(action, "snapshot") == 0)
            return decodeSocketLikeSnapshot_(data, size, "sockets", "sockets", out);
        if (strcmp(action, "set") == 0 || strcmp(action, "set_lights") == 0)
            return decodeSocketSet_(data, size, out);
    }
    if (strcmp(feature, "lights") == 0)
    {
        if (strcmp(action, "snapshot_req") == 0)
            return decodePageRequest_(data, size, out);
        if (strcmp(action, "snapshot") == 0)
            return decodeSocketLikeSnapshot_(data, size, "lights", "lights", out);
    }
    if (strcmp(feature, "meteo") == 0)
    {
        if (strcmp(action, "snapshot_req") == 0)
            return decodePageRequest_(data, size, out);
        if (strcmp(action, "snapshot") == 0)
            return decodeMeteoSnapshot_(data, size, out);
        if (strcmp(action, "set") == 0)
            return decodeMeteoSet_(data, size, out);
    }
    if (strcmp(feature, "thermo") == 0)
    {
        if (strcmp(action, "snapshot_req") == 0)
            return decodePageRequest_(data, size, out);
        if (strcmp(action, "snapshot") == 0)
            return decodeThermoSnapshot_(data, size, out);
        if (strcmp(action, "set") == 0)
            return decodeThermoSet_(data, size, out);
    }
    if (strcmp(feature, "tanks") == 0)
    {
        if (strcmp(action, "snapshot_req") == 0)
            return decodePageRequest_(data, size, out);
        if (strcmp(action, "snapshot") == 0)
            return decodeTankSnapshot_(data, size, out);
        if (strcmp(action, "set") == 0)
            return decodeTanksSet_(data, size, out);
        if (strcmp(action, "empty") == 0)
            return decodeTankEmpty_(data, size, out);
    }
    if (strcmp(feature, "leak") == 0)
    {
        if (strcmp(action, "snapshot_req") == 0)
            return decodePageRequest_(data, size, out);
        if (strcmp(action, "snapshot") == 0)
            return decodeLeakSnapshot_(data, size, out);
    }
    if (strcmp(feature, "septic") == 0)
    {
        if (strcmp(action, "set") == 0)
            return decodeSepticSet_(data, size, out);
        if (strcmp(action, "level") == 0)
            return decodeSepticLevel_(data, size, out);
    }
    if (strcmp(feature, "watering") == 0)
    {
        if (strcmp(action, "snapshot_req") == 0)
            return decodePageRequest_(data, size, out);
        if (strcmp(action, "snapshot") == 0)
            return decodeWateringSnapshot_(data, size, out);
        if (strcmp(action, "set") == 0)
            return decodeWateringSet_(data, size, out);
        if (strcmp(action, "event") == 0)
            return decodeWateringEvent_(data, size, out);
    }
    if (strcmp(feature, "security") == 0)
    {
        if (strcmp(action, "set") == 0)
            return decodeSecuritySet_(data, size, out);
        if (strcmp(action, "alarm") == 0)
            return decodeSecurityAlarm_(data, size, out);
        if (strcmp(action, "rfid") == 0)
            return decodeSecuritySimplePair_(data, size, "uid", "name", out);
        if (strcmp(action, "ibutton") == 0)
            return decodeSecuritySimplePair_(data, size, "serial", "name", out);
        if (strcmp(action, "status_req") == 0)
        {
            const uint8_t *p = data;
            size_t left = size;
            return readVersion_(p, left) && left == 0 && (out.clear(), true);
        }
        if (strcmp(action, "prearm_blocked") == 0)
            return decodeSecurityPrearmBlocked_(data, size, out);
        if (strcmp(action, "rfid_result") == 0)
            return decodeSecurityResult_(data, size, "uid", out);
        if (strcmp(action, "ibutton_result") == 0)
            return decodeSecurityResult_(data, size, "serial", out);
    }
    if (strcmp(feature, "quick_actions") == 0)
    {
        if (strcmp(action, "run") == 0)
            return decodeQuickActionRun_(data, size, out);
    }
    if (strcmp(feature, "rules") == 0)
    {
        if (strcmp(action, "run") == 0)
            return decodeRuleRun_(data, size, out);
    }
    return false;
}
