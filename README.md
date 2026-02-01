# plc-esp2
Programmable Logic Controller for ESP microcontrollers
<br>
<img src="https://raw.githubusercontent.com/Denisov-Foundation-Limited/plc-esp2/develop/img/tg1.png" width=300 />
<img src="https://raw.githubusercontent.com/Denisov-Foundation-Limited/plc-esp2/develop/img/tg2.png" width=300 />
<img src="https://raw.githubusercontent.com/Denisov-Foundation-Limited/plc-esp2/develop/img/tg3.png" width=300 />
<br>
<img src="https://raw.githubusercontent.com/Denisov-Foundation-Limited/plc-esp2/develop/img/web1.png" width=600 />
<img src="https://raw.githubusercontent.com/Denisov-Foundation-Limited/plc-esp2/develop/img/web2.png" width=600 />
<br>
<img src="https://raw.githubusercontent.com/Denisov-Foundation-Limited/plc-esp2/develop/img/web3.png" width=600 />
<img src="https://raw.githubusercontent.com/Denisov-Foundation-Limited/plc-esp2/develop/img/web4.png" width=600 />
<br>
<img src="https://raw.githubusercontent.com/Denisov-Foundation-Limited/plc-esp2/develop/img/web5.png" width=600 />
<img src="https://raw.githubusercontent.com/Denisov-Foundation-Limited/plc-esp2/develop/img/web6.png" width=600 />
<br>
<img src="https://raw.githubusercontent.com/Denisov-Foundation-Limited/plc-esp2/develop/img/web7.png" width=600 />
<img src="https://raw.githubusercontent.com/Denisov-Foundation-Limited/plc-esp2/develop/img/web8.png" width=600 />
<br>
<img src="https://raw.githubusercontent.com/Denisov-Foundation-Limited/plc-esp2/develop/img/web9.png" width=700 />
<br>
<img src="https://raw.githubusercontent.com/Denisov-Foundation-Limited/plc-esp2/develop/img/web10.png" width=700 />
<br>
<img src="https://raw.githubusercontent.com/Denisov-Foundation-Limited/plc-esp2/develop/img/web11.png" width=700 />
<br>
<img src="https://raw.githubusercontent.com/Denisov-Foundation-Limited/plc-esp2/develop/img/web12.png" width=700 />
<br>
<img src="https://raw.githubusercontent.com/Denisov-Foundation-Limited/plc-esp2/develop/img/web13.png" width=700 />
<br>
<img src="https://raw.githubusercontent.com/Denisov-Foundation-Limited/plc-esp2/develop/img/web14.png" width=700 />
<br>
<img src="https://raw.githubusercontent.com/Denisov-Foundation-Limited/plc-esp2/develop/img/web15.png" width=700 />
<br>
<img src="https://raw.githubusercontent.com/Denisov-Foundation-Limited/plc-esp2/develop/img/board2.png" width=700 />
<br>
<img src="https://raw.githubusercontent.com/Denisov-Foundation-Limited/plc-esp2/develop/img/ext.png" width=700 />
<br>
<img src="https://raw.githubusercontent.com/Denisov-Foundation-Limited/plc-esp2/develop/img/fan.png" width=700 />
<br>

```
[1329][INFO][TANK] controller: enabled
[1373][INFO][APP] Configs loaded: /startup-config.json (2492 bytes)
[1374][INFO][WIFI] Mode: STA (SSID=Denisov_VPN)
[1374][INFO][APP] Initializing HAL
[1374][INFO][HAL] I2C init
[1375][INFO][HAL] GPIO init
[1390][INFO][EXT] Extender 0 detected
[1394][INFO][HAL] SPI init
[1394][INFO][HAL] OneWire init
[1395][INFO][HAL] UART init
[1395][INFO][APP] Initializing EEPROM
[1395][INFO][APP] EEPROM used: 0 free: 65536 total: 65536
[1395][INFO][APP] Initializing RTC
[2000-01-02][23:06:37][INFO][APP] Initializing Display
[2000-01-02][23:06:37][INFO][APP] Initializing PLC Control
[2000-01-02][23:06:37][INFO][APP] Initializing Network
[2000-01-02][23:06:37][INFO][STACK] Role: master
[2000-01-02][23:06:37][INFO][CTRL] Sockets init
[2000-01-02][23:06:37][INFO][CTRL] Meteo init
[2000-01-02][23:06:37][INFO][CTRL] Thermo init
[2000-01-02][23:06:37][INFO][CTRL] Tanks init
[2000-01-02][23:06:37][INFO][APP] Application init [OK]
[2000-01-02][23:06:37][INFO][WIFI] STA DISCONNECTED
[2000-01-02][23:06:42][INFO][WIFI] STA CONNECTED
[2000-01-02][23:06:42][INFO][WIFI] STA IP 192.168.1.101
[2000-01-02][23:06:43][INFO][THERMO] id: 1 mode: auto temp: 26.88 heat: on cool: off

login: admin
password:
plc#
```

