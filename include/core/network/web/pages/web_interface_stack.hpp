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

static const char kWebInterfaceStackHtml[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Стек</title>
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
    input[type=text], select {
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
  </style>
</head>
<body>
  <div class="wrap">
    <div class="card">
      <div class="nav">
        <a href="/">FCPLC</a> | <a href="/wifi">Wi-Fi</a> | <a href="/manage">Прошивка и файлы</a> | <a href="/ports">Порты</a> | <a href="/buses">Шины</a> | <a href="/telegram">Telegram</a>
      </div>
      <h1>Стек</h1>
      <p class="status">Плата: <strong>%BOARD_NAME%</strong></p>
      <div class="section">
        <p class="status">Роль: <strong>%STACK_ROLE%</strong></p>
        <form method="POST" action="/stack">
          <div class="grid">
            <div>
              <label>Роль</label>
              <select name="role">
                <option value="master" %STACK_ROLE_MASTER_SEL%>master</option>
                <option value="slave" %STACK_ROLE_SLAVE_SEL%>slave</option>
              </select>
            </div>
            <div>
              <label>Master host/IP</label>
              <input type="text" name="master_host" value="%STACK_MASTER_HOST%" placeholder="192.168.1.10">
            </div>
          </div>
          <div class="row" style="margin-top:10px;">
            <button type="submit">Сохранить</button>
            <span class="status">%STACK_STATUS%</span>
          </div>
        </form>
      </div>
    </div>
  </div>
</body>
</html>
)HTML";
