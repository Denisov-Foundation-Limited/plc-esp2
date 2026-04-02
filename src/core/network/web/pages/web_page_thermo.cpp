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

#include <Arduino.h>
#include "core/network/web/pages/web_page_thermo.hpp"

const char kWebInterfaceThermoHtml[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>%THERMO_PAGE_TITLE%</title>
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
    .field.temp { width: 100%; }
    .field.name { min-width: 160px; }
    .field:disabled,
    select.field:disabled,
    input.field[readonly] {
      color: var(--muted);
      -webkit-text-fill-color: var(--muted);
      background: #0a1220;
      border-color: #1a2436;
      cursor: not-allowed;
    }
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
      box-shadow: 0 0 0 1px rgba(0,0,0,0.3);
    }
    input:checked + .track { background: #22c55e; }
    input:checked + .track .knob { transform: translateX(20px); }
    input:disabled + .track {
      background: #475569;
      border-color: #334155;
      cursor: not-allowed;
    }
    input:disabled + .track .knob {
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
      grid-template-columns: minmax(0, 220px) minmax(0, 1fr);
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
    .thermo-visual {
      position: relative;
      height: 240px;
      border-radius: 16px;
      border: 2px solid #1f2937;
      background: linear-gradient(180deg, #0a1220 0%, #0c1628 100%);
      overflow: hidden;
      cursor: pointer;
    }
    .thermo-left {
      display: flex;
      flex-direction: column;
      gap: 8px;
    }
    .icon {
      position: absolute;
      left: 50%;
      top: 50%;
      transform: translate(-50%, -50%);
      width: 120px;
      height: 120px;
      opacity: 0.2;
      transition: opacity .2s ease, transform .2s ease;
    }
    .icon.heat { color: #f97316; }
    .icon.cool { color: #38bdf8; }
    .icon.active { opacity: 1; transform: translate(-50%, -50%) scale(1.02); }
    .icon.inactive { opacity: 0.18; }
    .temp-pill {
      position: absolute;
      left: 50%;
      transform: translateX(-50%);
      padding: 2px 10px;
      border-radius: 999px;
      border: 1px solid #1f2937;
      background: rgba(17, 24, 39, 0.85);
      color: var(--text);
      font-size: 12px;
      letter-spacing: 0.2px;
      z-index: 2;
      white-space: nowrap;
    }
    .temp-pill .temp-value {
      font-weight: 700;
      color: #38bdf8;
    }
    .temp-pill.sensor { top: 10px; }
    .temp-pill.target { bottom: 10px; }
    .status-dot {
      display: inline-block;
      width: 10px;
      height: 10px;
      border-radius: 50%;
      box-shadow: 0 0 0 2px rgba(15, 23, 42, 0.6);
    }
    .status-heat { background: #f97316; }
    .status-cool { background: #38bdf8; }
    .status-idle { background: #64748b; }
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
    .form-row.full {
      grid-column: 1 / -1;
    }
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
    .form-row > label.switch {
      justify-self: start;
    }
    .status-line {
      display: flex;
      align-items: center;
      gap: 8px;
      color: var(--muted);
      font-size: 12px;
      margin-top: 8px;
    }
    .status-line .status-value {
      font-weight: 700;
      color: #38bdf8;
    }
    .status-line .status-value.status-text-heat { color: #f97316; }
    .status-line .status-value.status-text-cool { color: #38bdf8; }
    .status-line .status-value.status-text-idle { color: #64748b; }
    .tile .field.name { margin-bottom: 10px; }
    @media (max-width: 900px) {
      .wrap { margin: 20px auto; }
      .grid { grid-template-columns: 1fr; }
      .tile { grid-template-columns: 1fr; }
      .thermo-visual { height: 220px; }
      .form-grid { grid-template-columns: 1fr; }
    }
  </style>
</head>
<body>
  <div class="wrap">
    <div class="card">
      %NAV%
      <h1>%THERMO_PAGE_TITLE%</h1>
      %THERMO_DEVICE_SELECT%
      %THERMO_PAGINATION%
      <form method="POST" action="/thermo" id="thermo-form">
        %THERMO_FORM_HIDDEN%
        <div class="grid" id="thermo-grid">
          %THERMO_ROWS%
        </div>
        <div class="actions">
          %THERMO_SAVE_BTN%
        </div>
      </form>
    </div>
  </div>
  <script>
    const thermoOptions = {
      relay: %THERMO_RELAY_JSON%,
      dinput: %THERMO_DINPUT_JSON%
    };
    const thermoUsed = {
      relay: %THERMO_RELAY_USED_JSON%,
      dinput: %THERMO_DINPUT_USED_JSON%
    };
    const thermoGrid = document.getElementById('thermo-grid');
    const thermoPageValue = (() => {
      const url = new URL(window.location.href);
      return url.searchParams.get('page') || '1';
    })();
    async function loadThermoList() {
      if (!thermoGrid) return;
      try {
        const url = new URL('/thermo/list', window.location.origin);
        const cur = new URL(window.location.href);
        const unit = cur.searchParams.get('unit');
        const node = cur.searchParams.get('node');
        if (unit) url.searchParams.set('unit', unit);
        if (node) url.searchParams.set('node', node);
        url.searchParams.set('page', thermoPageValue);
        const res = await fetch(url.toString(), { cache: 'no-store', credentials: 'same-origin' });
        if (!res.ok) {
          thermoGrid.innerHTML = '<div class="tile empty">WEB busy</div>';
          return;
        }
        thermoGrid.innerHTML = await res.text();
        bindThermoHandlers();
        refreshThermoSelects();
      } catch (e) {
        thermoGrid.innerHTML = '<div class="tile empty">WEB busy</div>';
      }
    }
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
    function refreshThermoSelects() {
      const usedByType = {};
      const readSelected = (el) => {
        if (el.dataset.rendered === '1') return String(el.value || '');
        return String(el.dataset.selected || el.value || '');
      };
      document.querySelectorAll('select.thermo-select').forEach((el) => {
        const type = el.dataset.type;
        if (!usedByType[type]) {
          const base = thermoUsed[type] || [];
          usedByType[type] = new Set(base.map((v) => parseInt(v, 10)).filter((v) => !Number.isNaN(v)));
        }
        const v = parseInt(readSelected(el), 10);
        if (!Number.isNaN(v)) usedByType[type].add(v);
      });
      document.querySelectorAll('select.thermo-select').forEach((el) => {
        const type = el.dataset.type;
        const selected = readSelected(el);
        const list = thermoOptions[type] || [];
        el.innerHTML = buildOptions(list, selected, type, usedByType[type]);
        el.value = selected || '';
        el.dataset.rendered = '1';
      });
    }
    const thermoForm = document.getElementById('thermo-form');
    if (thermoForm) {
      const markDirty = () => { window.__plcDirty = true; };
      thermoForm.addEventListener('input', markDirty);
      thermoForm.addEventListener('change', markDirty);
    }
    const reloadKey = 'thermo_reload_left';
    const isStackView = (() => {
      const unit = document.querySelector('input[name="unit"]');
      return !!unit && unit.value === 'stack';
    })();
    const reloadLeftRaw = sessionStorage.getItem(reloadKey);
    const reloadLeft = reloadLeftRaw ? parseInt(reloadLeftRaw, 10) : 0;
    if (!Number.isNaN(reloadLeft) && reloadLeft > 0) {
      sessionStorage.setItem(reloadKey, String(reloadLeft - 1));
      setTimeout(() => {
        location.replace(location.pathname + location.search);
      }, 700);
    } else {
      sessionStorage.removeItem(reloadKey);
    }
    function submitThermoForm() {
      if (!thermoForm) return;
      sessionStorage.setItem(reloadKey, isStackView ? '3' : '1');
      if (typeof thermoForm.requestSubmit === 'function') {
        thermoForm.requestSubmit();
      } else {
        thermoForm.submit();
      }
    }
    function bindThermoHandlers() {
      document.querySelectorAll('select.thermo-select').forEach((el) => {
        if (el.dataset.boundSelect === '1') return;
        el.dataset.boundSelect = '1';
        el.addEventListener('change', refreshThermoSelects);
      });
      document.querySelectorAll('input.thermo-power').forEach((el) => {
        if (el.dataset.boundPower === '1') return;
        el.dataset.boundPower = '1';
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
          const idMatch = name ? name.match(/^t(\d+)_power$/) : null;
          const id = idMatch ? parseInt(idMatch[1], 10) : 0;
          try {
            if (!id) throw new Error('bad id');
            const expectedOn = !!el.checked;
            const state = await postThermoToggle(id, expectedOn ? 'on' : 'off');
            const isOn = state.known ? !!state.power : el.checked;
            el.checked = isOn;
            if (hidden) hidden.value = isOn ? 'on' : 'off';
            const working = (!!state.heat || !!state.cool);
            if (!expectedOn || !isOn || working) {
              applyThermoPowerUi(tile, isOn, !!state.heat, !!state.cool);
            }
            scheduleThermoStateRefresh(id, tile, el, hidden, reqId, expectedOn);
          } catch (e) {
            if (el.dataset.reqId !== reqId) return;
            el.checked = prev;
            if (hidden) hidden.value = prev ? 'on' : 'off';
            applyThermoPowerUi(tile, prev, false, false);
          } finally {
            if (el.dataset.reqId === reqId) {
              el.dataset.busy = '0';
            }
          }
        });
      });
      document.querySelectorAll('input.thermo-enable').forEach((el) => {
        if (el.dataset.boundEnable === '1') return;
        el.dataset.boundEnable = '1';
        el.addEventListener('change', () => {
          if (el.dataset.busy === '1') return;
          el.dataset.busy = '1';
          const tile = el.closest('.tile');
          if (tile) {
            const enForce = tile.querySelector('input[type="hidden"][name$="_en_force"]');
            if (enForce) {
              enForce.value = '';
            }
            if (!el.checked) {
              const nameField = tile.querySelector('input.field.name');
              if (nameField) nameField.value = '';
              const sensor = tile.querySelector('select[name$="_sensor"]');
              if (sensor) sensor.value = '';
              const mode = tile.querySelector('select[name$="_mode"]');
              if (mode) mode.value = 'auto';
              const target = tile.querySelector('input[name$="_target"]');
              if (target) target.value = '22';
              const hyst = tile.querySelector('input[name$="_hyst"]');
              if (hyst) hyst.value = '1';
              tile.querySelectorAll('select.thermo-select').forEach((sel) => {
                sel.value = '';
                sel.dataset.selected = '';
              });
              const power = tile.querySelector('input.thermo-power');
              if (power) {
                power.checked = false;
                power.disabled = true;
              }
              const powerHidden = tile.querySelector('input[type="hidden"][name$="_power"]');
              if (powerHidden) powerHidden.value = 'off';
              refreshThermoSelects();
            }
          }
          if (thermoForm) {
            submitThermoForm();
            setTimeout(() => {
              el.dataset.busy = '0';
            }, 1500);
          } else {
            el.dataset.busy = '0';
          }
        });
      });
      document.querySelectorAll('.thermo-visual').forEach((el) => {
        if (el.dataset.boundClick === '1') return;
        el.dataset.boundClick = '1';
        el.addEventListener('click', () => {
          const tile = el.closest('.tile');
          if (!tile) return;
          const power = tile.querySelector('input.thermo-power');
          if (!power || power.disabled) return;
          power.click();
        });
      });
    }
    async function postThermoToggle(id, action) {
      let body = 'id=' + encodeURIComponent(String(id)) + '&action=' + encodeURIComponent(action || 'toggle');
      if (isStackView) {
        const node = document.querySelector('input[name="node"]');
        if (node && node.value) {
          body += '&node_id=' + encodeURIComponent(node.value);
        }
      }
      const res = await fetch('/thermo/toggle', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body,
        credentials: 'same-origin'
      });
      if (!res.ok) throw new Error('toggle failed');
      const contentType = (res.headers.get('content-type') || '').toLowerCase();
      if (contentType.indexOf('application/json') >= 0) {
        const data = await res.json();
        return {
          known: true,
          power: !!data.power,
          heat: !!data.heat,
          cool: !!data.cool
        };
      }
      const raw = (await res.text()).trim().toLowerCase();
      if (raw === 'on' || raw === 'off') {
        return { known: true, power: raw === 'on', heat: false, cool: false };
      }
      return { known: false, power: false, heat: false, cool: false };
    }
    function applyThermoPowerUi(tile, isOn, heatOn, coolOn) {
      if (!tile) return;
      const badge = Array.from(tile.querySelectorAll('.status-line .badge')).find((b) => {
        const t = (b.textContent || '').toLowerCase();
        return t.indexOf('on') >= 0 || t.indexOf('off') >= 0;
      });
      if (badge) {
        const txt = badge.textContent || '';
        const p = txt.indexOf(':');
        if (p >= 0) badge.textContent = txt.slice(0, p + 1) + ' ' + (isOn ? 'on' : 'off');
      }
      const heatIcon = tile.querySelector('.icon.heat');
      const coolIcon = tile.querySelector('.icon.cool');
      const activeHeat = !!isOn && !!heatOn;
      const activeCool = !!isOn && !!coolOn;
      if (heatIcon) {
        heatIcon.classList.toggle('active', activeHeat);
        heatIcon.classList.toggle('inactive', !activeHeat);
      }
      if (coolIcon) {
        coolIcon.classList.toggle('active', activeCool);
        coolIcon.classList.toggle('inactive', !activeCool);
      }
      const dot = tile.querySelector('.status-dot');
      const sv = tile.querySelector('.status-value');
      if (dot) {
        dot.classList.remove('status-heat', 'status-cool', 'status-idle');
        if (activeHeat) dot.classList.add('status-heat');
        else if (activeCool) dot.classList.add('status-cool');
        else dot.classList.add('status-idle');
      }
      if (sv) {
        sv.classList.remove('status-text-heat', 'status-text-cool', 'status-text-idle');
        if (activeHeat) {
          sv.classList.add('status-text-heat');
          sv.textContent = '%THERMO_JS_HEAT%';
        } else if (activeCool) {
          sv.classList.add('status-text-cool');
          sv.textContent = '%THERMO_JS_COOL%';
        } else {
          sv.classList.add('status-text-idle');
          sv.textContent = '%THERMO_JS_IDLE%';
        }
      }
    }
    function scheduleThermoStateRefresh(id, tile, el, hidden, reqId, expectedOn) {
      const maxAttempts = 12;
      const delayMs = 350;
      let attempt = 0;
      const tick = async () => {
        if (!el || el.dataset.reqId !== reqId) return;
        attempt++;
        try {
          const st = await postThermoToggle(id, 'state');
          if (!el || el.dataset.reqId !== reqId) return;
          if (st.known) {
            el.checked = !!st.power;
            if (hidden) hidden.value = st.power ? 'on' : 'off';
            const working = (!!st.heat || !!st.cool);
            if (!expectedOn || !st.power || working || attempt >= maxAttempts) {
              applyThermoPowerUi(tile, !!st.power, !!st.heat, !!st.cool);
              return;
            }
          }
        } catch (e) {}
        if (attempt < maxAttempts && el && el.dataset.reqId === reqId) {
          setTimeout(tick, delayMs);
        }
      };
      setTimeout(tick, 220);
    }
    refreshThermoSelects();
    {
      const qs = (new URL(window.location.href)).searchParams;
      if (qs.get('unit') === 'stack' || qs.get('node') || qs.get('node_id')) {
      loadThermoList();
      }
    }
    bindThermoHandlers();
    function thermoTileId(tile) {
      if (!tile) return 0;
      const power = tile.querySelector('input.thermo-power');
      const action = power ? (power.dataset.action || '') : '';
      const m = action.match(/^t(\d+)_power$/);
      return m ? (parseInt(m[1], 10) || 0) : 0;
    }
    async function pollThermoStates() {
      const powers = Array.from(document.querySelectorAll('input.thermo-power'));
      for (let i = 0; i < powers.length; i++) {
        const el = powers[i];
        if (!el || el.dataset.busy === '1') continue;
        const tile = el.closest('.tile');
        const id = thermoTileId(tile);
        if (!id) continue;
        const hidden = document.querySelector('input[name="t' + String(id) + '_power"]');
        try {
          const st = await postThermoToggle(id, 'state');
          if (!st || !st.known) continue;
          el.checked = !!st.power;
          if (hidden) hidden.value = st.power ? 'on' : 'off';
          applyThermoPowerUi(tile, !!st.power, !!st.heat, !!st.cool);
        } catch (e) {}
      }
    }
    setInterval(pollThermoStates, 2000);
    setTimeout(pollThermoStates, 500);
    const scrollKey = 'thermo_scroll_y';
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
    const thermoDevice = document.getElementById('thermo-device');
    if (thermoDevice) {
      thermoDevice.addEventListener('change', () => {
        const val = thermoDevice.value || 'local';
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





