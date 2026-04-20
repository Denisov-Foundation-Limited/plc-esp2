# CLI.md

# ⌨️ CLI Reference

Актуальный CLI-справочник для текущей структуры `plc-esp2`.

Документ покрывает:

- режимы CLI
- команды `enable`
- команды `config`
- photo/camera сценарии
- stack/cloud настройки

Важно:

- это справочник именно по CLI, а не полный список всех возможностей проекта

Общий обзор проекта: [README.md](./README.md)

## 🔐 Режимы CLI

```text
login:
password:
plc#
plc(config)#
plc(config-<module>)#
```

### Что означают prompt'ы

- `login:` — ввод имени пользователя
- `password:` — ввод пароля
- `plc#` — enable mode
- `plc(config)#` — корень конфигурации
- `plc(config-wifi)#`, `plc(config-cloud)#`, ... — модульные контексты

Общие команды помощи:

- `help`
- `?`
- `help <topic>`

## 👤 Login

- введите логин
- затем пароль
- при ошибке сессия не открывается

## 🚀 Enable mode: `plc#`

### Сессия

- `disable`
- `logout`
- `exit`
- `configure terminal`
- `conf t`

### Системные команды

- `write` — сохранить конфиг
- `erase` — удалить конфиг
- `reload` — перезапуск
- `reset` — перезапуск
- `ext scan` — перескан расширителей
- `wifi restart` — перезапуск Wi-Fi
- `ftest` — функциональный тест

### Обновление прошивки

- `copy tftp://<ip>/firmware.bin firmware`
- `copy http://<ip>/firmware.bin firmware`

### `show ...`

- `show plc`
- `show board`
- `show wifi`
- `show time`
- `show i2c`
- `show ow`
- `show cloud`
- `show stack`
- `show config`
- `show ext`
- `show port <id>`
- `show ports`
- `show sockets`
- `show socket <id>`
- `show meteo`
- `show meteo <id>`
- `show thermo`
- `show thermo <id>`
- `show tanks`
- `show tank <id>`
- `show watering`
- `show watering <id>`
- `show septic`
- `show septic <id>`
- `show security`
- `show security <id>`
- `show ring`
- `show avr`
- `show leak`
- `show cameras`
- `show camera <id>`
- `show groups`
- `show group <id>`
- `show display`
- `show users`
- `show user <id>`

### Управление контроллерами

- `socket toggle <id>`
- `socket on <id>`
- `socket off <id>`
- `security status`
- `security arm`
- `security disarm`
- `ring on`
- `ring off`
- `avr on`
- `avr off`
- `avr source <off|main|reserve>`
- `avr clear_fault`

## 📷 Фото / камера

Эти команды работают через `hal::Camera` и буфер JPEG в `PSRAM`.

- `photo get <url>` — скачать JPEG по ссылке
- `photo upload <url>` — загрузить текущий JPEG на произвольный endpoint
- `photo cloud` — загрузить текущий JPEG в `plc-cloud` по текущему cloud config
- `photo status` — состояние фоновой camera task и буфера
- `photo clear` — освободить текущий буфер

### Типовой сценарий

```text
photo get http://192.168.1.55/cgi-bin/snapshot.cgi
photo status
photo cloud
```

## ⚙️ Config root: `plc(config)#`

### Общие команды

- `exit` — назад в `plc#`
- `end` — назад в `plc#`

### Admin / EEPROM

- `password <pass>`
- `admin password <pass>`
- `eeprom show`
- `eeprom save <on|off>`
- `eeprom load <on|off>`

### Переход в контексты

- `wifi`
- `time`
- `cloud`
- `camera`
- `groups`
- `display`
- `user`
- `socket`
- `meteo`
- `thermo`
- `tank`
- `septic`
- `security`
- `ring`
- `avr`
- `leak`
- `watering`

## 🌐 Stack config

Команды доступны из `plc(config)#`.

- `stack role <master|slave>`
- `stack master <host>`
- `stack policy <direct|poll>`
- `stack transport <websocket|rs485>`
- `stack payload <json|binary>`
- `stack fallback <on|off>`
- `stack fallback_host <host>`
- `stack slave_controller <on|off>`
- `stack api_key <value>`
- `stack api_key clear`
- `stack api_key gen`

### Что это значит

- `role` — роль узла
- `master` — адрес мастера для slave-режима
- `policy` — политика exchange в `StackRouteAdapter`
- `transport` — `websocket` или `rs485`
- `payload` — `json/binary`
- `fallback` — разрешить takeover/fallback mode
- `fallback_host` — переключаться на другой мастер вместо локального takeover
- `slave_controller` — публиковать узел как контроллер, а не просто модуль
- `api_key` — ключ stack auth

