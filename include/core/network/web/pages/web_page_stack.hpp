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
  <title>%STACK_PAGE_TITLE%</title>
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
    .badge.ok { color: #22c55e; border-color: rgba(34,197,94,0.5); background: rgba(34,197,94,0.1); }
    .badge.bad { color: #ef4444; border-color: rgba(239,68,68,0.5); background: rgba(239,68,68,0.1); }
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
      <h1>%STACK_TITLE%</h1>
      <div class="row" id="slave-link-field" style="%STACK_SLAVE_STYLE%">
        <span class="badge %STACK_SLAVE_LINK_CLASS%">%STACK_SLAVE_LINK_TEXT%</span>
      </div>
      <div class="section">
        <form method="POST" action="/stack" id="stack-form">
          <div class="grid">
            <div>
              <label>%STACK_LABEL_ROLE%</label>
              <select name="role">
                <option value="master" %STACK_ROLE_MASTER_SEL%>master</option>
                <option value="slave" %STACK_ROLE_SLAVE_SEL%>slave</option>
              </select>
            </div>
            <div id="master-host-field" style="%STACK_SLAVE_STYLE%">
              <label>%STACK_LABEL_MASTER_HOST%</label>
              <input type="text" name="master_host" value="%STACK_MASTER_HOST%" placeholder="192.168.1.10">
            </div>
            <div id="fallback-enabled-field" style="%STACK_SLAVE_STYLE%">
              <label>%STACK_LABEL_FALLBACK_MASTER%</label>
              <div class="row">
                <input type="checkbox" name="fallback_enabled" id="fallback-enabled" %STACK_FALLBACK_ENABLED_CHECKED%>
                <span class="status">%STACK_LABEL_ENABLE%</span>
              </div>
            </div>
            <div id="fallback-host-field" style="%STACK_SLAVE_STYLE%">
              <label>%STACK_LABEL_FALLBACK_HOST%</label>
              <input type="text" name="fallback_host" value="%STACK_FALLBACK_HOST%" placeholder="192.168.1.20">
            </div>
            <div id="slave-controller-field" style="%STACK_SLAVE_STYLE%">
              <label>%STACK_LABEL_SLAVE_CONTROLLER%</label>
              <div class="row">
                <input type="checkbox" name="slave_controller" id="slave-controller" %STACK_SLAVE_CONTROLLER_CHECKED%>
                <span class="status">%STACK_LABEL_FULL_CONTROLLER%</span>
              </div>
            </div>
            <div>
              <label>%STACK_LABEL_API_KEY%</label>
              <div class="row">
                <input type="text" name="api_key" value="%STACK_API_KEY%" placeholder="%STACK_API_KEY_PLACEHOLDER%">
                <button class="mini" type="button" id="gen-api-key">%STACK_BTN_GEN_KEY%</button>
              </div>
            </div>
          </div>
          <div class="row" style="margin-top:10px;">
            <button type="submit">%SAVE_TEXT%</button>
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
    const fallbackEnabled = document.getElementById('fallback-enabled-field');
    const fallbackEnabledToggle = document.getElementById('fallback-enabled');
    const fallbackHost = document.getElementById('fallback-host-field');
    const apiKeyInput = document.querySelector('input[name="api_key"]');
    const apiKeyBtn = document.getElementById('gen-api-key');
    function updateMasterHost() {
      if (!roleSelect || !masterHost) return;
      const isSlave = roleSelect.value === 'slave';
      const fallbackOn = !!(fallbackEnabledToggle && fallbackEnabledToggle.checked);
      masterHost.style.display = isSlave ? '' : 'none';
      if (fallbackEnabled) fallbackEnabled.style.display = isSlave ? '' : 'none';
      if (fallbackHost) fallbackHost.style.display = (isSlave && fallbackOn) ? '' : 'none';
      if (apiKeyBtn) apiKeyBtn.style.display = roleSelect.value === 'master' ? '' : 'none';
    }
    if (roleSelect) {
      roleSelect.addEventListener('change', updateMasterHost);
      updateMasterHost();
    }
    if (fallbackEnabledToggle) {
      fallbackEnabledToggle.addEventListener('change', updateMasterHost);
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
    const nodesTbody = document.getElementById('stack-nodes-tbody');
    async function pollStackNodes() {
      if (!nodesTbody) return;
      try {
        const resp = await fetch('/stack/nodes_tbody', { cache: 'no-store' });
        if (!resp.ok) return;
        const html = await resp.text();
        if (html !== nodesTbody.innerHTML) {
          nodesTbody.innerHTML = html;
        }
      } catch (e) {
      }
    }
    pollStackNodes();
    setInterval(pollStackNodes, 3000);
    const slaveLinkField = document.getElementById('slave-link-field');
    const slaveLinkBadge = slaveLinkField ? slaveLinkField.querySelector('.badge') : null;
    async function pollSlaveLink() {
      if (!slaveLinkField || !slaveLinkBadge) return;
      if (slaveLinkField.style.display === 'none') return;
      try {
        const resp = await fetch('/stack/slave_link', { cache: 'no-store' });
        if (!resp.ok) return;
        const data = await resp.json();
        const cls = (data && data.class) ? String(data.class) : 'bad';
        const text = (data && data.text) ? String(data.text) : '%STACK_SLAVE_LINK_DISCONNECTED%';
        slaveLinkBadge.classList.remove('ok', 'bad');
        slaveLinkBadge.classList.add(cls === 'ok' ? 'ok' : 'bad');
        slaveLinkBadge.textContent = text;
      } catch (e) {
      }
    }
    pollSlaveLink();
    setInterval(pollSlaveLink, 3000);
  </script>
</body>
</html>
)HTML";


