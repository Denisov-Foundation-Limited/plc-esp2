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

static const char kWebInterfaceRingHtml[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Звонок</title>
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
    .wrap { max-width: 720px; margin: 40px auto; padding: 0 16px; }
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
    .status { color: var(--muted); font-size: 12px; margin-bottom: 10px; }
    .row { display: flex; gap: 10px; align-items: center; }
    .muted { color: var(--muted); font-size: 12px; }
    .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
    label { display: block; margin-bottom: 4px; font-size: 12px; color: var(--muted); }
    .field {
      width: 100%;
      background: #0b1220;
      border: 1px solid #334155;
      color: var(--text);
      padding: 10px;
      border-radius: 8px;
    }
    .mini { width: 120px; }
    button {
      background: var(--accent);
      color: #00111a;
      border: none;
      padding: 10px 16px;
      border-radius: 8px;
      font-weight: 700;
      cursor: pointer;
    }
    .btn-on { background: #22c55e; }
    .btn-off { background: #ef4444; }
    .actions { margin-top: 12px; }
    @media (max-width: 720px) {
      .wrap { margin: 20px auto; }
      .grid { grid-template-columns: 1fr; }
    }
  </style>
</head>
<body>
  <div class="wrap">
    <div class="card">
      %NAV%
      <h1>Звонок</h1>
      <div class="status">%RING_STATUS%</div>
      %RING_DEVICE_SELECT%
      <form method="POST" action="/ring" id="ring-form">
        <input type="hidden" name="ring_save" value="1">
        <div class="grid">
          <div>
            <label>Кнопка (вход)</label>
            <select class="field ring-select" name="ring_button" data-type="dinput" data-selected="%RING_BUTTON_SELECTED%" %RING_FORM_DISABLED%></select>
          </div>
          <div>
            <label>Реле</label>
            <select class="field ring-select" name="ring_relay" data-type="relay" data-selected="%RING_RELAY_SELECTED%" %RING_FORM_DISABLED%></select>
          </div>
        </div>
      </form>
      <div class="actions">
        <button type="button" class="btn-on" data-ring="on">Звонить</button>
      </div>
      <div class="actions">
        <button type="submit" form="ring-form" %RING_SAVE_DISABLED%>Сохранить</button>
      </div>
    </div>
  </div>
  <script>
    const ringOptions = {
      dinput: %RING_DINPUT_JSON%,
      relay: %RING_RELAY_JSON%
    };
    const ringUsed = {
      dinput: %RING_DINPUT_USED_JSON%,
      relay: %RING_RELAY_USED_JSON%
    };
    function labelFor(type, val) {
      if (type === 'dinput') return 'in' + val;
      if (type === 'relay') return 'rly' + val;
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
      const used = ringUsed[type] || [];
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
    document.querySelectorAll('select.ring-select').forEach((el) => {
      const type = el.dataset.type;
      const selected = el.dataset.selected || '';
      const list = ringOptions[type] || [];
      el.innerHTML = buildOptions(list, selected, type);
    });

    const ringDeviceSelect = document.getElementById('ring-device');
    function ringNodeValue() {
      if (!ringDeviceSelect) return '';
      const val = ringDeviceSelect.value || 'local';
      if (val === 'local') return '';
      return val;
    }
    const ringStatus = document.querySelector('.status');
    function ringSetStatus(msg) {
      if (!ringStatus || !msg) return;
      ringStatus.textContent = msg;
    }
    function ringSend(state) {
      const node = ringNodeValue();
      let body = 'state=' + (state ? 'on' : 'off');
      if (node) body += '&node=' + encodeURIComponent(node);
      if (window.fetch) {
        return fetch('/ring/trigger', {
          method: 'POST',
          headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
          body,
          credentials: 'same-origin'
        }).then(async (res) => {
          const text = (await res.text()).trim();
          if (!res.ok) throw new Error(text || 'Ошибка');
          if (text) ringSetStatus(text);
        }).catch((err) => {
          ringSetStatus(err && err.message ? err.message : 'Ошибка');
        });
      }
      try {
        const xhr = new XMLHttpRequest();
        xhr.open('POST', '/ring/trigger', true);
        xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
        xhr.onreadystatechange = function () {
          if (xhr.readyState !== 4) return;
          const text = (xhr.responseText || '').trim();
          if (xhr.status >= 200 && xhr.status < 300) {
            if (text) ringSetStatus(text);
          } else {
            ringSetStatus(text || 'Ошибка');
          }
        };
        xhr.send(body);
      } catch (e) {
        ringSetStatus('Ошибка');
      }
      return null;
    }
    const ringOnButton = document.querySelector('button[data-ring="on"]');
    let ringHeld = false;
    function ringPress() {
      if (ringHeld) return;
      ringHeld = true;
      ringSend(true);
    }
    function ringRelease() {
      if (!ringHeld) return;
      ringHeld = false;
      ringSend(false);
    }
    if (ringOnButton) {
      ringOnButton.addEventListener('pointerdown', (evt) => {
        evt.preventDefault();
        ringPress();
      });
      ringOnButton.addEventListener('pointerup', ringRelease);
      ringOnButton.addEventListener('pointerleave', ringRelease);
      ringOnButton.addEventListener('pointercancel', ringRelease);
      ringOnButton.addEventListener('mousedown', ringPress);
      ringOnButton.addEventListener('mouseup', ringRelease);
      ringOnButton.addEventListener('mouseleave', ringRelease);
      ringOnButton.addEventListener('touchstart', (evt) => {
        evt.preventDefault();
        ringPress();
      }, { passive: false });
      ringOnButton.addEventListener('touchend', ringRelease);
      ringOnButton.addEventListener('touchcancel', ringRelease);
    }
    window.addEventListener('blur', ringRelease);
    if (ringDeviceSelect) {
      ringDeviceSelect.addEventListener('change', () => {
        const val = ringDeviceSelect.value || 'local';
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





