# STACk.md

Подробное описание stack-подсистемы в `plc-esp2`: роли, транспорт, форматы сообщений, кэши, синхронизация и диагностика.

Документ описывает текущее поведение по коду (master/slave, `StackMaster`, `StackNode`, `StackCache`, `StackSlaveHandler`, `App::pollStackCaches_()`).

## 1. Что такое Stack в проекте

Stack — это внутренний TCP-протокол обмена между контроллерами:

- `Master`:
  - принимает подключения от slave-узлов,
  - хранит список online нод,
  - опрашивает slave по feature (`CmdGet`),
  - отправляет команды (`CmdSet`),
  - агрегирует ответы в `StackCache`,
  - отдает данные в Web / Telegram / дисплей / алармы.

- `Slave`:
  - подключается к master по TCP,
  - шлёт `Hello` + `Status`,
  - принимает `CmdGet` / `CmdSet`,
  - отвечает `Ack` / `Err`,
  - может (в режиме slave) держать remote-кэши данных других нод через master (`StackSlaveHandler`).

## 2. Роли и основные компоненты

### 2.1 Master-side

- `StackMaster` (`include/core/network/stack/stack_master.hpp`)
  - TCP сервер (`AsyncServer`)
  - сессии подключенных нод (`MAX_SESSIONS = 8`)
  - привязка сессии к `node_id` после `Hello`
  - `sendTo(node_id, type, payload, len)`
  - авто-вставка `api_key` в JSON для `CmdGet/CmdSet` (если настроен)
  - callbacks:
    - `FrameHandler`
    - `EventHandler(node_id, online/offline)`

- `StackCache` (`include/core/network/stack/stack_cache.hpp`)
  - кэши по feature и по `node_id`
  - request-функции (`requestSockets`, `requestMeteo`, `requestPorts`, ...)
  - обработка `Ack/Err` и обновление кэшей
  - флаги `pending/has_data/last_ok/last_error`

### 2.2 Slave-side

- `StackNode` (`include/core/network/stack/stack_node.hpp`)
  - TCP клиент к master
  - периодический reconnect
  - периодический `Hello`
  - периодический `Status`
  - отправка/приём stack frames

- `StackSlaveHandler` (`include/core/network/stack/stack_slave_handler.hpp`)
  - обработчик входящих `CmdGet/CmdSet`
  - маршрутизация по `feature` + `action`
  - генерация `Ack/Err` JSON
  - trace callback для raw stack trace CLI
  - remote-кэши (для отображения данных других нод через master в режиме slave)

### 2.3 App orchestration

- `App::onStackNodeEvent_()` — реакция на `online/offline`
- `App::pollStackCaches_()` — фоновый опрос кэшей master
- `App::logStackNodeInventory_()` — логика `Sync slave unit ...` / `Sync slave unit complete`
- bootstrap sync queue (ускоренная первичная синхронизация новых нод)

```text
High-level architecture
-----------------------

   [Slave A] --TCP-->\
   [Slave B] --TCP----> [StackMaster] ---> [StackCache] ---> Web / Telegram / Display / Alarms
   [Slave C] --TCP-->/        ^
                              |
                        App::pollStackCaches_()
                        (round-robin + bootstrap queue)

On each slave:
  StackNode (TCP client) <-> StackSlaveHandler (CmdGet/CmdSet handlers, Ack/Err, remote caches)
```

## 3. Транспортный формат (бинарный frame)

Реализация: `StackCodec` в `include/core/network/stack/stack_protocol.hpp`.

### 3.1 Frame layout

Каждое сообщение передаётся как:

1. `StackHeader` (5 байт)
2. `payload` (0..1024 байт)
3. `CRC16` (2 байта)

`StackHeader`:

- `magic` = `0xA5`
- `version` = `1`
- `type` = тип stack-сообщения (`StackMsgType`)
- `length` = длина payload (`uint16`, little-endian)

Ограничения:

- `kMaxPayload = 1024`
- `kMaxFrame = 5 + 1024 + 2`

```text
Binary frame on wire
--------------------

+---------+---------+------+--------------+-------------------+---------+
| magic   | version | type | length (LE)  | payload (0..1024) | CRC16   |
| 1 byte  | 1 byte  | 1 b  | 2 bytes      | N bytes           | 2 bytes |
+---------+---------+------+--------------+-------------------+---------+

CRC16 is calculated over: [magic..payload]
```

### 3.2 Проверка целостности

