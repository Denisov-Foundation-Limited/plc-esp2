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
#include "core/network/web/pages/web_page_meteo.hpp"

const char kWebInterfaceMeteoHtml[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>%METEO_PAGE_TITLE%</title>
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
    .btn {
      border: none;
      padding: 10px 16px;
      border-radius: 10px;
      background: var(--accent);
      color: #0b1220;
      font-weight: 700;
      cursor: pointer;
    }
    .status-dot {
      display: inline-block;
      width: 10px;
      height: 10px;
      border-radius: 50%;
      box-shadow: 0 0 0 2px rgba(15, 23, 42, 0.6);
    }
    .status-ok { background: #22c55e; }
    .status-err { background: #64748b; }
    .status-na { background: #64748b; }
    .actions { margin-top: 14px; }
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
    .mini { width: 72px; }
    .addr { width: 100%; }
    .temp { width: 100%; }
    .hum { width: 100%; }
    .name { width: 100%; }
    .grid {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 14px;
      margin-top: 10px;
    }
    .tile {
      display: grid;
      grid-template-columns: 180px minmax(0, 1fr);
      gap: 14px;
      padding: 16px;
      border-radius: 12px;
      border: 1px solid #1f2937;
      background: #0b1220;
      position: relative;
      overflow: hidden;
      align-items: stretch;
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
    .sensor-visual {
      position: relative;
      min-height: 180px;
      height: 100%;
      border-radius: 16px;
      border: 2px solid #1f2937;
      background: linear-gradient(180deg, #0a1220 0%, #0c1628 100%);
      overflow: hidden;
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      gap: 6px;
      padding: 10px;
    }
    .sensor-icon {
      width: 72px;
      height: 72px;
      opacity: 1;
      color: #ef4444;
    }
    .sensor-icon.na {
      opacity: 0.35;
      color: #64748b;
    }
    .sensor-hum-icon {
      width: 22px;
      height: 22px;
      color: #38bdf8;
      opacity: 0.9;
    }
    .sensor-readout {
      text-align: center;
      line-height: 1.1;
    }
    .sensor-value {
      font-size: 24px;
      font-weight: 700;
      letter-spacing: 0.2px;
    }
    .sensor-unit {
      font-size: 11px;
      color: var(--muted);
      margin-top: 2px;
    }
    .sensor-hum {
      display: flex;
      align-items: center;
      justify-content: center;
      gap: 6px;
      margin-top: 6px;
    }
    .sensor-hum .sensor-value {
      font-size: 18px;
    }
    .badge {
      position: absolute;
      left: 10px;
      top: 10px;
      padding: 2px 8px;
      border-radius: 999px;
      border: 1px solid #1f2937;
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
      grid-template-columns: 64px minmax(0, 1fr);
      align-items: center;
      gap: 8px;
    }
    .form-row.full {
      grid-column: 1 / -1;
      grid-template-columns: 64px minmax(0, 1fr);
    }
    .form-row.name-local,
    .form-row.name-remote {
      margin-bottom: 8px;
    }
    .form-row > label:not(.switch) {
      color: var(--muted);
      font-size: 12px;
      white-space: nowrap;
    }
    .status-line {
      display: flex;
      align-items: center;
      gap: 10px;
      color: var(--muted);
      font-size: 12px;
      margin: 8px 0 10px;
      flex-wrap: wrap;
    }
    @media (max-width: 720px) {
      .wrap { margin: 20px auto; }
      .card { padding: 16px; }
      h1 { font-size: 20px; }
      .grid { grid-template-columns: 1fr; }
      .tile { grid-template-columns: 1fr; }
      .sensor-visual { min-height: 120px; height: auto; }
      .sensor-value { font-size: 22px; }
      .field { padding: 5px 6px; }
      .btn { padding: 8px 12px; }
      .mini { width: 64px; }
      .addr { width: 140px; }
      .name { width: 120px; }
      .temp { width: 70px; }
    }
  </style>
</head>
<body>
  <div class="wrap">
    <div class="card">
      %NAV%
      <h1>%METEO_PAGE_TITLE%</h1>
      %METEO_DEVICE_SELECT%
      %METEO_PAGINATION%
      <form method="POST" action="/meteo" id="meteo-form">
        %METEO_FORM_HIDDEN%
        <div class="grid" id="meteo-grid">
          %METEO_TILES%
        </div>
        <p class="actions">
          %METEO_SAVE_BTN%
        </p>
      </form>
    </div>
  </div>
  <script>
    window.__plcDisableAutoRefresh = true;
    const meteoCanEdit = %METEO_CAN_EDIT%;
    const sensorOptions = %SENSOR_JSON%;
    const sensorUsed = %SENSOR_USED_JSON%;
    const meteoGrid = document.getElementById('meteo-grid');
    const meteoPageValue = (() => {
      const url = new URL(window.location.href);
      return url.searchParams.get('page') || '1';
    })();
    async function loadMeteoList() {
      if (!meteoGrid) return;
      try {
        const url = new URL('/meteo/list', window.location.origin);
        const cur = new URL(window.location.href);
        const unit = cur.searchParams.get('unit');
        const node = cur.searchParams.get('node');
        if (unit) url.searchParams.set('unit', unit);
        if (node) url.searchParams.set('node', node);
        url.searchParams.set('page', meteoPageValue);
        const res = await fetch(url.toString(), { cache: 'no-store', credentials: 'same-origin' });
        if (!res.ok) {
          meteoGrid.innerHTML = '<div class="tile empty">WEB busy</div>';
          return;
        }
        meteoGrid.innerHTML = await res.text();
        bindMeteoHandlers();
        refreshMeteoPins();
      } catch (e) {
        meteoGrid.innerHTML = '<div class="tile empty">WEB busy</div>';
      }
    }

    function labelFor(val) {
      return 'sens' + val;
    }
    function optionValue(item) {
      return (item && typeof item === 'object') ? String(item.v) : String(item);
    }
    function optionLabel(item) {
      if (item && typeof item === 'object' && item.l) return item.l;
      return labelFor(optionValue(item));
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
        html += '<option value="' + val + '"' + (val === selected ? ' selected' : '') +
          (isUsed ? ' disabled' : '') + '>' + label + '</option>';
      }
      return html;
    }

    function refreshMeteoPins() {
      const used = new Set((sensorUsed || []).map((v) => parseInt(v, 10)).filter((v) => !Number.isNaN(v)));
      document.querySelectorAll('select.meteo-pin').forEach((el) => {
        const v = parseInt(el.value || el.dataset.selected || '', 10);
        if (!Number.isNaN(v)) used.add(v);
      });
      document.querySelectorAll('select.meteo-pin').forEach((el) => {
        const selected = el.value || el.dataset.selected || '';
        el.innerHTML = buildOptions(sensorOptions || [], selected, used);
        el.value = selected || '';
      });
    }
    const meteoForm = document.getElementById('meteo-form');
    let meteoDirty = false;
    if (meteoForm) {
      meteoForm.addEventListener('input', () => { meteoDirty = true; });
      meteoForm.addEventListener('change', () => { meteoDirty = true; });
    }
    const reloadKey = 'meteo_reload';
    if (sessionStorage.getItem(reloadKey)) {
      sessionStorage.removeItem(reloadKey);
      location.replace(location.pathname + location.search);
    }
    if (meteoForm) {
      meteoForm.addEventListener('submit', () => {
        sessionStorage.setItem(reloadKey, '1');
      });
    }

    function bindMeteoHandlers() {
      document.querySelectorAll('select.meteo-pin').forEach((el) => {
        if (el.dataset.boundPin === '1') return;
        el.dataset.boundPin = '1';
        el.addEventListener('change', refreshMeteoPins);
      });
      document.querySelectorAll('select.meteo-type').forEach((el) => {
        const row = el.closest('.tile');
        if (!row) return;
        updateRow(row);
        if (el.dataset.boundType === '1') return;
        el.dataset.boundType = '1';
        el.addEventListener('change', () => updateRow(row));
      });
      document.querySelectorAll('select.meteo-device').forEach((el) => {
        const row = el.closest('.tile');
        if (!row) return;
        if (el.dataset.boundDevice === '1') return;
        el.dataset.boundDevice = '1';
        el.addEventListener('change', () => updateRow(row));
      });
      document.querySelectorAll('input.meteo-enable').forEach((el) => {
        if (el.dataset.boundEnable === '1') return;
        el.dataset.boundEnable = '1';
        el.addEventListener('change', () => {
          const tile = el.closest('.tile');
          updateMeteoEnabled(tile, el.checked);
          if (!el.checked && meteoForm) {
            sessionStorage.setItem(reloadKey, '1');
            meteoForm.submit();
          }
        });
      });
    }

    function setDisabled(el, disabled) {
      if (!el) {
        return;
      }
      if (el.tagName === 'INPUT' || el.tagName === 'SELECT' || el.tagName === 'TEXTAREA') {
        el.disabled = disabled;
      }
    }

    function filterRemoteOptions(select, nodeVal) {
      if (!select) return;
      const node = nodeVal || '';
      const isLocal = (node === '' || node === 'local');
      select.querySelectorAll('option').forEach((opt) => {
        if (!opt.dataset || !opt.dataset.node) {
          opt.hidden = false;
          return;
        }
        opt.hidden = isLocal ? true : (opt.dataset.node !== node);
      });
      if (isLocal) {
        select.value = '';
      } else if (select.value) {
        const selected = select.querySelector('option[value="' + select.value + '"]');
        if (selected && selected.hidden) {
          select.value = '';
        }
      }
    }

    function updateRow(row) {
      const stackView = (new URLSearchParams(window.location.search)).get('unit') === 'stack';
      const type = row.querySelector('select.meteo-type');
      const pinCell = row.querySelector('.pin-cell');
      const addrCell = row.querySelector('.addr-cell');
      const pinSelect = pinCell ? pinCell.querySelector('select') : null;
      const addrInput = addrCell ? addrCell.querySelector('input') : null;
      const addrSelect = addrCell ? addrCell.querySelector('select') : null;
      const device = row.querySelector('select.meteo-device');
      const source = row.querySelector('select.meteo-source');
      const nameLocal = row.querySelector('.name-local');
      const nameRemote = row.querySelector('.name-remote');
      const nameInput = nameLocal ? nameLocal.querySelector('input') : null;
      const isLocal = !device || device.value === 'local' || device.value === '';
      const lockAll = false;
      const lockDevice = stackView;
      if (nameLocal) nameLocal.style.display = isLocal ? '' : 'none';
      if (nameRemote) nameRemote.style.display = isLocal ? 'none' : '';
      setDisabled(nameInput, lockAll || !isLocal || !meteoCanEdit);
      setDisabled(source, lockAll || !meteoCanEdit);
      if (!isLocal) {
        filterRemoteOptions(source, device ? device.value : '');
      } else if (source) {
        source.value = '';
      }
      const val = type ? type.value : 'none';
      const showPin = (val === 'dht22');
      const showAddr = (val === 'ds18b20');
      if (lockAll) {
        if (pinCell) pinCell.style.display = '';
        if (addrCell) addrCell.style.display = '';
        setDisabled(type, true);
        setDisabled(pinSelect, true);
        setDisabled(addrInput, true);
        setDisabled(addrSelect, true);
      } else if (!isLocal) {
        if (pinCell) pinCell.style.display = 'none';
        if (addrCell) addrCell.style.display = 'none';
        setDisabled(type, true);
        setDisabled(pinSelect, true);
        setDisabled(addrInput, true);
        setDisabled(addrSelect, true);
      } else {
        if (pinCell) pinCell.style.display = showPin ? '' : 'none';
        if (addrCell) addrCell.style.display = showAddr ? '' : 'none';
        setDisabled(type, !meteoCanEdit);
        setDisabled(pinSelect, !showPin || !meteoCanEdit);
        setDisabled(addrInput, !showAddr || !meteoCanEdit);
        setDisabled(addrSelect, !showAddr || !meteoCanEdit);
      }
      setDisabled(device, lockDevice || !meteoCanEdit);
    }

    function updateMeteoEnabled(tile, enabled) {
      if (!tile) return;
      tile.classList.toggle('disabled', !enabled);
      if (!enabled) {
        const name = tile.querySelector('input.meteo-name');
        if (name) name.value = '';
        const device = tile.querySelector('select.meteo-device');
        if (device) device.value = 'local';
        const source = tile.querySelector('select.meteo-source');
        if (source) source.value = '';
        const type = tile.querySelector('select.meteo-type');
        if (type) type.value = 'none';
        tile.querySelectorAll('select.meteo-pin').forEach((sel) => {
          sel.value = '';
          sel.dataset.selected = '';
        });
        tile.querySelectorAll('select.meteo-addr').forEach((sel) => {
          sel.value = '';
        });
        refreshMeteoPins();
        updateRow(tile);
      }
    }
    let remoteSourcesBusy = false;
    async function pollRemoteSources() {
      if (remoteSourcesBusy) return;
      const sourceSelects = Array.from(document.querySelectorAll('select.meteo-source'));
      if (!sourceSelects.length) return;
      remoteSourcesBusy = true;
      try {
        const resp = await fetch('/meteo/remote_sources', { cache: 'no-store' });
        if (!resp.ok) return;
        const html = await resp.text();
        sourceSelects.forEach((el) => {
          const prev = el.value || '';
          if (el.innerHTML !== html) {
            el.innerHTML = html;
          }
          if (prev) {
            el.value = prev;
          }
          const row = el.closest('.tile');
          if (row) updateRow(row);
        });
      } catch (e) {
      } finally {
        remoteSourcesBusy = false;
      }
    }
    pollRemoteSources();
    setInterval(pollRemoteSources, 3000);

    const meteoQs = new URLSearchParams(window.location.search);
    const meteoIsStackView = meteoQs.get('unit') === 'stack';
    const meteoNodeId = meteoQs.get('node') || '';
    function formatMeteoValue(v) {
      if (typeof v !== 'number' || Number.isNaN(v)) return '--';
      return String(Math.round(v * 10) / 10);
    }
    function meteoStatusText(item) {
      if (!item || !item.enabled) return '%METEO_STATUS_OFF_TEXT%';
      const hasData = !!item.has_temp || !!item.has_hum;
      if (!hasData) return '%METEO_STATUS_NODATA_TEXT%';
      return item.ok ? '%METEO_STATUS_OK_TEXT%' : '%METEO_STATUS_ERR_TEXT%';
    }
    function meteoAgeText(item) {
      const hasRead = !!(item && item.has_read);
      const age = (item && typeof item.age_s === 'number' && !Number.isNaN(item.age_s)) ? Math.max(0, Math.floor(item.age_s)) : 0;
      return '%METEO_STATUS_AGE_PREFIX%' + (hasRead ? (String(age) + 's') : '-');
    }
    function applyMeteoTileState(tile, item) {
      if (!tile || !item) return;
      tile.classList.toggle('disabled', !item.enabled);
      const icon = tile.querySelector('.sensor-icon');
      const hasData = !!item.has_temp || !!item.has_hum;
      const okOn = !!item.ok && hasData;
      if (icon) {
        icon.classList.toggle('na', !okOn);
      }
      const temp = tile.querySelector('.sensor-temp-value');
      if (temp) {
        temp.textContent = (item.has_temp ? formatMeteoValue(item.temp) : '--') + ' °C';
      }
      const hum = tile.querySelector('.sensor-hum-value');
      if (hum) {
        hum.textContent = item.has_hum ? formatMeteoValue(item.hum) : '--';
      }
      const dot = tile.querySelector('.sensor-status-dot');
      if (dot) {
        dot.classList.remove('status-ok', 'status-err', 'status-na');
        if (!item.enabled || !hasData) dot.classList.add('status-na');
        else dot.classList.add(item.ok ? 'status-ok' : 'status-err');
      }
      const ageText = tile.querySelector('.meteo-age-text');
      if (ageText) {
        ageText.textContent = meteoAgeText(item);
      }
    }
    async function pollMeteoStates() {
      const tiles = Array.from(document.querySelectorAll('.tile[data-sensor-id]'));
      if (!tiles.length) return;
      try {
        let url = '/meteo/state';
        if (meteoIsStackView && meteoNodeId) {
          url += '?node_id=' + encodeURIComponent(meteoNodeId);
        }
        const res = await fetch(url, { cache: 'no-store', credentials: 'same-origin' });
        if (!res.ok) return;
        const data = await res.json();
        const map = new Map();
        const list = Array.isArray(data.items) ? data.items : [];
        list.forEach((it) => map.set(String(it.id), it));
        tiles.forEach((tile) => {
          const id = tile.dataset.sensorId || '';
          const item = map.get(String(id));
          if (item) applyMeteoTileState(tile, item);
        });
      } catch (e) {}
    }
    setTimeout(pollMeteoStates, 500);
    setInterval(pollMeteoStates, 2000);
    refreshMeteoPins();
    if (meteoIsStackView) {
      loadMeteoList();
    }
    bindMeteoHandlers();
    const scrollKey = 'meteo_scroll_y';
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
    const meteoDevice = document.getElementById('meteo-device');
    if (meteoDevice) {
      meteoDevice.addEventListener('change', () => {
        const val = meteoDevice.value || 'local';
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











