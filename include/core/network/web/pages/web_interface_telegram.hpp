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

static const char kWebInterfaceTelegramHtml[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Telegram</title>
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
    input[type=text], input[type=password], input[type=number], select {
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
    .id-highlight { color: var(--accent); font-weight: 700; }
    a { color: #7dd3fc; text-decoration: none; }
    .nav { margin-bottom: 12px; }
    .checkbox { display: flex; gap: 8px; align-items: center; }
    .table { width: 100%; border-collapse: collapse; margin-top: 8px; }
    .table th, .table td { border-bottom: 1px solid #1f2937; padding: 8px; text-align: left; font-size: 12px; }
    .table th { color: var(--muted); font-weight: 600; }
    .mini { width: 100%; }
  </style>
</head>
<body>
  <div class="wrap">
    <div class="card">
      %NAV%
      <h1>Telegram</h1>
      <p class="status">Клиент: <strong>%TGBOT_CLIENT%</strong></p>
      <form method="POST" action="/telegram" id="telegram-form">
        <div class="section">
          <h2>Доступ</h2>
          <div class="grid">
            <div>
              <label>Token</label>
              <input type="text" name="token" value="%TGBOT_TOKEN%" placeholder="Bot token">
            </div>
            <div>
              <label>Chat ID</label>
              <input type="text" name="chat_id" list="chat-id-list" value="%TGBOT_CHAT_ID%" placeholder="123456789">
              <datalist id="chat-id-list">
                <option value="%TGBOT_LAST_CHAT_ID%"></option>
              </datalist>
            </div>
            <div class="checkbox" style="margin-top:22px;">
              <input type="checkbox" id="insecure" name="insecure" %TGBOT_INSECURE_CHECKED%>
              <label for="insecure">Insecure TLS</label>
            </div>
          </div>
          <div class="row" style="margin-top:8px;">
            <span class="status">Последний Chat ID: <span id="last-chat-id" class="id-highlight">%TGBOT_LAST_CHAT_ID%</span></span>
          </div>
        </div>
        <div class="section">
          <h2>Proxy</h2>
          <div class="grid">
            <div class="checkbox" style="margin-top:22px;">
              <input type="checkbox" id="use_proxy" name="use_proxy" %TGBOT_USE_PROXY_CHECKED%>
              <label for="use_proxy">Использовать proxy</label>
            </div>
            <div class="proxy-field">
              <label>Proxy host</label>
              <input type="text" name="proxy_host" value="%TGBOT_PROXY_HOST%">
            </div>
            <div class="proxy-field">
              <label>Proxy port</label>
              <input type="number" name="proxy_port" value="%TGBOT_PROXY_PORT%">
            </div>
            <div class="proxy-field">
              <label>Proxy path</label>
              <input type="text" name="proxy_path" value="%TGBOT_PROXY_PATH%">
            </div>
          </div>
        </div>
        <div class="section">
          <h2>Разрешенные пользователи</h2>
          <table class="table">
            <thead>
              <tr>
                <th>ID</th>
                <th>Имя пользователя</th>
                <th>Chat ID</th>
                <th>Админ</th>
                <th>Уведомл.</th>
                <th>Включен</th>
              </tr>
            </thead>
            <tbody>
              %TGBOT_ALLOWED_USERS_ROWS%
            </tbody>
          </table>
        </div>
        <div class="row" style="margin-top:10px;">
          <button type="submit">Сохранить</button>
          <span class="status"><strong>%TGBOT_STATUS%</strong></span>
        </div>
      </form>
    </div>
  </div>
  <script>
    const useProxy = document.getElementById('use_proxy');
    const proxyFields = document.querySelectorAll('.proxy-field');
    function updateProxyFields() {
      const show = useProxy && useProxy.checked;
      proxyFields.forEach((el) => {
        el.style.display = show ? '' : 'none';
      });
    }
    if (useProxy) {
      useProxy.addEventListener('change', updateProxyFields);
      updateProxyFields();
    }
    const lastChatIdEl = document.getElementById('last-chat-id');
    if (lastChatIdEl) {
      const lastChatId = (lastChatIdEl.textContent || '').trim();
      const hasLast = lastChatId && lastChatId !== '0';
      if (!hasLast) {
        lastChatIdEl.textContent = 'неизвестно';
        const list = document.getElementById('chat-id-list');
        if (list) {
          list.innerHTML = '';
        }
      }
    }
    const telegramForm = document.getElementById('telegram-form');
    const reloadKey = 'telegram_reload';
    if (sessionStorage.getItem(reloadKey)) {
      sessionStorage.removeItem(reloadKey);
      location.replace(location.pathname);
    }
    if (telegramForm) {
      telegramForm.addEventListener('submit', () => {
        sessionStorage.setItem(reloadKey, '1');
      });
    }
  </script>
</body>
</html>
)HTML";
