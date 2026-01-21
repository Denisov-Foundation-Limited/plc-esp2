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

Команды сгруппированы по режимам/контекстам. Используйте `help` или `?` для текущего режима, либо `help <topic>` для конкретной темы.

### Режим enable (`plc#`)

- `show plc` - состояние вентилятора и температура платы
- `show board` - имя профиля платы
- `show wifi` - конфигурация Wi-Fi
- `show time` - дата/время RTC
- `show i2c` - список устройств I2C
- `show ow` - список устройств OneWire
- `show stack` - настройки роли stack
- `show telegram` - настройки Telegram
- `show config` - содержимое конфигурационного файла
- `show port <id>` - параметры порта
- `show ports` - список портов
- `show sockets` - список розеток
- `show socket <id>` - параметры розетки
- `show meteo` - список датчиков meteo
- `show meteo <id>` - параметры датчика
- `show thermo` - список устройств thermo
- `show thermo <id>` - параметры устройства
- `show tanks` - список баков
- `show tank <id>` - параметры бака
- `show ext` - список расширителей
- `socket toggle <id>` - переключить реле розетки
- `socket on <id>` - включить реле
- `socket off <id>` - выключить реле
- `ftest` - запуск функционального теста
- `copy tftp://<ip>/firmware.bin firmware` - обновление прошивки
- `copy http://<ip>/firmware.bin firmware` - обновление прошивки
- `stack nodes` - список узлов stack
- `stack send <id> <get|set> <json>` - отправка команды stack
- `stack socket <unit> <on|off|toggle> <id>` - управление розеткой
- `stack thermo <unit> <on|off|toggle> <id>` - управление устройством thermo
- `wifi restart` - перезапуск Wi-Fi
- `reload` - перезапуск контроллера
- `reset` - перезапуск контроллера
- `write` - сохранить конфигурацию
- `erase` - удалить конфигурацию
- `ext scan` - пересканировать расширители
- `configure terminal` - вход в режим config
- `conf t` - вход в режим config
- `disable` - завершить сессию
- `logout` - завершить сессию
- `exit` - завершить сессию
- `help` - помощь для текущего режима
- `?` - помощь для текущего режима
- `help show` - помощь по командам "show"
- `help wifi` - помощь по командам Wi-Fi
- `help user` - помощь по административным командам
- `help system` - помощь по системным командам
- `help tgbot` - помощь по командам Telegram
- `help socket` - помощь по командам socket
- `help meteo` - помощь по командам meteo
- `help thermo` - помощь по командам thermo
- `help tank` - помощь по командам tank

### Режим config (`plc(config)#`)

- `password <pass>` - задать пароль администратора
- `admin password <pass>` - задать пароль администратора
- `stack role <master|slave>` - установить роль устройства
- `stack master <host>` - установить хост/IP мастера
- `wifi` - вход в контекст Wi-Fi
- `wifi ssid <value>` - задать SSID для STA
- `wifi password <value>` - задать пароль для STA
- `wifi ap on|off` - включить/выключить AP
- `wifi ap_ssid <value>` - задать SSID для AP
- `wifi ap_password <value>` - задать пароль для AP
- `wifi restart` - перезапуск Wi-Fi
- `tgbot` - вход в контекст Telegram
- `time` - вход в контекст времени
- `socket` - вход в контекст розеток
- `meteo` - вход в контекст meteo
- `thermo` - вход в контекст thermo
- `tank` - вход в контекст баков
- `exit` - возврат в enable
- `end` - возврат в enable
- `help` - помощь для текущего режима
- `?` - помощь для текущего режима
- `help <topic>` - помощь по теме (`show`, `wifi`, `user`, `system`, `tgbot`, `socket`, `meteo`, `thermo`, `tank`)

### Контекст Wi-Fi (`plc(config-wifi)#`)

- `ssid <value>` - задать SSID для STA
- `password <value>` - задать пароль для STA
- `ap on|off` - включить/выключить AP
- `ap_ssid <value>` - задать SSID для AP
- `ap_password <value>` - задать пароль для AP
- `restart` - перезапуск Wi-Fi
- `show` - показать конфигурацию Wi-Fi
- `exit` - возврат в config
- `end` - возврат в enable
- `help` - помощь для текущего режима
- `?` - помощь для текущего режима
- `help <topic>` - помощь по теме

