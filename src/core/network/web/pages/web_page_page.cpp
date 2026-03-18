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
#include "core/network/web/pages/web_page_page.hpp"

const char kWebInterfaceIndexHtml[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>FCPLC</title>
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
    .section { margin-top: 18px; }
    .row { display: flex; gap: 10px; align-items: center; }
    .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
    label { display: block; margin-bottom: 4px; font-size: 12px; color: var(--muted); }
    input[type=text], input[type=password], select {
      width: 100%;
      background: #0b1220;
      border: 1px solid #334155;
      color: var(--text);
      padding: 10px;
      border-radius: 8px;
    }
    button {
      background: var(--accent);
      color: #00111a;
      border: none;
      padding: 10px 16px;
      border-radius: 8px;
      font-weight: 700;
      cursor: pointer;
    }
    .status { color: var(--muted); font-size: 12px; }
    .notice {
      margin: 6px 0 14px;
      padding: 8px 10px;
      border-radius: 8px;
      background: #0b1220;
      border: 1px solid #334155;
      color: var(--text);
      font-size: 12px;
    }
    .notice:empty { display: none; }
    a { color: #7dd3fc; text-decoration: none; }
    .nav { margin-bottom: 12px; }
    table { width: 100%; border-collapse: collapse; margin-top: 8px; }
    th, td { text-align: left; padding: 6px; border-bottom: 1px solid #1f2937; }
    th { color: var(--muted); font-weight: 600; }
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
  </style>
</head>
<body>
  <div class="wrap">
    <div class="card">
      %NAV%
      <h1>FCPLC</h1>
      <p class="status">%INDEX_LABEL_BOARD%: <strong>%BOARD_NAME%</strong></p>
      <div class="section" style="%DEVICE_BLOCK_STYLE%">
        <h2>%INDEX_LABEL_DEVICE_NAME%</h2>
        <form method="POST" action="/device" id="device-form">
          <div class="row">
            <input type="text" name="device_name" value="%DEVICE_NAME%" placeholder="FCPLC" %DEVICE_NAME_DISABLED%>
            <button type="submit" %DEVICE_SAVE_DISABLED%>%SAVE_TEXT%</button>
          </div>
          <div class="status">%DEVICE_STATUS%</div>
        </form>
      </div>
      <div class="section">
        <h2>%INDEX_LABEL_STATUS%</h2>
        %INDEX_DEVICE_SELECT%
        <table>
          <tbody>
            <tr><td>%INDEX_LABEL_DEVICE_NAME%</td><td><strong id="status-device-name">%STATUS_DEVICE_NAME%</strong></td></tr>
            <tr><td>%INDEX_LABEL_DATE%</td><td><strong id="status-rtc-date">%RTC_DATE%</strong></td></tr>
            <tr><td>%INDEX_LABEL_TIME%</td><td><strong id="status-rtc-time">%RTC_TIME%</strong></td></tr>
            <tr><td>%INDEX_LABEL_RTC_TEMP%</td><td><strong id="status-rtc-temp">%RTC_TEMP%</strong></td></tr>
            <tr><td>%INDEX_LABEL_BOARD_TEMP%</td><td><strong id="status-board-temp">%BOARD_TEMP%</strong></td></tr>
            <tr><td>%INDEX_LABEL_FAN%</td><td id="status-fan">%FAN_STATUS_ICON%</td></tr>
          </tbody>
        </table>
      </div>
    </div>
  </div>
  <script>
    const deviceForm = document.getElementById('device-form');
    const deviceSelect = document.getElementById('index-device');
    const reloadKey = 'device_reload';
    if (sessionStorage.getItem(reloadKey)) {
      sessionStorage.removeItem(reloadKey);
      location.replace(location.pathname);
    }
    if (deviceForm) {
      deviceForm.addEventListener('submit', () => {
        sessionStorage.setItem(reloadKey, '1');
      });
    }
    if (deviceSelect) {
      deviceSelect.addEventListener('change', () => {
        const val = deviceSelect.value || 'local';
        const url = new URL(window.location.href);
        if (val === 'local') {
          url.searchParams.delete('node');
          url.searchParams.delete('node_id');
          url.searchParams.delete('unit');
        } else {
          url.searchParams.set('unit', 'stack');
          url.searchParams.set('node', val);
        }
        window.location.href = url.toString();
      });
    }
    let indexPollBusy = false;
    let indexPollPausedUntil = 0;
    const indexStateCache = {
      device_name: null,
      rtc_date: null,
      rtc_time: null,
      rtc_temp: null,
      board_temp: null,
      fan_html: null
    };
    function rememberIndexState(st) {
      if (!st || typeof st !== 'object') return;
      ['device_name', 'rtc_date', 'rtc_time', 'rtc_temp', 'board_temp', 'fan_html'].forEach((key) => {
        const val = st[key];
        if (typeof val !== 'string') return;
        if (!val.length || val === '...' || val === 'n/a') return;
        indexStateCache[key] = val;
      });
    }
    async function pollIndexState() {
      if (indexPollBusy) return;
      if (document.hidden) return;
      const now = Date.now();
      if (now < indexPollPausedUntil) return;
      indexPollBusy = true;
      try {
        const url = new URL(window.location.origin + '/index/state');
        const curr = new URL(window.location.href);
        const node = curr.searchParams.get('node') || curr.searchParams.get('node_id') || '';
        const unit = curr.searchParams.get('unit') || '';
        if (node) url.searchParams.set('node_id', node);
        if (unit) url.searchParams.set('unit', unit);
        const res = await fetch(url.toString(), { cache: 'no-store', credentials: 'same-origin' });
        if (res.status === 503) {
          indexPollPausedUntil = Date.now() + 1200;
          return;
        }
        if (!res.ok) return;
        const st = await res.json();
        rememberIndexState(st);
        const setText = (id, val) => {
          const el = document.getElementById(id);
          if (el && typeof val === 'string') el.textContent = val;
        };
        setText('status-device-name', indexStateCache.device_name || st.device_name || '');
        setText('status-rtc-date', indexStateCache.rtc_date || st.rtc_date || 'n/a');
        setText('status-rtc-time', indexStateCache.rtc_time || st.rtc_time || 'n/a');
        setText('status-rtc-temp', indexStateCache.rtc_temp || st.rtc_temp || 'n/a');
        setText('status-board-temp', indexStateCache.board_temp || st.board_temp || 'n/a');
        const fan = document.getElementById('status-fan');
        const fanHtml = indexStateCache.fan_html || st.fan_html || '';
        if (fan && typeof fanHtml === 'string' && fanHtml.length && fanHtml !== '...') fan.innerHTML = fanHtml;
      } catch (e) {
      } finally {
        indexPollBusy = false;
      }
    }
    document.addEventListener('visibilitychange', () => {
      if (!document.hidden) {
        setTimeout(pollIndexState, 150);
      }
    });
    setTimeout(pollIndexState, 250);
    setInterval(pollIndexState, 3000);
  </script>
</body>
</html>
)HTML";