## CLI Commands

Подсказка: используйте `help` или `?` для списка команд, `help <topic>` для справки по теме.

### Enable (`plc#`)

- `show plc` - состояние вентилятора и температура платы
- `show board` - имя профиля платы
- `show wifi` - конфигурация Wi-Fi
- `show time` - дата/время RTC
- `show i2c` - список I2C устройств
- `show ow` - список OneWire устройств
- `show stack` - настройки stack роли
- `show telegram` - настройки Telegram
- `show config` - содержимое конфигурационного файла
- `show port <id>` - детали порта
- `show ports` - список портов
- `show sockets` - список розеток
- `show socket <id>` - детали розетки
- `show meteo` - список датчиков meteo
- `show meteo <id>` - детали датчика
- `show thermo` - список термоустройств
- `show thermo <id>` - детали устройства
- `show tanks` - список баков
- `show tank <id>` - детали бака
- `show septic` - список септика
- `show septic <id>` - детали септика
- `show security` - список датчиков охраны
- `show security <id>` - детали датчика
- `socket toggle <id>` - переключить реле розетки
- `socket on <id>` - включить реле
- `socket off <id>` - выключить реле
- `security status` - статус охраны
- `security arm` - постановка под охрану
- `security disarm` - снятие с охраны
- `ftest` - функциональный тест
- `copy tftp://<ip>/firmware.bin firmware` - обновление прошивки
- `copy http://<ip>/firmware.bin firmware` - обновление прошивки
- `stack nodes` - список узлов стека
- `stack send <id> <get|set> <json>` - отправить команду в стек
- `stack socket <unit> <on|off|toggle> <id>` - управление розетками стека
- `stack thermo <unit> <on|off|toggle> <id>` - управление термо в стеке
- `stack septic <unit> <status|get>` - статус/список септика в стеке
- `stack septic <unit> monitor <id> <on|off>` - мониторинг септика в стеке
- `stack security <unit> <arm|disarm|status|clear>` - управление охраной в стеке
- `wifi restart` - перезапуск Wi-Fi
- `reload` - перезапуск контроллера
- `reset` - перезапуск контроллера
- `write` - сохранить конфигурацию
- `erase` - удалить конфигурацию
- `ext scan` - пересканировать расширители
- `show ext` - список расширителей
- `configure terminal` - вход в режим config
- `conf t` - вход в режим config
- `disable` - завершить сессию
- `logout` - завершить сессию
- `exit` - завершить сессию
- `help` / `?` - список команд
- `help <topic>` - справка (show, wifi, user, system, tgbot, socket, meteo, thermo, tank, septic, security)

### Config (`plc(config)#`)

- `password <pass>` - установить пароль администратора
- `admin password <pass>` - установить пароль администратора
- `stack role <master|slave>` - роль устройства в стеке
- `stack master <host>` - адрес мастера стека
- `wifi` - вход в контекст Wi-Fi
- `tgbot` - вход в контекст Telegram
- `time` - вход в контекст времени
- `socket` - вход в контекст розеток
- `meteo` - вход в контекст meteo
- `thermo` - вход в контекст термо
- `tank` - вход в контекст баков
- `septic` - вход в контекст септика
- `security` - вход в контекст охраны
- `exit` - выход в enable
- `end` - выход в enable
- `help` / `?` - список команд
- `help <topic>` - справка по теме

### Wi-Fi (`plc(config-wifi)#`)

- `ssid <value>` - установить STA SSID
- `password <value>` - установить STA пароль
- `ap on|off` - включить/выключить AP
- `ap_ssid <value>` - установить AP SSID
- `ap_password <value>` - установить AP пароль
- `restart` - перезапуск Wi-Fi
- `show` - показать настройки Wi-Fi
- `exit` - выход в config
- `end` - выход в enable
- `help` / `?` - список команд
- `help <topic>` - справка по теме

### Telegram (`plc(config-tgbot)#`)

