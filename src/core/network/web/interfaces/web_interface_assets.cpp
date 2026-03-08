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

#include "core/network/web/interfaces/web_interface_assets.hpp"

const char kWebAutoRefreshScript[] PROGMEM = R"HTML(
<script>
(() => {
  const pollMs = 5000;
  const endpoint = '/ui/hash';
  let lastHash = '';
  const markDirty = () => { window.__plcDirty = true; };
  document.addEventListener('input', markDirty, true);
  document.addEventListener('change', markDirty, true);
  async function poll() {
    try {
      if (window.__plcDisableAutoRefresh) {
        return;
      }
      const path = location.pathname || '/';
      const res = await fetch(endpoint + '?path=' + encodeURIComponent(path), { cache: 'no-store' });
      if (!res.ok) {
        return;
      }
      const hash = (await res.text()).trim();
      if (!lastHash) {
        lastHash = hash;
        return;
      }
      if (hash && hash !== lastHash) {
        const active = document.activeElement;
        if (window.__plcDirty) {
          return;
        }
        if (active && (active.tagName === 'INPUT' || active.tagName === 'SELECT' || active.tagName === 'TEXTAREA')) {
          return;
        }
        location.reload();
      }
    } catch (e) {
    }
  }
  poll();
  setInterval(poll, pollMs);
})();
</script>
)HTML";
