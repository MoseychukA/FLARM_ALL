/*
  WebRF.cpp — WEB-интерфейс внешнего дисплея FlyRf.
  Внешний вид, цветовая гамма и общая разметка взяты из WEB интерфейса
  базового проекта FlyRf_Base_26_04_27_01. Рабочими оставлены настройки
  отображения TFT и загрузка прошивки. Остальные разделы пока заглушки.
*/
#include "WebRF.h"
#include <Arduino.h>
#include <WebServer.h>
#include <Update.h>
#include "EEPROMRF.h"
#include "WiFiRF.h"
#include "DeviceInfo.h"
#include "RS485Display.h"
#include "DisplayRemote.h"
#include "ESP32RF.h"
#include "System.h"

static WebServer server(80);
static bool g_rebootPending = false;
static uint32_t g_rebootAtMs = 0;
static bool g_updateOk = false;
static bool g_updateStarted = false;
static String g_updateMessage;
static bool g_otaInProgress = false;
static uint32_t g_otaWritten = 0;
static uint32_t g_otaTotal = 0;
static uint32_t g_otaExpectedSize = 0;

static const char* sel(bool v) { return v ? "selected" : ""; }
static int clampInt(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
static void sendNoCache(){ server.sendHeader("Cache-Control","no-cache, no-store, must-revalidate"); server.sendHeader("Pragma","no-cache"); server.sendHeader("Expires","-1"); }
static void scheduleReboot(uint32_t ms=1500){ g_rebootPending = true; g_rebootAtMs = millis() + ms; }

static String htmlBegin(const String& title)
{
    String s;
    s.reserve(5000);
    s += F("<!DOCTYPE html><html><head><meta charset='utf-8'>");
    s += F("<meta name='viewport' content='width=device-width, initial-scale=1'>");
    s += F("<title>"); s += title; s += F("</title>");
    s += F("<style>");
    s += F("body{font-family:Arial,'Times New Roman',serif;background:#f3f3f3;color:#000;margin:0;padding:0;}");
    s += F(".page{max-width:980px;margin:0 auto;padding:8px 12px 28px 12px;}");
    s += F("h1{font-size:32px;line-height:1.08;text-align:center;font-weight:700;margin:10px 0 18px 0;}");
    s += F("h2{font-size:22px;line-height:1.1;text-align:center;font-weight:700;margin:8px 0 14px 0;}");
    s += F("table{width:100%;border-collapse:collapse;}");
    s += F("th,td{font-size:17px;line-height:1.18;padding:4px 0;vertical-align:top;}");
    s += F("th{text-align:left;font-weight:700;padding-right:10px;}");
    s += F("td{text-align:right;font-weight:400;}");
    s += F(".left{text-align:left;}.center{text-align:center;}");
    s += F(".hr{height:1px;background:#9a9a9a;margin:12px 0 14px 0;}");
    s += F(".settingsBlock{background:#fff;border:1px solid #cfcfcf;border-radius:8px;padding:10px 12px 12px 12px;margin:0 0 12px 0;box-shadow:0 1px 2px rgba(0,0,0,.05);overflow:hidden;}");
    s += F(".blockTitle{font-size:20px;line-height:1.1;font-weight:700;text-align:left;margin:0 0 8px 0;color:#1f1f1f;}");
    s += F(".blockTitleCenter{text-align:center;}");
    s += F(".settingsBlock table{margin:0;}");
    s += F(".note{font-size:17px;line-height:1.28;color:#333;text-align:right;overflow-wrap:anywhere;word-break:break-word;}");
    s += F(".btnRow{display:flex;justify-content:center;align-items:center;gap:8px;flex-wrap:wrap;margin:6px 0 0 0;}");
    s += F(".btn{display:inline-flex;justify-content:center;align-items:center;font-family:Arial,'Times New Roman',serif;font-size:22px;line-height:1.12;padding:13px 14px;margin:0;border:1px solid #8b8b8b;border-radius:3px;background:#ececec;color:#000;text-decoration:none;cursor:pointer;width:auto;min-width:148px;max-width:100%;box-sizing:border-box;text-align:center;white-space:nowrap;}.btnIcon{display:inline-block;font-size:1.48em;line-height:1;vertical-align:-0.12em;margin-right:0.30em;}.settingsTopCtl,.hardwareCtl,.settingsTopCtl50,.settingsTopCtl30,.hardwareCtl50,.hardwareCtl30,.hardwareCtl60,.wShort,.wMed,.wLong{width:auto;min-width:148px;max-width:100%;}.wShort{min-width:112px;}.wMed{min-width:148px;}.wLong{min-width:220px;}.compactSelect{width:250px;min-width:250px;max-width:250px;}");
    s += F("input[type=text],input[type=number],select{font-family:Arial,'Times New Roman',serif;font-size:20px;line-height:1.1;box-sizing:border-box;border:1px solid #8b8b8b;border-radius:3px;background:#fff;height:42px;padding:6px 9px;width:auto;min-width:148px;max-width:100%;}.compactSelect{font-size:18px;padding-right:28px;}");
    s += F("input[type=file]{position:absolute;left:-9999px;width:1px;height:1px;opacity:0;}");
    s += F(".btn.autoWide,.autoWide{min-width:220px;}.btn.autoNarrow,.autoNarrow{min-width:140px;}");
    s += F(".confirmTitle{font-size:20px;font-weight:700;text-align:center;line-height:1.22;margin-top:10px;}");
    s += F(".smallNote{font-size:15px;line-height:1.3;color:#444;margin-top:6px;text-align:right;}");
    s += F(".liveState{display:inline-block;min-width:230px;text-align:left;font-size:15px;line-height:1.25;color:#1d4f91;padding-top:7px;}");
    s += F(".about{font-size:18px;line-height:1.5;text-align:center;}");
    s += F(".monTools{display:flex;gap:8px;align-items:center;justify-content:space-between;flex-wrap:wrap;margin:4px 0 10px 0;}");
    s += F(".monInfo{font-size:18px;line-height:1.25;}");
    s += F(".monTable th,.monTable td{font-size:15px;padding:5px 4px;border-bottom:1px solid #d0d0d0;}");
    s += F(".monTable th{text-align:center;background:#ebebeb;position:sticky;top:0;}");
    s += F(".monTable td{text-align:center;}");
    s += F(".mono{font-family:Consolas,'Courier New',monospace;}");
    s += F(".progressWrap,.progress{max-width:560px;margin:12px auto 0 auto;border:1px solid #8b8b8b;background:#fff;height:24px;border-radius:4px;overflow:hidden;}");
    s += F(".progressBar,.bar{height:100%;width:0%;background:#7fa7ff;transition:width .15s;}");
    s += F(".btnRowEqual{display:flex;justify-content:center;gap:12px;flex-wrap:wrap;margin-top:12px;}");
    s += F(".btnRowEqual .btn{width:260px;max-width:100%;box-sizing:border-box;}");
    s += F(".viewToggleWrap{text-align:right;margin:0 0 8px 0;}.viewToggle{display:inline-block;font-family:Arial,'Times New Roman',serif;font-size:15px;line-height:1.1;padding:7px 12px;border:1px solid #8b8b8b;border-radius:3px;background:#ececec;color:#000;text-decoration:none;cursor:pointer;}");
    s += F(".frontBtn{width:260px;min-width:260px;height:38px;padding:8px 12px;font-size:20px;transition:background .15s,border-color .15s,color .15s,box-shadow .15s;}");
    s += F(".frontBtn.active{background:#0f8a0f;border-color:#13b913;color:#fff;box-shadow:0 0 8px rgba(20,185,20,.35);}");
    s += F(".frontBtn.data{background:#d7c432;border-color:#e8d94c;color:#111;box-shadow:0 0 8px rgba(220,200,50,.35);}");
    s += F(".frontBtn.error{background:#b53333;border-color:#d94a4a;color:#fff;box-shadow:0 0 8px rgba(217,74,74,.35);}");
    s += F(".ok{color:#0a7b20;font-weight:700;}.bad{color:#b00000;font-weight:700;}.muted{color:#555;}.danger{background:#b53333;border-color:#d94a4a;color:#fff;}");
    s += F("body.mobileUi .page{max-width:560px;padding:10px 10px 28px 10px;}");
    s += F("body.mobileUi h1{font-size:28px;line-height:1.12;margin:8px 0 14px 0;}body.mobileUi h2{font-size:22px;line-height:1.14;margin:8px 0 12px 0;}");
    s += F("body.mobileUi table,body.mobileUi tbody,body.mobileUi tr,body.mobileUi th,body.mobileUi td{display:block;width:100%;box-sizing:border-box;}");
    s += F("body.mobileUi tr{padding:0 0 10px 0;border-bottom:1px solid #d6d6d6;margin:0 0 10px 0;}body.mobileUi th{font-size:16px;line-height:1.25;padding:0 0 5px 0;text-align:left;}body.mobileUi td{text-align:left;padding:0;}");
    s += F("body.mobileUi input[type=text],body.mobileUi input[type=number],body.mobileUi select{width:100%;max-width:100%;height:44px;font-size:18px;padding:8px 10px;}");
    s += F("body.mobileUi .wShort,body.mobileUi .wMed,body.mobileUi .wLong,body.mobileUi .settingsTopCtl,body.mobileUi .settingsTopCtl50,body.mobileUi .settingsTopCtl30,body.mobileUi .hardwareCtl,body.mobileUi .hardwareCtl50,body.mobileUi .hardwareCtl30,body.mobileUi .hardwareCtl60,body.mobileUi .compactSelect{width:100%;min-width:0;max-width:100%;}body.mobileUi .btn{display:block;width:100%;min-width:0;font-size:18px;line-height:1.15;padding:12px 12px;margin:6px 0;}");
    s += F("body.mobileUi .btnRow{display:block;margin:6px 0 0 0;}body.mobileUi .liveState{display:block;min-width:0;width:100%;text-align:left;font-size:14px;padding-top:6px;}body.mobileUi .smallNote{text-align:left;}");
    s += F("body.mobileUi .frontBtn{width:100%;min-width:0;font-size:18px;height:40px;}");
    s += F("body.mobileUi .monTools{display:block;}body.mobileUi .monTools .btn{margin-top:6px;}body.mobileUi .monInfo{font-size:16px;margin-bottom:6px;}");
    s += F("body.mobileUi .monTable{display:block;overflow-x:auto;}body.mobileUi .monTable th,body.mobileUi .monTable td{display:table-cell;width:auto;white-space:nowrap;}");
    s += F("@media (max-width:760px){h1{font-size:30px;}th,td{font-size:16px;}input[type=text],input[type=number],input[type=file],select{font-size:16px;width:170px;height:34px;}select.compactSelect{width:170px;max-width:100%;}.btn{font-size:15px;padding:7px 12px;}.monTable th,.monTable td{font-size:13px;padding:4px 3px;}}");
    s += F("</style><script>(function(){var mode='desktop';try{mode=localStorage.getItem('flyrf_ui_mode')||'desktop';}catch(e){}if(mode==='mobile'){document.documentElement.className+=' mobileUi';}})();</script></head><body><div class='page'><div class='viewToggleWrap'><button type='button' id='uiModeToggle' class='viewToggle'><span class='btnIcon'>&#128187;</span>&#1056;&#1077;&#1078;&#1080;&#1084;: &#1055;&#1050;</button></div>");
    s += F("<script>(function(){var root=document.documentElement;var body=document.body;function setButton(mobile){var btn=document.getElementById('uiModeToggle');if(!btn)return;btn.innerHTML=mobile?\"<span class='btnIcon'>&#128241;</span>&#1056;&#1077;&#1078;&#1080;&#1084;: &#1058;&#1077;&#1083;&#1077;&#1092;&#1086;&#1085;\":\"<span class='btnIcon'>&#128187;</span>&#1056;&#1077;&#1078;&#1080;&#1084;: &#1055;&#1050;\";}function apply(mode){var mobile=(mode==='mobile');if(body)body.classList.toggle('mobileUi',mobile);root.classList.toggle('mobileUi',mobile);setButton(mobile);try{localStorage.setItem('flyrf_ui_mode',mobile?'mobile':'desktop');}catch(e){}}var start='desktop';try{start=localStorage.getItem('flyrf_ui_mode')||'desktop';}catch(e){}apply(start);document.addEventListener('click',function(ev){var t=ev.target;if(!t)return;while(t&&t.id!='uiModeToggle'&&t!==document.body){t=t.parentNode;}if(t&&t.id=='uiModeToggle'){apply((body&&body.classList.contains('mobileUi'))?'desktop':'mobile');}});})();</script>");
    return s;
}
static String htmlEnd(){ return F("</div></body></html>"); }
static String optionInt(int val, int cur, const char* text)
{
    String s = F("<option value='"); s += val; s += F("' "); s += sel(val == cur); s += F(">"); s += text; s += F("</option>"); return s;
}

static String displaySetName(uint8_t v)
{
    switch(v){
        case INFO_DISPLAY_COORDINATE: return F("Координаты");
        case INFO_DISPLAY_MAXI: return F("Полная информация");
        case INFO_DISPLAY_LORA_RAW: return F("Полная информация");
        default: return F("Выкл");
    }
}
static String rangeName(uint8_t v)
{
    return v ? F("Ручной") : F("Автоматический");
}
static String rs485State()
{
    const uint32_t last = RS485Display_lastRxMs();
    if (last && (uint32_t)(millis() - last) < BASE_LINK_TIMEOUT_MS) return F("<span class='ok'>связь с базой есть</span>");
    return F("<span class='bad'>нет связи с базой</span>");
}
static String rootPage()
{
    settings_t* s = EEPROM_getSettings();
    String h = htmlBegin("FlyRf внешний дисплей");
    h += F("<h1>FlyRf внешний дисплей</h1>");
    h += F("<div class='settingsBlock'><div class='blockTitle blockTitleCenter'>Состояние внешнего дисплея</div><table>");
    h += F("<tr><th>Версия ПО</th><td>"); h += DeviceInfo_programVersion(); h += F("</td></tr>");
    h += F("<tr><th>&#1047;&#1072;&#1097;&#1080;&#1090;&#1072; OTA</th><td>");
    if (SystemOtaPendingVerification()) h += F("&#1055;&#1088;&#1086;&#1074;&#1077;&#1088;&#1082;&#1072; &#1089;&#1090;&#1072;&#1073;&#1080;&#1083;&#1100;&#1085;&#1086;&#1089;&#1090;&#1080; (30 &#1089;)");
    else if (SystemOtaBootConfirmed()) h += F("<span class='ok'>&#1055;&#1088;&#1086;&#1096;&#1080;&#1074;&#1082;&#1072; &#1087;&#1086;&#1076;&#1090;&#1074;&#1077;&#1088;&#1078;&#1076;&#1077;&#1085;&#1072;</span>");
    else h += F("<span class='bad'>&#1053;&#1077; &#1087;&#1086;&#1076;&#1090;&#1074;&#1077;&#1088;&#1078;&#1076;&#1077;&#1085;&#1072;</span>");
    h += F("</td></tr>");
    h += F("<tr><th>Идентификатор устройства</th><td>"); h += DeviceInfo_chipIdHex(); h += F("</td></tr>");
    h += F("<tr><th>WiFi AP SSID</th><td>"); h += WiFi_ssid(); h += F("</td></tr>");
    h += F("<tr><th>WiFi AP IP</th><td>"); h += WiFi_apIP().toString(); h += F("</td></tr>");
    h += F("<tr><th>RS485</th><td>"); h += rs485State(); h += F("</td></tr>");
    h += F("<tr><th>Режим информации TFT</th><td>"); h += displaySetName(s->display_set); h += F("</td></tr>");
    h += F("<tr><th>Диапазон просмотра TFT</th><td>"); h += rangeName(s->radar_range_mode); h += F("</td></tr>");
    h += F("</table></div>");
    h += F("<div class='btnRow' style='flex-direction:column;align-items:center;'>");
    h += F("<a class='btn frontBtn' href='/settings'><span class='btnIcon'>&#9881;</span>Настройки отображения</a>");
    h += F("<a class='btn frontBtn' href='/about'><span class='btnIcon'>&#8505;</span>Информация</a>");
    h += F("<a class='btn frontBtn' href='/firmware'><span class='btnIcon'>&#11014;</span>Обновление программы</a>");
    h += F("</div>");
    h += htmlEnd(); return h;
}

static String settingsPage()
{
    settings_t* s = EEPROM_getSettings();
    String h = htmlBegin("Настройки отображения");
    h += F("<h1>Настройки отображения</h1><form method='POST' action='/settings_save'>");
    h += F("<div class='settingsBlock'><div class='blockTitle'>TFT экран как на базовом модуле</div><table>");
    h += F("<tr><th>Режим текстовой информации</th><td><select class='wLong' name='display_set'>");
    h += optionInt(0, s->display_set, "Выкл"); h += optionInt(INFO_DISPLAY_COORDINATE, s->display_set, "Координаты"); h += optionInt(INFO_DISPLAY_MAXI, s->display_set, "Полная информация"); h += F("</select></td></tr>");
    h += F("<tr><th>RSSI</th><td><select name='rssi_view'>"); h += optionInt(VIEW_RSSI_OFF, s->rssi_view, "Выкл"); h += optionInt(VIEW_RSSI_ON, s->rssi_view, "Вкл"); h += F("</select></td></tr>");
    h += F("<tr><th>Диапазон просмотра TFT</th><td><select class='wLong compactSelect' style='width:210px;min-width:210px;max-width:210px;' name='radar_range_mode'>");
    h += optionInt(0,s->radar_range_mode,"Автоматический"); h += optionInt(1,s->radar_range_mode,"Ручной"); h += F("</select></td></tr>");
    h += F("<tr><th>Показывать SOS</th><td><select name='display_sos'>"); h += optionInt(0,s->display_sos,"Выкл"); h += optionInt(1,s->display_sos,"Вкл"); h += F("</select></td></tr>");
    h += F("<tr><th>Параметры LAN на TFT</th><td><select name='lan_state_view'>"); h += optionInt(0,s->lan_state_view,"Выкл"); h += optionInt(1,s->lan_state_view,"Вкл"); h += F("</select></td></tr>");
    h += F("<tr><th>GPS Sat / координаты на TFT</th><td><select name='gps_state_view'>"); h += optionInt(0,s->gps_state_view,"Выкл"); h += optionInt(1,s->gps_state_view,"Вкл"); h += F("</select></td></tr>");
    h += F("<tr><th>Показывать ток INA219</th><td><select name='power_current_view'>"); h += optionInt(0,s->power_current_view,"Выкл"); h += optionInt(1,s->power_current_view,"Вкл"); h += F("</select></td></tr>");
    h += F("<tr><th>Показывать напряжение INA219</th><td><select name='power_voltage_view'>"); h += optionInt(0,s->power_voltage_view,"Выкл"); h += optionInt(1,s->power_voltage_view,"Вкл"); h += F("</select></td></tr>");
    h += F("<tr><th>Показывать батарейку</th><td><select name='power_battery_view'>"); h += optionInt(0,s->power_battery_view,"Выкл"); h += optionInt(1,s->power_battery_view,"Вкл"); h += F("</select></td></tr>");
    h += F("</table></div>");
    h += F("<div class='settingsBlock'><div class='blockTitle'>Пороги тревоги</div><table>");
    h += F("<tr><th>Внимание, м</th><td><input name='alarm_attention' type='number' value='"); h += s->alarm_attention; h += F("'></td></tr>");
    h += F("<tr><th>Предупреждение, м</th><td><input name='alarm_warning' type='number' value='"); h += s->alarm_warning; h += F("'></td></tr>");
    h += F("<tr><th>Тревога, м</th><td><input name='alarm_danger' type='number' value='"); h += s->alarm_danger; h += F("'></td></tr>");
    h += F("<tr><th>Разница высоты, м</th><td><input name='alarm_height' type='number' value='"); h += s->alarm_height; h += F("'></td></tr>");
    h += F("</table></div><div class='btnRowEqual'><button class='btn' type='submit'><span class='btnIcon'>&#128190;</span>Сохранить и обновить</button><a class='btn' href='/'><span class='btnIcon'>&#8634;</span>Возврат без сохранения</a></div></form>");
    h += htmlEnd(); return h;
}

static void handleSettingsSave()
{
    settings_t* s = EEPROM_getSettings();
    if(server.hasArg("display_set")) s->display_set = clampInt(server.arg("display_set").toInt(),0,INFO_DISPLAY_MAXI);
    if(server.hasArg("rssi_view")) s->rssi_view = clampInt(server.arg("rssi_view").toInt(),VIEW_RSSI_OFF,VIEW_RSSI_ON);
    if(server.hasArg("radar_range_mode"))
    {
        s->radar_range_mode = clampInt(server.arg("radar_range_mode").toInt(),0,1);
        // WEB-настройка диапазона должна применяться сразу: сбрасываем временный
        // ручной диапазон от кнопки, иначе он продолжает блокировать автоматический режим.
        set_view_range = 0;
    }
    if(server.hasArg("display_sos")) s->display_sos = clampInt(server.arg("display_sos").toInt(),0,1);
    if(server.hasArg("lan_state_view")) s->lan_state_view = clampInt(server.arg("lan_state_view").toInt(),0,1);
    if(server.hasArg("gps_state_view")) s->gps_state_view = clampInt(server.arg("gps_state_view").toInt(),0,1);
    s->tft_memory_view = 0;
    if(server.hasArg("power_current_view")) s->power_current_view = clampInt(server.arg("power_current_view").toInt(),0,1);
    if(server.hasArg("power_voltage_view")) s->power_voltage_view = clampInt(server.arg("power_voltage_view").toInt(),0,1);
    if(server.hasArg("power_battery_view")) s->power_battery_view = clampInt(server.arg("power_battery_view").toInt(),0,1);
    if(server.hasArg("alarm_attention")) s->alarm_attention = server.arg("alarm_attention").toInt();
    if(server.hasArg("alarm_warning")) s->alarm_warning = server.arg("alarm_warning").toInt();
    if(server.hasArg("alarm_danger")) s->alarm_danger = server.arg("alarm_danger").toInt();
    if(server.hasArg("alarm_height")) s->alarm_height = server.arg("alarm_height").toInt();
    EEPROM_store();
    server.sendHeader("Location","/settings"); server.send(303);
}

static String stubPage(const char* title, const char* text)
{
    String h=htmlBegin(title); h += F("<h1>"); h += title; h += F("</h1><div class='settingsBlock'><div class='blockTitle'>Раздел базового WEB-интерфейса</div><div class='note'>"); h += text; h += F("</div><p class='muted'>Внешний дисплей не выполняет функции базового модуля. При необходимости поля будут добавлены после расширения RS485-пакета.</p></div><a class='btn' href='/'>Назад</a>"); h += htmlEnd(); return h;
}

static String firmwarePage()
{
    String h = htmlBegin("Обновление программы");
    h += F("<h1>Обновление программы</h1>");
    h += F("<div class='settingsBlock'><div class='blockTitle blockTitleCenter'>Прошивка внешнего дисплея</div>");
    h += F("<div class='confirmTitle'>Выберите BIN-файл прошивки и нажмите кнопку обновления.</div>");
    h += F("<form id='upload_form'><input id='fwFile' name='update' type='file' accept='.bin,application/octet-stream' hidden>");
    h += F("<div class='btnRow' style='margin-top:14px'><button class='btn' type='button' id='pickBtn'><span class='btnIcon'>&#128193;</span>Выбрать файл</button></div>");
    h += F("<div id='fwName' class='confirmTitle' style='font-size:20px;margin-top:8px'>Файл не выбран</div>");
    h += F("<div class='btnRow'><button class='btn' type='submit' id='fwBtn'><span class='btnIcon'>&#11014;</span>Обновить программу</button></div></form>");
    h += F("<div class='progressWrap'><div id='bar' class='progressBar'></div></div>");
    h += F("<div id='prg' class='confirmTitle' style='font-size:20px'>Прогресс: 0%</div>");
    h += F("<div id='st' class='confirmTitle' style='font-size:18px'>Готов к загрузке</div>");
    h += F("<div class='btnRow'><a class='btn' href='/'><span class='btnIcon'>&#127968;</span>Вернуться на главную</a></div></div>");
    h += F(R"rawliteral(<script>(function(){
var form=document.getElementById('upload_form'),fileEl=document.getElementById('fwFile');
var pick=document.getElementById('pickBtn'),btn=document.getElementById('fwBtn');
var nameEl=document.getElementById('fwName'),prg=document.getElementById('prg');
var st=document.getElementById('st'),bar=document.getElementById('bar');
function progress(p){p=Math.max(0,Math.min(100,p||0));bar.style.width=p+'%';prg.textContent='Прогресс: '+p+'%';}
function status(t){st.textContent=t;} function busy(v){btn.disabled=v;pick.disabled=v;}
pick.onclick=function(){fileEl.click();};
fileEl.onchange=function(){if(fileEl.files.length){var f=fileEl.files[0];nameEl.innerHTML='Файл: '+f.name+'<br>('+f.size+' байт)';status('Файл выбран');}else{nameEl.textContent='Файл не выбран';}};
form.onsubmit=function(e){e.preventDefault();
 if(!fileEl.files.length){status('Сначала выберите BIN-файл');return;}
 var f=fileEl.files[0];if(!/\.bin$/i.test(f.name)){status('Нужен файл прошивки .bin');return;}
 busy(true);progress(0);status('Подготовка обновления...');
 fetch('/update_prepare?size='+encodeURIComponent(f.size),{method:'POST',cache:'no-store'}).then(function(r){return r.text().then(function(t){if(!r.ok||t.trim()!=='OK')throw new Error(t||('HTTP '+r.status));});}).then(function(){
  return new Promise(function(resolve,reject){var fd=new FormData();fd.append('update',f,f.name);var x=new XMLHttpRequest();x.open('POST','/update?size='+encodeURIComponent(f.size),true);x.timeout=0;
   x.upload.onprogress=function(ev){if(ev.lengthComputable){var p=Math.round(ev.loaded*100/ev.total);progress(p);status('Передача '+p+'%...');}};
   x.onload=function(){var t=String(x.responseText||'').trim();if(x.status===200&&t==='OK')resolve();else reject(new Error(t||('HTTP '+x.status)));};
   x.onerror=function(){reject(new Error('Ошибка сети при загрузке'));};x.onabort=function(){reject(new Error('Загрузка отменена'));};x.send(fd);});
 }).then(function(){progress(100);status('Обновление завершено. Перезагрузка...');}).catch(function(err){busy(false);status(err.message||String(err));});
};})();</script>)rawliteral");
    h += htmlEnd();
    return h;
}