- CRC16 (Modbus-style poly `0xA001`, init `0xFFFF`) считается по `header + payload`.
- Если CRC не сошёлся, декодер сдвигается на байт и продолжает синхронизацию.

### 3.3 Поведение декодера

`StackCodec::feed()`:

- буферизует входящий поток
- ищет `magic/version`
- проверяет длину payload
- проверяет CRC
- выдаёт `StackFrame{type, payload, payload_len}`

## 4. Типы сообщений (`StackMsgType`)

Из `include/core/network/stack/stack_protocol.hpp`:

- `Hello = 1`
- `Features = 2`
- `Status = 3`
- `CmdSet = 4`
- `CmdGet = 5`
- `Ack = 6`
- `Err = 7`

Практически в проекте основной рабочий обмен:

- `Hello`
- `Status`
- `CmdGet`
- `CmdSet`
- `Ack`
- `Err`

## 5. `Hello` / идентификация ноды

### 5.1 Структура `StackHello`

Реализация: `include/core/network/stack/stack_types.hpp`

Поля:

- `node_id : uint32_t`
- `proto_ver : uint16_t`
- `fw_ver : uint16_t`
- `caps : uint32_t`
- `name : String` (до 32 байт)

`caps` содержит флаги возможностей, например:

- `StackCapController` (`1 << 0`) — узел считается контроллером (а не “модулем”)

### 5.2 Когда slave шлёт `Hello`

`StackNode`:

- сразу после TCP connect (`onConnect_()`)
- затем периодически (`_hello_interval_ms`, по умолчанию `15000 ms`)

### 5.3 Как master привязывает сессию

`StackMaster::handleFrame_()`:

- при `Hello` декодирует `StackHello`
- ищет существующую сессию по `hello.node_id`
- если это тот же `node_id`, но другая TCP-сессия:
  - считает reconnect (или конфликт `node_id`)
  - освобождает/закрывает старую сессию
  - привязывает текущую

### 5.4 Конфликт `node_id` (важно)

Добавлена диагностика:

- если новый `Hello` пришёл с тем же `node_id`, но другим IP/именем, master логирует:
  - `Duplicate unit_id conflict: ...`

Это позволяет отлавливать ситуацию, когда два разных слейва имеют одинаковый `node_id`.

### 5.5 Генерация `node_id` на slave (текущая)

В `Network::beginStack_()` `node_id` вычисляется как 32-битный FNV-1a hash от всех 6 байт `ESP.getEfuseMac()`.

Это сделано вместо `low32(mac)` для снижения риска коллизий.

## 6. `Status` (heartbeat / liveness)

`StackNode` шлёт `Status` периодически (`_status_interval_ms`, по умолчанию `2000 ms`).

Базовый `StackStatus`:

- `uptime_ms : uint32_t`

Master использует входящий трафик (включая `Status`) для обновления `last_seen_ms`.

`StackMaster::pruneStaleSessions_()`:

- если по сессии долго нет трафика (`_session_silence_timeout_ms`, по умолчанию `7000 ms`)
- сессия закрывается
- генерируется `EventHandler(node_id, false)`

## 7. JSON-конверт команд и ответов (`CmdGet/CmdSet/Ack/Err`)

Поверх бинарного frame payload используется JSON.

### 7.1 Формат запроса (`CmdGet` / `CmdSet`)

Типовой запрос:

```json
{
  "cmd_id": 123,
  "feature": 14,
  "action": "get",
  "params": {
    "id": 1
  },
  "api_key": "optional"
}
```

Поля:

- `cmd_id` — идентификатор команды (master ожидает reply с тем же id)
- `feature` — код `StackFeature`
- `action` — строковое действие (`get`, `set`, `status`, `toggle`, ...)
- `params` — параметры действия
- `api_key` — опционально, master может автоматически вставлять (если настроен)

```text
Request/response envelope (JSON in payload)
-------------------------------------------

Master -> Slave (CmdGet/CmdSet frame)
  {
    cmd_id, feature, action, params, [api_key]
  }

Slave -> Master (Ack frame)
  {
    cmd_id, ok:true, feature, action, [data]
  }

Slave -> Master (Err frame)
  {
    cmd_id, ok:false, feature, action, error
  }
```

### 7.2 Формат успешного ответа (`Ack`)

`StackSlaveHandler::sendAck_()` формирует:

```json
{
  "cmd_id": 123,
  "ok": true,
  "feature": 14,
  "action": "get",
  "data": { ... }
}
```