Практические замечания:

- для `rs485` payload всё равно будет принудительно `binary`
- `show stack` показывает реальное runtime-состояние, а не только сохранённый конфиг

## 📡 `show stack`

`show stack` выводит:

- `role`
- `master_host`
- `policy`
- `transport`
- `payload`
- `fallback`
- `fallback_host`
- `controller`
- `api_key`

И runtime-диагностику:

- `state`
- `master_active`
- `fallback_active`
- `online`
- `net_lock_ms`
- `rt_lock_ms`
- `xchg_slave_q`
- `xchg_master_q`
- `notify_q`
- `retried`
- `expired`
- `dropped`
- `rs485_state`
- `rs485_tx_q`
- `rs485_pending`
- `rs485_timeouts`
- `rs485_tx_drop`
- `rs485_pend_drop`
- `rs485_lock_ms`

## 📶 Wi-Fi: `plc(config-wifi)#`

- `mode <sta|ap|sta_ap>`
- `ssid <value>`
- `password <value>`
- `ap on|off`
- `ap_ssid <value>`
- `ap_password <value>`
- `restart`
- `show`

## ☁️ Cloud: `plc(config-cloud)#`

- `enable on|off`
- `transport ws|http`
- `host <value>`
- `port <num>`
- `path <value>`
- `ssl on|off`
- `reconnect <ms>`
- `event <ms>`
- `api_key <value>`
- `api_key clear`
- `show`

## 📷 Camera: `plc(config-camera)#`

- `show`
- `show <id>`
- `enable <id>`
- `disable <id>`
- `name <id> <text>`
- `url <id> <value>`
- `user <id> <value|clear>`
- `password <id> <value|clear>`

Эти команды редактируют локальный `CameraStore` (`/cameras.json`) напрямую.

## 🧩 Groups: `plc(config-groups)#`

- `show`
- `show <id>`
- `add <name>`
- `name <id> <text>`
- `sort <id> <num>`
- `delete <id>`

## 🖥️ Display: `plc(config-display)#`

- `show`
- `show <slot>`
- `clear <slot>`
- `text <slot> <text>`
- `set <slot> <kind> <field> [index] [node]`

Допустимые `kind`:

- `none`
- `time`
- `socket`
- `light`
- `meteo`
- `thermo`
- `tank`
- `septic`
- `security`
- `avr`
- `leak`
- `text`

Допустимые `field`:

- `none`
- `hm`
- `min`
- `state`
- `temp`
- `hum`
- `level`
- `armed`
- `avr_source`
- `avr_main_ok`
- `avr_reserve_ok`
- `leak_state`
- `text`

## 👤 User / ACL: `plc(config-user)#`

- `show`
- `show <id>`
- `enable <id>`
- `disable <id>`
- `username <id> <text>`
- `password <id> <text|clear>`
- `phone <id> <num|none>`
- `sms <id> <on|off>`
- `call <id> <on|off>`
- `ibutton <id> <hex|clear>`
- `rfid <id> <hex|clear>`
- `acl show <id> [unit]`
- `acl controller <id> <unit> <ctrl> <on|off>`
- `acl view <id> <unit> <ctrl> <item> <on|off>`
- `acl control <id> <unit> <ctrl> <item> <on|off>`
- `acl clear <id> <unit>`
- `acl grant <id> <unit>`

`unit` задаётся так же, как в локальном web ACL:

- `1` — локальный PLC
- `2..8` — stack-юниты

Допустимые `ctrl`:

- `sockets`
- `lights`
- `meteo`
- `thermo`
- `tanks`
- `septic`
- `security`
- `watering`
- `leak`
- `avr`
- `ring`

## 🕒 Time: `plc(config-time)#`

- `date <YYYY-MM-DD>`
- `time <HH:MM:SS>`
- `set <YYYY-MM-DD> <HH:MM:SS>`
- `show`

## 🔌 Socket: `plc(config-socket)#`

- `show`
- `show <id>`
- `enable <id>`
- `disable <id>`
- `name <id> <text>`
- `button <id> <port|none>`
- `relay <id> <port|none>`

## 🌡️ Meteo: `plc(config-meteo)#`

- `show`
- `show <id>`
- `name <id> <text>`
- `enable <id>`
- `disable <id>`
- `type <id> <none|ds18b20|dht22>`
- `addr <id> <hex|none>`
- `pin <id> <pin|none>`

