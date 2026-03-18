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

#pragma once

namespace WebUiRu
{
namespace Controllers
{
extern const char kI2c[];
extern const char kOw[];
extern const char kPorts[];
extern const char kExtenders[];
extern const char kText[];
extern const char kUnitDevicenameNodeidIp[];
extern const char kText2[];
extern const char kIpRtcCpu[];
extern const char kText3[];
extern const char kText4[];
extern const char kText5[];
extern const char kText6[];
extern const char kFcplc[];
extern const char kText7[];
extern const char kText8[];
extern const char kText9[];
extern const char kText10[];
extern const char kTelegram[];
extern const char kLogs[];
} // namespace Controllers

namespace Display
{
extern const char kSelectClassFieldSlotKindNameDs[];
extern const char kSelectClassFieldSlotNodeNameDs[];
extern const char kSelectClassFieldSlotIndexNameDs[];
extern const char kSelectClassFieldSlotFieldNameDs[];
extern const char kInputClassFieldSlotTextTypeText[];
} // namespace Display

namespace Index
{
extern const char kBoard[];
extern const char kDeviceName[];
extern const char kStatus[];
extern const char kDate[];
extern const char kTime[];
extern const char kRtcTemp[];
extern const char kBoardTemp[];
extern const char kFan[];
} // namespace Index

namespace Lights
{
extern const char kText[];
extern const char kText2[];
extern const char kTitlePrefix[];
extern const char kLabelName[];
extern const char kText3[];
extern const char kText4[];
extern const char kPageTitle[];
extern const char kPagePrev[];
extern const char kPagePage[];
extern const char kPageNext[];
extern const char kText5[];
extern const char kText6[];
extern const char kText7[];
extern const char kText8[];
extern const char kText9[];
} // namespace Lights

namespace Meteo
{
extern const char kText[];
extern const char kText2[];
extern const char kPageTitle[];
extern const char kTitlePrefix[];
extern const char kPagePrev[];
extern const char kPagePage[];
extern const char kPageNext[];
extern const char kC[];
extern const char kText3[];
extern const char kText4[];
extern const char kText5[];
extern const char kText6[];
extern const char kText7[];
extern const char kText14[];
extern const char kText8[];
extern const char kText9[];
extern const char kText10[];
extern const char kText11[];
extern const char kText12[];
extern const char kInputClassFieldNameMeteoNameType[];
extern const char kSelectClassFieldNameMeteoSourceName[];
extern const char kText13[];
extern const char kSelectClassFieldMeteoTypeNameM[];
extern const char kSelectClassFieldMiniMeteoPinData[];
extern const char kSelectClassFieldAddrMeteoAddrName[];
} // namespace Meteo

namespace Ring
{
extern const char kText[];
extern const char kPageTitle[];
extern const char kLabelButton[];
extern const char kLabelRelay[];
extern const char kBtnRing[];
extern const char kJsError[];
extern const char kStackControlSlave[];
extern const char kLocalOnlySettings[];
extern const char kInvalidButtonPort[];
extern const char kInvalidRelayPort[];
} // namespace Ring

namespace Security
{
extern const char kText[];
extern const char kPageTitle[];
extern const char kLabelName[];
extern const char kLabelStatus[];
extern const char kLabelAlarm[];
extern const char kBtnArm[];
extern const char kBtnDisarm[];
extern const char kLabelSirenPort[];
extern const char kArmedOn[];
extern const char kArmedOff[];
extern const char kText2[];
extern const char kText3[];
extern const char kText4[];
extern const char kNum[];
extern const char kText5[];
extern const char kText6[];
extern const char kText7[];
extern const char kSelectClassFieldMiniNameSec[];
extern const char kSelectClassFieldMiniSecurityPortData[];
extern const char kInputTypeCheckboxNameSec[];
extern const char kText8[];
extern const char kText9[];
extern const char kText10[];
extern const char kText11[];
extern const char kText12[];
extern const char kText13[];
extern const char kInputClassFieldMiniTypeTextValue[];
extern const char kInputClassFieldMiniTypeTextValue2[];
extern const char kInputClassFieldMiniTypeTextValue3[];
} // namespace Security

namespace Septic
{
extern const char kPageTitle[];
extern const char kLabelName[];
extern const char kText[];
extern const char kText2[];
extern const char kText20[];
extern const char kText100[];
extern const char kText80[];
extern const char kDisabledStatus[];
extern const char kNum[];
extern const char kText3[];
extern const char kText4[];
extern const char kText5[];
extern const char kInputTypeCheckboxClassSepticMonitorData[];
extern const char kText6[];
extern const char kText7[];
extern const char kText8[];
extern const char kText9[];
extern const char kSelectClassFieldMiniSepticSelectData[];
extern const char kWarnSelectClassFieldMiniSepticSelect[];
extern const char kAlarmSelectClassFieldMiniSepticSelect[];
extern const char kRelayWarnSelectClassFieldMiniSeptic[];
extern const char kSpanClassStatusDot[];
extern const char kSpanClassStatusDot2[];
extern const char kSpanClassStatusDot3[];
extern const char kInputTypeCheckboxClassSepticMonitorData2[];
} // namespace Septic

namespace Sockets
{
extern const char kText[];
extern const char kText2[];
extern const char kTitlePrefix[];
extern const char kLabelName[];
extern const char kText3[];
extern const char kText4[];
extern const char kPageTitle[];
extern const char kPagePrev[];
extern const char kPagePage[];
extern const char kPageNext[];
extern const char kText5[];
extern const char kText6[];
extern const char kText7[];
extern const char kText8[];
extern const char kText9[];
extern const char kText10[];
} // namespace Sockets

namespace Tanks
{
extern const char kText[];
extern const char kText2[];
extern const char kText3[];
extern const char kTitlePrefix[];
extern const char kLabelName[];
extern const char kText4[];
extern const char kText5[];
extern const char kText6[];
extern const char kText7[];
extern const char kInputTypeCheckboxClassTankPowerData[];
extern const char kText8[];
extern const char kText9[];
extern const char kText10[];
extern const char kSelectClassFieldMiniTankSelectData[];
extern const char kSelectClassFieldMiniTankSelectData2[];
extern const char kSelectClassFieldMiniTankSelectData3[];
extern const char kSelectClassFieldMiniTankSelectData4[];
extern const char kSelectClassFieldMiniTankSelectData5[];
extern const char kSelectClassFieldMiniTankSelectData6[];
extern const char kInputTypeCheckboxClassTankPowerData2[];
extern const char kText11[];
extern const char kText12[];
extern const char kText14[];
extern const char kText13[];
extern const char kPageTitle[];
extern const char kInvalidPortForTankPrefix[];
} // namespace Tanks

namespace Leak
{
extern const char kPageTitle[];
extern const char kBtnAckAll[];
extern const char kHelpPorts[];
extern const char kCmdSent[];
extern const char kSendFailed[];
extern const char kAckDone[];
extern const char kSaveError[];
extern const char kInvalidSensorPort[];
extern const char kInvalidValvePort[];
extern const char kInvalidAlarmPort[];
extern const char kStackCacheUnavailable[];
extern const char kRequestingSlaveData[];
extern const char kWaitingSlave[];
extern const char kNoLeakZones[];
extern const char kLabelZonePrefix[];
extern const char kBadgeWater[];
extern const char kBadgeLatch[];
extern const char kLabelName[];
extern const char kLabelSensor[];
extern const char kLabelValve[];
extern const char kLabelAlarm[];
extern const char kToggleOn[];
extern const char kTogglePower[];
extern const char kToggleActiveLow[];
} // namespace Leak

namespace Avr
{
extern const char kPageTitle[];
extern const char kBtnResetFault[];
extern const char kStackCacheUnavailable[];
extern const char kWaitingSlave[];
extern const char kCmdSent[];
extern const char kSendFailed[];
extern const char kFaultCleared[];
extern const char kRequestingSlaveData[];
extern const char kActive[];
extern const char kTarget[];
extern const char kFault[];
extern const char kSwitching[];
extern const char kStateMain[];
extern const char kStateReserve[];
extern const char kStateFault[];
extern const char kInvalidMainOkPort[];
extern const char kInvalidReserveOkPort[];
extern const char kInvalidRelayMainPort[];
extern const char kInvalidRelayReservePort[];
extern const char kInvalidFeedbackMainPort[];
extern const char kInvalidFeedbackReservePort[];
extern const char kInvalidDebounceMs[];
extern const char kInvalidLossDelayMs[];
extern const char kInvalidReturnDelayMs[];
extern const char kInvalidBreakMs[];
extern const char kInvalidWarmupMs[];
extern const char kInvalidTransferTimeoutMs[];
extern const char kInvalidManualSource[];
extern const char kChkEnabled[];
extern const char kChkAutoMode[];
extern const char kChkPreferMain[];
extern const char kChkAutoReturnMain[];
extern const char kGroupManualSource[];
extern const char kLabelSource[];
extern const char kRadioOff[];
extern const char kRadioMain[];
extern const char kRadioReserve[];
extern const char kGroupPorts[];
extern const char kPortMainOk[];
extern const char kPortFeedbackMain[];
extern const char kPortRelayMain[];
extern const char kPortReserveOk[];
extern const char kPortFeedbackReserve[];
extern const char kPortRelayReserve[];
extern const char kGroupPortLogic[];
extern const char kAlMainOk[];
extern const char kAlReserveOk[];
extern const char kAlFeedbackMain[];
extern const char kAlFeedbackReserve[];
extern const char kInvRelayMain[];
extern const char kInvRelayReserve[];
extern const char kGroupTimings[];
extern const char kTimingDebounce[];
extern const char kTimingLossDelay[];
extern const char kTimingReturnDelay[];
extern const char kTimingBreak[];
extern const char kTimingWarmup[];
extern const char kTimingTransferTimeout[];
extern const char kPortsHelp[];
} // namespace Avr

namespace ControllersPage
{
extern const char kPageTitle[];
extern const char kNoAclControllers[];
extern const char kUnavailableUntilStartup[];
extern const char kEnabledMasc[];
extern const char kDisabledMasc[];
extern const char kEnabledFem[];
extern const char kDisabledFem[];
extern const char kEnabledNeut[];
extern const char kDisabledNeut[];
extern const char kEnabledPlural[];
extern const char kDisabledPlural[];
extern const char kUnavailable[];
extern const char kSocketsTitle[];
extern const char kSocketsDesc[];
extern const char kSocketsStatusLabel[];
extern const char kLightsTitle[];
extern const char kLightsDesc[];
extern const char kLightsStatusLabel[];
extern const char kMeteoTitle[];
extern const char kMeteoDesc[];
extern const char kMeteoStatusLabel[];
extern const char kThermoTitle[];
extern const char kThermoDesc[];
extern const char kThermoStatusLabel[];
extern const char kTanksTitle[];
extern const char kTanksDesc[];
extern const char kTanksStatusLabel[];
extern const char kWateringTitle[];
extern const char kWateringDesc[];
extern const char kWateringStatusLabel[];
extern const char kSepticTitle[];
extern const char kSepticDesc[];
extern const char kSepticStatusLabel[];
extern const char kRingTitle[];
extern const char kRingDesc[];
extern const char kRingStatusLabel[];
extern const char kSecurityTitle[];
extern const char kSecurityDesc[];
extern const char kSecurityStatusLabel[];
extern const char kAvrTitle[];
extern const char kAvrDesc[];
extern const char kAvrStatusLabel[];
extern const char kLeakTitle[];
extern const char kLeakDesc[];
extern const char kLeakStatusLabel[];
} // namespace ControllersPage

namespace Thermo
{
extern const char kText[];
extern const char kText2[];
extern const char kPageTitle[];
extern const char kLabelName[];
extern const char kText3[];
extern const char kText4[];
extern const char kText5[];
extern const char kText6[];
extern const char kText7[];
extern const char kText8[];
extern const char kC[];
extern const char kText9[];
extern const char kText10[];
extern const char kText11[];
extern const char kText12[];
extern const char kText13[];
extern const char kText14[];
extern const char kText15[];
extern const char kNum[];
extern const char kText16[];
extern const char kLabelPowerBadge[];
extern const char kLabelModeBadge[];
extern const char kLabelSensorBadge[];
extern const char kLabelActive[];
extern const char kLabelStatus[];
extern const char kLabelMode[];
extern const char kLabelTarget[];
extern const char kLabelHyst[];
extern const char kInputTypeCheckboxClassThermoPowerData[];
extern const char kSelectClassFieldMiniNameT[];
extern const char kSelectClassFieldMiniNameT2[];
extern const char kAutoInputClassFieldTempTypeNumber[];
extern const char kInputClassFieldTempTypeNumberStep[];
extern const char kSelectClassFieldMiniThermoSelectData[];
extern const char kSelectClassFieldMiniThermoSelectData2[];
extern const char kSelectClassFieldMiniThermoSelectData3[];
} // namespace Thermo

namespace Watering
{
extern const char kText[];
extern const char kText2[];
extern const char kPageTitle[];
extern const char kText3[];
extern const char kText4[];
extern const char kText5[];
extern const char kText6[];
extern const char kText7[];
extern const char kText8[];
extern const char kText9[];
extern const char kText10[];
extern const char kText11[];
extern const char kText12[];
extern const char kText13[];
extern const char kText14[];
extern const char kText15[];
extern const char kText16[];
extern const char kText17[];
extern const char kText18[];
extern const char kText19[];
extern const char kText20[];
extern const char kText21[];
extern const char kText22[];
extern const char kText23[];
extern const char kText24[];
extern const char kText25[];
extern const char kText32[];
extern const char kText33[];
extern const char kText26[];
extern const char kGe[];
extern const char kText27[];
extern const char kInputTypeCheckboxNameW[];
extern const char kSelectClassFieldMiniWateringSelectData[];
extern const char kText28[];
extern const char kInputClassFieldMiniTypeTimeName[];
extern const char kInputClassFieldMiniTypeNumberMin[];
extern const char kText2InputClassFieldMiniTypeTime[];
extern const char kText2InputClassFieldMiniTypeNumber[];
extern const char kText3InputClassFieldMiniTypeTime[];
extern const char kText3InputClassFieldMiniTypeNumber[];
extern const char kSelectClassFieldMiniWateringSelectData2[];
extern const char kInputTypeCheckboxNameW2[];
extern const char kGeSelectClassFieldMiniNameW[];
} // namespace Watering

namespace Common
{
extern const char kDevice[];
extern const char kNoDataFromSlave[];
extern const char kErrorPrefix[];
extern const char kStatusOk[];
extern const char kPagePrev[];
extern const char kPagePage[];
extern const char kPageNext[];
extern const char kControllersUnavailable[];
extern const char kConfigManagerUnavailable[];
extern const char kSaveFailed[];
extern const char kSaved[];
extern const char kUpdated[];
extern const char kNoChanges[];
extern const char kNoChangesAlt[];
extern const char kToastSlaveConnectedPrefix[];
extern const char kToastSlaveDisconnectedPrefix[];
extern const char kToastSlaveSyncCompletePrefix[];
extern const char kSave[];
extern const char kSaveAcl[];
extern const char kSaveRtc[];
extern const char kSaveRule[];
extern const char kSaveAction[];
} // namespace Common

namespace Users
{
extern const char kReady[];
extern const char kMasterOnlyEdit[];
extern const char kMasterOnlyAclEdit[];
extern const char kRegistryUnavailable[];
extern const char kAclSaved[];
extern const char kAclSaveFailed[];
extern const char kUsersLink[];
extern const char kUnit[];
extern const char kUnitLocal[];
extern const char kSelectAll[];
extern const char kClearAll[];
extern const char kAclLoadingUnit[];
extern const char kAclNoEnabledItems[];
extern const char kAclAll[];
extern const char kAclNone[];
extern const char kLabelUsername[];
extern const char kLabelPassword[];
extern const char kPasswordPlaceholder[];
extern const char kLabelTelegramLogin[];
extern const char kLabelTelegramChatId[];
extern const char kLabelAdmin[];
extern const char kLabelTelegramNotify[];
extern const char kLabelTelegramQuickActions[];
extern const char kLabelIButtonKey[];
extern const char kLabelRfidKey[];
extern const char kLabelPhoneGsm[];
extern const char kLabelCall[];
extern const char kCtrlSockets[];
extern const char kCtrlLights[];
extern const char kCtrlMeteo[];
extern const char kCtrlThermo[];
extern const char kCtrlTanks[];
extern const char kCtrlSeptic[];
extern const char kCtrlSecurity[];
extern const char kCtrlWatering[];
extern const char kCtrlLeak[];
extern const char kCtrlAvr[];
extern const char kCtrlRing[];
extern const char kItemSocket[];
extern const char kItemLight[];
extern const char kItemMeteo[];
extern const char kItemThermo[];
extern const char kItemTank[];
extern const char kItemSeptic[];
extern const char kItemSensor[];
extern const char kItemRule[];
extern const char kItemLeak[];
} // namespace Users

namespace Rules
{
extern const char kReady[];
extern const char kInvalidRuleId[];
extern const char kRuleNotFound[];
extern const char kActionNotFound[];
extern const char kSocketPrefix[];
extern const char kLightPrefix[];
extern const char kSensorPrefix[];
extern const char kMeteoSensorPrefix[];
extern const char kTankPrefix[];
extern const char kSepticPrefix[];
extern const char kSelectRule[];
extern const char kRules[];
extern const char kRule[];
extern const char kAction[];
extern const char kActionType[];
extern const char kPause[];
extern const char kBack[];
extern const char kInvalidRuleActionId[];
extern const char kLabelName[];
extern const char kLabelConditionEnabled[];
extern const char kLabelUnit[];
extern const char kLabelController[];
extern const char kLabelItem[];
extern const char kLabelParameter[];
extern const char kLabelOperator[];
extern const char kLabelValue[];
extern const char kLabelType[];
extern const char kLabelDelayMs[];
extern const char kLabelState[];
extern const char kActionLower[];
} // namespace Rules

namespace AdminPage
{
extern const char kPageTitle[];
extern const char kStatusLabel[];
extern const char kNewPassword[];
extern const char kNewPasswordPlaceholder[];
extern const char kRtcNow[];
extern const char kDate[];
extern const char kTime[];
extern const char kReboot[];
extern const char kAdminStatusAcl[];
extern const char kEepromSave[];
extern const char kEepromLoad[];
} // namespace AdminPage

namespace CloudPage
{
extern const char kPageTitle[];
extern const char kConnected[];
extern const char kDisconnected[];
extern const char kFwVersion[];
extern const char kDeviceId[];
extern const char kEnableCloud[];
extern const char kTransport[];
extern const char kTransportWs[];
extern const char kTransportHttp[];
extern const char kTransportHint[];
extern const char kHost[];
extern const char kPort[];
extern const char kPath[];
extern const char kUseSsl[];
extern const char kReconnectMs[];
extern const char kEventMs[];
extern const char kEventHint[];
extern const char kApiKey[];
} // namespace CloudPage

namespace WifiPage
{
extern const char kPageTitle[];
extern const char kMode[];
extern const char kPassword[];
extern const char kStaPasswordPlaceholder[];
extern const char kApPassword[];
extern const char kApPasswordPlaceholder[];
extern const char kEnabled[];
extern const char kState[];
extern const char kOperator[];
extern const char kSignal[];
extern const char kRegistration[];
extern const char kError[];
extern const char kLastUrc[];
extern const char kLastSms[];
extern const char kLastCall[];
extern const char kLastUssd[];
extern const char kUnavailable[];
extern const char kOn[];
extern const char kOff[];
extern const char kStarted[];
extern const char kNotStarted[];
extern const char kGsmTitle[];
extern const char kStaStatusRowLabel[];
} // namespace WifiPage

namespace DisplayPage
{
extern const char kPageTitle[];
extern const char kTitle[];
extern const char kSrcTime[];
extern const char kSrcSocket[];
extern const char kSrcLight[];
extern const char kSrcMeteo[];
extern const char kSrcThermo[];
extern const char kSrcTank[];
extern const char kSrcSeptic[];
extern const char kSrcSecurity[];
extern const char kSrcAvr[];
extern const char kSrcLeak[];
extern const char kSrcText[];
extern const char kFieldState[];
extern const char kFieldTemp[];
extern const char kFieldHum[];
extern const char kFieldStatus[];
extern const char kFieldLevel[];
extern const char kFieldSource[];
extern const char kFieldMainPower[];
extern const char kFieldReservePower[];
extern const char kFieldText[];
} // namespace DisplayPage

namespace PortsPage
{
extern const char kPageTitle[];
extern const char kTitle[];
extern const char kExtenders[];
extern const char kPorts[];
extern const char kBus[];
extern const char kAddress[];
extern const char kType[];
extern const char kBackend[];
extern const char kLocalShort[];
extern const char kControllerShort[];
extern const char kDeviceShort[];
extern const char kPin[];
} // namespace PortsPage

namespace GroupsPage
{
extern const char kPageTitle[];
extern const char kTitle[];
extern const char kDescription[];
extern const char kLabel[];
extern const char kName[];
extern const char kSort[];
extern const char kDelete[];
extern const char kNewGroup[];
extern const char kNamePlaceholder[];
extern const char kAdd[];
extern const char kAll[];
extern const char kNoGroup[];
extern const char kNoGroups[];
extern const char kEmptyList[];
extern const char kWaitingSlave[];
extern const char kDeleteLower[];
extern const char kAddFailed[];
extern const char kAdded[];
} // namespace GroupsPage

namespace StackPage
{
extern const char kPageTitle[];
extern const char kTitle[];
extern const char kRole[];
extern const char kMasterHost[];
extern const char kFallbackMaster[];
extern const char kEnable[];
extern const char kFallbackHost[];
extern const char kSlaveController[];
extern const char kFullController[];
extern const char kApiKey[];
extern const char kApiKeyPlaceholder[];
extern const char kGenerate[];
extern const char kSlaveLinkDisconnected[];
extern const char kSlaveLinkConnected[];
extern const char kSlaveLinkWaitingHello[];
extern const char kMasterLinkDisconnected[];
extern const char kMasterLinkConnected[];
extern const char kMasterLinkWaitingHello[];
extern const char kCurrentControllerPrefix[];
} // namespace StackPage

namespace ManagePage
{
extern const char kPageTitle[];
extern const char kTitle[];
extern const char kFwSection[];
extern const char kFwHint[];
extern const char kUploadFw[];
extern const char kFilesSection[];
extern const char kUploadFile[];
extern const char kStatusLink[];
extern const char kFileList[];
extern const char kName[];
extern const char kSize[];
extern const char kDelete[];
} // namespace ManagePage

namespace StatusPage
{
extern const char kPageTitle[];
extern const char kTitle[];
extern const char kNoData[];
} // namespace StatusPage

namespace BusesPage
{
extern const char kPageTitle[];
extern const char kTitle[];
extern const char kScanI2C[];
extern const char kScanOw[];
} // namespace BusesPage

namespace TelegramPage
{
extern const char kClient[];
extern const char kAccess[];
extern const char kLastChatId[];
extern const char kUseProxy[];
extern const char kUnknown[];
} // namespace TelegramPage

namespace WebCore
{
extern const char kExtendersAbsent[];
extern const char kRingWebButton[];
extern const char kStackShort[];
extern const char kLocalShort[];
extern const char kEnabled[];
extern const char kDisabled[];
extern const char kUnavailable[];
extern const char kOnShort[];
extern const char kOffShort[];
extern const char kEnablePrefix[];
extern const char kArmedPhrase[];
} // namespace WebCore

extern const char *const kDevice;
extern const char *const kNoDataFromSlave;
extern const char *const kErrorPrefix;
extern const char *const kStatusOk;
extern const char *const kPagePrev;
extern const char *const kPagePage;
extern const char *const kPageNext;
extern const char *const kControllersUnavailable;
extern const char *const kConfigManagerUnavailable;
extern const char *const kSaveFailed;
extern const char *const kSaved;
extern const char *const kUpdated;
extern const char *const kNoChanges;
extern const char *const kNoChangesAlt;
extern const char *const kSave;
extern const char *const kSaveAcl;
extern const char *const kSaveRtc;
extern const char *const kSaveRule;
extern const char *const kSaveAction;
} // namespace WebUiRu

