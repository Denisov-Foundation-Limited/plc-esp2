# AGENTS.md

Контекст и договоренности по проекту plc-esp2 (рабочие заметки)

## Важные наблюдения и договоренности

- В логах для параметров использовать : вместо =.\n- Регистрацию коллбеков отображения веб-страниц делать ПОСЛЕ регистрации всех функциональных коллбеков/обработчиков.
- Файлы с русским текстом сохранять в UTF-8, иначе проблемы с кириллицей (mojibake) и правками.
- При создании больших буферов и кэшей предлагать использовать PSRAM.
- Если в веб-страницах есть русские строки, их лучше править через команды записи целиком (Set-Content), чтобы не ломалась кодировка.
- В интерфейсе дисплея все поля ограничиваем 3 символами; ошибки показываем как `ERR` (3 символа). 4-й символ заполняется пробелом.
- Время на дисплее отрисовывается в 2 слотах (часы и минуты) — это осознанное ограничение по 3 символам.
- Для температуры: если значение <= -10, знак градуса не рисуется (чтобы влезть в 3 символа).
- При ошибках получения данных (кэш/запросы) в соответствующих полях дисплея выводится `ERR`.

## Изменения по стеку (архитектура и поведение)

- StackCache перемещён в App как отдельный член. Потребителям (WebInterface и др.) передаётся ссылка/указатель через `setStackCache()`.
- Инициализация кэша перенесена туда, где инициализируется сеть/стек (App::begin); из WebInterface удалён `initStackCacheAllocations()`.
- Для слейва добавлены отдельные кэши (sockets/lights/tanks/septic/meteo) с выделением памяти в PSRAM, опрос — только по запросу (для веба/дисплея).

## Фоллбек-режим стека

- Добавлен флаг фоллбека и адрес fallback-хоста в конфиг, настраиваются в Web (Стек) и CLI.
- При потере связи с мастером слейв переходит в режим master (если включен fallback и НЕ задан fallback_host).
- Если задан fallback_host — локальный master НЕ поднимается, слейв переключается на другого мастера.
- Логи: переход в fallback master и выход из него, а также лог «Role switch: master/slave».
- MASTER_LED_PIN: светодиод горит в режиме master (включая fallback) и гаснет в режиме slave.

## Признак «контроллер/модуль»

- Для слейва есть флаг `slave_controller` (по умолчанию true).
- Этот флаг передаётся через caps в StackHello; хранится в StackMaster.
- В Telegram и Cloud списках устройств показываются только узлы с `slave_controller=true`.
- В веб-странице Стек добавлена колонка «Тип» (Контроллер/Модуль).

## Веб-страница «Стек»

- Для роли master скрываются поля, относящиеся к slave.
- Русифицированы подписи, сокращены длинные тексты:
  - «Резервный мастер» + «Включить»
  - «Хост/IP резервного мастера»
  - «Слейв-контроллер» и т.п.

## Повторяющиеся ошибки/грабли

- Ошибка таб-комплишена CLI: при добавлении команд нужно обновлять размер массива `std::array`.
- Кодировка веб-страниц: редактирование через apply_patch может ломать UTF-8.
- При изменениях в Stack/CLI/Web важно синхронизировать:
  - config (ConfigsManager)
  - web формы + обработчики
  - CLI команды + help
  - README

## Стековые кэши: структура работы

- Два уровня кэшей:
  - Master: `StackCache` (в App), хранит кэши по нодам для sockets/lights/meteo/thermo/tanks/septic/security.
  - Slave: `StackSlaveHandler` держит `remote*Cache` (sockets/lights/meteo/thermo/tanks/septic/security) и запрашивает данные у мастера.
- Обновление кэша на мастере:
  - Только через ответы на `CmdGet` (`Ack/Err`) от слейвов.
  - Поля кэша: `has_data`, `pending`, `last_ok`, `last_error`, `updated_ms`, `item_count`, `items[]`.
  - Для `Security` в кэше хранится и статус: `enabled/armed/alarm`, плюс сенсоры.
- Обновление кэша на слейве:
  - Через ответы мастера на `CmdGet` (remote‑кэши).
  - Логика и флаги аналогичны master‑кэшу.
- Инициирование запросов:
  - В отображении (дисплей/веб) при отсутствии данных вызываются `request*`.
  - Для актуальности на мастере в фоне работает `pollStackCaches_()` — по очереди опрашивает ноды (security/septic/tanks/meteo).
- Дисплей:
  - Для `master` берёт данные из `StackCache`.
  - Для `slave` берёт данные из `remote*Cache` (через мастера).
