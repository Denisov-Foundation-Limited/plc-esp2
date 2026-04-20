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

#include "core/network/web/interfaces/web_interface_texts_ru.hpp"

namespace WebUiRu
{
namespace Controllers
{
const char kI2c[] = "I2C ошибка: ";
const char kOw[] = "OW ошибка: ";
const char kPorts[] = "Ports ошибка: ";
const char kExtenders[] = "Extenders ошибка: ";
const char kText[] = "<h2>Контроллеры</h2>";
const char kUnitDevicenameNodeidIp[] = "<th>Unit</th><th>DeviceName</th><th>NodeID</th><th>IP</th><th>Тип</th>";
const char kText2[] = "<p class=\"status\">Слейвы не подключены</p>";
const char kIpRtcCpu[] = "<th>Узел</th><th>IP</th><th>Дата</th><th>Время</th><th>RTC</th><th>Плата</th><th>CPU</th><th>Вент.</th>";
const char kText3[] = "<tr><td colspan=\"4\" style=\"color:#94a3b8\"><strong>Стек недоступен</strong></td></tr>";
const char kText4[] = "<tr><td colspan=\"4\" style=\"color:#94a3b8\"><strong>Контроллеров нет</strong></td></tr>";
const char kText5[] = "Контроллер";
const char kText6[] = "Модуль";
const char kFcplc[] = "<div class=\"nav\"><a href=\"/\">FCPLC</a> | <a href=\"/controllers\">Контроллеры</a>";
const char kText7[] = " | <a href=\"/wifi\">Сеть</a>";
const char kText8[] = " | <a href=\"/manage\">Прошивка и файлы</a> | <a href=\"/ports\">Порты</a> | <a href=\"/buses\">Шины</a>";
const char kText9[] = " | <a href=\"/stack\">Стек</a> | <a href=\"/users\">Пользователи</a> | <a href=\"/display\">Дисплей</a>";
const char kText10[] = " | <a href=\"/rules\">Правила</a>";
const char kTelegram[] = " | <a href=\"/cloud\">Облако</a>";
const char kLogs[] = " | <a href=\"/admin\">Система</a> | <a href=\"/logs\">Logs</a>";
} // namespace Controllers

namespace Display
{
const char kSelectClassFieldSlotKindNameDs[] = "<label>Источник</label><select class=\"field slot-kind\" name=\"ds";
const char kSelectClassFieldSlotNodeNameDs[] = "<label>Устройство</label><select class=\"field slot-node\" name=\"ds";
const char kSelectClassFieldSlotIndexNameDs[] = "<label>Объект</label><select class=\"field slot-index\" name=\"ds";
const char kSelectClassFieldSlotFieldNameDs[] = "<label>Параметр</label><select class=\"field slot-field\" name=\"ds";
const char kInputClassFieldSlotTextTypeText[] = "<label>Текст</label><input class=\"field slot-text\" type=\"text\" name=\"ds";
} // namespace Display

namespace Index
{
const char kBoard[] = "Плата";
const char kDeviceName[] = "Имя устройства";
const char kStatus[] = "Статус";
const char kDate[] = "Дата";
const char kTime[] = "Время";
const char kRtcTemp[] = "RTC температура";
const char kBoardTemp[] = "Температура платы";
const char kFan[] = "Вентилятор";
} // namespace Index

namespace Lights
{
const char kText[] = "<div class=\"tile empty\"><strong>Контроллеры недоступны</strong></div>";
const char kText2[] = "Свет";
const char kTitlePrefix[] = "Свет #";
const char kLabelName[] = "Имя";
const char kText3[] = "Включена";
const char kText4[] = "Выключена";
const char kPageTitle[] = "Свет";
const char kPagePrev[] = "Назад";
const char kPagePage[] = "Страница";
const char kPageNext[] = "Вперёд";
const char kText5[] = "<div class=\"form-row\"><label>Кнопка</label>";
const char kText6[] = "<div class=\"form-row\"><label>Реле</label>";
const char kText7[] = "<div class=\"form-row\"><label>Перекл.</label>";
const char kText8[] = "<div class=\"tile empty\"><strong>Свет отсутствует</strong></div>";
const char kText9[] = "<div class=\"tile empty\"><strong>Ожидаем данные со слейва</strong></div>";
} // namespace Lights

namespace Meteo
{
const char kText[] = "<div class=\"tile empty\"><strong>Ожидаем данные со слейва</strong></div>";
const char kText2[] = "<div class=\"tile empty\"><strong>Датчики отсутствуют</strong></div>";
const char kPageTitle[] = "Метео";
const char kTitlePrefix[] = "Датчик #";
const char kPagePrev[] = "Назад";
const char kPagePage[] = "Страница";
const char kPageNext[] = "Вперёд";
const char kC[] = "</div><div class=\"sensor-unit\">°C</div>";
const char kText3[] = "Датчик";
const char kText4[] = "Отключен";
const char kText5[] = "Нет данных";
const char kText6[] = "ОК";
const char kText7[] = "Ошибка";
const char kText14[] = "Доступно только на локальном устройстве";
const char kText8[] = "<div class=\"form-row\"><label>Тип</label><div class=\"field mini\">";
const char kText9[] = "<div class=\"form-row\"><label>Пин</label><div class=\"field mini\">";
const char kText10[] = "<div class=\"form-row full\"><label>Адрес</label><div class=\"field\">";
const char kText11[] = "<div class=\"tile empty\"><strong>Метео недоступно</strong></div>";
const char kText12[] = "<div class=\"form-row full\" style=\"margin-bottom:8px;\"><label>Устройство</label><select class=\"field meteo-device\">";
const char kInputClassFieldNameMeteoNameType[] = "<div class=\"form-row name-local\"><label>Имя</label><input class=\"field name meteo-name\" type=\"text\" name=\"m";
const char kSelectClassFieldNameMeteoSourceName[] = "<div class=\"form-row name-remote\" style=\"display:none;\"><label>Имя</label><select class=\"field name meteo-source\" name=\"m";
const char kText13[] = "Давность: ";
const char kSelectClassFieldMeteoTypeNameM[] = "<div class=\"form-row full\"><label>Тип</label><select class=\"field meteo-type\" name=\"m";
const char kSelectClassFieldMiniMeteoPinData[] = "<div class=\"form-row pin-cell full\"><label>Пин</label><select class=\"field meteo-pin\" data-selected=\"";
const char kSelectClassFieldAddrMeteoAddrName[] = "<div class=\"form-row addr-cell full\"><label>Адрес</label><select class=\"field addr meteo-addr\" name=\"m";
} // namespace Meteo

namespace Ring
{
const char kText[] = "<span class=\"muted\">Устройство</span>";
const char kPageTitle[] = "Звонок";
const char kLabelButton[] = "Кнопка (вход)";
const char kLabelRelay[] = "Реле";
const char kBtnRing[] = "Звонить";
const char kJsError[] = "Ошибка";
const char kStackControlSlave[] = "Стек: управление слейвом";
const char kLocalOnlySettings[] = "Настройки доступны только локально";
const char kInvalidButtonPort[] = "Неверный порт кнопки";
const char kInvalidRelayPort[] = "Неверный порт реле";
} // namespace Ring

namespace Security
{
const char kText[] = "Датчики";
const char kPageTitle[] = "Охрана";
const char kLabelName[] = "Имя";
const char kLabelStatus[] = "Статус";
const char kLabelAlarm[] = "Тревога";
const char kBtnArm[] = "Поставить";
const char kBtnDisarm[] = "Снять";
const char kLabelSirenPort[] = "Порт сирены";
const char kArmedOn[] = "под охраной";
const char kArmedOff[] = "снято";
const char kText2[] = "<tr><td colspan=\"7\" style=\"color:#94a3b8\"><strong>Контроллеры недоступны</strong></td></tr>";
const char kText3[] = "<tr><td colspan=\"7\" style=\"color:#94a3b8\"><strong>Датчики отсутствуют</strong></td></tr>";
const char kText4[] = "<div class=\"tile empty\"><strong>Контроллеры недоступны</strong></div>";
const char kNum[] = "Датчик #";
const char kText5[] = "Отключен";
const char kText6[] = "Сработал";
const char kText7[] = "Активен";
const char kSelectClassFieldMiniNameSec[] = "<div class=\"form-row\"><label>Тип</label><select class=\"field mini\" name=\"sec";
const char kSelectClassFieldMiniSecurityPortData[] = "<div class=\"form-row\"><label>Порт</label><select class=\"field mini security-port\" data-type=\"dinput\" data-selected=\"";
const char kInputTypeCheckboxNameSec[] = "<div class=\"form-row\"><label>Тихий</label><label class=\"switch\"><input type=\"checkbox\" name=\"sec";
const char kText8[] = "<div class=\"tile empty\"><strong>Датчики отсутствуют</strong></div>";
const char kText9[] = "<div class=\"tile empty\"><strong>Ожидаем данные со слейва</strong></div>";
const char kText10[] = "<div class=\"form-row\"><label>Тип</label><div class=\"field mini\">";
const char kText11[] = "<div class=\"form-row\"><label>Порт</label><div class=\"field mini\">";
const char kText12[] = "Датчик";
const char kText13[] = "ОК";
const char kInputClassFieldMiniTypeTextValue[] = "<div class=\"form-row\"><label>Тип</label><input class=\"field mini\" type=\"text\" value=\"";
const char kInputClassFieldMiniTypeTextValue2[] = "<div class=\"form-row\"><label>Порт</label><input class=\"field mini\" type=\"text\" value=\"";
const char kInputClassFieldMiniTypeTextValue3[] = "<div class=\"form-row\"><label>Тихий</label><input class=\"field mini\" type=\"text\" value=\"";
} // namespace Security

namespace Septic
{
const char kPageTitle[] = "Септик";
const char kLabelName[] = "Имя";
const char kText[] = "<div class=\"tile empty\"><strong>Ожидаем данные со слейва</strong></div>";
const char kText2[] = "<div class=\"tile empty\"><strong>Септик отсутствует</strong></div>";
const char kText20[] = "Уровень: 20%";
const char kText100[] = "Уровень: 100%";
const char kText80[] = "Уровень: 80%";
const char kDisabledStatus[] = "Септик выключен";
const char kNum[] = "</div></div><div><div class=\"tile-head\"><div><strong>Септик #";
const char kText3[] = " <span class=\"badge\">выкл</span>";
const char kText4[] = "\"></span><span>Датчик предупреждения</span></div>";
const char kText5[] = "\"></span><span>Датчик аварии</span></div>";
const char kInputTypeCheckboxClassSepticMonitorData[] = "</div><div class=\"form-row\" style=\"margin-top:8px;\"><label>Мониторинг</label><label class=\"switch\"><input type=\"checkbox\" class=\"septic-monitor\" data-action=\"sep";
const char kText6[] = "\"></span><span>Датчик тревоги</span></div>";
const char kText7[] = "<div class=\"status-line\"><span class=\"status-dot status-off\"></span><span>Реле предупреждения: н/д</span></div>";
const char kText8[] = "<div class=\"status-line\"><span class=\"status-dot status-off\"></span><span>Реле тревоги: н/д</span></div>";
const char kText9[] = "<div class=\"tile empty\"><strong>Контроллеры недоступны</strong></div>";
const char kSelectClassFieldMiniSepticSelectData[] = "\"><div class=\"form-grid\"><div class=\"form-row\"><label>Предупр.</label><select class=\"field mini septic-select\" data-type=\"dinput\" data-selected=\"";
const char kWarnSelectClassFieldMiniSepticSelect[] = "_warn\"></select></div><div class=\"form-row\"><label>Тревога</label><select class=\"field mini septic-select\" data-type=\"dinput\" data-selected=\"";
const char kAlarmSelectClassFieldMiniSepticSelect[] = "_alarm\"></select></div><div class=\"form-row\"><label>Реле предупр.</label><select class=\"field mini septic-select\" data-type=\"relay\" data-selected=\"";
const char kRelayWarnSelectClassFieldMiniSeptic[] = "_relay_warn\"></select></div><div class=\"form-row\"><label>Реле тревоги</label><select class=\"field mini septic-select\" data-type=\"relay\" data-selected=\"";
const char kSpanClassStatusDot[] = "\"></span><span>Датчик предупреждения</span></div><div class=\"status-line\"><span class=\"status-dot ";
const char kSpanClassStatusDot2[] = "\"></span><span>Датчик тревоги</span></div><div class=\"status-line\"><span class=\"status-dot ";
const char kSpanClassStatusDot3[] = "\"></span><span>Реле предупреждения</span></div><div class=\"status-line\"><span class=\"status-dot ";
const char kInputTypeCheckboxClassSepticMonitorData2[] = "\"></span><span>Реле тревоги</span></div></div><div class=\"form-row\" style=\"margin-top:8px;\"><label>Мониторинг</label><label class=\"switch\"><input type=\"checkbox\" class=\"septic-monitor\" data-action=\"sep";
} // namespace Septic

namespace Sockets
{
const char kText[] = "<div class=\"tile empty\"><strong>Контроллеры недоступны</strong></div>";
const char kText2[] = "Розетка";
const char kTitlePrefix[] = "Розетка #";
const char kLabelName[] = "Имя";
const char kText3[] = "Включена";
const char kText4[] = "Выключена";
const char kPageTitle[] = "Розетки";
const char kPagePrev[] = "Назад";
const char kPagePage[] = "Страница";
const char kPageNext[] = "Вперёд";
const char kText5[] = "<div class=\"form-row\"><label>Кнопка</label>";
const char kText6[] = "<div class=\"form-row\"><label>Реле</label>";
const char kText7[] = "<div class=\"form-row\"><label>Перекл.</label>";
const char kText8[] = "<div class=\"tile empty\"><strong>Розетки отсутствуют</strong></div>";
const char kText9[] = "<div class=\"tile empty\"><strong>Ожидаем данные со слейва</strong></div>";
const char kText10[] = "Таймаут ожидания ответа";
} // namespace Sockets

namespace Tanks
{
const char kText[] = "<div class=\"tile empty\"><strong>Ожидаем данные со слейва</strong></div>";
const char kText2[] = "<div class=\"tile empty\"><strong>Баки отсутствуют</strong></div>";
const char kText3[] = "Бак";
const char kTitlePrefix[] = "Бак #";
const char kLabelName[] = "Имя";
const char kText4[] = "\"></span><span>Питание</span></div>";
const char kText5[] = "\"></span><span>Клапан</span></div>";
const char kText6[] = "\"></span><span>Насос</span></div>";
const char kText7[] = "\"></span><span>Авария</span></div>";
const char kInputTypeCheckboxClassTankPowerData[] = "<div class=\"form-row power-row\"><label>Питание</label><label class=\"switch\"><input type=\"checkbox\" class=\"tank-power\" data-action=\"k";
const char kText8[] = "<div class=\"tile\" style=\"color:#94a3b8\"><strong>Контроллеры недоступны</strong></div>";
const char kText9[] = "вкл";
const char kText10[] = "выкл";
const char kSelectClassFieldMiniTankSelectData[] = "<div class=\"form-row\"><label>Низкий</label><select class=\"field mini tank-select\" data-type=\"dinput\" data-selected=\"";
const char kSelectClassFieldMiniTankSelectData2[] = "<div class=\"form-row\"><label>Средний</label><select class=\"field mini tank-select\" data-type=\"dinput\" data-selected=\"";
const char kSelectClassFieldMiniTankSelectData3[] = "<div class=\"form-row\"><label>Полный</label><select class=\"field mini tank-select\" data-type=\"dinput\" data-selected=\"";
const char kSelectClassFieldMiniTankSelectData4[] = "<div class=\"form-row\"><label>Клапан</label><select class=\"field mini tank-select\" data-type=\"relay\" data-selected=\"";
const char kSelectClassFieldMiniTankSelectData5[] = "<div class=\"form-row\"><label>Насос</label><select class=\"field mini tank-select\" data-type=\"relay\" data-selected=\"";
const char kSelectClassFieldMiniTankSelectData6[] = "<div class=\"form-row\"><label>Индикатор</label><select class=\"field mini tank-select\" data-type=\"relay\" data-selected=\"";
const char kInputTypeCheckboxClassTankPowerData2[] = "<div class=\"form-row\"><label>Питание</label><label class=\"switch\"><input type=\"checkbox\" class=\"tank-power\" data-action=\"k";
const char kText11[] = "\"></span><span>Насос</span>";
const char kText12[] = "\"></span><span>Клапан</span>";
const char kText14[] = "\"></span><span>Авария</span>";
const char kText13[] = "<div class=\"tile\" style=\"color:#94a3b8\"><strong>Баки отсутствуют</strong></div>";
const char kPageTitle[] = "Баки";
const char kInvalidPortForTankPrefix[] = "Неверный порт для бака ";
} // namespace Tanks

namespace Leak
{
const char kPageTitle[] = "Протечки";
const char kBtnAckAll[] = "Сброс тревог";
const char kHelpPorts[] = "Датчики протечки: DInput. Выходы кран/тревога: Relay.";
const char kCmdSent[] = "Команда отправлена";
const char kSendFailed[] = "Ошибка отправки";
const char kAckDone[] = "Сброс тревог выполнен";
const char kSaveError[] = "Ошибка сохранения";
const char kInvalidSensorPort[] = "Некорректный порт датчика";
const char kInvalidValvePort[] = "Некорректный порт крана";
const char kInvalidAlarmPort[] = "Некорректный порт тревоги";
const char kStackCacheUnavailable[] = "Стек-кэш недоступен";
const char kRequestingSlaveData[] = "Запрос данных со слейва...";
const char kWaitingSlave[] = "Ожидаем данные со слейва";
const char kNoLeakZones[] = "Нет зон протечки";
const char kLabelZonePrefix[] = "Зона ";
const char kBadgeWater[] = "Вода:";
const char kBadgeLatch[] = "Фиксация:";
const char kLabelName[] = "Имя";
const char kLabelSensor[] = "Датчик";
const char kLabelValve[] = "Кран";
const char kLabelAlarm[] = "Тревога";
const char kToggleOn[] = "ВКЛ";
const char kTogglePower[] = "ПИТ";
const char kToggleActiveLow[] = "Активный ноль";
} // namespace Leak

namespace Avr
{
const char kPageTitle[] = "Автоматический ввод резерва (АВР)";
const char kBtnResetFault[] = "Сбросить ошибку";
const char kStackCacheUnavailable[] = "Стек-кэш недоступен";
const char kWaitingSlave[] = "Ожидаем данные со слейва";
const char kCmdSent[] = "Команда отправлена";
const char kSendFailed[] = "Ошибка отправки";
const char kFaultCleared[] = "Ошибка сброшена";
const char kRequestingSlaveData[] = "Запрос данных со слейва...";
const char kActive[] = "активный:";
const char kTarget[] = " цель:";
const char kFault[] = " ошибка:";
const char kSwitching[] = " переключение:";
const char kStateMain[] = "Основная сеть";
const char kStateReserve[] = "Резервная сеть";
const char kStateFault[] = "Ошибка";
const char kInvalidMainOkPort[] = "Некорректный порт main_ok";
const char kInvalidReserveOkPort[] = "Некорректный порт reserve_ok";
const char kInvalidRelayMainPort[] = "Некорректный порт relay_main";
const char kInvalidRelayReservePort[] = "Некорректный порт relay_reserve";
const char kInvalidFeedbackMainPort[] = "Некорректный порт feedback_main";
const char kInvalidFeedbackReservePort[] = "Некорректный порт feedback_reserve";
const char kInvalidDebounceMs[] = "Некорректный debounce_ms";
const char kInvalidLossDelayMs[] = "Некорректный loss_delay_ms";
const char kInvalidReturnDelayMs[] = "Некорректный return_delay_ms";
const char kInvalidBreakMs[] = "Некорректный break_ms";
const char kInvalidWarmupMs[] = "Некорректный warmup_ms";
const char kInvalidTransferTimeoutMs[] = "Некорректный transfer_timeout_ms";
const char kInvalidManualSource[] = "Некорректный manual source";
const char kChkEnabled[] = "Включено";
const char kChkAutoMode[] = "Авто режим";
const char kChkPreferMain[] = "Приоритет Основной сети";
const char kChkAutoReturnMain[] = "Автовозврат на Основную сеть";
const char kGroupManualSource[] = "Ручной источник (используется только в ручном режиме)";
const char kLabelSource[] = "Источник";
const char kRadioOff[] = "Отключен";
const char kRadioMain[] = "Основная сеть";
const char kRadioReserve[] = "Резервная сеть";
const char kGroupPorts[] = "Порты";
const char kPortMainOk[] = "Вход Основная сеть OK";
const char kPortFeedbackMain[] = "Обратная связь Основная сеть";
const char kPortRelayMain[] = "Реле Основная сеть";
const char kPortReserveOk[] = "Вход Резервная сеть OK";
const char kPortFeedbackReserve[] = "Обратная связь Резервная сеть";
const char kPortRelayReserve[] = "Реле Резервная сеть";
const char kGroupPortLogic[] = "Логика портов";
const char kAlMainOk[] = "Основная сеть OK активный ноль";
const char kAlReserveOk[] = "Резервная сеть OK активный ноль";
const char kAlFeedbackMain[] = "Обратная связь Основная сеть активный ноль";
const char kAlFeedbackReserve[] = "Обратная связь Резервная сеть активный ноль";
const char kInvRelayMain[] = "Инверсия реле Основная сеть";
const char kInvRelayReserve[] = "Инверсия реле Резервная сеть";
const char kGroupTimings[] = "Тайминги (ms)";
const char kTimingDebounce[] = "Дребезг";
const char kTimingLossDelay[] = "Задержка потери";
const char kTimingReturnDelay[] = "Задержка возврата";
const char kTimingBreak[] = "Пауза переключения";
const char kTimingWarmup[] = "Прогрев";
const char kTimingTransferTimeout[] = "Таймаут переключения";
const char kPortsHelp[] = "Для входов используется список DInput, для выходов - список Relay.";
} // namespace Avr

namespace ControllersPage
{
const char kPageTitle[] = "Контроллеры";
const char kNoAclControllers[] = "Нет доступных контроллеров по ACL.";
const char kUnavailableUntilStartup[] = "Недоступно до сохранения startup-config";
const char kEnabledMasc[] = "включен";
const char kDisabledMasc[] = "выключен";
const char kEnabledFem[] = "включена";
const char kDisabledFem[] = "выключена";
const char kEnabledNeut[] = "включено";
const char kDisabledNeut[] = "выключено";
const char kEnabledPlural[] = "включены";
const char kDisabledPlural[] = "выключены";
const char kUnavailable[] = "недоступно";
const char kSocketsTitle[] = "Розетки";
const char kSocketsDesc[] = "Настройка реле и кнопок";
const char kSocketsStatusLabel[] = "Розетки:";
const char kLightsTitle[] = "Свет";
const char kLightsDesc[] = "Настройка освещения";
const char kLightsStatusLabel[] = "Свет:";
const char kMeteoTitle[] = "Метео";
const char kMeteoDesc[] = "Температура и влажность";
const char kMeteoStatusLabel[] = "Метео:";
const char kThermoTitle[] = "Термо";
const char kThermoDesc[] = "Климат: нагрев/охлаждение/авто";
const char kThermoStatusLabel[] = "Термо:";
const char kTanksTitle[] = "Баки";
const char kTanksDesc[] = "Уровень воды и автоматика";
const char kTanksStatusLabel[] = "Баки:";
const char kWateringTitle[] = "Полив";
const char kWateringDesc[] = "Правила и расписание полива";
const char kWateringStatusLabel[] = "Полив:";
const char kSepticTitle[] = "Септик";
const char kSepticDesc[] = "Уровень и индикация";
const char kSepticStatusLabel[] = "Септик:";
const char kRingTitle[] = "Звонок";
const char kRingDesc[] = "Кнопка и импульс реле";
const char kRingStatusLabel[] = "Звонок:";
const char kSecurityTitle[] = "Охрана";
const char kSecurityDesc[] = "Датчики и тревога";
const char kSecurityStatusLabel[] = "Охрана:";
const char kAvrTitle[] = "АВР";
const char kAvrDesc[] = "Автоматический ввод резерва";
const char kAvrStatusLabel[] = "АВР:";
const char kLeakTitle[] = "Протечки";
const char kLeakDesc[] = "Защита от протечек";
const char kLeakStatusLabel[] = "Протечки:";
} // namespace ControllersPage

namespace Thermo
{
const char kText[] = "<div class=\"tile empty\"><strong>Ожидаем данные со слейва</strong></div>";
const char kText2[] = "<div class=\"tile empty\"><strong>Термо отсутствует</strong></div>";
const char kPageTitle[] = "Термо";
const char kLabelName[] = "Имя";
const char kText3[] = "авто";
const char kText4[] = "выкл";
const char kText5[] = "нагрев";
const char kText6[] = "охлаждение";
const char kText7[] = "ожидание";
const char kText8[] = "нет";
const char kC[] = "°C";
const char kText9[] = "\"><div class=\"thermo-left\"><div class=\"thermo-visual\"><div class=\"temp-pill sensor\">Текущая: <span class=\"temp-value\">";
const char kText10[] = "</span></div><div class=\"temp-pill target\">Цель: <span class=\"temp-value\">";
const char kText11[] = "Термо";
const char kText12[] = "<div class=\"status-line\"><span class=\"badge\">Питание: ";
const char kText13[] = "</span><span class=\"badge\">Режим: ";
const char kText14[] = "<div class=\"status-line\"><span class=\"badge\">Датчик: ";
const char kText15[] = "</div><div class=\"temp-pill target\">Цель: <span class=\"temp-value\">";
const char kNum[] = "</div><div><div class=\"tile-head\"><div><strong>Термо #";
const char kText16[] = " <span class=\"badge\">выкл</span>";
const char kLabelPowerBadge[] = "Питание: ";
const char kLabelModeBadge[] = "Режим: ";
const char kLabelSensorBadge[] = "Датчик: ";
const char kLabelActive[] = "Активн.";
const char kLabelStatus[] = "Статус";
const char kLabelMode[] = "Режим";
const char kLabelTarget[] = "Цель";
const char kLabelHyst[] = "Гист.";
const char kInputTypeCheckboxClassThermoPowerData[] = "><div class=\"form-grid\"><div class=\"form-row\"><label>Активн.</label><label class=\"switch\"><input type=\"checkbox\" class=\"thermo-power\" data-action=\"t";
const char kSelectClassFieldMiniNameT[] = "<div class=\"form-row full\"><label>Датчик</label><select class=\"field mini\" name=\"t";
const char kSelectClassFieldMiniNameT2[] = "</select></div><div class=\"form-row\"><label>Режим</label><select class=\"field mini\" name=\"t";
const char kAutoInputClassFieldTempTypeNumber[] = ">auto</option></select></div><div class=\"form-row\"><label>Цель</label><input class=\"field temp\" type=\"number\" step=\"1\" name=\"t";
const char kInputClassFieldTempTypeNumberStep[] = "></div><div class=\"form-row\"><label>Гист.</label><input class=\"field temp\" type=\"number\" min=\"1\" step=\"1\" name=\"t";
const char kSelectClassFieldMiniThermoSelectData[] = "></div><div class=\"form-row\"><label>Нагрев</label><select class=\"field mini thermo-select\" data-type=\"relay\" data-selected=\"";
const char kSelectClassFieldMiniThermoSelectData2[] = "></select></div><div class=\"form-row\"><label>Охлажд</label><select class=\"field mini thermo-select\" data-type=\"relay\" data-selected=\"";
const char kSelectClassFieldMiniThermoSelectData3[] = "></select></div><div class=\"form-row\"><label>Кнопка</label><select class=\"field mini thermo-select\" data-type=\"dinput\" data-selected=\"";
} // namespace Thermo

namespace Watering
{
const char kText[] = "<div class=\"tile empty\"><strong>Ожидаем данные со слейва</strong></div>";
const char kText2[] = "<div class=\"tile empty\"><strong>Правила отсутствуют</strong></div>";
const char kPageTitle[] = "Полив";
const char kText3[] = "активно";
const char kText4[] = "пауза";
const char kText5[] = "ожидание";
const char kText6[] = "<div class=\"watering-visual\"><div class=\"tile-head\"><strong>Правило ";
const char kText7[] = "вкл";
const char kText8[] = "выкл";
const char kText9[] = "</span></div><svg class=\"watering-icon\" viewBox=\"0 0 24 24\" fill=\"currentColor\" aria-hidden=\"true\"><path d=\"M12 2c-2.3 3.5-6 7.4-6 11a6 6 0 0 0 12 0c0-3.6-3.7-7.5-6-11zm0 18a4 4 0 0 1-4-4c0-2.2 2.3-5.2 4-7.7 1.7 2.5 4 5.5 4 7.7a4 4 0 0 1-4 4z\"/></svg><div class=\"status-line\"><span class=\"muted\">Состояние</span><span class=\"status-value\">";
const char kText10[] = "Правило полива";
const char kText11[] = "<div class=\"form-row\"><label>Монитор</label><div class=\"field mini\">";
const char kText12[] = "<div class=\"form-row\"><label>Кран</label><div class=\"field mini\">";
const char kText13[] = "<div class=\"form-row full\"><label>Дни</label><div class=\"field\">";
const char kText14[] = "Пн";
const char kText15[] = "Вт";
const char kText16[] = "Ср";
const char kText17[] = "Чт";
const char kText18[] = "Пт";
const char kText19[] = "Сб";
const char kText20[] = "Вс";
const char kText21[] = "<div class=\"form-row\"><label>Время</label><div class=\"field mini\">";
const char kText22[] = "<div class=\"form-row\"><label>Бак</label><div class=\"field mini\">";
const char kText23[] = "<div class=\"form-row\"><label>Длит. (мин)</label><div class=\"field mini\">";
const char kText24[] = "<div class=\"form-row\"><label>Время 2</label><div class=\"field mini\">";
const char kText25[] = "<div class=\"form-row\"><label>Длит.2 (мин)</label><div class=\"field mini\">";
const char kText32[] = "<div class=\"form-row\"><label>Время 3</label><div class=\"field mini\">";
const char kText33[] = "<div class=\"form-row\"><label>Длит.3 (мин)</label><div class=\"field mini\">";
const char kText26[] = "<div class=\"form-row\"><label>Продолжать</label><div class=\"field mini\">";
const char kGe[] = "<div class=\"form-row full\"><label>Уровень >=</label><div class=\"field mini\">";
const char kText27[] = "<div class=\"tile empty\"><strong>Контроллеры недоступны</strong></div>";
const char kInputTypeCheckboxNameW[] = "<div class=\"form-row\"><label>Монитор</label><label class=\"switch\"><input type=\"checkbox\" name=\"w";
const char kSelectClassFieldMiniWateringSelectData[] = "<div class=\"form-row\"><label>Кран</label><select class=\"field mini watering-select\" data-type=\"relay\" data-selected=\"";
const char kText28[] = "<div class=\"form-row full\"><label>Дни</label><div class=\"weekday-group\">";
const char kInputClassFieldMiniTypeTimeName[] = "<div class=\"form-row\"><label>Время</label><input class=\"field mini\" type=\"time\" name=\"w";
const char kInputClassFieldMiniTypeNumberMin[] = "<div class=\"form-row\"><label>Длит. (мин)</label><input class=\"field mini\" type=\"number\" min=\"1\" step=\"1\" name=\"w";
const char kText2InputClassFieldMiniTypeTime[] = "<div class=\"form-row\"><label>Время 2</label><input class=\"field mini\" type=\"time\" name=\"w";
const char kText2InputClassFieldMiniTypeNumber[] = "<div class=\"form-row\"><label>Длит.2 (мин)</label><input class=\"field mini\" type=\"number\" min=\"0\" step=\"1\" name=\"w";
const char kText3InputClassFieldMiniTypeTime[] = "<div class=\"form-row\"><label>Время 3</label><input class=\"field mini\" type=\"time\" name=\"w";
const char kText3InputClassFieldMiniTypeNumber[] = "<div class=\"form-row\"><label>Длит.3 (мин)</label><input class=\"field mini\" type=\"number\" min=\"0\" step=\"1\" name=\"w";
const char kSelectClassFieldMiniWateringSelectData2[] = "<div class=\"form-row\"><label>Бак</label><select class=\"field mini watering-select\" data-type=\"tank\" data-selected=\"";
const char kInputTypeCheckboxNameW2[] = "<div class=\"form-row tank-dependent\"><label>Продолжать</label><label class=\"switch\"><input type=\"checkbox\" name=\"w";
const char kGeSelectClassFieldMiniNameW[] = "<div class=\"form-row full tank-dependent resume-dependent\"><label>Уровень >=</label><select class=\"field mini\" name=\"w";
} // namespace Watering

namespace Common
{
const char kDevice[] = "Юнит";
const char kNoDataFromSlave[] = "Нет данных со слейва";
const char kErrorPrefix[] = "Ошибка: ";
const char kStatusOk[] = "ОК";
const char kPagePrev[] = "Назад";
const char kPagePage[] = "Страница";
const char kPageNext[] = "Вперёд";
const char kControllersUnavailable[] = "Контроллеры недоступны";
const char kConfigManagerUnavailable[] = "Менеджер конфигурации недоступен";
const char kSaveFailed[] = "Сохранение не удалось";
const char kSaved[] = "Сохранено";
const char kUpdated[] = "Обновлено";
const char kNoChanges[] = "Без изменений";
const char kNoChangesAlt[] = "Нет изменений";
const char kToastSlaveConnectedPrefix[] = "Подключен слейв: ";
const char kToastSlaveDisconnectedPrefix[] = "Отключен слейв: ";
const char kToastSlaveSyncCompletePrefix[] = "Синхронизирован слейв: ";
const char kSave[] = "Сохранить";
const char kSaveAcl[] = "Сохранить ACL";
const char kSaveRtc[] = "Сохранить RTC";
const char kSaveRule[] = "Сохранить правило";
const char kSaveAction[] = "Сохранить действие";
} // namespace Common

namespace Users
{
const char kReady[] = "Готово";
const char kMasterOnlyEdit[] = "Редактирование доступно только на master-устройстве";
const char kMasterOnlyAclEdit[] = "Редактирование ACL доступно только на master-устройстве";
const char kRegistryUnavailable[] = "Реестр пользователей недоступен";
const char kAclSaved[] = "ACL сохранен";
const char kAclSaveFailed[] = "Ошибка сохранения ACL";
const char kUsersLink[] = "Пользователи";
const char kUnit[] = "Юнит";
const char kUnitLocal[] = "Локальный";
const char kSelectAll[] = "Выбрать все";
const char kClearAll[] = "Снять все";
const char kAclLoadingUnit[] = "Загрузка данных юнита...";
const char kAclNoEnabledItems[] = "Нет enabled элементов";
const char kAclAll[] = "Все";
const char kAclNone[] = "Ничего";
const char kLabelUsername[] = "Имя пользователя";
const char kLabelPassword[] = "Пароль";
const char kPasswordPlaceholder[] = "оставьте пустым, чтобы не менять";
const char kLabelIButtonKey[] = "Ключ iButton";
const char kLabelRfidKey[] = "Ключ RFID";
const char kLabelPhoneGsm[] = "Телефон (GSM)";
const char kLabelCall[] = "Звонок";
const char kCtrlSockets[] = "Розетки";
const char kCtrlLights[] = "Свет";
const char kCtrlMeteo[] = "Метео";
const char kCtrlThermo[] = "Термо";
const char kCtrlTanks[] = "Баки";
const char kCtrlSeptic[] = "Септик";
const char kCtrlSecurity[] = "Охрана";
const char kCtrlWatering[] = "Полив";
const char kCtrlLeak[] = "Протечки";
const char kCtrlAvr[] = "АВР";
const char kCtrlRing[] = "Звонок";
const char kItemSocket[] = "Розетка";
const char kItemLight[] = "Свет";
const char kItemMeteo[] = "Метео";
const char kItemThermo[] = "Термо";
const char kItemTank[] = "Бак";
const char kItemSeptic[] = "Септик";
const char kItemSensor[] = "Сенсор";
const char kItemRule[] = "Правило";
const char kItemLeak[] = "Протечка";
} // namespace Users

namespace Rules
{
const char kReady[] = "Готово";
const char kInvalidRuleId[] = "Неверный ID правила";
const char kRuleNotFound[] = "Правило не найдено";
const char kActionNotFound[] = "Действие не найдено";
const char kSocketPrefix[] = "Розетка #";
const char kLightPrefix[] = "Свет #";
const char kSensorPrefix[] = "Датчик #";
const char kMeteoSensorPrefix[] = "Сенсор #";
const char kTankPrefix[] = "Бак #";
const char kSepticPrefix[] = "Септик #";
const char kSelectRule[] = "Выберите правило";
const char kRules[] = "Правила";
const char kRule[] = "Правило";
const char kAction[] = "Действие ";
const char kActionType[] = "тип: ";
const char kPause[] = "пауза";
const char kBack[] = "Назад";
const char kInvalidRuleActionId[] = "Неверный rule/action ID";
const char kLabelName[] = "Название";
const char kLabelConditionEnabled[] = "Условие активно";
const char kLabelUnit[] = "Юнит";
const char kLabelController[] = "Контроллер";
const char kLabelItem[] = "Элемент";
const char kLabelParameter[] = "Параметр";
const char kLabelOperator[] = "Оператор";
const char kLabelValue[] = "Значение";
const char kLabelType[] = "Тип";
const char kLabelDelayMs[] = "Задержка, мс";
const char kLabelState[] = "Состояние";
const char kActionLower[] = "действие ";
} // namespace Rules

namespace AdminPage
{
const char kPageTitle[] = "Система";
const char kStatusLabel[] = "Статус:";
const char kNewPassword[] = "Новый пароль";
const char kNewPasswordPlaceholder[] = "Введите новый пароль";
const char kRtcNow[] = "RTC сейчас:";
const char kDate[] = "Дата";
const char kTime[] = "Время";
const char kReboot[] = "Перезагрузить";
const char kAdminStatusAcl[] = "управление через users ACL";
const char kEepromSave[] = "Сохранять";
const char kEepromLoad[] = "Загружать";
} // namespace AdminPage

namespace CloudPage
{
const char kPageTitle[] = "Облако";
const char kConnected[] = "Подключен";
const char kDisconnected[] = "Отключен";
const char kFwVersion[] = "Версия FW:";
const char kDeviceId[] = "ID устройства:";
const char kEnableCloud[] = "Включить облако";
const char kTransport[] = "Транспорт";
const char kTransportWs[] = "WebSocket";
const char kTransportHttp[] = "HTTP";
const char kTransportHint[] = "HTTP пока заготовка: переключатель добавлен для будущего серверного транспорта";
const char kHost[] = "Хост";
const char kPort[] = "Порт";
const char kPath[] = "Путь";
const char kUseSsl[] = "Использовать SSL";
const char kReconnectMs[] = "Переподключение (мс)";
const char kEventMs[] = "Интервал событий (мс)";
const char kEventHint[] = "0 - отключить авто-события";
const char kApiKey[] = "API ключ";
} // namespace CloudPage

namespace WifiPage
{
const char kPageTitle[] = "Сеть";
const char kMode[] = "Режим";
const char kPassword[] = "Пароль";
const char kStaPasswordPlaceholder[] = "Введите пароль для STA";
const char kApPassword[] = "Пароль AP";
const char kApPasswordPlaceholder[] = "Введите пароль для AP";
const char kEnabled[] = "Включен";
const char kState[] = "Состояние";
const char kOperator[] = "Оператор";
const char kSignal[] = "Сигнал";
const char kRegistration[] = "Регистрация";
const char kError[] = "Ошибка";
const char kLastUrc[] = "Последний URC";
const char kLastSms[] = "Последний SMS";
const char kLastCall[] = "Последний звонок";
const char kLastUssd[] = "Последний USSD";
const char kUnavailable[] = "недоступно";
const char kOn[] = "включен";
const char kOff[] = "выключен";
const char kStarted[] = "инициализирован";
const char kNotStarted[] = "не инициализирован";
const char kGsmTitle[] = "GSM";
const char kStaStatusRowLabel[] = "STA";
} // namespace WifiPage

namespace DisplayPage
{
const char kPageTitle[] = "Дисплей";
const char kTitle[] = "Дисплей (LCD1602)";
const char kSrcTime[] = "Время";
const char kSrcSocket[] = "Розетка";
const char kSrcLight[] = "Свет";
const char kSrcMeteo[] = "Метео";
const char kSrcThermo[] = "Термо";
const char kSrcTank[] = "Бак";
const char kSrcSeptic[] = "Септик";
const char kSrcSecurity[] = "Охрана";
const char kSrcAvr[] = "АВР";
const char kSrcLeak[] = "Протечки";
const char kSrcText[] = "Текст";
const char kFieldState[] = "Состояние";
const char kFieldTemp[] = "Темп";
const char kFieldHum[] = "Влажн";
const char kFieldStatus[] = "Статус";
const char kFieldLevel[] = "Уровень";
const char kFieldSource[] = "Источник";
const char kFieldMainPower[] = "Основная сеть";
const char kFieldReservePower[] = "Резервная сеть";
const char kFieldText[] = "Текст";
} // namespace DisplayPage

namespace PortsPage
{
const char kPageTitle[] = "Порты";
const char kTitle[] = "Порты";
const char kExtenders[] = "Расширители";
const char kPorts[] = "Порты";
const char kBus[] = "Шина";
const char kAddress[] = "Адрес";
const char kType[] = "Тип";
const char kBackend[] = "Бекенд";
const char kLocalShort[] = "Лок.";
const char kControllerShort[] = "Контр.";
const char kDeviceShort[] = "Устр.";
const char kPin[] = "Пин";
} // namespace PortsPage

namespace GroupsPage
{
const char kPageTitle[] = "Группы";
const char kTitle[] = "Группы";
const char kDescription[] = "Общий список групп для элементов контроллеров.";
const char kLabel[] = "Группа";
const char kName[] = "Название";
const char kSort[] = "Сортировка";
const char kDelete[] = "Удалить";
const char kNewGroup[] = "Новая группа";
const char kNamePlaceholder[] = "Название";
const char kAdd[] = "Добавить";
const char kAll[] = "Все";
const char kNoGroup[] = "Без группы";
const char kNoGroups[] = "Нет групп";
const char kEmptyList[] = "Список пуст";
const char kWaitingSlave[] = "Ожидаем данные со слейва";
const char kDeleteLower[] = "удалить";
const char kAddFailed[] = "Не удалось добавить группу";
const char kAdded[] = "Группа добавлена";
} // namespace GroupsPage

namespace StackPage
{
const char kPageTitle[] = "Стек";
const char kTitle[] = "Стек";
const char kRole[] = "Роль";
const char kMasterHost[] = "Хост/IP мастера";
const char kExchangePolicy[] = "Политика обмена";
const char kPolicyAuto[] = "Авто";
const char kPolicyDirect[] = "Прямой обмен";
const char kPolicyPoll[] = "Опрос";
const char kTransport[] = "Транспорт";
const char kTransportWebSocket[] = "WebSocket";
const char kTransportRs485[] = "RS485";
const char kPayloadMode[] = "Формат route";
const char kPayloadAuto[] = "Авто";
const char kPayloadJson[] = "JSON";
const char kPayloadBinary[] = "Binary";
const char kFallbackMaster[] = "Резервный мастер";
const char kEnable[] = "Включить";
const char kFallbackHost[] = "Хост/IP резервного мастера";
const char kSlaveController[] = "Слейв-контроллер";
const char kFullController[] = "Полноценный контроллер";
const char kApiKey[] = "API ключ";
const char kApiKeyPlaceholder[] = "необязательно";
const char kGenerate[] = "Сгенерировать";
const char kSlaveLinkDisconnected[] = "Связь со слейвом: нет";
const char kSlaveLinkConnected[] = "Связь со слейвом: есть";
const char kSlaveLinkWaitingHello[] = "Связь со слейвом: есть, ожидание hello";
const char kMasterLinkDisconnected[] = "Связь с мастером: нет";
const char kMasterLinkConnected[] = "Связь с мастером: есть";
const char kMasterLinkWaitingHello[] = "Связь с мастером: есть, ожидание hello";
const char kCurrentControllerPrefix[] = "Текущий контроллер";
} // namespace StackPage

namespace ManagePage
{
const char kPageTitle[] = "Прошивка и файлы";
const char kTitle[] = "Прошивка и файлы";
const char kFwSection[] = "Прошивка";
const char kFwHint[] = "OTA загрузка (.bin)";
const char kUploadFw[] = "Загрузить прошивку";
const char kFilesSection[] = "Файлы";
const char kUploadFile[] = "Загрузить файл";
const char kStatusLink[] = "Статус";
const char kFileList[] = "Список файлов";
const char kName[] = "Имя";
const char kSize[] = "Размер";
const char kDelete[] = "Удалить";
} // namespace ManagePage

namespace StatusPage
{
const char kPageTitle[] = "Статус";
const char kTitle[] = "Статус";
const char kNoData[] = "Нет данных";
} // namespace StatusPage

namespace BusesPage
{
const char kPageTitle[] = "Шины";
const char kTitle[] = "Шины";
const char kScanI2C[] = "Сканировать I2C";
const char kScanOw[] = "Сканировать OW";
} // namespace BusesPage

namespace TelegramPage
{
const char kClient[] = "Клиент:";
const char kAccess[] = "Доступ";
const char kLastChatId[] = "Последний Chat ID:";
const char kUseProxy[] = "Использовать proxy";
const char kUnknown[] = "неизвестно";
} // namespace TelegramPage

namespace WebCore
{
const char kExtendersAbsent[] = "Extenders отсутствуют";
const char kRingWebButton[] = "Звонок: веб-кнопка";
const char kStackShort[] = "стек";
const char kLocalShort[] = "локально";
const char kEnabled[] = "включен";
const char kDisabled[] = "выключен";
const char kUnavailable[] = "недоступно";
const char kOnShort[] = "вкл";
const char kOffShort[] = "выкл";
const char kEnablePrefix[] = "включ";
const char kArmedPhrase[] = "под охраной";
} // namespace WebCore

const char *const kDevice = Common::kDevice;
const char *const kNoDataFromSlave = Common::kNoDataFromSlave;
const char *const kErrorPrefix = Common::kErrorPrefix;
const char *const kStatusOk = Common::kStatusOk;
const char *const kPagePrev = Common::kPagePrev;
const char *const kPagePage = Common::kPagePage;
const char *const kPageNext = Common::kPageNext;
const char *const kControllersUnavailable = Common::kControllersUnavailable;
const char *const kConfigManagerUnavailable = Common::kConfigManagerUnavailable;
const char *const kSaveFailed = Common::kSaveFailed;
const char *const kSaved = Common::kSaved;
const char *const kUpdated = Common::kUpdated;
const char *const kNoChanges = Common::kNoChanges;
const char *const kNoChangesAlt = Common::kNoChangesAlt;
const char *const kSave = Common::kSave;
const char *const kSaveAcl = Common::kSaveAcl;
const char *const kSaveRtc = Common::kSaveRtc;
const char *const kSaveRule = Common::kSaveRule;
const char *const kSaveAction = Common::kSaveAction;
} // namespace WebUiRu


