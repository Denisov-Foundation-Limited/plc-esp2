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

static const char kWebInterfaceControllersHtml[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>%CTRL_PAGE_TITLE%</title>
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
    .nav { margin-bottom: 12px; }
    .row { display: flex; gap: 12px; align-items: center; }
    .tile-head { display: flex; align-items: center; justify-content: space-between; gap: 10px; width: 100%; }
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
    .status { color: var(--muted); font-size: 12px; }
    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
      gap: 12px;
      margin-top: 12px;
    }
    .tile {
      display: block;
      position: relative;
      overflow: hidden;
      aspect-ratio: 1 / 1;
      min-height: 0;
      padding: 16px;
      border-radius: 12px;
      background: #0b1220;
      border: 1px solid #1f2937;
      color: var(--text);
      cursor: pointer;
    }
    .tile-body {
      position: relative;
      z-index: 1;
      display: flex;
      flex-direction: column;
      height: 100%;
    }
    .tile-hero {
      position: absolute;
      left: 50%;
      top: 68%;
      width: 132px;
      height: 132px;
      transform: translate(-50%, -50%);
      color: #67d8ff;
      opacity: 0.72;
      pointer-events: none;
      transition: color .2s ease, transform .2s ease;
    }
    .tile:hover .tile-hero {
      color: #a5ecff;
      transform: translate(-50%, calc(-50% - 2px));
    }
    .switch input:not(:checked) ~ .track ~ .tile-hero,
    .tile:has(.switch input:not(:checked)) .tile-hero {
      color: #2b5b72;
      opacity: 0.3;
    }
    .tile-hero svg {
      width: 100%;
      height: 100%;
      display: block;
    }
    button {
      background: var(--accent);
      color: #00111a;
      border: none;
      padding: 6px 10px;
      border-radius: 8px;
      font-weight: 700;
      cursor: pointer;
      font-size: 12px;
    }
    .tile > span {
      display: block;
      margin-top: 6px;
      color: var(--muted);
      font-size: 12px;
    }
    .tile-body > span:first-of-type {
      margin-top: 10px;
      font-size: 11px;
      line-height: 1.4;
      max-width: 26ch;
    }
    .tile-body > span.status {
      max-width: 100%;
    }
    @media (max-width: 720px) {
      .wrap { margin: 20px auto; }
      .card { padding: 16px; }
      h1 { font-size: 20px; }
      .tile-head { flex-wrap: wrap; }
      .grid { grid-template-columns: 1fr; }
      .tile {
        aspect-ratio: auto;
        min-height: 240px;
      }
      .tile-hero {
        top: 72%;
        width: 104px;
        height: 104px;
        opacity: 0.56;
      }
      .tile-body > span.status { max-width: 100%; }
    }
  </style>
