/*
  Модуль NMEA.cpp
  Назначение:
  - Формирование и отправка текстовых выходных сообщений о полетных данных.

  Основные задачи модуля:
  - Генерировать NMEA-строки нашего самолета и сторонних целей.
  - Формировать контейнерный текстовый вывод для Serial/RS485/UDP/Bluetooth.
  - Соблюдать выбранные в WEB-интерфейсе режимы вывода.
  - Сопровождать вывод диагностикой и объявлением активного режима Serial.
*/

#include "NMEA.h"
#include "TrafficDB.h"
#include "DeviceInfo.h"
#include "EEPROMRF.h"
#include "RS485Display.h"
#include "Bluetooth.h"
#include "GNSS.h"
#include "WiFiRF.h"
#include <TimeLib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#ifndef DEG_TO_RAD
#define DEG_TO_RAD 0.017453292519943295769236907684886f
#endif

static bool g_rs485Ready = false;
static NMEADiag g_diag = {};
static uint8_t g_lastAnnouncedSerialMode = 0xFFU;
static uint32_t g_lastPgrmzMs = 0;

static const char* serialModeHeaderName(uint8_t mode)
{
    switch (mode)
    {
        case OUTPUT_MODE_CONTAINER: return "Container";
        case OUTPUT_MODE_NMEA: return "NMEA";
        case OUTPUT_MODE_RP2040: return "RP2040 RX";
        case OUTPUT_MODE_FLARM: return "FLARM RX";
        case OUTPUT_MODE_LORA_RAW: return "LoRa RAW RX";
        default: return "Off";
    }
}

void NMEA_announceSerialModeIfNeeded()
{
    const uint8_t mode = (settings != nullptr) ? settings->serial_out : OUTPUT_MODE_OFF;
    if (mode == g_lastAnnouncedSerialMode)
    {
        return;
    }

    g_lastAnnouncedSerialMode = mode;
    Serial.print("=== SERIAL MODE: ");
    Serial.print(serialModeHeaderName(mode));
    Serial.print(" ===\r\n");
    vTaskDelay(pdMS_TO_TICKS(4));
}

static uint8_t nmeaChecksum(const char* sentenceWithoutChecksum)
{
    uint8_t cs = 0;
    if (sentenceWithoutChecksum == nullptr) return 0;
    while (*sentenceWithoutChecksum)
    {
        cs ^= (uint8_t)*sentenceWithoutChecksum++;
    }
    return cs;
}

static size_t wrapNMEALine(const char* body, char* out, size_t outSize)
{
    if (body == nullptr || out == nullptr || outSize < 8)
    {
        return 0;
    }

    const uint8_t cs = nmeaChecksum(body);
    const int written = snprintf(out, outSize, "$%s*%02X\r\n", body, cs);
    if (written <= 0 || (size_t)written >= outSize)
    {
        return 0;
    }
    return (size_t)written;
}

static void rs485Write(const char* data, size_t len)
{
    if (!g_rs485Ready || data == nullptr || len == 0)
    {
        return;
    }

#if SOC_GPIO_PIN_RS485_DE >= 0
    digitalWrite(SOC_GPIO_PIN_RS485_DE, HIGH);
    delayMicroseconds(100);
#endif
    RS485_SERIAL.write((const uint8_t*)data, len);
    RS485_SERIAL.flush();
#if SOC_GPIO_PIN_RS485_DE >= 0
    delayMicroseconds(100);
    digitalWrite(SOC_GPIO_PIN_RS485_DE, LOW);
#endif
}

