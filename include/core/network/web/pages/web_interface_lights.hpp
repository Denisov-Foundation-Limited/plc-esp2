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

static const char kWebInterfaceLightsHtml[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Свет</title>
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
    .wrap { max-width: 980px; margin: 40px auto; padding: 0 16px; }
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
    .btn-sm {
      padding: 6px 10px;
      border-radius: 8px;
      font-size: 12px;
    }
    .btn-on { background: #22c55e; color: #0b1220; }
    .btn-off { background: #f97316; color: #0b1220; }
    .btn-toggle { background: #38bdf8; color: #0b1220; }
    .status-dot {
      display: inline-block;
      width: 10px;
      height: 10px;
      border-radius: 50%;
      box-shadow: 0 0 0 2px rgba(15, 23, 42, 0.6);
    }
    .status-on { background: #22c55e; }
    .status-off { background: #64748b; }
    .mini { width: 72px; }
    .name { width: 180px; }
    .pagination {
      display: flex;
      align-items: center;
      gap: 10px;
      margin: 8px 0 12px;
      color: var(--muted);
      font-size: 12px;
    }
    .page-info { white-space: nowrap; }
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
    .sock-visual {
      position: relative;
      height: 140px;
      border-radius: 16px;
      border: 2px solid #1f2937;
      background: linear-gradient(180deg, #0a1220 0%, #0c1628 100%);
      overflow: hidden;
      cursor: pointer;
    }
    .tile.disabled .sock-visual { cursor: not-allowed; }
    .sock-icon {
      position: absolute;
      left: 50%;
      top: 50%;
      transform: translate(-50%, -50%);
      width: 90px;
      height: 90px;
      opacity: 0.2;
      color: #38bdf8;
      transition: opacity .2s ease, transform .2s ease, filter .2s ease;
    }
    .sock-icon.on {
      opacity: 1;
      filter: drop-shadow(0 0 12px rgba(250, 204, 21, 0.6));
      color: #facc15;
      transform: translate(-50%, -50%) scale(1.02);
    }
    .sock-icon.off {
      opacity: 0.22;
      filter: none;
      color: #64748b;
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
      grid-template-columns: max-content minmax(0, 1fr);
      align-items: center;
      gap: 8px;
    }
    .form-row > label:not(.switch) {
      color: var(--muted);
      font-size: 12px;
      white-space: nowrap;
    }
    .form-row .field,
    .form-row select {
      width: 100%;
      min-width: 0;
      max-width: 100%;
    }
    .status-line {
      display: flex;
      align-items: center;
      gap: 8px;
      color: var(--muted);
      font-size: 12px;
      margin: 8px 0 10px;
    }
    .actions { margin-top: 14px; }
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
    .table-wrap { width: 100%; overflow-x: auto; -webkit-overflow-scrolling: touch; }
    table { min-width: 720px; }
        @media (max-width: 900px) {
      .wrap { margin: 20px auto; }
      .card { padding: 16px; }
      h1 { font-size: 20px; }
      .grid { grid-template-columns: 1fr; }
      .tile { grid-template-columns: 1fr; }
      .sock-visual { height: 120px; }
      .form-grid { grid-template-columns: 1fr; }
    }
  </style>
</head>
<body>
  <div class="wrap">
    <div class="card">
      %NAV%
      <h1>Свет</h1>
      <div class="status">%LIGHTS_STATUS%</div>
      %LIGHTS_DEVICE_SELECT%
      <div class="pagination" %LIGHTS_PAGINATION_STYLE%>
        <button type="button" class="btn btn-sm" id="lights-prev">Назад</button>
        <span class="page-info">Страница</span>
        <select id="lights-page" class="field mini"></select>
        <span class="page-info">/ %LIGHTS_PAGES%</span>
        <button type="button" class="btn btn-sm" id="lights-next">Вперёд</button>
      </div>
      <form method="POST" action="/lights" id="lights-form">
        <div class="grid">
          %LIGHTS%
        </div>
        <p class="actions">
          %LIGHTS_SAVE_BTN%
        </p>
      </form>
    </div>
  </div>
  <script>
    const lightsUnit = "%LIGHTS_UNIT%";
    const lightsNodeId = %LIGHTS_NODE_ID%;
    const socketOptions = {
      dinput: %DINPUT_JSON%,
      relay: %RELAY_JSON%
    };
    const lightsPage = %LIGHTS_PAGE%;
    const lightsPages = %LIGHTS_PAGES%;
    const socketUsed = {
      dinput: %DINPUT_USED_JSON%,
      relay: %RELAY_USED_JSON%
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
    function buildOptions(list, selected, type) {
      let html = '<option value="">-</option>';
      const used = socketUsed[type] || [];
      for (let i = 0; i < list.length; i++) {
        const val = optionValue(list[i]);
        if (used.indexOf(parseInt(val, 10)) !== -1 && val !== selected) {
          continue;
        }
        const label = optionLabel(list[i], type);
        html += '<option value="' + val + '"' + (val === selected ? ' selected' : '') + '>' + label + '</option>';
      }
      return html;
    }
    document.querySelectorAll('select.socket-select').forEach((el) => {
      const type = el.dataset.type;
      const selected = el.dataset.selected || '';
      const list = socketOptions[type] || [];
      el.innerHTML = buildOptions(list, selected, type);
    });
    const prevBtn = document.getElementById('lights-prev');
    const nextBtn = document.getElementById('lights-next');
    const pageSelect = document.getElementById('lights-page');
    if (pageSelect) {
      for (let i = 1; i <= lightsPages; i++) {
        const opt = document.createElement('option');
        opt.value = String(i);
        opt.textContent = String(i);
        if (i === lightsPage) opt.selected = true;
        pageSelect.appendChild(opt);
      }
      pageSelect.addEventListener('change', () => {
        const url = new URL(window.location.href);
        url.searchParams.set('page', pageSelect.value || String(lightsPage));
        window.location.href = url.toString();
      });
    }
    if (prevBtn) {
      prevBtn.disabled = lightsPage <= 1;
      prevBtn.addEventListener('click', () => {
        const url = new URL(window.location.href);
        url.searchParams.set('page', String(Math.max(1, lightsPage - 1)));
        window.location.href = url.toString();
      });
    }
    if (nextBtn) {
      nextBtn.disabled = lightsPage >= lightsPages;
      nextBtn.addEventListener('click', () => {
        const url = new URL(window.location.href);
        url.searchParams.set('page', String(Math.min(lightsPages, lightsPage + 1)));
        window.location.href = url.toString();
      });
    }
        async function postForm(url, body) {
      if (lightsUnit === 'stack' && lightsNodeId) {
        body += '&node_id=' + encodeURIComponent(String(lightsNodeId));
      }
      const res = await fetch(url, {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body,
        credentials: 'same-origin'
      });
      if (!res.ok) {
        throw new Error('Request failed');
      }
      return (await res.text()).trim();
    }
    function updateSocketVisual(tile, isOn) {
      if (!tile) return;
      const icon = tile.querySelector('.sock-icon');
      const dot = tile.querySelector('.status-dot');
      const text = tile.querySelector('.status-text');
      if (icon) {
        icon.classList.toggle('on', isOn);
        icon.classList.toggle('off', !isOn);
      }
      if (dot) {
        dot.classList.toggle('status-on', isOn);
        dot.classList.toggle('status-off', !isOn);
      }
      if (text) {
        text.textContent = isOn ? 'Включена' : 'Выключена';
      }
    }
    function updateSocketEnabled(tile, enabled) {
      if (!tile) return;
      tile.classList.toggle('disabled', !enabled);
      const toggle = tile.querySelector('input.socket-toggle');
      if (toggle) {
        toggle.disabled = !enabled;
      }
    }
    function toggleFromVisual(visual) {
      const tile = visual.closest('.tile');
      if (!tile) return;
      const toggle = tile.querySelector('input.socket-toggle');
      if (!toggle || toggle.disabled || toggle.dataset.busy === '1') return;
      toggle.checked = !toggle.checked;
      toggle.dispatchEvent(new Event('change', { bubbles: true }));
    }
    document.querySelectorAll('.sock-visual').forEach((visual) => {
      visual.addEventListener('click', () => toggleFromVisual(visual));
    });
    document.querySelectorAll('input.socket-toggle').forEach((el) => {
      el.addEventListener('change', async () => {
        const id = el.dataset.id;
        const tile = el.closest('.tile');
        if (el.dataset.busy === '1') return;
        const desired = el.checked;
        el.dataset.busy = '1';
        el.disabled = true;
        try {
          const action = el.checked ? 'on' : 'off';
          const state = await postForm('/lights/toggle', 'id=' + encodeURIComponent(id) + '&action=' + action);
          if (state === 'pending') {
            updateSocketVisual(tile, desired);
            return;
          }
          const isOn = state === 'on' || state === '1' || state === 'true';
          el.checked = isOn;
          updateSocketVisual(tile, isOn);
        } catch (e) {
          el.checked = !desired;
          updateSocketVisual(tile, el.checked);
        } finally {
          el.disabled = false;
          el.dataset.busy = '0';
        }
      });
    });
      document.querySelectorAll('input.socket-enable').forEach((el) => {
        el.addEventListener('change', async () => {
          const id = el.dataset.id;
          const tile = el.closest('.tile');
          const enabled = el.checked ? '1' : '0';
        try {
          const state = await postForm('/lights/enable', 'id=' + encodeURIComponent(id) + '&enabled=' + enabled);
          let isEnabled = false;
          try {
            const data = JSON.parse(state);
            isEnabled = data.parsed === true || data.parsed === 'true' || data.result === '1';
          } catch (e) {
            isEnabled = state === '1' || state === 'true' || state === 'on';
          }
          el.checked = isEnabled;
          updateSocketEnabled(tile, isEnabled);
        } catch (e) {
          el.checked = !el.checked;
        }
        });
      });
      async function fetchState(id) {
        let url = '/lights/toggle?id=' + encodeURIComponent(id) + '&action=state';
        if (lightsUnit === 'stack' && lightsNodeId) {
          url += '&node_id=' + encodeURIComponent(String(lightsNodeId));
        }
        const res = await fetch(url, {
          cache: 'no-store',
          credentials: 'same-origin'
        });
        if (!res.ok) {
          throw new Error('state');
        }
        return (await res.text()).trim();
      }
      async function pollStates() {
        if (document.hidden) return;
        const toggles = document.querySelectorAll('input.socket-toggle');
        for (let i = 0; i < toggles.length; i++) {
          const el = toggles[i];
          if (el.dataset.busy === '1') continue;
          if (el.disabled) continue;
          const tile = el.closest('.tile');
          try {
            const state = await fetchState(el.dataset.id);
            if (state === 'unknown' || state === 'pending') {
              continue;
            }
            const isOn = state === 'on' || state === '1' || state === 'true';
            if (el.checked !== isOn) {
              el.checked = isOn;
              updateSocketVisual(tile, isOn);
            }
          } catch (e) {
          }
        }
      }
      setInterval(pollStates, 2000);
      const lightsForm = document.getElementById('lights-form');
      const reloadKey = 'lights_reload';
      if (sessionStorage.getItem(reloadKey)) {
        sessionStorage.removeItem(reloadKey);
        location.replace(location.pathname);
      }
      if (lightsForm) {
        lightsForm.addEventListener('submit', () => {
          sessionStorage.setItem(reloadKey, '1');
        });
      }
      const deviceSelect = document.getElementById('lights-device');
      if (deviceSelect) {
        deviceSelect.addEventListener('change', () => {
          const val = deviceSelect.value || 'local';
          const url = new URL(window.location.href);
          if (val === 'local') {
            url.searchParams.delete('node');
            url.searchParams.delete('unit');
          } else {
            url.searchParams.set('unit', 'stack');
            url.searchParams.set('node', val);
          }
          url.searchParams.delete('page');
          window.location.href = url.toString();
        });
      }
    </script>
</body>
</html>
)HTML";











