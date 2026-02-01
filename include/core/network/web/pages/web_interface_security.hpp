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
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 14px;
      margin-top: 10px;
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
    .status-msg {
      margin: 6px 0 12px;
      color: var(--muted);
      font-size: 12px;
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
    .btn-sm {
      padding: 6px 10px;
      border-radius: 8px;
      font-size: 12px;
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
    .pagination {
      display: flex;
      align-items: center;
      gap: 10px;
      margin: 8px 0 12px;
      color: var(--muted);
      font-size: 12px;
    }
    .page-info { white-space: nowrap; }
    .tile {
      display: grid;
      grid-template-columns: 140px minmax(0, 1fr);
      gap: 14px;
      padding: 16px;
      border-radius: 12px;
      border: 1px solid var(--border);
      background: #0b1220;
      position: relative;
      overflow: hidden;
    }
    .tile.disabled { opacity: 0.55; }
    .tile.empty { grid-template-columns: 1fr; text-align: center; color: var(--muted); }
    .tile-head {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 10px;
      margin-bottom: 8px;
    }
    .sock-visual {
      position: relative;
      height: 140px;
      border-radius: 16px;
      border: 2px solid var(--border);
      background: linear-gradient(180deg, #0a1220 0%, #0c1628 100%);
      overflow: hidden;
    }
    .sock-icon {
      position: absolute;
      left: 50%;
      top: 50%;
      transform: translate(-50%, -50%);
      width: 90px;
      height: 90px;
      opacity: 0.2;
      color: #38bdf8;
      transition: opacity .2s ease, transform .2s ease, filter .2s ease;
    }
    .sock-icon.on {
      opacity: 1;
      filter: drop-shadow(0 0 12px rgba(34, 197, 94, 0.6));
      color: #22c55e;
      transform: translate(-50%, -50%) scale(1.02);
    }
    .sock-icon.off {
      opacity: 0.22;
      filter: none;
      color: #64748b;
    }
    .sock-icon.alert {
      opacity: 1;
      filter: drop-shadow(0 0 12px rgba(239, 68, 68, 0.5));
      color: #ef4444;
      transform: translate(-50%, -50%) scale(1.02);
    }
    .badge {
      position: absolute;
      left: 10px;
      top: 10px;
      padding: 2px 8px;
      border-radius: 999px;
      border: 1px solid var(--border);
      background: rgba(17, 24, 39, 0.85);
      color: var(--text);
      font-size: 11px;
      letter-spacing: 0.2px;
      z-index: 2;
    }
    .form-grid {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 10px 14px;
    }
    .form-row {
      display: grid;
      grid-template-columns: max-content minmax(0, 1fr);
      align-items: center;
      gap: 8px;
    }
    .form-row > label:not(.switch) {
      color: var(--muted);
      font-size: 12px;
      white-space: nowrap;
    }
    .form-row .field,
    .form-row select {
      width: 100%;
      min-width: 0;
      max-width: 100%;
    }
    .status-line {
      display: flex;
      align-items: center;
      gap: 8px;
      color: var(--muted);
      font-size: 12px;
      margin: 8px 0 10px;
    }
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
    @media (max-width: 900px) {
      .grid { grid-template-columns: 1fr; }
      .tile { grid-template-columns: 1fr; }
      .sock-visual { height: 120px; }
      .form-grid { grid-template-columns: 1fr; }
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
          <span class="pill">Статус: <span class="status-dot" data-state="%SECURITY_ARMED_LABEL%" title="%SECURITY_ARMED_LABEL%"></span></span>
          <span class="pill">Тревога: <span class="status-dot" data-state="%SECURITY_ALARM_LABEL%" title="%SECURITY_ALARM_LABEL%"></span></span>
          <span class="pill">GSM: <span class="status-dot" data-kind="gsm" data-state="%SECURITY_GSM_LABEL%" title="%SECURITY_GSM_LABEL%"></span></span>
        </div>
        <div class="status-msg" id="security-status">%SECURITY_STATUS%</div>
        <div class="grid">
          <div>
            <label>Охрана</label>
            <div class="buttons">
              <button class="primary" name="action" value="arm">Поставить</button>
              <button class="warn" name="action" value="disarm">Снять</button>
            </div>
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
        <div class="pagination">
          <button type="button" class="btn btn-sm" id="security-prev">Назад</button>
          <span class="page-info">Страница</span>
          <select id="security-page" class="field mini"></select>
          <span class="page-info">/ %SECURITY_SENSORS_PAGES%</span>
          <button type="button" class="btn btn-sm" id="security-next">Вперёд</button>
        </div>
        <div class="grid">
          %SECURITY_SENSORS%
        </div>
        <div class="buttons">
          <button class="primary" name="action" value="save">Сохранить</button>
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
    const secPrev = document.getElementById('security-prev');
    const secNext = document.getElementById('security-next');
    const secPage = document.getElementById('security-page');
    const secPages = %SECURITY_SENSORS_PAGES%;
    const secCurrent = %SECURITY_SENSORS_PAGE%;
    if (secPage) {
      for (let i = 1; i <= secPages; i++) {
        const opt = document.createElement('option');
        opt.value = String(i);
        opt.textContent = String(i);
        if (i === secCurrent) opt.selected = true;
        secPage.appendChild(opt);
      }
      secPage.addEventListener('change', () => {
        const url = new URL(window.location.href);
        url.searchParams.set('page', secPage.value || String(secCurrent));
        window.location.href = url.toString();
      });
    }
    if (secPrev) {
      secPrev.disabled = secCurrent <= 1;
      secPrev.addEventListener('click', () => {
        const url = new URL(window.location.href);
        url.searchParams.set('page', String(Math.max(1, secCurrent - 1)));
        window.location.href = url.toString();
      });
    }
    if (secNext) {
      secNext.disabled = secCurrent >= secPages;
      secNext.addEventListener('click', () => {
        const url = new URL(window.location.href);
        url.searchParams.set('page', String(Math.min(secPages, secCurrent + 1)));
        window.location.href = url.toString();
      });
    }
    async function postForm(url, body) {
      const res = await fetch(url, {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body,
        credentials: 'same-origin'
      });
      if (!res.ok) throw new Error('Request failed');
      return (await res.text()).trim();
    }
    const securityStatus = document.getElementById('security-status');
    document.querySelectorAll('select.security-port').forEach((el) => {
      const type = el.dataset.type;
      const selected = el.dataset.selected || '';
      const list = sensorOptions[type] || [];
      el.innerHTML = buildOptions(list, selected, type);
    });
    function buildSirenOptions(list, selected) {
      let html = '<option value="none">-</option>';
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
    const securityPage = %SECURITY_SENSORS_PAGE%;
    const securityPages = %SECURITY_SENSORS_PAGES%;
    const pageSelect = document.getElementById('security-page');
    if (pageSelect) {
      for (let i = 1; i <= securityPages; i++) {
        const opt = document.createElement('option');
        opt.value = String(i);
        opt.textContent = String(i);
        if (i === securityPage) opt.selected = true;
        pageSelect.appendChild(opt);
      }
      pageSelect.addEventListener('change', () => {
        const url = new URL(window.location.href);
        url.searchParams.set('page', pageSelect.value || String(securityPage));
        window.location.href = url.toString();
      });
    }
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
