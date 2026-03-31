# CLI Reference

Полный справочник по CLI проекта `plc-esp2`.

## Общая схема режимов

- `login:` -> ввод логина
- `password:` -> ввод пароля
- `plc#` -> enable-режим (диагностика и управление)
- `plc(config)#` -> корень конфигурации
- `plc(config-<module>)#` -> контекст конкретного модуля

Общие команды помощи:
- `help`
- `?`
- `help <topic>`

## Login

- `login:` -> имя пользователя
- `password:` -> пароль
- При ошибке: `Login invalid`

## Enable (`plc#`)

### Сессия и навигация

- `configure terminal`
- `conf t`
- `disable`
- `logout`
- `exit`

### Системные команды

- `write`
- `erase`
- `reload`
- `reset`
- `wifi restart`
- `ext scan`

### Обновление прошивки

- `copy tftp://<ip>/firmware.bin firmware`
- `copy http://<ip>/firmware.bin firmware`

### Просмотр состояния

- `show plc`
- `show board`
- `show wifi`
- `show time`
- `show i2c`
- `show ow`
- `show stack`
- `show telegram`
- `show cloud`
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
- `ftest`

### Stack-команды

- `stack nodes`
- `stack trace`
- `stack trace on`
- `stack trace off`
- `stack send <id> <get|set> <json>`
- `stack socket <unit> <on|off|toggle> <id>`
- `stack thermo <unit> <on|off|toggle> <id>`
- `stack septic <unit> <status|get>`
- `stack septic <unit> monitor <id> <on|off>`
- `stack security <unit> <arm|disarm|status|clear>`
- `stack ring <unit> <on|off>`

## Config Root (`plc(config)#`)

### Глобальные параметры

- `password <pass>`
- `admin password <pass>`
- `eeprom show`
- `eeprom save <on|off>`
- `eeprom load <on|off>`

### Stack-настройки

- `stack role <master|slave>`
- `stack master <host>`
- `stack policy <auto|direct|poll>`
- `stack transport <websocket|rs485>`
- `stack payload <auto|json|binary>`
- `stack fallback <on|off>`
- `stack fallback_host <host>`
- `stack slave_controller <on|off>`
- `stack api_key <value>`
- `stack api_key clear`
- `stack api_key gen`

### Переход в подконтексты

- `wifi`
- `tgbot`
- `cloud`
- `time`
- `socket`
- `meteo`
- `thermo`
- `tank`
- `watering`
- `septic`
- `security`
- `ring`
- `avr`
- `leak`

### Выход

- `exit`
- `end`

## Wi-Fi (`plc(config-wifi)#`)

- `ssid <value>`
- `password <value>`
- `ap on|off`
- `ap_ssid <value>`
- `ap_password <value>`
- `restart`
- `show`
- `exit`
- `end`
- `help`

## Telegram (`plc(config-tgbot)#`)

### Базовые параметры

- `token <value>`
- `chat <id>`
- `insecure on|off`
- `send <text>`
- `poll`
- `show`

### Пользователи

- `user list`
- `user enable <id> <on|off>`
- `user username <id> <value|clear>`
- `user tg_username <id> <value|clear>`
- `user tg_chat <id> <chat_id|0>`
- `user is_admin <id> <on|off>`
- `user tg_notify <id> <on|off>`
- `user tg_quick <id> <on|off>`
- `user webpass <id> <password|clear>`
- `user acl <id> <all|none>`

### Allowlist

- `allow list`
- `allow add <username> [chat_id] [admin] [notify] [off]`
- `allow del <username>`
- `allow clear`

### Медиа-команды (сборки с поддержкой файлов)

- `snapdoc <http(s)://url> [filename] [caption] [chat_id]`
- `snapphoto <http(s)://url> [filename] [caption] [chat_id]`

### Выход

- `exit`
- `end`
- `help`

## Cloud (`plc(config-cloud)#`)

- `enable on|off`
- `host <value>`
- `port <num>`
- `path <value>`
- `ssl on|off`
- `reconnect <ms>`
- `event <ms>`
- `api_key <value>`
- `api_key clear`
- `show`
- `exit`
- `end`
- `help`