</head>
<body>
  <div class="wrap">
    <div class="card">
      %NAV%
      <h1>%CTRL_PAGE_TITLE%</h1>
      %CONTROLLERS_EMPTY_HINT%
      <div class="grid">
        <div class="tile js-controller-tile" data-href="/sockets" style="%ACL_HIDE_SOCKETS%">
          <div class="tile-body">
          <form method="POST" action="/controllers" id="sockets-form">
            <input type="hidden" name="ctrl" value="sockets">
            <div class="tile-head">
              <a href="/sockets">%CTRL_SOCKETS_TITLE%</a>
              <label class="switch">
                <input type="checkbox" id="sockets-enabled" name="sockets_enabled" %SOCKETS_ENABLED_CHECKED% %CONTROLLERS_SWITCH_DISABLED%>
                <span class="track"><span class="knob"></span></span>
              </label>
            </div>
          </form>
          <span>%CTRL_SOCKETS_DESC%</span>
          <span class="status">%SOCKETS_STATUS%</span>
          </div>
          <div class="tile-hero" aria-hidden="true"><svg viewBox="0 0 96 96" fill="none"><rect x="18" y="12" width="60" height="72" rx="18" stroke="currentColor" stroke-width="5"/><circle cx="36" cy="36" r="6" fill="currentColor"/><circle cx="60" cy="36" r="6" fill="currentColor"/><rect x="41" y="54" width="14" height="20" rx="5" fill="currentColor"/></svg></div>
        </div>
        <div class="tile js-controller-tile" data-href="/lights" style="%ACL_HIDE_LIGHTS%">
          <div class="tile-body">
          <form method="POST" action="/controllers" id="lights-form">
            <input type="hidden" name="ctrl" value="lights">
            <div class="tile-head">
              <a href="/lights">%CTRL_LIGHTS_TITLE%</a>
              <label class="switch">
                <input type="checkbox" id="lights-enabled" name="lights_enabled" %LIGHTS_ENABLED_CHECKED% %CONTROLLERS_SWITCH_DISABLED%>
                <span class="track"><span class="knob"></span></span>
              </label>
            </div>
          </form>
          <span>%CTRL_LIGHTS_DESC%</span>
          <span class="status">%LIGHTS_STATUS%</span>
          </div>
          <div class="tile-hero" aria-hidden="true"><svg viewBox="0 0 96 96" fill="none"><path d="M48 14c-14.4 0-26 11.6-26 26 0 10 5.6 18.7 13.9 23.1 2.6 1.4 4.1 4 4.1 6.9V72h16v-2c0-2.9 1.5-5.5 4.1-6.9C68.4 58.7 74 50 74 40c0-14.4-11.6-26-26-26Z" stroke="currentColor" stroke-width="5"/><path d="M38 78h20M40 84h16" stroke="currentColor" stroke-width="5" stroke-linecap="round"/><path d="M40 46c2.5-4 5.2-6 8-6s5.5 2 8 6" stroke="currentColor" stroke-width="5" stroke-linecap="round"/></svg></div>
        </div>
        <div class="tile js-controller-tile" data-href="/meteo" style="%ACL_HIDE_METEO%">
          <div class="tile-body">
          <form method="POST" action="/controllers" id="meteo-form">
            <input type="hidden" name="ctrl" value="meteo">
            <div class="tile-head">
              <a href="/meteo">%CTRL_METEO_TITLE%</a>
              <label class="switch">
                <input type="checkbox" id="meteo-enabled" name="meteo_enabled" %METEO_ENABLED_CHECKED% %CONTROLLERS_SWITCH_DISABLED%>
                <span class="track"><span class="knob"></span></span>
              </label>
            </div>
          </form>
          <span>%CTRL_METEO_DESC%</span>
          <span class="status">%METEO_STATUS%</span>
          </div>
          <div class="tile-hero" aria-hidden="true"><svg viewBox="0 0 96 96" fill="none"><path d="M28 58c-7.7 0-14-6.3-14-14s6.3-14 14-14c2.2 0 4.2.5 6.1 1.4C37.5 24 44.1 20 52 20c11 0 20 9 20 20v1c6.6 1 12 6.7 12 13.6C84 62 77.9 68 70.4 68H28Z" stroke="currentColor" stroke-width="5"/><path d="M48 42v24" stroke="currentColor" stroke-width="5" stroke-linecap="round"/><circle cx="48" cy="34" r="8" stroke="currentColor" stroke-width="5"/><path d="M62 72c0 5.5-4.5 10-10 10s-10-4.5-10-10c0-7 10-18 10-18s10 11 10 18Z" fill="currentColor" opacity=".45"/></svg></div>
        </div>
        <div class="tile js-controller-tile" data-href="/thermo" style="%ACL_HIDE_THERMO%">
          <div class="tile-body">
          <form method="POST" action="/controllers" id="thermo-form">
            <input type="hidden" name="ctrl" value="thermo">
            <div class="tile-head">
              <a href="/thermo">%CTRL_THERMO_TITLE%</a>
              <label class="switch">
                <input type="checkbox" id="thermo-enabled" name="thermo_enabled" %THERMO_ENABLED_CHECKED% %CONTROLLERS_SWITCH_DISABLED%>
                <span class="track"><span class="knob"></span></span>
              </label>
            </div>
          </form>
          <span>%CTRL_THERMO_DESC%</span>
          <span class="status">%THERMO_STATUS%</span>
          </div>
          <div class="tile-hero" aria-hidden="true"><svg viewBox="0 0 96 96" fill="none"><rect x="30" y="12" width="36" height="72" rx="18" stroke="currentColor" stroke-width="5"/><path d="M48 24v34" stroke="currentColor" stroke-width="5" stroke-linecap="round"/><circle cx="48" cy="66" r="12" fill="currentColor"/><path d="M24 28h10M24 42h10M24 56h10" stroke="currentColor" stroke-width="5" stroke-linecap="round"/></svg></div>
        </div>
        <div class="tile js-controller-tile" data-href="/tanks" style="%ACL_HIDE_TANKS%">
          <div class="tile-body">
          <form method="POST" action="/controllers" id="tanks-form">
            <input type="hidden" name="ctrl" value="tanks">
            <div class="tile-head">
              <a href="/tanks">%CTRL_TANKS_TITLE%</a>
              <label class="switch">
                <input type="checkbox" id="tanks-enabled" name="tanks_enabled" %TANKS_ENABLED_CHECKED% %CONTROLLERS_SWITCH_DISABLED%>
                <span class="track"><span class="knob"></span></span>
              </label>
            </div>
          </form>
          <span>%CTRL_TANKS_DESC%</span>
          <span class="status">%TANKS_STATUS%</span>
          </div>
          <div class="tile-hero" aria-hidden="true"><svg viewBox="0 0 96 96" fill="none"><rect x="22" y="14" width="52" height="68" rx="12" stroke="currentColor" stroke-width="5"/><path d="M30 56c8-4 28-4 36 0v14H30V56Z" fill="currentColor" opacity=".45"/><path d="M38 14v-6h20v6" stroke="currentColor" stroke-width="5" stroke-linecap="round"/><path d="M30 44h36" stroke="currentColor" stroke-width="5" stroke-linecap="round"/></svg></div>
        </div>
        <div class="tile js-controller-tile" data-href="/watering" style="%ACL_HIDE_WATERING%">
          <div class="tile-body">
          <form method="POST" action="/controllers" id="watering-form">
            <input type="hidden" name="ctrl" value="watering">
            <div class="tile-head">
              <a href="/watering">%CTRL_WATERING_TITLE%</a>
              <label class="switch">
                <input type="checkbox" id="watering-enabled" name="watering_enabled" %WATERING_ENABLED_CHECKED% %CONTROLLERS_SWITCH_DISABLED%>
                <span class="track"><span class="knob"></span></span>
              </label>
            </div>
          </form>
          <span>%CTRL_WATERING_DESC%</span>
          <span class="status">%WATERING_STATUS%</span>
          </div>
          <div class="tile-hero" aria-hidden="true"><svg viewBox="0 0 96 96" fill="none"><path d="M34 18h28" stroke="currentColor" stroke-width="5" stroke-linecap="round"/><circle cx="34" cy="18" r="4" fill="currentColor"/><circle cx="62" cy="18" r="4" fill="currentColor"/><circle cx="48" cy="18" r="7" fill="currentColor"/><path d="M48 25v11" stroke="currentColor" stroke-width="5" stroke-linecap="round"/><path d="M22 42h34c9 0 17 8 17 17v2c0 4-3 7-7 7h-8V57c0-4-3-7-7-7H22z" fill="currentColor"/><path d="M58 42h8c10 0 18 8 18 18v12h-8V61c0-6-5-11-11-11h-7z" fill="currentColor"/><path d="M70 72h14v4H70z" fill="currentColor"/><path d="M66 79c0 6.6-5.4 12-12 12s-12-5.4-12-12c0-7.6 12-20 12-20s12 12.4 12 20Z" fill="currentColor" opacity=".7"/><path d="M57 72c2 3 3 6 3 9 0 4.5-2.7 8-7 8" stroke="#0b1220" stroke-width="3" stroke-linecap="round" opacity=".55"/></svg></div>
        </div>
