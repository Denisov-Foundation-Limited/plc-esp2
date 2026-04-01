# STACK.md

# 🌐 Stack в plc-esp2

Актуальное описание stack-подсистемы на текущей ветке `develop`.

Этот документ описывает именно новую структуру проекта:

- `StackTransport`
- `StackMasterServer`
- `StackMasterRouter`
- `StackSlaveClient`
- `StackRouteAdapter`
- `StackDeviceRegistry`
- `StackUnitSnapshot`

Старые сущности вроде `StackMaster`, `StackNode`, `StackCache`, `StackSlaveHandler` и старые TCP-схемы больше не являются источником правды для текущего кода.

Общий обзор проекта: [README.md](./README.md)

## 🧭 Назначение stack

Stack нужен для связи между PLC-узлами в распределённой системе:

- master агрегирует доступные slave-узлы
- slave исполняет входящие команды и отдает snapshot/state
- web/display/rules/cloud на мастере работают не с “сырым сокетом”, а с индексом устройств и снимками состояния

## 🧱 Верхнеуровневая архитектура

```mermaid
flowchart TD
  MASTER_RT[Master runtime]
  SLAVE_RT[Slave runtime]
  SERVER[StackMasterServer]
  ROUTER[StackMasterRouter]
  ADAPTER[StackRouteAdapter]
  REG[StackDeviceRegistry]
  SNAP[StackUnitSnapshot]
  CLIENT[StackSlaveClient]
  WS[WebSocket transport]
  RS[RS485 transport]

  MASTER_RT --> ADAPTER
  ADAPTER --> SERVER
  SERVER --> ROUTER
  ROUTER --> REG
  ROUTER --> SNAP
  ADAPTER --> WS
  ADAPTER --> RS
  CLIENT --> ADAPTER
  SLAVE_RT --> CLIENT
```

## 🧩 Основные компоненты

### `StackTransport`

Базовый транспортный интерфейс. От него ожидается:

- connect/disconnect lifecycle
- text/binary message delivery
- callback-уведомления о событиях транспорта

Фактические реализации:

- websocket
- rs485

### `StackMasterServer`

Серверная часть мастера. Поднимает серверный transport и передает сообщения в router.

Отвечает за:

- запуск мастер-сервера
- binding обработчиков route/notify
- работу в роли master

### `StackMasterRouter`

Центральная точка мастер-маршрутизации.

Отвечает за:

- auth/accept узлов
- регистрацию устройства в `StackDeviceRegistry`
- отправку `text/binary` на нужный `node_id`
- доставку route/event/request/response наверх

### `StackSlaveClient`

Клиентская часть слейва.

Отвечает за:

- подключение к мастеру
- auth/hello
- приём команд и route
- отправку ответов, событий и notify на мастер

### `StackRouteAdapter`

Это текущий верхний API stack-обмена. Вокруг него сейчас строится вся прикладная логика.

Он даёт:

- `sendRequest(...)`
- `sendEvent(...)`
- `sendRoute(...)`
- `sendResponse(...)`
- unified route/notify handling
- payload mode:
  - `auto`
  - `json`
  - `binary`
- exchange policy:
  - `auto`
  - `direct`
  - `poll`

### `StackDeviceRegistry`

Реестр онлайн-узлов.

Хранит:

- `node_id`
- имя
- IP/transport diagnostics
- caps
- online/offline presence

Используется для:

- web-страницы стека
- определения online/offline
- именования удалённых источников
- bootstrap/sync логики

### `StackUnitSnapshot`

Текущий индекс удалённых данных по узлам.

Это уже не “живой сокетный кэш старого образца”, а snapshot/index слой для удалённых контроллеров:

- `State`
- `CacheState`
- `RequestState`
- страницы данных (`sockets/lights/meteo/thermo/tanks`)
- bookkeeping по page requests

Ключевая идея:

- UI и runtime читают из snapshot/index
- route responses обновляют snapshot
- старые/пустые данные не считаются готовыми

## 👑 Роли узла

### Master

Master:

- принимает slave-узлы
- ведет registry
- ведет snapshot/index
- опрашивает удалённые фичи
- отдает stack-данные в web/display/rules/cloud

### Slave

Slave:

- подключается к master
- принимает команды/requests
- возвращает snapshots/state/pages
- шлёт доменные события на мастер

### Fallback

Поддерживается fallback-поведение:

- при потере мастера slave может поднять локальный master
- если указан `fallback_host`, slave переключается на другой мастер вместо локального takeover

## 🔀 Exchange policy и payload mode

CLI/Web конфиг поддерживает:

### Policy

- `auto`
- `direct`
- `poll`

Практический смысл:

- `direct` — упор на прямую доставку/обмен
- `poll` — более выраженный запросный режим
- `auto` — стандартный рекомендуемый режим

### Payload mode

- `auto`
- `json`
- `binary`

Практический смысл:

- `json` удобно для совместимости и трассировки
- `binary` нужен для более компактного или типизированного обмена
- `auto` оставляет выбор route layer/runtime