static void handleRoot(){ sendNoCache(); server.send(200,"text/html; charset=utf-8",rootPage()); }
static void handleSettings(){ sendNoCache(); server.send(200,"text/html; charset=utf-8",settingsPage()); }
static void handleAbout(){ sendNoCache(); server.send(200,"text/html; charset=utf-8",stubPage("Информация","Проект внешнего TFT-дисплея. Экранная логика берется из базового модуля, данные приходят только из RS485-пакетов.")); }
static void handleFirmware(){ sendNoCache(); server.send(200,"text/html; charset=utf-8",firmwarePage()); }

static uint32_t otaMaxSketchSpace()
{
    const uint32_t freeSpace=ESP.getFreeSketchSpace();
    return freeSpace>0x1000U ? ((freeSpace-0x1000U)&0xFFFFF000U) : 0U;
}

static void handleUpdatePrepare()
{
    sendNoCache();
    g_otaExpectedSize=server.hasArg("size") ? (uint32_t)server.arg("size").toInt() : 0U;
    if(g_otaExpectedSize==0U)
    {
        server.send(400,"text/plain; charset=utf-8","FAIL: неверный размер файла");
        return;
    }
    const uint32_t maxSize=otaMaxSketchSpace();
    if(maxSize==0U)
    {
        g_updateMessage="FAIL: отсутствует свободный OTA-раздел; требуется загрузка по USB";
        server.send(409,"text/plain; charset=utf-8",g_updateMessage);
        return;
    }
    if(g_otaExpectedSize>maxSize)
    {
        g_updateMessage="FAIL: файл больше OTA-раздела";
        server.send(413,"text/plain; charset=utf-8",g_updateMessage);
        return;
    }
    SystemEnterOtaMode();
    g_updateOk=false; g_updateStarted=false; g_otaInProgress=false;
    g_otaWritten=0; g_otaTotal=g_otaExpectedSize;
    g_updateMessage="Готов к приёму прошивки";
    server.send(200,"text/plain; charset=utf-8","OK");
}

