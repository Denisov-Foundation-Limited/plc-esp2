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

#include "core/network/stack/stack_json_protocol.hpp"

#include <ArduinoJson.h>
#include <string.h>

namespace
{
const char *exchangeKindToString_(StackTransport::ExchangeKind kind)
{
    switch (kind)
    {
    case StackTransport::ExchangeKind::Request:
        return "request";
    case StackTransport::ExchangeKind::Response:
        return "response";
    case StackTransport::ExchangeKind::Event:
    default:
        return "event";
    }
}

StackTransport::ExchangeKind exchangeKindFromString_(const char *value)
{
    if (!value || !value[0])
        return StackTransport::ExchangeKind::Event;
    if (strcmp(value, "request") == 0)
        return StackTransport::ExchangeKind::Request;
    if (strcmp(value, "response") == 0)
        return StackTransport::ExchangeKind::Response;
    return StackTransport::ExchangeKind::Event;
}

bool copyString_(JsonVariantConst value, char *dst, size_t cap)
{
    if (!dst || cap == 0)
        return false;
    dst[0] = '\0';
    if (value.isNull())
        return true;
    const char *src = value.as<const char *>();
    if (!src)
        return false;
    strncpy(dst, src, cap - 1);
    dst[cap - 1] = '\0';
    return true;
}
} // namespace

StackJsonProtocol::MessageKind StackJsonProtocol::detectKind(const uint8_t *data, size_t size)
{
    if (!data || size == 0)
        return MessageKind::Unknown;

    DynamicJsonDocument filter(96);
    filter["type"] = true;
    filter["api_key"] = true;

    DynamicJsonDocument doc(128);
    if (deserializeJson(doc, data, size, DeserializationOption::Filter(filter)))
        return MessageKind::Unknown;

    const char *type = doc["type"] | "";
    if (strcmp(type, "auth") == 0 || doc["api_key"].is<const char *>())
        return MessageKind::Auth;
    if (strcmp(type, "route") == 0)
        return MessageKind::Route;
    if (strcmp(type, "notify") == 0)
        return MessageKind::Notify;
    return MessageKind::Unknown;
}

bool StackJsonProtocol::parseAuth(const uint8_t *data, size_t size, AuthMessage &out)
{
    if (!data || size == 0)
        return false;

    DynamicJsonDocument doc(384);
    if (deserializeJson(doc, data, size))
        return false;

    if (!copyString_(doc["api_key"], out.api_key, sizeof(out.api_key)))
        return false;
    if (!copyString_(doc["name"], out.name, sizeof(out.name)))
        return false;
    if (!copyString_(doc["ip"], out.ip, sizeof(out.ip)))
        return false;

    out.node_id = doc["node_id"] | 0u;
    out.caps = doc["caps"] | 0u;
    out.fw_version = doc["fw_ver"] | 0u;
    return out.node_id != 0 && out.name[0] != '\0';
}

bool StackJsonProtocol::parseRoute(const uint8_t *data, size_t size, RouteMessage &out)
{
    if (!data || size == 0)
        return false;

    out = RouteMessage{};
    auto doc = std::make_shared<DynamicJsonDocument>(3072);
    if (!doc)
        return false;
    if (deserializeJson(*doc, data, size))
        return false;
    if (strcmp((*doc)["type"] | "", "route") != 0)
        return false;
    if (!copyString_((*doc)["feature"], out.feature, sizeof(out.feature)))
        return false;
    if (!copyString_((*doc)["action"], out.action, sizeof(out.action)))
        return false;

    out.source_node = (*doc)["source_node"] | 0u;
    out.target_node = (*doc)["target_node"] | 0u;
    out.meta.exchange_kind = exchangeKindFromString_((*doc)["meta"]["exchange"] | (*doc)["exchange"] | "event");
    out.meta.request_id = (*doc)["meta"]["request_id"] | (*doc)["request_id"] | 0u;
    out.meta.reply_to = (*doc)["meta"]["reply_to"] | (*doc)["reply_to"] | 0u;
    out.meta.expect_response = (*doc)["meta"]["expect_response"] | (*doc)["expect_response"] | false;
    out.payload = "";
    out.payload_storage = doc;
    out.payload_json = (*doc)["payload"].as<JsonVariantConst>();
    if (!out.payload_json.isNull())
        serializeJson(out.payload_json, out.payload);
    return out.feature[0] != '\0' && out.action[0] != '\0';
}

