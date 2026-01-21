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

static const char kWebInterfaceSepticHtml[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Септик</title>
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
    .field.mini { padding: 4px 6px; }
    .field.name { min-width: 160px; }
    .actions { display: flex; gap: 10px; margin-top: 16px; }
    button {
      border: none;
      border-radius: 10px;
      padding: 10px 16px;
      background: var(--accent);
      color: #0b1220;
      font-weight: 700;
      cursor: pointer;
    }
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
    }
    .switch input:checked + .track { background: #22c55e; }
    .switch input:checked + .track .knob { transform: translateX(18px); }
    .table-wrap { width: 100%; overflow-x: auto; -webkit-overflow-scrolling: touch; }
    .status-dot {
      display: inline-block;
      width: 10px;
      height: 10px;
      border-radius: 50%;
      box-shadow: 0 0 0 2px rgba(15, 23, 42, 0.6);
    }
    .status-on { background: #22c55e; }
    .status-off { background: #ef4444; }
    .water-low {
      background: linear-gradient(180deg, #0ea5e9 0%, #0284c7 100%);
    }
    .water-warn {
      background: linear-gradient(180deg, #facc15 0%, #eab308 100%);
    }
    .water-alarm {
      background: linear-gradient(180deg, #ef4444 0%, #dc2626 100%);
    }
    .schematic {
      margin: 10px 0 16px;
      padding: 16px;
      border-radius: 12px;
      border: 1px solid #1f2937;
      background: #0b1220;
      display: grid;
      grid-template-columns: 1fr 240px;
      gap: 16px;
      align-items: center;
    }
    .tank {
      position: relative;
      height: 220px;
      border-radius: 16px;
      border: 2px solid #1f2937;
      background: linear-gradient(180deg, #0a1220 0%, #0c1628 100%);
      overflow: hidden;
    }
    .liquid {
      position: absolute;
      left: 0;
      right: 0;
      bottom: 0;
      height: 20%;
      background: linear-gradient(180deg, #0ea5e9 0%, #0284c7 100%);
      opacity: 0.7;
      transition: height .3s ease, background .3s ease;
    }
    .liquid::before {
      content: "";
      position: absolute;
      left: -20%;
      right: -20%;
      top: -12px;
      height: 24px;
      background: radial-gradient(circle at 20% 50%, rgba(255,255,255,0.25) 0 18px, transparent 20px) repeat-x;
      background-size: 80px 24px;
      opacity: 0.55;
      animation: wave 3.2s linear infinite;
    }
    @keyframes wave {
      0% { transform: translateX(0); }
      100% { transform: translateX(80px); }
    }
    .level-label {
      position: absolute;
      left: 50%;
      top: 10px;
      transform: translateX(-50%);
      padding: 2px 10px;
      border-radius: 999px;
      border: 1px solid #1f2937;
      background: rgba(17, 24, 39, 0.8);
      color: #e5e7eb;
      font-size: 12px;
      letter-spacing: 0.2px;
    }
    .probe {
      position: absolute;
      left: 16px;
      right: 16px;
      height: 2px;
      background: #1f2937;
    }
    .probe.warn { top: 60px; }
    .probe.alarm { top: 24px; }
    .probe-label {
      position: absolute;
      right: 8px;
      top: -10px;
      font-size: 11px;
      color: var(--muted);
    }
    .probe-dot {
      position: absolute;
      left: 10px;
      top: -5px;
    }
    .legend {
      display: grid;
      gap: 10px;
      font-size: 13px;
      color: var(--muted);
    }
    .legend .row {
      display: flex;
      align-items: center;
      gap: 8px;
      justify-content: space-between;
    }
    .legend .label {
      display: flex;
      align-items: center;
      gap: 8px;
      color: var(--text);
    }
    .badge {
      padding: 2px 8px;
      border-radius: 999px;
      border: 1px solid #1f2937;
      background: #111827;
      color: var(--text);
      font-size: 11px;
    }
    @media (max-width: 900px) {
      .wrap { margin: 20px auto; }
      .schematic { grid-template-columns: 1fr; }
    }
  </style>
</head>
<body>
  <div class="wrap">
    %NAV%
    <div class="card">
      <h1>Септик</h1>
      <p>Плата: <strong>%BOARD_NAME%</strong></p>
      <div class="status">%SEPTIC_STATUS%</div>
      <div class="schematic">
        <div class="tank">
          <div class="liquid %SEPTIC_WATER_CLASS%" style="height:%SEPTIC_WATER_LEVEL%;"></div>
          <div class="level-label">%SEPTIC_WATER_LABEL%</div>
          <div class="probe alarm">
            <span class="probe-label">ТРЕВОГА</span>
            <span class="probe-dot status-dot %SEPTIC_ALARM_CLASS%"></span>
          </div>
          <div class="probe warn">
            <span class="probe-label">ПРЕДУПРЕЖДЕНИЕ</span>
            <span class="probe-dot status-dot %SEPTIC_WARN_CLASS%"></span>
          </div>
        </div>
        <div class="legend">
          <div class="row">
            <div class="label">
              <span class="status-dot %SEPTIC_WARN_CLASS%"></span>
              <span>Датчик предупреждения</span>
            </div>
            <span class="badge">%SEPTIC_WARN_LABEL%</span>
          </div>
          <div class="row">
            <div class="label">
              <span class="status-dot %SEPTIC_ALARM_CLASS%"></span>
              <span>Датчик тревоги</span>
            </div>
            <span class="badge">%SEPTIC_ALARM_LABEL%</span>
          </div>
          <div class="row">
            <div class="label">Реле предупреждения</div>
            <span class="badge">%SEPTIC_RELAY_WARN_LABEL%</span>
          </div>
          <div class="row">
            <div class="label">Реле тревоги</div>
            <span class="badge">%SEPTIC_RELAY_ALARM_LABEL%</span>
          </div>
        </div>
      </div>
      <form method="POST" action="/septic" id="septic-form">
        <div class="table-wrap">
          <table>
            <thead>
              <tr>
                <th class="right">ID</th>
                <th>Вкл</th>
                <th>Имя</th>
                <th class="right">Предупр</th>
                <th class="right">Тревога</th>
                <th class="right">Реле пред.</th>
                <th class="right">Реле трев.</th>
                <th class="center">Мониторинг</th>
                <th class="center">Пред.</th>
                <th class="center">Трев.</th>
              </tr>
            </thead>
            <tbody>
              %SEPTIC_ITEMS%
            </tbody>
          </table>
        </div>
        <div class="actions">
          <button type="submit">Сохранить</button>
        </div>
      </form>
    </div>
  </div>
  <script>
    const septicOptions = {
      dinput: %SEPTIC_DINPUT_JSON%,
      relay: %SEPTIC_RELAY_JSON%
    };
    const septicUsed = {
      dinput: %SEPTIC_DINPUT_USED_JSON%,
      relay: %SEPTIC_RELAY_USED_JSON%
    };
    function buildOptions(list, selected, type) {
      let html = '<option value="">-</option>';
      const used = septicUsed[type] || [];
      for (let i = 0; i < list.length; i++) {
        const val = String(list[i]);
        if (used.indexOf(parseInt(val, 10)) !== -1 && val !== selected) {
          continue;
        }
        html += '<option value="' + val + '"' + (val === selected ? ' selected' : '') + '>' + val + '</option>';
      }
      return html;
    }
    document.querySelectorAll('select.septic-select').forEach((el) => {
      const type = el.dataset.type;
      const selected = el.dataset.selected || '';
      const list = septicOptions[type] || [];
      el.innerHTML = buildOptions(list, selected, type);
    });
    const septicForm = document.getElementById('septic-form');
    document.querySelectorAll('input.septic-monitor').forEach((el) => {
      el.addEventListener('change', () => {
        const name = el.dataset.action;
        const hidden = document.querySelector('input[name="' + name + '"]');
        if (hidden) {
          hidden.value = el.checked ? 'on' : 'off';
        }
        if (septicForm) {
          septicForm.submit();
        }
      });
    });
  </script>
</body>
</html>
)HTML";
