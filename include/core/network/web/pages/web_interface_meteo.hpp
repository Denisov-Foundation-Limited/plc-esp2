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

static const char kWebInterfaceMeteoHtml[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Метео</title>
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
    .wrap { max-width: 1100px; margin: 40px auto; padding: 0 16px; }
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
    .status-dot {
      display: inline-block;
      width: 10px;
      height: 10px;
      border-radius: 50%;
      box-shadow: 0 0 0 2px rgba(15, 23, 42, 0.6);
    }
    .status-ok { background: #22c55e; }
    .status-err { background: #ef4444; }
    .status-na { background: #64748b; }
    .actions { margin-top: 14px; }
    .mini { width: 90px; }
    .addr { width: 160px; }
    .temp { width: 90px; }
    .hum { width: 90px; }
  </style>
</head>
<body>
  <div class="wrap">
    <div class="card">
      %NAV%
      <h1>Метео</h1>
      <p>Плата: <strong>%BOARD_NAME%</strong></p>
      <div class="status">%METEO_STATUS%</div>
      <form method="POST" action="/meteo" id="meteo-form">
        <table>
          <thead>
            <tr>
              <th class="right">ID</th>
              <th>Вкл</th>
              <th>Тип</th>
              <th>Пин</th>
              <th>Адрес</th>
              <th class="right">Темп</th>
              <th class="right">Влажн</th>
              <th class="center">OK</th>
              <th class="right">Возраст</th>
            </tr>
          </thead>
          <tbody>
            %METEO_ROWS%
          </tbody>
        </table>
        <p class="actions">
          <button class="btn" type="submit">Сохранить</button>
        </p>
      </form>
    </div>
  </div>
  <script>
    const sensorOptions = %SENSOR_JSON%;
    const sensorUsed = %SENSOR_USED_JSON%;

    function buildOptions(list, selected) {
      let html = '<option value="">-</option>';
      for (let i = 0; i < list.length; i++) {
        const val = String(list[i]);
        if (sensorUsed.indexOf(parseInt(val, 10)) !== -1 && val !== selected) {
          continue;
        }
        html += '<option value="' + val + '"' + (val === selected ? ' selected' : '') + '>' + val + '</option>';
      }
      return html;
    }

    document.querySelectorAll('select.meteo-pin').forEach((el) => {
      const selected = el.dataset.selected || '';
      el.innerHTML = buildOptions(sensorOptions || [], selected);
    });

    function setDisabled(el, disabled) {
      if (!el) {
        return;
      }
      if (el.tagName === 'INPUT' || el.tagName === 'SELECT' || el.tagName === 'TEXTAREA') {
        el.disabled = disabled;
      }
    }

    function updateRow(row) {
      const type = row.querySelector('select.meteo-type');
      const pinCell = row.querySelector('.pin-cell');
      const addrCell = row.querySelector('.addr-cell');
      const pinSelect = pinCell ? pinCell.querySelector('select') : null;
      const addrInput = addrCell ? addrCell.querySelector('input') : null;
      const val = type ? type.value : 'none';
      setDisabled(pinSelect, val !== 'dht22');
      setDisabled(addrInput, val !== 'ds18b20');
    }

    document.querySelectorAll('select.meteo-type').forEach((el) => {
      const row = el.closest('tr');
      if (row) {
        updateRow(row);
        el.addEventListener('change', () => updateRow(row));
      }
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