## 🌡️ Thermo: `plc(config-thermo)#`

- `show`
- `show <id>`
- `name <id> <text>`
- `enable <id>`
- `disable <id>`
- `mode <id> <off|heat|cool|auto>`
- `sensor <id> <sensor|none>`
- `target <id> <temp>`
- `hyst <id> <temp>`
- `heat <id> <port|none>`
- `cool <id> <port|none>`
- `button <id> <port|none>`

## 🛢️ Tank: `plc(config-tank)#`

- `show`
- `show <id>`
- `enable <id>`
- `disable <id>`
- `name <id> <text>`
- `power <id> <0|1>`
- `low <id> <port|none>`
- `mid <id> <port|none>`
- `full <id> <port|none>`
- `valve <id> <port|none>`
- `pump <id> <port|none>`
- `alarm <id> <port|none>`

## 🚽 Septic: `plc(config-septic)#`

- `show`
- `show <id>`
- `enable <id>`
- `disable <id>`
- `name <id> <text>`
- `warn <id> <port|none>`
- `alarm <id> <port|none>`
- `relay_warn <id> <port|none>`
- `relay_alarm <id> <port|none>`

## 🛡️ Security: `plc(config-security)#`

- `show`
- `show <id>`
- `enable <id>`
- `disable <id>`
- `type <id> <pir|reed>`
- `port <id> <port|none>`
- `name <id> <text>`
- `silent <id> <on|off>`
- `siren <port|none>`

## 🔔 Ring: `plc(config-ring)#`

- `show`
- `enable|disable`
- `button <port|none>`
- `relay <port|none>`
- `duration <ms>`

## ⚡ AVR: `plc(config-avr)#`

- `show`
- `enable|disable`
- `mode <auto|manual>`
- `source <off|main|reserve>`
- `prefer_main <on|off>`
- `auto_return <on|off>`
- `main_ok <port|none>`
- `reserve_ok <port|none>`
- `relay_main <port|none>`
- `relay_reserve <port|none>`
- `fb_main <port|none>`
- `fb_reserve <port|none>`
- `debounce <ms>`
- `loss_delay <ms>`
- `return_delay <ms>`
- `break <ms>`
- `warmup <ms>`
- `timeout <ms>`
- `main_ok_al <on|off>`
- `reserve_ok_al <on|off>`
- `fb_main_al <on|off>`
- `fb_reserve_al <on|off>`
- `relay_main_inv <on|off>`
- `relay_reserve_inv <on|off>`
- `clear_fault`

## 💧 Leak: `plc(config-leak)#`

- `show`
- `show <id>`
- `enable|disable`
- `zone enable <id>`
- `zone disable <id>`
- `power <id> <on|off>`
- `sensor <id> <port|none>`
- `valve <id> <port|none>`
- `alarm <id> <port|none>`
- `active_low <id> <on|off>`
- `open_on_power <id> <on|off>`
- `name <id> <text>`
- `ack <id|all>`

## 🌿 Watering: `plc(config-watering)#`

- `show`
- `show <id>`
- `name <id> <text>`
- `enable <id>`
- `disable <id>`
- `status <id> <on|off>`
- `port <id> <port|none>`
- `tank <id> <tank_id|none>`
- `days <id> <mon,tue,wed,thu,fri,sat,sun|all|none>`
- `time <id> <HH:MM>`
- `time2 <id> <HH:MM>`
- `time3 <id> <HH:MM>`
- `slot1 <id> <on|off>`
- `slot2 <id> <on|off>`
- `slot3 <id> <on|off>`
- `duration <id> <min>`
- `duration2 <id> <min>`
- `duration3 <id> <min>`
- `resume <id> <on|off>`
- `resume_level <id> <low|mid|full>`
- `force <id> <on|off>`

CLI для полива сейчас покрывает полный рабочий конфиг правила:

- имя, enable, status
- GPIO-порт
- дни недели
- 3 времени, 3 длительности и `slot_enabled`
- привязку бака
- возобновление после refill и порог возобновления
- ручной `force`-режим полива

## 📝 Практические замечания

- если забыли синтаксис, не угадывайте: `help` внутри нужного режима уже показывает актуальный набор команд
- `photo get` ожидает прямой `http://` или `https://` JPEG endpoint
- `photo cloud` использует текущий cloud config и API key
- `show stack` сейчас важнее старых “trace/nodes/send” команд, которые относились к предыдущей модели stack
- если документация и `help` расходятся, источником правды считать `help` и текущий код
