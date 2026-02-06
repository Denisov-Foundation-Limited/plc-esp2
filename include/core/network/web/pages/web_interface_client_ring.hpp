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

static const char kWebInterfaceClientRingHtml[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Ring client</title>
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
    .row { display: flex; align-items: center; gap: 12px; flex-wrap: wrap; }
    .label { color: var(--muted); font-size: 12px; text-transform: uppercase; letter-spacing: .04em; }
    .status { color: var(--muted); font-size: 12px; margin-bottom: 10px; }
    .badge { padding: 4px 10px; border-radius: 999px; font-size: 12px; font-weight: 600; }
    .badge.on { background: #14532d; color: #dcfce7; }
    .badge.off { background: #2f2f2f; color: #d1d5db; }
    .field {
      width: 100%;
      background: #0b1220;
      border: 1px solid #334155;
      color: var(--text);
      padding: 10px;
      border-radius: 8px;
    }
    .mini { width: 180px; }
    .btn {
      background: var(--accent);
      color: #00111a;
      border: none;
      padding: 10px 16px;
      border-radius: 8px;
      font-weight: 700;
      cursor: pointer;
    }
    .actions { margin-top: 12px; }
    .toggle {
      display: flex;
      align-items: center;
      gap: 10px;
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
    @media (max-width: 720px) {
      .wrap { margin: 20px auto; }
    }
  </style>
</head>
<body>
  <div class="wrap">
    <div class="card">
      %NAV%
      <h1>Ring client</h1>
      <div class="status">%RING_CLIENT_STATUS%</div>
      <div class="row toggle">
        <label class="switch">
          <input type="checkbox" name="ring_client_enabled" form="ring-client-form" %RING_CLIENT_ENABLED_CHECKED%>
          <span class="track"><span class="knob"></span></span>
        </label>
        <span> Ring client</span>
        <span class="badge %RING_CLIENT_ACTIVE_CLASS%">%RING_CLIENT_ACTIVE_LABEL%</span>
      </div>
      <div class="row">
        <div class="label"></div>
        <select class="field mini" id="ring-client-button" name="ring_client_button" form="ring-client-form"></select>
      </div>
      <form method="POST" action="/client/ring" id="ring-client-form">
        <div class="actions">
          <button class="btn" type="submit"></button>
        </div>
      </form>
    </div>
  </div>
  <script>
    const buttonOptions = %RING_CLIENT_BUTTON_JSON%;
    const buttonUsed = %RING_CLIENT_DINPUT_USED_JSON%;
    const buttonSelected = "%RING_CLIENT_BUTTON_SELECTED%";
    const select = document.getElementById('ring-client-button');
    function buildButtonOptions(list, selected, usedSet) {
      let html = '<option value="">-</option>';
      const used = usedSet || new Set();
      for (let i = 0; i < list.length; i++) {
        const v = String(list[i].v);
        const num = parseInt(v, 10);
        const isUsed = !Number.isNaN(num) && used.has(num) && v !== selected;
        const l = list[i].l || v;
        html += '<option value="' + v + '"' + (v === selected ? ' selected' : '') +
          (isUsed ? ' disabled' : '') + '>' + l + '</option>';
      }
      return html;
    }
    function refreshRingClientButton() {
      if (!select) return;
      const used = new Set((buttonUsed || []).map((v) => parseInt(v, 10)).filter((v) => !Number.isNaN(v)));
      const selected = select.value || buttonSelected || '';
      const selectedNum = parseInt(selected, 10);
      if (!Number.isNaN(selectedNum)) used.add(selectedNum);
      select.innerHTML = buildButtonOptions(buttonOptions || [], selected, used);
      if (selected) select.value = selected;
    }
    refreshRingClientButton();
    if (select) {
      select.addEventListener('change', refreshRingClientButton);
    }
  </script>
</body>
</html>
)HTML";