`feature` / `action` возвращаются из `_rx_doc` (эхо запроса), что удобно для маршрутизации reply и отладки.

### 7.3 Формат ошибки (`Err`)

`StackSlaveHandler::sendErr_()` формирует:

```json
{
  "cmd_id": 123,
  "ok": false,
  "feature": 14,
  "action": "get",
  "error": "auth"
}
```

Типичные ошибки:

- `json parse`
- `auth`
- `unknown feature`
- `unsupported`
- feature-specific (`missing params`, `invalid id`, ...)

### 7.4 Аутентификация (`api_key`)

На master:

- `StackMaster::sendTo()` может автоматически добавить `api_key` в JSON для `CmdGet/CmdSet`
- ключ берётся из `ConfigsManagerIface::stackApiKey()`

На slave:

- `StackSlaveHandler::authOk_()` проверяет ключ (если configured)
- при неуспехе отдаёт `Err` с `error: "auth"`

## 8. `StackFeature` (feature routing)

Перечень кодов: `include/core/network/stack/stack_features.hpp`

Примеры:

- `Ports`
- `TempSensors`
- `I2cScan`
- `OwScan`
- `Rtc`
- `PlcStatus`
- `Sockets`
- `Meteo`
- `Thermo`
- `Security`
- `Septic`
- `Tanks`
- `Ring`
- `Watering`
- `Avr`
- `Leak`

`StackSlaveHandler::handleFrame_()`:

- парсит JSON
- извлекает `feature` + `action`
- вызывает `handle<Feature>_(cmd_id, action, params)`

## 9. Master-side cache model (`StackCache`)

`StackCache` хранит агрегированные данные по нодам и feature.

### 9.1 Общая схема кэша

Для большинства feature кэш содержит:

- `node_id`
- `updated_ms`
- `pending_cmd_id`
- `pending`
- `has_data`
- `last_ok`
- `last_error`
- `items[]`
- `item_count`

Смысл флагов:

- `pending = true`
  - запрос отправлен, ответ ещё не обработан
- `has_data = true`
  - в кэше есть валидные данные items
- `last_ok = true`
  - последний ответ был успешным
- `last_error != ""`
  - последний ответ завершился ошибкой

UI/дисплей/логика часто используют правило “кэш готов”, если:

- `has_data` или `last_ok` или `last_error` (то есть был хотя бы один ответ `Ack/Err`)

### 9.2 Особенности тяжёлых feature

Некоторые feature — page-based или multipart-like по смыслу:

- `Ports` — page-based pull (`offset/limit`, `next_offset`, `done`)
- `TempSensors` — page-based pull
- `Watering` — есть paging-поля (`next_offset`) и постраничная сборка

Для них кэши имеют дополнительные поля, например:

- `next_offset`
- `page_limit`
- `parts_expected / parts_received` (историческая/совместимая логика для части feature)
- `pending_since_ms`
- `present[]` (для устойчивой сборки кэша портов)

## 10. Page-based `Ports` (важно)

Для `Ports` реализован устойчивый page-based обмен вместо burst multipart.

### 10.1 Запрос `Ports get_state`

Master (`StackCache`) отправляет `CmdGet` с:

```json
{
  "cmd_id": 10,
  "feature": 2,
  "action": "get_state",
  "params": {
    "brief": true,
    "offset": 0,
    "limit": N
  }
}
```

Поля:

- `brief: true`
  - slave возвращает только поля, нужные UI-селекторам GPIO
- `offset`, `limit`
  - page-based pull

### 10.2 Ответ страницы `Ports`

Slave отвечает `Ack` с `data`, содержащим:

- `ports` — массив элементов страницы
- `offset`
- `limit`
- `total`
- `next_offset`
- `done`

Master после получения страницы:

- мержит страницу в кэш
- если `done == false` — запрашивает следующую страницу
- если `done == true` — завершает сборку snapshot

```text
Ports page-based sync (master pull)
-----------------------------------

Master                         Slave
  | CmdGet Ports get_state(offset=0, limit=N, brief=true) |
  |------------------------------------------------------->|
  |<---------------- Ack data{ports[], offset, next, done}-|
  | if done=false:                                         |
  | CmdGet Ports get_state(offset=next_offset, limit=N)    |
  |------------------------------------------------------->|
  |<---------------- Ack data{ports[], offset, next, done}-|
  | ... repeats until done=true                            |
  | finalize cache snapshot                                |
```

### 10.3 Почему так сделано

Чтобы избежать:

