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

static const char kWebInterfaceDisplayHtml[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Р”РёСЃРїР»РµР№</title>
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
    .wrap { max-width: 1100px; margin: 40px auto; padding: 0 16px; }
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
    .status { color: var(--muted); font-size: 12px; margin-bottom: 12px; }
    .grid {
      display: grid;
      grid-template-columns: repeat(4, minmax(0, 1fr));
      gap: 12px;
    }
    .slot {
      border: 1px solid #1f2937;
      background: #0b1220;
      border-radius: 12px;
      padding: 12px;
    }
    .slot-head {
      display: flex;
      align-items: center;
      justify-content: space-between;
      font-size: 12px;
      color: var(--muted);
      margin-bottom: 8px;
    }
    label { display: block; margin-bottom: 4px; font-size: 12px; color: var(--muted); }
    .field {
      width: 100%;
      padding: 6px 8px;
      border-radius: 8px;
      border: 1px solid #1f2937;
      background: #0b1220;
      color: var(--text);
    }
    .row { display: grid; grid-template-columns: repeat(4, minmax(0, 1fr)); gap: 12px; }
    .actions { margin-top: 16px; }
    .btn {
      border: none;
      padding: 10px 16px;
      border-radius: 10px;
      background: var(--accent);
      color: #0b1220;
      font-weight: 700;
      cursor: pointer;
    }
    @media (max-width: 920px) {
      .grid { grid-template-columns: repeat(2, minmax(0, 1fr)); }
    }
    @media (max-width: 640px) {
      .grid { grid-template-columns: 1fr; }
    }
  </style>
</head>
<body>
  <div class="wrap">
    <div class="card">
      %NAV%
      <h1>Р”РёСЃРїР»РµР№ (LCD1602)</h1>
      <div class="status">%DISPLAY_STATUS%</div>
      <pre id="display-preview" style="background:#0b1220;border:1px solid #1f2937;border-radius:10px;padding:10px;color:#e5e7eb;white-space:pre;font-family:Consolas,""Courier New"",monospace;margin:0 0 12px;">
                </pre>
      <form method="POST" action="/display" id="display-form">
        <div class="grid">
          %DISPLAY_SLOTS%
        </div>
        <div class="actions">
          <button class="btn" type="submit">%SAVE_TEXT%</button>
        </div>
      </form>
    </div>
  </div>
  <script>
    const sourceOptions = [
      { v: 'none', l: '-' },
      { v: 'time', l: 'Р’СЂРµРјСЏ' },
      { v: 'socket', l: 'Р РѕР·РµС‚РєР°' },
      { v: 'light', l: 'РЎРІРµС‚' },
      { v: 'meteo', l: 'РњРµС‚РµРѕ' },
      { v: 'thermo', l: 'РўРµСЂРјРѕ' },
      { v: 'tank', l: 'Р‘Р°Рє' },
      { v: 'septic', l: 'РЎРµРїС‚РёРє' },
      { v: 'security', l: 'РћС…СЂР°РЅР°' },
      { v: 'avr', l: 'РђР’Р ' },
      { v: 'leak', l: 'РџСЂРѕС‚РµС‡РєРё' },
      { v: 'text', l: 'РўРµРєСЃС‚' }
    ];
    const fieldOptions = {
      time: [{ v: 'hm', l: 'HH:' }, { v: 'min', l: 'MM' }],
      socket: [{ v: 'state', l: 'РЎРѕСЃС‚РѕСЏРЅРёРµ' }],
      light: [{ v: 'state', l: 'РЎРѕСЃС‚РѕСЏРЅРёРµ' }],
      meteo: [{ v: 'temp', l: 'РўРµРјРї' }, { v: 'hum', l: 'Р’Р»Р°Р¶РЅ' }],
      thermo: [{ v: 'state', l: 'РЎС‚Р°С‚СѓСЃ' }],
      tank: [{ v: 'level', l: 'РЈСЂРѕРІРµРЅСЊ' }],
      septic: [{ v: 'level', l: 'РЈСЂРѕРІРµРЅСЊ' }],
      security: [{ v: 'armed', l: 'ARM/DIS' }],
      avr: [{ v: 'avr_source', l: 'РСЃС‚РѕС‡РЅРёРє' }, { v: 'avr_main_ok', l: 'РћСЃРЅРѕРІРЅР°СЏ СЃРµС‚СЊ' }, { v: 'avr_reserve_ok', l: 'Р РµР·РµСЂРІРЅР°СЏ СЃРµС‚СЊ' }],
      leak: [{ v: 'leak_state', l: 'РЎРѕСЃС‚РѕСЏРЅРёРµ' }],
      text: [{ v: 'text', l: 'РўРµРєСЃС‚' }]
    };
    const deviceOptions = %DISPLAY_DEVICE_JSON%;
    const socketOptions = %DISPLAY_SOCKET_JSON%;
    const lightOptions = %DISPLAY_LIGHT_JSON%;
    const meteoOptions = %DISPLAY_METEO_JSON%;
    const thermoOptions = %DISPLAY_THERMO_JSON%;
    const tankOptions = %DISPLAY_TANK_JSON%;
    const septicOptions = %DISPLAY_SEPTIC_JSON%;
    const avrOptions = %DISPLAY_AVR_JSON%;
    const leakOptions = %DISPLAY_LEAK_JSON%;

    function buildOptions(list, selected) {
      let html = '<option value="">-</option>';
      for (let i = 0; i < list.length; i++) {
        const v = String(list[i].v);
        const l = list[i].l || v;
        html += '<option value="' + v + '"' + (v === selected ? ' selected' : '') + '>' + l + '</option>';
      }
      return html;
    }

    function buildFieldOptions(kind, selected) {
      const list = fieldOptions[kind] || [];
      let html = '<option value="">-</option>';
      for (let i = 0; i < list.length; i++) {
        const v = String(list[i].v);
        const l = list[i].l || v;
        html += '<option value="' + v + '"' + (v === selected ? ' selected' : '') + '>' + l + '</option>';
      }
      return html;
    }

    function buildSourceOptions(selected) {
      let html = '';
      for (let i = 0; i < sourceOptions.length; i++) {
        const v = String(sourceOptions[i].v);
        const l = sourceOptions[i].l || v;
        html += '<option value="' + v + '"' + (v === selected ? ' selected' : '') + '>' + l + '</option>';
      }
      return html;
    }

    function slotOptionsFor(kind, nodeId) {
      const key = String(nodeId || '0');
      if (kind === 'socket') return socketOptions[key] || socketOptions['0'] || [];
      if (kind === 'light') return lightOptions[key] || lightOptions['0'] || [];
      if (kind === 'meteo') return meteoOptions[key] || meteoOptions['0'] || [];
      if (kind === 'thermo') return thermoOptions[key] || thermoOptions['0'] || [];
      if (kind === 'tank') return tankOptions[key] || tankOptions['0'] || [];
      if (kind === 'septic') return septicOptions[key] || septicOptions['0'] || [];
      if (kind === 'avr') return avrOptions[key] || avrOptions['0'] || [];
      if (kind === 'leak') return leakOptions[key] || leakOptions['0'] || [];
      return [];
    }

    document.querySelectorAll('.display-slot').forEach((slot) => {
      const kind = slot.dataset.kind || 'none';
      const idx = slot.dataset.index || '';
      const field = slot.dataset.field || '';
      const node = slot.dataset.node || '0';
      const kindSelect = slot.querySelector('select.slot-kind');
      const nodeSelect = slot.querySelector('select.slot-node');
      const idxSelect = slot.querySelector('select.slot-index');
      const fieldSelect = slot.querySelector('select.slot-field');
      const textInput = slot.querySelector('input.slot-text');

      if (kindSelect) kindSelect.innerHTML = buildSourceOptions(kind);
      if (nodeSelect) nodeSelect.innerHTML = buildOptions(deviceOptions, node);
      if (idxSelect) idxSelect.innerHTML = buildOptions(slotOptionsFor(kind, node), idx);
      if (fieldSelect) fieldSelect.innerHTML = buildFieldOptions(kind, field);

      function updateVisibility() {
        const curr = kindSelect ? kindSelect.value : kind;
        const dev = nodeSelect ? nodeSelect.value : node;
        if (idxSelect) idxSelect.style.display = (curr === 'socket' || curr === 'light' || curr === 'meteo' || curr === 'thermo' || curr === 'tank' || curr === 'septic' || curr === 'avr' || curr === 'leak') ? '' : 'none';
        if (fieldSelect) fieldSelect.style.display = (curr === 'none') ? 'none' : '';
        if (textInput) textInput.style.display = (curr === 'text') ? '' : 'none';
        if (idxSelect) idxSelect.innerHTML = buildOptions(slotOptionsFor(curr, dev), idxSelect.value || '');
        if (fieldSelect) fieldSelect.innerHTML = buildFieldOptions(curr, fieldSelect.value || '');
      }

      if (kindSelect) {
        kindSelect.addEventListener('change', () => updateVisibility());
      }
      if (nodeSelect) {
        nodeSelect.addEventListener('change', () => updateVisibility());
      }
      updateVisibility();
    });
    const preview = document.getElementById('display-preview');
    function pad4(txt) {
      let t = String(txt || '');
      if (t.length > 4) t = t.slice(0, 4);
      while (t.length < 4) t += ' ';
      return t;
    }
    function sampleFor(kind, field, text) {
      if (kind === 'time') return field === 'min' ? '22 ' : '12:';
      if (kind === 'socket' || kind === 'light') return 'ON  ';
      if (kind === 'meteo') return field === 'hum' ? '45%' : '23C ';
      if (kind === 'thermo') return 'IDL ';
      if (kind === 'tank') return '66% ';
      if (kind === 'septic') return 'ALM ';
      if (kind === 'security') return 'ARM ';
      if (kind === 'avr') {
        if (field === 'avr_main_ok' || field === 'avr_reserve_ok') return 'ON  ';
        return 'MAN ';
      }
      if (kind === 'leak') return 'DRY ';
      if (kind === 'text') return pad4(text || '');
      return '    ';
    }
    function updatePreview() {
      if (!preview) return;
      const slots = Array.from(document.querySelectorAll('.display-slot')).map((slot) => {
        const kind = slot.querySelector('select.slot-kind')?.value || 'none';
        const field = slot.querySelector('select.slot-field')?.value || '';
        const text = slot.querySelector('input.slot-text')?.value || '';
        return { kind, field, text };
      });
      const line0 = Array(16).fill(' ');
      const line1 = Array(16).fill(' ');
      slots.forEach((slot, idx) => {
        const sample = pad4(sampleFor(slot.kind, slot.field, slot.text));
        const row = idx < 4 ? 0 : 1;
        let col = (idx % 4) * 4;
        const prev = (idx % 4) !== 0 ? slots[idx - 1] : null;
        if (slot.kind === 'time' && slot.field === 'min' && prev && prev.kind === 'time' && prev.field === 'hm') {
          col -= 1;
        }
        for (let k = 0; k < 4; k++) {
          const pos = col + k;
          if (pos < 0 || pos >= 16) continue;
          const ch = sample[k] || ' ';
          if (row === 0) line0[pos] = ch;
          else line1[pos] = ch;
        }
      });
      preview.textContent = line0.join('') + '\n' + line1.join('');
    }
    document.getElementById('display-form')?.addEventListener('input', updatePreview);
    updatePreview();  </script>
</body>
</html>
)HTML";




