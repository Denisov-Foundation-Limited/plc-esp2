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

static const char kWebInterfaceSecurityHtml[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Охрана</title>
  <style>
    :root {
      --bg: #0f172a;
      --card: #111827;
      --accent: #38bdf8;
      --text: #e5e7eb;
      --muted: #94a3b8;
      --border: #1f2937;
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
      border: 1px solid var(--border);
      border-radius: 14px;
      padding: 22px;
      box-shadow: 0 10px 30px rgba(0,0,0,0.35);
    }
    h1 { margin: 0 0 6px; font-size: 22px; }
    h2 { margin: 16px 0 6px; font-size: 16px; color: var(--text); }
    p { margin: 0 0 18px; color: var(--muted); }
    a { color: #7dd3fc; text-decoration: none; }
    .nav { margin-bottom: 12px; }
    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(260px, 1fr));
      gap: 12px;
      margin: 12px 0;
    }
    label { display: block; margin-bottom: 6px; color: var(--muted); font-size: 12px; }
    .status {
      display: flex;
      gap: 12px;
      flex-wrap: wrap;
      padding: 10px 12px;
      background: #0b1220;
      border: 1px solid var(--border);
      border-radius: 10px;
      margin: 12px 0;
      font-size: 13px;
    }
    .pill {
      padding: 4px 8px;
      border-radius: 999px;
      border: 1px solid var(--border);
      background: #111827;
      color: var(--text);
    }
    table { width: 100%; border-collapse: collapse; margin-top: 10px; }
    th, td { text-align: left; padding: 6px; border-bottom: 1px solid var(--border); }
    th { color: var(--muted); font-weight: 600; }
    .right { text-align: right; }
    .center { text-align: center; }
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
      background: #64748b;
      border-radius: 999px;
      border: 1px solid var(--border);
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
    .buttons { display: flex; gap: 8px; flex-wrap: wrap; margin-top: 12px; }
    button {
      border: none;
      background: var(--accent);
      color: #0b1220;
      padding: 10px 16px;
      border-radius: 10px;
      font-weight: 700;
      cursor: pointer;
    }
    button.primary { background: var(--accent); }
    button.warn { background: #ef4444; }
    .field {
      width: 100%;
      padding: 6px 8px;
      border-radius: 8px;
      border: 1px solid var(--border);
      background: #0b1220;
      color: var(--text);
    }
    .mini { width: 88px; }
    .name { width: 180px; }
    .serial {
      width: 220px;
      font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", "Courier New", monospace;
    }
    .status-dot {
      display: inline-block;
      width: 10px;
      height: 10px;
      border-radius: 50%;
      box-shadow: 0 0 0 2px rgba(15, 23, 42, 0.6);
    }
    .status-on { background: #22c55e; }
    .status-off { background: #64748b; }
    .status-bad { background: #ef4444; }
    .table-wrap { width: 100%; overflow-x: auto; -webkit-overflow-scrolling: touch; }
    table { min-width: 720px; }
    @media (max-width: 720px) {
      .wrap { margin: 20px auto; }
      .card { padding: 16px; }
      h1 { font-size: 20px; }
      h2 { font-size: 15px; }
      table { min-width: 640px; font-size: 12px; }
      th, td { padding: 5px; }
      .field { padding: 5px 6px; }
      .mini { width: 72px; }
      .name { width: 140px; }
      .serial { width: 180px; }
    }
  </style>
</head>
<body>
  <div class="wrap">
    <div class="card">
      %NAV%
      <h1>Охрана</h1>
      <p>Плата: <strong>%BOARD_NAME%</strong></p>
      <form method="POST" action="/security" id="security-form">
        <div class="status">
          <span class="pill">Контроллер: <span class="status-dot" data-state="%SECURITY_ENABLED_LABEL%" title="%SECURITY_ENABLED_LABEL%"></span></span>
          <span class="pill">Статус: <span class="status-dot" data-state="%SECURITY_ARMED_LABEL%" title="%SECURITY_ARMED_LABEL%"></span></span>
          <span class="pill">Тревога: <span class="status-dot" data-state="%SECURITY_ALARM_LABEL%" title="%SECURITY_ALARM_LABEL%"></span></span>
          <span class="pill">GSM: <span class="status-dot" data-kind="gsm" data-state="%SECURITY_GSM_LABEL%" title="%SECURITY_GSM_LABEL%"></span></span>
          <span class="pill">Состояние: <span class="status-dot" data-state="%SECURITY_STATUS%" title="%SECURITY_STATUS%"></span></span>
        </div>
        <div class="grid">
          <div>
            <label>Включить контроллер охраны</label>
            <label class="switch">
              <input type="checkbox" name="security_enabled" %SECURITY_ENABLED_CHECKED%>
              <span class="track"><span class="knob"></span></span>
            </label>
          </div>
          <div>
            <label>Порт сирены</label>
            <select class="field mini siren-select" data-selected="%SECURITY_SIREN%" name="security_siren"></select>
          </div>
        </div>
        <h2>Ключи iButton</h2>
        <div class="table-wrap">
        <table>
          <thead>
            <tr>
              <th class="right">ID</th>
              <th>Вкл</th>
              <th>Имя</th>
              <th>Серийный</th>
            </tr>
          </thead>
          <tbody>
            %SECURITY_KEYS_ROWS%
          </tbody>
        </table>
        </div>
        <h2>GSM телефоны</h2>
        <div class="table-wrap">
        <table>
          <thead>
            <tr>
              <th class="right">ID</th>
              <th>Вкл</th>
              <th>Имя</th>
              <th>Телефон</th>
              <th>Уведомл.</th>
              <th>Звонок</th>
            </tr>
          </thead>
          <tbody>
            %SECURITY_PHONES_ROWS%
          </tbody>
        </table>
        </div>
        <h2>Датчики</h2>
        <div class="table-wrap">
        <table>
          <thead>
            <tr>
              <th class="right">ID</th>
              <th>Вкл</th>
              <th>Имя</th>
              <th>Тип</th>
              <th>Порт</th>
              <th>Тихий</th>
              <th class="center">Сработал</th>
            </tr>
          </thead>
          <tbody>
            %SECURITY_SENSORS_ROWS%
          </tbody>
        </table>
        </div>
        <div class="buttons">
          <button class="primary" name="action" value="save">Сохранить</button>
          <button name="action" value="arm">Поставить на охрану</button>
          <button name="action" value="disarm">Снять с охраны</button>
          <button class="warn" name="action" value="clear">Сбросить сработки</button>
        </div>
      </form>
    </div>
  </div>
  <script>
    function stateToBool(value) {
      const s = String(value || '').trim().toLowerCase();
      if (!s) return false;
      if (s === '1' || s === 'true' || s === 'on' || s === 'ok' || s === 'ready' || s === 'active') return true;
      if (s.indexOf('вкл') !== -1) return true;
      if (s.indexOf('под охраной') !== -1) return true;
      if (s.indexOf('включ') !== -1) return true;
      if (s.indexOf('armed') !== -1) return true;
      if (s.indexOf('enabled') !== -1) return true;
      if (s.indexOf('alarm') !== -1) return true;
      if (s.indexOf('updated') !== -1) return true;
      if (s.indexOf('saved') !== -1) return true;
      return false;
    }
    document.querySelectorAll('.status-dot[data-state]').forEach((dot) => {
      const kind = dot.dataset.kind || '';
      const state = dot.dataset.state || '';
      if (kind === 'gsm') {
        const s = String(state).trim().toLowerCase();
        const ok = s === 'ok' || s === '1' || s === 'true' || s === 'on';
        const off = s === 'off' || s === 'недоступно' || s === 'выкл' || s === 'not started';
        dot.classList.toggle('status-on', ok);
        dot.classList.toggle('status-off', off);
        dot.classList.toggle('status-bad', !ok && !off);
        return;
      }
      const on = stateToBool(state);
      dot.classList.toggle('status-on', on);
      dot.classList.toggle('status-off', !on);
    });
    const sensorOptions = {
      dinput: %SECURITY_SENSOR_JSON%
    };
    const sensorUsed = {
      dinput: %SECURITY_SENSOR_USED_JSON%
    };
    const sirenOptions = {
      relay: %SECURITY_SIREN_JSON%
    };
    const sirenUsed = {
      relay: %SECURITY_SIREN_USED_JSON%
    };
    function labelFor(type, val) {
      if (type === 'dinput') return 'in' + val;
      if (type === 'relay') return 'rly' + val;
      if (type === 'sensor') return 'sens' + val;
      return val;
    }
    function optionValue(item) {
      return (item && typeof item === 'object') ? String(item.v) : String(item);
    }
    function optionLabel(item, type) {
      if (item && typeof item === 'object' && item.l) return item.l;
      return labelFor(type, optionValue(item));
    }
    function buildOptions(list, selected, type) {
      let html = '<option value="">-</option>';
      const used = sensorUsed[type] || [];
      for (let i = 0; i < list.length; i++) {
        const val = optionValue(list[i]);
        if (used.indexOf(parseInt(val, 10)) !== -1 && val !== selected) {
          continue;
        }
        const label = optionLabel(list[i], type);
        html += '<option value="' + val + '"' + (val === selected ? ' selected' : '') + '>' + label + '</option>';
      }
      return html;
    }
    document.querySelectorAll('select.security-port').forEach((el) => {
      const type = el.dataset.type;
      const selected = el.dataset.selected || '';
      const list = sensorOptions[type] || [];
      el.innerHTML = buildOptions(list, selected, type);
    });
    function buildSirenOptions(list, selected) {
      let html = '<option value="">-</option>';
      const used = sirenUsed.relay || [];
      for (let i = 0; i < list.length; i++) {
        const val = optionValue(list[i]);
        if (used.indexOf(parseInt(val, 10)) !== -1 && val !== selected) {
          continue;
        }
        const label = optionLabel(list[i], 'relay');
        html += '<option value="' + val + '"' + (val === selected ? ' selected' : '') + '>' + label + '</option>';
      }
      return html;
    }
    document.querySelectorAll('select.siren-select').forEach((el) => {
      const selected = el.dataset.selected || '';
      const list = sirenOptions.relay || [];
      el.innerHTML = buildSirenOptions(list, selected);
    });
    const securityForm = document.getElementById('security-form');
    const reloadKey = 'security_reload';
    if (sessionStorage.getItem(reloadKey)) {
      sessionStorage.removeItem(reloadKey);
      location.replace(location.pathname);
    }
    if (securityForm) {
      securityForm.addEventListener('submit', (e) => {
        const submitter = e.submitter;
        if (submitter && submitter.value !== 'save') {
          return;
        }
        sessionStorage.setItem(reloadKey, '1');
      });
    }
  </script>
</body>
</html>
)HTML";