## 📡 Транспорт

Сейчас проект поддерживает два транспортных направления stack:

- `websocket`
- `rs485`

Конфигурация выбирается через:

- CLI
- Web `/stack`
- runtime apply через `ConfigsManager`

## 🔁 Поток обмена

### Route-запрос/ответ

```mermaid
sequenceDiagram
  participant M as Master runtime
  participant A as StackRouteAdapter
  participant S as Slave client

  M->>A: sendRequest(node_id, feature, action)
  A->>S: route request
  S-->>A: route response
  A-->>M: callback / snapshot update
```

### Event от slave на master

```mermaid
sequenceDiagram
  participant SL as Slave controller
  participant A as StackRouteAdapter
  participant MR as Master runtime

  SL->>A: sendEvent(0, feature, action, payload)
  A->>MR: route event callback
  MR->>MR: update runtime / alarms / cloud
```

## 🗂️ Snapshot-индекс

`StackUnitSnapshot` сейчас хранит не всё подряд, а наборы, реально нужные runtime/UI:

- system state
- summary state
- sockets page
- lights page
- meteo page
- thermo page
- tanks page

И для каждого узла ведёт:

- есть ли данные
- свежесть данных
- pending request
- pending page request
- offset/limit bookkeeping

### Почему это важно

Это решает старые проблемы:

- UI не должен считать stale-cache за актуальное состояние
- page request bookkeeping нельзя перетирать временными snapshot-объектами
- partial updates можно применять аккуратно, не ломая всё состояние узла

## 🧠 Интеграция с `AppRuntime`

`AppRuntime` сейчас использует stack так:

- `pollStackCaches_()` — фоновые запросы к stack-feature страницам
- bootstrap sync новых узлов
- display remote slot rendering
- cloud stack events
- alarms/summaries по удалённым узлам

```mermaid
flowchart TD
  NET[Network / Stack route callbacks]
  SNAP[StackUnitSnapshot]
  RUNTIME[AppRuntime]
  WEB[Web]
  DISP[Display]
  CLOUD[Cloud]

  NET --> SNAP
  SNAP --> RUNTIME
  RUNTIME --> WEB
  RUNTIME --> DISP
  RUNTIME --> CLOUD
```

## 📄 Пагинация

Для тяжёлых stack-данных используется page-based sync:

- sockets
- lights
- meteo
- thermo
- tanks

Это важно, чтобы:

- не раздувать payload
- не устраивать burst-запросы
- не ронять parse/latency на больших наборах

### Типовой page-поток

```mermaid
sequenceDiagram
  participant M as Master
  participant S as Slave
  participant X as StackUnitSnapshot

  M->>S: request page offset: 0 limit: N
  S-->>M: items 0..N
  M->>X: apply page
  M->>S: request next page
  S-->>M: items N..end
  M->>X: finalize
```

## 🔔 Stack и Cloud

Stack-события теперь могут публиковаться в cloud через master.

Важно:

- master шлёт cloud event с `unit: "stack"` и `node_id`
- в `payload.data` прокидывается `source_name`
- контроллеры не шлют в cloud напрямую, это делает runtime/cloud layer

## 🧪 Диагностика

### `show stack`

CLI показывает:

- `role`
- `master_host`
- `policy`
- `transport`
- `payload`
- `fallback`
- `fallback_host`
- `controller`
- `api_key`
- runtime diagnostics:
  - `state`
  - `master_active`
  - `fallback_active`
  - `online`
  - exchange queue/load counters
  - rs485 counters/state

### Что смотреть при проблемах

1. online/offline узла
2. registry и имя `node_id`
3. свежесть `StackUnitSnapshot`
4. page request stuck / stale data
5. `retried/expired/dropped`
6. `network_lock_held_ms` / `rt_lock_held_ms`
7. `rs485_*` counters при транспортных проблемах

## ⚠️ Частые ошибки

- Документация/код рассинхронизированы и продолжают ссылаться на старые stack-классы
- page bookkeeping случайно затирается временным snapshot
- UI принимает pending/stale state за финальное состояние
- новые stack feature меняются в runtime, но не синхронизируются с:
  - CLI
  - Web
  - README/STACK docs

## 🛠️ Что менять синхронно при доработке stack

- `ConfigsManager`
- CLI `stack ...`
- Web `/stack`
- `Network`
- `StackRouteAdapter`
- `AppRuntime`
- docs

## 📁 Карта кода

- `include/core/network/stack/stack_transport.hpp`
- `include/core/network/stack/stack_master_server.hpp`
- `include/core/network/stack/stack_master_router.hpp`
- `include/core/network/stack/stack_slave_client.hpp`
- `include/core/network/stack/stack_route_adapter.hpp`
- `include/core/network/stack/stack_device_registry.hpp`
- `include/core/network/stack/stack_unit_snapshot.hpp`
- `src/core/network/stack/`
- `include/core/runtime/app_runtime.hpp`
- `src/core/runtime/`
