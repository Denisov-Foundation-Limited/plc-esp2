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

static const char kWebInterfaceStackHtml[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Стек</title>
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
    button.mini {
      padding: 8px 12px;
      font-size: 12px;
    }
    .status { color: var(--muted); font-size: 12px; }
    a { color: #7dd3fc; text-decoration: none; }
    .nav { margin-bottom: 12px; }
    table { width: 100%; border-collapse: collapse; margin-top: 8px; }
    th, td { text-align: left; padding: 6px; border-bottom: 1px solid #1f2937; }
    th { color: var(--muted); font-weight: 600; }
  </style>
</head>
<body>
  <div class="wrap">
    <div class="card">
      %NAV%
      <h1>Стек</h1>
      <div class="section">
        <form method="POST" action="/stack" id="stack-form">
          <div class="grid">
            <div>
              <label>Роль</label>
              <select name="role">
                <option value="master" %STACK_ROLE_MASTER_SEL%>master</option>
                <option value="slave" %STACK_ROLE_SLAVE_SEL%>slave</option>
              </select>
            </div>
            <div id="master-host-field">
              <label>Master host/IP</label>
              <input type="text" name="master_host" value="%STACK_MASTER_HOST%" placeholder="192.168.1.10">
            </div>
            <div>
              <label>API key</label>
              <div class="row">
                <input type="text" name="api_key" value="%STACK_API_KEY%" placeholder="optional">
                <button class="mini" type="button" id="gen-api-key">Сгенерировать</button>
              </div>
            </div>
          </div>
          <div class="row" style="margin-top:10px;">
            <button type="submit">Сохранить</button>
            <span class="status">%STACK_STATUS%</span>
          </div>
        </form>
      </div>
      %STACK_NODES_BLOCK%
    </div>
  </div>
  <script>
    const roleSelect = document.querySelector('select[name="role"]');
    const masterHost = document.getElementById('master-host-field');
    const apiKeyInput = document.querySelector('input[name="api_key"]');
    const apiKeyBtn = document.getElementById('gen-api-key');
    function updateMasterHost() {
      if (!roleSelect || !masterHost) return;
      masterHost.style.display = roleSelect.value === 'slave' ? '' : 'none';
      if (apiKeyBtn) apiKeyBtn.style.display = roleSelect.value === 'master' ? '' : 'none';
    }
    if (roleSelect) {
      roleSelect.addEventListener('change', updateMasterHost);
      updateMasterHost();
    }
    if (apiKeyBtn && apiKeyInput) {
      apiKeyBtn.addEventListener('click', () => {
        fetch('/stack/gen_key', { method: 'POST' })
          .then((resp) => resp.ok ? resp.text() : Promise.reject(resp.status))
          .then((key) => { apiKeyInput.value = key.trim(); })
          .catch(() => {});
      });
    }
    const stackForm = document.getElementById('stack-form');
    const reloadKey = 'stack_reload';
    if (sessionStorage.getItem(reloadKey)) {
      sessionStorage.removeItem(reloadKey);
      location.replace(location.pathname);
    }
    if (stackForm) {
      stackForm.addEventListener('submit', () => {
        sessionStorage.setItem(reloadKey, '1');
      });
    }
  </script>
</body>
</html>
)HTML";
