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

static const char kWebInterfaceClientsHtml[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Клиенты</title>
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
    .wrap { max-width: 980px; margin: 40px auto; padding: 0 16px; }
    .card {
      background: linear-gradient(180deg, #0f172a 0%, #0b1220 100%);
      border: 1px solid #1f2937;
      border-radius: 14px;
      padding: 22px;
      box-shadow: 0 10px 30px rgba(0,0,0,0.35);
    }
    h1 { margin: 0 0 6px; font-size: 22px; }
    p { margin: 0 0 18px; color: var(--muted); }
    a { color: #7dd3fc; text-decoration: none; }
    .nav { margin-bottom: 12px; }
    .tile-head { display: flex; align-items: center; justify-content: space-between; gap: 10px; }
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
    .status { color: var(--muted); font-size: 12px; }
    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
      gap: 12px;
      margin-top: 12px;
    }
    .tile {
      display: block;
      padding: 16px;
      border-radius: 12px;
      background: #0b1220;
      border: 1px solid #1f2937;
      color: var(--text);
    }
    .tile > span {
      display: block;
      margin-top: 6px;
      color: var(--muted);
      font-size: 12px;
    }
    .field {
      width: 100%;
      padding: 6px 8px;
      border-radius: 8px;
      border: 1px solid #1f2937;
      background: #0b1220;
      color: var(--text);
    }
    .mini { width: 88px; }
    @media (max-width: 720px) {
      .wrap { margin: 20px auto; }
      .card { padding: 16px; }
      h1 { font-size: 20px; }
      .grid { grid-template-columns: 1fr; }
    }
  </style>
</head>
<body>
  <div class="wrap">
    <div class="card">
      %NAV%
      <h1>Клиенты</h1>
      <p class="status">%CLIENTS_STATUS%</p>
      <div class="grid">
        %CLIENTS_TILES%
      </div>
    </div>
  </div>
  <script>
    const rfidToggle = document.getElementById('rfid-enabled');
    const rfidForm = document.getElementById('rfid-form');
    if (rfidToggle && rfidForm) {
      rfidToggle.addEventListener('change', () => rfidForm.submit());
    }
    const ringToggle = document.getElementById('ring-client-enabled');
    const ringForm = document.getElementById('ring-client-form');
    if (ringToggle && ringForm) {
      ringToggle.addEventListener('change', () => ringForm.submit());
    }
  </script>
</body>
</html>
)HTML";
