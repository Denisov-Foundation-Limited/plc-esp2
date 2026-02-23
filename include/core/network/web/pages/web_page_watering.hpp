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
  <title>%WATERING_PAGE_TITLE%</title>
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
    .field:disabled,
    select.field:disabled,
    input.field[readonly] {
      color: var(--muted);
      -webkit-text-fill-color: var(--muted);
      background: #0a1220;
      border-color: #1a2436;
      cursor: not-allowed;
    }    .field.mini { padding: 4px 6px; width: 96px; }
    .field.name { min-width: 160px; }
    .field.name { margin-bottom: 8px; }
    .actions { display: flex; gap: 10px; margin-top: 16px; }
    .pagination {
      display: flex;
      align-items: center;
      gap: 10px;
      margin: 8px 0 12px;
      color: var(--muted);
      font-size: 12px;
      flex-wrap: wrap;
    }
    .page-info { white-space: nowrap; }
    .page-btn {
      display: inline-block;
      padding: 6px 10px;
      border-radius: 8px;
      border: 1px solid #1f2937;
      background: #0b1220;
      color: var(--text);
      text-decoration: none;
    }
    .page-btn.disabled {
      opacity: .5;
      pointer-events: none;
    }
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
    .switch input:disabled + .track {
      background: #475569;
      border-color: #334155;
      cursor: not-allowed;
    }
    .switch input:disabled + .track .knob {
      background: #1f2937;
      box-shadow: 0 0 0 1px rgba(15, 23, 42, 0.7);
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
      <h1>%WATERING_PAGE_TITLE%</h1>
      <div class="status">%WATERING_STATUS%</div>
      %WATERING_DEVICE_SELECT%
      %WATERING_PAGINATION%
      <form method="POST" action="%WATERING_FORM_ACTION%" id="watering-form">
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
    const wateringUsed = {
      relay: %WATERING_RELAY_USED_JSON%
    };
    function optionValue(item) {
      return (item && typeof item === 'object') ? String(item.v) : String(item);
    }
    function optionLabel(item) {
      if (item && typeof item === 'object' && item.l) return item.l;
      return String(item);
    }
    function buildOptions(list, selected, usedSet) {
      const sel = String(selected || '');
      let html = '<option value=""' + (sel === '' ? ' selected' : '') + '>-</option>';
      const used = usedSet || new Set();
      for (let i = 0; i < list.length; i++) {
        const val = optionValue(list[i]);
        const num = parseInt(val, 10);
        const isUsed = !Number.isNaN(num) && used.has(num) && val !== sel;
        const label = optionLabel(list[i]);
        html += '<option value="' + val + '"' + (val === sel ? ' selected' : '') +
          (isUsed ? ' disabled' : '') + '>' + label + '</option>';
      }
      return html;
    }
    function refreshWateringSelects() {
      const usedByType = {};
      document.querySelectorAll('select.watering-select').forEach((el) => {
        const type = el.dataset.type;
        if (!usedByType[type]) {
          const base = wateringUsed[type] || [];
          usedByType[type] = new Set(base.map((v) => parseInt(v, 10)).filter((v) => !Number.isNaN(v)));
        }
        if (type !== 'relay') return;
        const v = parseInt(el.value || el.dataset.selected || '', 10);
        if (!Number.isNaN(v)) usedByType[type].add(v);
      });
      document.querySelectorAll('select.watering-select').forEach((el) => {
        const type = el.dataset.type;
        const selected = el.value || el.dataset.selected || '';
        const list = wateringOptions[type] || [];
        const usedSet = (type === 'relay') ? usedByType[type] : undefined;
        el.innerHTML = buildOptions(list, selected, usedSet);
        el.value = selected || '';
      });
    }
    refreshWateringSelects();
    document.querySelectorAll('select.watering-select').forEach((el) => {
      el.addEventListener('change', refreshWateringSelects);
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
      location.replace(location.pathname + location.search);
    }
    if (wateringForm) {
      wateringForm.addEventListener('submit', () => {
        sessionStorage.setItem(reloadKey, '1');
      });
    }
    function wateringStateLabel(item) {
      if (item && item.active) return '%WATERING_JS_STATE_ACTIVE%';
      if (item && item.paused) return '%WATERING_JS_STATE_PAUSED%';
      return '%WATERING_JS_STATE_WAIT%';
    }
    function applyWateringState(payload) {
      if (!payload || typeof payload !== 'object') return;
      const map = new Map();
      (payload.items || []).forEach((it) => map.set(Number(it.id), it));
      document.querySelectorAll('.tile[data-rule-id]').forEach((tile) => {
        const id = Number(tile.dataset.ruleId || 0);
        if (!map.has(id)) return;
        const it = map.get(id);
        tile.dataset.active = it.active ? '1' : '0';
        tile.classList.toggle('disabled', !it.enabled);
        const badge = tile.querySelector('.badge');
        if (badge) {
          badge.textContent = (it.enabled ? '%WATERING_JS_ON%' : '%WATERING_JS_OFF%') + ' • ' + wateringStateLabel(it);
        }
      });
    }
    let wateringPollBusy = false;
    async function pollWateringState() {
      if (wateringPollBusy) return;
      wateringPollBusy = true;
      try {
        const url = new URL('/watering/state', window.location.origin);
        const cur = new URL(window.location.href);
        const unit = cur.searchParams.get('unit');
        const node = cur.searchParams.get('node');
        if (unit) url.searchParams.set('unit', unit);
        if (node) url.searchParams.set('node', node);
        const res = await fetch(url.toString(), { credentials: 'same-origin' });
        if (!res.ok) throw new Error('state fetch failed');
        const payload = await res.json();
        if (!payload.pending) applyWateringState(payload);
      } catch (_) {
      } finally {
        wateringPollBusy = false;
      }
    }
    setTimeout(pollWateringState, 600);
    setInterval(pollWateringState, 2000);
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








