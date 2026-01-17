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

static const char kWebInterfaceSocketsHtml[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Розетки</title>
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
    table { width: 100%; border-collapse: collapse; margin-top: 10px; }
    th, td { text-align: left; padding: 6px; border-bottom: 1px solid #1f2937; }
    th { color: var(--muted); font-weight: 600; }
    .right { text-align: right; }
    .nav { margin-bottom: 12px; }
    .status { margin: 8px 0 16px; color: var(--accent); font-weight: 600; }
    .field {
      width: 100%;
      padding: 6px 8px;
      border-radius: 8px;
      border: 1px solid #1f2937;
      background: #0b1220;
      color: var(--text);
    }
    .btn {
      border: none;
      padding: 10px 16px;
      border-radius: 10px;
      background: var(--accent);
      color: #0b1220;
      font-weight: 700;
      cursor: pointer;
    }
    .btn-sm {
      padding: 6px 10px;
      border-radius: 8px;
      font-size: 12px;
    }
    .btn-on { background: #22c55e; color: #0b1220; }
    .btn-off { background: #f97316; color: #0b1220; }
    .btn-toggle { background: #38bdf8; color: #0b1220; }
    .row-on { background: rgba(34, 197, 94, 0.12); }
    .row-off { background: rgba(148, 163, 184, 0.08); }
    .mini { width: 72px; }
    .name { width: 180px; }
  </style>
</head>
<body>
  <div class="wrap">
    <div class="card">
      %NAV%
      <h1>Розетки</h1>
      <p>Плата: <strong>%BOARD_NAME%</strong></p>
      <div class="status">%SOCKETS_STATUS%</div>
      <form method="POST" action="/sockets">
        <table>
          <thead>
            <tr>
              <th class="right">ID</th>
              <th>En</th>
              <th>Name</th>
              <th>Button</th>
              <th>Relay</th>
              <th>State</th>
            </tr>
          </thead>
          <tbody>
            %SOCKETS%
          </tbody>
        </table>
        <p>
          <button class="btn" type="submit">Save</button>
        </p>
      </form>
    </div>
  </div>
  <script>
    const socketOptions = {
      dinput: %DINPUT_JSON%,
      relay: %RELAY_JSON%
    };
    function buildOptions(list, selected) {
      let html = '<option value="">-</option>';
      for (let i = 0; i < list.length; i++) {
        const val = String(list[i]);
        html += '<option value="' + val + '"' + (val === selected ? ' selected' : '') + '>' + val + '</option>';
      }
      return html;
    }
    document.querySelectorAll('select.socket-select').forEach((el) => {
      const type = el.dataset.type;
      const selected = el.dataset.selected || '';
      const list = socketOptions[type] || [];
      el.innerHTML = buildOptions(list, selected);
    });
  </script>
</body>
</html>
)HTML";
