/*
  Модуль WebRF.cpp
  Назначение:
  - WEB-интерфейс настройки и обслуживания устройства.

  Основные задачи модуля:
  - Поднимать HTTP-сервер и формировать HTML-страницы конфигуратора.
  - Показывать текущие параметры работы устройства и служебную диагностику.
  - Принимать изменения настроек, применять их в runtime и сохранять в EEPROM.
  - Выполнять сервисные действия: перезагрузка, обновление, изменение режимов и параметров вывода.
*/

#include "WebRF.h"
#include <WebServer.h>
#include <Update.h>
#include <math.h>
#include <TimeLib.h>
#include "WiFiRF.h"
#include "LANRF.h"
#include "EEPROMRF.h"
#include "RP2040Bridge.h"
#include "ADSBReceiverESP32.h"
#include "TrafficDB.h"
#include "Log.h"
#include "NMEA.h"
#include "DeviceInfo.h"
#include "RF.h"
#include "ESP32RF.h"
#include "System.h"
#include "GNSS.h"
#include "Baro.h"
#include "Bluetooth.h"
#include <freqplan.h>


String TxDataTemplate;
static WebServer server(80);
static bool g_handlersRegistered = false;
static bool g_webStarted = false;
static bool g_rebootPending = false;
static uint32_t g_rebootAtMs = 0;
static bool g_updateOk = false;
static String g_updateMessage;
static bool g_otaInProgress = false;
static uint32_t g_otaWritten = 0;
static uint32_t g_otaTotal = 0;

static uint32_t g_otaExpectedSize = 0;
static uint32_t g_otaProtectUntilMs = 0;
static bool g_otaWriteFailed = false;
static uint32_t g_otaChunkOffset = 0;
static uint32_t g_otaChunkReceived = 0;
static bool g_otaSkipChunk = false;

static void keepOtaProtected(uint32_t delayMs = 20000UL)
{
    g_otaProtectUntilMs = millis() + delayMs;
}

static bool otaProtected()
{
    return g_otaInProgress ||
           g_rebootPending ||
           (g_otaProtectUntilMs != 0U && (int32_t)(millis() - g_otaProtectUntilMs) < 0);
}

static void scheduleReboot(uint32_t delayMs = 1500)
{
    g_rebootPending = true;
    g_rebootAtMs = millis() + delayMs;
    keepOtaProtected(delayMs + 3000UL);
}

static void sendNoCache()
{
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "-1");
}

