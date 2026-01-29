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
  <title>Контроллеры</title>
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
    .tile-head { display: flex; align-items: center; justify-content: space-between; gap: 10px; }
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
    .status { color: var(--muted); font-size: 12px; }
    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
      gap: 12px;
      margin-top: 12px;
    }
    .tile {
      display: block;
      padding: 16px;
      border-radius: 12px;
      background: #0b1220;
      border: 1px solid #1f2937;
      color: var(--text);
    }
    .tile > span {
      display: block;
      margin-top: 6px;
      color: var(--muted);
      font-size: 12px;
    }
    @media (max-width: 720px) {
      .wrap { margin: 20px auto; }
      .card { padding: 16px; }
      h1 { font-size: 20px; }
      .tile-head { flex-wrap: wrap; }
      .grid { grid-template-columns: 1fr; }
    }
  </style>
</head>
<body>
  <div class="wrap">
    <div class="card">
      %NAV%
      <h1>Контроллеры</h1>
      <p>Плата: <strong>%BOARD_NAME%</strong></p>
      <div class="grid">
        <div class="tile">
          <form method="POST" action="/controllers" id="sockets-form">
            <input type="hidden" name="ctrl" value="sockets">
            <div class="tile-head">
              <a href="/sockets">Розетки</a>
              <label class="switch">
                <input type="checkbox" id="sockets-enabled" name="sockets_enabled" %SOCKETS_ENABLED_CHECKED%>
                <span class="track"><span class="knob"></span></span>
              </label>
            </div>
          </form>
          <span>Настройка реле и кнопок</span>
          <span class="status">Розетки: <strong>%SOCKETS_ENABLED_LABEL%</strong></span>
          <span class="status">%CONTROLLERS_STATUS%</span>
        </div>
        <div class="tile">
          <form method="POST" action="/controllers" id="lights-form">
            <input type="hidden" name="ctrl" value="sockets">
            <div class="tile-head">
              <a href="/lights">Свет</a>
              <label class="switch">
                <input type="checkbox" id="lights-enabled" name="sockets_enabled" %SOCKETS_ENABLED_CHECKED%>
                <span class="track"><span class="knob"></span></span>
              </label>
            </div>
          </form>
          <span>Настройка освещения</span>
          <span class="status">Свет: <strong>%SOCKETS_ENABLED_LABEL%</strong></span>
          <span class="status">%CONTROLLERS_STATUS%</span>
        </div>
        <div class="tile">
          <form method="POST" action="/controllers" id="meteo-form">
            <input type="hidden" name="ctrl" value="meteo">
            <div class="tile-head">
              <a href="/meteo">Метео</a>
              <label class="switch">
                <input type="checkbox" id="meteo-enabled" name="meteo_enabled" %METEO_ENABLED_CHECKED%>
                <span class="track"><span class="knob"></span></span>
              </label>
            </div>
          </form>
          <span>Температура и влажность</span>
          <span class="status">Метео: <strong>%METEO_ENABLED_LABEL%</strong></span>
          <span class="status">%METEO_STATUS%</span>
        </div>
        <div class="tile">
          <form method="POST" action="/controllers" id="thermo-form">
            <input type="hidden" name="ctrl" value="thermo">
            <div class="tile-head">
              <a href="/thermo">Термо</a>
              <label class="switch">
                <input type="checkbox" id="thermo-enabled" name="thermo_enabled" %THERMO_ENABLED_CHECKED%>
                <span class="track"><span class="knob"></span></span>
              </label>
            </div>
          </form>
          <span>Климат: нагрев/охлаждение/авто</span>
          <span class="status">Термо: <strong>%THERMO_ENABLED_LABEL%</strong></span>
          <span class="status">%THERMO_STATUS%</span>
        </div>
        <div class="tile">
          <form method="POST" action="/controllers" id="tanks-form">
            <input type="hidden" name="ctrl" value="tanks">
            <div class="tile-head">
              <a href="/tanks">Баки</a>
              <label class="switch">
                <input type="checkbox" id="tanks-enabled" name="tanks_enabled" %TANKS_ENABLED_CHECKED%>
                <span class="track"><span class="knob"></span></span>
              </label>
            </div>
          </form>
          <span>Уровень воды и автоматика</span>
          <span class="status">Баки: <strong>%TANKS_ENABLED_LABEL%</strong></span>
          <span class="status">%TANKS_STATUS%</span>
        </div>
        <div class="tile">
          <form method="POST" action="/controllers" id="septic-form">
            <input type="hidden" name="ctrl" value="septic">
            <div class="tile-head">
              <a href="/septic">Септик</a>
              <label class="switch">
                <input type="checkbox" id="septic-enabled" name="septic_enabled" %SEPTIC_ENABLED_CHECKED%>
                <span class="track"><span class="knob"></span></span>
              </label>
            </div>
          </form>
          <span>Уровень и индикация</span>
          <span class="status">Септик: <strong>%SEPTIC_ENABLED_LABEL%</strong></span>
          <span class="status">%SEPTIC_STATUS%</span>
        </div>
        <div class="tile">
          <form method="POST" action="/controllers" id="security-form">
            <input type="hidden" name="ctrl" value="security">
            <div class="tile-head">
              <a href="/security">Охрана</a>
              <label class="switch">
                <input type="checkbox" id="security-enabled" name="security_enabled" %SECURITY_ENABLED_CHECKED%>
                <span class="track"><span class="knob"></span></span>
              </label>
            </div>
          </form>
          <span>Датчики и тревога</span>
          <span class="status">Охрана: <strong>%SECURITY_ENABLED_LABEL%</strong></span>
          <span class="status">%SECURITY_STATUS%</span>
        </div>
      </div>
    </div>
  </div>
  <script>
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
    const septicToggle = document.getElementById('septic-enabled');
    const septicForm = document.getElementById('septic-form');
    if (septicToggle && septicForm) {
      septicToggle.addEventListener('change', () => septicForm.submit());
    }
    const securityToggle = document.getElementById('security-enabled');
    const securityForm = document.getElementById('security-form');
    if (securityToggle && securityForm) {
      securityToggle.addEventListener('change', () => securityForm.submit());
    }
  </script>
</body>
</html>
)HTML";