## Time (`plc(config-time)#`)

- `date <YYYY-MM-DD>`
- `time <HH:MM:SS>`
- `set <YYYY-MM-DD> <HH:MM:SS>`
- `show`
- `exit`
- `end`
- `help`

## Socket (`plc(config-socket)#`)

- `show`
- `show <id>`
- `enable <id>`
- `disable <id>`
- `name <id> <value>`
- `button <id> <port|none>`
- `relay <id> <port|none>`
- `exit`
- `end`
- `help`

## Meteo (`plc(config-meteo)#`)

- `show`
- `show <id>`
- `enable <id>`
- `disable <id>`
- `name <id> <text>`
- `type <id> <none|ds18b20|dht22>`
- `addr <id> <hex|none>`
- `pin <id> <pin|none>`
- `exit`
- `end`
- `help`

## Thermo (`plc(config-thermo)#`)

- `show`
- `show <id>`
- `enable <id>`
- `disable <id>`
- `name <id> <text>`
- `mode <id> <off|heat|cool|auto>`
- `sensor <id> <sensor|none>`
- `target <id> <temp>`
- `hyst <id> <temp>`
- `heat <id> <port|none>`
- `cool <id> <port|none>`
- `button <id> <port|none>`
- `exit`
- `end`
- `help`

## Tank (`plc(config-tank)#`)

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
- `exit`
- `end`
- `help`

## Watering (`plc(config-watering)#`)

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
- `duration <id> <min>`
- `duration2 <id> <min>`
- `duration3 <id> <min>`
- `resume <id> <on|off>`
- `resume_level <id> <low|mid|full>`
- `exit`
- `end`
- `help`

## Septic (`plc(config-septic)#`)

- `show`
- `show <id>`
- `enable <id>`
- `disable <id>`
- `name <id> <text>`
- `warning <id> <port|none>`
- `alarm <id> <port|none>`
- `relay_warn <id> <port|none>`
- `relay_alarm <id> <port|none>`
- `monitor <id> <on|off>`
- `exit`
- `end`
- `help`

## Security (`plc(config-security)#`)

### Датчики и сирена

- `show`
- `show <id>`
- `enable <id>`
- `disable <id>`
- `type <id> <pir|reed>`
- `port <id> <port|none>`
- `name <id> <text>`
- `silent <id> <on|off>`
- `siren <port|none>`

### Ключи

- `keys list`
- `key add <hex16> [name]`
- `key name <hex16> <text>`
- `key del <hex16>`
- `key clear`

### Телефоны

- `phones list`
- `phone set <id> <num|none> [name]`
- `phone name <id> <text>`
- `phone enable <id> <on|off>`
- `phone notify <id> <on|off>`
- `phone call <id> <on|off>`
- `phone clear`

### Выход

- `exit`
- `end`
- `help`

## Ring (`plc(config-ring)#`)

- `show`
- `on`
- `off`
- `enable`
- `disable`
- `button <port|none>`
- `relay <port|none>`
- `exit`
- `end`
- `help`

## AVR (`plc(config-avr)#`)

- `show`
- `enable`
- `disable`
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
- `main_ok_al <on|off>`
- `reserve_ok_al <on|off>`
- `fb_main_al <on|off>`
- `fb_reserve_al <on|off>`
- `relay_main_inv <on|off>`
- `relay_reserve_inv <on|off>`
- `debounce <ms>`
- `loss_delay <ms>`
- `return_delay <ms>`
- `break <ms>`
- `warmup <ms>`
- `timeout <ms>`
- `clear_fault`
- `exit`
- `end`
- `help`

## Leak (`plc(config-leak)#`)

- `show`
- `show <id>`
- `enable`
- `disable`
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
- `exit`
- `end`
- `help`

## Примечания

- Некоторые команды доступны только при соответствующих compile-time флагах и поддержке подсистемы в текущей сборке.
- В разных профилях плат фактическое количество элементов (`<id>`) зависит от конфигурации.
