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

static const char kWebInterfaceRulesHtml[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Правила</title>
  <style>
    :root {
      --bg: #0f172a;
      --card: #111827;
      --tile: #0b1220;
      --text: #e5e7eb;
      --muted: #94a3b8;
      --border: #1f2937;
      --accent: #38bdf8;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      min-height: 100vh;
      font-family: "Segoe UI", Tahoma, Arial, sans-serif;
      background: radial-gradient(1200px 600px at 10% -10%, #1f2937 0%, #0b1220 60%, #080d17 100%);
      color: var(--text);
    }
    .wrap { max-width: 1180px; margin: 24px auto; padding: 0 12px; }
    .card {
      background: linear-gradient(180deg, #0f172a 0%, #0b1220 100%);
      border: 1px solid var(--border);
      border-radius: 12px;
      padding: 16px;
    }
    .nav { margin-bottom: 10px; }
    a { color: #7dd3fc; text-decoration: none; }
    h1 { margin: 0 0 8px; font-size: 22px; }
    .status { color: var(--muted); margin: 6px 0 10px; font-size: 12px; }
    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
      gap: 10px;
      margin-top: 8px;
    }
    .tile {
      display: block;
      padding: 12px;
      border: 1px solid var(--border);
      border-radius: 10px;
      background: var(--tile);
      color: var(--text);
    }
    .tile-head {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 10px;
    }
    .tile-link {
      color: var(--text);
      text-decoration: none;
      min-width: 0;
      flex: 1 1 auto;
    }
    .tile-link strong { display: block; }
    .inline-toggle { margin: 0; flex: 0 0 auto; }
    .tile .meta { color: var(--muted); font-size: 12px; margin-top: 4px; display: block; }
    .crumbs { color: var(--muted); font-size: 13px; margin-bottom: 8px; }
    .row { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin: 8px 0; }
    .row.single { grid-template-columns: 1fr; }
    label { color: var(--muted); font-size: 12px; display: block; margin-bottom: 4px; }
    input[type=text], input[type=number], select {
      width: 100%;
      background: #0b1220;
      color: var(--text);
      border: 1px solid var(--border);
      border-radius: 8px;
      padding: 9px;
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
      position: relative;
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
    .switch input:checked + .track { background: #22c55e; }
    .switch input:checked + .track .knob { transform: translateX(20px); }
    button {
      border: none;
      border-radius: 8px;
      background: var(--accent);
      color: #00111a;
      font-weight: 700;
      padding: 10px 14px;
      cursor: pointer;
    }
    @media (max-width: 760px) {
      .row { grid-template-columns: 1fr; }
      .grid { grid-template-columns: 1fr; }
    }
  </style>
</head>
<body>
  <div class="wrap">
    <div class="card">
      %NAV%
      <h1>Правила</h1>
      <p class="status">%RULES_STATUS%</p>
      %RULES_BODY%
    </div>
  </div>
</body>
</html>
)HTML";
