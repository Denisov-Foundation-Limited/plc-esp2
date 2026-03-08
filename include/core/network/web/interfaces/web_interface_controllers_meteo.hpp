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
#include "controllers/meteo_controller.hpp"

class WebInterface;
class WebInterfaceControllersMeteoHelper;

class WebInterfaceControllersMeteoHelper
{
public:
    static size_t meteoLocalRenderCount_(const WebInterface &web);

    static String meteoDeviceSelectHtml_(const WebInterface &web, uint32_t selected_node_id, bool stack_view);

    static String stackMeteoStatusText_(const WebInterface &web, uint32_t node_id);

    static bool isStackMeteoView_(const WebInterface &web, uint32_t node_id);

    static bool requestStackMeteo_(WebInterface &web, uint32_t node_id);

    static size_t stackMeteoVisibleCount_(const WebInterface &web, uint32_t node_id);

    static String listStackMeteoHtml_(WebInterface &web, uint32_t node_id, size_t offset, size_t limit);

    static String listMeteoHtml_(WebInterface &web, size_t offset, size_t limit);

    static String listMeteoHtml_(WebInterface &web);

    static String meteoPortOptionsJson_(const WebInterface &web);

    static String meteoUsedPinsJson_(const WebInterface &web);

    static String meteoSensorOptionsHtml_(const WebInterface &web, uint8_t selected_id, uint32_t selected_node_id,
                                          const uint8_t used_local[MeteoController::kSensorCount + 1],
                                          const uint32_t *used_remote, size_t used_remote_count);

    static String meteoRemoteSensorOptionsHtml_(const WebInterface &web, uint8_t selected_id, uint32_t selected_node_id);

    static String meteoRemoteNodeOptionsHtml_(const WebInterface &web, uint32_t selected_node_id);

    static String meteoRemoteLabel_(const WebInterface &web, uint32_t node_id, uint8_t sensor_id);

    static String meteoRemoteSensorName_(const WebInterface &web, uint32_t node_id, uint8_t sensor_id);

    static bool meteoRemoteType_(const WebInterface &web, uint32_t node_id, uint8_t sensor_id, MeteoController::SensorType &out);

    static bool isMeteoSensorActive_(const WebInterface &web, uint8_t id);

    static bool isRemoteMeteoSensorActive_(const WebInterface &web, uint32_t node_id, uint8_t id);
};
