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
#include "core/network/web/pages/web_page_tanks.hpp"

const char kWebInterfaceTanksHtml[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>%TANK_PAGE_TITLE%</title>
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
    }    .field.mini { padding: 4px 6px; width: 72px; }
    .field.name { min-width: 160px; }
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
    .tank-status-dot {
      display: inline-block;
      width: 12px;
      height: 12px;
      border-radius: 50%;
      box-shadow: 0 0 0 2px rgba(15, 23, 42, 0.6);
      flex: 0 0 auto;
    }
    .tank-status-on { background: #22c55e; }
    .tank-status-off { background: #64748b; }
    .tank-status-bad { background: #ef4444; }
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
    .form-row.power-row {
      justify-content: flex-start;
      gap: 12px;
      margin-top: 10px;
    }
    .form-row.power-row > label:not(.switch) {
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
      <h1>%TANK_PAGE_TITLE%</h1>
      %TANK_DEVICE_SELECT%
      %TANK_PAGINATION%
      <form method="POST" action="/tanks" id="tanks-form">
        %TANK_FORM_HIDDEN%
        <div class="grid" id="tanks-grid">
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
    const tanksGrid = document.getElementById('tanks-grid');
    const tanksPageValue = (() => {
      const url = new URL(window.location.href);
      return url.searchParams.get('page') || '1';
    })();
    async function loadTanksList() {
      if (!tanksGrid) return;
      try {
        const url = new URL('/tanks/list', window.location.origin);
        const cur = new URL(window.location.href);
        const unit = cur.searchParams.get('unit');
        const node = cur.searchParams.get('node');
        if (unit) url.searchParams.set('unit', unit);
        if (node) url.searchParams.set('node', node);
        url.searchParams.set('page', tanksPageValue);
        const res = await fetch(url.toString(), { cache: 'no-store', credentials: 'same-origin' });
        if (!res.ok) {
          tanksGrid.innerHTML = '<div class="tile empty">WEB busy</div>';
          return;
        }
        tanksGrid.innerHTML = await res.text();
        bindTankHandlers();
        refreshTankSelects();
      } catch (e) {
        tanksGrid.innerHTML = '<div class="tile empty">WEB busy</div>';
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
    function bindTankHandlers() {
      document.querySelectorAll('select.tank-select').forEach((el) => {
        if (el.dataset.boundChange === '1') return;
        el.dataset.boundChange = '1';
        el.addEventListener('change', refreshTankSelects);
      });
      document.querySelectorAll('input[type="checkbox"][name^="k"][name$="_en"]').forEach((el) => {
        if (el.dataset.boundEnable === '1') return;
        el.dataset.boundEnable = '1';
        el.addEventListener('change', () => {
          const tile = el.closest('.tile');
          updateTankEnabled(tile, el.checked);
          if (!el.checked && tanksForm) {
            sessionStorage.setItem(reloadKey, '1');
            tanksForm.submit();
          }
        });
      });
      document.querySelectorAll('input.tank-power').forEach((el) => {
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
          const idMatch = name ? name.match(/^k(\d+)_power$/) : null;
          const id = idMatch ? parseInt(idMatch[1], 10) : 0;
          try {
            if (!id) throw new Error('bad id');
            const desired = !!el.checked;
            const st = await postTankToggle(id, desired ? 'on' : 'off');
            if (typeof st === 'object' && st) {
              el.checked = !!st.power;
              if (hidden) hidden.value = st.power ? 'on' : 'off';
              applyTankStateUi(tile, st);
            }
            scheduleTankStateRefresh(id, tile, el, hidden, reqId);
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
    }
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
    const tanksQs = new URLSearchParams(window.location.search);
    const tanksIsStackView = tanksQs.get('unit') === 'stack';
    const tanksNodeId = tanksQs.get('node') || '';
    async function postTankToggle(id, action) {
      let body = 'id=' + encodeURIComponent(String(id)) + '&action=' + encodeURIComponent(action || 'toggle');
      if (tanksIsStackView && tanksNodeId) {
        body += '&node_id=' + encodeURIComponent(tanksNodeId);
      }
      const res = await fetch('/tanks/toggle', {
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
    function applyTankStateUi(tile, st) {
      if (!tile || !st) return;
      const fill = tile.querySelector('.tank-fill');
      const label = tile.querySelector('.tank-label');
      let levelClass = 'level-empty';
      let levelPct = 0;
      if (st.level_full) {
        levelClass = 'level-full';
        levelPct = 99;
      } else if (st.level_mid) {
        levelClass = 'level-mid';
        levelPct = 66;
      } else if (st.level_low) {
        levelClass = 'level-low';
        levelPct = 33;
      }
      if (fill) {
        fill.classList.remove('level-low', 'level-mid', 'level-full', 'level-empty');
        fill.classList.add(levelClass);
        fill.style.height = String(levelPct) + '%';
      }
      if (label) {
        label.textContent = String(levelPct) + '%';
      }
      const dots = tile.querySelectorAll('.status-row .tank-status-dot');
      if (dots.length > 0) {
        dots[0].classList.remove('tank-status-on', 'tank-status-off', 'tank-status-bad');
        dots[0].classList.add(st.pump ? 'tank-status-on' : 'tank-status-off');
      }
      if (dots.length > 1) {
        dots[1].classList.remove('tank-status-on', 'tank-status-off', 'tank-status-bad');
        dots[1].classList.add(st.valve ? 'tank-status-on' : 'tank-status-off');
      }
      if (dots.length > 2) {
        dots[2].classList.remove('tank-status-on', 'tank-status-off', 'tank-status-bad');
        dots[2].classList.add(st.alarm ? 'tank-status-bad' : 'tank-status-off');
      }
    }
    function scheduleTankStateRefresh(id, tile, el, hidden, reqId) {
      const maxAttempts = 8;
      const delayMs = 350;
      let attempt = 0;
      const tick = async () => {
        if (!el || el.dataset.reqId !== reqId) return;
        attempt++;
        try {
          const st = await postTankToggle(id, 'state');
          if (!el || el.dataset.reqId !== reqId) return;
          if (typeof st === 'object' && st) {
            el.checked = !!st.power;
            if (hidden) hidden.value = st.power ? 'on' : 'off';
            applyTankStateUi(tile, st);
            return;
          }
        } catch (e) {}
        if (attempt < maxAttempts && el && el.dataset.reqId === reqId) {
          setTimeout(tick, delayMs);
        }
      };
      setTimeout(tick, 220);
    }
    refreshTankSelects();
    if (tanksIsStackView) {
      loadTanksList();
    }
    bindTankHandlers();
    async function pollTankTilesState() {
      const controls = Array.from(document.querySelectorAll('input.tank-power'));
      for (const el of controls) {
        if (!el || el.dataset.busy === '1') continue;
        const name = el.dataset.action;
        const idMatch = name ? name.match(/^k(\d+)_power$/) : null;
        const id = idMatch ? parseInt(idMatch[1], 10) : 0;
        if (!id) continue;
        const hidden = document.querySelector('input[name="' + name + '"]');
        const tile = el.closest('.tile');
        try {
          const st = await postTankToggle(id, 'state');
          if (typeof st !== 'object' || !st) continue;
          el.checked = !!st.power;
          if (hidden) hidden.value = st.power ? 'on' : 'off';
          applyTankStateUi(tile, st);
        } catch (e) {}
      }
    }
    setTimeout(pollTankTilesState, 600);
    setInterval(pollTankTilesState, tanksIsStackView ? 2500 : 2000);
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






