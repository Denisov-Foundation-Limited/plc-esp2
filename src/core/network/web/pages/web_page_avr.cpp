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
#include "core/network/web/pages/web_page_avr.hpp"

const char kWebInterfaceAvrHtml[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>%AVR_PAGE_TITLE%</title>
  <style>
    :root {
      --bg: #0f172a;
      --card: #111827;
      --accent: #38bdf8;
      --text: #e5e7eb;
      --muted: #94a3b8;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      min-height: 100vh;
      font-family: "Segoe UI", Tahoma, Arial, sans-serif;
      background: radial-gradient(1200px 600px at 10% -10%, #1f2937 0%, #0b1220 60%, #080d17 100%);
      color: var(--text);
    }
    .wrap { max-width: 980px; margin: 32px auto; padding: 0 16px; }
    .card {
      background: linear-gradient(180deg, #0f172a 0%, #0b1220 100%);
      border: 1px solid #1f2937;
      border-radius: 14px;
      padding: 22px;
      box-shadow: 0 10px 30px rgba(0,0,0,0.35);
    }
    h1 { margin: 0 0 8px; font-size: 22px; }
    .status { color: var(--muted); font-size: 13px; margin-bottom: 12px; }
    .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(240px, 1fr)); gap: 10px; }
    label { display: block; margin: 0 0 4px; font-size: 12px; color: var(--muted); }
    .field {
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
    }    .row { display: flex; align-items: center; gap: 12px; margin-bottom: 10px; }
    .radio-group {
      display: flex;
      gap: 14px;
      flex-wrap: wrap;
      padding: 10px 12px;
      border: 1px solid #334155;
      border-radius: 8px;
      background: #0b1220;
    }
    .radio-item {
      display: inline-flex;
      align-items: center;
      gap: 6px;
      color: var(--text);
      font-size: 13px;
      margin: 0;
    }
    .radio-item input[type="radio"] { margin: 0; }
    .muted { color: var(--muted); font-size: 12px; }
    .actions { display: flex; gap: 10px; margin-top: 12px; }
    button {
      background: var(--accent);
      color: #00111a;
      border: none;
      padding: 10px 14px;
      border-radius: 8px;
      font-weight: 700;
      cursor: pointer;
    }
    .btn-muted { background: #64748b; color: #fff; }
    .group-title { margin: 16px 0 8px; color: #bae6fd; font-size: 13px; }
    .state-indicators { display: flex; gap: 14px; flex-wrap: wrap; margin-bottom: 12px; }
    .state-item { display: inline-flex; align-items: center; gap: 8px; color: var(--muted); font-size: 13px; }
    .state-dot {
      width: 10px;
      height: 10px;
      border-radius: 50%;
      background: #64748b;
      box-shadow: 0 0 0 2px rgba(15,23,42,0.6);
    }
    .state-dot.net-on { background: #22c55e; }
    .state-dot.err-on { background: #ef4444; }
    a { color: #7dd3fc; text-decoration: none; }
    .nav { margin-bottom: 12px; }
    @media (max-width: 720px) {
      .wrap { margin: 20px auto; }
      .card { padding: 16px; }
      .actions { flex-wrap: wrap; }
    }
  </style>
</head>
<body>
  <div class="wrap">
    <div class="card">
      %NAV%
      <h1>%AVR_PAGE_TITLE%</h1>
      %AVR_DEVICE_SELECT%
      <div class="status">%AVR_STATUS%</div>
      <div class="status">%AVR_STATE_TEXT%</div>
      <form method="POST" action="%AVR_FORM_ACTION%" id="avr-form">
        <input type="hidden" name="avr_save" value="1">
        <div class="row">
          <label><input type="checkbox" name="avr_enabled" %AVR_ENABLED_CHECKED%> %AVR_CHK_ENABLED%</label>
          <label><input type="checkbox" name="avr_auto_mode" %AVR_AUTO_MODE_CHECKED%> %AVR_CHK_AUTO_MODE%</label>
          <label><input type="checkbox" name="avr_prefer_main" %AVR_PREFER_MAIN_CHECKED%> %AVR_CHK_PREFER_MAIN%</label>
          <label><input type="checkbox" name="avr_auto_return_main" %AVR_AUTO_RETURN_MAIN_CHECKED%> %AVR_CHK_AUTO_RETURN_MAIN%</label>
        </div>

        <div class="group-title">%AVR_GROUP_MANUAL_SOURCE%</div>
        <div class="grid">
          <div>
            <label>%AVR_LABEL_SOURCE%</label>
            <div class="radio-group">
              <label class="radio-item"><input type="radio" name="avr_manual_source" value="off" %AVR_MANUAL_OFF_SELECTED%>%AVR_RADIO_OFF%</label>
              <label class="radio-item"><input type="radio" name="avr_manual_source" value="main" %AVR_MANUAL_MAIN_SELECTED%>%AVR_RADIO_MAIN%</label>
              <label class="radio-item"><input type="radio" name="avr_manual_source" value="reserve" %AVR_MANUAL_RESERVE_SELECTED%>%AVR_RADIO_RESERVE%</label>
            </div>
          </div>
        </div>

        <div class="group-title">%AVR_GROUP_PORTS%</div>
        <div class="grid">
          <div><label>%AVR_PORT_MAIN_OK%</label><select class="field avr-select" data-type="dinput" data-selected="%AVR_MAIN_OK_SELECTED%" name="avr_main_ok"></select></div>
          <div><label>%AVR_PORT_FEEDBACK_MAIN%</label><select class="field avr-select" data-type="dinput" data-selected="%AVR_FB_MAIN_SELECTED%" name="avr_feedback_main"></select></div>
          <div><label>%AVR_PORT_RELAY_MAIN%</label><select class="field avr-select" data-type="relay" data-selected="%AVR_RELAY_MAIN_SELECTED%" name="avr_relay_main"></select></div>
          <div><label>%AVR_PORT_RESERVE_OK%</label><select class="field avr-select" data-type="dinput" data-selected="%AVR_RESERVE_OK_SELECTED%" name="avr_reserve_ok"></select></div>
          <div><label>%AVR_PORT_FEEDBACK_RESERVE%</label><select class="field avr-select" data-type="dinput" data-selected="%AVR_FB_RESERVE_SELECTED%" name="avr_feedback_reserve"></select></div>
          <div><label>%AVR_PORT_RELAY_RESERVE%</label><select class="field avr-select" data-type="relay" data-selected="%AVR_RELAY_RESERVE_SELECTED%" name="avr_relay_reserve"></select></div>
        </div>

        <div class="group-title">%AVR_GROUP_PORT_LOGIC%</div>
        <div class="row">
          <label><input type="checkbox" name="avr_main_ok_active_low" %AVR_MAIN_OK_AL_CHECKED%> %AVR_AL_MAIN_OK%</label>
          <label><input type="checkbox" name="avr_reserve_ok_active_low" %AVR_RESERVE_OK_AL_CHECKED%> %AVR_AL_RESERVE_OK%</label>
          <label><input type="checkbox" name="avr_feedback_main_active_low" %AVR_FB_MAIN_AL_CHECKED%> %AVR_AL_FEEDBACK_MAIN%</label>
          <label><input type="checkbox" name="avr_feedback_reserve_active_low" %AVR_FB_RESERVE_AL_CHECKED%> %AVR_AL_FEEDBACK_RESERVE%</label>
        </div>
        <div class="row">
          <label><input type="checkbox" name="avr_relay_main_invert" %AVR_RELAY_MAIN_INV_CHECKED%> %AVR_INV_RELAY_MAIN%</label>
          <label><input type="checkbox" name="avr_relay_reserve_invert" %AVR_RELAY_RESERVE_INV_CHECKED%> %AVR_INV_RELAY_RESERVE%</label>
        </div>

        <div class="group-title">%AVR_GROUP_TIMINGS%</div>
        <div class="grid">
          <div><label>%AVR_TIMING_DEBOUNCE%</label><input class="field" type="number" min="0" name="avr_debounce_ms" value="%AVR_DEBOUNCE_MS%"></div>
          <div><label>%AVR_TIMING_LOSS_DELAY%</label><input class="field" type="number" min="0" name="avr_loss_delay_ms" value="%AVR_LOSS_DELAY_MS%"></div>
          <div><label>%AVR_TIMING_RETURN_DELAY%</label><input class="field" type="number" min="0" name="avr_return_delay_ms" value="%AVR_RETURN_DELAY_MS%"></div>
          <div><label>%AVR_TIMING_BREAK%</label><input class="field" type="number" min="0" name="avr_break_ms" value="%AVR_BREAK_MS%"></div>
          <div><label>%AVR_TIMING_WARMUP%</label><input class="field" type="number" min="0" name="avr_warmup_ms" value="%AVR_WARMUP_MS%"></div>
          <div><label>%AVR_TIMING_TRANSFER_TIMEOUT%</label><input class="field" type="number" min="0" name="avr_transfer_timeout_ms" value="%AVR_TRANSFER_TIMEOUT_MS%"></div>
        </div>
      </form>

      <form method="POST" action="%AVR_FAULT_FORM_ACTION%" id="avr-fault-form">
        <input type="hidden" name="avr_clear_fault" value="1">
      </form>

      <div class="actions">
        %AVR_SAVE_BTN%
        %AVR_FAULT_BTN%
      </div>
      <div class="status">%AVR_PORTS_HELP%</div>
    </div>
  </div>
  <script>
    const avrOptions = { dinput: %AVR_DINPUT_JSON%, relay: %AVR_RELAY_JSON% };
    const avrUsed = { dinput: %AVR_DINPUT_USED_JSON%, relay: %AVR_RELAY_USED_JSON% };
    function itemValue(item) { return (item && typeof item === 'object') ? String(item.v) : String(item); }
    function itemLabel(item, type) {
      if (item && typeof item === 'object' && item.l) return item.l;
      const v = itemValue(item);
      return type === 'relay' ? ('rly' + v) : ('in' + v);
    }
    function buildOptions(type, selected, used) {
      const list = avrOptions[type] || [];
      const sel = String(selected || '');
      let html = '<option value=""' + (sel === '' ? ' selected' : '') + '>-</option>';
      for (let i = 0; i < list.length; i++) {
        const val = itemValue(list[i]);
        const n = parseInt(val, 10);
        const busy = !Number.isNaN(n) && used.has(n) && val !== sel;
        html += '<option value="' + val + '"' + (val === sel ? ' selected' : '') + (busy ? ' disabled' : '') + '>' + itemLabel(list[i], type) + '</option>';
      }
      return html;
    }
    function refreshAvrSelects() {
      const byType = {};
      document.querySelectorAll('select.avr-select').forEach((el) => {
        const type = el.dataset.type;
        if (!byType[type]) {
          byType[type] = new Set((avrUsed[type] || []).map((v) => parseInt(v, 10)).filter((v) => !Number.isNaN(v)));
        }
        const v = parseInt(el.value || el.dataset.selected || '', 10);
        if (!Number.isNaN(v)) byType[type].add(v);
      });
      document.querySelectorAll('select.avr-select').forEach((el) => {
        const type = el.dataset.type;
        const sel = el.value || el.dataset.selected || '';
        el.innerHTML = buildOptions(type, sel, byType[type] || new Set());
        el.value = sel;
      });
    }
    refreshAvrSelects();
    document.querySelectorAll('select.avr-select').forEach((el) => el.addEventListener('change', refreshAvrSelects));
    (function () {
      const sel = document.getElementById('avr-device');
      if (!sel) return;
      sel.addEventListener('change', () => {
        const v = sel.value;
        if (!v || v === '0' || v === 'local') {
          location.href = '/avr';
          return;
        }
        location.href = '/avr?node=' + encodeURIComponent(v) + '&unit=stack';
      });
    })();
    if (window.__plcSetupIdleReload) {
      window.__plcSetupIdleReload(6000);
    }
  </script>
</body>
</html>
)HTML";






