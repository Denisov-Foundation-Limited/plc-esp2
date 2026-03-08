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

#include <Arduino.h>
#include "core/network/web/pages/web_page_status.hpp"

const char kWebInterfaceStatusHtml[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>%STATUS_PAGE_TITLE%</title>
  <style>
    html, body { height: 100%; }
    body {
      margin: 0;
      min-height: 100vh;
      font-family: "Segoe UI", Tahoma, Arial, sans-serif;
      background: radial-gradient(1200px 600px at 10% -10%, #1f2937 0%, #0b1220 60%, #080d17 100%);
      background-repeat: no-repeat;
      background-size: cover;
      background-attachment: fixed;
      color:#e5e7eb;
    }
    .wrap { max-width: 560px; margin: 40px auto; padding: 0 16px; }
    .card { background:#0f172a; border:1px solid #1f2937; border-radius:12px; padding:20px; }
    a { color:#7dd3fc; text-decoration:none; }
    .nav { margin-bottom: 12px; }
  </style>
</head>
<body>
  <div class="wrap">
    <div class="card">
      %NAV%
      <h2>%STATUS_TITLE%</h2>
      <pre><strong>%STATUS%</strong></pre>
      
    </div>
  </div>
</body>
</html>
)HTML";