bool StackJsonProtocol::parseRoutePayload(const String &payload, RouteMessage &out)
{
    out.payload = payload;
    out.payload_storage.reset();
    out.payload_json = JsonVariantConst();
    if (!payload.length())
        return true;
    const size_t cap = (payload.length() * 2u) + 256u;
    auto doc = std::make_shared<DynamicJsonDocument>(cap);
    if (!doc)
        return false;
    if (deserializeJson(*doc, payload.c_str(), payload.length()))
        return false;
    out.payload_storage = doc;
    out.payload_json = doc->as<JsonVariantConst>();
    return true;
}

bool StackJsonProtocol::parseNotify(const uint8_t *data, size_t size, NotifyMessage &out)
{
    if (!data || size == 0)
        return false;

    DynamicJsonDocument doc(3072);
    if (deserializeJson(doc, data, size))
        return false;
    if (strcmp(doc["type"] | "", "notify") != 0)
        return false;
    if (!copyString_(doc["level"], out.level, sizeof(out.level)))
        return false;
    if (!copyString_(doc["feature"], out.feature, sizeof(out.feature)))
        return false;
    if (!copyString_(doc["code"], out.code, sizeof(out.code)))
        return false;
    out.source_node = doc["source_node"] | 0u;
    out.message = doc["message"] | "";
    out.payload = "";
    if (doc["payload"].is<JsonVariantConst>())
        serializeJson(doc["payload"], out.payload);
    return out.source_node != 0 && out.level[0] != '\0' && out.feature[0] != '\0';
}

String StackJsonProtocol::makeRoute(uint32_t source_node, uint32_t target_node, const char *feature, const char *action,
                                    const JsonDocument *payload, const StackTransport::RouteMeta *meta)
{
    return makeRoute(source_node, target_node, feature, action,
                     payload ? payload->as<JsonVariantConst>() : JsonVariantConst(),
                     meta);
}

String StackJsonProtocol::makeRoute(uint32_t source_node, uint32_t target_node, const char *feature, const char *action,
                                    JsonVariantConst payload, const StackTransport::RouteMeta *meta)
{
    DynamicJsonDocument doc(3072);
    doc["type"] = "route";
    doc["source_node"] = source_node;
    doc["target_node"] = target_node;
    doc["feature"] = feature ? feature : "";
    doc["action"] = action ? action : "";
    JsonObject meta_obj = doc.createNestedObject("meta");
    meta_obj["exchange"] = exchangeKindToString_(meta ? meta->exchange_kind : StackTransport::ExchangeKind::Event);
    meta_obj["request_id"] = meta ? meta->request_id : 0u;
    meta_obj["reply_to"] = meta ? meta->reply_to : 0u;
    meta_obj["expect_response"] = meta ? meta->expect_response : false;
    if (!payload.isNull())
        doc["payload"].set(payload);
    else
        doc["payload"].to<JsonObject>();
    String out;
    serializeJson(doc, out);
    return out;
}

String StackJsonProtocol::makeNotify(uint32_t source_node, const char *level, const char *feature, const char *code,
                                     const char *message, const JsonDocument *payload)
{
    DynamicJsonDocument doc(768);
    doc["type"] = "notify";
    doc["source_node"] = source_node;
    doc["level"] = (level && level[0]) ? level : "info";
    doc["feature"] = feature ? feature : "";
    doc["code"] = code ? code : "";
    if (message && message[0])
        doc["message"] = message;
    if (payload)
        doc["payload"].set(payload->as<JsonVariantConst>());
    else
        doc["payload"].to<JsonObject>();
    String out;
    serializeJson(doc, out);
    return out;
}

String StackJsonProtocol::makeOk(const char *message)
{
    DynamicJsonDocument doc(128);
    doc["ok"] = true;
    if (message && message[0])
        doc["message"] = message;
    String out;
    serializeJson(doc, out);
    return out;
}

String StackJsonProtocol::makeError(const char *message)
{
    DynamicJsonDocument doc(160);
    doc["ok"] = false;
    doc["error"] = (message && message[0]) ? message : "error";
    String out;
    serializeJson(doc, out);
    return out;
}