static bool sendNmeaToConfiguredOutput(const char* line, size_t len)
{
    if (settings == nullptr || line == nullptr || len == 0)
    {
        return false;
    }

    g_diag.nmeaOutput = settings->nmea_out;
    g_diag.udpPort = (settings->udp_port != 0U) ? settings->udp_port : WiFi_defaultNmeaUdpPort();
    g_diag.bluetoothMode = settings->bluetooth;

    bool sent = false;
    switch (settings->nmea_out)
    {
        case NMEA_OUTPUT_SERIAL:
            Serial.write((const uint8_t*)line, len);
            ++g_diag.sentencesSerial;
            sent = true;
            break;
        case NMEA_OUTPUT_UDP:
            if (WiFi_transmitUDP(g_diag.udpPort, (const uint8_t*)line, len))
            {
                ++g_diag.sentencesUDP;
                sent = true;
            }
            break;
        case NMEA_OUTPUT_BLUETOOTH:
            if (settings->bluetooth == BLUETOOTH_LE)
            {
                const size_t written = Bluetooth_write((const uint8_t*)line, len);
                if (written > 0)
                {
                    ++g_diag.sentencesBluetooth;
                    sent = true;
                }
            }
            break;
        case NMEA_OUTPUT_OFF:
        default:
            break;
    }

    if (settings->serial_out == OUTPUT_MODE_NMEA && settings->nmea_out != NMEA_OUTPUT_SERIAL)
    {
        Serial.write((const uint8_t*)line, len);
        ++g_diag.sentencesSerial;
        sent = true;
    }

    if (settings->rs485_out == OUTPUT_MODE_NMEA)
    {
        rs485Write(line, len);
        ++g_diag.sentencesRS485;
        sent = true;
    }

    return sent;
}

static void sendContainerToConfiguredOutputs(const char* line, size_t len)
{
    if (settings == nullptr || line == nullptr || len == 0)
    {
        return;
    }

    g_diag.serialMode = settings->serial_out;
    g_diag.rs485Mode = settings->rs485_out;

    // Для Serial в режиме OUTPUT_MODE_CONTAINER используется штатный дамп
    // ThisAircraft / Container[i] ниже в NMEA_loop().
    // Здесь оставляем вывод контейнера только для RS485, чтобы при выборе
    // "Вывод данных в Serial = Container" в Serial не появлялся второй,
    // лишний формат строк вида ICAO=... .
    if (settings->rs485_out != OUTPUT_MODE_RS485_DISPLAY && settings->rs485_out == OUTPUT_MODE_CONTAINER)
    {
        rs485Write(line, len);
        ++g_diag.sentencesRS485;
    }
}

static bool aircraftHasCoordinates(const Aircraft& ac)
{
    return fabsf(ac.lat) > 0.00001f || fabsf(ac.lon) > 0.00001f;
}

static void sanitizeCallsign(const char* src, char* dst, size_t dstSize)
{
    if (dst == nullptr || dstSize == 0) return;
    memset(dst, 0, dstSize);
    if (src == nullptr) return;

    size_t outIdx = 0;
    for (size_t i = 0; src[i] != '\0' && outIdx < (dstSize - 1); ++i)
    {
        const char c = src[i];
        if (c == ' ' || c == ',' || c == '*') break;
        dst[outIdx++] = c;
    }
    dst[outIdx] = 0;
}

static void makeFallbackCallsign(const Aircraft& ac, char* dst, size_t dstSize)
{
    if (dst == nullptr || dstSize == 0) return;
    snprintf(dst, dstSize, "T_%06lX", (unsigned long)(ac.addr & 0xFFFFFFUL));
}

static bool buildDateTimeFields(char* utcTime, size_t utcTimeSize, char* utcDate, size_t utcDateSize)
{
    if (!GNSS_timeValid() || utcTime == nullptr || utcDate == nullptr || utcTimeSize < 7 || utcDateSize < 7)
    {
        return false;
    }

    const time_t t = now();
    snprintf(utcTime, utcTimeSize, "%02d%02d%02d", hour(t), minute(t), second(t));
    snprintf(utcDate, utcDateSize, "%02d%02d%02d", day(t), month(t), year(t) % 100);
    return true;
}