- oversized payload (~1024 bytes)
- `json parse failed` на master
- потери частей burst multipart
- пустых/дергающихся списков GPIO в web

## 11. Slave-side обработка команд (`StackSlaveHandler`)

`StackSlaveHandler::handleFrame_()`:

1. `traceFrame_(false, frame)` — trace callback (если включён)
2. Если `Ack/Err`:
   - это ответы от master на remote-запросы slave (remote-кэши)
3. Если `CmdGet/CmdSet`:
   - parse JSON
   - auth check
   - dispatch по `feature`
   - `sendAck_()` / `sendErr_()`

### 11.1 Remote-кэши на slave

В режиме slave `StackSlaveHandler` может запрашивать через master данные других нод и держать кэши:

- remote sockets
- remote lights
- remote meteo
- remote thermo
- remote septic
- remote tanks
- remote security

Это используется для:

- remote display slots на слейве
- локального UI/логики слейва, когда источником являются другие ноды через master

## 12. Master polling и синхронизация

Реализация orchestration — в `App`.

### 12.1 Базовый фоновой polling (round-robin)

`App::pollStackCaches_()`:

- работает только в режиме master
- идёт по online нодам (`nodeCount()`, `nodeIdAt(i)`)
- отправляет **один запрос одной feature за тик**
- ротирует:
  - индекс ноды (`_stack_poll_index`)
  - индекс feature (`_stack_poll_feature_index`)

Это предотвращает залп запросов и starvation, которое ранее происходило при массовой отправке всех `CmdGet`.

```text
Round-robin polling (master)
----------------------------

tick T0: node[0], feature[0] -> requestSockets(node0)
tick T1: node[1], feature[1] -> requestLights(node1)
tick T2: node[2], feature[2] -> requestSecurity(node2)
tick T3: node[0], feature[3] -> requestSecurityPrearm(node0)
tick T4: node[1], feature[4] -> requestThermo(node1)
...

One request per tick only.
```

### 12.2 Feature sequence (типично)

В polling участвуют (по индексу):

- sockets
- lights
- security
- security prearm
- thermo
- septic
- tanks
- meteo
- watering
- avr
- leak
- ports
- temp_sensors

## 13. Ускоренная первичная синхронизация (bootstrap sync)

Добавлена для ускорения появления данных после `Unit online`.

### 13.1 Зачем

Обычный round-robin может дать заметную задержку до первой полной синхронизации ноды.

При этом делать burst `request*` сразу по всем feature нельзя (перегрузка/pending/starvation).

### 13.2 Как работает bootstrap sync

После `Unit online`:

- нода ставится в bootstrap queue
- активная bootstrap-нода опрашивается с повышенным приоритетом
- по-прежнему отправляется **один запрос одной feature за тик**
- используется более частый интервал bootstrap

```text
Bootstrap sync queue (priority warm-up)
---------------------------------------

Unit online(nodeA) -> enqueue [A]
Unit online(nodeB) -> enqueue [A,B]

Active bootstrap: A
  every bootstrap tick -> send 1 feature request for A
  until core sync ready / timeout / offline
  then handoff

Active bootstrap: B
  every bootstrap tick -> send 1 feature request for B
  ...

Meanwhile:
  normal round-robin remains as fallback/background
```

Текущие параметры (см. `App`):

- `kStackBootstrapPollMs = 250`
- `kStackBootstrapTimeoutMs = 25000`
- `kStackBootstrapPasses = 2`

### 13.3 Очередь bootstrap (несколько нод)

Если одновременно подключились несколько слейвов:

- они ставятся в очередь bootstrap
- синхронизируются по очереди
- `offline` нода удаляется из активной/bootstrap queue

### 13.4 Handoff на следующего слейва (важно)

Чтобы второй слейв не ждал слишком долго:

- bootstrap handoff **не ждёт** тяжёлые feature (`Ports`, `TempSensors`)
- достаточно:
  - `Sync slave unit complete` (core inventory готов)
  - и отправки bootstrap-запросов по core-feature

`Ports` / `TempSensors` догружаются далее обычным round-robin.

```text
Bootstrap handoff condition (simplified)
----------------------------------------

if Sync slave unit complete == true
   and core features were already requested
then
   stop bootstrap(current)
   start bootstrap(next from queue)
else
   continue bootstrap(current)

Heavy features (Ports, TempSensors) do NOT block handoff.
```

### 13.5 Логи bootstrap

По умолчанию текстовые event-логи bootstrap выключены:

