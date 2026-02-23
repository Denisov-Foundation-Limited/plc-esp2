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

static const char kWebInterfaceUsersHtml[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>%USERS_PAGE_TITLE%</title>
  <style>
    :root {
      --bg: #0f172a;
      --card: #111827;
      --text: #e5e7eb;
      --muted: #94a3b8;
      --border: #1f2937;
      --accent: #38bdf8;
      --tile: #0b1220;
    }
    body {
      margin: 0;
      font-family: Segoe UI, Tahoma, Arial, sans-serif;
      background: radial-gradient(1200px 600px at 10% -10%, #1f2937 0%, #0b1220 60%, #080d17 100%);
      color: var(--text);
    }
    .wrap { max-width: 1200px; margin: 24px auto; padding: 0 12px; }
    .card {
      background: linear-gradient(180deg, #0f172a 0%, #0b1220 100%);
      border: 1px solid var(--border);
      border-radius: 12px;
      padding: 16px;
    }
    .nav { margin-bottom: 10px; }
    a { color: #7dd3fc; text-decoration: none; }
    h1 { margin: 0 0 8px; font-size: 22px; }
    .status { color: var(--muted); margin: 8px 0; font-size: 12px; }

    .tiles {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(320px, 1fr));
      gap: 12px;
      margin-top: 10px;
    }
    .tile {
      background: var(--tile);
      border: 1px solid var(--border);
      border-radius: 12px;
      padding: 12px;
    }
    .tile.disabled { opacity: 0.7; }
    .tile.empty { color: var(--muted); }
    .tile-head {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 10px;
      gap: 8px;
    }
    .head-left { display: inline-flex; align-items: center; gap: 8px; }
    .user-icon {
      width: 26px;
      height: 26px;
      border-radius: 999px;
      display: inline-flex;
      align-items: center;
      justify-content: center;
      color: #7dd3fc;
      background: rgba(56, 189, 248, 0.12);
      border: 1px solid rgba(56, 189, 248, 0.35);
      flex: 0 0 26px;
    }
    .user-icon svg { width: 16px; height: 16px; }

    .grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 8px 10px;
    }
    .form-row { display: flex; flex-direction: column; gap: 4px; }
    .form-row.full { grid-column: 1 / -1; }
    label { font-size: 12px; color: var(--muted); }
    .field {
      width: 100%;
      background: #0b1220;
      color: var(--text);
      border: 1px solid var(--border);
      border-radius: 8px;
      padding: 8px;
      min-height: 36px;
      box-sizing: border-box;
    }
    .field:disabled,
    select.field:disabled,
    input.field[readonly] {
      color: var(--muted);
      -webkit-text-fill-color: var(--muted);
      background: #0a1220;
      border-color: #1a2436;
      cursor: not-allowed;
    }
    .switch { display: inline-flex; align-items: center; }
    .switch input { display: none; }
    .track {
      width: 44px;
      height: 24px;
      background: #334155;
      border-radius: 999px;
      position: relative;
      transition: background 0.15s ease;
      display: inline-block;
      vertical-align: middle;
    }
    .knob {
      width: 18px;
      height: 18px;
      border-radius: 50%;
      background: #e5e7eb;
      position: absolute;
      top: 3px;
      left: 3px;
      transition: transform 0.15s ease;
    }
    .switch input:checked + .track { background: #0ea5e9; }
    .switch input:checked + .track .knob { transform: translateX(20px); }
    input:disabled + .track {
      background: #475569;
      border-color: #334155;
      cursor: not-allowed;
    }
    input:disabled + .track .knob {
      background: #1f2937;
      box-shadow: 0 0 0 1px rgba(15, 23, 42, 0.7);
    }

    .btn {
      margin-top: 14px;
      border: none;
      border-radius: 8px;
      padding: 10px 16px;
      font-weight: 700;
      background: var(--accent);
      color: #0b1220;
      cursor: pointer;
    }
    @media (max-width: 720px) {
      .grid { grid-template-columns: 1fr; }
    }
  </style>
</head>
<body>
  <div class="wrap">
    <div class="card">
      %NAV%
      <h1>%USERS_PAGE_H1%</h1>
      <p class="status">%USERS_STATUS%</p>
      %USERS_READONLY_NOTE%
      <form method="POST" action="/users">
        <div class="tiles">
          %USERS_CARDS%
        </div>
        %USERS_IBUTTON_DATALIST%
        %USERS_RFID_DATALIST%
        <button class="btn" type="submit" %USERS_FORM_DISABLED%>%SAVE_TEXT%</button>
      </form>
    </div>
  </div>
</body>
</html>
)HTML";




