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

class AsyncWebServerRequest;
class WebInterface;

class WebInterfaceStackOps
{
public:
    explicit WebInterfaceStackOps(WebInterface &web) : _web(web) {}

    String indexDeviceSelectHtml_(uint32_t selected_node_id, bool stack_view) const;
    String busesDeviceSelectHtml_(uint32_t selected_node_id, bool stack_view) const;
    String portsDeviceSelectHtml_(uint32_t selected_node_id, bool stack_view) const;
    String stackBusesStatusText_(uint32_t node_id) const;
    String stackPortsStatusText_(uint32_t node_id) const;
    bool isStackBusesView_(uint32_t node_id) const;
    bool isStackPortsView_(uint32_t node_id) const;
    uint32_t parseStackNodeIdParam_(AsyncWebServerRequest *request) const;
    bool requestStackPorts_(uint32_t node_id);
    bool refreshStackPorts_(uint32_t node_id);
    bool requestStackExtenders_(uint32_t node_id);
    bool requestStackI2c_(uint32_t node_id, bool run);
    bool requestStackOw_(uint32_t node_id, bool run);
    bool requestStackTempSensors_(uint32_t node_id);
    bool refreshStackTempSensors_(uint32_t node_id);
    bool requestStackIndexState_(uint32_t node_id);
    bool requestStackPlcStatus_(uint32_t node_id);
    bool requestStackRtcStatus_(uint32_t node_id);
    uint16_t nextStackCmdId_();

private:
    WebInterface &_web;
};
