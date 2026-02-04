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

static const char kWebInterfaceRfidHtml[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>RFID</title>
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
    .wrap { max-width: 720px; margin: 40px auto; padding: 0 16px; }
    .card {
      background: linear-gradient(180deg, #0f172a 0%, #0b1220 100%);
      border: 1px solid #1f2937;
      border-radius: 14px;
      padding: 22px;
      box-shadow: 0 10px 30px rgba(0,0,0,0.35);
    }
    h1 { margin: 0 0 6px; font-size: 22px; }
    a { color: #7dd3fc; text-decoration: none; }
    .nav { margin-bottom: 12px; }
    .row { display: flex; align-items: center; gap: 12px; flex-wrap: wrap; }
    .label { color: var(--muted); font-size: 12px; text-transform: uppercase; letter-spacing: .04em; }
    .status { color: var(--muted); font-size: 12px; margin-bottom: 10px; }
    .badge { padding: 4px 10px; border-radius: 999px; font-size: 12px; font-weight: 600; }
    .badge.on { background: #14532d; color: #dcfce7; }
    .badge.off { background: #2f2f2f; color: #d1d5db; }
    .field {
      width: 100%;
      background: #0b1220;
      border: 1px solid #334155;
      color: var(--text);
      padding: 10px;
      border-radius: 8px;
    }
    .btn {
      background: var(--accent);
      color: #00111a;
      border: none;
      padding: 10px 16px;
      border-radius: 8px;
      font-weight: 700;
      cursor: pointer;
    }
    .actions { margin-top: 12px; }
    .toggle {
      display: flex;
      align-items: center;
      gap: 10px;
    }
    .switch {
      display: inline-block;
      width: 40px;
      height: 20px;
      vertical-align: middle;
      flex: 0 0 auto;
    }
    .switch input { display: none; }
    .track {
      display: flex;
      align-items: center;
      width: 100%;
      height: 100%;
      padding: 2px;
      background: #64748b;
      border-radius: 999px;
      border: 1px solid #1f2937;
      transition: .2s;
    }
    .knob {
      width: 16px;
      height: 16px;
      border-radius: 50%;
      background: #0b1220;
      transition: .2s;
      box-shadow: 0 0 0 1px rgba(0,0,0,0.3);
    }
    input:checked + .track { background: #22c55e; }
    input:checked + .track .knob { transform: translateX(20px); }
    .key-box {
      display: inline-flex;
      align-items: center;
      gap: 10px;
      padding: 10px 14px;
      border-radius: 10px;
      border: 1px solid #1f2937;
      background: linear-gradient(135deg, #0b1220 0%, #111827 100%);
      box-shadow: inset 0 0 0 1px rgba(255,255,255,0.02), 0 4px 12px rgba(0,0,0,0.25);
    }
    .key-chip {
      width: 8px;
      height: 8px;
      border-radius: 999px;
      background: #22c55e;
      box-shadow: 0 0 10px rgba(34,197,94,0.7);
    }
    .key-value {
      font-family: "Consolas","Courier New",monospace;
      letter-spacing: .06em;
      font-size: 15px;
    }
    @media (max-width: 720px) {
      .wrap { margin: 20px auto; }
    }
  </style>
</head>
<body>
  <div class="wrap">
    <div class="card">
      %NAV%
      <h1>RFID</h1>
      <div class="status">%RFID_STATUS%</div>
      <div class="row toggle">
        <label class="switch">
          <input type="checkbox" name="rfid_enabled" form="rfid-form" %RFID_ENABLED_CHECKED%>
          <span class="track"><span class="knob"></span></span>
        </label>
        <span>Включить RFID reader</span>
        <span class="badge %RFID_ACTIVE_CLASS%">%RFID_ACTIVE_LABEL%</span>
      </div>
      <div class="row">
        <div class="label">последний ключ</div>
        <span class="key-box">
          <span class="key-chip"></span>
          <span class="key-value">%RFID_LAST_UID%</span>
        </span>
      </div>
      <form method="POST" action="/rfid" id="rfid-form">
        <div class="actions">
          <button class="btn" type="submit">Сохранить</button>
        </div>
      </form>
    </div>
  </div>
</body>
</html>
)HTML";
