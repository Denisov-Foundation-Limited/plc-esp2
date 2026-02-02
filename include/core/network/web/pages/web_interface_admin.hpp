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

static const char kWebInterfaceAdminHtml[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Система</title>
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
    @media (max-width: 520px) {
      .row { grid-template-columns: 1fr; }
    }
  </style>
</head>
<body>
  <div class="wrap">
    <div class="card">
      %NAV%
      <h1>Система</h1>
      <p class="status">Статус: <strong>%ADMIN_STATUS%</strong></p>
      <form method="POST" action="/admin" id="admin-form">
        <label for="password">Новый пароль</label>
        <input id="password" type="password" name="password" placeholder="Введите новый пароль">
        <button type="submit">Сохранить</button>
      </form>
      <div class="section">
        <p class="status">RTC сейчас: <strong>%RTC_DATE%</strong> <strong>%RTC_TIME%</strong></p>
        <form method="POST" action="/admin" id="rtc-form">
          <div class="row">
            <div>
              <label for="rtc-date">Дата</label>
              <input id="rtc-date" type="date" name="rtc_date" value="%RTC_DATE_VAL%">
            </div>
            <div>
              <label for="rtc-time">Время</label>
              <input id="rtc-time" type="time" step="1" name="rtc_time" value="%RTC_TIME_VAL%">
            </div>
          </div>
          <button type="submit">Сохранить RTC</button>
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
