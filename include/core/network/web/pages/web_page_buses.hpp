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

static const char kWebInterfaceBusesHtml[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Шины</title>
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
    .btn {
      background: var(--accent);
      color: #00111a;
      border: none;
      padding: 8px 12px;
      border-radius: 8px;
      font-weight: 700;
      text-decoration: none;
      display: inline-block;
    }
    select {
      width: 100%;
      background: #0b1220;
      border: 1px solid #334155;
      color: var(--text);
      padding: 8px;
      border-radius: 8px;
    }
  </style>
</head>
<body>
  <div class="wrap">
    <div class="card">
      %NAV%
      <h1>Шины</h1>
      <div class="status">%BUS_STACK_STATUS%</div>
      %BUS_DEVICE_SELECT%
      <h2>I2C</h2>
      <div class="row" style="gap:8px; margin-bottom:8px;">
        <a class="btn" href="%BUS_I2C_SCAN_URL%">Сканировать I2C</a>
      </div>
      <table>
        <thead>
          <tr>
            <th class="right">Bus</th>
            <th>Addr</th>
          </tr>
        </thead>
        <tbody>
          %I2C%
        </tbody>
      </table>
      <h2>OneWire</h2>
      <div class="row" style="gap:8px; margin-bottom:8px;">
        <a class="btn" href="%BUS_OW_SCAN_URL%">Сканировать OW</a>
      </div>
      <table>
        <thead>
          <tr>
            <th class="right">Bus</th>
            <th>Type</th>
            <th>Addr</th>
          </tr>
        </thead>
        <tbody>
          %OW%
        </tbody>
      </table>
    </div>
  </div>
</body>
</html>
)HTML";