- Алармы:
  - На мастере алермы пересчитываются по кэшу и локальным данным.
  - На слейве удалённые алермы не учитываются (только локальные).

## Дисплей: источники данных и режимы

- Общая схема:
  - В каждом слоте дисплея хранится `kind/field/index/node_id`.
  - `node_id = 0` означает локальные данные.
  - `node_id != 0` означает данные другого юнита.
- Режим master:
  - Данные других юнитов берутся из `StackCache` (master‑кэши).
  - Если данных нет, инициируется `request*` (CmdGet) к слейву.
  - Ошибки получения данных показываются как `ERR`.
- Режим slave:
  - Данные других юнитов берутся из `StackSlaveHandler::remote*Cache`.
  - При отсутствии данных инициируется запрос к мастеру (`requestRemote*`).
  - Ошибки получения данных показываются как `ERR`.
- Поддерживаемые слоты (remote):
  - `Socket`, `Light`, `Meteo`, `Thermo`, `Tank`, `Septic`, `Security` — все имеют remote‑режим.
  - `Security`: отображает `ARM`/`DIS` (armed) по кэшу.
  - `Thermo`: отображает статус `IDL/HET/COL` по `power_on/heat_on/cool_on`.
- Локальные источники:
  - `Security` — `control.controllers.security().armed()`
  - `Meteo` — `MeteoController::state(...)`
  - `Thermo` — `ThermoController::state(...)`
  - `Tank` — `TankController::state(...)`
  - `Septic` — `SepticController::stateByIndex(...)`

## Алармы: структура и логика

- Базовая структура в `PlcControl`:
  - `AlarmModule` = `Sockets/Lights/Meteo/Thermo/Tanks/Septic/Security/Ring`.
  - `_alarm_mask` — маска “модуль имеет аларм”.
  - `_alarm_detail_mask[mod]` — детальная маска (датчик/бак/канал) внутри модуля.
  - `_alarm_unit_mask[mod]` — маска по юнитам (stack node index) внутри модуля.
  - LED мигает, если есть любой активный бит (`alarm_mask | detail_any | unit_any`).
- Слои логики:
  - Детальные/юнит‑маски автоматически поднимают/снимают модульную маску.
  - Для `unit_mask` используется индекс ноды в `StackMaster` (0..31).
- Где выставляются/очищаются:
  - **Tanks**
    - Локально: `updateTankAlarms_()` — если `!levels_ok || empty` → детальный бит.
    - Remote (только master): берётся из `stack_cache.tanksCache` и ставит детальный + unit‑бит.
  - **Septic**
    - Локально: `updateSepticAlarms_()` — `st->alarm` → детальный бит.
    - Remote (только master): из `stack_cache.septicCache` + unit‑бит.
  - **Security**
    - Локально: `onSecurityDetect_()` — если `!silent` → детальный бит.
    - Очистка: `onSecurityClearDetect_()` → сброс detail/unit.
    - Remote (только master): по `StackCache` и по входящим `alarm` (stack frame) ставится detail+unit.
  - **Meteo**
    - Локально: `updateMeteoAlarms_()` — если `!ok` → детальный бит.
    - Remote (только master): из `stack_cache.meteoCache` + unit‑бит.
  - **Thermo**
    - Сейчас только для отображения; алармы не заведены.
  - **Sockets/Lights/Ring**
    - Алармы не используются, только модульные/детальные поля зарезервированы.
- Важные ограничения:
  - На **slave** удалённые алармы не учитываются (только локальные).
  - На **master** удалённые алармы пересчитываются по кэшу и входящим событиям.

## Контроллеры: поведение и источники данных

- **Sockets (Розетки)**
  - Конфиги: `SocketController::configByIndex`, `lightConfigByIndex`.
  - Стейт: `state(id)`, `lightState(id)` → `relay_on`.
  - Стек: master — `StackCache::socketsCache/lightsCache`, slave — `remoteSocketsCache/remoteLightsCache`.
  - Дисплей: показывает `ON/OFF`.
  - Алармы: не используются.

- **Meteo (Метео)**
  - Конфиги: сенсоры `configByIndex`; поддержка remote‑источника (`source_node_id/source_sensor_id`).
  - Стейт: `state(id)` → `ok/has_temp/has_humidity/temp_c/humidity`.
  - Логи: ошибки/восстановления логируются по смене состояния (без спама).
  - Стек: master — `StackCache::meteoCache`, slave — `remoteMeteoCache`.
  - Дисплей: temp/hum, ошибки → `ERR`.
  - Алармы: если `!ok` (локально + remote через кэш).

