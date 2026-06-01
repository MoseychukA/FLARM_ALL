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
#include "LANRF.h"
#include "WiFiRF.h"
#include <TimeLib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#ifndef DEG_TO_RAD
#define DEG_TO_RAD 0.017453292519943295769236907684886f
#endif

static bool g_rs485Ready = false;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
static NMEADiag g_diag = {};  // Логический флаг состояния: показывает, разрешена ли операция, активен ли режим или есть ли данные.
static uint8_t g_lastAnnouncedSerialMode = 0xFFU;  // Навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта.
static uint32_t g_lastPgrmzMs = 0;  // Логический флаг состояния: показывает, разрешена ли операция, активен ли режим или есть ли данные.

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `serialModeHeaderName` и обрабатывает порт Serial режим заголовок name в контексте модуля NMEA.cpp.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
static const char* serialModeHeaderName(uint8_t mode)
{
    switch (mode)
    {
        case OUTPUT_MODE_CONTAINER: return "Container";
        case OUTPUT_MODE_NMEA: return "NMEA";
        case OUTPUT_MODE_RP2040: return "RP2040 RX";
        case OUTPUT_MODE_FLARM: return "FLARM RX";
        case OUTPUT_MODE_LORA_RAW: return "LoRa RAW RX";
        case OUTPUT_MODE_MAVLINK: return "MAVLink";
        default: return "Off";
    }
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `NMEA_announceSerialModeIfNeeded` и обрабатывает nmea announce порт Serial режим if needed в контексте модуля NMEA.cpp.
// Локальные переменные: uint8_t — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
//------------------------------------------------------------------------------
void NMEA_announceSerialModeIfNeeded()
{
    const uint8_t mode = (settings != nullptr) ? settings->serial_out : OUTPUT_MODE_OFF;
    if (mode == g_lastAnnouncedSerialMode)
    {
        return;
    }

    g_lastAnnouncedSerialMode = mode;
    if (mode == OUTPUT_MODE_MAVLINK)
    {
        return;
    }

    Serial.print("=== SERIAL MODE: ");
    Serial.print(serialModeHeaderName(mode));
    Serial.print(" ===\r\n");
    vTaskDelay(pdMS_TO_TICKS(4));
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `nmeaChecksum` и обрабатывает nmea контрольную сумму в контексте модуля NMEA.cpp.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
static uint8_t nmeaChecksum(const char* sentenceWithoutChecksum)
{
    uint8_t cs = 0;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
    if (sentenceWithoutChecksum == nullptr) return 0;
    while (*sentenceWithoutChecksum)
    {
        cs ^= (uint8_t)*sentenceWithoutChecksum++;
    }
    return cs;
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `wrapNMEALine` и обрабатывает wrap nmealine в контексте модуля NMEA.cpp.
// Локальные переменные: uint8_t — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; written — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; outSize — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; body — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; cs — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `rs485Write` и обрабатывает канал RS485 в контексте модуля NMEA.cpp.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Назначение функции: Формирует и отправляет nmea configured output через нужный интерфейс связи или вывода.
// Локальные переменные: sent — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
//------------------------------------------------------------------------------
static bool sendNmeaToConfiguredOutput(const char* line, size_t len)
{
    if (settings == nullptr || line == nullptr || len == 0)
    {
        return false;
    }

    g_diag.nmeaOutput = settings->nmea_out;
    g_diag.udpPort = (settings->udp_port != 0U) ? settings->udp_port : (uint16_t)NMEA_UDP_PORT;
    g_diag.bluetoothMode = settings->bluetooth;

    bool sent = false;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
    switch (settings->nmea_out)
    {
        case NMEA_OUTPUT_SERIAL:
            Serial.write((const uint8_t*)line, len);
            ++g_diag.sentencesSerial;
            sent = true;
            break;
        case NMEA_OUTPUT_UDP:
        {
            bool udpSent = false;
            if (LAN_udpWorking())
            {
                LAN_sendUDP((const uint8_t*)line, len);
                udpSent = true;
            }
            if (WiFi_transmitUDP(g_diag.udpPort, (const uint8_t*)line, len))
            {
                udpSent = true;
            }
            if (udpSent)
            {
                ++g_diag.sentencesUDP;
                sent = true;
            }
            break;
        }
        case NMEA_OUTPUT_BLUETOOTH:
            if (settings->bluetooth != BLUETOOTH_OFF)
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

//------------------------------------------------------------------------------
// Назначение функции: Формирует и отправляет базу целей Container configured outputs через нужный интерфейс связи или вывода.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `aircraftHasCoordinates` и обрабатывает самолет coordinates в контексте модуля NMEA.cpp.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
static bool aircraftHasCoordinates(const Aircraft& ac)
{
    return fabsf(ac.lat) > 0.00001f || fabsf(ac.lon) > 0.00001f;
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `sanitizeCallsign` и обрабатывает sanitize callsign в контексте модуля NMEA.cpp.
// Локальные переменные: outIdx — счетчик или индекс: указывает позицию элемента, номер строки, слота или текущую стадию перебора.
//------------------------------------------------------------------------------
static void sanitizeCallsign(const char* src, char* dst, size_t dstSize)
{
    if (dst == nullptr || dstSize == 0) return;
    memset(dst, 0, dstSize);
    if (src == nullptr) return;

    size_t outIdx = 0;  // Счетчик или индекс: указывает позицию элемента, номер строки, слота или текущую стадию перебора.
    for (size_t i = 0; src[i] != '\0' && outIdx < (dstSize - 1); ++i)
    {
        const char c = src[i];  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
        if (c == ' ' || c == ',' || c == '*') break;
        dst[outIdx++] = c;
    }
    dst[outIdx] = 0;
}

//------------------------------------------------------------------------------
// Назначение функции: Формирует fallback callsign в готовом виде для вывода, передачи или последующей обработки.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
static void makeFallbackCallsign(const Aircraft& ac, char* dst, size_t dstSize)
{
    if (dst == nullptr || dstSize == 0) return;
    snprintf(dst, dstSize, "T_%06lX", (unsigned long)(ac.addr & 0xFFFFFFUL));
}

//------------------------------------------------------------------------------
// Назначение функции: Формирует date время fields в готовом виде для вывода, передачи или последующей обработки.
// Локальные переменные: t — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Назначение функции: Формирует и отправляет ownship rmc через нужный интерфейс связи или вывода.
// Локальные переменные: lat — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; lon — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; latHem — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; lonHem — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; absLat — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; absLon — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта.
// - utcTime: Временная отметка, интервал или значение тайм-аута.
// - utcDate: Служебная переменная, используемая для промежуточных вычислений и логики модуля.
// - body: Параметр геометрии, координаты, размера или угла.
// - sentence: Служебная переменная, используемая для промежуточных вычислений и логики модуля.
//------------------------------------------------------------------------------
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

    char utcTime[16] = {};  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
    char utcDate[16] = {};  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
    if (!buildDateTimeFields(utcTime, sizeof(utcTime), utcDate, sizeof(utcDate))) return;

    char body[160];  // Навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта.
    char sentence[180];  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
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

//------------------------------------------------------------------------------
// Назначение функции: Формирует и отправляет ownship gga через нужный интерфейс связи или вывода.
// Локальные переменные: lat — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; lon — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; latHem — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; lonHem — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; absLat — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; absLon — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта.
// - utcTime: Временная отметка, интервал или значение тайм-аута.
// - utcDate: Служебная переменная, используемая для промежуточных вычислений и логики модуля.
// - body: Параметр геометрии, координаты, размера или угла.
// - sentence: Служебная переменная, используемая для промежуточных вычислений и логики модуля.
//------------------------------------------------------------------------------
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

    char utcTime[16] = {};  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
    char utcDate[16] = {};  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
    if (!buildDateTimeFields(utcTime, sizeof(utcTime), utcDate, sizeof(utcDate))) return;
    (void)utcDate;

    char body[160];  // Навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта.
    char sentence[180];  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
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

//------------------------------------------------------------------------------
// Назначение функции: Формирует и отправляет ownship pgrmz через нужный интерфейс связи или вывода.
// Локальные переменные: altitudeFt — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; body — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; sentence — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; len — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; sizeof — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
// - body: Параметр геометрии, координаты, размера или угла.
// - sentence: Служебная переменная, используемая для промежуточных вычислений и логики модуля.
//------------------------------------------------------------------------------
static void sendOwnshipPGRMZ()
{
    if (settings == nullptr) return;
    if ((uint32_t)(millis() - g_lastPgrmzMs) < 1000UL) return;

    const int altitudeFt = constrain((int)lroundf(ThisAircraft.pressure_altitude * 3.28084f), -1000, 60000);
    char body[64];  // Навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта.
    char sentence[80];  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
    snprintf(body, sizeof(body), "PGRMZ,%d,f,%c", altitudeFt, GNSS_coordinatesValid() ? '3' : '1');
    const size_t len = wrapNMEALine(body, sentence, sizeof(sentence));
    if (len > 0)
    {
        sendNmeaToConfiguredOutput(sentence, len);
        g_lastPgrmzMs = millis();
    }
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `exportSignalSource` и обрабатывает export signal source в контексте модуля NMEA.cpp.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
static int exportSignalSource(uint8_t source)
{
    switch (source)
    {
        case TRAFFIC_SOURCE_FLARM_LORA: return 1;
        case TRAFFIC_SOURCE_ADSB_DUMP1090: return 2;
        default: return 0;
    }
}

//------------------------------------------------------------------------------
// Назначение функции: Формирует и отправляет ownship flyrf через нужный интерфейс связи или вывода.
// Локальные переменные: lat — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; lon — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; hasUtc — логический флаг состояния: показывает, разрешена ли операция, активен ли режим или есть ли данные; t — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; hourMsg — временной параметр или отметка времени: используется для тайм-аутов, задержек, мигания или контроля давности данных; minMsg — временной параметр или отметка времени: используется для тайм-аутов, задержек, мигания или контроля давности данных.
// - callsign: Служебная переменная, используемая для промежуточных вычислений и логики модуля.
// - body: Параметр геометрии, координаты, размера или угла.
// - sentence: Служебная переменная, используемая для промежуточных вычислений и логики модуля.
//------------------------------------------------------------------------------
static void sendOwnshipFLYRF()
{
    if (settings == nullptr) return;
    if (ThisAircraft.addr == 0U) return;

    const bool hasCoords =
        (fabsf(ThisAircraft.latitude) > 0.00001f || fabsf(ThisAircraft.longitude) > 0.00001f ||
         fabsf(ThisAircraft.local_latitude) > 0.00001f || fabsf(ThisAircraft.local_longitude) > 0.00001f ||
         fabsf(ThisAircraft.old_latitude) > 0.00001f || fabsf(ThisAircraft.old_longitude) > 0.00001f);
    if (!hasCoords) return;

    float lat = ThisAircraft.latitude;  // Навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта.
    float lon = ThisAircraft.longitude;  // Навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта.
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

    char callsign[16] = {};  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
    sanitizeCallsign(ThisAircraft.callsign, callsign, sizeof(callsign));
    if (callsign[0] == '\0')
    {
        snprintf(callsign, sizeof(callsign), "SELF%06lX", (unsigned long)(ThisAircraft.addr & 0xFFFFFFUL));
    }

    const bool hasUtc = GNSS_timeValid();
    const time_t t = hasUtc ? now() : 0;
    const int hourMsg = hasUtc ? hour(t) : 0;
    const int minMsg = hasUtc ? minute(t) : 0;

    char body[256];  // Навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта.
    char sentence[280];  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
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

//------------------------------------------------------------------------------
// Назначение функции: Формирует и отправляет самолет pflaa через нужный интерфейс связи или вывода.
// Локальные переменные: relNorth — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; relEast — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; relVertical — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; alarmLevel — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; body — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; sentence — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
// - callsign: Служебная переменная, используемая для промежуточных вычислений и логики модуля.
// - climbRate: Служебная переменная, используемая для промежуточных вычислений и логики модуля.
// - body: Параметр геометрии, координаты, размера или угла.
// - sentence: Служебная переменная, используемая для промежуточных вычислений и логики модуля.
//------------------------------------------------------------------------------
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
    char callsign[16] = {};  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
    sanitizeCallsign(ac.callsign, callsign, sizeof(callsign));
    if (callsign[0] == '\0')
    {
        makeFallbackCallsign(ac, callsign, sizeof(callsign));
    }

    char climbRate[12] = {};  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
    snprintf(climbRate, sizeof(climbRate), "%.1f", ac.vert_rate / 196.8504f);

    char body[220];  // Навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта.
    char sentence[244];  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
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

//------------------------------------------------------------------------------
// Назначение функции: Формирует и отправляет pflau через нужный интерфейс связи или вывода.
// Локальные переменные: body — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; sentence — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; relBearing — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; len — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; sizeof — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
// - body: Параметр геометрии, координаты, размера или угла.
// - sentence: Служебная переменная, используемая для промежуточных вычислений и логики модуля.
//------------------------------------------------------------------------------
static void sendPFLAU(int totalObjects, float hpDistance, int hpRelNorth, int hpRelEast, int hpAltDiff, uint32_t hpAddr)
{
    if (settings == nullptr) return;

    char body[160];  // Навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта.
    char sentence[180];  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
    int relBearing = 0;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
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

//------------------------------------------------------------------------------
// Назначение функции: Формирует и отправляет самолет базу целей Container через нужный интерфейс связи или вывода.
// Локальные переменные: line — текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
// - line: Счетчик, индекс, позиция или номер элемента.
//------------------------------------------------------------------------------
static void sendAircraftContainer(const Aircraft& ac)
{
    char line[180];  // Текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
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

//------------------------------------------------------------------------------
// Назначение функции: Формирует callsign string в готовом виде для вывода, передачи или последующей обработки.
// Локальные переменные: copyLen — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; dstSize — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Назначение функции: Формирует и отправляет local самолет порт Serial через нужный интерфейс связи или вывода.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
// - callsign: Служебная переменная, используемая для промежуточных вычислений и логики модуля.
//------------------------------------------------------------------------------
static void printLocalAircraftToSerial(const LocalAircraftState& ac)
{
    char callsign[9] = {};  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
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

//------------------------------------------------------------------------------
// Назначение функции: Формирует и отправляет базу целей Container самолет порт Serial через нужный интерфейс связи или вывода.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
// - callsign: Служебная переменная, используемая для промежуточных вычислений и логики модуля.
//------------------------------------------------------------------------------
static void printContainerAircraftToSerial(const ufo_t* ac)
{
    if (ac == nullptr)
    {
        return;
    }

    char callsign[9] = {};  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
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

//------------------------------------------------------------------------------
// Назначение функции: Формирует и отправляет базу целей Container dump через нужный интерфейс связи или вывода.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Назначение функции: Инициализирует nmea, подготавливает связанные объекты и включает работу соответствующего узла.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Назначение функции: Обслуживает nmea в основном цикле: проверяет события, обновляет состояние и запускает нужные действия.
// Локальные переменные: uint32_t — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; list — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; totalObjects — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; hpDistance — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; hpRelNorth — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; hpRelEast — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
//------------------------------------------------------------------------------
void NMEA_loop()
{
    static uint32_t lastSendMs = 0;  // Временной параметр или отметка времени: используется для тайм-аутов, задержек, мигания или контроля давности данных.
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

    int totalObjects = 0;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
    float hpDistance = 1.0e9f;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
    int hpRelNorth = 0;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
    int hpRelEast = 0;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
    int hpAltDiff = 0;  // Навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта.
    uint32_t hpAddr = 0;  // Параметр конфигурации: хранит выбранный режим, адрес, настройки модуля или пользовательское значение.

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

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `NMEA_fini` и обрабатывает nmea fini в контексте модуля NMEA.cpp.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `NMEA_getDiag` и обрабатывает nmea diag в контексте модуля NMEA.cpp.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
bool NMEA_getDiag(NMEADiag& outDiag)
{
    outDiag = g_diag;
    return true;
}