- `token <value>` - токен бота
- `chat <id>` - chat id
- `insecure on|off` - проверка TLS
- `allow list` - список разрешенных пользователей
- `allow add <username> [chat_id] [admin] [notify] [off]` - добавить пользователя
- `allow del <username>` - удалить пользователя
- `allow clear` - очистить список
- `send <text>` - отправить сообщение
- `poll` - опрос команд
- `show` - показать настройки
- `exit` - выход в config
- `end` - выход в enable
- `help` / `?` - список команд
- `help <topic>` - справка по теме

### Time (`plc(config-time)#`)

- `date <YYYY-MM-DD>` - установить дату
- `time <HH:MM:SS>` - установить время
- `set <YYYY-MM-DD> <HH:MM:SS>` - установить дату и время
- `show` - показать время RTC
- `exit` - выход в config
- `end` - выход в enable
- `help` / `?` - список команд
- `help <topic>` - справка по теме

### Socket (`plc(config-socket)#`)

- `show` - список розеток
- `show <id>` - детали розетки
- `enable <id>` - включить розетку
- `disable <id>` - выключить розетку
- `name <id> <value>` - имя розетки
- `button <id> <port|none>` - порт кнопки
- `relay <id> <port|none>` - порт реле
- `exit` - выход в config
- `end` - выход в enable
- `help` / `?` - список команд
- `help <topic>` - справка по теме

### Meteo (`plc(config-meteo)#`)

- `show` - список датчиков
- `show <id>` - детали датчика
- `name <id> <text>` - имя датчика
- `enable <id>` - включить датчик
- `disable <id>` - выключить датчик
- `type <id> <none|ds18b20|dht22>` - тип датчика
- `addr <id> <hex|none>` - адрес DS18B20
- `pin <id> <pin|none>` - пин DHT22
- `exit` - выход в config
- `end` - выход в enable
- `help` / `?` - список команд
- `help <topic>` - справка по теме

### Thermo (`plc(config-thermo)#`)

- `show` - список устройств
- `show <id>` - детали устройства
- `name <id> <text>` - имя устройства
- `enable <id>` - включить устройство
- `disable <id>` - выключить устройство
- `mode <id> <off|heat|cool|auto>` - режим
- `sensor <id> <sensor|none>` - датчик meteo
- `target <id> <temp>` - целевая температура
- `hyst <id> <temp>` - гистерезис
- `heat <id> <port|none>` - порт нагрева
- `cool <id> <port|none>` - порт охлаждения
- `button <id> <port|none>` - порт кнопки
- `exit` - выход в config
- `end` - выход в enable
- `help` / `?` - список команд
- `help <topic>` - справка по теме

### Tank (`plc(config-tank)#`)

- `show` - список баков
- `show <id>` - детали бака
- `name <id> <text>` - имя бака
- `enable <id>` - включить бак
- `disable <id>` - выключить бак
- `power <id> <0|1>` - питание
- `low <id> <port|none>` - нижний уровень
- `mid <id> <port|none>` - средний уровень
- `full <id> <port|none>` - полный уровень
- `valve <id> <port|none>` - реле клапана
- `pump <id> <port|none>` - реле насоса
- `alarm <id> <port|none>` - реле аварии
- `exit` - выход в config
- `end` - выход в enable
- `help` / `?` - список команд
- `help <topic>` - справка по теме

### Septic (`plc(config-septic)#`)

- `show` - список септиков
- `show <id>` - детали
- `name <id> <text>` - имя
- `enable <id>` - включить
- `disable <id>` - выключить
- `warning <id> <port|none>` - вход предупреждения
- `alarm <id> <port|none>` - вход тревоги
- `relay_warn <id> <port|none>` - реле предупреждения
- `relay_alarm <id> <port|none>` - реле тревоги
- `monitor <id> <on|off>` - мониторинг
- `exit` - выход в config
- `end` - выход в enable
- `help` / `?` - список команд
- `help <topic>` - справка по теме

### Security (`plc(config-security)#`)

- `show` - список датчиков
- `show <id>` - детали датчика
- `enable <id>` - включить датчик
- `disable <id>` - выключить датчик
- `type <id> <pir|reed>` - тип датчика
- `port <id> <port|none>` - порт датчика
- `name <id> <text>` - имя датчика
- `silent <id> <on|off>` - тихий режим
- `siren <port|none>` - порт сирены
- `keys list` - список ключей iButton
- `key add <hex16>` - добавить ключ iButton
- `key del <hex16>` - удалить ключ iButton
- `key clear` - очистить список ключей
- `exit` - выход в config
- `end` - выход в enable
- `help` / `?` - список команд
- `help <topic>` - справка по теме