- **Thermo (Термо)**
  - Конфиги: `configByIndex` (mode, target, hyst, ports, sensor).
  - Стейт: `power_on/heat_on/cool_on`.
  - Стек: master — `StackCache::thermoCache`, slave — `remoteThermoCache`.
  - Дисплей: `IDL/HET/COL` по `power_on/heat_on/cool_on`.
  - Алармы: не заведены.

- **Tanks (Баки)**
  - Конфиги: `configByIndex` (id, ports, enabled).
  - Стейт: `level_low/mid/full`, `levels_ok`, `valve_on`, `pump_on`.
  - Стек: master — `StackCache::tanksCache`, slave — `remoteTanksCache`.
  - Дисплей: `FULL/MID/LOW/EMP`.
  - Алармы: `!levels_ok || empty` → detail; remote (master) → detail + unit.

- **Septic (Септик)**
  - Конфиги: `configByIndex` (monitoring_on, ports).
  - Стейт: `warning/alarm`, реле warning/alarm.
  - Стек: master — `StackCache::septicCache`, slave — `remoteSepticCache`.
  - Дисплей: `ALRM/WARN/OK`.
  - Алармы: `alarm` → detail; remote (master) → detail + unit.

- **Security (Охрана)**
  - Конфиги: `configByIndex` (silent, type, port, name).
  - Стейт: `is_detect`, `armed`, `alarm_on`.
  - Стек: master — `StackCache::securityCache` (сенсоры + `enabled/armed/alarm`), slave — `remoteSecurityCache`.
  - Дисплей: `ARM/DIS` по `armed`.
  - Алармы: `!silent` детекты → detail; `clearDetect` → сброс; remote (master) → detail + unit.

- **Ring (Звонок)**
  - Стейт: `relay_on`.
  - Стек: есть команды `button/set`, отображение на дисплее не предусмотрено.
  - Алармы: не используются.

- **PLC/RTC**
  - PLC: температуры (board/cpu), вентиляторы, alarm LED.
  - RTC: дата/время/температура.

## Стек: архитектура и обмен

- **Роли**
  - Master — агрегирует данные, держит `StackCache`, раздаёт web/дисплей/alarms.
  - Slave — подключается к мастеру, использует `StackSlaveHandler` для обработки команд и отдачи данных.
  - Fallback — при потере мастера может стать мастером (если включено и нет `fallback_host`).

- **Транспорт и типы сообщений**
  - Используются `StackMsgType::CmdGet/CmdSet/Ack/Err`.
  - Master отправляет `CmdGet` слейву для чтения состояния/конфигов.
  - Slave отвечает `Ack` (ok + data) или `Err` (error).

- **Фичи (StackFeature)**
  - Покрывают подсистемы: `Sockets`, `Lights`, `Meteo`, `Thermo`, `Tanks`, `Septic`, `Security`, `Ring`, `PlcStatus`, `Rtc`, и др.
  - Каждая фича имеет набор `action` (`get`, `set`, `status`, `status_req`, `button`, и т.п.).

- **StackCache (на мастере)**
  - Хранит кэши по нодам для основных фич.
  - Обновляется только по ответам `Ack/Err`.
  - Используется вебом/дисплеем/alarms.

- **StackSlaveHandler (на слейве)**
  - Получает команды от мастера, отдаёт ответы.
  - Держит remote‑кэши для отображения данных с других узлов через мастера.
  - В режиме слейва, при необходимости, запрашивает у мастера (`requestRemote*`).

- **Идентификация узлов**
  - `node_id` используется как уникальный идентификатор.
  - На мастере хранится имя и IP (для UI/логов).
  - Индекс узла в `StackMaster` используется для unit‑alarm масок.

- **Обновление данных**
  - Мастер выполняет периодический `pollStackCaches_()` (по очереди опрашивает ноды).
  - В UI/дисплее при отсутствии данных инициируется запрос `request*`.

- **Безопасность**
  - Поддерживается `stackApiKey` (проверяется в master‑обработчике запросов).

- **Ограничения**
  - На слейве удалённые алармы не учитываются.
- Remote‑дисплей работает через кэш (master‑cache или remote‑cache).

## Облако и proto.json

- В проекте есть `proto.json` — единый JSON‑протокол, используемый и в прошивке (C++), и в облаке (например, Node.js).
- `proto.json` служит источником правды для:
  - формата команд/ответов;
  - структуры данных (features/parts/actions);
  - совместимости версии протокола.
- Любые изменения в протоколе должны синхронно отражаться в прошивке и в облаке.


