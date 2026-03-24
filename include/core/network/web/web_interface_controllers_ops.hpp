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

#include <Arduino.h>

#include "core/display_slots.hpp"
#include "controllers/meteo_controller.hpp"
#include "controllers/security_controller.hpp"
#include "controllers/thermo_controller.hpp"
#include "hal/gpio/portio.hpp"
#include "utils/configs_manager_iface.hpp"
#include "utils/users_registry.hpp"

class AsyncWebServerRequest;
class WebInterface;

class WebInterfaceControllersOps
{
public:
    explicit WebInterfaceControllersOps(WebInterface &web) : _web(web) {}

    String listPortsHtml_();

    String listExtendersHtml_();

    String listStackPortsHtml_(uint32_t node_id) const;

    String listStackExtendersHtml_(uint32_t node_id) const;

    String listI2cHtml_();

    String listStackI2cHtml_(uint32_t node_id) const;

    String stackNodesBlockHtml_() const;

    String listStackNodesStatusHtml_() const;

    String listStackNodesHtml_() const;

    String globalUsedPortsJson_(PortIO::PinType type) const;

    String stackPortOptionsJson_(uint32_t node_id, PortIO::PinType type) const;

    String stackUsedPortsJson_(uint32_t node_id, PortIO::PinType type) const;

    String stackMeteoDs18OptionsJson_(uint32_t node_id) const;

    String stackMeteoDs18UsedJson_(uint32_t node_id) const;

    String listOwHtml_();

    String listStackOwHtml_(uint32_t node_id) const;

    void handleUpload_(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data,
                       size_t len, bool final);

    void handleOta_(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len,
                    bool final);

    void handleUploadDone_(AsyncWebServerRequest *request);

    void handleOtaDone_(AsyncWebServerRequest *request);

    void handleWifiSave_(AsyncWebServerRequest *request);

    void handleStackSave_(AsyncWebServerRequest *request);

    void handleStackGenKey_(AsyncWebServerRequest *request);

    void handleDeviceSave_(AsyncWebServerRequest *request);

    void handleReboot_(AsyncWebServerRequest *request);

    void handleFileDownload_(AsyncWebServerRequest *request);

    void handleDelete_(AsyncWebServerRequest *request);

    void handleUiHash_(AsyncWebServerRequest *request);

    bool hasUsersRegistryWebAuth_() const;

    bool checkLegacyAdminAuth_(const String &user, const String &pass) const;

    bool findUsersRegistryAuth_(const String &user, const String &pass, size_t &user_idx) const;

    bool checkUsersRegistryAuth_(const String &user, const String &pass) const;

    bool checkAuth_(AsyncWebServerRequest *request, bool *set_cookie, bool require_session = false);

    bool checkAuthApi_(AsyncWebServerRequest *request, bool *set_cookie);

    const UsersRegistry::User *sessionUser_() const;

    uint8_t aclUnitByNodeId_(uint32_t node_id) const;

    bool webAclControllerAllowed_(UsersRegistry::AclController ctrl, uint32_t node_id = 0) const;

    bool webAclCanViewItem_(UsersRegistry::AclController ctrl, uint16_t item_id, uint32_t node_id = 0) const;

    bool webAclCanControlItem_(UsersRegistry::AclController ctrl, uint16_t item_id, uint32_t node_id = 0) const;

    bool webSessionIsAdmin_() const;

    bool requireWebAdmin_(AsyncWebServerRequest *request, bool *set_cookie);

    bool requireWebAclController_(AsyncWebServerRequest *request, bool *set_cookie,
                                  UsersRegistry::AclController ctrl, uint32_t node_id = 0);

    String requestIp_(AsyncWebServerRequest *request) const;

    String wifiIp_() const;

    String wifiStaSegment_() const;

    String wifiStaStatus_() const;

    String navHtml_() const;

    String deviceName_() const;

    ConfigsManagerIface::StackRole stackRole_() const;

    String stackMasterHost_() const;

    ConfigsManagerIface::StackExchangePolicy stackExchangePolicy_() const;

    ConfigsManagerIface::StackTransportKind stackTransport_() const;

    ConfigsManagerIface::StackPayloadMode stackPayloadMode_() const;

    bool stackFallbackEnabled_() const;

    String stackFallbackHost_() const;

    bool stackSlaveController_() const;

    String stackApiKey_() const;

    bool cloudEnabled_() const;

    CloudTransportKind cloudTransport_() const;

    String cloudHost_() const;

    uint16_t cloudPort_() const;

    String cloudPath_() const;

    bool cloudUseSsl_() const;

    uint32_t cloudReconnectMs_() const;

    uint32_t cloudEventMs_() const;

    String cloudApiKey_() const;

    String cloudFwVersion_() const;

    uint32_t cloudDeviceId_() const;

    bool cloudConnected_() const;

    const char *stackRoleName_(ConfigsManagerIface::StackRole role);

    bool saveWifiConfig_();

    bool isAllowedExt_(const String &path) const;

    bool parseSocketPort_(const String &input, uint8_t &out);

    const char *displaySlotKindName_(DisplaySlotKind kind) const;

    const char *displaySlotFieldName_(DisplaySlotField field) const;

    bool parseDisplaySlotKind_(const String &input, DisplaySlotKind &out);

    bool parseDisplaySlotField_(const String &input, DisplaySlotField &out);

    bool parseUint_(const String &input, uint16_t &out);

    bool parseUint_(const String &input, uint32_t &out);

    bool parseMeteoType_(const String &input, MeteoController::SensorType &out);

    MeteoController::SensorType parseMeteoTypeName_(const char *input) const;

    bool parseSecurityType_(const String &input, SecurityController::SensorType &out);

    bool parseMeteoPin_(const String &input, uint8_t &out);

    bool parseMeteoAddr_(const String &input, uint8_t out[MeteoController::kAddrLen], bool &set);

    bool parseSecurityKeyHex_(const String &s, uint8_t out[8]);

    bool parseThermoSensor_(const String &input, uint8_t &out, uint32_t &out_node);

    bool parseThermoMode_(const String &input, ThermoController::Mode &out);

    bool parseThermoFloat_(const String &input, float &out);

private:
    WebInterface &_web;
};