<div class="tile js-controller-tile" data-href="/septic" style="%ACL_HIDE_SEPTIC%">
          <div class="tile-body">
          <form method="POST" action="/controllers" id="septic-form">
            <input type="hidden" name="ctrl" value="septic">
            <div class="tile-head">
              <a href="/septic">%CTRL_SEPTIC_TITLE%</a>
              <label class="switch">
                <input type="checkbox" id="septic-enabled" name="septic_enabled" %SEPTIC_ENABLED_CHECKED% %CONTROLLERS_SWITCH_DISABLED%>
                <span class="track"><span class="knob"></span></span>
              </label>
            </div>
          </form>
          <span>%CTRL_SEPTIC_DESC%</span>
          <span class="status">%SEPTIC_STATUS%</span>
          </div>
          <div class="tile-hero" aria-hidden="true"><svg viewBox="0 0 96 96" fill="none"><rect x="18" y="18" width="60" height="50" rx="10" stroke="currentColor" stroke-width="5"/><path d="M18 52h60" stroke="currentColor" stroke-width="5"/><path d="M32 76h32" stroke="currentColor" stroke-width="5" stroke-linecap="round"/><path d="M48 68v8" stroke="currentColor" stroke-width="5" stroke-linecap="round"/><path d="M28 42c10-4 30-4 40 0" stroke="currentColor" stroke-width="5" stroke-linecap="round"/></svg></div>
        </div>
        <div class="tile js-controller-tile" data-href="/ring" style="%ACL_HIDE_RING%">
          <div class="tile-body">
          <form method="POST" action="/controllers" id="ring-form">
            <input type="hidden" name="ctrl" value="ring">
            <div class="tile-head">
              <a href="/ring">%CTRL_RING_TITLE%</a>
              <label class="switch">
                <input type="checkbox" id="ring-enabled" name="ring_enabled" %RING_ENABLED_CHECKED% %CONTROLLERS_SWITCH_DISABLED%>
                <span class="track"><span class="knob"></span></span>
              </label>
            </div>
          </form>
          <span>%CTRL_RING_DESC%</span>
          <span class="status">%RING_STATUS%</span>
          </div>
          <div class="tile-hero" aria-hidden="true"><svg viewBox="0 0 96 96" fill="none"><path d="M48 18c-12 0-22 10-22 22v11c0 7-2.8 13.7-7.8 18.7L14 74h68l-4.2-4.3C72.8 64.7 70 58 70 51V40c0-12-10-22-22-22Z" stroke="currentColor" stroke-width="5" stroke-linejoin="round"/><path d="M40 80c2 4 4.8 6 8 6s6-2 8-6" stroke="currentColor" stroke-width="5" stroke-linecap="round"/></svg></div>
        </div>
        <div class="tile js-controller-tile" data-href="/security" style="%ACL_HIDE_SECURITY%">
          <div class="tile-body">
          <form method="POST" action="/controllers" id="security-form">
            <input type="hidden" name="ctrl" value="security">
            <div class="tile-head">
              <a href="/security">%CTRL_SECURITY_TITLE%</a>
              <label class="switch">
                <input type="checkbox" id="security-enabled" name="security_enabled" %SECURITY_ENABLED_CHECKED% %CONTROLLERS_SWITCH_DISABLED%>
                <span class="track"><span class="knob"></span></span>
              </label>
            </div>
          </form>
          <span>%CTRL_SECURITY_DESC%</span>
          <span class="status">%SECURITY_STATUS%</span>
          </div>
          <div class="tile-hero" aria-hidden="true"><svg viewBox="0 0 96 96" fill="none"><path d="M48 12 22 22v22c0 18 10.7 30.8 26 39 15.3-8.2 26-21 26-39V22L48 12Z" stroke="currentColor" stroke-width="5" stroke-linejoin="round"/><rect x="38" y="40" width="20" height="18" rx="4" stroke="currentColor" stroke-width="5"/><path d="M42 40v-6a6 6 0 1 1 12 0v6" stroke="currentColor" stroke-width="5" stroke-linecap="round"/></svg></div>
        </div>
        <div class="tile js-controller-tile" data-href="/avr" style="%ACL_HIDE_AVR%">
          <div class="tile-body">
          <form method="POST" action="/controllers" id="avr-form">
            <input type="hidden" name="ctrl" value="avr">
            <div class="tile-head">
              <a href="/avr">%CTRL_AVR_TITLE%</a>
              <label class="switch">
                <input type="checkbox" id="avr-enabled" name="avr_enabled" %AVR_ENABLED_CHECKED% %CONTROLLERS_SWITCH_DISABLED%>
                <span class="track"><span class="knob"></span></span>
              </label>
            </div>
          </form>
          <span>%CTRL_AVR_DESC%</span>
          <span class="status">%AVR_STATUS%</span>
          </div>
          <div class="tile-hero" aria-hidden="true"><svg viewBox="0 0 96 96" fill="none"><path d="M54 10 28 52h18l-4 34 26-42H50l4-34Z" fill="currentColor"/><path d="M18 24h18M60 72h18" stroke="currentColor" stroke-width="5" stroke-linecap="round"/></svg></div>
        </div>
        <div class="tile js-controller-tile" data-href="/leak" style="%ACL_HIDE_LEAK%">
          <div class="tile-body">
          <form method="POST" action="/controllers" id="leak-form">
            <input type="hidden" name="ctrl" value="leak">
            <div class="tile-head">
              <a href="/leak">%CTRL_LEAK_TITLE%</a>
              <label class="switch">
                <input type="checkbox" id="leak-enabled" name="leak_enabled" %LEAK_ENABLED_CHECKED% %CONTROLLERS_SWITCH_DISABLED%>
                <span class="track"><span class="knob"></span></span>
              </label>
            </div>
          </form>
          <span>%CTRL_LEAK_DESC%</span>
          <span class="status">%LEAK_STATUS%</span>
          </div>
          <div class="tile-hero" aria-hidden="true"><svg viewBox="0 0 96 96" fill="none"><path d="M48 14c-10 14-24 29.1-24 43a24 24 0 0 0 48 0c0-13.9-14-29-24-43Z" stroke="currentColor" stroke-width="5"/><path d="M30 70c8-5 28-5 36 0" stroke="currentColor" stroke-width="5" stroke-linecap="round"/><circle cx="48" cy="58" r="8" fill="currentColor" opacity=".45"/></svg></div>
        </div>
      </div>
    </div>
  </div>
  <script>
    const controllersLock = "%CONTROLLERS_SWITCH_LOCK%" === "1";
    document.querySelectorAll('.js-controller-tile').forEach((tile) => {
      tile.addEventListener('click', (ev) => {
        const interactive = ev.target.closest('a, button, input, label, form');
        if (interactive) return;
        const href = tile.dataset.href;
        if (href) window.location.href = href;
      });
    });
    document.querySelectorAll('.js-controller-tile a, .js-controller-tile button, .js-controller-tile input, .js-controller-tile label, .js-controller-tile form').forEach((el) => {
      el.addEventListener('click', (ev) => ev.stopPropagation());
    });
    if (controllersLock) {
      document.querySelectorAll('input[type="checkbox"][id$="-enabled"]').forEach((el) => { el.disabled = true; });
    }
    const socketsToggle = document.getElementById('sockets-enabled');
    const socketsForm = document.getElementById('sockets-form');
    if (socketsToggle && socketsForm) {
      socketsToggle.addEventListener('change', () => socketsForm.submit());
    }
    const lightsToggle = document.getElementById('lights-enabled');
    const lightsForm = document.getElementById('lights-form');
    if (lightsToggle && lightsForm) {
      lightsToggle.addEventListener('change', () => lightsForm.submit());
    }
    const meteoToggle = document.getElementById('meteo-enabled');
    const meteoForm = document.getElementById('meteo-form');
    if (meteoToggle && meteoForm) {
      meteoToggle.addEventListener('change', () => meteoForm.submit());
    }
    const thermoToggle = document.getElementById('thermo-enabled');
    const thermoForm = document.getElementById('thermo-form');
    if (thermoToggle && thermoForm) {
      thermoToggle.addEventListener('change', () => thermoForm.submit());
    }
    const tanksToggle = document.getElementById('tanks-enabled');
    const tanksForm = document.getElementById('tanks-form');
    if (tanksToggle && tanksForm) {
      tanksToggle.addEventListener('change', () => tanksForm.submit());
    }
    const wateringToggle = document.getElementById('watering-enabled');
    const wateringForm = document.getElementById('watering-form');
    if (wateringToggle && wateringForm) {
      wateringToggle.addEventListener('change', () => wateringForm.submit());
    }
    const septicToggle = document.getElementById('septic-enabled');
    const septicForm = document.getElementById('septic-form');
    if (septicToggle && septicForm) {
      septicToggle.addEventListener('change', () => septicForm.submit());
    }
    const ringToggle = document.getElementById('ring-enabled');
    const ringForm = document.getElementById('ring-form');
    if (ringToggle && ringForm) {
      ringToggle.addEventListener('change', () => ringForm.submit());
    }
    const securityToggle = document.getElementById('security-enabled');
    const securityForm = document.getElementById('security-form');
    if (securityToggle && securityForm) {
      securityToggle.addEventListener('change', () => securityForm.submit());
    }
    const avrToggle = document.getElementById('avr-enabled');
    const avrForm = document.getElementById('avr-form');
    if (avrToggle && avrForm) {
      avrToggle.addEventListener('change', () => avrForm.submit());
    }
    const leakToggle = document.getElementById('leak-enabled');
    const leakForm = document.getElementById('leak-form');
    if (leakToggle && leakForm) {
      leakToggle.addEventListener('change', () => leakForm.submit());
    }
  </script>
</body>
</html>
)HTML";













