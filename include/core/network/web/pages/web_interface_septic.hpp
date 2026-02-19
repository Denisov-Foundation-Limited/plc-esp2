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
    .switch input:checked + .track { background: #22c55e; }
    .switch input:checked + .track .knob { transform: translateX(18px); }
    .grid {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 14px;
      margin-top: 10px;
    }
    .tile {
      display: grid;
      grid-template-columns: 220px 1fr;
      gap: 14px;
      padding: 16px;
      border-radius: 12px;
      border: 1px solid #1f2937;
      background: #0b1220;
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
    .status-dot {
      display: inline-block;
      width: 10px;
      height: 10px;
      border-radius: 50%;
      box-shadow: 0 0 0 2px rgba(15, 23, 42, 0.6);
    }
    .status-on { background: #22c55e; }
    .status-off { background: #64748b; }
    .septic-visual {
      position: relative;
      height: 240px;
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
      z-index: 1;
      transition: height .3s ease, background .3s ease;
    }
    .water-low {
      background: linear-gradient(180deg, #0ea5e9 0%, #0284c7 100%);
    }
    .water-warn {
      background: linear-gradient(180deg, #facc15 0%, #eab308 100%);
    }
    .water-alarm {
      background: linear-gradient(180deg, #dc2626 0%, #7f1d1d 100%);
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
      z-index: 2;
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
    .form-row > label.switch {
      justify-self: start;
    }
    .form-row .field,
    .form-row select {
      width: 100%;
      min-width: 0;
      max-width: 100%;
    }
    .status-grid {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 8px 12px;
      margin-top: 8px;
      color: var(--muted);
      font-size: 12px;
    }
    .status-line {
      display: flex;
      align-items: center;
      gap: 8px;
    }
    .tile .field.name {
      margin-bottom: 10px;
    }
    @media (max-width: 900px) {
      .wrap { margin: 20px auto; }
      .grid { grid-template-columns: 1fr; }
      .tile { grid-template-columns: 1fr; }
      .septic-visual { height: 220px; }
      .form-grid { grid-template-columns: 1fr; }
      .status-grid { grid-template-columns: 1fr; }
    }
  </style>
</head>
<body>
  <div class="wrap">
    %NAV%
    <div class="card">
      <h1>Септик</h1>
      <div class="status">%SEPTIC_STATUS%</div>
      %SEPTIC_DEVICE_SELECT%
      <form method="POST" action="/septic" id="septic-form">
        <div class="grid">
          %SEPTIC_ITEMS%
        </div>
        <div class="actions">
          %SEPTIC_SAVE_BTN%
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
    function refreshSepticSelects() {
      const usedByType = {};
      document.querySelectorAll('select.septic-select').forEach((el) => {
        const type = el.dataset.type;
        if (!usedByType[type]) {
          const base = septicUsed[type] || [];
          usedByType[type] = new Set(base.map((v) => parseInt(v, 10)).filter((v) => !Number.isNaN(v)));
        }
        const v = parseInt(el.value || el.dataset.selected || '', 10);
        if (!Number.isNaN(v)) usedByType[type].add(v);
      });
      document.querySelectorAll('select.septic-select').forEach((el) => {
        const type = el.dataset.type;
        const selected = el.value || el.dataset.selected || '';
        const list = septicOptions[type] || [];
        el.innerHTML = buildOptions(list, selected, type, usedByType[type]);
        el.value = selected || '';
      });
    }
    refreshSepticSelects();
    document.querySelectorAll('select.septic-select').forEach((el) => {
      el.addEventListener('change', refreshSepticSelects);
    });
    function updateSepticEnabled(tile, enabled) {
      if (!tile) return;
      tile.classList.toggle('disabled', !enabled);
      if (!enabled) {
        const name = tile.querySelector('input.field.name');
        if (name) name.value = '';
        tile.querySelectorAll('select.septic-select').forEach((sel) => {
          sel.value = '';
          sel.dataset.selected = '';
        });
        const monitor = tile.querySelector('input.septic-monitor');
        if (monitor) {
          monitor.checked = false;
          monitor.disabled = true;
        }
        const monitorHidden = tile.querySelector('input[type="hidden"][name$="_mon"]');
        if (monitorHidden) monitorHidden.value = 'off';
        refreshSepticSelects();
      }
    }
    document.querySelectorAll('input[type="checkbox"][name^="sep"][name$="_en"]').forEach((el) => {
      el.addEventListener('change', () => {
        const tile = el.closest('.tile');
        updateSepticEnabled(tile, el.checked);
        if (!el.checked && septicForm) {
          sessionStorage.setItem(reloadKey, '1');
          septicForm.submit();
        }
      });
    });
    const septicForm = document.getElementById('septic-form');
    const reloadKey = 'septic_reload';
    if (sessionStorage.getItem(reloadKey)) {
      sessionStorage.removeItem(reloadKey);
      location.replace(location.pathname);
    }
    if (septicForm) {
      septicForm.addEventListener('submit', () => {
        sessionStorage.setItem(reloadKey, '1');
      });
    }
    const septicQs = new URLSearchParams(window.location.search);
    const septicIsStackView = septicQs.get('unit') === 'stack';
    const septicNodeId = septicQs.get('node') || '';
    async function postSepticToggle(id, action) {
      let body = 'id=' + encodeURIComponent(String(id)) + '&action=' + encodeURIComponent(action || 'toggle');
      if (septicIsStackView && septicNodeId) {
        body += '&node_id=' + encodeURIComponent(septicNodeId);
      }
      const res = await fetch('/septic/toggle', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body,
        credentials: 'same-origin'
      });
      if (!res.ok) throw new Error('toggle failed');
      const contentType = (res.headers.get('content-type') || '').toLowerCase();
      if (contentType.indexOf('application/json') >= 0) {
        return await res.json();
      }
      const raw = (await res.text()).trim().toLowerCase();
      if (raw === 'pending' || raw === 'unknown') return raw;
      throw new Error('bad state');
    }
    function applySepticStateUi(tile, st) {
      if (!tile || !st) return;
      const dots = tile.querySelectorAll('.status-grid .status-dot');
      if (dots.length > 0) {
        dots[0].classList.remove('status-on', 'status-off');
        dots[0].classList.add(st.warning ? 'status-on' : 'status-off');
      }
      if (dots.length > 1) {
        dots[1].classList.remove('status-on', 'status-off');
        dots[1].classList.add(st.alarm ? 'status-on' : 'status-off');
      }
      if (dots.length > 2) {
        dots[2].classList.remove('status-on', 'status-off');
        dots[2].classList.add(st.relay_warning ? 'status-on' : 'status-off');
      }
      if (dots.length > 3) {
        dots[3].classList.remove('status-on', 'status-off');
        dots[3].classList.add(st.relay_alarm ? 'status-on' : 'status-off');
      }
      let waterClass = 'water-low';
      let waterLevel = '20%';
      let waterLabel = 'Уровень: 20%';
      if (st.alarm) {
        waterClass = 'water-alarm';
        waterLevel = '100%';
        waterLabel = 'Уровень: 100%';
      } else if (st.warning) {
        waterClass = 'water-warn';
        waterLevel = '80%';
        waterLabel = 'Уровень: 80%';
      }
      const liquid = tile.querySelector('.liquid');
      if (liquid) {
        liquid.classList.remove('water-low', 'water-warn', 'water-alarm');
        liquid.classList.add(waterClass);
        liquid.style.height = waterLevel;
      }
      const label = tile.querySelector('.level-label');
      if (label) label.textContent = waterLabel;
    }
    function scheduleSepticStateRefresh(id, tile, el, hidden, reqId) {
      const maxAttempts = 8;
      const delayMs = 350;
      let attempt = 0;
      const tick = async () => {
        if (!el || el.dataset.reqId !== reqId) return;
        attempt++;
        try {
          const st = await postSepticToggle(id, 'state');
          if (!el || el.dataset.reqId !== reqId) return;
          if (typeof st === 'object' && st) {
            el.checked = !!st.monitor;
            if (hidden) hidden.value = st.monitor ? 'on' : 'off';
            applySepticStateUi(tile, st);
            return;
          }
        } catch (e) {}
        if (attempt < maxAttempts && el && el.dataset.reqId === reqId) {
          setTimeout(tick, delayMs);
        }
      };
      setTimeout(tick, 220);
    }
    document.querySelectorAll('input.septic-monitor').forEach((el) => {
      el.addEventListener('change', async () => {
        if (el.dataset.busy === '1') return;
        el.dataset.busy = '1';
        const reqId = String((parseInt(el.dataset.reqId || '0', 10) || 0) + 1);
        el.dataset.reqId = reqId;
        const prev = !el.checked;
        const name = el.dataset.action;
        const hidden = document.querySelector('input[name="' + name + '"]');
        if (hidden) {
          hidden.value = el.checked ? 'on' : 'off';
        }
        const tile = el.closest('.tile');
        const idMatch = name ? name.match(/^sep(\d+)_mon$/) : null;
        const id = idMatch ? parseInt(idMatch[1], 10) : 0;
        try {
          if (!id) throw new Error('bad id');
          const desired = !!el.checked;
          const st = await postSepticToggle(id, desired ? 'on' : 'off');
          if (typeof st === 'object' && st) {
            el.checked = !!st.monitor;
            if (hidden) hidden.value = st.monitor ? 'on' : 'off';
            applySepticStateUi(tile, st);
          }
          scheduleSepticStateRefresh(id, tile, el, hidden, reqId);
        } catch (e) {
          if (el.dataset.reqId !== reqId) return;
          el.checked = prev;
          if (hidden) hidden.value = prev ? 'on' : 'off';
        } finally {
          if (el.dataset.reqId === reqId) {
            el.dataset.busy = '0';
          }
        }
      });
    });
    const septicDevice = document.getElementById('septic-device');
    if (septicDevice) {
      septicDevice.addEventListener('change', () => {
        const val = septicDevice.value || 'local';
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

