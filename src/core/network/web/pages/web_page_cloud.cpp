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
#include "core/network/web/pages/web_page_cloud.hpp"

const char kWebInterfaceCloudHtml[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>%CLOUD_PAGE_TITLE%</title>
  <style>
    :root {
      --bg: #0f172a;
      --card: #111827;
      --accent: #38bdf8;
      --text: #e5e7eb;
      --muted: #94a3b8;
      --ok: #22c55e;
      --bad: #ef4444;
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
      color: var(--text);
    }
    .wrap { max-width: 760px; margin: 40px auto; padding: 0 16px; }
    .card {
      background: linear-gradient(180deg, #0f172a 0%, #0b1220 100%);
      border: 1px solid #1f2937;
      border-radius: 16px;
      padding: 22px;
      box-shadow: 0 12px 32px rgba(0,0,0,0.35);
    }
    h1 { margin: 0 0 8px; font-size: 22px; }
    p { margin: 0 0 18px; color: var(--muted); }
    a { color: #7dd3fc; text-decoration: none; }
    .nav { margin-bottom: 12px; }
    .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; }
    .form-row { display: block; }
    .form-row.full { grid-column: 1 / -1; }
    label { display: block; margin-bottom: 4px; font-size: 12px; color: var(--muted); }
    input[type=text], input[type=password], input[type=number], .field {
      width: 100%;
      background: #0b1220;
      border: 1px solid #334155;
      color: var(--text);
      padding: 10px;
      border-radius: 8px;
    }
    .field:disabled,
    select.field:disabled,
    input.field[readonly] {
      color: var(--muted);
      -webkit-text-fill-color: var(--muted);
      background: #0a1220;
      border-color: #1a2436;
      cursor: not-allowed;
    }    .row { display: flex; gap: 12px; align-items: center; flex-wrap: wrap; }
    .checkbox { display: flex; gap: 8px; align-items: center; }
    button {
      background: var(--accent);
      color: #0b1220;
      border: none;
      padding: 10px 16px;
      border-radius: 8px;
      font-weight: 700;
      cursor: pointer;
    }
    .badge {
      display: inline-flex;
      align-items: center;
      gap: 8px;
      padding: 4px 10px;
      border-radius: 999px;
      font-size: 12px;
      font-weight: 700;
      background: rgba(148,163,184,0.15);
      border: 1px solid #1f2937;
    }
    .badge.ok { color: var(--ok); border-color: rgba(34,197,94,0.5); background: rgba(34,197,94,0.1); }
    .badge.bad { color: var(--bad); border-color: rgba(239,68,68,0.5); background: rgba(239,68,68,0.1); }
    .status { color: var(--muted); font-size: 12px; }
    .hint { font-size: 12px; color: var(--muted); margin-top: 6px; }
    .fw { font-family: Consolas, "Courier New", monospace; }
  </style>
</head>
<body>
  <div class="wrap">
    <div class="card">
      %NAV%
      <h1>%CLOUD_PAGE_TITLE%</h1>
      <div class="row" style="margin: 8px 0 16px;">
        <span class="badge %CLOUD_CONNECTED_CLASS%">%CLOUD_CONNECTED_TEXT%</span>
        <span class="status">%CLOUD_FW_LABEL% <span class="fw">%CLOUD_FW_VERSION%</span></span>
        <span class="status">%CLOUD_DEVICE_ID_LABEL% <span class="fw">%CLOUD_DEVICE_ID%</span></span>
      </div>
      <form method="POST" action="/cloud" id="cloud-form">
        <div class="row" style="margin-bottom: 12px;">
          <div class="checkbox">
            <input type="checkbox" id="cloud_enabled" name="cloud_enabled" %CLOUD_ENABLED_CHECKED%>
            <label for="cloud_enabled">%CLOUD_ENABLE_LABEL%</label>
          </div>
        </div>
        <div class="grid" id="cloud-fields">
          <div class="form-row">
            <label>%CLOUD_TRANSPORT_LABEL%</label>
            <select class="field" name="transport">
              <option value="ws" %CLOUD_TRANSPORT_WS_SELECTED%>%CLOUD_TRANSPORT_WS%</option>
              <option value="http" %CLOUD_TRANSPORT_HTTP_SELECTED%>%CLOUD_TRANSPORT_HTTP%</option>
            </select>
            <div class="hint">%CLOUD_TRANSPORT_HINT%</div>
          </div>
          <div class="form-row">
            <label>%CLOUD_HOST_LABEL%</label>
            <input class="field" type="text" name="host" value="%CLOUD_HOST%" placeholder="cloud.example.com">
          </div>
          <div class="form-row">
            <label>%CLOUD_PORT_LABEL%</label>
            <input class="field" type="number" name="port" value="%CLOUD_PORT%" placeholder="443">
          </div>
          <div class="form-row">
            <label>%CLOUD_PATH_LABEL%</label>
            <input class="field" type="text" name="path" value="%CLOUD_PATH%" placeholder="/">
          </div>
          <div class="checkbox" style="margin-top:22px;">
            <input type="checkbox" id="ssl" name="ssl" %CLOUD_SSL_CHECKED%>
            <label for="ssl">%CLOUD_SSL_LABEL%</label>
          </div>
          <div class="form-row">
            <label>%CLOUD_RECONNECT_LABEL%</label>
            <input class="field" type="number" name="reconnect_ms" value="%CLOUD_RECONNECT_MS%" placeholder="5000">
          </div>
          <div class="form-row">
            <label>%CLOUD_EVENT_LABEL%</label>
            <input class="field" type="number" name="event_ms" value="%CLOUD_EVENT_MS%" placeholder="0">
            <div class="hint">%CLOUD_EVENT_HINT%</div>
          </div>
          <div class="form-row full">
            <label>%CLOUD_API_KEY_LABEL%</label>
            <input class="field" type="password" name="api_key" value="%CLOUD_API_KEY%" placeholder="api_key">
          </div>
        </div>
        <div class="row" style="margin-top:12px;">
          <button type="submit">%SAVE_TEXT%</button>
          <span class="status"><strong>%CLOUD_STATUS%</strong></span>
        </div>
      </form>
    </div>
  </div>
  <script>
    const cloudEnabled = document.getElementById('cloud_enabled');
    const fields = document.getElementById('cloud-fields');
    function setFieldsEnabled() {
      const enabled = cloudEnabled && cloudEnabled.checked;
      if (fields) {
        fields.style.opacity = enabled ? '1' : '0.55';
        fields.style.pointerEvents = enabled ? 'auto' : 'none';
      }
      const inputs = fields ? fields.querySelectorAll('input, select, textarea, button') : [];
      inputs.forEach((el) => {
        el.disabled = !enabled;
      });
    }
    if (cloudEnabled) {
      cloudEnabled.addEventListener('change', setFieldsEnabled);
      setFieldsEnabled();
    }
  </script>
</body>
</html>
)HTML";




