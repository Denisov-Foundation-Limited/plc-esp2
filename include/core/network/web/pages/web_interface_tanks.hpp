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

static const char kWebInterfaceTanksHtml[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Баки</title>
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
    .wrap { max-width: 1200px; margin: 40px auto; padding: 0 16px; }
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
    .field {
      width: 100%;
      padding: 6px 8px;
      border-radius: 8px;
      border: 1px solid #1f2937;
      background: #0b1220;
      color: var(--text);
    }
    .field.mini { padding: 4px 6px; }
    .field.name { min-width: 160px; }
    .actions { display: flex; gap: 10px; margin-top: 16px; }
    button {
      border: none;
      border-radius: 10px;
      padding: 10px 16px;
      background: var(--accent);
      color: #0b1220;
      font-weight: 700;
      cursor: pointer;
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
      background: #ef4444;
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
    }
    .switch input:checked + .track {
      background: #22c55e;
    }
    .switch input:checked + .track .knob {
      transform: translateX(18px);
    }
    .table-wrap { width: 100%; overflow-x: auto; -webkit-overflow-scrolling: touch; }
    @media (max-width: 900px) {
      .wrap { margin: 20px auto; }
    }
  </style>
</head>
<body>
  <div class="wrap">
    %NAV%
    <div class="card">
      <h1>Баки</h1>
      <p>%BOARD_NAME%</p>
      <div class="status">%TANK_STATUS%</div>
      <form method="POST" action="/tanks" id="tanks-form">
        <div class="table-wrap">
          <table>
            <thead>
              <tr>
                <th class="right">ID</th>
                <th>Вкл</th>
                <th>Имя</th>
                <th class="right">Низкий</th>
                <th class="right">Средний</th>
                <th class="right">Полный</th>
                <th class="right">Клапан</th>
                <th class="right">Насос</th>
                <th class="right">Сигнал</th>
                <th class="center">Уровень</th>
                <th class="center">Питание</th>
              </tr>
            </thead>
            <tbody>
              %TANK_ITEMS%
            </tbody>
          </table>
        </div>
        <div class="actions">
          <button type="submit">Сохранить</button>
        </div>
      </form>
    </div>
  </div>
  <script>
    const tankOptions = {
      dinput: %TANK_DINPUT_JSON%,
      relay: %TANK_RELAY_JSON%
    };
    const tankUsed = {
      dinput: %TANK_DINPUT_USED_JSON%,
      relay: %TANK_RELAY_USED_JSON%
    };
    function buildOptions(list, selected, type) {
      let html = '<option value="">-</option>';
      const used = tankUsed[type] || [];
      for (let i = 0; i < list.length; i++) {
        const val = String(list[i]);
        if (used.indexOf(parseInt(val, 10)) !== -1 && val !== selected) {
          continue;
        }
        html += '<option value="' + val + '"' + (val === selected ? ' selected' : '') + '>' + val + '</option>';
      }
      return html;
    }
    document.querySelectorAll('select.tank-select').forEach((el) => {
      const type = el.dataset.type;
      const selected = el.dataset.selected || '';
      const list = tankOptions[type] || [];
      el.innerHTML = buildOptions(list, selected, type);
    });
    const tanksForm = document.getElementById('tanks-form');
    let tanksDirty = false;
    if (tanksForm) {
      tanksForm.addEventListener('input', () => { tanksDirty = true; });
      tanksForm.addEventListener('change', () => { tanksDirty = true; });
    }
    const scrollKey = 'tanks_scroll_y';
    const savedScroll = sessionStorage.getItem(scrollKey);
    if (savedScroll) {
      const y = parseInt(savedScroll, 10);
      if (!Number.isNaN(y)) {
        window.scrollTo(0, y);
      }
    }
    window.addEventListener('scroll', () => {
      sessionStorage.setItem(scrollKey, String(window.scrollY));
    }, { passive: true });
    setInterval(() => {
      if (tanksDirty) {
        return;
      }
      const el = document.activeElement;
      if (el && (el.tagName === 'INPUT' || el.tagName === 'SELECT' || el.tagName === 'TEXTAREA')) {
        return;
      }
      location.reload();
    }, 3000);
  </script>
</body>
</html>
)HTML";