### Контекст Telegram (`plc(config-tgbot)#`)

- `token <value>` - задать токен бота
- `chat <id>` - задать chat id
- `insecure on|off` - проверка TLS
- `allow list` - показать список разрешенных пользователей
- `allow add <username> [chat_id] [admin] [notify] [off]` - добавить пользователя
- `allow del <username>` - удалить пользователя
- `allow clear` - очистить список
- `send <text>` - отправить сообщение
- `poll` - опрос команд
- `show` - показать настройки
- `exit` - возврат в config
- `end` - возврат в enable
- `help` - помощь для текущего режима
- `?` - помощь для текущего режима
- `help <topic>` - помощь по теме

### Контекст времени (`plc(config-time)#`)

- `date <YYYY-MM-DD>` - задать дату
- `time <HH:MM:SS>` - задать время
- `set <YYYY-MM-DD> <HH:MM:SS>` - задать дату/время
- `show` - показать время RTC
- `exit` - возврат в config
- `end` - возврат в enable
- `help` - помощь для текущего режима
- `?` - помощь для текущего режима
- `help <topic>` - помощь по теме

### Контекст socket (`plc(config-socket)#`)

- `show` - список розеток
- `show <id>` - параметры розетки
- `enable <id>` - включить розетку
- `disable <id>` - отключить розетку
- `name <id> <value>` - задать имя розетки
- `button <id> <port|none>` - задать порт кнопки
- `relay <id> <port|none>` - задать порт реле
- `exit` - возврат в config
- `end` - возврат в enable
- `help` - помощь для текущего режима
- `?` - помощь для текущего режима
- `help <topic>` - помощь по теме

### Контекст meteo (`plc(config-meteo)#`)

- `show` - список датчиков meteo
- `show <id>` - параметры датчика
- `name <id> <text>` - задать имя датчика
- `enable <id>` - включить датчик
- `disable <id>` - отключить датчик
- `type <id> <none|ds18b20|dht22>` - задать тип датчика
- `addr <id> <hex|none>` - задать адрес DS18B20
- `pin <id> <pin|none>` - задать пин DHT22
- `exit` - возврат в config
- `end` - возврат в enable
- `help` - помощь для текущего режима
- `?` - помощь для текущего режима
- `help <topic>` - помощь по теме

### Контекст thermo (`plc(config-thermo)#`)

- `show` - список устройств thermo
- `show <id>` - параметры устройства
- `name <id> <text>` - задать имя устройства
- `enable <id>` - включить устройство
- `disable <id>` - отключить устройство
- `mode <id> <off|heat|cool|auto>` - задать режим
- `sensor <id> <sensor|none>` - задать датчик meteo
- `target <id> <temp>` - задать целевую температуру
- `hyst <id> <temp>` - задать гистерезис
- `heat <id> <port|none>` - задать порт реле нагрева
- `cool <id> <port|none>` - задать порт реле охлаждения
- `button <id> <port|none>` - задать порт кнопки
- `exit` - возврат в config
- `end` - возврат в enable
- `help` - помощь для текущего режима
- `?` - помощь для текущего режима
- `help <topic>` - помощь по теме

### Контекст tank (`plc(config-tank)#`)

- `show` - список баков
- `show <id>` - параметры бака
- `name <id> <text>` - задать имя бака
- `enable <id>` - включить бак
- `disable <id>` - отключить бак
- `power <id> <0|1>` - включение питания
- `low <id> <port|none>` - задать вход низкого уровня
- `mid <id> <port|none>` - задать вход среднего уровня
- `full <id> <port|none>` - задать вход высокого уровня
- `valve <id> <port|none>` - задать реле клапана
- `pump <id> <port|none>` - задать реле насоса
- `alarm <id> <port|none>` - задать реле тревоги
- `exit` - возврат в config
- `end` - возврат в enable
- `help` - помощь для текущего режима
- `?` - помощь для текущего режима
- `help <topic>` - помощь по теме
