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

static const char kWebInterfaceWateringHtml[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Полив</title>
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
    .wrap { max-width: 1200px; margin: 40px auto; padding: 0 16px; }
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
    .right { text-align: right; }
    .center { text-align: center; }
    .nav { margin-bottom: 12px; }
    .status { margin: 8px 0 16px; color: var(--accent); font-weight: 600; }
    .row { display: flex; align-items: center; gap: 10px; flex-wrap: wrap; }
    .muted { color: var(--muted); font-size: 12px; }
    .field {
      width: 100%;
      padding: 6px 8px;
      border-radius: 8px;
      border: 1px solid #1f2937;
      background: #0b1220;
      color: var(--text);
    }
    .field.mini { padding: 4px 6px; width: 96px; }
    .field.name { min-width: 160px; }
    .field.name { margin-bottom: 8px; }
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
      background: #64748b;
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
    .switch input:checked + .track {
      background: #22c55e;
    }
    .switch input:checked + .track .knob {
      transform: translateX(18px);
    }
    .grid {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 14px;
      margin-top: 10px;
    }
    .tile {
      display: grid;
      grid-template-columns: 200px 1fr;
      gap: 14px;
      padding: 16px;
      border-radius: 12px;
      border: 1px solid #1f2937;
      background: #0b1220;
    }
    .tile.empty {
      grid-template-columns: 1fr;
      text-align: center;
      color: var(--muted);
    }
    .tile-head {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 10px;
      margin-bottom: 8px;
    }
    .watering-visual {
      display: grid;
      grid-template-rows: auto 1fr;
      align-items: start;
      gap: 10px;
      padding: 10px;
      border-radius: 10px;
      background: linear-gradient(160deg, rgba(56,189,248,0.08), rgba(15,23,42,0.85));
      border: 1px solid #1f2937;
      min-height: 180px;
    }
    .watering-icon {
      width: 110px;
      height: 110px;
      margin: 6px auto 0;
      color: #38bdf8;
      opacity: 0.25;
      filter: grayscale(1);
      transition: .2s;
    }
    .tile[data-active="1"] .watering-icon {
      opacity: 1;
      filter: none;
    }
    .status-line {
      display: grid;
      gap: 4px;
      color: var(--muted);
      font-size: 12px;
    }
    .status-line .status-value {
      font-weight: 700;
      color: #38bdf8;
    }
    .badge {
      padding: 2px 8px;
      border-radius: 999px;
      border: 1px solid #1f2937;
      background: #111827;
      color: var(--text);
      font-size: 11px;
    }
    .form-grid {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 10px;
    }
    .form-row {
      display: grid;
      grid-template-columns: max-content minmax(0, 1fr);
      align-items: center;
      gap: 8px;
    }
    .form-row.full { grid-column: span 2; }
    .form-row > label:not(.switch) {
      color: var(--muted);
      font-size: 12px;
      white-space: nowrap;
    }
    .form-row > label.switch {
      justify-self: end;
    }
    .form-row .field,
    .form-row select {
      width: 100%;
      min-width: 0;
      max-width: 100%;
    }
    .weekday-group {
      display: flex;
      flex-wrap: wrap;
      gap: 6px 8px;
    }
    .weekday-item {
      display: inline-flex;
      align-items: center;
      gap: 4px;
      color: var(--muted);
      font-size: 12px;
    }
    .weekday-item input {
      margin: 0;
    }
    @media (max-width: 900px) {
      .wrap { margin: 20px auto; }
      .grid { grid-template-columns: 1fr; }
      .tile { grid-template-columns: 1fr; }
      .form-grid { grid-template-columns: 1fr; }
    }
  </style>
</head>
<body>
  <div class="wrap">
    <div class="card">
      %NAV%
      <h1>Полив</h1>
      <div class="status">%WATERING_STATUS%</div>
      %WATERING_DEVICE_SELECT%
      <form method="POST" action="/watering" id="watering-form">
        <div class="grid">
          %WATERING_ROWS%
        </div>
        <div class="actions">
          %WATERING_SAVE_BTN%
        </div>
      </form>
    </div>
  </div>
  <script>
    const wateringOptions = {
      relay: %WATERING_RELAY_JSON%,
      tank: %WATERING_TANK_JSON%
    };
    function optionValue(item) {
      return (item && typeof item === 'object') ? String(item.v) : String(item);
    }
    function optionLabel(item) {
      if (item && typeof item === 'object' && item.l) return item.l;
      return String(item);
    }
    function buildOptions(list, selected) {
      const sel = String(selected || '');
      let html = '<option value=""' + (sel === '' ? ' selected' : '') + '>-</option>';
      for (let i = 0; i < list.length; i++) {
        const val = optionValue(list[i]);
        const label = optionLabel(list[i]);
        html += '<option value="' + val + '"' + (val === sel ? ' selected' : '') + '>' + label + '</option>';
      }
      return html;
    }
    document.querySelectorAll('select.watering-select').forEach((el) => {
      const type = el.dataset.type;
      const selected = el.dataset.selected || '';
      const list = wateringOptions[type] || [];
      el.innerHTML = buildOptions(list, selected);
      el.value = selected || '';
    });
    function updateTankDependent(tile) {
      const tankSelect = tile.querySelector('select[data-type="tank"]');
      const hasTank = tankSelect && tankSelect.value && tankSelect.value !== '0';
      tile.querySelectorAll('.tank-dependent').forEach((el) => {
        el.style.display = hasTank ? '' : 'none';
      });
      updateResumeDependent(tile);
    }
    function updateResumeDependent(tile) {
      const tankSelect = tile.querySelector('select[data-type="tank"]');
      const hasTank = tankSelect && tankSelect.value && tankSelect.value !== '0';
      const resumeToggle = tile.querySelector('input[name$="_resume"]');
      const resumeOn = resumeToggle && resumeToggle.checked;
      tile.querySelectorAll('.resume-dependent').forEach((el) => {
        el.style.display = (hasTank && resumeOn) ? '' : 'none';
      });
    }
    document.querySelectorAll('.tile').forEach((tile) => {
      updateTankDependent(tile);
      updateResumeDependent(tile);
      const tankSelect = tile.querySelector('select[data-type="tank"]');
      if (tankSelect) {
        tankSelect.addEventListener('change', () => updateTankDependent(tile));
      }
      const resumeToggle = tile.querySelector('input[name$="_resume"]');
      if (resumeToggle) {
        resumeToggle.addEventListener('change', () => updateResumeDependent(tile));
      }
    });
    const wateringForm = document.getElementById('watering-form');
    if (wateringForm) {
      const markDirty = () => { window.__plcDirty = true; };
      wateringForm.addEventListener('input', markDirty);
      wateringForm.addEventListener('change', markDirty);
    }
    const reloadKey = 'watering_reload';
    if (sessionStorage.getItem(reloadKey)) {
      sessionStorage.removeItem(reloadKey);
      location.replace(location.pathname);
    }
    if (wateringForm) {
      wateringForm.addEventListener('submit', () => {
        sessionStorage.setItem(reloadKey, '1');
      });
    }
    const wateringDevice = document.getElementById('watering-device');
    if (wateringDevice) {
      wateringDevice.addEventListener('change', () => {
        const val = wateringDevice.value || 'local';
        const url = new URL(window.location.href);
        if (val === 'local') {
          url.searchParams.delete('node');
          url.searchParams.delete('unit');
        } else {
          url.searchParams.set('unit', 'stack');
          url.searchParams.set('node', val);
        }
        window.location.href = url.toString();
      });
    }
  </script>
</body>
</html>
)HTML";







