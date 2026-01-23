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

static const char kWebInterfaceWifiHtml[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Сеть</title>
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
    a { color: #7dd3fc; text-decoration: none; }
    .nav { margin-bottom: 12px; }
    table.status-table { width: 100%; border-collapse: collapse; margin: 8px 0 18px; }
    table.status-table td { padding: 6px; border-bottom: 1px solid #1f2937; }
  </style>
</head>
<body>
  <div class="wrap">
    <div class="card">
      %NAV%
      <h1>Сеть</h1>
      <table class="status-table">
        <tbody>
          <tr><td>Режим</td><td><strong>%WIFI_MODE%</strong></td></tr>
          <tr><td>SSID</td><td><strong>%WIFI_CUR_SSID%</strong></td></tr>
          <tr><td>IP</td><td><strong>%WIFI_IP%</strong></td></tr>
          %WIFI_STA_ROW%
        </tbody>
      </table>
      <form method="POST" action="/wifi">
        <div class="grid">
          <div>
            <label>Режим</label>
            <select name="mode">
              <option value="sta" %WIFI_STA_SEL%>STA</option>
              <option value="ap" %WIFI_AP_SEL%>AP</option>
            </select>
          </div>
          <div>
            <label>SSID</label>
            <input type="text" name="ssid" value="%WIFI_SSID%" placeholder="SSID STA">
          </div>
          <div>
            <label>Пароль</label>
            <input type="password" name="password" placeholder="Введите пароль для STA">
          </div>
          <div id="ap-ssid-field">
            <label>AP SSID</label>
            <input type="text" name="ap_ssid" value="%WIFI_AP_SSID%" placeholder="SSID AP">
          </div>
          <div id="ap-pass-field">
            <label>Пароль AP</label>
            <input type="password" name="ap_password" placeholder="Введите пароль для AP">
          </div>
        </div>
        <div class="section">
          <h2>GSM</h2>
          <div class="row" style="margin-bottom:10px;">
            <label style="margin-right:8px;">Включен</label>
            <input type="checkbox" name="gsm_enabled" %GSM_ENABLED_CHECKED%>
            <span class="status">%GSM_ENABLED_LABEL%</span>
          </div>
          <table class="status-table">
            <tbody>
              <tr><td>Состояние</td><td><strong>%GSM_STARTED_LABEL%</strong></td></tr>
              <tr><td>IMEI</td><td><strong>%GSM_IMEI%</strong></td></tr>
              <tr><td>IMSI</td><td><strong>%GSM_IMSI%</strong></td></tr>
              <tr><td>Оператор</td><td><strong>%GSM_OPERATOR%</strong></td></tr>
              <tr><td>Сигнал</td><td><strong>%GSM_SIGNAL%</strong></td></tr>
              <tr><td>Регистрация</td><td><strong>%GSM_REG_STATUS%</strong></td></tr>
              <tr><td>Ошибка</td><td><strong>%GSM_LAST_ERROR%</strong></td></tr>
              <tr><td>Последний URC</td><td><strong>%GSM_LAST_URC%</strong></td></tr>
              <tr><td>Последний SMS</td><td><strong>%GSM_LAST_SMS%</strong></td></tr>
              <tr><td>Последний звонок</td><td><strong>%GSM_LAST_CALL%</strong></td></tr>
              <tr><td>Последний USSD</td><td><strong>%GSM_LAST_USSD%</strong></td></tr>
              <tr><td>HTTP status</td><td><strong>%GSM_HTTP_STATUS%</strong></td></tr>
              <tr><td>HTTP len</td><td><strong>%GSM_HTTP_LEN%</strong></td></tr>
            </tbody>
          </table>
        </div>
        <div class="row" style="margin-top:10px;">
          <button type="submit">Сохранить</button>
          <span class="status">%WIFI_STATUS%</span>
          <span class="status">%GSM_STATUS%</span>
        </div>
      </form>
    </div>
  </div>
  <script>
    const modeSelect = document.querySelector('select[name="mode"]');
    const apSsid = document.getElementById('ap-ssid-field');
    const apPass = document.getElementById('ap-pass-field');
    function updateApFields() {
      if (!modeSelect) return;
      const show = modeSelect.value === 'ap';
      if (apSsid) apSsid.style.display = show ? '' : 'none';
      if (apPass) apPass.style.display = show ? '' : 'none';
    }
    if (modeSelect) {
      modeSelect.addEventListener('change', updateApFields);
      updateApFields();
    }
  </script>
</body>
</html>
)HTML";
