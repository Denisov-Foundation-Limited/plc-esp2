#pragma once

namespace WebUiRu
{
namespace Controllers
{
inline constexpr const char kI2c[] = "I2C ошибка: ";
inline constexpr const char kOw[] = "OW ошибка: ";
inline constexpr const char kPorts[] = "Ports ошибка: ";
inline constexpr const char kExtenders[] = "Extenders ошибка: ";
inline constexpr const char kText[] = "<h2>Контроллеры</h2>";
inline constexpr const char kUnitDevicenameNodeidIp[] = "<th>Unit</th><th>DeviceName</th><th>NodeID</th><th>IP</th><th>Тип</th>";
inline constexpr const char kText2[] = "<p class=\"status\">Слейвы не подключены</p>";
inline constexpr const char kIpRtcCpu[] = "<th>Узел</th><th>IP</th><th>Дата</th><th>Время</th><th>RTC</th><th>Плата</th><th>CPU</th><th>Вент.</th>";
inline constexpr const char kText3[] = "<tr><td colspan=\"4\" style=\"color:#94a3b8\"><strong>Стек недоступен</strong></td></tr>";
inline constexpr const char kText4[] = "<tr><td colspan=\"4\" style=\"color:#94a3b8\"><strong>Контроллеров нет</strong></td></tr>";
inline constexpr const char kText5[] = "Контроллер";
inline constexpr const char kText6[] = "Модуль";
inline constexpr const char kFcplc[] = "<div class=\"nav\"><a href=\"/\">FCPLC</a> | <a href=\"/controllers\">Контроллеры</a>";
inline constexpr const char kText7[] = " | <a href=\"/wifi\">Сеть</a>";
inline constexpr const char kText8[] = " | <a href=\"/manage\">Прошивка и файлы</a> | <a href=\"/ports\">Порты</a> | <a href=\"/buses\">Шины</a>";
inline constexpr const char kText9[] = " | <a href=\"/stack\">Стек</a> | <a href=\"/users\">Пользователи</a> | <a href=\"/display\">Дисплей</a>";
inline constexpr const char kText10[] = " | <a href=\"/rules\">Правила</a>";
inline constexpr const char kTelegram[] = " | <a href=\"/telegram\">Telegram</a> | <a href=\"/cloud\">Облако</a>";
inline constexpr const char kLogs[] = " | <a href=\"/admin\">Система</a> | <a href=\"/logs\">Logs</a>";
} // namespace Controllers

namespace Display
{
inline constexpr const char kSelectClassFieldSlotKindNameDs[] = "<label>Источник</label><select class=\"field slot-kind\" name=\"ds";
inline constexpr const char kSelectClassFieldSlotNodeNameDs[] = "<label>Устройство</label><select class=\"field slot-node\" name=\"ds";
inline constexpr const char kSelectClassFieldSlotIndexNameDs[] = "<label>Объект</label><select class=\"field slot-index\" name=\"ds";
inline constexpr const char kSelectClassFieldSlotFieldNameDs[] = "<label>Параметр</label><select class=\"field slot-field\" name=\"ds";
inline constexpr const char kInputClassFieldSlotTextTypeText[] = "<label>Текст</label><input class=\"field slot-text\" type=\"text\" name=\"ds";
} // namespace Display

namespace Index
{
inline constexpr const char kBoard[] = "Плата";
inline constexpr const char kDeviceName[] = "Имя устройства";
inline constexpr const char kStatus[] = "Статус";
inline constexpr const char kDate[] = "Дата";
inline constexpr const char kTime[] = "Время";
inline constexpr const char kRtcTemp[] = "RTC температура";
inline constexpr const char kBoardTemp[] = "Температура платы";
inline constexpr const char kFan[] = "Вентилятор";
} // namespace Index

namespace Lights
{
inline constexpr const char kText[] = "<div class=\"tile empty\"><strong>Контроллеры недоступны</strong></div>";
inline constexpr const char kText2[] = "Свет";
inline constexpr const char kText3[] = "Включена";
inline constexpr const char kText4[] = "Выключена";
inline constexpr const char kPageTitle[] = "Свет";
inline constexpr const char kPagePrev[] = "Назад";
inline constexpr const char kPagePage[] = "Страница";
inline constexpr const char kPageNext[] = "Вперёд";
inline constexpr const char kText5[] = "<div class=\"form-row\"><label>Кнопка</label>";
inline constexpr const char kText6[] = "<div class=\"form-row\"><label>Реле</label>";
inline constexpr const char kText7[] = "<div class=\"form-row\"><label>Перекл.</label>";
inline constexpr const char kText8[] = "<div class=\"tile empty\"><strong>Свет отсутствует</strong></div>";
inline constexpr const char kText9[] = "<div class=\"tile empty\"><strong>Ожидаем данные со слейва</strong></div>";
} // namespace Lights

namespace Meteo
{
inline constexpr const char kText[] = "<div class=\"tile empty\"><strong>Ожидаем данные со слейва</strong></div>";
inline constexpr const char kText2[] = "<div class=\"tile empty\"><strong>Датчики отсутствуют</strong></div>";
inline constexpr const char kPageTitle[] = "Метео";
inline constexpr const char kPagePrev[] = "Назад";
inline constexpr const char kPagePage[] = "Страница";
inline constexpr const char kPageNext[] = "Вперёд";
inline constexpr const char kC[] = "</div><div class=\"sensor-unit\">°C</div>";
inline constexpr const char kText3[] = "Датчик";
inline constexpr const char kText4[] = "Отключен";
inline constexpr const char kText5[] = "Нет данных";
inline constexpr const char kText6[] = "ОК";
inline constexpr const char kText7[] = "Ошибка";
inline constexpr const char kText14[] = "Доступно только на локальном устройстве";
inline constexpr const char kText8[] = "<div class=\"form-row\"><label>Тип</label><div class=\"field mini\">";
inline constexpr const char kText9[] = "<div class=\"form-row\"><label>Пин</label><div class=\"field mini\">";
inline constexpr const char kText10[] = "<div class=\"form-row full\"><label>Адрес</label><div class=\"field\">";
inline constexpr const char kText11[] = "<div class=\"tile empty\"><strong>Метео недоступно</strong></div>";
inline constexpr const char kText12[] = "<div class=\"form-row full\" style=\"margin-bottom:8px;\"><label>Устройство</label><select class=\"field meteo-device\">";
inline constexpr const char kInputClassFieldNameMeteoNameType[] = "<div class=\"form-row name-local\"><label>Имя</label><input class=\"field name meteo-name\" type=\"text\" name=\"m";
inline constexpr const char kSelectClassFieldNameMeteoSourceName[] = "<div class=\"form-row name-remote\" style=\"display:none;\"><label>Имя</label><select class=\"field name meteo-source\" name=\"m";
inline constexpr const char kText13[] = "Давность: ";
inline constexpr const char kSelectClassFieldMeteoTypeNameM[] = "<div class=\"form-row\"><label>Тип</label><select class=\"field meteo-type\" name=\"m";
inline constexpr const char kSelectClassFieldMiniMeteoPinData[] = "<div class=\"form-row pin-cell\"><label>Пин</label><select class=\"field mini meteo-pin\" data-selected=\"";
inline constexpr const char kSelectClassFieldAddrMeteoAddrName[] = "<div class=\"form-row addr-cell full\"><label>Адрес</label><select class=\"field addr meteo-addr\" name=\"m";
} // namespace Meteo

namespace Ring
{
inline constexpr const char kText[] = "<span class=\"muted\">Устройство</span>";
inline constexpr const char kPageTitle[] = "Звонок";
inline constexpr const char kLabelButton[] = "Кнопка (вход)";
inline constexpr const char kLabelRelay[] = "Реле";
inline constexpr const char kBtnRing[] = "Звонить";
inline constexpr const char kJsError[] = "Ошибка";
inline constexpr const char kStackControlSlave[] = "Стек: управление слейвом";
inline constexpr const char kLocalOnlySettings[] = "Настройки доступны только локально";
inline constexpr const char kInvalidButtonPort[] = "Неверный порт кнопки";
inline constexpr const char kInvalidRelayPort[] = "Неверный порт реле";
} // namespace Ring

namespace Security
{
inline constexpr const char kText[] = "Датчики";
inline constexpr const char kPageTitle[] = "Охрана";
inline constexpr const char kLabelStatus[] = "Статус";
inline constexpr const char kLabelAlarm[] = "Тревога";
inline constexpr const char kBtnArm[] = "Поставить";
inline constexpr const char kBtnDisarm[] = "Снять";
inline constexpr const char kLabelSirenPort[] = "Порт сирены";
inline constexpr const char kArmedOn[] = "под охраной";
inline constexpr const char kArmedOff[] = "снято";
inline constexpr const char kText2[] = "<tr><td colspan=\"7\" style=\"color:#94a3b8\"><strong>Контроллеры недоступны</strong></td></tr>";
inline constexpr const char kText3[] = "<tr><td colspan=\"7\" style=\"color:#94a3b8\"><strong>Датчики отсутствуют</strong></td></tr>";
inline constexpr const char kText4[] = "<div class=\"tile empty\"><strong>Контроллеры недоступны</strong></div>";
inline constexpr const char kNum[] = "Датчик #";
inline constexpr const char kText5[] = "Отключен";
inline constexpr const char kText6[] = "Сработал";
inline constexpr const char kText7[] = "Активен";
inline constexpr const char kSelectClassFieldMiniNameSec[] = "<div class=\"form-row\"><label>Тип</label><select class=\"field mini\" name=\"sec";
inline constexpr const char kSelectClassFieldMiniSecurityPortData[] = "<div class=\"form-row\"><label>Порт</label><select class=\"field mini security-port\" data-type=\"dinput\" data-selected=\"";
inline constexpr const char kInputTypeCheckboxNameSec[] = "<div class=\"form-row\"><label>Тихий</label><label class=\"switch\"><input type=\"checkbox\" name=\"sec";
inline constexpr const char kText8[] = "<div class=\"tile empty\"><strong>Датчики отсутствуют</strong></div>";
inline constexpr const char kText9[] = "<div class=\"tile empty\"><strong>Ожидаем данные со слейва</strong></div>";
inline constexpr const char kText10[] = "<div class=\"form-row\"><label>Тип</label><div class=\"field mini\">";
inline constexpr const char kText11[] = "<div class=\"form-row\"><label>Порт</label><div class=\"field mini\">";
inline constexpr const char kText12[] = "Датчик";
inline constexpr const char kText13[] = "ОК";
inline constexpr const char kInputClassFieldMiniTypeTextValue[] = "<div class=\"form-row\"><label>Тип</label><input class=\"field mini\" type=\"text\" value=\"";
inline constexpr const char kInputClassFieldMiniTypeTextValue2[] = "<div class=\"form-row\"><label>Порт</label><input class=\"field mini\" type=\"text\" value=\"";
inline constexpr const char kInputClassFieldMiniTypeTextValue3[] = "<div class=\"form-row\"><label>Тихий</label><input class=\"field mini\" type=\"text\" value=\"";
} // namespace Security

namespace Septic
{
inline constexpr const char kPageTitle[] = "Септик";
inline constexpr const char kText[] = "<div class=\"tile empty\"><strong>Ожидаем данные со слейва</strong></div>";
inline constexpr const char kText2[] = "<div class=\"tile empty\"><strong>Септик отсутствует</strong></div>";
inline constexpr const char kText20[] = "Уровень: 20%";
inline constexpr const char kText100[] = "Уровень: 100%";
inline constexpr const char kText80[] = "Уровень: 80%";
inline constexpr const char kDisabledStatus[] = "Септик выключен";
inline constexpr const char kNum[] = "</div></div><div><div class=\"tile-head\"><div><strong>Септик #";
inline constexpr const char kText3[] = " <span class=\"badge\">выкл</span>";
inline constexpr const char kText4[] = "\"></span><span>Датчик предупреждения</span></div>";
inline constexpr const char kText5[] = "\"></span><span>Датчик аварии</span></div>";
inline constexpr const char kInputTypeCheckboxClassSepticMonitorData[] = "</div><div class=\"form-row\" style=\"margin-top:8px;\"><label>Мониторинг</label><label class=\"switch\"><input type=\"checkbox\" class=\"septic-monitor\" data-action=\"sep";
inline constexpr const char kText6[] = "\"></span><span>Датчик тревоги</span></div>";
inline constexpr const char kText7[] = "<div class=\"status-line\"><span class=\"status-dot status-off\"></span><span>Реле предупреждения: н/д</span></div>";
inline constexpr const char kText8[] = "<div class=\"status-line\"><span class=\"status-dot status-off\"></span><span>Реле тревоги: н/д</span></div>";
inline constexpr const char kText9[] = "<div class=\"tile empty\"><strong>Контроллеры недоступны</strong></div>";
inline constexpr const char kSelectClassFieldMiniSepticSelectData[] = "\"><div class=\"form-grid\"><div class=\"form-row\"><label>Предупр.</label><select class=\"field mini septic-select\" data-type=\"dinput\" data-selected=\"";
inline constexpr const char kWarnSelectClassFieldMiniSepticSelect[] = "_warn\"></select></div><div class=\"form-row\"><label>Тревога</label><select class=\"field mini septic-select\" data-type=\"dinput\" data-selected=\"";
inline constexpr const char kAlarmSelectClassFieldMiniSepticSelect[] = "_alarm\"></select></div><div class=\"form-row\"><label>Реле предупр.</label><select class=\"field mini septic-select\" data-type=\"relay\" data-selected=\"";
inline constexpr const char kRelayWarnSelectClassFieldMiniSeptic[] = "_relay_warn\"></select></div><div class=\"form-row\"><label>Реле тревоги</label><select class=\"field mini septic-select\" data-type=\"relay\" data-selected=\"";
inline constexpr const char kSpanClassStatusDot[] = "\"></span><span>Датчик предупреждения</span></div><div class=\"status-line\"><span class=\"status-dot ";
inline constexpr const char kSpanClassStatusDot2[] = "\"></span><span>Датчик тревоги</span></div><div class=\"status-line\"><span class=\"status-dot ";
inline constexpr const char kSpanClassStatusDot3[] = "\"></span><span>Реле предупреждения</span></div><div class=\"status-line\"><span class=\"status-dot ";
inline constexpr const char kInputTypeCheckboxClassSepticMonitorData2[] = "\"></span><span>Реле тревоги</span></div></div><div class=\"form-row\" style=\"margin-top:8px;\"><label>Мониторинг</label><label class=\"switch\"><input type=\"checkbox\" class=\"septic-monitor\" data-action=\"sep";
} // namespace Septic

namespace Sockets
{
inline constexpr const char kText[] = "<div class=\"tile empty\"><strong>Контроллеры недоступны</strong></div>";
inline constexpr const char kText2[] = "Розетка";
inline constexpr const char kText3[] = "Включена";
inline constexpr const char kText4[] = "Выключена";
inline constexpr const char kPageTitle[] = "Розетки";
inline constexpr const char kPagePrev[] = "Назад";
inline constexpr const char kPagePage[] = "Страница";
inline constexpr const char kPageNext[] = "Вперёд";
inline constexpr const char kText5[] = "<div class=\"form-row\"><label>Кнопка</label>";
inline constexpr const char kText6[] = "<div class=\"form-row\"><label>Реле</label>";
inline constexpr const char kText7[] = "<div class=\"form-row\"><label>Перекл.</label>";
inline constexpr const char kText8[] = "<div class=\"tile empty\"><strong>Розетки отсутствуют</strong></div>";
inline constexpr const char kText9[] = "<div class=\"tile empty\"><strong>Ожидаем данные со слейва</strong></div>";
inline constexpr const char kText10[] = "Таймаут ожидания ответа";
} // namespace Sockets

namespace Tanks
{
inline constexpr const char kText[] = "<div class=\"tile empty\"><strong>Ожидаем данные со слейва</strong></div>";
inline constexpr const char kText2[] = "<div class=\"tile empty\"><strong>Баки отсутствуют</strong></div>";
inline constexpr const char kText3[] = "Бак";
inline constexpr const char kText4[] = "\"></span><span>Питание</span></div>";
inline constexpr const char kText5[] = "\"></span><span>Клапан</span></div>";
inline constexpr const char kText6[] = "\"></span><span>Насос</span></div>";
inline constexpr const char kText7[] = "\"></span><span>Авария</span></div>";
inline constexpr const char kInputTypeCheckboxClassTankPowerData[] = "<div class=\"form-row power-row\"><label>Питание</label><label class=\"switch\"><input type=\"checkbox\" class=\"tank-power\" data-action=\"k";
inline constexpr const char kText8[] = "<div class=\"tile\" style=\"color:#94a3b8\"><strong>Контроллеры недоступны</strong></div>";
inline constexpr const char kText9[] = "вкл";
inline constexpr const char kText10[] = "выкл";
inline constexpr const char kSelectClassFieldMiniTankSelectData[] = "<div class=\"form-row\"><label>Низкий</label><select class=\"field mini tank-select\" data-type=\"dinput\" data-selected=\"";
inline constexpr const char kSelectClassFieldMiniTankSelectData2[] = "<div class=\"form-row\"><label>Средний</label><select class=\"field mini tank-select\" data-type=\"dinput\" data-selected=\"";
inline constexpr const char kSelectClassFieldMiniTankSelectData3[] = "<div class=\"form-row\"><label>Полный</label><select class=\"field mini tank-select\" data-type=\"dinput\" data-selected=\"";
inline constexpr const char kSelectClassFieldMiniTankSelectData4[] = "<div class=\"form-row\"><label>Клапан</label><select class=\"field mini tank-select\" data-type=\"relay\" data-selected=\"";
inline constexpr const char kSelectClassFieldMiniTankSelectData5[] = "<div class=\"form-row\"><label>Насос</label><select class=\"field mini tank-select\" data-type=\"relay\" data-selected=\"";
inline constexpr const char kSelectClassFieldMiniTankSelectData6[] = "<div class=\"form-row\"><label>Индикатор</label><select class=\"field mini tank-select\" data-type=\"relay\" data-selected=\"";
inline constexpr const char kInputTypeCheckboxClassTankPowerData2[] = "<div class=\"form-row\"><label>Питание</label><label class=\"switch\"><input type=\"checkbox\" class=\"tank-power\" data-action=\"k";
inline constexpr const char kText11[] = "\"></span><span>Насос</span>";
inline constexpr const char kText12[] = "\"></span><span>Клапан</span>";
inline constexpr const char kText14[] = "\"></span><span>Авария</span>";
inline constexpr const char kText13[] = "<div class=\"tile\" style=\"color:#94a3b8\"><strong>Баки отсутствуют</strong></div>";
inline constexpr const char kPageTitle[] = "Баки";
inline constexpr const char kInvalidPortForTankPrefix[] = "Неверный порт для бака ";
} // namespace Tanks

namespace Leak
{
inline constexpr const char kPageTitle[] = "Протечки";
inline constexpr const char kBtnAckAll[] = "Сброс тревог";
inline constexpr const char kHelpPorts[] = "Датчики протечки: DInput. Выходы кран/тревога: Relay.";
inline constexpr const char kCmdSent[] = "Команда отправлена";
inline constexpr const char kSendFailed[] = "Ошибка отправки";
inline constexpr const char kAckDone[] = "Сброс тревог выполнен";
inline constexpr const char kSaveError[] = "Ошибка сохранения";
inline constexpr const char kInvalidSensorPort[] = "Некорректный порт датчика";
inline constexpr const char kInvalidValvePort[] = "Некорректный порт крана";
inline constexpr const char kInvalidAlarmPort[] = "Некорректный порт тревоги";
inline constexpr const char kStackCacheUnavailable[] = "Стек-кэш недоступен";
inline constexpr const char kRequestingSlaveData[] = "Запрос данных со слейва...";
inline constexpr const char kWaitingSlave[] = "Ожидаем данные со слейва";
inline constexpr const char kNoLeakZones[] = "Нет зон протечки";
inline constexpr const char kLabelZonePrefix[] = "Зона ";
inline constexpr const char kBadgeWater[] = "Вода:";
inline constexpr const char kBadgeLatch[] = "Фиксация:";
inline constexpr const char kLabelName[] = "Имя";
inline constexpr const char kLabelSensor[] = "Датчик";
inline constexpr const char kLabelValve[] = "Кран";
inline constexpr const char kLabelAlarm[] = "Тревога";
inline constexpr const char kToggleOn[] = "ВКЛ";
inline constexpr const char kTogglePower[] = "ПИТ";
inline constexpr const char kToggleActiveLow[] = "Активный ноль";
} // namespace Leak

namespace Avr
{
inline constexpr const char kPageTitle[] = "Автоматический ввод резерва (АВР)";
inline constexpr const char kBtnResetFault[] = "Сбросить ошибку";
inline constexpr const char kStackCacheUnavailable[] = "Стек-кэш недоступен";
inline constexpr const char kWaitingSlave[] = "Ожидаем данные со слейва";
inline constexpr const char kCmdSent[] = "Команда отправлена";
inline constexpr const char kSendFailed[] = "Ошибка отправки";
inline constexpr const char kFaultCleared[] = "Ошибка сброшена";
inline constexpr const char kRequestingSlaveData[] = "Запрос данных со слейва...";
inline constexpr const char kActive[] = "активный:";
inline constexpr const char kTarget[] = " цель:";
inline constexpr const char kFault[] = " ошибка:";
inline constexpr const char kSwitching[] = " переключение:";
inline constexpr const char kStateMain[] = "Основная сеть";
inline constexpr const char kStateReserve[] = "Резервная сеть";
inline constexpr const char kStateFault[] = "Ошибка";
inline constexpr const char kInvalidMainOkPort[] = "Некорректный порт main_ok";
inline constexpr const char kInvalidReserveOkPort[] = "Некорректный порт reserve_ok";
inline constexpr const char kInvalidRelayMainPort[] = "Некорректный порт relay_main";
inline constexpr const char kInvalidRelayReservePort[] = "Некорректный порт relay_reserve";
inline constexpr const char kInvalidFeedbackMainPort[] = "Некорректный порт feedback_main";
inline constexpr const char kInvalidFeedbackReservePort[] = "Некорректный порт feedback_reserve";
inline constexpr const char kInvalidDebounceMs[] = "Некорректный debounce_ms";
inline constexpr const char kInvalidLossDelayMs[] = "Некорректный loss_delay_ms";
inline constexpr const char kInvalidReturnDelayMs[] = "Некорректный return_delay_ms";
inline constexpr const char kInvalidBreakMs[] = "Некорректный break_ms";
inline constexpr const char kInvalidWarmupMs[] = "Некорректный warmup_ms";
inline constexpr const char kInvalidTransferTimeoutMs[] = "Некорректный transfer_timeout_ms";
inline constexpr const char kInvalidManualSource[] = "Некорректный manual source";
inline constexpr const char kChkEnabled[] = "Включено";
inline constexpr const char kChkAutoMode[] = "Авто режим";
inline constexpr const char kChkPreferMain[] = "Приоритет Основной сети";
inline constexpr const char kChkAutoReturnMain[] = "Автовозврат на Основную сеть";
inline constexpr const char kGroupManualSource[] = "Ручной источник (используется только в ручном режиме)";
inline constexpr const char kLabelSource[] = "Источник";
inline constexpr const char kRadioOff[] = "Отключен";
inline constexpr const char kRadioMain[] = "Основная сеть";
inline constexpr const char kRadioReserve[] = "Резервная сеть";
inline constexpr const char kGroupPorts[] = "Порты";
inline constexpr const char kPortMainOk[] = "Вход Основная сеть OK";
inline constexpr const char kPortFeedbackMain[] = "Обратная связь Основная сеть";
inline constexpr const char kPortRelayMain[] = "Реле Основная сеть";
inline constexpr const char kPortReserveOk[] = "Вход Резервная сеть OK";
inline constexpr const char kPortFeedbackReserve[] = "Обратная связь Резервная сеть";
inline constexpr const char kPortRelayReserve[] = "Реле Резервная сеть";
inline constexpr const char kGroupPortLogic[] = "Логика портов";
inline constexpr const char kAlMainOk[] = "Основная сеть OK активный ноль";
inline constexpr const char kAlReserveOk[] = "Резервная сеть OK активный ноль";
inline constexpr const char kAlFeedbackMain[] = "Обратная связь Основная сеть активный ноль";
inline constexpr const char kAlFeedbackReserve[] = "Обратная связь Резервная сеть активный ноль";
inline constexpr const char kInvRelayMain[] = "Инверсия реле Основная сеть";
inline constexpr const char kInvRelayReserve[] = "Инверсия реле Резервная сеть";
inline constexpr const char kGroupTimings[] = "Тайминги (ms)";
inline constexpr const char kTimingDebounce[] = "Дребезг";
inline constexpr const char kTimingLossDelay[] = "Задержка потери";
inline constexpr const char kTimingReturnDelay[] = "Задержка возврата";
inline constexpr const char kTimingBreak[] = "Пауза переключения";
inline constexpr const char kTimingWarmup[] = "Прогрев";
inline constexpr const char kTimingTransferTimeout[] = "Таймаут переключения";
inline constexpr const char kPortsHelp[] = "Для входов используется список DInput, для выходов - список Relay.";
} // namespace Avr

namespace ControllersPage
{
inline constexpr const char kPageTitle[] = "Контроллеры";
inline constexpr const char kNoAclControllers[] = "Нет доступных контроллеров по ACL.";
inline constexpr const char kUnavailableUntilStartup[] = "Недоступно до сохранения startup-config";
inline constexpr const char kEnabledMasc[] = "включен";
inline constexpr const char kDisabledMasc[] = "выключен";
inline constexpr const char kEnabledFem[] = "включена";
inline constexpr const char kDisabledFem[] = "выключена";
inline constexpr const char kEnabledNeut[] = "включено";
inline constexpr const char kDisabledNeut[] = "выключено";
inline constexpr const char kEnabledPlural[] = "включены";
inline constexpr const char kDisabledPlural[] = "выключены";
inline constexpr const char kUnavailable[] = "недоступно";
inline constexpr const char kSocketsTitle[] = "Розетки";
inline constexpr const char kSocketsDesc[] = "Настройка реле и кнопок";
inline constexpr const char kSocketsStatusLabel[] = "Розетки:";
inline constexpr const char kLightsTitle[] = "Свет";
inline constexpr const char kLightsDesc[] = "Настройка освещения";
inline constexpr const char kLightsStatusLabel[] = "Свет:";
inline constexpr const char kMeteoTitle[] = "Метео";
inline constexpr const char kMeteoDesc[] = "Температура и влажность";
inline constexpr const char kMeteoStatusLabel[] = "Метео:";
inline constexpr const char kThermoTitle[] = "Термо";
inline constexpr const char kThermoDesc[] = "Климат: нагрев/охлаждение/авто";
inline constexpr const char kThermoStatusLabel[] = "Термо:";
inline constexpr const char kTanksTitle[] = "Баки";
inline constexpr const char kTanksDesc[] = "Уровень воды и автоматика";
inline constexpr const char kTanksStatusLabel[] = "Баки:";
inline constexpr const char kWateringTitle[] = "Полив";
inline constexpr const char kWateringDesc[] = "Правила и расписание полива";
inline constexpr const char kWateringStatusLabel[] = "Полив:";
inline constexpr const char kSepticTitle[] = "Септик";
inline constexpr const char kSepticDesc[] = "Уровень и индикация";
inline constexpr const char kSepticStatusLabel[] = "Септик:";
inline constexpr const char kRingTitle[] = "Звонок";
inline constexpr const char kRingDesc[] = "Кнопка и импульс реле";
inline constexpr const char kRingStatusLabel[] = "Звонок:";
inline constexpr const char kSecurityTitle[] = "Охрана";
inline constexpr const char kSecurityDesc[] = "Датчики и тревога";
inline constexpr const char kSecurityStatusLabel[] = "Охрана:";
inline constexpr const char kAvrTitle[] = "АВР";
inline constexpr const char kAvrDesc[] = "Автоматический ввод резерва";
inline constexpr const char kAvrStatusLabel[] = "АВР:";
inline constexpr const char kLeakTitle[] = "Протечки";
inline constexpr const char kLeakDesc[] = "Защита от протечек";
inline constexpr const char kLeakStatusLabel[] = "Протечки:";
} // namespace ControllersPage

namespace Thermo
{
inline constexpr const char kText[] = "<div class=\"tile empty\"><strong>Ожидаем данные со слейва</strong></div>";
inline constexpr const char kText2[] = "<div class=\"tile empty\"><strong>Термо отсутствует</strong></div>";
inline constexpr const char kPageTitle[] = "Термо";
inline constexpr const char kText3[] = "авто";
inline constexpr const char kText4[] = "выкл";
inline constexpr const char kText5[] = "нагрев";
inline constexpr const char kText6[] = "охлаждение";
inline constexpr const char kText7[] = "ожидание";
inline constexpr const char kText8[] = "нет";
inline constexpr const char kC[] = "°C";
inline constexpr const char kText9[] = "\"><div class=\"thermo-left\"><div class=\"thermo-visual\"><div class=\"temp-pill sensor\">Текущая: <span class=\"temp-value\">";
inline constexpr const char kText10[] = "</span></div><div class=\"temp-pill target\">Цель: <span class=\"temp-value\">";
inline constexpr const char kText11[] = "Термо";
inline constexpr const char kText12[] = "<div class=\"status-line\"><span class=\"badge\">Питание: ";
inline constexpr const char kText13[] = "</span><span class=\"badge\">Режим: ";
inline constexpr const char kText14[] = "<div class=\"status-line\"><span class=\"badge\">Датчик: ";
inline constexpr const char kText15[] = "</div><div class=\"temp-pill target\">Цель: <span class=\"temp-value\">";
inline constexpr const char kNum[] = "</div><div><div class=\"tile-head\"><div><strong>Термо #";
inline constexpr const char kText16[] = " <span class=\"badge\">выкл</span>";
inline constexpr const char kLabelPowerBadge[] = "Питание: ";
inline constexpr const char kLabelModeBadge[] = "Режим: ";
inline constexpr const char kLabelSensorBadge[] = "Датчик: ";
inline constexpr const char kLabelActive[] = "Активн.";
inline constexpr const char kLabelStatus[] = "Статус";
inline constexpr const char kLabelMode[] = "Режим";
inline constexpr const char kLabelTarget[] = "Цель";
inline constexpr const char kLabelHyst[] = "Гист.";
inline constexpr const char kInputTypeCheckboxClassThermoPowerData[] = "><div class=\"form-grid\"><div class=\"form-row\"><label>Активн.</label><label class=\"switch\"><input type=\"checkbox\" class=\"thermo-power\" data-action=\"t";
inline constexpr const char kSelectClassFieldMiniNameT[] = "<div class=\"form-row full\"><label>Датчик</label><select class=\"field mini\" name=\"t";
inline constexpr const char kSelectClassFieldMiniNameT2[] = "</select></div><div class=\"form-row\"><label>Режим</label><select class=\"field mini\" name=\"t";
inline constexpr const char kAutoInputClassFieldTempTypeNumber[] = ">auto</option></select></div><div class=\"form-row\"><label>Цель</label><input class=\"field temp\" type=\"number\" step=\"1\" name=\"t";
inline constexpr const char kInputClassFieldTempTypeNumberStep[] = "></div><div class=\"form-row\"><label>Гист.</label><input class=\"field temp\" type=\"number\" min=\"1\" step=\"1\" name=\"t";
inline constexpr const char kSelectClassFieldMiniThermoSelectData[] = "></div><div class=\"form-row\"><label>Нагрев</label><select class=\"field mini thermo-select\" data-type=\"relay\" data-selected=\"";
inline constexpr const char kSelectClassFieldMiniThermoSelectData2[] = "></select></div><div class=\"form-row\"><label>Охлажд</label><select class=\"field mini thermo-select\" data-type=\"relay\" data-selected=\"";
inline constexpr const char kSelectClassFieldMiniThermoSelectData3[] = "></select></div><div class=\"form-row\"><label>Кнопка</label><select class=\"field mini thermo-select\" data-type=\"dinput\" data-selected=\"";
} // namespace Thermo

namespace Watering
{
inline constexpr const char kText[] = "<div class=\"tile empty\"><strong>Ожидаем данные со слейва</strong></div>";
inline constexpr const char kText2[] = "<div class=\"tile empty\"><strong>Правила отсутствуют</strong></div>";
inline constexpr const char kPageTitle[] = "Полив";
inline constexpr const char kText3[] = "активно";
inline constexpr const char kText4[] = "пауза";
inline constexpr const char kText5[] = "ожидание";
inline constexpr const char kText6[] = "<div class=\"watering-visual\"><div class=\"tile-head\"><strong>Правило ";
inline constexpr const char kText7[] = "вкл";
inline constexpr const char kText8[] = "выкл";
inline constexpr const char kText9[] = "</span></div><svg class=\"watering-icon\" viewBox=\"0 0 24 24\" fill=\"currentColor\" aria-hidden=\"true\"><path d=\"M12 2c-2.3 3.5-6 7.4-6 11a6 6 0 0 0 12 0c0-3.6-3.7-7.5-6-11zm0 18a4 4 0 0 1-4-4c0-2.2 2.3-5.2 4-7.7 1.7 2.5 4 5.5 4 7.7a4 4 0 0 1-4 4z\"/></svg><div class=\"status-line\"><span class=\"muted\">Состояние</span><span class=\"status-value\">";
inline constexpr const char kText10[] = "Правило полива";
inline constexpr const char kText11[] = "<div class=\"form-row\"><label>Монитор</label><div class=\"field mini\">";
inline constexpr const char kText12[] = "<div class=\"form-row\"><label>Кран</label><div class=\"field mini\">";
inline constexpr const char kText13[] = "<div class=\"form-row full\"><label>Дни</label><div class=\"field\">";
inline constexpr const char kText14[] = "Пн";
inline constexpr const char kText15[] = "Вт";
inline constexpr const char kText16[] = "Ср";
inline constexpr const char kText17[] = "Чт";
inline constexpr const char kText18[] = "Пт";
inline constexpr const char kText19[] = "Сб";
inline constexpr const char kText20[] = "Вс";
inline constexpr const char kText21[] = "<div class=\"form-row\"><label>Время</label><div class=\"field mini\">";
inline constexpr const char kText22[] = "<div class=\"form-row\"><label>Бак</label><div class=\"field mini\">";
inline constexpr const char kText23[] = "<div class=\"form-row\"><label>Длит. (мин)</label><div class=\"field mini\">";
inline constexpr const char kText24[] = "<div class=\"form-row\"><label>Время 2</label><div class=\"field mini\">";
inline constexpr const char kText25[] = "<div class=\"form-row\"><label>Длит.2 (мин)</label><div class=\"field mini\">";
inline constexpr const char kText32[] = "<div class=\"form-row\"><label>Время 3</label><div class=\"field mini\">";
inline constexpr const char kText33[] = "<div class=\"form-row\"><label>Длит.3 (мин)</label><div class=\"field mini\">";
inline constexpr const char kText26[] = "<div class=\"form-row\"><label>Продолжать</label><div class=\"field mini\">";
inline constexpr const char kGe[] = "<div class=\"form-row full\"><label>Уровень >=</label><div class=\"field mini\">";
inline constexpr const char kText27[] = "<div class=\"tile empty\"><strong>Контроллеры недоступны</strong></div>";
inline constexpr const char kInputTypeCheckboxNameW[] = "<div class=\"form-row\"><label>Монитор</label><label class=\"switch\"><input type=\"checkbox\" name=\"w";
inline constexpr const char kSelectClassFieldMiniWateringSelectData[] = "<div class=\"form-row\"><label>Кран</label><select class=\"field mini watering-select\" data-type=\"relay\" data-selected=\"";
inline constexpr const char kText28[] = "<div class=\"form-row full\"><label>Дни</label><div class=\"weekday-group\">";
inline constexpr const char kInputClassFieldMiniTypeTimeName[] = "<div class=\"form-row\"><label>Время</label><input class=\"field mini\" type=\"time\" name=\"w";
inline constexpr const char kInputClassFieldMiniTypeNumberMin[] = "<div class=\"form-row\"><label>Длит. (мин)</label><input class=\"field mini\" type=\"number\" min=\"1\" step=\"1\" name=\"w";
inline constexpr const char kText2InputClassFieldMiniTypeTime[] = "<div class=\"form-row\"><label>Время 2</label><input class=\"field mini\" type=\"time\" name=\"w";
inline constexpr const char kText2InputClassFieldMiniTypeNumber[] = "<div class=\"form-row\"><label>Длит.2 (мин)</label><input class=\"field mini\" type=\"number\" min=\"0\" step=\"1\" name=\"w";
inline constexpr const char kText3InputClassFieldMiniTypeTime[] = "<div class=\"form-row\"><label>Время 3</label><input class=\"field mini\" type=\"time\" name=\"w";
inline constexpr const char kText3InputClassFieldMiniTypeNumber[] = "<div class=\"form-row\"><label>Длит.3 (мин)</label><input class=\"field mini\" type=\"number\" min=\"0\" step=\"1\" name=\"w";
inline constexpr const char kSelectClassFieldMiniWateringSelectData2[] = "<div class=\"form-row\"><label>Бак</label><select class=\"field mini watering-select\" data-type=\"tank\" data-selected=\"";
inline constexpr const char kInputTypeCheckboxNameW2[] = "<div class=\"form-row tank-dependent\"><label>Продолжать</label><label class=\"switch\"><input type=\"checkbox\" name=\"w";
inline constexpr const char kGeSelectClassFieldMiniNameW[] = "<div class=\"form-row full tank-dependent resume-dependent\"><label>Уровень >=</label><select class=\"field mini\" name=\"w";
} // namespace Watering

namespace Common
{
inline constexpr const char kDevice[] = "Устройство";
inline constexpr const char kNoDataFromSlave[] = "Нет данных со слейва";
inline constexpr const char kErrorPrefix[] = "Ошибка: ";
inline constexpr const char kStatusOk[] = "ОК";
inline constexpr const char kPagePrev[] = "Назад";
inline constexpr const char kPagePage[] = "Страница";
inline constexpr const char kPageNext[] = "Вперёд";
inline constexpr const char kControllersUnavailable[] = "Контроллеры недоступны";
inline constexpr const char kConfigManagerUnavailable[] = "Менеджер конфигурации недоступен";
inline constexpr const char kSaveFailed[] = "Сохранение не удалось";
inline constexpr const char kSaved[] = "Сохранено";
inline constexpr const char kUpdated[] = "Обновлено";
inline constexpr const char kNoChanges[] = "Без изменений";
inline constexpr const char kNoChangesAlt[] = "Нет изменений";
inline constexpr const char kToastSlaveConnectedPrefix[] = "Подключен слейв: ";
inline constexpr const char kToastSlaveDisconnectedPrefix[] = "Отключен слейв: ";
inline constexpr const char kToastSlaveSyncCompletePrefix[] = "Синхронизирован слейв: ";
inline constexpr const char kSave[] = "Сохранить";
inline constexpr const char kSaveAcl[] = "Сохранить ACL";
inline constexpr const char kSaveRtc[] = "Сохранить RTC";
inline constexpr const char kSaveRule[] = "Сохранить правило";
inline constexpr const char kSaveAction[] = "Сохранить действие";
} // namespace Common

namespace Users
{
inline constexpr const char kReady[] = "Готово";
inline constexpr const char kMasterOnlyEdit[] = "Редактирование доступно только на master-устройстве";
inline constexpr const char kMasterOnlyAclEdit[] = "Редактирование ACL доступно только на master-устройстве";
inline constexpr const char kRegistryUnavailable[] = "Реестр пользователей недоступен";
inline constexpr const char kAclSaved[] = "ACL сохранен";
inline constexpr const char kAclSaveFailed[] = "Ошибка сохранения ACL";
inline constexpr const char kUsersLink[] = "Пользователи";
inline constexpr const char kUnit[] = "Юнит";
inline constexpr const char kUnitLocal[] = "Локальный";
inline constexpr const char kSelectAll[] = "Выбрать все";
inline constexpr const char kClearAll[] = "Снять все";
inline constexpr const char kAclLoadingUnit[] = "Загрузка данных юнита...";
inline constexpr const char kAclNoEnabledItems[] = "Нет enabled элементов";
inline constexpr const char kAclAll[] = "Все";
inline constexpr const char kAclNone[] = "Ничего";
inline constexpr const char kLabelUsername[] = "Имя пользователя";
inline constexpr const char kLabelPassword[] = "Пароль";
inline constexpr const char kPasswordPlaceholder[] = "оставьте пустым, чтобы не менять";
inline constexpr const char kLabelTelegramLogin[] = "Telegram логин";
inline constexpr const char kLabelAdmin[] = "Админ";
inline constexpr const char kLabelTelegramNotify[] = "Уведомления Telegram";
inline constexpr const char kLabelIButtonKey[] = "Ключ iButton";
inline constexpr const char kLabelRfidKey[] = "Ключ RFID";
inline constexpr const char kLabelPhoneGsm[] = "Телефон (GSM)";
inline constexpr const char kLabelCall[] = "Звонок";
inline constexpr const char kCtrlSockets[] = "Розетки";
inline constexpr const char kCtrlLights[] = "Свет";
inline constexpr const char kCtrlMeteo[] = "Метео";
inline constexpr const char kCtrlThermo[] = "Термо";
inline constexpr const char kCtrlTanks[] = "Баки";
inline constexpr const char kCtrlSeptic[] = "Септик";
inline constexpr const char kCtrlSecurity[] = "Охрана";
inline constexpr const char kCtrlWatering[] = "Полив";
inline constexpr const char kCtrlLeak[] = "Протечки";
inline constexpr const char kCtrlAvr[] = "АВР";
inline constexpr const char kCtrlRing[] = "Звонок";
inline constexpr const char kItemSocket[] = "Розетка";
inline constexpr const char kItemLight[] = "Свет";
inline constexpr const char kItemMeteo[] = "Метео";
inline constexpr const char kItemThermo[] = "Термо";
inline constexpr const char kItemTank[] = "Бак";
inline constexpr const char kItemSeptic[] = "Септик";
inline constexpr const char kItemSensor[] = "Сенсор";
inline constexpr const char kItemRule[] = "Правило";
inline constexpr const char kItemLeak[] = "Протечка";
} // namespace Users

namespace Rules
{
inline constexpr const char kReady[] = "Готово";
inline constexpr const char kInvalidRuleId[] = "Неверный ID правила";
inline constexpr const char kRuleNotFound[] = "Правило не найдено";
inline constexpr const char kActionNotFound[] = "Действие не найдено";
inline constexpr const char kSocketPrefix[] = "Розетка #";
inline constexpr const char kLightPrefix[] = "Свет #";
inline constexpr const char kSensorPrefix[] = "Датчик #";
inline constexpr const char kMeteoSensorPrefix[] = "Сенсор #";
inline constexpr const char kTankPrefix[] = "Бак #";
inline constexpr const char kSepticPrefix[] = "Септик #";
inline constexpr const char kSelectRule[] = "Выберите правило";
inline constexpr const char kRules[] = "Правила";
inline constexpr const char kRule[] = "Правило";
inline constexpr const char kAction[] = "Действие ";
inline constexpr const char kActionType[] = "тип: ";
inline constexpr const char kPause[] = "пауза";
inline constexpr const char kBack[] = "Назад";
inline constexpr const char kInvalidRuleActionId[] = "Неверный rule/action ID";
inline constexpr const char kLabelName[] = "Название";
inline constexpr const char kLabelConditionEnabled[] = "Условие активно";
inline constexpr const char kLabelUnit[] = "Юнит";
inline constexpr const char kLabelController[] = "Контроллер";
inline constexpr const char kLabelItem[] = "Элемент";
inline constexpr const char kLabelParameter[] = "Параметр";
inline constexpr const char kLabelOperator[] = "Оператор";
inline constexpr const char kLabelValue[] = "Значение";
inline constexpr const char kLabelType[] = "Тип";
inline constexpr const char kLabelDelayMs[] = "Задержка, мс";
inline constexpr const char kLabelState[] = "Состояние";
inline constexpr const char kActionLower[] = "действие ";
} // namespace Rules

namespace AdminPage
{
inline constexpr const char kPageTitle[] = "Система";
inline constexpr const char kStatusLabel[] = "Статус:";
inline constexpr const char kNewPassword[] = "Новый пароль";
inline constexpr const char kNewPasswordPlaceholder[] = "Введите новый пароль";
inline constexpr const char kRtcNow[] = "RTC сейчас:";
inline constexpr const char kDate[] = "Дата";
inline constexpr const char kTime[] = "Время";
inline constexpr const char kReboot[] = "Перезагрузить";
inline constexpr const char kAdminStatusAcl[] = "управление через users ACL";
} // namespace AdminPage

namespace CloudPage
{
inline constexpr const char kPageTitle[] = "Облако";
inline constexpr const char kConnected[] = "Подключен";
inline constexpr const char kDisconnected[] = "Отключен";
inline constexpr const char kFwVersion[] = "Версия FW:";
inline constexpr const char kDeviceId[] = "ID устройства:";
inline constexpr const char kEnableCloud[] = "Включить облако";
inline constexpr const char kHost[] = "Хост";
inline constexpr const char kPort[] = "Порт";
inline constexpr const char kPath[] = "Путь";
inline constexpr const char kUseSsl[] = "Использовать SSL";
inline constexpr const char kReconnectMs[] = "Переподключение (мс)";
inline constexpr const char kEventMs[] = "Интервал событий (мс)";
inline constexpr const char kEventHint[] = "0 - отключить авто-события";
inline constexpr const char kApiKey[] = "API ключ";
} // namespace CloudPage

namespace WifiPage
{
inline constexpr const char kPageTitle[] = "Сеть";
inline constexpr const char kMode[] = "Режим";
inline constexpr const char kPassword[] = "Пароль";
inline constexpr const char kStaPasswordPlaceholder[] = "Введите пароль для STA";
inline constexpr const char kApPassword[] = "Пароль AP";
inline constexpr const char kApPasswordPlaceholder[] = "Введите пароль для AP";
inline constexpr const char kEnabled[] = "Включен";
inline constexpr const char kState[] = "Состояние";
inline constexpr const char kOperator[] = "Оператор";
inline constexpr const char kSignal[] = "Сигнал";
inline constexpr const char kRegistration[] = "Регистрация";
inline constexpr const char kError[] = "Ошибка";
inline constexpr const char kLastUrc[] = "Последний URC";
inline constexpr const char kLastSms[] = "Последний SMS";
inline constexpr const char kLastCall[] = "Последний звонок";
inline constexpr const char kLastUssd[] = "Последний USSD";
inline constexpr const char kUnavailable[] = "недоступно";
inline constexpr const char kOn[] = "включен";
inline constexpr const char kOff[] = "выключен";
inline constexpr const char kStarted[] = "инициализирован";
inline constexpr const char kNotStarted[] = "не инициализирован";
inline constexpr const char kGsmTitle[] = "GSM";
inline constexpr const char kStaStatusRowLabel[] = "STA";
} // namespace WifiPage

namespace DisplayPage
{
inline constexpr const char kPageTitle[] = "Дисплей";
inline constexpr const char kTitle[] = "Дисплей (LCD1602)";
inline constexpr const char kSrcTime[] = "Время";
inline constexpr const char kSrcSocket[] = "Розетка";
inline constexpr const char kSrcLight[] = "Свет";
inline constexpr const char kSrcMeteo[] = "Метео";
inline constexpr const char kSrcThermo[] = "Термо";
inline constexpr const char kSrcTank[] = "Бак";
inline constexpr const char kSrcSeptic[] = "Септик";
inline constexpr const char kSrcSecurity[] = "Охрана";
inline constexpr const char kSrcAvr[] = "АВР";
inline constexpr const char kSrcLeak[] = "Протечки";
inline constexpr const char kSrcText[] = "Текст";
inline constexpr const char kFieldState[] = "Состояние";
inline constexpr const char kFieldTemp[] = "Темп";
inline constexpr const char kFieldHum[] = "Влажн";
inline constexpr const char kFieldStatus[] = "Статус";
inline constexpr const char kFieldLevel[] = "Уровень";
inline constexpr const char kFieldSource[] = "Источник";
inline constexpr const char kFieldMainPower[] = "Основная сеть";
inline constexpr const char kFieldReservePower[] = "Резервная сеть";
inline constexpr const char kFieldText[] = "Текст";
} // namespace DisplayPage

namespace PortsPage
{
inline constexpr const char kPageTitle[] = "Порты";
inline constexpr const char kTitle[] = "Порты";
inline constexpr const char kExtenders[] = "Расширители";
inline constexpr const char kPorts[] = "Порты";
inline constexpr const char kBus[] = "Шина";
inline constexpr const char kAddress[] = "Адрес";
inline constexpr const char kType[] = "Тип";
inline constexpr const char kBackend[] = "Бекенд";
inline constexpr const char kLocalShort[] = "Лок.";
inline constexpr const char kControllerShort[] = "Контр.";
inline constexpr const char kDeviceShort[] = "Устр.";
inline constexpr const char kPin[] = "Пин";
} // namespace PortsPage

namespace StackPage
{
inline constexpr const char kPageTitle[] = "Стек";
inline constexpr const char kTitle[] = "Стек";
inline constexpr const char kRole[] = "Роль";
inline constexpr const char kMasterHost[] = "Хост/IP мастера";
inline constexpr const char kFallbackMaster[] = "Резервный мастер";
inline constexpr const char kEnable[] = "Включить";
inline constexpr const char kFallbackHost[] = "Хост/IP резервного мастера";
inline constexpr const char kSlaveController[] = "Слейв-контроллер";
inline constexpr const char kFullController[] = "Полноценный контроллер";
inline constexpr const char kApiKey[] = "API ключ";
inline constexpr const char kApiKeyPlaceholder[] = "необязательно";
inline constexpr const char kGenerate[] = "Сгенерировать";
inline constexpr const char kSlaveLinkDisconnected[] = "Связь со слейвом: нет";
inline constexpr const char kSlaveLinkConnected[] = "Связь со слейвом: есть";
inline constexpr const char kSlaveLinkWaitingHello[] = "Связь со слейвом: есть, ожидание hello";
inline constexpr const char kMasterLinkDisconnected[] = "Связь с мастером: нет";
inline constexpr const char kMasterLinkConnected[] = "Связь с мастером: есть";
inline constexpr const char kMasterLinkWaitingHello[] = "Связь с мастером: есть, ожидание hello";
inline constexpr const char kCurrentControllerPrefix[] = "Текущий контроллер";
} // namespace StackPage

namespace ManagePage
{
inline constexpr const char kPageTitle[] = "Прошивка и файлы";
inline constexpr const char kTitle[] = "Прошивка и файлы";
inline constexpr const char kFwSection[] = "Прошивка";
inline constexpr const char kFwHint[] = "OTA загрузка (.bin)";
inline constexpr const char kUploadFw[] = "Загрузить прошивку";
inline constexpr const char kFilesSection[] = "Файлы";
inline constexpr const char kUploadFile[] = "Загрузить файл";
inline constexpr const char kStatusLink[] = "Статус";
inline constexpr const char kFileList[] = "Список файлов";
inline constexpr const char kName[] = "Имя";
inline constexpr const char kSize[] = "Размер";
inline constexpr const char kDelete[] = "Удалить";
} // namespace ManagePage

namespace StatusPage
{
inline constexpr const char kPageTitle[] = "Статус";
inline constexpr const char kTitle[] = "Статус";
inline constexpr const char kNoData[] = "Нет данных";
} // namespace StatusPage

namespace BusesPage
{
inline constexpr const char kPageTitle[] = "Шины";
inline constexpr const char kTitle[] = "Шины";
inline constexpr const char kScanI2C[] = "Сканировать I2C";
inline constexpr const char kScanOw[] = "Сканировать OW";
} // namespace BusesPage

namespace TelegramPage
{
inline constexpr const char kClient[] = "Клиент:";
inline constexpr const char kAccess[] = "Доступ";
inline constexpr const char kLastChatId[] = "Последний Chat ID:";
inline constexpr const char kUseProxy[] = "Использовать proxy";
inline constexpr const char kUnknown[] = "неизвестно";
} // namespace TelegramPage

namespace WebCore
{
inline constexpr const char kExtendersAbsent[] = "Extenders отсутствуют";
inline constexpr const char kRingWebButton[] = "Звонок: веб-кнопка";
inline constexpr const char kStackShort[] = "стек";
inline constexpr const char kLocalShort[] = "локально";
inline constexpr const char kEnabled[] = "включен";
inline constexpr const char kDisabled[] = "выключен";
inline constexpr const char kUnavailable[] = "недоступно";
inline constexpr const char kOnShort[] = "вкл";
inline constexpr const char kOffShort[] = "выкл";
inline constexpr const char kEnablePrefix[] = "включ";
inline constexpr const char kArmedPhrase[] = "под охраной";
} // namespace WebCore

inline constexpr const char *kDevice = Common::kDevice;
inline constexpr const char *kNoDataFromSlave = Common::kNoDataFromSlave;
inline constexpr const char *kErrorPrefix = Common::kErrorPrefix;
inline constexpr const char *kStatusOk = Common::kStatusOk;
inline constexpr const char *kPagePrev = Common::kPagePrev;
inline constexpr const char *kPagePage = Common::kPagePage;
inline constexpr const char *kPageNext = Common::kPageNext;
inline constexpr const char *kControllersUnavailable = Common::kControllersUnavailable;
inline constexpr const char *kConfigManagerUnavailable = Common::kConfigManagerUnavailable;
inline constexpr const char *kSaveFailed = Common::kSaveFailed;
inline constexpr const char *kSaved = Common::kSaved;
inline constexpr const char *kUpdated = Common::kUpdated;
inline constexpr const char *kNoChanges = Common::kNoChanges;
inline constexpr const char *kNoChangesAlt = Common::kNoChangesAlt;
inline constexpr const char *kSave = Common::kSave;
inline constexpr const char *kSaveAcl = Common::kSaveAcl;
inline constexpr const char *kSaveRtc = Common::kSaveRtc;
inline constexpr const char *kSaveRule = Common::kSaveRule;
inline constexpr const char *kSaveAction = Common::kSaveAction;
} // namespace WebUiRu
