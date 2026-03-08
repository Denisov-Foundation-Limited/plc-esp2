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
#include "core/network/web/pages/web_page_groups.hpp"

const char kWebInterfaceGroupsHtml[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>%GROUPS_PAGE_TITLE%</title>
  <style>
    :root { --bg:#0f172a; --card:#111827; --accent:#38bdf8; --text:#e5e7eb; --muted:#94a3b8; --danger:#ef4444; }
    * { box-sizing:border-box; }
    body { margin:0; min-height:100vh; font-family:"Segoe UI",Tahoma,Arial,sans-serif; background:radial-gradient(1200px 600px at 10% -10%, #1f2937 0%, #0b1220 60%, #080d17 100%); color:var(--text); }
    .wrap { max-width:900px; margin:40px auto; padding:0 16px; }
    .card { background:linear-gradient(180deg, #0f172a 0%, #0b1220 100%); border:1px solid #1f2937; border-radius:14px; padding:22px; box-shadow:0 10px 30px rgba(0,0,0,.35); }
    .nav { margin-bottom:12px; }
    a { color:#7dd3fc; text-decoration:none; }
    h1 { margin:0 0 8px; font-size:22px; }
    h2 { margin:0 0 12px; font-size:18px; }
    p { margin:0 0 18px; color:var(--muted); }
    .section { margin-top:20px; }
    .row { display:flex; gap:10px; align-items:center; flex-wrap:wrap; }
    .field, select.mini {
      min-height:42px;
      padding:8px 12px;
      border-radius:10px;
      border:1px solid #334155;
      background:#0b1220;
      color:var(--text);
    }
    .field { width:100%; }
    select.field, select.mini {
      appearance:none;
      -webkit-appearance:none;
      -moz-appearance:none;
      background-image:url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='18' height='18' viewBox='0 0 20 20' fill='none'%3E%3Cpath d='M5 7.5L10 12.5L15 7.5' stroke='%23e5e7eb' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'/%3E%3C/svg%3E");
      background-repeat:no-repeat;
      background-position:right 12px center;
      padding-right:38px;
    }
    .mini { width:120px; }
    select.mini { width:190px; max-width:100%; }
    .btn { border:none; padding:10px 16px; border-radius:8px; background:var(--accent); color:#08111b; font-weight:700; cursor:pointer; }
    table { width:100%; border-collapse:collapse; margin-top:10px; }
    th, td { text-align:left; padding:8px 6px; border-bottom:1px solid #1f2937; vertical-align:middle; }
    th { color:var(--muted); font-weight:600; }
    .right { text-align:right; }
    .muted { color:var(--muted); }
  </style>
</head>
<body>
  <div class="wrap">
    <div class="card">
      %NAV%
      <h1>%GROUPS_TITLE%</h1>
      <p>%GROUPS_DESCRIPTION%</p>
      %GROUPS_DEVICE_SELECT%
      <div class="section">
        <form method="POST" action="/groups">
          <input type="hidden" name="action" value="save">
          %GROUPS_SAVE_HIDDEN%
          <table>
            <thead>
              <tr>
                <th class="right">ID</th>
                <th>%GROUPS_NAME%</th>
                <th class="right">%GROUPS_SORT%</th>
                <th class="right">%GROUPS_DELETE%</th>
              </tr>
            </thead>
            <tbody>
              %GROUPS_ROWS%
            </tbody>
          </table>
          <div class="section">
            <button class="btn" type="submit">%GROUPS_SAVE%</button>
          </div>
        </form>
      </div>

      <div class="section">
        <h2>%GROUPS_NEW_GROUP%</h2>
        <form method="POST" action="/groups">
          <input type="hidden" name="action" value="add">
          %GROUPS_ADD_HIDDEN%
          <div class="row">
            <input class="field" type="text" name="name" placeholder="%GROUPS_NAME_PLACEHOLDER%">
            <input class="field mini" type="number" min="0" max="65535" name="sort" value="0">
            <button class="btn" type="submit">%GROUPS_ADD%</button>
          </div>
        </form>
      </div>
    </div>
  </div>
  %GROUPS_AUTO_REFRESH%
  <script>
    const deviceSelect = document.getElementById('index-device');
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
  </script>
</body>
</html>
)HTML";


