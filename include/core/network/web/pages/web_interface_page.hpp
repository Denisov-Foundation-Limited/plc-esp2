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

static const char kWebInterfaceIndexHtml[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>FCPLC</title>
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
    p { margin: 0 0 18px; color: var(--muted); }
    .section { margin-top: 18px; }
    .row { display: flex; gap: 10px; align-items: center; }
    .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
    label { display: block; margin-bottom: 4px; font-size: 12px; color: var(--muted); }
    input[type=text], input[type=password], select {
      width: 100%;
      background: #0b1220;
      border: 1px solid #334155;
      color: var(--text);
      padding: 10px;
      border-radius: 8px;
    }
    button {
      background: var(--accent);
      color: #00111a;
      border: none;
      padding: 10px 16px;
      border-radius: 8px;
      font-weight: 700;
      cursor: pointer;
    }
    .status { color: var(--muted); font-size: 12px; }
    .notice {
      margin: 6px 0 14px;
      padding: 8px 10px;
      border-radius: 8px;
      background: #0b1220;
      border: 1px solid #334155;
      color: var(--text);
      font-size: 12px;
    }
    .notice:empty { display: none; }
    a { color: #7dd3fc; text-decoration: none; }
    .nav { margin-bottom: 12px; }
    table { width: 100%; border-collapse: collapse; margin-top: 8px; }
    th, td { text-align: left; padding: 6px; border-bottom: 1px solid #1f2937; }
    th { color: var(--muted); font-weight: 600; }
    .status-dot {
      display: inline-block;
      width: 10px;
      height: 10px;
      border-radius: 50%;
      box-shadow: 0 0 0 2px rgba(15, 23, 42, 0.6);
    }
    .status-on { background: #22c55e; }
    .status-off { background: #64748b; }
  </style>
</head>
<body>
  <div class="wrap">
    <div class="card">
      %NAV%
      <h1>FCPLC</h1>
      <p class="status">Плата: <strong>%BOARD_NAME%</strong></p>
      <p>Управление контроллером FCPLC</p>
      <div class="section">
        <h2>Статус</h2>
        <table>
          <tbody>
            <tr><td>Имя устройства</td><td><strong>%DEVICE_NAME%</strong></td></tr>
            <tr><td>Дата</td><td><strong>%RTC_DATE%</strong></td></tr>
            <tr><td>Время</td><td><strong>%RTC_TIME%</strong></td></tr>
            <tr><td>RTC температура</td><td><strong>%RTC_TEMP%</strong></td></tr>
            <tr><td>Температура платы</td><td><strong>%BOARD_TEMP%</strong></td></tr>
            <tr><td>CPU</td><td><strong>%CPU_TEMP%</strong></td></tr>
            <tr><td>Вентилятор</td><td>%FAN_STATUS_ICON%</td></tr>
          </tbody>
        </table>
      </div>
      <div class="section">
        <h2>Имя устройства</h2>
        <form method="POST" action="/device">
          <div class="row">
            <input type="text" name="device_name" value="%DEVICE_NAME%" placeholder="FCPLC">
            <button type="submit">Сохранить</button>
          </div>
          <div class="status">%DEVICE_STATUS%</div>
        </form>
      </div>
      <div class="section">
        <h2>Система</h2>
        <form method="POST" action="/reboot">
          <div class="row">
            <button type="submit">Перезагрузить</button>
          </div>
        </form>
      </div>
    </div>
  </div>
</body>
</html>
)HTML";
