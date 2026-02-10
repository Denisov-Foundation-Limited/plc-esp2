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

static const char kWebInterfaceLeakHtml[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Протечки</title>
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
    body {
      margin: 0;
      min-height: 100vh;
      font-family: "Segoe UI", Tahoma, Arial, sans-serif;
      background: radial-gradient(1200px 600px at 10% -10%, #1f2937 0%, #0b1220 60%, #080d17 100%);
      color: var(--text);
    }
    .wrap { max-width: 1200px; margin: 28px auto; padding: 0 14px; }
    .card {
      background: linear-gradient(180deg, #0f172a 0%, #0b1220 100%);
      border: 1px solid var(--border);
      border-radius: 14px;
      padding: 18px;
      box-shadow: 0 10px 30px rgba(0,0,0,0.35);
    }
    h1 { margin: 0 0 6px; font-size: 22px; }
    .status { color: var(--muted); font-size: 12px; margin-bottom: 10px; }

    .grid {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 14px;
      margin-top: 10px;
    }
    .tile {
      border: 1px solid var(--border);
      border-radius: 12px;
      padding: 12px;
      background: #0b1220;
      display: grid;
      gap: 10px;
    }
    .tile.disabled { opacity: 0.55; }
    .tile.alert {
      border-color: #7f1d1d;
      box-shadow: inset 0 0 0 1px rgba(220, 38, 38, 0.35);
    }
    .tile-empty {
      text-align: center;
      color: var(--muted);
      padding: 20px;
    }
    .tile-left {
      display: inline-flex;
      align-items: center;
      gap: 8px;
      min-width: 0;
    }
    .tile-head {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 8px;
      border-bottom: 1px solid var(--border);
      padding-bottom: 8px;
    }
    .tile-id {
      font-weight: 700;
      color: #93c5fd;
    }
    .leak-icon {
      width: 22px;
      height: 22px;
      color: #38bdf8;
      opacity: 0.9;
      flex: 0 0 auto;
    }
    .tile.alert .leak-icon {
      color: #ef4444;
      filter: drop-shadow(0 0 6px rgba(239, 68, 68, 0.35));
    }
    .badge-row {
      display: flex;
      gap: 6px;
      flex-wrap: wrap;
    }
    .badge {
      padding: 2px 8px;
      border-radius: 999px;
      border: 1px solid var(--border);
      background: #111827;
      color: var(--text);
      font-size: 11px;
    }
    .tile-grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 8px 10px;
    }
    .field-row {
      display: grid;
      gap: 4px;
    }
    .field-row.full {
      grid-column: span 2;
    }
    .field-label {
      color: var(--muted);
      font-size: 12px;
    }
    input[type="text"], select {
      width: 100%;
      background: #0b1220;
      color: var(--text);
      border: 1px solid #334155;
      border-radius: 8px;
      padding: 7px 8px;
      font-size: 12px;
    }
    .checks {
      grid-column: span 2;
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 6px 10px;
      padding-top: 4px;
    }
    .checks label {
      color: var(--muted);
      font-size: 12px;
      display: inline-flex;
      align-items: center;
      gap: 6px;
    }

    .actions { display: flex; gap: 10px; margin-top: 12px; flex-wrap: wrap; }
    button {
      border: none;
      border-radius: 8px;
      padding: 9px 12px;
      font-weight: 700;
      cursor: pointer;
      background: var(--accent);
      color: #00111a;
    }
    .btn-muted { background: #64748b; color: #fff; }
    a { color: #7dd3fc; text-decoration: none; }
    .nav { margin-bottom: 12px; }

    @media (max-width: 900px) {
      .grid { grid-template-columns: 1fr; }
      .tile-grid { grid-template-columns: 1fr; }
      .field-row.full,
      .checks { grid-column: span 1; }
      .checks { grid-template-columns: 1fr; }
    }
  </style>
</head>
<body>
  <div class="wrap">
    <div class="card">
      %NAV%
      <h1>Протечки</h1>
      %LEAK_DEVICE_SELECT%
      <div class="status">%LEAK_STATUS%</div>
      <form method="POST" action="%LEAK_FORM_ACTION%" id="leak-save">
        <input type="hidden" name="leak_save" value="1">
        <div class="grid">
          %LEAK_ROWS%
        </div>
      </form>
      <form method="POST" action="%LEAK_ACK_FORM_ACTION%" id="leak-ack">
        <input type="hidden" name="leak_ack_all" value="1">
      </form>
      <div class="actions">
        <button type="submit" form="leak-save">Сохранить</button>
        <button type="submit" form="leak-ack" class="btn-muted">Сброс тревог</button>
      </div>
      <div class="status">Датчики протечки: DInput. Выходы кран/тревога: Relay.</div>
    </div>
  </div>
  <script>
    const leakOptions = { dinput: %LEAK_DINPUT_JSON%, relay: %LEAK_RELAY_JSON% };
    const leakUsed = { dinput: %LEAK_DINPUT_USED_JSON%, relay: %LEAK_RELAY_USED_JSON% };
    function itemValue(item) { return (item && typeof item === 'object') ? String(item.v) : String(item); }
    function itemLabel(item, type) {
      if (item && typeof item === 'object' && item.l) return item.l;
      const v = itemValue(item);
      return type === 'relay' ? ('rly' + v) : ('in' + v);
    }
    function buildOptions(type, selected, used) {
      const list = leakOptions[type] || [];
      const sel = String(selected || '');
      let html = '<option value=""' + (sel === '' ? ' selected' : '') + '>-</option>';
      for (let i = 0; i < list.length; i++) {
        const val = itemValue(list[i]);
        const n = parseInt(val, 10);
        const busy = !Number.isNaN(n) && used.has(n) && val !== sel;
        html += '<option value="' + val + '"' + (val === sel ? ' selected' : '') + (busy ? ' disabled' : '') + '>' + itemLabel(list[i], type) + '</option>';
      }
      return html;
    }
    function refreshLeakSelects() {
      const byType = {};
      document.querySelectorAll('select.leak-select').forEach((el) => {
        const type = el.dataset.type;
        if (!byType[type]) {
          byType[type] = new Set((leakUsed[type] || []).map((v) => parseInt(v, 10)).filter((v) => !Number.isNaN(v)));
        }
        const v = parseInt(el.value || el.dataset.selected || '', 10);
        if (!Number.isNaN(v)) byType[type].add(v);
      });
      document.querySelectorAll('select.leak-select').forEach((el) => {
        const type = el.dataset.type;
        const sel = el.value || el.dataset.selected || '';
        el.innerHTML = buildOptions(type, sel, byType[type] || new Set());
        el.value = sel;
      });
    }
    refreshLeakSelects();
    document.querySelectorAll('select.leak-select').forEach((el) => el.addEventListener('change', refreshLeakSelects));
    function updateLeakEnabled(tile, enabled) {
      if (!tile) return;
      tile.classList.toggle('disabled', !enabled);
      const power = tile.querySelector('input[type="checkbox"][name^="leak_pwr_"]');
      const activeLow = tile.querySelector('input[type="checkbox"][name^="leak_al_"]');
      if (power) power.disabled = !enabled;
      if (activeLow) activeLow.disabled = !enabled;
      if (!enabled) {
        const name = tile.querySelector('input[type="text"][name^="leak_name_"]');
        if (name) name.value = '';
        tile.querySelectorAll('select.leak-select').forEach((sel) => {
          sel.value = '';
          sel.dataset.selected = '';
        });
        if (power) power.checked = false;
        if (activeLow) activeLow.checked = false;
        refreshLeakSelects();
      }
    }
    const leakForm = document.getElementById('leak-save');
    const reloadKey = 'leak_reload';
    if (sessionStorage.getItem(reloadKey)) {
      sessionStorage.removeItem(reloadKey);
      location.replace(location.pathname + location.search);
    }
    if (leakForm) {
      leakForm.addEventListener('submit', () => {
        sessionStorage.setItem(reloadKey, '1');
      });
    }
    document.querySelectorAll('input[type="checkbox"][name^="leak_en_"]').forEach((el) => {
      const tile = el.closest('.tile');
      updateLeakEnabled(tile, el.checked);
      el.addEventListener('change', () => {
        const curr = el.closest('.tile');
        updateLeakEnabled(curr, el.checked);
        if (!el.checked && leakForm) {
          sessionStorage.setItem(reloadKey, '1');
          leakForm.submit();
        }
      });
    });
    (function () {
      const sel = document.getElementById('leak-device');
      if (!sel) return;
      sel.addEventListener('change', () => {
        const v = sel.value;
        if (!v || v === '0' || v === 'local') {
          location.href = '/leak';
          return;
        }
        location.href = '/leak?node=' + encodeURIComponent(v) + '&unit=stack';
      });
    })();
  </script>
</body>
</html>
)HTML";
