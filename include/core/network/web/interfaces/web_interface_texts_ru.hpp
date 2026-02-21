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
} // namespace Ring

namespace Security
{
inline constexpr const char kText[] = "Датчики";
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
inline constexpr const char kText[] = "<div class=\"tile empty\"><strong>Ожидаем данные со слейва</strong></div>";
inline constexpr const char kText2[] = "<div class=\"tile empty\"><strong>Септик отсутствует</strong></div>";
inline constexpr const char kText20[] = "Уровень: 20%";
inline constexpr const char kText100[] = "Уровень: 100%";
inline constexpr const char kText80[] = "Уровень: 80%";
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
} // namespace Tanks

namespace Thermo
{
inline constexpr const char kText[] = "<div class=\"tile empty\"><strong>Ожидаем данные со слейва</strong></div>";
inline constexpr const char kText2[] = "<div class=\"tile empty\"><strong>Термо отсутствует</strong></div>";
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
inline constexpr const char kSave[] = "Сохранить";
inline constexpr const char kSaveAcl[] = "Сохранить ACL";
inline constexpr const char kSaveRtc[] = "Сохранить RTC";
inline constexpr const char kSaveRule[] = "Сохранить правило";
inline constexpr const char kSaveAction[] = "Сохранить действие";
} // namespace Common

inline constexpr const char *kDevice = Common::kDevice;
inline constexpr const char *kNoDataFromSlave = Common::kNoDataFromSlave;
inline constexpr const char *kErrorPrefix = Common::kErrorPrefix;
inline constexpr const char *kStatusOk = Common::kStatusOk;
inline constexpr const char *kSave = Common::kSave;
inline constexpr const char *kSaveAcl = Common::kSaveAcl;
inline constexpr const char *kSaveRtc = Common::kSaveRtc;
inline constexpr const char *kSaveRule = Common::kSaveRule;
inline constexpr const char *kSaveAction = Common::kSaveAction;
} // namespace WebUiRu