static void handleUpdateFinished()
{
    sendNoCache();
    if (!Update.hasError() && g_updateOk)
    {
        server.send(200, "text/plain; charset=utf-8", "OK");
        scheduleReboot(4000);
    }
    else
    {
        String response = F("FAIL: ");
        response += g_updateMessage;
        server.send(500, "text/plain; charset=utf-8", response);
    }
}
static void handleUpdateUpload()
{
    HTTPUpload& upload=server.upload();
    if(upload.status==UPLOAD_FILE_START)
    {
        g_updateOk=false; g_updateStarted=false; g_otaInProgress=true;
        g_otaWritten=0;
        if(server.hasArg("size")) g_otaExpectedSize=(uint32_t)server.arg("size").toInt();
        g_otaTotal=g_otaExpectedSize ? g_otaExpectedSize : upload.totalSize;
        g_updateMessage="Подготовка обновления";
        const uint32_t freeSketchSpace=ESP.getFreeSketchSpace();
        if(freeSketchSpace<=0x1000)
        {
            g_otaInProgress=false;
            g_updateMessage="Нет свободного OTA-раздела";
        }
        else
        {
            const uint32_t maxSketchSpace=(freeSketchSpace-0x1000)&0xFFFFF000;
            if(g_otaExpectedSize>maxSketchSpace)
            {
                g_otaInProgress=false;
                g_updateMessage="Файл больше OTA-раздела";
                return;
            }
            const uint32_t updateSize=g_otaExpectedSize ? g_otaExpectedSize : maxSketchSpace;
            g_updateStarted=Update.begin(updateSize, U_FLASH);
            if(!g_updateStarted)
            {
                g_otaInProgress=false;
                g_updateMessage="Не удалось начать обновление";
                Update.printError(Serial);
            }
        }
    }
    else if(upload.status==UPLOAD_FILE_WRITE)
    {
        if(!g_updateStarted) return;
        if(g_otaExpectedSize && (g_otaWritten+upload.currentSize)>g_otaExpectedSize)
        {
            g_updateStarted=false; g_updateOk=false; g_otaInProgress=false;
            g_updateMessage="Принято данных больше размера файла"; Update.abort();
        }
        else if(Update.write(upload.buf,upload.currentSize)!=upload.currentSize)
        {
            g_updateStarted=false; g_updateOk=false; g_otaInProgress=false;
            g_updateMessage="Ошибка записи прошивки";
            Update.printError(Serial);
        }
        else {g_otaWritten+=upload.currentSize; g_updateMessage="Передача файла";}
    }
    else if(upload.status==UPLOAD_FILE_END)
    {
        g_otaInProgress=false;
        if(g_updateStarted && g_otaExpectedSize && g_otaWritten!=g_otaExpectedSize)
        {
            g_updateStarted=false; g_updateOk=false; Update.abort();
            g_updateMessage="Файл принят не полностью";
        }
        else if(g_updateStarted && Update.end(true))
        {
            g_updateStarted=false; g_updateOk=true;
            g_otaTotal=g_otaExpectedSize ? g_otaExpectedSize : g_otaWritten;
            g_otaWritten=g_otaTotal;
            g_updateMessage="Обновление завершено";
        }
        else if(g_updateStarted)
        {
            g_updateStarted=false; g_updateOk=false;
            g_updateMessage="Ошибка завершения обновления";
            Update.printError(Serial);
        }
    }
    else if(upload.status==UPLOAD_FILE_ABORTED)
    {
        g_otaInProgress=false; g_updateStarted=false; g_updateOk=false;
        g_updateMessage="Загрузка отменена"; Update.abort();
    }
    yield();
}
static void handleOtaStatus(){ String j=F("{\"in_progress\":"); j+=g_otaInProgress?F("true"):F("false"); j+=F(",\"written\":"); j+=g_otaWritten; j+=F(",\"total\":"); j+=g_otaTotal; j+=F(",\"ok\":"); j+=g_updateOk?F("true"):F("false"); j+=F(",\"message\":\""); j+=g_updateMessage; j+=F("\"}"); sendNoCache(); server.send(200,"application/json; charset=utf-8",j); }
static void handleNotFound(){ server.send(404,"text/plain; charset=utf-8","Страница не найдена"); }

void Web_setup()
{
    server.on("/", handleRoot);
    server.on("/settings", handleSettings);
    server.on("/settings_save", HTTP_POST, handleSettingsSave);
    server.on("/about", handleAbout);
    server.on("/firmware", handleFirmware);
    server.on("/update_prepare", HTTP_POST, handleUpdatePrepare);
    server.on("/update", HTTP_POST, handleUpdateFinished, handleUpdateUpload);
    server.on("/ota_status", handleOtaStatus);
    server.onNotFound(handleNotFound);
    server.begin();
    Serial.println(F("[WEB] Сервер внешнего дисплея запущен"));
}
void Web_loop(){ server.handleClient(); if(g_rebootPending && (int32_t)(millis()-g_rebootAtMs)>=0){ g_rebootPending=false; ESP.restart(); } }
void Web_fini(){ server.stop(); }
bool Web_displayUpdateAllowed(){ return !g_otaInProgress; }
