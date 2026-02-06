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

static const char kWebInterfaceTanksHtml[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Баки</title>
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
    .field.mini { padding: 4px 6px; width: 72px; }
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
    .tile.disabled { opacity: 0.55; }
    .tile-head {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 10px;
      margin-bottom: 8px;
    }
    .tank-visual {
      position: relative;
      height: 240px;
      border-radius: 16px;
      border: 2px solid #1f2937;
      background: linear-gradient(180deg, #0a1220 0%, #0c1628 100%);
      overflow: hidden;
    }
    .tank-fill {
      position: absolute;
      left: 0;
      right: 0;
      bottom: 0;
      height: 10%;
      background: linear-gradient(180deg, #0ea5e9 0%, #0284c7 100%);
      transition: height .2s ease;
    }
    .tank-fill.level-low { background: linear-gradient(180deg, #38bdf8 0%, #0ea5e9 100%); }
    .tank-fill.level-mid { background: linear-gradient(180deg, #38bdf8 0%, #0ea5e9 100%); }
    .tank-fill.level-full { background: linear-gradient(180deg, #38bdf8 0%, #0ea5e9 100%); }
    .tank-fill.level-empty { background: linear-gradient(180deg, #dc2626 0%, #7f1d1d 100%); }
    .tank-label {
      position: absolute;
      top: 10px;
      left: 50%;
      transform: translateX(-50%);
      padding: 3px 10px;
      border-radius: 999px;
      border: 1px solid #1f2937;
      background: rgba(17, 24, 39, 0.85);
      color: var(--text);
      font-size: 12px;
      letter-spacing: 0.2px;
    }
    .form-grid {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 10px;
    }
    .status-row {
      display: flex;
      align-items: center;
      gap: 10px;
      margin-top: 6px;
      color: var(--muted);
      font-size: 12px;
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
    .tile .field.name {
      margin-bottom: 8px;
    }
    .form-row {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 8px;
    }
    .form-row > label:not(.switch) {
      color: var(--muted);
      font-size: 12px;
      min-width: 72px;
    }
    .form-row > label.switch {
      min-width: 0;
    }
    .status-line {
      display: flex;
      align-items: center;
      gap: 10px;
      color: var(--muted);
      font-size: 12px;
      margin-top: 6px;
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
      .tile { grid-template-columns: 1fr; }
      .tank-visual { height: 220px; }
      .form-grid { grid-template-columns: 1fr; }
      .grid { grid-template-columns: 1fr; }
    }
  </style>
</head>
<body>
  <div class="wrap">
    %NAV%
    <div class="card">
      <h1>Баки</h1>
      <div class="status">%TANK_STATUS%</div>
      %TANK_DEVICE_SELECT%
      <form method="POST" action="/tanks" id="tanks-form">
        <div class="grid">
          %TANK_ITEMS%
        </div>
        <div class="actions">
          %TANK_SAVE_BTN%
        </div>
      </form>
    </div>
  </div>
  <script>
    const tankOptions = {
      dinput: %TANK_DINPUT_JSON%,
      relay: %TANK_RELAY_JSON%
    };
    const tankUsed = {
      dinput: %TANK_DINPUT_USED_JSON%,
      relay: %TANK_RELAY_USED_JSON%
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
    function buildOptions(list, selected, type, usedSet) {
      const sel = String(selected || '');
      let html = '<option value=""' + (sel === '' ? ' selected' : '') + '>-</option>';
      const used = usedSet || new Set();
      for (let i = 0; i < list.length; i++) {
        const val = optionValue(list[i]);
        const num = parseInt(val, 10);
        const isUsed = !Number.isNaN(num) && used.has(num) && val !== sel;
        const label = optionLabel(list[i], type);
        html += '<option value="' + val + '"' + (val === selected ? ' selected' : '') +
          (isUsed ? ' disabled' : '') + '>' + label + '</option>';
      }
      return html;
    }
    function refreshTankSelects() {
      const usedByType = {};
      document.querySelectorAll('select.tank-select').forEach((el) => {
        const type = el.dataset.type;
        if (!usedByType[type]) {
          const base = tankUsed[type] || [];
          usedByType[type] = new Set(base.map((v) => parseInt(v, 10)).filter((v) => !Number.isNaN(v)));
        }
        const v = parseInt(el.value || el.dataset.selected || '', 10);
        if (!Number.isNaN(v)) usedByType[type].add(v);
      });
      document.querySelectorAll('select.tank-select').forEach((el) => {
        const type = el.dataset.type;
        const selected = el.value || el.dataset.selected || '';
        const list = tankOptions[type] || [];
        el.innerHTML = buildOptions(list, selected, type, usedByType[type]);
        el.value = selected || '';
      });
    }
    refreshTankSelects();
    document.querySelectorAll('select.tank-select').forEach((el) => {
      el.addEventListener('change', refreshTankSelects);
    });
    function updateTankEnabled(tile, enabled) {
      if (!tile) return;
      tile.classList.toggle('disabled', !enabled);
      if (!enabled) {
        const name = tile.querySelector('input.field.name');
        if (name) name.value = '';
        tile.querySelectorAll('select.tank-select').forEach((sel) => {
          sel.value = '';
          sel.dataset.selected = '';
        });
        const power = tile.querySelector('input.tank-power');
        if (power) {
          power.checked = false;
          power.disabled = true;
        }
        const powerHidden = tile.querySelector('input[type="hidden"][name$="_power"]');
        if (powerHidden) powerHidden.value = 'off';
        refreshTankSelects();
      }
    }
    document.querySelectorAll('input[type="checkbox"][name^="k"][name$="_en"]').forEach((el) => {
      el.addEventListener('change', () => {
        const tile = el.closest('.tile');
        updateTankEnabled(tile, el.checked);
        if (!el.checked && tanksForm) {
          sessionStorage.setItem(reloadKey, '1');
          tanksForm.submit();
        }
      });
    });
    const tanksForm = document.getElementById('tanks-form');
    let tanksDirty = false;
    const markDirty = () => {
      tanksDirty = true;
      window.__plcDirty = true;
    };
    if (tanksForm) {
      tanksForm.addEventListener('input', markDirty);
      tanksForm.addEventListener('change', markDirty);
    }
    const reloadKey = 'tanks_reload';
    if (sessionStorage.getItem(reloadKey)) {
      sessionStorage.removeItem(reloadKey);
      location.replace(location.pathname);
    }
    if (tanksForm) {
      tanksForm.addEventListener('submit', () => {
        sessionStorage.setItem(reloadKey, '1');
      });
    }
    document.querySelectorAll('input.tank-power').forEach((el) => {
      el.addEventListener('change', () => {
        if (el.dataset.busy === '1') return;
        el.dataset.busy = '1';
        el.disabled = true;
        const name = el.dataset.action;
        const hidden = document.querySelector('input[name="' + name + '"]');
        if (hidden) {
          hidden.value = el.checked ? 'on' : 'off';
        }
        if (tanksForm) {
          tanksForm.submit();
        }
      });
    });
    const scrollKey = 'tanks_scroll_y';
    const savedScroll = sessionStorage.getItem(scrollKey);
    if (savedScroll) {
      const y = parseInt(savedScroll, 10);
      if (!Number.isNaN(y)) {
        window.scrollTo(0, y);
      }
    }
    window.addEventListener('scroll', () => {
      sessionStorage.setItem(scrollKey, String(window.scrollY));
    }, { passive: true });
    const tanksDevice = document.getElementById('tanks-device');
    if (tanksDevice) {
      tanksDevice.addEventListener('change', () => {
        const val = tanksDevice.value || 'local';
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
