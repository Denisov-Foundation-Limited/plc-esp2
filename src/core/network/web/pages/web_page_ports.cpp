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
#include "core/network/web/pages/web_page_ports.hpp"

const char kWebInterfacePortsHtml[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>%PORTS_PAGE_TITLE%</title>
  <style>
    :root {
      --bg: #0f172a;
      --card: #111827;
      --accent: #38bdf8;
      --text: #e5e7eb;
      --muted: #94a3b8;
    }
    * { box-sizing: border-box; }
    html, body { height: 100%; }
    body {
      margin: 0;
      min-height: 100vh;
      font-family: "Segoe UI", Tahoma, Arial, sans-serif;
      background: radial-gradient(1200px 600px at 10% -10%, #1f2937 0%, #0b1220 60%, #080d17 100%);
      background-repeat: no-repeat;
      background-size: cover;
      background-attachment: fixed;
      color: var(--text);
    }
    .wrap { max-width: 900px; margin: 40px auto; padding: 0 16px; }
    .card {
      background: linear-gradient(180deg, #0f172a 0%, #0b1220 100%);
      border: 1px solid #1f2937;
      border-radius: 14px;
      padding: 22px;
      box-shadow: 0 10px 30px rgba(0,0,0,0.35);
    }
    h1 { margin: 0 0 6px; font-size: 22px; }
    h2 { margin: 16px 0 8px; font-size: 18px; }
    p { margin: 0 0 18px; color: var(--muted); }
    a { color: #7dd3fc; text-decoration: none; }
    table { width: 100%; border-collapse: collapse; margin-top: 10px; }
    th, td { text-align: left; padding: 8px 6px; border-bottom: 1px solid #1f2937; }
    th { color: var(--muted); font-weight: 600; }
    .right { text-align: right; }
    .nav { margin-bottom: 12px; }
    .row { display: flex; gap: 10px; align-items: center; }
    .status { margin: 8px 0 10px; color: var(--muted); }
    .field {
      width: 100%;
      background: #0b1220;
      border: 1px solid #334155;
      color: var(--text);
      padding: 8px;
      border-radius: 8px;
    }
    .field:disabled,
    select.field:disabled,
    input.field[readonly] {
      color: var(--muted);
      -webkit-text-fill-color: var(--muted);
      background: #0a1220;
      border-color: #1a2436;
      cursor: not-allowed;
    }    select {
      width: 100%;
      background: #0b1220;
      border: 1px solid #334155;
      color: var(--text);
      padding: 8px;
      border-radius: 8px;
    }
    .mini { width: 88px; }
  </style>
</head>
<body>
  <div class="wrap">
    <div class="card">
      %NAV%
      <h1>%PORTS_TITLE%</h1>
      <div class="status">%PORTS_STACK_STATUS%</div>
      
      %PORTS_DEVICE_SELECT%
      <h2>%PORTS_EXTENDERS%</h2>
      <table>
        <thead>
          <tr>
            <th class="right">ID</th>
            <th class="right">%PORTS_COL_BUS%</th>
            <th>%PORTS_COL_ADDR%</th>
            <th>%PORTS_COL_TYPE%</th>
          </tr>
        </thead>
        <tbody>
          %EXTENDERS%
        </tbody>
      </table>
      <h2>%PORTS_SECTION_PORTS%</h2>
      <table>
        <thead>
          <tr>
            <th class="right">ID</th>
            <th>%PORTS_COL_BACKEND%</th>
            <th>%PORTS_COL_LOCAL%</th>
            <th>%PORTS_COL_TYPE2%</th>
            <th>%PORTS_COL_CTRL%</th>
            <th class="right">%PORTS_COL_DEV%</th>
            <th class="right">%PORTS_COL_PIN%</th>
            <th>State</th>
            <th>HW</th>
          </tr>
        </thead>
        <tbody>
          %PORTS%
        </tbody>
      </table>
    </div>
  </div>
</body>
</html>
)HTML";