static void sendOwnshipRMC()
{
    if (settings == nullptr) return;
    if (!GNSS_coordinatesValid() || !GNSS_timeValid()) return;

    const float lat = GNSS_latitude();
    const float lon = GNSS_longitude();
    const char latHem = (lat >= 0.0f) ? 'N' : 'S';
    const char lonHem = (lon >= 0.0f) ? 'E' : 'W';
    const float absLat = fabsf(lat);
    const float absLon = fabsf(lon);
    const int latDeg = (int)absLat;
    const int lonDeg = (int)absLon;
    const float latMin = (absLat - (float)latDeg) * 60.0f;
    const float lonMin = (absLon - (float)lonDeg) * 60.0f;

    char utcTime[16] = {};
    char utcDate[16] = {};
    if (!buildDateTimeFields(utcTime, sizeof(utcTime), utcDate, sizeof(utcDate))) return;

    char body[160];
    char sentence[180];
    snprintf(body, sizeof(body),
             "GPRMC,%s,A,%02d%07.4f,%c,%03d%07.4f,%c,%.1f,%.1f,%s,,,A",
             utcTime,
             latDeg, latMin, latHem,
             lonDeg, lonMin, lonHem,
             ThisAircraft.speed,
             ThisAircraft.course,
             utcDate);
    const size_t len = wrapNMEALine(body, sentence, sizeof(sentence));
    sendNmeaToConfiguredOutput(sentence, len);
}

static void sendOwnshipGGA()
{
    if (settings == nullptr) return;
    if (!GNSS_coordinatesValid() || !GNSS_timeValid()) return;

    const float lat = GNSS_latitude();
    const float lon = GNSS_longitude();
    const char latHem = (lat >= 0.0f) ? 'N' : 'S';
    const char lonHem = (lon >= 0.0f) ? 'E' : 'W';
    const float absLat = fabsf(lat);
    const float absLon = fabsf(lon);
    const int latDeg = (int)absLat;
    const int lonDeg = (int)absLon;
    const float latMin = (absLat - (float)latDeg) * 60.0f;
    const float lonMin = (absLon - (float)lonDeg) * 60.0f;
    const uint8_t sats = GNSS_satellitesValid() ? GNSS_satellites() : 0U;

    char utcTime[16] = {};
    char utcDate[16] = {};
    if (!buildDateTimeFields(utcTime, sizeof(utcTime), utcDate, sizeof(utcDate))) return;
    (void)utcDate;

    char body[160];
    char sentence[180];
    snprintf(body, sizeof(body),
             "GPGGA,%s,%02d%07.4f,%c,%03d%07.4f,%c,1,%02u,1.0,%.1f,M,0.0,M,,",
             utcTime,
             latDeg, latMin, latHem,
             lonDeg, lonMin, lonHem,
             (unsigned)sats,
             GNSS_altitudeValid() ? GNSS_altitudeMeters() : ThisAircraft.altitude);
    const size_t len = wrapNMEALine(body, sentence, sizeof(sentence));
    sendNmeaToConfiguredOutput(sentence, len);
}

static void sendOwnshipPGRMZ()
{
    if (settings == nullptr) return;
    if ((uint32_t)(millis() - g_lastPgrmzMs) < 1000UL) return;

    const int altitudeFt = constrain((int)lroundf(ThisAircraft.pressure_altitude * 3.28084f), -1000, 60000);
    char body[64];
    char sentence[80];
    snprintf(body, sizeof(body), "PGRMZ,%d,f,%c", altitudeFt, GNSS_coordinatesValid() ? '3' : '1');
    const size_t len = wrapNMEALine(body, sentence, sizeof(sentence));
    if (len > 0)
    {
        sendNmeaToConfiguredOutput(sentence, len);
        g_lastPgrmzMs = millis();
    }
}

static int exportSignalSource(uint8_t source)
{
    switch (source)
    {
        case TRAFFIC_SOURCE_FLARM_LORA: return 1;
        case TRAFFIC_SOURCE_ADSB_DUMP1090: return 2;
        default: return 0;
    }
}

