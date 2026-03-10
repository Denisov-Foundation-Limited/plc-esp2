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
#include "core/network/web/pages/web_page_admin.hpp"

const char kWebInterfaceAdminHtml[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>%ADMIN_PAGE_TITLE%</title>
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
    .wrap { max-width: 620px; margin: 40px auto; padding: 0 16px; }
    .card {
      background: linear-gradient(180deg, #0f172a 0%, #0b1220 100%);
      border: 1px solid #1f2937;
      border-radius: 14px;
      padding: 22px;
      box-shadow: 0 10px 30px rgba(0,0,0,0.35);
    }
    h1 { margin: 0 0 6px; font-size: 22px; }
    p { margin: 0 0 18px; color: var(--muted); }
    label { display: block; margin-bottom: 6px; font-size: 12px; color: var(--muted); }
    input[type=password], input[type=date], input[type=time], input[type=text] {
      width: 100%;
      background: #0b1220;
      border: 1px solid #334155;
      color: var(--text);
      padding: 10px;
      border-radius: 8px;
      box-shadow: none;
    }
    input[type=password]:focus,
    input[type=date]:focus,
    input[type=time]:focus,
    input[type=text]:focus {
      outline: none;
      border-color: #3b82f6;
      box-shadow: 0 0 0 2px rgba(59, 130, 246, 0.2);
      background: #0b1220;
    }
    input:-webkit-autofill,
    input:-webkit-autofill:hover,
    input:-webkit-autofill:focus,
    input:-webkit-autofill:active {
      -webkit-text-fill-color: var(--text);
      box-shadow: 0 0 0 1000px #0b1220 inset;
      transition: background-color 9999s ease-out;
    }
    button {
      margin-top: 12px;
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
    .section { margin-top: 18px; padding-top: 12px; border-top: 1px solid #1f2937; }
    .row { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; }
    .check-list { display: grid; gap: 10px; margin-top: 10px; }
    .check-list.inline-pairs { grid-template-columns: repeat(2, max-content); column-gap: 18px; align-items: center; }
    .check-item { display:flex; align-items:center; gap:8px; color: var(--text); }
    .section-title { margin: 0 0 6px; font-size: 15px; font-weight: 700; color: var(--text); }
    @media (max-width: 520px) {
      .row { grid-template-columns: 1fr; }
      .check-list.inline-pairs { grid-template-columns: 1fr; }
    }
  </style>
</head>
<body>
  <div class="wrap">
    <div class="card">
      %NAV%
      <h1>%ADMIN_PAGE_TITLE%</h1>
      <p class="status">%ADMIN_STATUS_LABEL% <strong>%ADMIN_STATUS%</strong></p>
      <form method="POST" action="/admin" id="admin-form">
        <label for="password">%ADMIN_NEW_PASSWORD%</label>
        <input id="password" type="password" name="password" placeholder="%ADMIN_NEW_PASSWORD_PLACEHOLDER%">
        <label style="margin-top:10px;display:flex;align-items:center;gap:8px;color:var(--text)">
          <input type="hidden" name="buzzer_present" value="1">
          <input type="checkbox" name="buzzer_enabled" %BUZZER_CHECKED%>
          Buzzer
        </label>
        <div class="section">
          <p class="section-title">EEPROM</p>
          <input type="hidden" name="eeprom_present" value="1">
          <div class="check-list inline-pairs">
            <label class="check-item">
              <input type="checkbox" name="eeprom_save" %EEPROM_SAVE_CHECKED%>
              %EEPROM_SAVE_TEXT%
            </label>
            <label class="check-item">
              <input type="checkbox" name="eeprom_load" %EEPROM_LOAD_CHECKED%>
              %EEPROM_LOAD_TEXT%
            </label>
          </div>
        </div>
        <button type="submit">%SAVE_TEXT%</button>
      </form>
      <div class="section">
        <p class="status">%ADMIN_RTC_NOW% <strong>%RTC_DATE%</strong> <strong>%RTC_TIME%</strong></p>
        <form method="POST" action="/admin" id="rtc-form">
          <div class="row">
            <div>
              <label for="rtc-date">%ADMIN_DATE%</label>
              <input id="rtc-date" type="date" name="rtc_date" value="%RTC_DATE_VAL%">
            </div>
            <div>
              <label for="rtc-time">%ADMIN_TIME%</label>
              <input id="rtc-time" type="time" step="1" name="rtc_time" value="%RTC_TIME_VAL%">
            </div>
          </div>
          <button type="submit">%SAVE_RTC_TEXT%</button>
        </form>
      </div>

      <div class="section">
        <form method="POST" action="/reboot">
          <button type="submit">%ADMIN_REBOOT%</button>
        </form>
      </div>
    </div>
  </div>
  <script>
    const adminForm = document.getElementById('admin-form');
    const rtcForm = document.getElementById('rtc-form');
    const reloadKey = 'admin_reload';
    if (sessionStorage.getItem(reloadKey)) {
      sessionStorage.removeItem(reloadKey);
      location.replace(location.pathname);
    }
    if (adminForm) {
      adminForm.addEventListener('submit', () => {
        sessionStorage.setItem(reloadKey, '1');
      });
    }
    if (rtcForm) {
      rtcForm.addEventListener('submit', () => {
        sessionStorage.setItem(reloadKey, '1');
      });
    }
  </script>
</body>
</html>
)HTML";




