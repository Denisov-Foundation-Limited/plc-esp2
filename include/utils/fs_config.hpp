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

#include <stdint.h>

namespace FsConfig
{
    static constexpr const char *kPartitionLabel = "littlefs";
    static constexpr const char *kBasePath = "/littlefs";
    static constexpr uint8_t kMaxOpenFiles = 10;
}