static void sendOwnshipFLYRF()
{
    if (settings == nullptr) return;
    if (ThisAircraft.addr == 0U) return;

    const bool hasCoords =
        (fabsf(ThisAircraft.latitude) > 0.00001f || fabsf(ThisAircraft.longitude) > 0.00001f ||
         fabsf(ThisAircraft.local_latitude) > 0.00001f || fabsf(ThisAircraft.local_longitude) > 0.00001f ||
         fabsf(ThisAircraft.old_latitude) > 0.00001f || fabsf(ThisAircraft.old_longitude) > 0.00001f);
    if (!hasCoords) return;

    float lat = ThisAircraft.latitude;
    float lon = ThisAircraft.longitude;
    if (fabsf(lat) <= 0.00001f && fabsf(lon) <= 0.00001f)
    {
        lat = ThisAircraft.local_latitude;
        lon = ThisAircraft.local_longitude;
    }
    if (fabsf(lat) <= 0.00001f && fabsf(lon) <= 0.00001f)
    {
        lat = ThisAircraft.old_latitude;
        lon = ThisAircraft.old_longitude;
    }

    char callsign[16] = {};
    sanitizeCallsign(ThisAircraft.callsign, callsign, sizeof(callsign));
    if (callsign[0] == '\0')
    {
        snprintf(callsign, sizeof(callsign), "SELF%06lX", (unsigned long)(ThisAircraft.addr & 0xFFFFFFUL));
    }

    const bool hasUtc = GNSS_timeValid();
    const time_t t = hasUtc ? now() : 0;
    const int hourMsg = hasUtc ? hour(t) : 0;
    const int minMsg = hasUtc ? minute(t) : 0;

    char body[256];
    char sentence[280];
    snprintf(body, sizeof(body),
             "FLYRF,%06lX,%d,%s,%d,%d,%d,%d,%d,%.6f,%.6f,%d,%d,%d,%d",
             (unsigned long)(ThisAircraft.addr & 0xFFFFFFUL),
             ThisAircraft.squawk,
             callsign,
             (int)lroundf(ThisAircraft.altitude),
             (int)lroundf(ThisAircraft.pressure_altitude),
             (int)lroundf(ThisAircraft.speed),
             (int)lroundf(ThisAircraft.course),
             ThisAircraft.vert_rate,
             (double)lat,
             (double)lon,
             (int)ThisAircraft.aircraft_type,
             1,
             hourMsg,
             minMsg);

    const size_t len = wrapNMEALine(body, sentence, sizeof(sentence));
    sendNmeaToConfiguredOutput(sentence, len);
}

static void sendAircraftPFLAA(const Aircraft& ac, int& totalObjects, float& hpDistance, int& hpRelNorth, int& hpRelEast, int& hpAltDiff, uint32_t& hpAddr)
{
    if (settings == nullptr) return;
    if (!ac.valid || ac.addr == 0U) return;
    if (!aircraftHasCoordinates(ac)) return;
    if (ThisAircraft.local_latitude == 0.0f && ThisAircraft.local_longitude == 0.0f) return;

    const int relNorth = (int)lroundf(ac.distance * cosf(ac.bearing * DEG_TO_RAD));
    const int relEast = (int)lroundf(ac.distance * sinf(ac.bearing * DEG_TO_RAD));
    const int relVertical = (int)lroundf(ac.altitude - ThisAircraft.altitude);
    const int alarmLevel = (ac.alarm_level >= 0) ? ac.alarm_level : 0;
    char callsign[16] = {};
    sanitizeCallsign(ac.callsign, callsign, sizeof(callsign));
    if (callsign[0] == '\0')
    {
        makeFallbackCallsign(ac, callsign, sizeof(callsign));
    }

    char climbRate[12] = {};
    snprintf(climbRate, sizeof(climbRate), "%.1f", ac.vert_rate / 196.8504f);

    char body[220];
    char sentence[244];
    snprintf(body, sizeof(body),
             "PFLAA,%d,%d,%d,%d,1,%06lX!%s,%d,,%d,%s,1",
             alarmLevel,
             relNorth,
             relEast,
             relVertical,
             (unsigned long)(ac.addr & 0xFFFFFFUL),
             callsign,
             (int)lroundf(ac.course),
             (int)lroundf(ac.speed * 0.514444f),
             climbRate);

    const size_t len = wrapNMEALine(body, sentence, sizeof(sentence));
    if (len > 0)
    {
        sendNmeaToConfiguredOutput(sentence, len);
        ++totalObjects;
        if (ac.distance < hpDistance)
        {
            hpDistance = ac.distance;
            hpRelNorth = relNorth;
            hpRelEast = relEast;
            hpAltDiff = relVertical;
            hpAddr = ac.addr & 0xFFFFFFUL;
        }
    }
}