static int clampInt(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static const char* sel(bool cond) { return cond ? "selected" : ""; }
static const char* chk(bool cond) { return cond ? "checked" : ""; }


static int clampAlarmAttention(int v) { return clampInt(v, 2000, 3000); }
static int clampAlarmWarning(int v)   { return clampInt(v,  200, 2000); }
static int clampAlarmDanger(int v)    { return clampInt(v,   10,  500); }
static int clampAlarmHeight(int v)    { return clampInt(v,   30,  300); }

static String ipToString(const uint8_t ip[4])
{
    if (ip == nullptr) return String("0.0.0.0");
    return String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
}

static String ipToString(const IPAddress& ip)
{
    return String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
}

static uint16_t udpPortValue()
{
    if (settings && settings->udp_port != 0U) return settings->udp_port;
    return (uint16_t)NMEA_UDP_PORT;
}

// - buf: Буфер, текстовая строка или рабочее сообщение.
static String formatUtcNowText()
{
    if (!GNSS_timeValid()) return String("&#1085;&#1077;&#1090;");

    const time_t t = now();
    char buf[32];  // Текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
    snprintf(buf, sizeof(buf), "%02d.%02d.%04d %02d:%02d:%02d",
             day(t), month(t), year(t), hour(t), minute(t), second(t));
    return String(buf);
}

static String formatGnssCoordinateText()
{
    if (!GNSS_coordinatesValid()) return String("&#1085;&#1077;&#1090;");
    return String(GNSS_latitude(), 5) + ", " + String(GNSS_longitude(), 5);
}

// - buf: Буфер, текстовая строка или рабочее сообщение.
static String formatAltitudeText()
{
    char buf[48];  // Текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
    if (Baro_available())
    {
        snprintf(buf, sizeof(buf), "%.1f m (BMP180)", Baro_altitudeMeters());
        return String(buf);
    }
    if (GNSS_altitudeValid())
    {
        snprintf(buf, sizeof(buf), "%.1f m (GNSS)", GNSS_altitudeMeters());
        return String(buf);
    }
    return String("&#1085;&#1077;&#1090;");
}

// - buf: Буфер, текстовая строка или рабочее сообщение.
static String formatGpsAltitudeText()
{
    char buf[32];  // Текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
    if (GNSS_altitudeValid())
    {
        snprintf(buf, sizeof(buf), "%.1f m", GNSS_altitudeMeters());
        return String(buf);
    }
    return String("&#1085;&#1077;&#1090;");
}


static void applyRuntimeLocalCoordinates()
{
    if (!settings) return;

    settings->input_coordinates = FlyRfMode_usesLocalCoordinates(settings->mode) ? IMPUT_COORD_MANUAL : IMPUT_COORD_GNSS;
    GNSS_applyCurrentStateToThisAircraft();
}

static const char* sourceName(TrafficSource source)
{
    switch (source)
    {
        case TRAFFIC_SOURCE_FLARM_LORA:    return "FLARM";
        case TRAFFIC_SOURCE_ADSB_DUMP1090: return "ADS-B";
        case TRAFFIC_SOURCE_UNKNOWN:
        default:                           return "?";
    }
}

static const char* trackerSendName(uint8_t v)
{
    switch (v)
    {
        case 1: return "Single";
        case 2: return "Auto";
        case 3: return "Mini";
        default: return "Off";
    }
}

static const char* workModeName(uint8_t mode)
{
    switch (mode)
    {
        case FLYRF_MODE_TXRX_TEST1: return "&#1057;&#1072;&#1084;&#1086;&#1083;&#1077;&#1090; &#1079;&#1072;&#1092;&#1080;&#1082;&#1089;&#1080;&#1088;&#1086;&#1074;&#1072;&#1085;";
        case FLYRF_MODE_TXRX_TEST2: return "&#1057;&#1072;&#1084;&#1086;&#1083;&#1077;&#1090; &#1074; &#1087;&#1086;&#1083;&#1077;&#1090;&#1077;";
        case FLYRF_MODE_TXRX_TEST3: return "&#1057;&#1072;&#1084;&#1086;&#1083;&#1077;&#1090; + 5 &#1089;&#1080;&#1085;&#1093;&#1088;&#1086;&#1085;&#1085;&#1086;";
        case FLYRF_MODE_TXRX_TEST4: return "&#1057;&#1072;&#1084;&#1086;&#1083;&#1077;&#1090; + 5 &#1085;&#1077; &#1089;&#1080;&#1085;&#1093;&#1088;&#1086;&#1085;&#1085;&#1086;";
        case FLYRF_MODE_NORMAL:
        default: return "&#1053;&#1086;&#1088;&#1084;&#1072;&#1083;&#1100;&#1085;&#1099;&#1081; &#1088;&#1077;&#1078;&#1080;&#1084;";
    }
}


static const char* nmeaName(uint8_t mode)
{
    switch (mode)
    {
        case NMEA_OUTPUT_SERIAL: return "Serial";
        case NMEA_OUTPUT_UDP: return "UDP";
        case NMEA_OUTPUT_BLUETOOTH: return "Bluetooth";
        default: return "Off";
    }
}

static const char* rfProtocolName(uint8_t protocol)
{
    (void)protocol;
    return "OGNTP";
}

static uint8_t parseRfProtocolArg(const String& value)
{
    (void)value;
    // В Web-меню пункт "RF протокол" теперь управляет только профилем LoRa.
    // Реальный RF-протокол для радиоканала оставляем OGNTP; MAVLink убран из списка выбора.
    return RF_PROTOCOL_OGNTP;
}

static uint8_t parseLoRaProfileArg(const String& value)
{
    return (uint8_t)clampInt(value.toInt(), 0, 4);
}

static const char* loraBwName(uint8_t bw)
{
    switch (bw)
    {
        case 1: return "250 kHz";
        case 2: return "500 kHz";
        case 0:
        default: return "125 kHz";
    }
}

static const char* loraCrName(uint8_t cr)
{
    switch (cr)
    {
        case 1: return "4/6";
        case 2: return "4/7";
        case 3: return "4/8";
        case 0:
        default: return "4/5";
    }
}

static const char* serialModeName(uint8_t mode)
{
    switch (mode)
    {
        case OUTPUT_MODE_CONTAINER: return "Container";
        case OUTPUT_MODE_NMEA: return "NMEA";
        case OUTPUT_MODE_RP2040: return "RP2040 RX";
        case OUTPUT_MODE_FLARM: return "FLARM RX";
        case OUTPUT_MODE_LORA_RAW: return "LoRa RAW RX";
        case OUTPUT_MODE_MAVLINK: return "MAVLink";
        default: return "&#1053;&#1077; &#1074;&#1099;&#1074;&#1086;&#1076;&#1080;&#1090;&#1100;";
    }
}

static const char* rs485ModeName(uint8_t mode)
{
    switch (mode)
    {
        case OUTPUT_MODE_CONTAINER: return "Container";
        case OUTPUT_MODE_NMEA: return "NMEA";
        case OUTPUT_MODE_RS485_DISPLAY: return "&#1042;&#1085;&#1077;&#1096;&#1085;&#1080;&#1081; &#1076;&#1080;&#1089;&#1087;&#1083;&#1077;&#1081;";
        case OUTPUT_MODE_FLARM: return "FLARM RX";
        case OUTPUT_MODE_MAVLINK: return "MAVLink";
        default: return "&#1053;&#1077; &#1074;&#1099;&#1074;&#1086;&#1076;&#1080;&#1090;&#1100;";
    }
}

static const char* bluetoothModeName(uint8_t mode)
{
    switch (mode)
    {
        case BLUETOOTH_LE: return "LE";
        case BLUETOOTH_AUTO: return "Auto (BLE)";
        default: return "Service only";
    }
}

static const char* onOffName(uint8_t v)
{
    return v ? "&#1042;&#1082;&#1083;&#1102;&#1095;&#1077;&#1085;" : "&#1042;&#1099;&#1082;&#1083;&#1102;&#1095;&#1077;&#1085;";
}

static const char* yesNo(uint8_t v, const char* noTxt = "&#1053;&#1077;&#1090;", const char* yesTxt = "&#1044;&#1072;")
{
    return v ? yesTxt : noTxt;
}

static const char* tftMemoryViewName(uint8_t v)
{
    return yesNo(v, "&#1042;&#1099;&#1082;&#1083;&#1102;&#1095;&#1077;&#1085;", "&#1042;&#1082;&#1083;&#1102;&#1095;&#1077;&#1085;");
}

static const char* displayModeName(uint8_t mode)
{
    switch (mode)
    {
        case INFO_DISPLAY_COORDINATE: return "&#1057; &#1082;&#1086;&#1086;&#1088;&#1076;&#1080;&#1085;&#1072;&#1090;&#1072;&#1084;&#1080;";
        case INFO_DISPLAY_MAXI:       return "&#1055;&#1086;&#1083;&#1085;&#1072;&#1103; &#1080;&#1085;&#1092;&#1086;&#1088;&#1084;&#1072;&#1094;&#1080;&#1103;";
        case INFO_DISPLAY_LORA_RAW:   return "LoRa RAW RX";
        case INFO_DISTLAY_OFF:
        default:                      return "&#1053;&#1077; &#1074;&#1099;&#1074;&#1086;&#1076;&#1080;&#1090;&#1100;";
    }
}

static const char* loraFixedModeName(uint8_t mode)
{
    (void)mode;
    return "Fixed";
}

static const char* loraFixedFreqName(uint8_t idx)
{
    static char buf[16];
    if (idx > 9U) idx = 8U;
    snprintf(buf, sizeof(buf), "868.%u MHz", (unsigned)idx);
    return buf;
}

static const char* radarRangeModeName(uint8_t mode)
{
    return mode ? "&#1060;&#1080;&#1082;&#1089;&#1080;&#1088;&#1086;&#1074;&#1072;&#1085;&#1085;&#1099;&#1081;" : "&#1040;&#1074;&#1090;&#1086;&#1084;&#1072;&#1090;&#1080;&#1095;&#1077;&#1089;&#1082;&#1080;&#1081;";
}

static const char* adsbReceiverName(uint8_t mode)
{
    return mode == ADSB_RECEIVER_INTERNAL ? "ESP32S3 GPIO2" : "RP2040 Serial";
}

// - syncBuf: Буфер, текстовая строка или рабочее сообщение.
// - html: Параметр геометрии, координаты, размера или угла.
static String loraRuntimeModeText()
{
    RF_LoraRuntimeInfo info = {};
    if (!RF_GetLoraRuntimeInfo(info) || !info.valid)
    {
        return String("недоступно");
    }

    const char* runtimeProfile = "OGN compatible / custom";
    if (info.bandwidthKHz == 125 && info.spreadingFactor == 9 && info.codingRateDenom == 6)
    {
        runtimeProfile = "Long range";
    }
    else if (info.bandwidthKHz == 125 && info.spreadingFactor == 12 && info.codingRateDenom == 8)
    {
        runtimeProfile = "Max range";
    }
    else if (info.bandwidthKHz == 125 && info.spreadingFactor == 8 && info.codingRateDenom == 6)
    {
        runtimeProfile = "Fast robust";
    }

    char syncBuf[16];  // Текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
    snprintf(syncBuf, sizeof(syncBuf), "%02X", (unsigned)info.syncWord);

    String html;  // Текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
    html.reserve(900);
    html += F("<table style='width:100%;border-collapse:collapse;table-layout:fixed;'>");
    html += String("<tr><th class='left' style='width:54%;padding:2px 8px 2px 0;'>Профиль</th><td style='padding:2px 0;text-align:right;'>") + runtimeProfile + String("</td></tr>");
    html += String("<tr><th class='left' style='padding:2px 8px 2px 0;'>Частота</th><td style='padding:2px 0;text-align:right;'>") + String((double)info.frequencyHz / 1000000.0, 3) + String(" MHz</td></tr>");
    html += String("<tr><th class='left' style='padding:2px 8px 2px 0;'>SF</th><td style='padding:2px 0;text-align:right;'>") + String(info.spreadingFactor) + String("</td></tr>");
    html += String("<tr><th class='left' style='padding:2px 8px 2px 0;'>BW</th><td style='padding:2px 0;text-align:right;'>") + String(info.bandwidthKHz) + String(" kHz</td></tr>");
    html += String("<tr><th class='left' style='padding:2px 8px 2px 0;'>CR</th><td style='padding:2px 0;text-align:right;'>4/") + String(info.codingRateDenom) + String("</td></tr>");
    html += String("<tr><th class='left' style='padding:2px 8px 2px 0;'>SyncWord</th><td style='padding:2px 0;text-align:right;'>0x") + String(syncBuf) + String("</td></tr>");
    html += String("<tr><th class='left' style='padding:2px 8px 2px 0;'>RX symbols</th><td style='padding:2px 0;text-align:right;'>") + String(info.rxSymbols) + String("</td></tr>");
    html += String("<tr><th class='left' style='padding:2px 8px 2px 0;'>TX power</th><td style='padding:2px 0;text-align:right;'>") + String(info.txPowerDbm) + String(" dBm</td></tr>");
    html += String("<tr><th class='left' style='padding:2px 8px 2px 0;'>LowDataRateOptimize</th><td style='padding:2px 0;text-align:right;'>") + (info.lowDataRateOptimize ? String("On") : String("Off")) + String("</td></tr>");
    html += String("<tr><th class='left' style='padding:2px 8px 2px 0;'>CRC payload</th><td style='padding:2px 0;text-align:right;'>") + (info.crcOnPayload ? String("On") : String("Off")) + String("</td></tr>");
    html += String("<tr><th class='left' style='padding:2px 8px 2px 0;'>Header mode</th><td style='padding:2px 0;text-align:right;'>") + (info.implicitHeader ? String("Implicit") : String("Explicit")) + String("</td></tr>");
    html += F("</table>");
    return html;
}

static String loraRuntimeSourceText()
{
    return RF_GetLoraRegistersSourceText();
}

// - html: Параметр геометрии, координаты, размера или угла.
static String loraRuntimeSettingsBlock(const char* titleHtml, bool centeredTitle = false)
{
    String html;  // Текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
    html.reserve(1400);
    html += F("<div class='settingsBlock'><div class='blockTitle");
    if (centeredTitle) html += F(" blockTitleCenter");
    html += F("'>");
    html += (titleHtml ? String(titleHtml) : String("LoRa / LMIC"));
    html += F("</div>");
    html += loraRuntimeModeText();
    html += F("<table><tr><th>Источник данных LMIC / LoRa</th><td>");
    html += loraRuntimeSourceText();
    html += F("</td></tr></table></div>");
    return html;
}

static void parseIpArg(const String& name, uint8_t out[4])
{
    if (!server.hasArg(name) || out == nullptr) return;

    int a = 0, b = 0, c = 0, d = 0;
    if (sscanf(server.arg(name).c_str(), "%d.%d.%d.%d", &a, &b, &c, &d) == 4)
    {
        out[0] = (uint8_t)clampInt(a, 0, 255);
        out[1] = (uint8_t)clampInt(b, 0, 255);
        out[2] = (uint8_t)clampInt(c, 0, 255);
        out[3] = (uint8_t)clampInt(d, 0, 255);
    }
}


static void parseIpArgFromValue(const String& value, uint8_t out[4])
{
    if (out == nullptr) return;

    int a = 0, b = 0, c = 0, d = 0;
    if (sscanf(value.c_str(), "%d.%d.%d.%d", &a, &b, &c, &d) == 4)
    {
        out[0] = (uint8_t)clampInt(a, 0, 255);
        out[1] = (uint8_t)clampInt(b, 0, 255);
        out[2] = (uint8_t)clampInt(c, 0, 255);
        out[3] = (uint8_t)clampInt(d, 0, 255);
    }
}

static String jsonEscape(const String& src)
{
    String out;
    out.reserve(src.length() + 8);
    for (size_t i = 0; i < src.length(); ++i)
    {
        const char c = src[i];
        if (c == '\\' || c == '"') out += '\\';
        if (c == '\r' || c == '\n') out += ' ';
        else out += c;
    }
    return out;
}

static float distanceKm(float lat1, float lon1, float lat2, float lon2)
{
    const float DEG_TO_RAD_F = 0.017453292519943295769f;
    const float R = 6371.0f;

    const float p1 = lat1 * DEG_TO_RAD_F;
    const float p2 = lat2 * DEG_TO_RAD_F;
    const float dp = (lat2 - lat1) * DEG_TO_RAD_F;
    const float dl = (lon2 - lon1) * DEG_TO_RAD_F;

    const float a = sinf(dp * 0.5f) * sinf(dp * 0.5f) +
                    cosf(p1) * cosf(p2) * sinf(dl * 0.5f) * sinf(dl * 0.5f);
    const float c = 2.0f * atan2f(sqrtf(a), sqrtf(1.0f - a));
    return R * c;
}

// - tbody: Параметр геометрии, координаты, размера или угла.
// - th: Параметр геометрии, координаты, размера или угла.
static String htmlBegin(const String& title)
{
    String s;
    s.reserve(5000);
    s += F("<!DOCTYPE html><html><head><meta charset='utf-8'>");
    s += F("<meta name='viewport' content='width=device-width, initial-scale=1'>");
    s += F("<title>");
    s += title;
    s += F("</title>");
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
    s += F(".hardwareCompactBlock table{table-layout:fixed;} .hardwareCompactBlock th{width:42%;} .hardwareCompactBlock td{width:58%;overflow:hidden;} .hardwareCompactBlock select,.hardwareCompactBlock input,.hardwareCompactBlock .note{width:100%;min-width:0;max-width:100%;box-sizing:border-box;} .hardwareCompactBlock .wShort,.hardwareCompactBlock .wMed,.hardwareCompactBlock .wLong,.hardwareCompactBlock .compactSelect,.hardwareCompactBlock .hardwareCtl,.hardwareCompactBlock .hardwareCtl50,.hardwareCompactBlock .hardwareCtl30,.hardwareCompactBlock .hardwareCtl60{width:100%;min-width:0;max-width:100%;}");
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
    s += F(".progressWrap{max-width:560px;margin:12px auto 0 auto;border:1px solid #8b8b8b;background:#fff;height:24px;border-radius:4px;overflow:hidden;}");
    s += F(".progressBar{height:100%;width:0%;background:#7fa7ff;transition:width .15s;}");
    s += F(".btnRowEqual{display:flex;justify-content:center;gap:12px;flex-wrap:wrap;margin-top:12px;}");
    s += F(".btnRowEqual .btn{width:260px;max-width:100%;box-sizing:border-box;}");
    s += F(".viewToggleWrap{text-align:right;margin:0 0 8px 0;}.viewToggle{display:inline-block;font-family:Arial,'Times New Roman',serif;font-size:15px;line-height:1.1;padding:7px 12px;border:1px solid #8b8b8b;border-radius:3px;background:#ececec;color:#000;text-decoration:none;cursor:pointer;}");
    s += F(".frontBtn{width:260px;min-width:260px;height:38px;padding:8px 12px;font-size:20px;transition:background .15s,border-color .15s,color .15s,box-shadow .15s;}");
    s += F(".frontBtn.active{background:#0f8a0f;border-color:#13b913;color:#fff;box-shadow:0 0 8px rgba(20,185,20,.35);}");
    s += F(".frontBtn.data{background:#d7c432;border-color:#e8d94c;color:#111;box-shadow:0 0 8px rgba(220,200,50,.35);}");
    s += F(".frontBtn.error{background:#b53333;border-color:#d94a4a;color:#fff;box-shadow:0 0 8px rgba(217,74,74,.35);}");
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
    s += F("@media (max-width:760px){h1{font-size:30px;}th,td{font-size:16px;}input[type=text],input[type=number],input[type=file],select{font-size:16px;width:170px;height:34px;}select.compactSelect{width:170px;max-width:100%;}.btn{font-size:15px;padding:7px 12px;}.monTable th,.monTable td{font-size:13px;padding:4px 3px;}}");    s += F("</style><script>(function(){var mode='desktop';try{mode=localStorage.getItem('flyrf_ui_mode')||'desktop';}catch(e){}if(mode==='mobile'){document.documentElement.className+=' mobileUi';}})();</script></head><body><div class='page'><div class='viewToggleWrap'><button type='button' id='uiModeToggle' class='viewToggle'><span class='btnIcon'>&#128187;</span>&#1056;&#1077;&#1078;&#1080;&#1084;: &#1055;&#1050;</button></div>");
    s += F("<script>(function(){var root=document.documentElement;var body=document.body;function setButton(mobile){var btn=document.getElementById('uiModeToggle');if(!btn)return;btn.innerHTML=mobile?\"<span class='btnIcon'>&#128241;</span>&#1056;&#1077;&#1078;&#1080;&#1084;: &#1058;&#1077;&#1083;&#1077;&#1092;&#1086;&#1085;\":\"<span class='btnIcon'>&#128187;</span>&#1056;&#1077;&#1078;&#1080;&#1084;: &#1055;&#1050;\";}function apply(mode){var mobile=(mode==='mobile');if(body)body.classList.toggle('mobileUi',mobile);root.classList.toggle('mobileUi',mobile);setButton(mobile);try{localStorage.setItem('flyrf_ui_mode',mobile?'mobile':'desktop');}catch(e){}}var start='desktop';try{start=localStorage.getItem('flyrf_ui_mode')||'desktop';}catch(e){}apply(start);document.addEventListener('click',function(ev){var t=ev.target;if(!t)return;while(t&&t.id!='uiModeToggle'&&t!==document.body){t=t.parentNode;}if(t&&t.id=='uiModeToggle'){apply((body&&body.classList.contains('mobileUi'))?'desktop':'mobile');}});})();</script>");
    return s;
}

static void sendPage(const String& html)
{
    sendNoCache();
    server.send(200, "text/html; charset=utf-8", html);
}

static void sendValidationErrorPage(const String& title, const String& message)
{
    String html = htmlBegin(title);
    html += F("<h1>");
    html += title;
    html += F("</h1><div class='settingsBlock'><div class='confirmTitle'>");
    html += message;
    html += F("</div><div class='btnRow'><a class='btn' href='/hardware_settings'><span class='btnIcon'>&#8634;</span>&#1042;&#1077;&#1088;&#1085;&#1091;&#1090;&#1100;&#1089;&#1103; &#1082; &#1085;&#1072;&#1089;&#1090;&#1088;&#1086;&#1081;&#1082;&#1072;&#1084;</a></div></div></div></body></html>");
    sendPage(html);
}

static bool normalizeBlockAddrText(const String& value, String& normalized)
{
    normalized = value;
    normalized.trim();
    normalized.toUpperCase();


    if (normalized.length() != 6)
    {
        return false;
    }

    for (size_t i = 0; i < normalized.length(); ++i)
    {
        const char c = normalized.charAt(i);
        const bool digit = (c >= '0' && c <= '9');
        const bool latinHex = (c >= 'A' && c <= 'F');
        if (!digit && !latinHex)
        {
            return false;
        }
    }

    return true;
}

static uint32_t parseBlockAddrArg(const String& value)
{
    String normalized;
    if (!normalizeBlockAddrText(value, normalized))
    {
        return 0U;
    }

    if (normalized.length() == 0)
    {
        return 0U;
    }

    const unsigned long parsed = strtoul(normalized.c_str(), nullptr, 16);
    if (parsed > 0xFFFFFFUL)
    {
        return 0U;
    }
    return (uint32_t)parsed;
}

static bool readBlockAddrArg(const char* argName, uint32_t& outValue, String& normalized, String& errorText)
{
    normalized = String();
    errorText = String();
    outValue = 0U;

    if (!server.hasArg(argName))
    {
        return true;
    }

    const String raw = server.arg(argName);
    if (!normalizeBlockAddrText(raw, normalized))
    {
        errorText = F("&#1053;&#1077;&#1074;&#1077;&#1088;&#1085;&#1099;&#1081; &#1074;&#1074;&#1086;&#1076; &#1072;&#1076;&#1088;&#1077;&#1089;&#1072; &#1074; &#1087;&#1086;&#1083;&#1077; ");
        errorText += argName;
        errorText += F(". &#1042;&#1074;&#1077;&#1076;&#1080;&#1090;&#1077; 6 HEX-&#1089;&#1080;&#1084;&#1074;&#1086;&#1083;&#1086;&#1074;: 0-9 &#1080; A-F.");
        return false;
    }

    outValue = parseBlockAddrArg(normalized);
    return true;
}

static String blockAddrInputFilterScript(const char* inputName)
{
    String s;
    s.reserve(320);
    s += F("<script>(function(){var el=document.querySelector('input[name=\"");
    s += inputName;
    s += F("\"]');if(!el)return;function norm(forceZero){var v=(el.value||'').toUpperCase().replace(/[^0-9A-F]/g,'').slice(0,6);if(forceZero&&v.length===0)v='000000';if(el.value!==v)el.value=v;}el.addEventListener('input',function(){norm(false);});el.addEventListener('blur',function(){norm(true);});norm(true);})();</script>");
    return s;
}

static void formatBlockAddr(char* out, size_t outSize, uint32_t value)
{
    if (out == nullptr || outSize == 0)
    {
        return;
    }

    snprintf(out, outSize, "%06lX", (unsigned long)(value & 0xFFFFFFUL));
}

static void formatBlockAddrSummary(char* out, size_t outSize, uint32_t value)
{
    if (out == nullptr || outSize == 0)
    {
        return;
    }

    snprintf(out, outSize, "%06lX", (unsigned long)(value & 0xFFFFFFUL));
}

static String blockAddrSummary()
{
    char addr1[16];
    char addr2[16];
    char addr3[16];
    formatBlockAddrSummary(addr1, sizeof(addr1), settings ? settings->block_addr[0] : 0U);
    formatBlockAddrSummary(addr2, sizeof(addr2), settings ? settings->block_addr[1] : 0U);
    formatBlockAddrSummary(addr3, sizeof(addr3), settings ? settings->block_addr[2] : 0U);

    String s;
    s.reserve(64);
    s += addr1;
    s += F(", ");
    s += addr2;
    s += F(", ");
    s += addr3;
    return s;
}

// - busy: Параметр геометрии, координаты, размера или угла.
static String rootPage()
{
    String html = htmlBegin("FlyRf Base");
    html += F("<h1>FlyRf Base</h1><table>");
    html += F("<tr><th>&#1048;&#1076;&#1077;&#1085;&#1090;&#1080;&#1092;&#1080;&#1082;&#1072;&#1090;&#1086;&#1088; &#1091;&#1089;&#1090;&#1088;&#1086;&#1081;&#1089;&#1090;&#1074;&#1072;</th><td>");
    html += DeviceInfo_chipIdHex();
    html += F("</td></tr>");
    html += F("<tr><th>&#1041;&#1083;&#1086;&#1082;&#1080;&#1088;&#1086;&#1074;&#1072;&#1090;&#1100; &#1072;&#1076;&#1088;&#1077;&#1089;</th><td>");
    html += blockAddrSummary();
    html += F("</td></tr>");
    html += F("<tr><th>&#1042;&#1077;&#1088;&#1089;&#1080;&#1103; &#1055;&#1054;</th><td>");
    html += DeviceInfo_programVersion();
    html += F("</td></tr>");
    html += F("<tr><th>Radio</th><td>SX127x</td></tr>");
    uint32_t txPackets = 0;
    uint32_t rxPackets = 0;
    RF_GetPacketCounters(txPackets, rxPackets);
    html += F("<tr><th>LoRa &#1087;&#1072;&#1082;&#1077;&#1090;&#1086;&#1074;</th><td id='pktCounters'>Tx&nbsp; ");
    html += String(txPackets);
    html += F("&nbsp;&nbsp;&nbsp; Rx&nbsp; ");
    html += String(rxPackets);
    html += F("</td></tr>");
    html += F("<tr><th>&#1051;&#1086;&#1082;&#1072;&#1083;&#1100;&#1085;&#1072;&#1103; latitude</th><td>");
    html += String(settings ? settings->local_latitude : 55.95501f, 5);
    html += F("</td></tr>");
    html += F("<tr><th>&#1051;&#1086;&#1082;&#1072;&#1083;&#1100;&#1085;&#1072;&#1103; longitude</th><td>");
    html += String(settings ? settings->local_longitude : 37.23166f, 5);
    html += F("</td></tr>");
    html += F("<tr><th>WiFi AP SSID</th><td>");
    html += String(WiFi_ssid());
    html += F("</td></tr>");
    html += F("<tr><th>WiFi AP IP</th><td>");
    html += WiFi_apIP().toString();
    html += F("</td></tr>");
    html += F("<tr><th>WiFi MAC</th><td>");
    html += WiFi_macAddressStr();
    html += F("</td></tr>");
    uint32_t lanTxPackets = 0;
    uint32_t lanRxPackets = 0;
    uint32_t lanUdpTxPackets = 0;
    uint32_t lanUdpRxPackets = 0;
    uint32_t lanTcpTxPackets = 0;
    uint32_t lanTcpRxPackets = 0;
    LAN_getPacketCounters(lanTxPackets, lanRxPackets, lanUdpTxPackets, lanUdpRxPackets, lanTcpTxPackets, lanTcpRxPackets);
    html += F("<tr><th>LAN MAC</th><td id='lanMac'>");
    html += LAN_macAddressStr();
    html += F("</td></tr>");
    html += F("<tr><th>LAN IP</th><td id='lanIp'>");
    html += LAN_localIPStr();
    html += F("</td></tr>");
    html += F("<tr><th>LAN state</th><td id='lanState'>");
    html += LAN_linkStatusStr();
    html += LAN_dhcpLeaseActive() ? F(" / DHCP") : F(" / Static");
    html += F("</td></tr>");
    html += F("<tr><th>LAN packets</th><td id='lanPktCounters'>Tx&nbsp; ");
    html += String(lanTxPackets);
    html += F("&nbsp;&nbsp;&nbsp; Rx&nbsp; ");
    html += String(lanRxPackets);
    html += F("</td></tr>");
    html += F("<tr><th>NMEA output</th><td>");
    html += nmeaName(settings ? settings->nmea_out : NMEA_OUTPUT_OFF);
    if (settings && settings->nmea_out == NMEA_OUTPUT_UDP)
    {
        html += F(" / port ");
        html += String(udpPortValue());
    }
    html += F("</td></tr>");
    html += F("<tr><th>Bluetooth</th><td>");
    html += bluetoothModeName(settings ? settings->bluetooth : BLUETOOTH_OFF);
    if (settings)
    {
        html += F(" / ");
        html += Bluetooth_name();
    }
    html += F("</td></tr>");
    html += F("</table><div class='hr'></div>");
    html += F("<div class='settingsBlock'><div class='blockTitle blockTitleCenter'>&#1055;&#1086;&#1089;&#1083;&#1077;&#1076;&#1085;&#1080;&#1077; &#1076;&#1072;&#1085;&#1085;&#1099;&#1077; GNSS</div><table>");
    html += F("<tr><th>&#1042;&#1088;&#1077;&#1084;&#1103; UTC</th><td id='gnssTimeUtc'>");
    html += formatUtcNowText();
    html += F("</td></tr>");
    html += F("<tr><th>GPS &#1089;&#1087;&#1091;&#1090;&#1085;&#1080;&#1082;&#1080;</th><td id='gnssSats'>");
    html += String(GNSS_satellitesValid() ? GNSS_satellites() : 0);
    html += F("</td></tr>");
    html += F("<tr><th>&#1054;&#1087;&#1088;&#1077;&#1076;&#1077;&#1083;&#1105;&#1085;&#1085;&#1099;&#1077; &#1082;&#1086;&#1086;&#1088;&#1076;&#1080;&#1085;&#1072;&#1090;&#1099;</th><td id='gnssCoords'>");
    html += formatGnssCoordinateText();
    html += F("</td></tr>");
    html += F("<tr><th>&#1042;&#1099;&#1089;&#1086;&#1090;&#1072; GPS</th><td id='gnssGpsAlt'>");
    html += formatGpsAltitudeText();
    html += F("</td></tr>");
    html += F("<tr><th>&#1042;&#1099;&#1089;&#1086;&#1090;&#1072; &#1085;&#1072;&#1076; &#1091;&#1088;&#1086;&#1074;&#1085;&#1077;&#1084; &#1084;&#1086;&#1088;&#1103;</th><td id='gnssAlt'>");
    html += formatAltitudeText();
    html += F("</td></tr></table></div>");
    html += loraRuntimeSettingsBlock("&#1056;&#1077;&#1072;&#1083;&#1100;&#1085;&#1099;&#1077; &#1085;&#1072;&#1089;&#1090;&#1088;&#1086;&#1081;&#1082;&#1080; LoRa / LMIC", true);
    html += F("<div class='btnRow' style='flex-direction:column;align-items:center;'>");
    html += F("<a class='btn frontBtn' data-startbtn='1' href='/settings'><span class='btnIcon'>&#9881;</span>&#1053;&#1072;&#1089;&#1090;&#1088;&#1086;&#1081;&#1082;&#1080;</a>");
    html += F("<a class='btn frontBtn' data-startbtn='1' href='/hardware_settings'><span class='btnIcon'>&#129520;</span>&#1040;&#1087;&#1087;&#1072;&#1088;&#1072;&#1090;&#1085;&#1099;&#1077; &#1085;&#1072;&#1089;&#1090;&#1088;&#1086;&#1081;&#1082;&#1080;</a>");
    html += F("<a class='btn frontBtn' data-startbtn='1' href='/outputs'><span class='btnIcon'>&#128228;</span>&#1059;&#1087;&#1088;&#1072;&#1074;&#1083;&#1077;&#1085;&#1080;&#1077; &#1074;&#1099;&#1074;&#1086;&#1076;&#1086;&#1084;</a>");
    html += F("<a class='btn frontBtn' data-startbtn='1' href='/about'><span class='btnIcon'>&#8505;</span>&#1048;&#1085;&#1092;&#1086;&#1088;&#1084;&#1072;&#1094;&#1080;&#1103;</a>");
    html += F("<a class='btn frontBtn' data-startbtn='1' href='/firmware'><span class='btnIcon'>&#11014;</span>&#1054;&#1073;&#1085;&#1086;&#1074;&#1083;&#1077;&#1085;&#1080;&#1077; &#1087;&#1088;&#1086;&#1075;&#1088;&#1072;&#1084;&#1084;&#1099;</a>");
    html += F("</div><div class='hr'></div>");
    html += F(R"JS(<script>
(function(){
  var el=document.getElementById('pktCounters');
  if(!el) return;
  var lanPkt=document.getElementById('lanPktCounters');
  var lanIp=document.getElementById('lanIp');
  var lanState=document.getElementById('lanState');
  var lanMac=document.getElementById('lanMac');
  var gnssTime=document.getElementById('gnssTimeUtc');
  var gnssSats=document.getElementById('gnssSats');
  var gnssCoords=document.getElementById('gnssCoords');
  var gnssGpsAlt=document.getElementById('gnssGpsAlt');
  var gnssAlt=document.getElementById('gnssAlt');
  var busy=false;
  function applyActiveButtons(){
    var items=document.querySelectorAll('[data-startbtn="1"]');
    if(!items || !items.length) return;
    var active='';
    try{ active=localStorage.getItem('flyrf_start_active') || ''; }catch(e){}
    for(var i=0;i<items.length;i++){
      var href=items[i].getAttribute('href') || '';
      if(active && href===active) items[i].classList.add('active');
      else items[i].classList.remove('active');
      items[i].addEventListener('click', function(){
        try{ localStorage.setItem('flyrf_start_active', this.getAttribute('href') || ''); }catch(e){}
      });
    }
  }
  applyActiveButtons();

  function updateCounters(){
    if(busy) return;
    busy=true;
    fetch('/packet_counters',{cache:'no-store'})
      .then(function(r){ return r.ok ? r.json() : Promise.reject(); })
      .then(function(data){
        if(!data) return;
        el.innerHTML='Tx&nbsp; '+(data.tx||0)+'&nbsp;&nbsp;&nbsp; Rx&nbsp; '+(data.rx||0);
        if(lanPkt) lanPkt.innerHTML='Tx&nbsp; '+(data.lan_tx||0)+'&nbsp;&nbsp;&nbsp; Rx&nbsp; '+(data.lan_rx||0);
        if(lanIp && data.lan_ip!==undefined) lanIp.textContent=String(data.lan_ip);
        if(lanState && data.lan_link_text!==undefined) lanState.textContent=String(data.lan_link_text)+' / '+((data.lan_dhcp||false)?'DHCP':'Static');
        if(lanMac && data.lan_mac!==undefined) lanMac.textContent=String(data.lan_mac);
        if(gnssTime && data.utc_text!==undefined) gnssTime.innerHTML=String(data.utc_text);
        if(gnssSats && data.satellites!==undefined) gnssSats.textContent=String(data.satellites);
        if(gnssCoords && data.coords_text!==undefined) gnssCoords.innerHTML=String(data.coords_text);
        if(gnssGpsAlt && data.gps_altitude_text!==undefined) gnssGpsAlt.innerHTML=String(data.gps_altitude_text);
        if(gnssAlt && data.altitude_text!==undefined) gnssAlt.innerHTML=String(data.altitude_text);
      })
      .catch(function(){} )
      .finally(function(){ busy=false; });
  }
  updateCounters();
  setInterval(updateCounters,1000);
})();
</script>)JS");
    html += F("</div></body></html>");
    return html;
}

static String settingsPage()
{
    String html = htmlBegin("&#1053;&#1072;&#1089;&#1090;&#1088;&#1086;&#1081;&#1082;&#1080;");
    html += F("<h1>&#1053;&#1072;&#1089;&#1090;&#1088;&#1086;&#1081;&#1082;&#1080;</h1><form id='settings_form' action='/input' method='GET'>");
    html += F("<div class='settingsBlock'><div class='blockTitle'>&#1051;&#1086;&#1082;&#1072;&#1083;&#1100;&#1085;&#1099;&#1077; &#1082;&#1086;&#1086;&#1088;&#1076;&#1080;&#1085;&#1072;&#1090;&#1099; &#1080; &#1088;&#1077;&#1078;&#1080;&#1084; &#1088;&#1072;&#1073;&#1086;&#1090;&#1099;</div><table>");
    html += F("<tr><th>&#1056;&#1077;&#1078;&#1080;&#1084; &#1088;&#1072;&#1073;&#1086;&#1090;&#1099;</th><td><select class='wLong settingsTopCtl50 compactSelect' style='width:210px;min-width:210px;max-width:210px;' name='mode'>");
    html += String("<option ") + sel((settings ? settings->mode : FLYRF_MODE_NORMAL) == FLYRF_MODE_NORMAL) + " value='0'>&#1053;&#1086;&#1088;&#1084;&#1072;&#1083;&#1100;&#1085;&#1099;&#1081; &#1088;&#1077;&#1078;&#1080;&#1084;</option>";
    html += String("<option ") + sel((settings ? settings->mode : FLYRF_MODE_NORMAL) == FLYRF_MODE_TXRX_TEST1) + " value='1'>&#1057;&#1072;&#1084;&#1086;&#1083;&#1077;&#1090; &#1079;&#1072;&#1092;&#1080;&#1082;&#1089;&#1080;&#1088;&#1086;&#1074;&#1072;&#1085;</option>";
    html += String("<option ") + sel((settings ? settings->mode : FLYRF_MODE_NORMAL) == FLYRF_MODE_TXRX_TEST2) + " value='2'>&#1057;&#1072;&#1084;&#1086;&#1083;&#1077;&#1090; &#1074; &#1087;&#1086;&#1083;&#1077;&#1090;&#1077;</option>";
    html += String("<option ") + sel((settings ? settings->mode : FLYRF_MODE_NORMAL) == FLYRF_MODE_TXRX_TEST3) + " value='3'> + 5 &#1089;&#1080;&#1085;&#1093;&#1088;&#1086;&#1085;&#1085;&#1086;</option>";
    html += String("<option ") + sel((settings ? settings->mode : FLYRF_MODE_NORMAL) == FLYRF_MODE_TXRX_TEST4) + " value='4'> + 5 &#1085;&#1077; &#1089;&#1080;&#1085;&#1093;&#1088;&#1086;&#1085;&#1085;&#1086;</option>";
    html += F("</select></td></tr>");
    html += F("<tr><th>&#1055;&#1086;&#1083;&#1091;&#1096;&#1072;&#1088;&#1080;&#1077; (&#1089;&#1077;&#1074;&#1077;&#1088;/&#1102;&#1075;)</th><td><select class='wShort settingsTopCtl50 compactSelect' style='width:100px;min-width:100px;max-width:100px; name='input_N_S'>");
    html += String("<option ") + sel(settings && settings->input_N_S == 0) + " value='0'>N</option>";
    html += String("<option ") + sel(settings && settings->input_N_S != 0) + " value='1'>S</option>";
    html += F("</select></td></tr>");
    html += F("<tr><th>latitude (&#1096;&#1080;&#1088;&#1086;&#1090;&#1072; 0&#176; - 90&#176;)</th><td><input class='settingsTopCtl30 compactSelect' style='width:120px;min-width:120px;max-width:120px; type='text' name='local_latitude' value='");
    html += String(settings ? settings->local_latitude : 55.95501f, 5);
    html += F("'></td></tr>");
    html += F("<tr><th>&#1044;&#1086;&#1083;&#1075;&#1086;&#1090;&#1072; (&#1074;&#1086;&#1089;&#1090;&#1086;&#1082;/&#1079;&#1072;&#1087;&#1072;&#1076;)</th><td><select class='wShort settingsTopCtl50 compactSelect' style='width:100px;min-width:100px;max-width:100px; name='input_E_W'>");
    html += String("<option ") + sel(settings && settings->input_E_W == 0) + " value='0'>E</option>";
    html += String("<option ") + sel(settings && settings->input_E_W != 0) + " value='1'>W</option>";
    html += F("</select></td></tr>");
    html += F("<tr><th>longitude (0&#176; - 180&#176;)</th><td><input class='settingsTopCtl30 compactSelect' style='width:120px;min-width:120px;max-width:120px; type='text' name='local_longitude' value='");
    html += String(settings ? settings->local_longitude : 37.23166f, 5);
    html += F("'></td></tr>");
    html += F("</table></div><div class='btnRowEqual'><button class='btn' type='submit'><span class='btnIcon'>&#128190;</span>&#1057;&#1086;&#1093;&#1088;&#1072;&#1085;&#1080;&#1090;&#1100; &#1080; &#1086;&#1073;&#1085;&#1086;&#1074;&#1080;&#1090;&#1100;</button><a class='btn' href='/'><span class='btnIcon'>&#8634;</span>&#1042;&#1086;&#1079;&#1074;&#1088;&#1072;&#1090; &#1073;&#1077;&#1079; &#1089;&#1086;&#1093;&#1088;&#1072;&#1085;&#1077;&#1085;&#1080;&#1103;</a></div></form>");
    html += F("</div></body></html>");

    return html;
}

static String settingsSavedPage()
{
    String html = htmlBegin("&#1053;&#1086;&#1074;&#1099;&#1077; &#1085;&#1072;&#1089;&#1090;&#1088;&#1086;&#1081;&#1082;&#1080;");
    html += F("<h1>&#1053;&#1086;&#1074;&#1099;&#1077; &#1085;&#1072;&#1089;&#1090;&#1088;&#1086;&#1081;&#1082;&#1080;:</h1><table>");
    html += F("<tr><th>TRACKER send</th><td>"); html += trackerSendName(settings ? settings->tracker_send : 0); html += F("</td></tr>");
    html += F("<tr><th>&#1056;&#1077;&#1078;&#1080;&#1084; &#1088;&#1072;&#1073;&#1086;&#1090;&#1099;</th><td>"); html += workModeName(settings ? settings->mode : DEVICE_MODE_NORMAL); html += F("</td></tr>");
    html += F("<tr><th>&#1048;&#1089;&#1090;&#1086;&#1095;&#1085;&#1080;&#1082; &#1082;&#1086;&#1086;&#1088;&#1076;&#1080;&#1085;&#1072;&#1090;</th><td>"); html += ((settings && FlyRfMode_usesLocalCoordinates(settings->mode)) ? "WEB/local" : "GPS/GNSS"); html += F("</td></tr>");
    html += F("<tr><th>&#1055;&#1086;&#1083;&#1091;&#1096;&#1072;&#1088;&#1080;&#1077;</th><td>"); html += (settings && settings->input_N_S ? "S" : "N"); html += F("</td></tr>");
    html += F("<tr><th>Latitude</th><td>"); html += String(settings ? settings->local_latitude : 55.95501f, 5); html += F("</td></tr>");
    html += F("<tr><th>&#1044;&#1086;&#1083;&#1075;&#1086;&#1090;&#1072;</th><td>"); html += (settings && settings->input_E_W ? "W" : "E"); html += F("</td></tr>");
    html += F("<tr><th>Longitude</th><td>"); html += String(settings ? settings->local_longitude : 37.23166f, 5); html += F("</td></tr>");
    html += F("<tr><th>WiFi AP IP</th><td>"); html += WiFi_apIP().toString(); html += F("</td></tr>");
    html += F("</table>");
    html += loraRuntimeSettingsBlock("&#1056;&#1077;&#1072;&#1083;&#1100;&#1085;&#1099;&#1077; &#1085;&#1072;&#1089;&#1090;&#1088;&#1086;&#1081;&#1082;&#1080; LoRa / LMIC");
    html += F("<div class='hr'></div><div class='confirmTitle'>&#1053;&#1072;&#1089;&#1090;&#1088;&#1086;&#1081;&#1082;&#1080; &#1089;&#1086;&#1093;&#1088;&#1072;&#1085;&#1077;&#1085;&#1099; &#1073;&#1077;&#1079; &#1087;&#1077;&#1088;&#1077;&#1079;&#1072;&#1075;&#1088;&#1091;&#1079;&#1082;&#1080;.</div><div class='btnRow'><a class='btn' href='/'><span class='btnIcon'>&#127968;</span>&#1053;&#1072; &#1075;&#1083;&#1072;&#1074;&#1085;&#1091;&#1102;</a></div></div></body></html>");
    return html;
}

static String hardwarePage()
{
    String html = htmlBegin("&#1040;&#1087;&#1087;&#1072;&#1088;&#1072;&#1090;&#1085;&#1099;&#1077; &#1085;&#1072;&#1089;&#1090;&#1088;&#1086;&#1081;&#1082;&#1080;");
    html += F("<h1>&#1040;&#1087;&#1087;&#1072;&#1088;&#1072;&#1090;&#1085;&#1099;&#1077; &#1085;&#1072;&#1089;&#1090;&#1088;&#1086;&#1081;&#1082;&#1080;</h1><form id='hardware_form' action='/hardware_input' method='GET'>");
    html += F("<div class='settingsBlock'><div class='blockTitle'>&#1044;&#1080;&#1089;&#1087;&#1083;&#1077;&#1081;</div><table>");
    html += F("<tr><th>&#1044;&#1080;&#1089;&#1087;&#1083;&#1077;&#1081;</th><td><select class='wMed hardwareCtl50' name='display_enabled'>");
    html += String("<option ") + sel(settings && !settings->display_enabled) + " value='0'>&#1042;&#1099;&#1082;&#1083;&#1102;&#1095;&#1080;&#1090;&#1100;</option>";
    html += String("<option ") + sel(settings == nullptr || settings->display_enabled) + " value='1'>&#1042;&#1082;&#1083;&#1102;&#1095;&#1080;&#1090;&#1100;</option>";
    html += F("</select></td></tr>");
    html += F("<tr><th>&#1054;&#1090;&#1086;&#1073;&#1088;&#1072;&#1078;&#1072;&#1090;&#1100; &#1076;&#1072;&#1085;&#1085;&#1099;&#1077; LoRa &#1085;&#1072; TFT</th><td><select class='wMed hardwareCtl50' name='rssi_view'>");
    html += String("<option ") + sel(settings && settings->rssi_view == 0) + " value='0'>&#1042;&#1099;&#1082;&#1083;&#1102;&#1095;&#1077;&#1085;</option>";
    html += String("<option ") + sel(settings && settings->rssi_view != 0) + " value='1'>&#1042;&#1082;&#1083;&#1102;&#1095;&#1077;&#1085;</option>";
    html += F("</select></td></tr>");
    html += F("<tr><th>&#1055;&#1072;&#1088;&#1072;&#1084;&#1077;&#1090;&#1088;&#1099; LAN &#1085;&#1072; TFT</th><td><select class='wMed hardwareCtl50' name='lan_state_view'>");
    html += String("<option ") + sel(!(settings && settings->lan_state_view)) + " value='0'>&#1042;&#1099;&#1082;&#1083;&#1102;&#1095;&#1080;&#1090;&#1100;</option>";
    html += String("<option ") + sel(settings && settings->lan_state_view) + " value='1'>&#1042;&#1082;&#1083;&#1102;&#1095;&#1080;&#1090;&#1100;</option>";
    html += F("</select></td></tr>");
    html += F("<tr><th>GPS Sat / координаты на TFT</th><td><select class='wMed hardwareCtl50' name='gps_state_view'>");
    html += String("<option ") + sel(!(settings && settings->gps_state_view)) + " value='0'>&#1042;&#1099;&#1082;&#1083;&#1102;&#1095;&#1080;&#1090;&#1100;</option>";
    html += String("<option ") + sel(settings && settings->gps_state_view) + " value='1'>&#1042;&#1082;&#1083;&#1102;&#1095;&#1080;&#1090;&#1100;</option>";
    html += F("</select></td></tr>");
    html += F("<tr><th>&#1042;&#1099;&#1074;&#1086;&#1076; &#1090;&#1077;&#1082;&#1089;&#1090;&#1086;&#1074;&#1086;&#1081; &#1080;&#1085;&#1092;&#1086;&#1088;&#1084;&#1072;&#1094;&#1080;&#1080; &#1085;&#1072; &#1076;&#1080;&#1089;&#1087;&#1083;&#1077;&#1081;</th><td><select class='wLong hardwareCtl30' name='display_set'>");
    html += String("<option ") + sel((settings ? settings->display_set : INFO_DISTLAY_OFF) == INFO_DISTLAY_OFF) + " value='0'>&#1053;&#1077; &#1074;&#1099;&#1074;&#1086;&#1076;&#1080;&#1090;&#1100;</option>";
    html += String("<option ") + sel((settings ? settings->display_set : INFO_DISPLAY_COORDINATE) == INFO_DISPLAY_COORDINATE) + " value='1'>&#1057; &#1082;&#1086;&#1086;&#1088;&#1076;&#1080;&#1085;&#1072;&#1090;&#1072;&#1084;&#1080;</option>";
    html += String("<option ") + sel((settings ? settings->display_set : INFO_DISPLAY_MAXI) == INFO_DISPLAY_MAXI) + " value='2'>&#1055;&#1086;&#1083;&#1085;&#1072;&#1103; &#1080;&#1085;&#1092;&#1086;&#1088;&#1084;&#1072;&#1094;&#1080;&#1103;</option>";
    html += String("<option ") + sel((settings ? settings->display_set : INFO_DISPLAY_LORA_RAW) == INFO_DISPLAY_LORA_RAW) + " value='3'>LoRa RAW RX</option>";
    html += F("</select></td></tr>");
    html += F("<tr><th>&#1053;&#1072;&#1076;&#1087;&#1080;&#1089;&#1100; SOS &#1085;&#1072; TFT</th><td><select class='wMed hardwareCtl50' name='display_sos'>");
    html += String("<option ") + sel(!(settings && settings->display_sos)) + " value='0'>&#1042;&#1099;&#1082;&#1083;&#1102;&#1095;&#1080;&#1090;&#1100;</option>";
    html += String("<option ") + sel(settings && settings->display_sos) + " value='1'>&#1042;&#1082;&#1083;&#1102;&#1095;&#1080;&#1090;&#1100;</option>";
    html += F("</select></td></tr>");
    html += F("<tr><th>&#1044;&#1080;&#1072;&#1087;&#1072;&#1079;&#1086;&#1085; &#1087;&#1088;&#1086;&#1089;&#1084;&#1086;&#1090;&#1088;&#1072; TFT</th><td><select class='wLong hardwareCtl50 compactSelect' style='width:210px;min-width:210px;max-width:210px;' name='radar_range_mode'>");
    html += String("<option ") + sel(!(settings && settings->radar_range_mode)) + " value='0'>&#1040;&#1074;&#1090;&#1086;&#1084;&#1072;&#1090;&#1080;&#1095;&#1077;&#1089;&#1082;&#1080;&#1081;</option>";
    html += String("<option ") + sel(settings && settings->radar_range_mode) + " value='1'>&#1060;&#1080;&#1082;&#1089;&#1080;&#1088;&#1086;&#1074;&#1072;&#1085;&#1085;&#1099;&#1081;</option>";
    html += F("</select></td></tr>");
    html += F("</table></div>");
    html += F("<div class='settingsBlock'><div class='blockTitle'>LAN / W5500</div><table>");
    html += F("<tr><th>LAN / W5500</th><td><select class='wMed hardwareCtl50' name='lan_enabled'>");
    html += String("<option ") + sel(settings && !settings->lan_enabled) + " value='0'>&#1042;&#1099;&#1082;&#1083;&#1102;&#1095;&#1080;&#1090;&#1100;</option>";
    html += String("<option ") + sel(settings == nullptr || settings->lan_enabled) + " value='1'>&#1042;&#1082;&#1083;&#1102;&#1095;&#1080;&#1090;&#1100;</option>";
    html += F("</select></td></tr>");
    html += F("<tr><th>Local IP (UDP)</th><td><input class='wLong hardwareCtl30 compactSelect' style='width:210px;min-width:210px;max-width:210px;' type='text' name='local_ip' value='");
    html += ipToString(settings ? settings->g_localIP : nullptr);
    html += F("'></td></tr>");
    html += F("<tr><th>Gateway IP</th><td><input class='wLong hardwareCtl30 compactSelect' style='width:210px;min-width:210px;max-width:210px;' type='text' name='gateway_ip' value='");
    html += ipToString(settings ? settings->g_gatewayIP : nullptr);
    html += F("'></td></tr>");
    html += F("<tr><th>Subnet mask</th><td><input class='wLong hardwareCtl30 compactSelect' style='width:210px;min-width:210px;max-width:210px;' type='text' name='subnet_mask' value='");
    html += ipToString(settings ? settings->g_subnetMask : nullptr);
    html += F("'></td></tr>");
    html += F("<tr><th>DNS server</th><td><input class='wLong hardwareCtl30 compactSelect' style='width:210px;min-width:210px;max-width:210px;' type='text' name='dns_server' value='");
    html += ipToString(settings ? settings->g_dns_server : nullptr);
    html += F("'></td></tr>");
    html += F("<tr><th>UDP port</th><td><input class='wLong hardwareCtl30 compactSelect' style='width:210px;min-width:210px;max-width:210px;' type='number' min='1' max='65535' name='udp_port' value='");
    html += String(udpPortValue());
    html += F("'></td></tr>");
    html += F("<tr><th>MAC</th><td>");
    html += LAN_macAddressStr();
    html += F("</td></tr>");
    html += F("<tr><th>Runtime IP</th><td>");
    html += LAN_localIPStr();
    html += F("</td></tr>");
    html += F("<tr><th>State</th><td>");
    html += LAN_linkStatusStr();
    html += LAN_dhcpLeaseActive() ? F(" / DHCP") : F(" / Static");
    html += F("</td></tr></table></div>");
    html += F("<div class='settingsBlock'><div class='blockTitle'>&#1050;&#1086;&#1085;&#1090;&#1088;&#1086;&#1083;&#1100; &#1086;&#1087;&#1072;&#1089;&#1085;&#1086;&#1075;&#1086; &#1089;&#1073;&#1083;&#1080;&#1078;&#1077;&#1085;&#1080;&#1103;</div><table>");
    html += F("<tr><th>&#1058;&#1088;&#1077;&#1074;&#1086;&#1075;&#1072; &#1074;&#1085;&#1080;&#1084;&#1072;&#1085;&#1080;&#1077; (m)</th><td><input class='wMed hardwareCtl50' type='number' name='alarm_attention' min='2000' max='3000' value='");
    html += String(settings ? settings->alarm_attention : 3000);
    html += F("'></td></tr>");
    html += F("<tr><th>&#1058;&#1088;&#1077;&#1074;&#1086;&#1075;&#1072; &#1087;&#1088;&#1077;&#1076;&#1091;&#1087;&#1088;&#1077;&#1078;&#1076;&#1077;&#1085;&#1080;&#1077; (m)</th><td><input class='wMed hardwareCtl50' type='number' name='alarm_warning' min='200' max='2000' value='");
    html += String(settings ? settings->alarm_warning : 2000);
    html += F("'></td></tr>");
    html += F("<tr><th>&#1058;&#1088;&#1077;&#1074;&#1086;&#1075;&#1072; &#1086;&#1087;&#1072;&#1089;&#1085;&#1086;&#1089;&#1090;&#1100; (m)</th><td><input class='wMed hardwareCtl50' type='number' name='alarm_danger' min='10' max='500' value='");
    html += String(settings ? settings->alarm_danger : 500);
    html += F("'></td></tr>");
    html += F("<tr><th>&#1058;&#1088;&#1077;&#1074;&#1086;&#1075;&#1072; &#1074;&#1099;&#1089;&#1086;&#1090;&#1072; (m)</th><td><input class='wMed hardwareCtl50' type='number' name='alarm_height' min='30' max='300' value='");
    html += String(settings ? settings->alarm_height : 300);
    html += F("'></td></tr>");
    html += F("</table></div>");
    html += F("<div class='settingsBlock hardwareCompactBlock'><div class='blockTitle'>LoRa</div><table>");
    const uint8_t currentLoRaProfile = settings ? settings->lora_profile : 0;
    const uint8_t currentLoRaFreq = settings ? settings->lora_fixed_freq : 8;
    const uint8_t currentLoRaSf = settings ? settings->lora_custom_sf : 12;
    const uint8_t currentLoRaBw = settings ? settings->lora_custom_bw : 0;
    const uint8_t currentLoRaCr = settings ? settings->lora_custom_cr : 3;
    html += F("<tr><th>RF &#1087;&#1088;&#1086;&#1090;&#1086;&#1082;&#1086;&#1083;</th><td><select class='wLong hardwareCtl50 compactSelect' name='rf_protocol'>");
    html += String("<option ") + sel(currentLoRaProfile == 0) + " value='0'>OGN compatible</option>";
    html += String("<option ") + sel(currentLoRaProfile == 1) + " value='1'>Long range (SF9 BW125 CR4/6)</option>";
    html += String("<option ") + sel(currentLoRaProfile == 2) + " value='2'>Max range (SF12 BW125 CR4/8)</option>";
    html += String("<option ") + sel(currentLoRaProfile == 3) + " value='3'>Fast robust (SF8 BW125 CR4/6)</option>";
    html += String("<option ") + sel(currentLoRaProfile == 4) + " value='4'>Custom SF/BW/CR</option>";
    html += F("</select></td></tr>");
    html += F("<tr><th>LoRa SF</th><td><select class='wMed hardwareCtl50 compactSelect' name='lora_custom_sf'>");
    for (uint8_t sf = 7; sf <= 12; ++sf)
    {
        html += String("<option ") + sel(currentLoRaSf == sf) + " value='" + String(sf) + "'>SF" + String(sf) + "</option>";
    }
    html += F("</select></td></tr>");
    html += F("<tr><th>LoRa BW</th><td><select class='wMed hardwareCtl50 compactSelect' name='lora_custom_bw'>");
    for (uint8_t bw = 0; bw <= 2; ++bw)
    {
        html += String("<option ") + sel(currentLoRaBw == bw) + " value='" + String(bw) + "'>" + loraBwName(bw) + "</option>";
    }
    html += F("</select></td></tr>");
    html += F("<tr><th>LoRa CR</th><td><select class='wMed hardwareCtl50 compactSelect' name='lora_custom_cr'>");
    for (uint8_t cr = 0; cr <= 3; ++cr)
    {
        html += String("<option ") + sel(currentLoRaCr == cr) + " value='" + String(cr) + "'>CR4/" + String(cr + 5) + "</option>";
    }
    html += F("</select></td></tr>");
    html += F("<tr><th>LoRa &#1095;&#1072;&#1089;&#1090;&#1086;&#1090;&#1072;</th><td><select class='wLong hardwareCtl50 compactSelect' name='lora_fixed_freq'>");
    for (uint8_t i = 0; i <= 9; ++i)
    {
        html += String("<option ") + sel(currentLoRaFreq == i) + " value='" + String(i) + "'>868." + String(i) + " MHz</option>";
    }
    html += F("</select></td></tr>");
    html += F("</table></div>");
    html += loraRuntimeSettingsBlock("&#1056;&#1077;&#1072;&#1083;&#1100;&#1085;&#1099;&#1077; &#1085;&#1072;&#1089;&#1090;&#1088;&#1086;&#1081;&#1082;&#1080; LoRa / LMIC");
    html += F("<div class='settingsBlock'><div class='blockTitle'>&#1057;&#1083;&#1091;&#1078;&#1077;&#1073;&#1085;&#1099;&#1077; &#1087;&#1072;&#1088;&#1072;&#1084;&#1077;&#1090;&#1088;&#1099;</div><table>");
    html += F("<tr><th>&#1059;&#1089;&#1090;&#1072;&#1085;&#1086;&#1074;&#1080;&#1090;&#1100; &#1085;&#1072;&#1089;&#1090;&#1088;&#1086;&#1081;&#1082;&#1080; &#1087;&#1086; &#1091;&#1084;&#1086;&#1083;&#1095;&#1072;&#1085;&#1080;&#1102;</th><td><select class='wMed hardwareCtl30' name='default_settings'>");
    html += F("<option value='0' selected>&#1053;&#1077; &#1091;&#1089;&#1090;&#1072;&#1085;&#1072;&#1074;&#1083;&#1080;&#1074;&#1072;&#1090;&#1100;</option>");
    html += F("<option value='1'>&#1059;&#1089;&#1090;&#1072;&#1085;&#1086;&#1074;&#1080;&#1090;&#1100;</option>");
    html += F("</select></td></tr>");
    char blockAddr1[16];
    char blockAddr2[16];
    char blockAddr3[16];
    formatBlockAddr(blockAddr1, sizeof(blockAddr1), settings ? settings->block_addr[0] : 0U);
    formatBlockAddr(blockAddr2, sizeof(blockAddr2), settings ? settings->block_addr[1] : 0U);
    formatBlockAddr(blockAddr3, sizeof(blockAddr3), settings ? settings->block_addr[2] : 0U);
    html += F("<tr><th>&#1041;&#1083;&#1086;&#1082;&#1080;&#1088;&#1086;&#1074;&#1072;&#1090;&#1100; &#1072;&#1076;&#1088;&#1077;&#1089; 1</th><td><input class='hardwareCtl60' type='text' name='block_addr' maxlength='6' minlength='6' pattern='[0-9A-Fa-f]{6}' required title='&#1042;&#1074;&#1077;&#1076;&#1080;&#1090;&#1077; 6 HEX-&#1089;&#1080;&#1084;&#1074;&#1086;&#1083;&#1086;&#1074;: 0-9 &#1080; A-F' placeholder='000000' autocomplete='off' spellcheck='false' value='");
    html += blockAddr1;
    html += F("'></td></tr>");
    html += F("<tr><th>&#1041;&#1083;&#1086;&#1082;&#1080;&#1088;&#1086;&#1074;&#1072;&#1090;&#1100; &#1072;&#1076;&#1088;&#1077;&#1089; 2</th><td><input class='hardwareCtl60' type='text' name='block_addr2' maxlength='6' minlength='6' pattern='[0-9A-Fa-f]{6}' required title='&#1042;&#1074;&#1077;&#1076;&#1080;&#1090;&#1077; 6 HEX-&#1089;&#1080;&#1084;&#1074;&#1086;&#1083;&#1086;&#1074;: 0-9 &#1080; A-F' placeholder='000000' autocomplete='off' spellcheck='false' value='");
    html += blockAddr2;
    html += F("'></td></tr>");
    html += F("<tr><th>&#1041;&#1083;&#1086;&#1082;&#1080;&#1088;&#1086;&#1074;&#1072;&#1090;&#1100; &#1072;&#1076;&#1088;&#1077;&#1089; 3</th><td><input class='hardwareCtl60' type='text' name='block_addr3' maxlength='6' minlength='6' pattern='[0-9A-Fa-f]{6}' required title='&#1042;&#1074;&#1077;&#1076;&#1080;&#1090;&#1077; 6 HEX-&#1089;&#1080;&#1084;&#1074;&#1086;&#1083;&#1086;&#1074;: 0-9 &#1080; A-F' placeholder='000000' autocomplete='off' spellcheck='false' value='");
    html += blockAddr3;
    html += F("'></td></tr>");
    html += F("<tr><th>&#1055;&#1088;&#1080;&#1077;&#1084;&#1085;&#1080;&#1082; ADSB 1090</th><td><select class='wLong hardwareCtl60 compactSelect' name='adsb_receiver'>");
    html += String("<option ") + sel((settings ? settings->adsb_receiver : ADSB_RECEIVER_EXTERNAL) == ADSB_RECEIVER_EXTERNAL) + " value='0'>RP2040 Serial</option>";
    html += String("<option ") + sel((settings ? settings->adsb_receiver : ADSB_RECEIVER_EXTERNAL) == ADSB_RECEIVER_INTERNAL) + " value='1'>ESP32S3 GPIO2</option>";
    html += F("</select></td></tr>");
    html += F("<tr><th>&#1059;&#1088;&#1086;&#1074;&#1077;&#1085;&#1100; &#1087;&#1086;&#1088;&#1086;&#1075;&#1072; &#1091;&#1089;&#1080;&#1083;&#1080;&#1090;&#1077;&#1083;&#1103; 1090&#1084;&#1043;&#1094; (mV)</th><td><input class='hardwareCtl60' type='number' name='threshold_level' min='400' max='1500' value='");
    html += String(settings ? settings->threshold_level : 910);
    html += F("'></td></tr>");
    html += blockAddrInputFilterScript("block_addr");
    html += blockAddrInputFilterScript("block_addr2");
    html += blockAddrInputFilterScript("block_addr3");
    html += F("</table></div><div class='btnRowEqual'><button class='btn' type='submit'><span class='btnIcon'>&#128190;</span>&#1057;&#1086;&#1093;&#1088;&#1072;&#1085;&#1080;&#1090;&#1100; &#1080; &#1086;&#1073;&#1085;&#1086;&#1074;&#1080;&#1090;&#1100;</button><a class='btn' href='/'><span class='btnIcon'>&#8634;</span>&#1042;&#1086;&#1079;&#1074;&#1088;&#1072;&#1090; &#1073;&#1077;&#1079; &#1089;&#1086;&#1093;&#1088;&#1072;&#1085;&#1077;&#1085;&#1080;&#1103;</a></div></form>");
    html += F("</div></body></html>");

    return html;
}

static String hardwareSavedPage(bool defaultsLoaded)
{
    String html = htmlBegin("&#1053;&#1086;&#1074;&#1099;&#1077; &#1072;&#1087;&#1087;&#1072;&#1088;&#1072;&#1090;&#1085;&#1099;&#1077; &#1085;&#1072;&#1089;&#1090;&#1088;&#1086;&#1081;&#1082;&#1080;");
    html += F("<h1>&#1053;&#1086;&#1074;&#1099;&#1077; &#1072;&#1087;&#1087;&#1072;&#1088;&#1072;&#1090;&#1085;&#1099;&#1077; &#1085;&#1072;&#1089;&#1090;&#1088;&#1086;&#1081;&#1082;&#1080;:</h1><table>");
    html += F("<tr><th>&#1044;&#1080;&#1089;&#1087;&#1083;&#1077;&#1081;</th><td>"); html += yesNo(settings ? settings->display_enabled : 1, "&#1042;&#1099;&#1082;&#1083;&#1102;&#1095;&#1077;&#1085;", "&#1042;&#1082;&#1083;&#1102;&#1095;&#1077;&#1085;"); html += F("</td></tr>");
    html += F("<tr><th>&#1044;&#1072;&#1085;&#1085;&#1099;&#1077; LoRa &#1085;&#1072; TFT</th><td>"); html += yesNo(settings ? settings->rssi_view : 0, "&#1042;&#1099;&#1082;&#1083;&#1102;&#1095;&#1077;&#1085;", "&#1042;&#1082;&#1083;&#1102;&#1095;&#1077;&#1085;"); html += F("</td></tr>");
    html += F("<tr><th>&#1056;&#1077;&#1078;&#1080;&#1084; &#1088;&#1072;&#1073;&#1086;&#1090;&#1099; &#1076;&#1080;&#1089;&#1087;&#1083;&#1077;&#1103;</th><td>"); html += displayModeName(settings ? settings->display_set : INFO_DISTLAY_OFF); html += F("</td></tr>");
    html += F("<tr><th>LoRa mode</th><td>"); html += loraFixedModeName(settings ? settings->lora_fixed_channel : 1); html += F("</td></tr>");
    html += F("<tr><th>LoRa freq</th><td>"); html += loraFixedFreqName(settings ? settings->lora_fixed_freq : 0); html += F("</td></tr>");
    html += F("<tr><th>RF protocol</th><td>"); html += RF_GetProfileName(settings ? settings->lora_profile : 0); html += F("</td></tr>");
    html += F("<tr><th>LoRa custom SF/BW/CR</th><td>SF"); html += String(settings ? settings->lora_custom_sf : 12); html += F(" / "); html += loraBwName(settings ? settings->lora_custom_bw : 0); html += F(" / CR"); html += loraCrName(settings ? settings->lora_custom_cr : 3); html += F("</td></tr>");
    html += F("<tr><th>LoRa TX power (&#1085;&#1072;&#1089;&#1090;&#1088;&#1086;&#1081;&#1082;&#1072;)</th><td>"); html += String(settings ? settings->txpower : 20); html += F(" dBm</td></tr>");
    html += F("<tr><th>LAN / W5500</th><td>"); html += yesNo(settings ? settings->lan_enabled : 1, "&#1042;&#1099;&#1082;&#1083;&#1102;&#1095;&#1077;&#1085;", "&#1042;&#1082;&#1083;&#1102;&#1095;&#1077;&#1085;"); html += F("</td></tr>");
    html += F("<tr><th>&#1055;&#1072;&#1088;&#1072;&#1084;&#1077;&#1090;&#1088;&#1099; LAN &#1085;&#1072; TFT</th><td>"); html += yesNo(settings ? settings->lan_state_view : 0, "&#1042;&#1099;&#1082;&#1083;&#1102;&#1095;&#1080;&#1090;&#1100;", "&#1042;&#1082;&#1083;&#1102;&#1095;&#1080;&#1090;&#1100;"); html += F("</td></tr>");
    html += F("<tr><th>GPS Sat / координаты на TFT</th><td>"); html += yesNo(settings ? settings->gps_state_view : 1, "&#1042;&#1099;&#1082;&#1083;&#1102;&#1095;&#1080;&#1090;&#1100;", "&#1042;&#1082;&#1083;&#1102;&#1095;&#1080;&#1090;&#1100;"); html += F("</td></tr>");
    html += F("<tr><th>&#1053;&#1072;&#1076;&#1087;&#1080;&#1089;&#1100; SOS &#1085;&#1072; TFT</th><td>"); html += yesNo(settings ? settings->display_sos : 1, "&#1042;&#1099;&#1082;&#1083;&#1102;&#1095;&#1080;&#1090;&#1100;", "&#1042;&#1082;&#1083;&#1102;&#1095;&#1080;&#1090;&#1100;"); html += F("</td></tr>");
    html += F("<tr><th>&#1044;&#1080;&#1072;&#1087;&#1072;&#1079;&#1086;&#1085; &#1087;&#1088;&#1086;&#1089;&#1084;&#1086;&#1090;&#1088;&#1072; TFT</th><td>"); html += radarRangeModeName(settings ? settings->radar_range_mode : 0); html += F("</td></tr>");
    html += F("<tr><th>&#1058;&#1088;&#1077;&#1074;&#1086;&#1075;&#1072; &#1074;&#1085;&#1080;&#1084;&#1072;&#1085;&#1080;&#1077; (m)</th><td>"); html += String(settings ? settings->alarm_attention : 3000); html += F("</td></tr>");
    html += F("<tr><th>&#1058;&#1088;&#1077;&#1074;&#1086;&#1075;&#1072; &#1087;&#1088;&#1077;&#1076;&#1091;&#1087;&#1088;&#1077;&#1078;&#1076;&#1077;&#1085;&#1080;&#1077; (m)</th><td>"); html += String(settings ? settings->alarm_warning : 2000); html += F("</td></tr>");
    html += F("<tr><th>&#1058;&#1088;&#1077;&#1074;&#1086;&#1075;&#1072; &#1086;&#1087;&#1072;&#1089;&#1085;&#1086;&#1089;&#1090;&#1100; (m)</th><td>"); html += String(settings ? settings->alarm_danger : 500); html += F("</td></tr>");
    html += F("<tr><th>&#1058;&#1088;&#1077;&#1074;&#1086;&#1075;&#1072; &#1074;&#1099;&#1089;&#1086;&#1090;&#1072; (m)</th><td>"); html += String(settings ? settings->alarm_height : 300); html += F("</td></tr>");
    html += F("<tr><th>&#1059;&#1089;&#1090;&#1072;&#1085;&#1086;&#1074;&#1080;&#1090;&#1100; &#1085;&#1072;&#1089;&#1090;&#1088;&#1086;&#1081;&#1082;&#1080; &#1087;&#1086; &#1091;&#1084;&#1086;&#1083;&#1095;&#1072;&#1085;&#1080;&#1102;</th><td>"); html += (defaultsLoaded ? "&#1059;&#1089;&#1090;&#1072;&#1085;&#1086;&#1074;&#1083;&#1077;&#1085;&#1086;" : "&#1053;&#1077; &#1091;&#1089;&#1090;&#1072;&#1085;&#1072;&#1074;&#1083;&#1080;&#1074;&#1072;&#1090;&#1100;"); html += F("</td></tr>");
    html += F("<tr><th>&#1041;&#1083;&#1086;&#1082;&#1080;&#1088;&#1086;&#1074;&#1072;&#1090;&#1100; &#1072;&#1076;&#1088;&#1077;&#1089;&#1072;</th><td>"); html += blockAddrSummary(); html += F("</td></tr>");
    html += F("<tr><th>&#1059;&#1088;&#1086;&#1074;&#1077;&#1085;&#1100; &#1087;&#1086;&#1088;&#1086;&#1075;&#1072; &#1091;&#1089;&#1080;&#1083;&#1080;&#1090;&#1077;&#1083;&#1103; 1090&#1084;&#1043;&#1094; (mV)</th><td>"); html += String(settings ? settings->threshold_level : 910); html += F("</td></tr>");
    html += F("<tr><th>ADSB 1090 receiver</th><td>"); html += adsbReceiverName(settings ? settings->adsb_receiver : ADSB_RECEIVER_EXTERNAL); html += F("</td></tr>");
    html += F("<tr><th>LAN Local IP (UDP)</th><td>"); html += ipToString(settings ? settings->g_localIP : nullptr); html += F("</td></tr>");
    html += F("<tr><th>LAN Gateway IP</th><td>"); html += ipToString(settings ? settings->g_gatewayIP : nullptr); html += F("</td></tr>");
    html += F("<tr><th>LAN Subnet mask</th><td>"); html += ipToString(settings ? settings->g_subnetMask : nullptr); html += F("</td></tr>");
    html += F("<tr><th>LAN DNS server</th><td>"); html += ipToString(settings ? settings->g_dns_server : nullptr); html += F("</td></tr>");
    html += F("<tr><th>UDP port</th><td>"); html += String(udpPortValue()); html += F("</td></tr>");
    html += F("<tr><th>&#1040;&#1082;&#1090;&#1080;&#1074;&#1085;&#1099;&#1081; AP IP</th><td>"); html += WiFi_apIP().toString(); html += F("</td></tr>");
    html += F("</table>");
    html += loraRuntimeSettingsBlock("&#1056;&#1077;&#1072;&#1083;&#1100;&#1085;&#1099;&#1077; &#1085;&#1072;&#1089;&#1090;&#1088;&#1086;&#1081;&#1082;&#1080; LoRa / LMIC");
    html += F("<div class='hr'></div><div class='confirmTitle'>&#1053;&#1072;&#1089;&#1090;&#1088;&#1086;&#1081;&#1082;&#1080; &#1089;&#1086;&#1093;&#1088;&#1072;&#1085;&#1077;&#1085;&#1099;.</div><div class='confirmTitle'>&#1059;&#1089;&#1090;&#1088;&#1086;&#1081;&#1089;&#1090;&#1074;&#1086; &#1087;&#1077;&#1088;&#1077;&#1079;&#1072;&#1075;&#1088;&#1091;&#1078;&#1072;&#1077;&#1090;&#1089;&#1103;...</div><div class='btnRow'><a class='btn' href='/'><span class='btnIcon'>&#127968;</span>&#1053;&#1072; &#1075;&#1083;&#1072;&#1074;&#1085;&#1091;&#1102;</a></div></div></body></html>");
    return html;
}

static String outputsPage()
{
    String html = htmlBegin("&#1059;&#1087;&#1088;&#1072;&#1074;&#1083;&#1077;&#1085;&#1080;&#1077; &#1074;&#1099;&#1074;&#1086;&#1076;&#1086;&#1084;");
    html += F("<h1>&#1059;&#1087;&#1088;&#1072;&#1074;&#1083;&#1077;&#1085;&#1080;&#1077; &#1074;&#1099;&#1074;&#1086;&#1076;&#1086;&#1084;</h1><form id='outputs_form' action='/outputs_save' method='GET'><table>");
    html += F("<tr><th>NMEA output</th><td><select class='wLong hardwareCtl30 compactSelect' style='width:210px;min-width:210px;max-width:210px;' name='nmea_out'>");
    html += String("<option ") + sel((settings ? settings->nmea_out : NMEA_OUTPUT_OFF) == NMEA_OUTPUT_OFF) + " value='0'>Off</option>";
    html += String("<option ") + sel((settings ? settings->nmea_out : NMEA_OUTPUT_OFF) == NMEA_OUTPUT_SERIAL) + " value='1'>Serial</option>";
    html += String("<option ") + sel((settings ? settings->nmea_out : NMEA_OUTPUT_OFF) == NMEA_OUTPUT_UDP) + " value='2'>UDP</option>";
    html += String("<option ") + sel((settings ? settings->nmea_out : NMEA_OUTPUT_OFF) == NMEA_OUTPUT_BLUETOOTH) + " value='3'>Bluetooth</option>";
    html += F("</select></td></tr>");
    html += F("<tr><th>UDP port</th><td><input class='wLong hardwareCtl30 compactSelect' style='width:210px;min-width:210px;max-width:210px;' type='number' min='1' max='65535' name='udp_port' value='");
    html += String(udpPortValue());
    html += F("'></td></tr>");
    html += F("<tr><th>&#1042;&#1099;&#1074;&#1086;&#1076; &#1076;&#1072;&#1085;&#1085;&#1099;&#1093; &#1074; Serial</th><td><select class='wLong hardwareCtl30 compactSelect' style='width:210px;min-width:210px;max-width:210px;' name='serial_out'>");
    html += String("<option ") + sel((settings ? settings->serial_out : OUTPUT_MODE_OFF) == OUTPUT_MODE_OFF) + " value='0'>&#1053;&#1077; &#1074;&#1099;&#1074;&#1086;&#1076;&#1080;&#1090;&#1100;</option>";
    html += String("<option ") + sel((settings ? settings->serial_out : OUTPUT_MODE_OFF) == OUTPUT_MODE_CONTAINER) + " value='1'>Container</option>";
    html += String("<option ") + sel((settings ? settings->serial_out : OUTPUT_MODE_OFF) == OUTPUT_MODE_RP2040) + " value='3'>RP2040 RX</option>";
    html += String("<option ") + sel((settings ? settings->serial_out : OUTPUT_MODE_OFF) == OUTPUT_MODE_FLARM) + " value='4'>FLARM RX</option>";
    html += String("<option ") + sel((settings ? settings->serial_out : OUTPUT_MODE_OFF) == OUTPUT_MODE_LORA_RAW) + " value='5'>LoRa RAW RX</option>";
    html += String("<option ") + sel((settings ? settings->serial_out : OUTPUT_MODE_OFF) == OUTPUT_MODE_MAVLINK) + " value='6'>MAVLink</option>";
    html += F("</select></td></tr>");
    html += F("<tr><th>&#1042;&#1099;&#1074;&#1086;&#1076; &#1076;&#1072;&#1085;&#1085;&#1099;&#1093; &#1074; RS485</th><td><select class='wLong hardwareCtl30 compactSelect' style='width:210px;min-width:210px;max-width:210px;' name='rs485_out'>");
    html += String("<option ") + sel((settings ? settings->rs485_out : OUTPUT_MODE_OFF) == OUTPUT_MODE_OFF) + " value='0'>&#1053;&#1077; &#1074;&#1099;&#1074;&#1086;&#1076;&#1080;&#1090;&#1100;</option>";
    html += String("<option ") + sel((settings ? settings->rs485_out : OUTPUT_MODE_OFF) == OUTPUT_MODE_CONTAINER) + " value='1'>Container</option>";
    html += String("<option ") + sel((settings ? settings->rs485_out : OUTPUT_MODE_OFF) == OUTPUT_MODE_NMEA) + " value='2'>NMEA</option>";
    html += String("<option ") + sel((settings ? settings->rs485_out : OUTPUT_MODE_OFF) == OUTPUT_MODE_RS485_DISPLAY) + " value='3'>&#1042;&#1085;&#1077;&#1096;&#1085;&#1080;&#1081; &#1076;&#1080;&#1089;&#1087;&#1083;&#1077;&#1081;</option>";
    html += String("<option ") + sel((settings ? settings->rs485_out : OUTPUT_MODE_OFF) == OUTPUT_MODE_FLARM) + " value='4'>FLARM RX</option>";
    html += String("<option ") + sel((settings ? settings->rs485_out : OUTPUT_MODE_OFF) == OUTPUT_MODE_MAVLINK) + " value='6'>MAVLink</option>";
    html += F("</select></td></tr></table><div class='btnRowEqual'><button class='btn' type='submit'><span class='btnIcon'>&#128190;</span>&#1057;&#1086;&#1093;&#1088;&#1072;&#1085;&#1080;&#1090;&#1100; &#1080; &#1086;&#1073;&#1085;&#1086;&#1074;&#1080;&#1090;&#1100;</button><a class='btn' href='/'><span class='btnIcon'>&#8634;</span>&#1042;&#1086;&#1079;&#1074;&#1088;&#1072;&#1090; &#1073;&#1077;&#1079; &#1089;&#1086;&#1093;&#1088;&#1072;&#1085;&#1077;&#1085;&#1080;&#1103;</a></div></form>");
    html += F("</div></body></html>");

    return html;
}

static String outputsSavedPage()
{
    String html = htmlBegin("&#1053;&#1086;&#1074;&#1099;&#1077; &#1087;&#1072;&#1088;&#1072;&#1084;&#1077;&#1090;&#1088;&#1099; &#1074;&#1099;&#1074;&#1086;&#1076;&#1072;");
    html += F("<h1>&#1053;&#1086;&#1074;&#1099;&#1077; &#1087;&#1072;&#1088;&#1072;&#1084;&#1077;&#1090;&#1088;&#1099; &#1074;&#1099;&#1074;&#1086;&#1076;&#1072;:</h1><table>");
    html += F("<tr><th>&#1042;&#1099;&#1074;&#1086;&#1076; &#1076;&#1072;&#1085;&#1085;&#1099;&#1093; &#1074; Serial</th><td>"); html += serialModeName(settings ? settings->serial_out : OUTPUT_MODE_OFF); html += F("</td></tr>");
    html += F("<tr><th>&#1042;&#1099;&#1074;&#1086;&#1076; &#1076;&#1072;&#1085;&#1085;&#1099;&#1093; &#1074; RS485</th><td>"); html += rs485ModeName(settings ? settings->rs485_out : OUTPUT_MODE_OFF); html += F("</td></tr>");
    html += F("<tr><th>NMEA output</th><td>"); html += nmeaName(settings ? settings->nmea_out : NMEA_OUTPUT_OFF); if (settings && settings->nmea_out == NMEA_OUTPUT_UDP) { html += F(" / port "); html += String(udpPortValue()); } html += F("</td></tr>");
    html += F("</table><div class='hr'></div><div class='confirmTitle'>&#1053;&#1072;&#1089;&#1090;&#1088;&#1086;&#1081;&#1082;&#1080; &#1089;&#1086;&#1093;&#1088;&#1072;&#1085;&#1077;&#1085;&#1099; &#1073;&#1077;&#1079; &#1087;&#1077;&#1088;&#1077;&#1079;&#1072;&#1075;&#1088;&#1091;&#1079;&#1082;&#1080;.</div><div class='btnRow'><a class='btn' href='/'><span class='btnIcon'>&#127968;</span>&#1053;&#1072; &#1075;&#1083;&#1072;&#1074;&#1085;&#1091;&#1102;</a></div></div></body></html>");
    return html;
}

static String aboutPage()
{
    String html = htmlBegin("&#1048;&#1085;&#1092;&#1086;&#1088;&#1084;&#1072;&#1094;&#1080;&#1103;");
    html += F("<h1>&#1048;&#1085;&#1092;&#1086;&#1088;&#1084;&#1072;&#1094;&#1080;&#1103;</h1>");
    html += F("<div class='about'>== &#1069;&#1090;&#1072; &#1087;&#1088;&#1086;&#1075;&#1088;&#1072;&#1084;&#1084;&#1072; &#1095;&#1072;&#1089;&#1090;&#1100; &#1087;&#1088;&#1086;&#1077;&#1082;&#1090;&#1072; FlyRF ==<br>");
    html += F("== &#1041;&#1072;&#1079;&#1086;&#1074;&#1099;&#1081; &#1084;&#1086;&#1076;&#1091;&#1083;&#1100; ==<br>");
    html += F("&#1055;&#1088;&#1086;&#1075;&#1088;&#1072;&#1084;&#1084;&#1072; &#1089;&#1086;&#1079;&#1076;&#1072;&#1085;&#1072; &#1089; &#1087;&#1088;&#1080;&#1084;&#1077;&#1085;&#1077;&#1085;&#1080;&#1077;&#1084;<br>");
    html += F("&#1080;&#1089;&#1082;&#1091;&#1089;&#1089;&#1090;&#1074;&#1077;&#1085;&#1085;&#1086;&#1075;&#1086; &#1080;&#1085;&#1090;&#1077;&#1083;&#1083;&#1077;&#1082;&#1090;&#1072;<br><br>");
    html += F("URL: https://www.decima.ru/contacts/<br>");
    html += F("E-mail: decima@decima.ru</div><div class='hr'></div>");
    html += F("<div class='about' style='text-align:center'>Copyright (C) 2023-2026&nbsp;&nbsp;&nbsp; &#1054;&#1054;&#1054; &#1044;&#1077;&#1094;&#1080;&#1084;&#1072;</div>");
    html += F("</div></body></html>");
    return html;
}

static String firmwarePage()
{
    String html = htmlBegin("&#1054;&#1073;&#1085;&#1086;&#1074;&#1083;&#1077;&#1085;&#1080;&#1077; &#1087;&#1088;&#1086;&#1075;&#1088;&#1072;&#1084;&#1084;&#1099;");
    html += F("<h1>&#1054;&#1073;&#1085;&#1086;&#1074;&#1083;&#1077;&#1085;&#1080;&#1077; &#1087;&#1088;&#1086;&#1075;&#1088;&#1072;&#1084;&#1084;&#1099;</h1>");
    html += F("<div class='confirmTitle'>&#1042;&#1099;&#1073;&#1077;&#1088;&#1080;&#1090;&#1077; BIN-&#1092;&#1072;&#1081;&#1083; &#1087;&#1088;&#1086;&#1096;&#1080;&#1074;&#1082;&#1080; &#1080; &#1085;&#1072;&#1078;&#1084;&#1080;&#1090;&#1077; &#1082;&#1085;&#1086;&#1087;&#1082;&#1091; &#1086;&#1073;&#1085;&#1086;&#1074;&#1083;&#1077;&#1085;&#1080;&#1103;.</div>");
    html += F("<form id='upload_form' method='POST' action='/update' enctype='multipart/form-data'>");
    html += F("<input id='fwFile' name='update' type='file' accept='.bin,application/octet-stream' hidden>");
    html += F("<div class='btnRow' style='margin-top:14px'><button class='btn' type='button' id='pickBtn'><span class='btnIcon'>&#128193;</span>&#1042;&#1099;&#1073;&#1088;&#1072;&#1090;&#1100; &#1092;&#1072;&#1081;&#1083;</button></div>");
    html += F("<div id='fwName' class='confirmTitle' style='font-size:20px;margin-top:8px'>&#1060;&#1072;&#1081;&#1083; &#1085;&#1077; &#1074;&#1099;&#1073;&#1088;&#1072;&#1085;</div>");
    html += F("<div class='btnRow'><button class='btn' type='submit' id='fwBtn'><span class='btnIcon'>&#11014;</span>&#1054;&#1073;&#1085;&#1086;&#1074;&#1080;&#1090;&#1100; &#1087;&#1088;&#1086;&#1075;&#1088;&#1072;&#1084;&#1084;&#1091;</button></div>");
    html += F("</form>");
    html += F("<div class='progressWrap'><div id='bar' class='progressBar'></div></div>");
    html += F("<div id='prg' class='confirmTitle' style='font-size:20px'>&#1055;&#1088;&#1086;&#1075;&#1088;&#1077;&#1089;&#1089;: 0%</div>");
    html += F("<div id='st' class='confirmTitle' style='font-size:18px'>&#1043;&#1086;&#1090;&#1086;&#1074; &#1082; &#1079;&#1072;&#1075;&#1088;&#1091;&#1079;&#1082;&#1077;</div>");
    html += F("<div class='btnRow'><a class='btn' href='/'><span class='btnIcon'>&#127968;</span>&#1042;&#1077;&#1088;&#1085;&#1091;&#1090;&#1100;&#1089;&#1103; &#1085;&#1072; &#1075;&#1083;&#1072;&#1074;&#1085;&#1091;&#1102;</a></div>");
    html += F(R"rawliteral(
<script>
(function(){
  var form=document.getElementById('upload_form');
  var fileEl=document.getElementById('fwFile');
  var pickBtn=document.getElementById('pickBtn');
  var btn=document.getElementById('fwBtn');
  var nameEl=document.getElementById('fwName');
  var prg=document.getElementById('prg');
  var st=document.getElementById('st');
  var bar=document.getElementById('bar');
  var lastProgress=0;
  function setProgress(p){
    if(!isFinite(p)) p=0;
    if(p<0) p=0; if(p>100) p=100;
    lastProgress=p;
    bar.style.width=p+'%';
    prg.innerHTML='&#1055;&#1088;&#1086;&#1075;&#1088;&#1077;&#1089;&#1089;: '+p+'%';
  }
  function decodeHtml(t){ var e=document.createElement('textarea'); e.innerHTML=String(t||''); return e.value; }
  function setStatus(t){ st.textContent=decodeHtml(t); }
  function setBusy(b){ btn.disabled=b; pickBtn.disabled=b; }
  function showFile(){
    if(fileEl.files && fileEl.files.length){
      var f=fileEl.files[0];
      nameEl.innerHTML='&#1060;&#1072;&#1081;&#1083;: '+f.name+'<br>(' + f.size + ' &#1073;&#1072;&#1081;&#1090;)';
      setStatus('&#1060;&#1072;&#1081;&#1083; &#1074;&#1099;&#1073;&#1088;&#1072;&#1085;');
    } else {
      nameEl.innerHTML='&#1060;&#1072;&#1081;&#1083; &#1085;&#1077; &#1074;&#1099;&#1073;&#1088;&#1072;&#1085;';
      setStatus('&#1043;&#1086;&#1090;&#1086;&#1074; &#1082; &#1079;&#1072;&#1075;&#1088;&#1091;&#1079;&#1082;&#1077;');
    }
  }
  pickBtn.addEventListener('click', function(){ fileEl.click(); });
  fileEl.addEventListener('change', showFile);
  form.addEventListener('submit', function(e){
    e.preventDefault();
    if(!fileEl.files || !fileEl.files.length){ setStatus('&#1057;&#1085;&#1072;&#1095;&#1072;&#1083;&#1072; &#1074;&#1099;&#1073;&#1077;&#1088;&#1080;&#1090;&#1077; BIN-&#1092;&#1072;&#1081;&#1083;'); return; }
    var file=fileEl.files[0];
    if(!/\.bin$/i.test(file.name)){ setStatus('&#1053;&#1091;&#1078;&#1077;&#1085; &#1092;&#1072;&#1081;&#1083; &#1087;&#1088;&#1086;&#1096;&#1080;&#1074;&#1082;&#1080; .bin'); return; }
    setBusy(true);
    setProgress(0);
    setStatus('&#1048;&#1076;&#1105;&#1090; &#1087;&#1086;&#1076;&#1075;&#1086;&#1090;&#1086;&#1074;&#1082;&#1072;...');
    function postText(url){
      return fetch(url, {method:'POST', cache:'no-store'}).then(function(r){
        return r.text().then(function(t){
          if(!r.ok || String(t).trim()!=='OK') throw new Error(t || ('HTTP '+r.status));
          return t;
        });
      });
    }
    function delay(ms){ return new Promise(function(resolve){ setTimeout(resolve, ms); }); }
    function uploadFullFile(file){
      return new Promise(function(resolve, reject){
        var data=new FormData();
        data.append('update', file, file.name);
        var xhr=new XMLHttpRequest();
        xhr.open('POST','/update?size='+encodeURIComponent(file.size),true);
        xhr.timeout=0;
        xhr.upload.onprogress=function(ev){
          if(ev.lengthComputable){
            var p=Math.round((ev.loaded*100)/ev.total);
            setProgress(p);
            setStatus('&#1055;&#1077;&#1088;&#1077;&#1076;&#1072;&#1095;&#1072; '+p+'%...');
          }
        };
        xhr.onload=function(){
          var txt=String(xhr.responseText||'').trim();
          if(xhr.status===200 && txt==='OK') resolve();
          else reject(new Error(txt || ('HTTP '+xhr.status)));
        };
        xhr.onerror=function(){ reject(new Error('&#1054;&#1096;&#1080;&#1073;&#1082;&#1072; &#1089;&#1077;&#1090;&#1080; &#1087;&#1088;&#1080; &#1079;&#1072;&#1075;&#1088;&#1091;&#1079;&#1082;&#1077;')); };
        xhr.onabort=function(){ reject(new Error('&#1047;&#1072;&#1075;&#1088;&#1091;&#1079;&#1082;&#1072; &#1086;&#1090;&#1084;&#1077;&#1085;&#1077;&#1085;&#1072;')); };
        xhr.send(data);
      });
    }
    (async function(){
      try{
        await postText('/update_prepare?size='+encodeURIComponent(file.size));
        setStatus('&#1057;&#1083;&#1091;&#1078;&#1073;&#1099; &#1086;&#1089;&#1090;&#1072;&#1085;&#1086;&#1074;&#1083;&#1077;&#1085;&#1099;. &#1055;&#1077;&#1088;&#1077;&#1076;&#1072;&#1095;&#1072; &#1092;&#1072;&#1081;&#1083;&#1072;...');
        await delay(1200);
        await uploadFullFile(file);
        setProgress(100);
        setStatus('&#1054;&#1073;&#1085;&#1086;&#1074;&#1083;&#1077;&#1085;&#1080;&#1077; &#1079;&#1072;&#1074;&#1077;&#1088;&#1096;&#1077;&#1085;&#1086;. &#1055;&#1077;&#1088;&#1077;&#1079;&#1072;&#1075;&#1088;&#1091;&#1079;&#1082;&#1072;...');
      }catch(err){
        setBusy(false);
        setStatus(String(err && err.message ? err.message : err));
      }
    })();
  });
})();
</script>)rawliteral");
    html += F("</div></body></html>");
    return html;
}

static String monitorPage()
{
    String html = htmlBegin("&#1052;&#1086;&#1085;&#1080;&#1090;&#1086;&#1088; &#1089;&#1072;&#1084;&#1086;&#1083;&#1105;&#1090;&#1086;&#1074;");
    html += F("<h1>&#1052;&#1086;&#1085;&#1080;&#1090;&#1086;&#1088; &#1089;&#1072;&#1084;&#1086;&#1083;&#1105;&#1090;&#1086;&#1074;</h1>");
    html += F("<div class='monTools'><div class='monInfo'>&#1054;&#1073;&#1098;&#1077;&#1082;&#1090;&#1086;&#1074; &#1074; Container: <span id='cnt'>0</span> | FLARM: <span id='flarmCnt'>0</span> | ADS-B: <span id='adsbCnt'>0</span></div><div><a class='btn' href='/'><span class='btnIcon'>&#127968;</span>&#1043;&#1083;&#1072;&#1074;&#1085;&#1072;&#1103;</a><a class='btn' href='/monitor'><span class='btnIcon'>&#128260;</span>&#1054;&#1073;&#1085;&#1086;&#1074;&#1080;&#1090;&#1100;</a></div></div>");
    html += F("<table><tr><th>&#1057;&#1086;&#1088;&#1090;&#1080;&#1088;&#1086;&#1074;&#1082;&#1072;</th><td class='left'><select id='sortSel'><option value='age'>&#1057;&#1085;&#1072;&#1095;&#1072;&#1083;&#1072; &#1085;&#1086;&#1074;&#1099;&#1077;</option><option value='dist'>&#1055;&#1086; &#1088;&#1072;&#1089;&#1089;&#1090;&#1086;&#1103;&#1085;&#1080;&#1102;</option><option value='alt'>&#1055;&#1086; &#1074;&#1099;&#1089;&#1086;&#1090;&#1077;</option><option value='src'>&#1055;&#1086; &#1080;&#1089;&#1090;&#1086;&#1095;&#1085;&#1080;&#1082;&#1091;</option><option value='icao'>&#1055;&#1086; ICAO</option></select></td></tr><tr><th>&#1052;&#1072;&#1082;&#1089;. &#1074;&#1086;&#1079;&#1088;&#1072;&#1089;&#1090;, &#1089;&#1077;&#1082;</th><td class='left'><select id='ageSel'><option value='0'>&#1041;&#1077;&#1079; &#1092;&#1080;&#1083;&#1100;&#1090;&#1088;&#1072;</option><option value='10'>10</option><option value='30' selected>30</option><option value='60'>60</option><option value='120'>120</option></select></td></tr><tr><th>&#1058;&#1086;&#1083;&#1100;&#1082;&#1086; &#1089; &#1082;&#1086;&#1086;&#1088;&#1076;&#1080;&#1085;&#1072;&#1090;&#1072;&#1084;&#1080;</th><td class='left'><select id='posSel'><option value='0'>&#1053;&#1077;&#1090;</option><option value='1'>&#1044;&#1072;</option></select></td></tr></table>");
    html += F("<table class='monTable' id='acTable'><thead><tr><th>ICAO</th><th>&#1048;&#1089;&#1090;&#1086;&#1095;&#1085;&#1080;&#1082;</th><th>&#1044;&#1080;&#1089;&#1090;., &#1082;&#1084;</th><th>Latitude</th><th>Longitude</th><th>Alt</th><th>Speed</th><th>Course</th><th>RSSI</th><th>SNR</th><th>Age, ms</th></tr></thead><tbody><tr><td colspan='11'>&#1053;&#1077;&#1090; &#1076;&#1072;&#1085;&#1085;&#1099;&#1093;</td></tr></tbody></table>");
    html += F(R"rawliteral(
<script>
function esc(s){return String(s===undefined?"":s).replace(/[&<>\"]/g,function(c){return {'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c];});}
function num(v,d){var n=Number(v); return isFinite(n)?n.toFixed(d):'';}
// - body: Параметр геометрии, координаты, размера или угла.
function upd(){fetch('/aircraft.json',{cache:'no-store'}).then(r=>r.json()).then(d=>{
  document.getElementById('cnt').textContent=d.count||0;
  document.getElementById('flarmCnt').textContent=d.flarm_count||0;
  document.getElementById('adsbCnt').textContent=d.adsb_count||0;
  var items=(d.items||[]).slice();
  var maxAge=Number(document.getElementById('ageSel').value||0);
  var posOnly=document.getElementById('posSel').value==='1';
  if(maxAge>0) items=items.filter(a=>Number(a.age_ms)<=maxAge*1000);
  if(posOnly) items=items.filter(a=>Math.abs(Number(a.lat))>0.00001 || Math.abs(Number(a.lon))>0.00001);
  var sort=document.getElementById('sortSel').value;
  items.sort(function(a,b){
    if(sort==='dist') return Number(a.distance_km)-Number(b.distance_km);
    if(sort==='alt') return Number(b.altitude)-Number(a.altitude);
    if(sort==='src') return String(a.source).localeCompare(String(b.source));
    if(sort==='icao') return String(a.icao).localeCompare(String(b.icao));
    return Number(a.age_ms)-Number(b.age_ms);
  });
  var body='';
  if(!items.length){body="<tr><td colspan='11'>??? ??????</td></tr>";}
  else{
    items.forEach(function(a){
      body += '<tr>'+
        '<td class="mono">'+esc(a.icao)+'</td>'+
        '<td>'+esc(a.source)+'</td>'+
        '<td>'+num(a.distance_km,2)+'</td>'+
        '<td>'+num(a.lat,5)+'</td>'+
        '<td>'+num(a.lon,5)+'</td>'+
        '<td>'+esc(a.altitude)+'</td>'+
        '<td>'+num(a.speed,1)+'</td>'+
        '<td>'+num(a.course,1)+'</td>'+
        '<td>'+esc(a.rssi)+'</td>'+
        '<td>'+num(a.snr,1)+'</td>'+
        '<td>'+esc(a.age_ms)+'</td>'+
      '</tr>';
    });
  }
  document.querySelector('#acTable tbody').innerHTML = body;
}).catch(function(){document.querySelector('#acTable tbody').innerHTML="<tr><td colspan='11'>?????? ????????? ??????</td></tr>";});}
['sortSel','ageSel','posSel'].forEach(function(id){document.addEventListener('change', function(e){ if(e.target && e.target.id===id) upd(); });});
upd(); setInterval(upd, 2000);
</script>)rawliteral");
    html += F("</div></body></html>");
    return html;
}

// - icaoBuf: Буфер, текстовая строка или рабочее сообщение.
static void handleAircraftJson()
{
    String json;  // Текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
    json.reserve(6144);
    json += "{\"count\":";
    json += String(TrafficDB.getCount());

    const Aircraft* list = TrafficDB.getList();
    const uint32_t nowMs = millis();
    int flarmCount = 0;
    int adsbCount = 0;
    for (int i = 0; i < MAX_AIRCRAFT; ++i)
    {
        if (!list[i].valid) continue;
        if (list[i].source == TRAFFIC_SOURCE_FLARM_LORA) flarmCount++;
        else if (list[i].source == TRAFFIC_SOURCE_ADSB_DUMP1090) adsbCount++;
    }

    json += ",\"flarm_count\":";
    json += String(flarmCount);
    json += ",\"adsb_count\":";
    json += String(adsbCount);
    json += ",\"items\":[";

    bool first = true;
    const float rxLat = ThisAircraft.local_latitude;
    const float rxLon = ThisAircraft.local_longitude;

    for (int i = 0; i < MAX_AIRCRAFT; ++i)
    {
        if (!list[i].valid) continue;
        if (!first) json += ',';
        first = false;

        char icaoBuf[16];
        snprintf(icaoBuf, sizeof(icaoBuf), "%06lX", (unsigned long)list[i].icao);

        float distKm = 0.0f;
        if ((fabsf(list[i].lat) > 0.00001f || fabsf(list[i].lon) > 0.00001f) &&
            (fabsf(rxLat) > 0.00001f || fabsf(rxLon) > 0.00001f))
        {
            distKm = distanceKm(rxLat, rxLon, list[i].lat, list[i].lon);
        }

        json += '{';
        json += "\"icao\":\""; json += icaoBuf; json += "\",";
        json += "\"source\":\""; json += sourceName(list[i].source); json += "\",";
        json += "\"distance_km\":"; json += String(distKm, 2); json += ',';
        json += "\"lat\":"; json += String(list[i].lat, 5); json += ',';
        json += "\"lon\":"; json += String(list[i].lon, 5); json += ',';
        json += "\"altitude\":"; json += String(list[i].altitude); json += ',';
        json += "\"speed\":"; json += String(list[i].speed, 1); json += ',';
        json += "\"course\":"; json += String(list[i].course, 1); json += ',';
        json += "\"rssi\":"; json += String(list[i].rssi); json += ',';
        json += "\"snr\":"; json += String(list[i].snr, 1); json += ',';
        json += "\"age_ms\":"; json += String((uint32_t)(nowMs - list[i].lastUpdate));
        json += '}';
    }
    json += "]}";

    sendNoCache();
    server.send(200, "application/json; charset=utf-8", json);
}

static void handleRoot() { sendPage(rootPage()); }
static void handleSettings() { sendPage(settingsPage()); }
static void handleHardwareSettings() { sendPage(hardwarePage()); }
static void handleOutputs() { sendPage(outputsPage()); }
static void handleAbout() { sendPage(aboutPage()); }
static void handleFirmware() { sendPage(firmwarePage()); }
static void handleMonitor() { sendPage(monitorPage()); }

static void handleInput()
{
    if (settings)
    {
        if (server.hasArg("tracker_send")) settings->tracker_send = (uint8_t)clampInt(server.arg("tracker_send").toInt(), 0, 15);
        if (server.hasArg("mode")) settings->mode = (uint8_t)clampInt(server.arg("mode").toInt(), FLYRF_MODE_NORMAL, FLYRF_MODE_TXRX_TEST_MAX);
        if (server.hasArg("input_N_S")) settings->input_N_S = (uint8_t)clampInt(server.arg("input_N_S").toInt(), 0, 1);
        if (server.hasArg("local_latitude")) settings->local_latitude = server.arg("local_latitude").toFloat();
        if (server.hasArg("input_E_W")) settings->input_E_W = (uint8_t)clampInt(server.arg("input_E_W").toInt(), 0, 1);
        if (server.hasArg("local_longitude")) settings->local_longitude = server.arg("local_longitude").toFloat();
        applyRuntimeLocalCoordinates();
        EEPROM_store();
    }
    sendPage(settingsSavedPage());
}

static void handleDisplayModeLive()
{
    if (!settings || !server.hasArg("display_set"))
    {
        sendNoCache();
        server.send(400, "application/json; charset=utf-8", "{\"ok\":false,\"message\":\"display_set not specified\"}");
        return;
    }

    settings->display_set = (uint8_t)clampInt(server.arg("display_set").toInt(), INFO_DISTLAY_OFF, INFO_DISPLAY_LORA_RAW);
    EEPROM_store();

    String json = F("{\"ok\":true,\"display_set\":");
    json += String(settings->display_set);
    json += F(",\"name\":\"");
    json += displayModeName(settings->display_set);
    json += F("\"}");
    sendNoCache();
    server.send(200, "application/json; charset=utf-8", json);
}

static void handleSettingsLive()
{
    if (!settings)
    {
        sendNoCache();
        server.send(500, "application/json; charset=utf-8", "{\"ok\":false,\"message\":\"settings not ready\"}");
        return;
    }

    if (server.hasArg("tracker_send")) settings->tracker_send = (uint8_t)clampInt(server.arg("tracker_send").toInt(), 0, 15);
    if (server.hasArg("mode")) settings->mode = (uint8_t)clampInt(server.arg("mode").toInt(), FLYRF_MODE_NORMAL, FLYRF_MODE_TXRX_TEST_MAX);
    if (server.hasArg("input_N_S")) settings->input_N_S = (uint8_t)clampInt(server.arg("input_N_S").toInt(), 0, 1);
    if (server.hasArg("local_latitude")) settings->local_latitude = server.arg("local_latitude").toFloat();
    if (server.hasArg("input_E_W")) settings->input_E_W = (uint8_t)clampInt(server.arg("input_E_W").toInt(), 0, 1);
    if (server.hasArg("local_longitude")) settings->local_longitude = server.arg("local_longitude").toFloat();
    applyRuntimeLocalCoordinates();
    EEPROM_store();

    sendNoCache();
    server.send(200, "application/json; charset=utf-8", "{\"ok\":true}");
}

static void handleHardwareLive()
{
    if (!settings)
    {
        sendNoCache();
        server.send(500, "application/json; charset=utf-8", "{\"ok\":false,\"message\":\"settings not ready\"}");
        return;
    }

    bool resetDefaults = false;
    if (server.hasArg("default_settings") && server.arg("default_settings").toInt() == 1)
    {
        EEPROM_clear();
        resetDefaults = true;
    }
    else
    {
        if (server.hasArg("rssi_view")) settings->rssi_view = (uint8_t)clampInt(server.arg("rssi_view").toInt(), 0, 1);
        if (server.hasArg("display_enabled")) settings->display_enabled = (uint8_t)clampInt(server.arg("display_enabled").toInt(), 0, 1);
        if (server.hasArg("display_set")) settings->display_set = (uint8_t)clampInt(server.arg("display_set").toInt(), INFO_DISTLAY_OFF, INFO_DISPLAY_LORA_RAW);
        settings->tft_memory_view = 0;
        if (server.hasArg("serial_out")) settings->serial_out = (uint8_t)clampInt(server.arg("serial_out").toInt(), OUTPUT_MODE_OFF, OUTPUT_MODE_MAVLINK);
        if (server.hasArg("rs485_out")) settings->rs485_out = (uint8_t)clampInt(server.arg("rs485_out").toInt(), OUTPUT_MODE_OFF, OUTPUT_MODE_MAVLINK);
        if (server.hasArg("lan_state_view")) settings->lan_state_view = (uint8_t)clampInt(server.arg("lan_state_view").toInt(), 0, 1);
        if (server.hasArg("lan_enabled")) settings->lan_enabled = (uint8_t)clampInt(server.arg("lan_enabled").toInt(), 0, 1);
        if (server.hasArg("gps_state_view")) settings->gps_state_view = (uint8_t)clampInt(server.arg("gps_state_view").toInt(), 0, 1);
        else if (server.hasArg("view_gps_data")) settings->gps_state_view = (uint8_t)clampInt(server.arg("view_gps_data").toInt(), 0, 1);
        parseIpArg("local_ip", settings->g_localIP);
        parseIpArg("gateway_ip", settings->g_gatewayIP);
        parseIpArg("subnet_mask", settings->g_subnetMask);
        parseIpArg("dns_server", settings->g_dns_server);
        if (server.hasArg("udp_port")) settings->udp_port = (uint16_t)clampInt(server.arg("udp_port").toInt(), 1, 65535);
        if (settings->udp_port == 0) settings->udp_port = (uint16_t)NMEA_UDP_PORT;
        if (server.hasArg("rf_protocol"))
        {
            settings->rf_protocol = parseRfProtocolArg(server.arg("rf_protocol"));
            settings->lora_profile = parseLoRaProfileArg(server.arg("rf_protocol"));
        }
        settings->band = RF_BAND_RU;
        settings->lora_fixed_channel = 1;
        if (server.hasArg("lora_fixed_freq")) settings->lora_fixed_freq = (uint8_t)clampInt(server.arg("lora_fixed_freq").toInt(), 0, 9);
        if (server.hasArg("lora_custom_sf")) settings->lora_custom_sf = (uint8_t)clampInt(server.arg("lora_custom_sf").toInt(), 7, 12);
        if (server.hasArg("lora_custom_bw")) settings->lora_custom_bw = (uint8_t)clampInt(server.arg("lora_custom_bw").toInt(), 0, 2);
        if (server.hasArg("lora_custom_cr")) settings->lora_custom_cr = (uint8_t)clampInt(server.arg("lora_custom_cr").toInt(), 0, 3);
        settings->txpower = 20;
        if (server.hasArg("display_sos")) settings->display_sos = (uint8_t)clampInt(server.arg("display_sos").toInt(), 0, 1);
        if (server.hasArg("radar_range_mode")) settings->radar_range_mode = (uint8_t)clampInt(server.arg("radar_range_mode").toInt(), 0, 1);
        uint32_t parsedBlockAddr[3] = {0U, 0U, 0U};
        String normalizedBlockAddr;
        String normalizedBlockAddr2;
        String normalizedBlockAddr3;
        String blockAddrError;
        if (!readBlockAddrArg("block_addr", parsedBlockAddr[0], normalizedBlockAddr, blockAddrError) ||
            !readBlockAddrArg("block_addr2", parsedBlockAddr[1], normalizedBlockAddr2, blockAddrError) ||
            !readBlockAddrArg("block_addr3", parsedBlockAddr[2], normalizedBlockAddr3, blockAddrError))
        {
            sendNoCache();
            String json = F("{\"ok\":false,\"message\":\"");
            json += blockAddrError;
            json += F("\"}");
            server.send(400, "application/json; charset=utf-8", json);
            return;
        }
        for (size_t i = 0; i < 3; ++i)
        {
            settings->block_addr[i] = parsedBlockAddr[i];
        }
        if (server.hasArg("threshold_level")) settings->threshold_level = (int16_t)clampInt(server.arg("threshold_level").toInt(), 400, 1500);
        if (server.hasArg("adsb_receiver")) settings->adsb_receiver = (uint8_t)clampInt(server.arg("adsb_receiver").toInt(), ADSB_RECEIVER_EXTERNAL, ADSB_RECEIVER_INTERNAL);
        if (server.hasArg("alarm_attention")) settings->alarm_attention = (int16_t)clampAlarmAttention(server.arg("alarm_attention").toInt());
        if (server.hasArg("alarm_warning")) settings->alarm_warning = (int16_t)clampAlarmWarning(server.arg("alarm_warning").toInt());
        if (server.hasArg("alarm_danger")) settings->alarm_danger = (int16_t)clampAlarmDanger(server.arg("alarm_danger").toInt());
        if (server.hasArg("alarm_height")) settings->alarm_height = (int16_t)clampAlarmHeight(server.arg("alarm_height").toInt());
        if (settings->alarm_warning > settings->alarm_attention) settings->alarm_warning = settings->alarm_attention;
        if (settings->alarm_danger > settings->alarm_warning) settings->alarm_danger = settings->alarm_warning;
        EEPROM_store();
        RF_NotifySettingsChanged();
    }
    if (resetDefaults) RF_NotifySettingsChanged();
    ADSBReceiverESP32_applySettings();
    RP2040Bridge_sendGainNow();

    String json = F("{\"ok\":true,\"reset_defaults\":");
    json += resetDefaults ? F("true") : F("false");
    json += F("}");
    sendNoCache();
    server.send(200, "application/json; charset=utf-8", json);
    scheduleReboot(1000);
}

static void handleOutputsLive()
{
    if (!settings)
    {
        sendNoCache();
        server.send(500, "application/json; charset=utf-8", "{\"ok\":false,\"message\":\"settings not ready\"}");
        return;
    }

    if (server.hasArg("serial_out")) settings->serial_out = (uint8_t)clampInt(server.arg("serial_out").toInt(), OUTPUT_MODE_OFF, OUTPUT_MODE_MAVLINK);
    if (server.hasArg("rs485_out")) settings->rs485_out = (uint8_t)clampInt(server.arg("rs485_out").toInt(), OUTPUT_MODE_OFF, OUTPUT_MODE_MAVLINK);
    if (server.hasArg("nmea_out")) settings->nmea_out = (uint8_t)clampInt(server.arg("nmea_out").toInt(), NMEA_OUTPUT_OFF, NMEA_OUTPUT_BLUETOOTH);
    settings->bluetooth = (settings->nmea_out == NMEA_OUTPUT_BLUETOOTH) ? BLUETOOTH_LE : BLUETOOTH_OFF;
    if (server.hasArg("udp_port")) settings->udp_port = (uint16_t)clampInt(server.arg("udp_port").toInt(), 1, 65535);
    if (settings->udp_port == 0) settings->udp_port = (uint16_t)NMEA_UDP_PORT;
    EEPROM_store();

    sendNoCache();
    server.send(200, "application/json; charset=utf-8", "{\"ok\":true}");
}

static void handleHardwareInput()
{
    bool defaultsLoaded = false;
    if (settings)
    {
        if (server.hasArg("default_settings") && server.arg("default_settings").toInt() == 1)
        {
            EEPROM_clear();
            defaultsLoaded = true;
        }
        else
        {
            if (server.hasArg("rssi_view")) settings->rssi_view = (uint8_t)clampInt(server.arg("rssi_view").toInt(), 0, 1);
            if (server.hasArg("display_enabled")) settings->display_enabled = (uint8_t)clampInt(server.arg("display_enabled").toInt(), 0, 1);
            if (server.hasArg("display_set")) settings->display_set = (uint8_t)clampInt(server.arg("display_set").toInt(), INFO_DISTLAY_OFF, INFO_DISPLAY_LORA_RAW);
            settings->tft_memory_view = 0;
            if (server.hasArg("serial_out")) settings->serial_out = (uint8_t)clampInt(server.arg("serial_out").toInt(), OUTPUT_MODE_OFF, OUTPUT_MODE_MAVLINK);
        if (server.hasArg("rs485_out")) settings->rs485_out = (uint8_t)clampInt(server.arg("rs485_out").toInt(), OUTPUT_MODE_OFF, OUTPUT_MODE_MAVLINK);
            if (server.hasArg("lan_state_view")) settings->lan_state_view = (uint8_t)clampInt(server.arg("lan_state_view").toInt(), 0, 1);
            if (server.hasArg("lan_enabled")) settings->lan_enabled = (uint8_t)clampInt(server.arg("lan_enabled").toInt(), 0, 1);
            if (server.hasArg("gps_state_view")) settings->gps_state_view = (uint8_t)clampInt(server.arg("gps_state_view").toInt(), 0, 1);
            else if (server.hasArg("view_gps_data")) settings->gps_state_view = (uint8_t)clampInt(server.arg("view_gps_data").toInt(), 0, 1);
            parseIpArg("local_ip", settings->g_localIP);
            parseIpArg("gateway_ip", settings->g_gatewayIP);
            parseIpArg("subnet_mask", settings->g_subnetMask);
            parseIpArg("dns_server", settings->g_dns_server);
            if (server.hasArg("udp_port")) settings->udp_port = (uint16_t)clampInt(server.arg("udp_port").toInt(), 1, 65535);
            if (settings->udp_port == 0) settings->udp_port = (uint16_t)NMEA_UDP_PORT;
            if (server.hasArg("rf_protocol"))
        {
            settings->rf_protocol = parseRfProtocolArg(server.arg("rf_protocol"));
            settings->lora_profile = parseLoRaProfileArg(server.arg("rf_protocol"));
        }
        settings->band = RF_BAND_RU;
        settings->lora_fixed_channel = 1;
        if (server.hasArg("lora_fixed_freq")) settings->lora_fixed_freq = (uint8_t)clampInt(server.arg("lora_fixed_freq").toInt(), 0, 9);
        if (server.hasArg("lora_custom_sf")) settings->lora_custom_sf = (uint8_t)clampInt(server.arg("lora_custom_sf").toInt(), 7, 12);
        if (server.hasArg("lora_custom_bw")) settings->lora_custom_bw = (uint8_t)clampInt(server.arg("lora_custom_bw").toInt(), 0, 2);
        if (server.hasArg("lora_custom_cr")) settings->lora_custom_cr = (uint8_t)clampInt(server.arg("lora_custom_cr").toInt(), 0, 3);
        if (server.hasArg("display_sos")) settings->display_sos = (uint8_t)clampInt(server.arg("display_sos").toInt(), 0, 1);
            if (server.hasArg("radar_range_mode")) settings->radar_range_mode = (uint8_t)clampInt(server.arg("radar_range_mode").toInt(), 0, 1);
            uint32_t parsedBlockAddr[3] = {0U, 0U, 0U};
            String normalizedBlockAddr;
            String normalizedBlockAddr2;
            String normalizedBlockAddr3;
            String blockAddrError;
            if (!readBlockAddrArg("block_addr", parsedBlockAddr[0], normalizedBlockAddr, blockAddrError) ||
                !readBlockAddrArg("block_addr2", parsedBlockAddr[1], normalizedBlockAddr2, blockAddrError) ||
                !readBlockAddrArg("block_addr3", parsedBlockAddr[2], normalizedBlockAddr3, blockAddrError))
            {
                sendValidationErrorPage(F("&#1054;&#1096;&#1080;&#1073;&#1082;&#1072; &#1074;&#1074;&#1086;&#1076;&#1072;"), blockAddrError);
                return;
            }
            for (size_t i = 0; i < 3; ++i)
            {
                settings->block_addr[i] = parsedBlockAddr[i];
            }
            if (server.hasArg("threshold_level")) settings->threshold_level = (int16_t)clampInt(server.arg("threshold_level").toInt(), 400, 1500);
            if (server.hasArg("adsb_receiver")) settings->adsb_receiver = (uint8_t)clampInt(server.arg("adsb_receiver").toInt(), ADSB_RECEIVER_EXTERNAL, ADSB_RECEIVER_INTERNAL);
            if (server.hasArg("alarm_attention")) settings->alarm_attention = (int16_t)clampAlarmAttention(server.arg("alarm_attention").toInt());
            if (server.hasArg("alarm_warning")) settings->alarm_warning = (int16_t)clampAlarmWarning(server.arg("alarm_warning").toInt());
            if (server.hasArg("alarm_danger")) settings->alarm_danger = (int16_t)clampAlarmDanger(server.arg("alarm_danger").toInt());
            if (server.hasArg("alarm_height")) settings->alarm_height = (int16_t)clampAlarmHeight(server.arg("alarm_height").toInt());
            if (settings->alarm_warning > settings->alarm_attention) settings->alarm_warning = settings->alarm_attention;
            if (settings->alarm_danger > settings->alarm_warning) settings->alarm_danger = settings->alarm_warning;
            EEPROM_store();
        RF_NotifySettingsChanged();
        }
        if (defaultsLoaded) RF_NotifySettingsChanged();
        ADSBReceiverESP32_applySettings();
        RP2040Bridge_sendGainNow();
    }
    sendPage(hardwareSavedPage(defaultsLoaded));
    scheduleReboot(1000);
}

static void handleOutputsSave()
{
    if (settings)
    {
        if (server.hasArg("serial_out")) settings->serial_out = (uint8_t)clampInt(server.arg("serial_out").toInt(), OUTPUT_MODE_OFF, OUTPUT_MODE_MAVLINK);
        if (server.hasArg("rs485_out")) settings->rs485_out = (uint8_t)clampInt(server.arg("rs485_out").toInt(), OUTPUT_MODE_OFF, OUTPUT_MODE_MAVLINK);
        if (server.hasArg("nmea_out")) settings->nmea_out = (uint8_t)clampInt(server.arg("nmea_out").toInt(), NMEA_OUTPUT_OFF, NMEA_OUTPUT_BLUETOOTH);
        settings->bluetooth = (settings->nmea_out == NMEA_OUTPUT_BLUETOOTH) ? BLUETOOTH_LE : BLUETOOTH_OFF;
        if (server.hasArg("udp_port")) settings->udp_port = (uint16_t)clampInt(server.arg("udp_port").toInt(), 1, 65535);
    if (settings->udp_port == 0) settings->udp_port = (uint16_t)NMEA_UDP_PORT;
        EEPROM_store();
    }
    sendPage(outputsSavedPage());
}

static void handlePacketCounters()
{
    uint32_t txPackets = 0;
    uint32_t rxPackets = 0;
    RF_GetPacketCounters(txPackets, rxPackets);

    uint32_t lanTxPackets = 0;
    uint32_t lanRxPackets = 0;
    uint32_t lanUdpTxPackets = 0;
    uint32_t lanUdpRxPackets = 0;
    uint32_t lanTcpTxPackets = 0;
    uint32_t lanTcpRxPackets = 0;
    LAN_getPacketCounters(lanTxPackets, lanRxPackets, lanUdpTxPackets, lanUdpRxPackets, lanTcpTxPackets, lanTcpRxPackets);

    String json = F("{\"tx\":");
    json += String(txPackets);
    json += F(",\"rx\":");
    json += String(rxPackets);
    json += F(",\"lan_tx\":");
    json += String(lanTxPackets);
    json += F(",\"lan_rx\":");
    json += String(lanRxPackets);
    json += F(",\"lan_udp_tx\":");
    json += String(lanUdpTxPackets);
    json += F(",\"lan_udp_rx\":");
    json += String(lanUdpRxPackets);
    json += F(",\"lan_tcp_tx\":");
    json += String(lanTcpTxPackets);
    json += F(",\"lan_tcp_rx\":");
    json += String(lanTcpRxPackets);
    json += F(",\"lan_ip\":\"");
    json += LAN_localIPStr();
    json += F("\",\"lan_mac\":\"");
    json += LAN_macAddressStr();
    json += F("\",\"lan_link_text\":\"");
    json += LAN_linkStatusStr();
    json += F("\",\"lan_dhcp\":");
    json += LAN_dhcpLeaseActive() ? F("true") : F("false");
    json += F(",\"utc_text\":\"");
    json += jsonEscape(formatUtcNowText());
    json += F("\",\"satellites\":");
    json += String(GNSS_satellitesValid() ? GNSS_satellites() : 0);
    json += F(",\"coords_text\":\"");
    json += jsonEscape(formatGnssCoordinateText());
    json += F("\",\"gps_altitude_text\":\"");
    json += jsonEscape(formatGpsAltitudeText());
    json += F("\",\"altitude_text\":\"");
    json += jsonEscape(formatAltitudeText());
    json += F("\"}");
    sendNoCache();
    server.send(200, "application/json; charset=utf-8", json);
}

static void handleOtaStatus()
{
    String json = F("{\"in_progress\":");
    json += otaProtected() ? F("true") : F("false");
    json += F(",\"written\":");
    json += String(g_otaWritten);
    json += F(",\"total\":");
    json += String(g_otaTotal);
    json += F(",\"ok\":");
    json += g_updateOk ? F("true") : F("false");
    json += F(",\"message\":\"");
    String msg = g_updateMessage;
    msg.replace("\\", "\\\\");
    msg.replace("\"", "\\\"");
    msg.replace("\r", " ");
    msg.replace("\n", " ");
    json += msg;
    json += F("\"}");
    sendNoCache();
    server.send(200, "application/json; charset=utf-8", json);
}

static void handleUpdatePrepare()
{
    sendNoCache();
    g_otaExpectedSize = server.hasArg("size") ? (uint32_t)server.arg("size").toInt() : 0U;
    if (g_otaExpectedSize == 0U)
    {
        server.send(400, "text/plain; charset=utf-8", "FAIL: bad size");
        return;
    }

    const uint32_t maxSketchSpace = ((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000);
    if (g_otaExpectedSize > maxSketchSpace)
    {
        g_updateMessage = F("FAIL: firmware is larger than OTA partition");
        server.send(413, "text/plain; charset=utf-8", g_updateMessage);
        return;
    }

    SystemEnterOtaMode();
    WiFi_pauseUDP();
    g_updateOk = false;
    g_otaWriteFailed = false;
    g_otaInProgress = false;
    g_otaWritten = 0;
    g_otaTotal = g_otaExpectedSize;
    g_otaChunkOffset = 0;
    g_otaChunkReceived = 0;
    g_otaSkipChunk = false;
    g_updateMessage = F("Ready for full firmware upload");
    keepOtaProtected(60000UL);
    server.send(200, "text/plain; charset=utf-8", "OK");
}

static void handleUpdateBegin()
{
    sendNoCache();
    g_otaExpectedSize = server.hasArg("size") ? (uint32_t)server.arg("size").toInt() : 0U;
    if (g_otaExpectedSize == 0U)
    {
        server.send(400, "text/plain; charset=utf-8", "FAIL: bad size");
        return;
    }

    const uint32_t maxSketchSpace = ((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000);
    if (g_otaExpectedSize > maxSketchSpace)
    {
        g_updateMessage = F("FAIL: firmware is larger than OTA partition");
        server.send(413, "text/plain; charset=utf-8", g_updateMessage);
        return;
    }

    SystemEnterOtaMode();
    WiFi_pauseUDP();
    g_updateOk = false;
    g_otaWriteFailed = false;
    g_otaInProgress = true;
    g_otaWritten = 0;
    g_otaTotal = g_otaExpectedSize;
    g_otaChunkOffset = 0;
    g_otaChunkReceived = 0;
    g_otaSkipChunk = false;
    g_updateMessage = F("Receiving firmware chunks");
    keepOtaProtected(30000UL);

    if (!Update.begin(g_otaExpectedSize))
    {
        g_otaInProgress = false;
        g_otaWriteFailed = true;
        g_updateMessage = F("FAIL: Update.begin");
        Update.printError(Serial);
        server.send(500, "text/plain; charset=utf-8", g_updateMessage);
        return;
    }

    server.send(200, "text/plain; charset=utf-8", "OK");
}

static void handleUpdateChunkFinished()
{
    sendNoCache();
    if (!g_otaInProgress || g_otaWriteFailed || Update.hasError())
    {
        server.send(500, "text/plain; charset=utf-8", g_updateMessage.length() ? g_updateMessage : String("FAIL"));
        return;
    }
    server.send(200, "text/plain; charset=utf-8", "OK");
}

static void handleUpdateChunkUpload()
{
    HTTPUpload& upload = server.upload();
    keepOtaProtected(30000UL);

    if (upload.status == UPLOAD_FILE_START)
    {
        g_otaChunkOffset = server.hasArg("offset") ? (uint32_t)server.arg("offset").toInt() : g_otaWritten;
        g_otaChunkReceived = 0;
        g_otaSkipChunk = false;

        if (!g_otaInProgress || g_otaWriteFailed)
        {
            return;
        }

        if (g_otaChunkOffset < g_otaWritten)
        {
            g_otaSkipChunk = true;
            g_updateMessage = F("Duplicate chunk ignored");
            return;
        }

        if (g_otaChunkOffset != g_otaWritten)
        {
            g_otaWriteFailed = true;
            g_otaInProgress = false;
            g_updateMessage = F("FAIL: missing chunk");
            Update.abort();
            return;
        }
    }

    if (!g_otaInProgress || g_otaWriteFailed)
    {
        return;
    }

    if (upload.status == UPLOAD_FILE_WRITE)
    {
        if (g_otaSkipChunk)
        {
            g_otaChunkReceived += upload.currentSize;
            return;
        }

        if (g_otaExpectedSize != 0U && (g_otaWritten + upload.currentSize) > g_otaExpectedSize)
        {
            g_otaWriteFailed = true;
            g_otaInProgress = false;
            g_updateMessage = F("FAIL: chunk beyond firmware size");
            Update.abort();
            return;
        }

        const size_t written = Update.write(upload.buf, upload.currentSize);
        if (written != upload.currentSize)
        {
            g_otaWriteFailed = true;
            g_otaInProgress = false;
            g_updateMessage = F("FAIL: flash write");
            Update.printError(Serial);
            Update.abort();
            return;
        }

        g_otaWritten += upload.currentSize;
        g_otaChunkReceived += upload.currentSize;
        g_updateMessage = F("Receiving firmware chunks");
    }
    else if (upload.status == UPLOAD_FILE_ABORTED)
    {
        g_otaWriteFailed = true;
        g_otaInProgress = false;
        g_updateMessage = F("FAIL: chunk aborted");
        Update.abort();
    }
    yield();
}

static void handleUpdateEnd()
{
    sendNoCache();
    server.sendHeader("Connection", "close");
    keepOtaProtected(30000UL);

    if (!g_otaInProgress || g_otaWriteFailed || Update.hasError())
    {
        g_otaInProgress = false;
        Update.abort();
        server.send(500, "text/plain; charset=utf-8", g_updateMessage.length() ? g_updateMessage : String("FAIL"));
        return;
    }

    if (g_otaExpectedSize != 0U && g_otaWritten != g_otaExpectedSize)
    {
        g_otaInProgress = false;
        g_otaWriteFailed = true;
        g_updateMessage = F("FAIL: incomplete firmware");
        Update.abort();
        server.send(500, "text/plain; charset=utf-8", g_updateMessage);
        return;
    }

    g_otaInProgress = false;
    if (Update.end(true))
    {
        g_updateOk = true;
        g_otaWritten = g_otaTotal;
        g_updateMessage = F("Update complete. Rebooting...");
        server.send(200, "text/plain; charset=utf-8", "OK");
        scheduleReboot(10000);
    }
    else
    {
        g_updateOk = false;
        g_otaWriteFailed = true;
        g_updateMessage = F("FAIL: Update.end");
        Update.printError(Serial);
        server.send(500, "text/plain; charset=utf-8", g_updateMessage);
    }
}

static void handleUpdateFinished()
{
    sendNoCache();
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain; charset=utf-8", (Update.hasError() || !g_updateOk) ? "FAIL" : "OK");
    if (!Update.hasError() && g_updateOk)
    {
        scheduleReboot(10000);
    }
}

static void handleUpdateUpload()
{
    HTTPUpload& upload = server.upload();

    if (upload.status == UPLOAD_FILE_START)
    {
        Serial.printf("[OTA] START filename=%s\n", upload.filename.c_str());
        g_updateOk = false;
        g_otaWriteFailed = false;
        g_updateMessage = F("&#1048;&#1076;&#1105;&#1090; &#1087;&#1086;&#1076;&#1075;&#1086;&#1090;&#1086;&#1074;&#1082;&#1072; &#1086;&#1073;&#1085;&#1086;&#1074;&#1083;&#1077;&#1085;&#1080;&#1103;...");
        g_otaInProgress = true;
        g_otaWritten = 0;
        g_otaExpectedSize = server.hasArg("size") ? (uint32_t)server.arg("size").toInt() : 0U;
        g_otaTotal = (g_otaExpectedSize != 0U) ? g_otaExpectedSize : upload.totalSize;
        g_otaChunkOffset = 0;
        g_otaChunkReceived = 0;
        g_otaSkipChunk = false;
        keepOtaProtected(30000UL);
        SystemEnterOtaMode();
        WiFi_pauseUDP();

        uint32_t maxSketchSpace = ((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000);
        if (g_otaExpectedSize != 0U && g_otaExpectedSize > maxSketchSpace)
        {
            g_updateMessage = F("&#1054;&#1096;&#1080;&#1073;&#1082;&#1072;: &#1092;&#1072;&#1081;&#1083; &#1073;&#1086;&#1083;&#1100;&#1096;&#1077; OTA-&#1088;&#1072;&#1079;&#1076;&#1077;&#1083;&#1072;");
            g_updateOk = false;
            g_otaWriteFailed = true;
            g_otaInProgress = false;
            WiFi_resumeUDP();
            return;
        }

        const uint32_t updateSize = (g_otaExpectedSize != 0U) ? g_otaExpectedSize : maxSketchSpace;
        if (!Update.begin(updateSize))
        {
            g_updateMessage = F("&#1053;&#1077; &#1091;&#1076;&#1072;&#1083;&#1086;&#1089;&#1100; &#1085;&#1072;&#1095;&#1072;&#1090;&#1100; &#1086;&#1073;&#1085;&#1086;&#1074;&#1083;&#1077;&#1085;&#1080;&#1077;");
            g_otaInProgress = false;
            g_otaWriteFailed = true;
            WiFi_resumeUDP();
            Update.printError(Serial);
        }
    }
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        keepOtaProtected(20000UL);
        if (!g_otaInProgress || g_otaWriteFailed)
        {
            return;
        }

        if (g_otaExpectedSize == 0U && upload.totalSize > g_otaTotal) g_otaTotal = upload.totalSize;
        if (g_otaExpectedSize != 0U && (g_otaWritten + upload.currentSize) > g_otaExpectedSize)
        {
            g_updateMessage = F("&#1054;&#1096;&#1080;&#1073;&#1082;&#1072;: &#1076;&#1072;&#1085;&#1085;&#1099;&#1093; &#1073;&#1086;&#1083;&#1100;&#1096;&#1077; &#1088;&#1072;&#1079;&#1084;&#1077;&#1088;&#1072; &#1092;&#1072;&#1081;&#1083;&#1072;");
            g_updateOk = false;
            g_otaWriteFailed = true;
            g_otaInProgress = false;
            Update.abort();
            WiFi_resumeUDP();
            return;
        }

        const size_t written = Update.write(upload.buf, upload.currentSize);
        if (written != upload.currentSize)
        {
            g_updateMessage = F("&#1054;&#1096;&#1080;&#1073;&#1082;&#1072; &#1079;&#1072;&#1087;&#1080;&#1089;&#1080; &#1087;&#1088;&#1086;&#1096;&#1080;&#1074;&#1082;&#1080;");
            g_updateOk = false;
            g_otaWriteFailed = true;
            g_otaInProgress = false;
            Update.abort();
            WiFi_resumeUDP();
            Update.printError(Serial);
        }
        else
        {
            g_otaWritten += upload.currentSize;
            g_updateMessage = F("&#1055;&#1077;&#1088;&#1077;&#1076;&#1072;&#1095;&#1072; &#1092;&#1072;&#1081;&#1083;&#1072; &#1085;&#1072; &#1091;&#1089;&#1090;&#1088;&#1086;&#1081;&#1089;&#1090;&#1074;&#1086;...");
            if ((g_otaWritten % 65536U) < (uint32_t)upload.currentSize)
                Serial.printf("[OTA] WRITTEN=%u/%u\n", (unsigned)g_otaWritten, (unsigned)g_otaTotal);
        }
    }
    else if (upload.status == UPLOAD_FILE_END)
    {
        keepOtaProtected(30000UL);
        if (!g_otaInProgress || g_otaWriteFailed || Update.hasError())
        {
            g_updateOk = false;
            g_otaInProgress = false;
            if (!Update.hasError())
            {
                Update.abort();
            }
            WiFi_resumeUDP();
            return;
        }

        g_otaInProgress = false;
        if (g_otaExpectedSize == 0U && upload.totalSize > g_otaTotal) g_otaTotal = upload.totalSize;
        if (g_otaTotal == 0U) g_otaTotal = g_otaWritten;
        Serial.printf("[OTA] END written=%u total=%u\n", (unsigned)g_otaWritten, (unsigned)g_otaTotal);
        if (g_otaExpectedSize != 0U && g_otaWritten != g_otaExpectedSize)
        {
            g_updateOk = false;
            g_updateMessage = F("&#1054;&#1096;&#1080;&#1073;&#1082;&#1072;: &#1092;&#1072;&#1081;&#1083; &#1087;&#1088;&#1080;&#1085;&#1103;&#1090; &#1085;&#1077; &#1087;&#1086;&#1083;&#1085;&#1086;&#1089;&#1090;&#1100;&#1102;");
            Update.abort();
            WiFi_resumeUDP();
        }
        else if (Update.end(true))
        {
            g_updateOk = true;
            g_otaWritten = g_otaTotal;
            g_updateMessage = F("&#1054;&#1073;&#1085;&#1086;&#1074;&#1083;&#1077;&#1085;&#1080;&#1077; &#1079;&#1072;&#1074;&#1077;&#1088;&#1096;&#1077;&#1085;&#1086;. &#1055;&#1077;&#1088;&#1077;&#1079;&#1072;&#1075;&#1088;&#1091;&#1079;&#1082;&#1072;...");
            Serial.printf("[OTA] SUCCESS size=%u\n", (unsigned)upload.totalSize);
        }
        else
        {
            g_updateOk = false;
            g_updateMessage = F("&#1054;&#1096;&#1080;&#1073;&#1082;&#1072; &#1079;&#1072;&#1074;&#1077;&#1088;&#1096;&#1077;&#1085;&#1080;&#1103; &#1086;&#1073;&#1085;&#1086;&#1074;&#1083;&#1077;&#1085;&#1080;&#1103;");
            WiFi_resumeUDP();
            Update.printError(Serial);
        }
    }
    else if (upload.status == UPLOAD_FILE_ABORTED)
    {
        g_otaInProgress = false;
        keepOtaProtected(15000UL);
        if (!g_otaWriteFailed && g_otaExpectedSize != 0U && g_otaWritten >= g_otaExpectedSize && !Update.hasError())
        {
            g_otaTotal = g_otaExpectedSize;
            if (Update.end(true))
            {
                g_updateOk = true;
                g_otaWritten = g_otaTotal;
                g_updateMessage = F("&#1054;&#1073;&#1085;&#1086;&#1074;&#1083;&#1077;&#1085;&#1080;&#1077; &#1079;&#1072;&#1074;&#1077;&#1088;&#1096;&#1077;&#1085;&#1086;. &#1055;&#1077;&#1088;&#1077;&#1079;&#1072;&#1075;&#1088;&#1091;&#1079;&#1082;&#1072;...");
                Serial.printf("[OTA] ABORT after full file, finalized size=%u\n", (unsigned)g_otaTotal);
                scheduleReboot(10000);
            }
            else
            {
                g_updateOk = false;
                g_updateMessage = F("&#1054;&#1096;&#1080;&#1073;&#1082;&#1072; &#1079;&#1072;&#1074;&#1077;&#1088;&#1096;&#1077;&#1085;&#1080;&#1103; &#1086;&#1073;&#1085;&#1086;&#1074;&#1083;&#1077;&#1085;&#1080;&#1103;");
                WiFi_resumeUDP();
                Update.printError(Serial);
            }
        }
        else
        {
            g_updateOk = false;
            g_updateMessage = F("&#1047;&#1072;&#1075;&#1088;&#1091;&#1079;&#1082;&#1072; &#1086;&#1090;&#1084;&#1077;&#1085;&#1077;&#1085;&#1072;");
            Serial.println("[OTA] ABORT");
            Update.abort();
            WiFi_resumeUDP();
        }
    }
    yield();
}

bool Web_otaInProgress(void)
{
    return otaProtected();
}

static void handleNotFound()
{
    server.send(404, "text/plain; charset=utf-8", "File Not Found");
}

void Web_setup()
{
    if (g_handlersRegistered)
    {
        if (WiFi_ready() && !g_webStarted)
        {
            server.begin();
            g_webStarted = true;
        }
        return;
    }
    server.on("/", handleRoot);
    server.on("/settings", handleSettings);
    server.on("/input", handleInput);
    server.on("/hardware_settings", handleHardwareSettings);
    server.on("/hardware_input", handleHardwareInput);
    server.on("/outputs", handleOutputs);
    server.on("/outputs_save", handleOutputsSave);
    server.on("/monitor", handleMonitor);
    server.on("/aircraft.json", handleAircraftJson);
    server.on("/packet_counters", handlePacketCounters);
    server.on("/about", handleAbout);
    server.on("/firmware", handleFirmware);
    server.on("/update_prepare", HTTP_POST, handleUpdatePrepare);
    server.on("/update_begin", HTTP_POST, handleUpdateBegin);
    server.on("/update_chunk", HTTP_POST, handleUpdateChunkFinished, handleUpdateChunkUpload);
    server.on("/update_end", HTTP_POST, handleUpdateEnd);
    server.on("/update", HTTP_POST, handleUpdateFinished, handleUpdateUpload);
    server.on("/ota_status", handleOtaStatus);
    server.onNotFound(handleNotFound);
    g_handlersRegistered = true;
    if (WiFi_ready())
    {
        server.begin();
        g_webStarted = true;
    }
}

void Web_loop()
{
    if (!WiFi_ready())
    {
        return;
    }
    if (!g_webStarted)
    {
        Web_setup();
    }
    server.handleClient();
    if (otaProtected())
    {
        for (uint8_t i = 0; i < 4U; ++i)
        {
            server.handleClient();
            yield();
        }
    }
    if (g_rebootPending && (int32_t)(millis() - g_rebootAtMs) >= 0)
    {
        g_rebootPending = false;
        ESP.restart();
    }
}

void Web_fini()
{
    if (g_webStarted)
    {
        server.stop();
        g_webStarted = false;
    }
}
