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

#include "core/network/web/pages/web_page_cameras.hpp"

const char kWebInterfaceCamerasHtml[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Камеры</title>
  <style>
    :root {
      --bg: #0f172a;
      --card: #111827;
      --accent: #38bdf8;
      --text: #e5e7eb;
      --muted: #94a3b8;
      --line: #1f2937;
      --ok: #22c55e;
      --warn: #f59e0b;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      min-height: 100vh;
      font-family: "Segoe UI", Tahoma, Arial, sans-serif;
      background: radial-gradient(1200px 600px at 10% -10%, #1f2937 0%, #0b1220 60%, #080d17 100%);
      color: var(--text);
    }
    .wrap { max-width: 1320px; margin: 40px auto; padding: 0 16px; }
    .card {
      background: linear-gradient(180deg, #0f172a 0%, #0b1220 100%);
      border: 1px solid var(--line);
      border-radius: 18px;
      padding: 22px;
      box-shadow: 0 10px 30px rgba(0,0,0,0.35);
    }
    h1 { margin: 0 0 6px; font-size: 22px; }
    .status { margin: 10px 0 18px; color: var(--muted); }
    .grid {
      display: grid;
      grid-template-columns: minmax(0, 1fr);
      gap: 18px;
      max-width: 1120px;
      margin: 0 auto;
    }
    .tile {
      display: grid;
      grid-template-columns: minmax(520px, 700px) minmax(0, 1fr);
      gap: 18px;
      padding: 18px;
      border-radius: 16px;
      border: 1px solid var(--line);
      background: #0b1220;
      align-items: stretch;
    }
    .tile.disabled { opacity: .72; }
    .preview {
      position: relative;
      min-height: 240px;
      border-radius: 18px;
      overflow: hidden;
      border: 1px solid var(--line);
      background:
        linear-gradient(135deg, rgba(56,189,248,.16), rgba(15,23,42,.12)),
        radial-gradient(circle at top left, rgba(125,211,252,.12), transparent 45%),
        #0a1220;
    }
    .preview img {
      position: relative;
      z-index: 1;
      display: block;
      width: 100%;
      height: 100%;
      object-fit: cover;
      min-height: 240px;
    }
    .preview::after {
      content: "";
      position: absolute;
      inset: 0;
      background: linear-gradient(180deg, rgba(2,6,23,.02) 0%, rgba(2,6,23,.3) 100%);
      pointer-events: none;
    }
    .preview-badge {
      position: absolute;
      left: 14px;
      top: 14px;
      z-index: 2;
      display: inline-flex;
      align-items: center;
      gap: 8px;
      padding: 6px 10px;
      border-radius: 999px;
      border: 1px solid rgba(148,163,184,.22);
      background: rgba(15,23,42,.78);
      color: var(--text);
      font-size: 12px;
    }
    .dot {
      width: 10px;
      height: 10px;
      border-radius: 999px;
      background: var(--warn);
      box-shadow: 0 0 0 2px rgba(15,23,42,.6);
    }
    .dot.ok { background: var(--ok); }
    .dot.off { background: #64748b; }
    .preview-empty {
      display: none;
    }
    .form-head {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 12px;
      margin-bottom: 12px;
    }
    .title {
      font-size: 18px;
      font-weight: 700;
    }
    .switch {
      display: inline-block;
      width: 46px;
      height: 24px;
      flex: 0 0 auto;
    }
    .switch input { display: none; }
    .track {
      display: flex;
      align-items: center;
      width: 100%;
      height: 100%;
      padding: 2px;
      background: #475569;
      border-radius: 999px;
      border: 1px solid var(--line);
    }
    .knob {
      width: 18px;
      height: 18px;
      border-radius: 50%;
      background: #0b1220;
      transition: .2s;
    }
    input:checked + .track { background: #22c55e; }
    input:checked + .track .knob { transform: translateX(22px); }
    .form-grid {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 12px 16px;
    }
    .form-row {
      display: flex;
      flex-direction: column;
      gap: 6px;
    }
    .form-row.full { grid-column: 1 / -1; }
    .form-row label {
      color: var(--muted);
      font-size: 12px;
    }
    .field {
      width: 100%;
      padding: 10px 12px;
      border-radius: 12px;
      border: 1px solid var(--line);
      background: #0a1220;
      color: var(--text);
      font-size: 15px;
    }
    .actions {
      display: flex;
      gap: 10px;
      margin-top: 16px;
      flex-wrap: wrap;
    }
    .btn {
      border: none;
      padding: 10px 16px;
      border-radius: 12px;
      font-weight: 700;
      cursor: pointer;
      background: #38bdf8;
      color: #04131d;
    }
    .btn.secondary {
      background: #172033;
      color: var(--text);
      border: 1px solid var(--line);
    }
    .tile-status {
      margin-top: 10px;
      color: var(--muted);
      font-size: 13px;
      min-height: 18px;
    }
    @media (max-width: 960px) {
      .tile { grid-template-columns: 1fr; }
      .preview, .preview img { min-height: 260px; }
    }
    @media (max-width: 640px) {
      .wrap { margin: 18px auto; }
      .card { padding: 16px; }
      .form-grid { grid-template-columns: 1fr; }
    }
  </style>
</head>
<body>
  <div class="wrap">
    <div class="card">
      %NAV%
      <h1>Камеры</h1>
      <div class="status" id="camera-page-status">%CAMERA_STATUS%</div>
      <div class="grid">
        %CAMERA_TILES%
      </div>
    </div>
  </div>
  <script>
    async function parseApiResponse(res) {
      const contentType = (res.headers.get('content-type') || '').toLowerCase();
      if (contentType.indexOf('application/json') >= 0) {
        return await res.json();
      }
      const text = await res.text();
      return { ok: res.ok, error: text || ('HTTP ' + res.status), status: text || ('HTTP ' + res.status) };
    }
    function setTileStatus(card, text) {
      const node = card.querySelector('.tile-status');
      if (node) node.textContent = text || '';
    }
    function refreshPreview(card, ver) {
      const img = card.querySelector('img[data-preview]');
      if (!img) return;
      const base = img.getAttribute('data-preview');
      img.src = base + '&v=' + encodeURIComponent(String(ver || Date.now()));
    }
    function setBusy(card, busy) {
      const btn = card.querySelector('.js-snapshot-btn');
      if (btn) {
        btn.disabled = !!busy;
        btn.textContent = busy ? 'Загрузка...' : 'Получить фото';
      }
    }
    async function pollTask(card, id) {
      try {
        const res = await fetch('/cameras/task?id=' + encodeURIComponent(id), { credentials: 'same-origin' });
        const data = await parseApiResponse(res);
        if (data.busy) {
          setBusy(card, true);
          setTileStatus(card, data.status || 'Получаем фото...');
          setTimeout(() => pollTask(card, id), 900);
          return;
        }
        setBusy(card, false);
        setTileStatus(card, data.status || (data.ok ? 'Фото обновлено' : 'Ошибка загрузки'));
        if (data.ok && data.ver) refreshPreview(card, data.ver);
      } catch (e) {
        setBusy(card, false);
        setTileStatus(card, 'Ошибка опроса состояния');
      }
    }
    document.querySelectorAll('.js-snapshot-btn').forEach((btn) => {
      btn.addEventListener('click', async () => {
        const card = btn.closest('.tile');
        const id = btn.getAttribute('data-id');
        try {
          setBusy(card, true);
          setTileStatus(card, 'Запускаем загрузку...');
          const res = await fetch('/cameras/snapshot?id=' + encodeURIComponent(id), {
            method: 'POST',
            credentials: 'same-origin'
          });
          const data = await parseApiResponse(res);
          if (!data.ok) {
            setBusy(card, false);
            setTileStatus(card, data.error || 'Не удалось запустить загрузку');
            return;
          }
          refreshPreview(card, data.ver || Date.now());
          pollTask(card, id);
        } catch (e) {
          setBusy(card, false);
          setTileStatus(card, 'Ошибка запроса');
        }
      });
    });
  </script>
</body>
</html>
)HTML";