static void sendPFLAU(int totalObjects, float hpDistance, int hpRelNorth, int hpRelEast, int hpAltDiff, uint32_t hpAddr)
{
    if (settings == nullptr) return;

    char body[160];
    char sentence[180];
    int relBearing = 0;
    if (hpDistance < 1.0e9f)
    {
        relBearing = (int)lroundf(atan2f((float)hpRelEast, (float)hpRelNorth) * 57.2957795f - ThisAircraft.course);
        while (relBearing < -180) relBearing += 360;
        while (relBearing > 180) relBearing -= 360;
        snprintf(body, sizeof(body),
                 "PFLAU,%d,1,%d,1,0,%d,2,%d,%d,%06lX",
                 totalObjects,
                 GNSS_coordinatesValid() ? 2 : 0,
                 relBearing,
                 hpAltDiff,
                 (int)lroundf(hpDistance),
                 (unsigned long)hpAddr);
    }
    else
    {
        snprintf(body, sizeof(body),
                 "PFLAU,0,1,%d,1,0,,0,,,",
                 GNSS_coordinatesValid() ? 2 : 0);
    }
    const size_t len = wrapNMEALine(body, sentence, sizeof(sentence));
    sendNmeaToConfiguredOutput(sentence, len);
}

static void sendAircraftContainer(const Aircraft& ac)
{
    char line[180];
    const int written = snprintf(line, sizeof(line),
                                 "ICAO=%06lX,SRC=%u,LAT=%.6f,LON=%.6f,ALT=%d,SPD=%.1f,CRS=%.1f,RSSI=%d,SNR=%.1f,AGE=%lu\r\n",
                                 (unsigned long)ac.icao,
                                 (unsigned)ac.source,
                                 ac.lat,
                                 ac.lon,
                                 (int)lroundf(ac.altitude),
                                 ac.speed,
                                 ac.course,
                                 ac.rssi,
                                 ac.snr,
                                 (unsigned long)(millis() - ac.lastUpdate));
    if (written > 0)
    {
        sendContainerToConfiguredOutputs(line, (size_t)written);
    }
}

static void makeCallsignString(const char* src, char* dst, size_t dstSize)
{
    if (dst == nullptr || dstSize == 0)
    {
        return;
    }

    memset(dst, 0, dstSize);
    if (src == nullptr)
    {
        return;
    }

    const size_t copyLen = dstSize > 0 ? min((size_t)8, dstSize - 1) : 0;
    memcpy(dst, src, copyLen);
    dst[copyLen] = 0;
}

static void printLocalAircraftToSerial(const LocalAircraftState& ac)
{
    char callsign[9] = {};
    makeCallsignString(ac.callsign, callsign, sizeof(callsign));
    Serial.printf("%06X:%d:%8s:%.0f:%.0f:%.0f:%.0f:%d:%.6f:%.6f\r\n",
                  (unsigned)(ac.addr & 0xFFFFFFUL),
                  ac.squawk,
                  callsign,
                  (double)ac.altitude,
                  (double)ac.altitude,
                  (double)ac.speed,
                  (double)ac.course,
                  ac.vert_rate,
                  (double)ac.latitude,
                  (double)ac.longitude);
    vTaskDelay(pdMS_TO_TICKS(4));
}

static void printContainerAircraftToSerial(const ufo_t* ac)
{
    if (ac == nullptr)
    {
        return;
    }

    char callsign[9] = {};
    makeCallsignString(ac->callsign, callsign, sizeof(callsign));
    Serial.printf("%06X:%d:%8s:%.0f:%.0f:%.0f:%.0f:%d:%.6f:%.6f:%d:%d:%d\r\n",
                  (unsigned)(ac->addr & 0xFFFFFFUL),
                  ac->squawk,
                  callsign,
                  (double)ac->altitude,
                  (double)ac->altitude,
                  (double)ac->speed,
                  (double)ac->course,
                  ac->vert_rate,
                  (double)ac->latitude,
                  (double)ac->longitude,
                  (int)ac->rssi_LoRa,
                  (int)ac->rssi_rp2040,
                  (int)ac->signal_source);
    vTaskDelay(pdMS_TO_TICKS(4));
}