- `STACK_BOOTSTRAP_EVENT_LOGS = 0`

Отдельные debug-логи bootstrap тоже выключены:

- `STACK_BOOTSTRAP_DEBUG_LOGS = 0`

Если включить debug:

- будут логи очереди / feature / pass / skipped request

Примечание:

- raw CLI `stack trace` — это другой механизм (trace stack frames), он не равен текстовым bootstrap-логам.

## 14. Логи синхронизации (`Sync slave unit ...`)

`App::logStackNodeInventory_()` логирует инвентарь slave по мере готовности кэшей:

- `Sync slave unit: <name> item: sockets ...`
- `... meteo ...`
- `... tanks ...`
- ...
- `Sync slave unit complete: <name>`

Логика “готовности” для feature:

- кэш считается готовым после реального ответа (`Ack` или `Err`)
- не только после отправки запроса

Это важно, чтобы не считать sync завершённым по `pending`.

## 15. Web / Telegram / дисплей и stack-кэши

### 15.1 Master

На master:

- UI/дисплей/алармы читают remote-данные из `StackCache`
- при отсутствии данных могут инициировать `request*`

### 15.2 Slave

На slave:

- remote-данные берутся из `StackSlaveHandler::remote*Cache`
- при отсутствии данных могут инициироваться `requestRemote*` к master

### 15.3 Актуальность и pending

UI должен учитывать:

- `pending`
- stale-данные
- отсутствие `has_data`

И не считать первый попавшийся старый state “финальным” после `CmdSet`.

## 16. Raw trace (CLI `stack trace`)

`CLIStack` умеет показывать raw stack frames:

- master-side frames (`[STACK] node=... type=... payload=...`)
- slave-side trace (`[STACK] slave tx/rx ...`)

Это trace низкого уровня:

- бинарный frame уже декодирован в `type + payload`
- payload печатается как строка (обычно JSON)

Используется для диагностики:

- некорректных `Ack/Err`
- неправильного `cmd_id`
- проблем с `feature/action`

## 17. Типичные проблемы и как диагностировать

### 17.1 В списке только один слейв при двух устройствах

Симптомы:

- `Unit online` скачет между двумя IP
- `Duplicate unit_id conflict`

Причина:

- одинаковый `node_id`

Что смотреть:

- логи `Unit online: ... id: 0x...`
- MAC / генерацию `node_id`

### 17.2 Долгая первичная синхронизация

Смотреть:

- `Sync slave unit ...`
- `Sync slave unit complete`
- (если включен debug bootstrap) feature/pass/skipped

Что влияет:

- Wi-Fi качество
- pending на тяжёлых feature
- page-based `Ports` / `TempSensors`

### 17.3 `json parse failed` / большие payload

Причина:

- payload упёрся в лимит `1024`
- oversized `Ack`

Что делать:

- page-based pull
- `brief` payload
- уменьшать page size

### 17.4 Auth errors

Если slave отвечает `Err` с `error: "auth"`:

- проверить `stackApiKey` на master
- проверить конфиг slave

## 18. Практические правила развития stack

1. Не отправлять “залп” всех `CmdGet` на ноду в одном тике.
2. Для тяжёлых feature использовать page-based pull (`offset/limit`).
3. `Ack set` и `Ack get` обрабатывать раздельно (не смешивать кэш-логики).
4. Для bool-полей в частичном апдейте не использовать `|`-слияние (иначе теряется `false`).
5. Логи sync строить по факту реальной готовности кэша (`Ack/Err`), а не по отправке request.
6. Для диагностики конфликтов нод всегда логировать `node_id` и IP.

## 19. Куда смотреть в коде (карта)

- Транспорт / codec:
  - `include/core/network/stack/stack_protocol.hpp`
- Базовые типы (`Hello`, `Status`, caps):
  - `include/core/network/stack/stack_types.hpp`
- Feature enum:
  - `include/core/network/stack/stack_features.hpp`
- Master sessions / sendTo / events:
  - `include/core/network/stack/stack_master.hpp`
- Slave TCP client / hello/status:
  - `include/core/network/stack/stack_node.hpp`
- Slave command handler / Ack/Err / remote-caches:
  - `include/core/network/stack/stack_slave_handler.hpp`
- Master cache + requests + reply parsing:
  - `include/core/network/stack/stack_cache.hpp`
- Orchestration / polling / bootstrap / sync logs:
  - `src/app.cpp`
- Stack role / node_id setup:
  - `src/core/network/network.cpp`
