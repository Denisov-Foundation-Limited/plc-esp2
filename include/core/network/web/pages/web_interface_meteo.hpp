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

static const char kWebInterfaceMeteoHtml[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Метео</title>
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
      grid-template-columns: 140px minmax(0, 1fr);
      gap: 14px;
      padding: 16px;
      border-radius: 12px;
      border: 1px solid #1f2937;
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
    .sensor-visual {
      position: relative;
      height: 140px;
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
      .sensor-visual { height: 120px; }
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
      <h1>Метео</h1>
      <div class="status">%METEO_STATUS%</div>
      %METEO_DEVICE_SELECT%
      <form method="POST" action="/meteo" id="meteo-form">
        <div class="grid">
          %METEO_TILES%
        </div>
        <p class="actions">
          %METEO_SAVE_BTN%
        </p>
      </form>
    </div>
  </div>
  <script>
    const sensorOptions = %SENSOR_JSON%;
    const sensorUsed = %SENSOR_USED_JSON%;

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

    refreshMeteoPins();
    document.querySelectorAll('select.meteo-pin').forEach((el) => {
      el.addEventListener('change', refreshMeteoPins);
    });

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
      if (nameLocal) nameLocal.style.display = isLocal ? '' : 'none';
      if (nameRemote) nameRemote.style.display = isLocal ? 'none' : '';
      setDisabled(nameInput, !isLocal);
      setDisabled(source, false);
      if (!isLocal) {
        filterRemoteOptions(source, device ? device.value : '');
      } else if (source) {
        source.value = '';
      }
      const val = type ? type.value : 'none';
      const showPin = (val === 'dht22');
      const showAddr = (val === 'ds18b20');
      if (!isLocal) {
        if (pinCell) pinCell.style.display = 'none';
        if (addrCell) addrCell.style.display = 'none';
        setDisabled(type, true);
        setDisabled(pinSelect, true);
        setDisabled(addrInput, true);
        setDisabled(addrSelect, true);
      } else {
        if (pinCell) pinCell.style.display = showPin ? '' : 'none';
        if (addrCell) addrCell.style.display = showAddr ? '' : 'none';
        setDisabled(type, false);
        setDisabled(pinSelect, !showPin);
        setDisabled(addrInput, !showAddr);
        setDisabled(addrSelect, !showAddr);
      }
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

    document.querySelectorAll('select.meteo-type').forEach((el) => {
      const row = el.closest('.tile');
      if (row) {
        updateRow(row);
        el.addEventListener('change', () => updateRow(row));
      }
    });
    document.querySelectorAll('select.meteo-device').forEach((el) => {
      const row = el.closest('.tile');
      if (row) {
        el.addEventListener('change', () => updateRow(row));
      }
    });
    document.querySelectorAll('input.meteo-enable').forEach((el) => {
      el.addEventListener('change', () => {
        const tile = el.closest('.tile');
        updateMeteoEnabled(tile, el.checked);
        if (!el.checked && meteoForm) {
          sessionStorage.setItem(reloadKey, '1');
          meteoForm.submit();
        }
      });
    });
    const meteoForm = document.getElementById('meteo-form');
    let meteoDirty = false;
    if (meteoForm) {
      meteoForm.addEventListener('input', () => { meteoDirty = true; });
      meteoForm.addEventListener('change', () => { meteoDirty = true; });
    }
    const reloadKey = 'meteo_reload';
    if (sessionStorage.getItem(reloadKey)) {
      sessionStorage.removeItem(reloadKey);
      location.replace(location.pathname);
    }
    if (meteoForm) {
      meteoForm.addEventListener('submit', () => {
        sessionStorage.setItem(reloadKey, '1');
      });
    }
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