static void printContainerDump(const ufo_t* arr, int n)
{
    if (arr == nullptr || n <= 0)
    {
        return;
    }
    for (int i = 0; i < n; ++i)
    {
        Serial.print("Container[");
        Serial.print(i);
        Serial.print("]:");
        printContainerAircraftToSerial(&arr[i]);
    }
    vTaskDelay(pdMS_TO_TICKS(4));
}

void NMEA_setup()
{
    memset(&g_diag, 0, sizeof(g_diag));
    g_lastAnnouncedSerialMode = 0xFFU;
    g_lastPgrmzMs = 0;

#if SOC_GPIO_PIN_RS485_DE >= 0
    pinMode(SOC_GPIO_PIN_RS485_DE, OUTPUT);
    digitalWrite(SOC_GPIO_PIN_RS485_DE, LOW);
#endif

#if SOC_GPIO_PIN_RS485_RX >= 0 && SOC_GPIO_PIN_RS485_TX >= 0
    if (settings == nullptr || settings->rs485_out != OUTPUT_MODE_RS485_DISPLAY)
    {
        RS485_SERIAL.setRxBufferSize(1024);
        RS485_SERIAL.setTxBufferSize(1024);
        RS485_SERIAL.begin(115200, SERIAL_8N1, SOC_GPIO_PIN_RS485_RX, SOC_GPIO_PIN_RS485_TX);
        g_rs485Ready = true;
    }
    else
    {
        g_rs485Ready = false;
    }
#else
    g_rs485Ready = false;
#endif

    g_diag.rs485Ready = g_rs485Ready;
}

void NMEA_loop()
{
    static uint32_t lastSendMs = 0;
    const uint32_t nowMs = millis();
    if ((uint32_t)(nowMs - lastSendMs) < NMEA_UPDATE_INTERVAL_MS)
    {
        return;
    }
    lastSendMs = nowMs;

    const Aircraft* list = TrafficDB.getList();

    NMEA_announceSerialModeIfNeeded();

    if (settings != nullptr && settings->serial_out == OUTPUT_MODE_CONTAINER)
    {
        Serial.print("ThisAircraft:");
        printLocalAircraftToSerial(ThisAircraft);
        Serial.println("--------------------------------------------------------------------");
        printContainerDump(list, MAX_AIRCRAFT);
        Serial.println("===========================================================================");
    }

    sendOwnshipRMC();
    sendOwnshipGGA();
    sendOwnshipPGRMZ();

    int totalObjects = 0;
    float hpDistance = 1.0e9f;
    int hpRelNorth = 0;
    int hpRelEast = 0;
    int hpAltDiff = 0;
    uint32_t hpAddr = 0;

    for (int i = 0; i < MAX_AIRCRAFT; ++i)
    {
        if (!list[i].valid)
        {
            continue;
        }

        sendAircraftPFLAA(list[i], totalObjects, hpDistance, hpRelNorth, hpRelEast, hpAltDiff, hpAddr);
        sendAircraftContainer(list[i]);
    }

    sendPFLAU(totalObjects, hpDistance, hpRelNorth, hpRelEast, hpAltDiff, hpAddr);
    sendOwnshipFLYRF();

    ++g_diag.batchesSent;
    g_diag.lastBatchMs = nowMs;
}

void NMEA_fini()
{
    if (g_rs485Ready)
    {
        RS485_SERIAL.flush();
        RS485_SERIAL.end();
    }
    g_rs485Ready = false;
    g_diag.rs485Ready = false;
}

bool NMEA_getDiag(NMEADiag& outDiag)
{
    outDiag = g_diag;
    return true;
}
