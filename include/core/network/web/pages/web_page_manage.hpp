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

static const char kWebInterfaceManageHtml[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Прошивка и файлы</title>
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
    input[type=file] {
      flex: 1;
      background: #0b1220;
      border: 1px dashed #334155;
      color: var(--text);
      padding: 10px;
      border-radius: 8px;
    }
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
    table { width: 100%; border-collapse: collapse; margin-top: 10px; }
    th, td { text-align: left; padding: 8px 6px; border-bottom: 1px solid #1f2937; }
    th { color: var(--muted); font-weight: 600; }
    a { color: #7dd3fc; text-decoration: none; }
    .del { color: #fca5a5; }
    .right { text-align: right; }
    .nav { margin-bottom: 12px; }
  </style>
</head>
<body>
  <div class="wrap">
    <div class="card">
      %NAV%
      <h1>Прошивка и файлы</h1>
      <div class="section">
        <h2>Прошивка</h2>
        <p class="status">OTA загрузка (.bin)</p>
        <form method="POST" action="/ota" enctype="multipart/form-data">
          <div class="row">
            <input type="file" name="firmware">
            <button type="submit">Загрузить прошивку</button>
          </div>
        </form>
      </div>
      <div class="section">
        <h2>Файлы</h2>
        <form method="POST" action="/upload" enctype="multipart/form-data">
          <div class="row">
            <input type="file" name="file">
            <button type="submit">Загрузить файл</button>
          </div>
        </form>
        <p><a href="/status">Статус</a></p>
      </div>
      <div class="section">
        <h2>Список файлов</h2>
        <table>
          <thead>
            <tr>
              <th>Имя</th>
              <th class="right">Размер</th>
              <th class="right">Удалить</th>
            </tr>
          </thead>
          <tbody>
            %FILES%
          </tbody>
        </table>
      </div>
    </div>
  </div>
</body>
</html>
)HTML";
