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

static const char kWebInterfaceLightsHtml[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Свет</title>
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
    .center { text-align: center; }
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
    .status-dot {
      display: inline-block;
      width: 10px;
      height: 10px;
      border-radius: 50%;
      box-shadow: 0 0 0 2px rgba(15, 23, 42, 0.6);
    }
    .status-on { background: #22c55e; }
    .status-off { background: #ef4444; }
    .mini { width: 72px; }
    .name { width: 180px; }
    .actions { margin-top: 14px; }
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
      background: #ef4444;
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
    .table-wrap { width: 100%; overflow-x: auto; -webkit-overflow-scrolling: touch; }
    table { min-width: 720px; }
    @media (max-width: 720px) {
      .wrap { margin: 20px auto; }
      .card { padding: 16px; }
      h1 { font-size: 20px; }
      table { min-width: 640px; font-size: 12px; }
      th, td { padding: 5px; }
      .field { padding: 5px 6px; }
      .btn { padding: 8px 12px; }
      .mini { width: 64px; }
      .name { width: 140px; }
    }
  </style>
</head>
<body>
  <div class="wrap">
    <div class="card">
      %NAV%
      <h1>Свет</h1>
      <p>Плата: <strong>%BOARD_NAME%</strong></p>
      <div class="status">%SOCKETS_STATUS%</div>
      <form method="POST" action="/lights" id="lights-form">
        <div class="table-wrap">
        <table>
          <thead>
            <tr>
              <th class="right">ID</th>
              <th>Вкл</th>
              <th>Имя</th>
              <th>Кнопка</th>
              <th>Реле</th>
              <th class="center">Статус</th>
              <th>Управление</th>
            </tr>
          </thead>
          <tbody>
            %LIGHTS%
          </tbody>
        </table>
        </div>
        <p class="actions">
          <button class="btn" type="submit">Сохранить</button>
        </p>
      </form>
    </div>
  </div>
  <script>
    const socketOptions = {
      dinput: %DINPUT_JSON%,
      relay: %RELAY_JSON%
    };
    const socketUsed = {
      dinput: %DINPUT_USED_JSON%,
      relay: %RELAY_USED_JSON%
    };
    function buildOptions(list, selected, type) {
      let html = '<option value="">-</option>';
      const used = socketUsed[type] || [];
      for (let i = 0; i < list.length; i++) {
        const val = String(list[i]);
        if (used.indexOf(parseInt(val, 10)) !== -1 && val !== selected) {
          continue;
        }
        html += '<option value="' + val + '"' + (val === selected ? ' selected' : '') + '>' + val + '</option>';
      }
      return html;
    }
    document.querySelectorAll('select.socket-select').forEach((el) => {
      const type = el.dataset.type;
      const selected = el.dataset.selected || '';
      const list = socketOptions[type] || [];
      el.innerHTML = buildOptions(list, selected, type);
    });
    const lightsForm = document.getElementById('lights-form');
    document.querySelectorAll('input.socket-toggle').forEach((el) => {
      el.addEventListener('change', () => {
        const name = el.dataset.action;
        const hidden = document.querySelector('input[name="' + name + '"]');
        if (hidden) {
          hidden.value = el.checked ? 'on' : 'off';
        }
        if (lightsForm) {
          lightsForm.submit();
        }
      });
    });
    setInterval(() => {
      const el = document.activeElement;
      if (el && (el.tagName === 'INPUT' || el.tagName === 'SELECT' || el.tagName === 'TEXTAREA')) {
        return;
      }
      location.reload();
    }, 3000);
  </script>
</body>
</html>
)HTML";
