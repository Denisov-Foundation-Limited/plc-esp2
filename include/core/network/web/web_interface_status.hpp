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

static const char kWebInterfaceStatusHtml[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Статус</title>
  <style>
    body { font-family: "Segoe UI", Tahoma, Arial, sans-serif; background:#0b1220; color:#e5e7eb; }
    .wrap { max-width: 560px; margin: 40px auto; padding: 0 16px; }
    .card { background:#0f172a; border:1px solid #1f2937; border-radius:12px; padding:20px; }
    a { color:#7dd3fc; text-decoration:none; }
  </style>
</head>
<body>
  <div class="wrap">
    <div class="card">
      <h2>Статус</h2>
      <p>Плата: %BOARD_NAME%</p>
      <pre>%STATUS%</pre>
      <p><a href="/">Назад</a></p>
    </div>
  </div>
</body>
</html>
)HTML";
