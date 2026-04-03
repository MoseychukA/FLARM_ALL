#include "NMEA.h"
#include "TrafficDB.h"
#include "DeviceInfo.h"
#include "EEPROMRF.h"
#include <HardwareSerial.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

static HardwareSerial RS485Serial(2);
static bool g_rs485Ready = false;
static NMEADiag g_diag = {};

static uint8_t g_lastAnnouncedSerialMode = 0xFFU;

static const char* serialModeHeaderName(uint8_t mode)
{
    switch (mode)
    {
        case OUTPUT_MODE_CONTAINER: return "Container";
        case OUTPUT_MODE_NMEA: return "NMEA";
        case OUTPUT_MODE_RP2040: return "RP2040 RX";
        case OUTPUT_MODE_FLARM: return "FLARM RX";
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
    RS485Serial.write((const uint8_t*)data, len);
    RS485Serial.flush();
#if SOC_GPIO_PIN_RS485_DE >= 0
    delayMicroseconds(100);
    digitalWrite(SOC_GPIO_PIN_RS485_DE, LOW);
#endif
}

static void sendLineToSelectedOutputs(const char* line, size_t len, bool nmeaLine)
{
    if (settings == nullptr || line == nullptr || len == 0)
    {
        return;
    }

    const uint8_t serialMode = settings->serial_out;
    const uint8_t rs485Mode = settings->rs485_out;
    g_diag.serialMode = serialMode;
    g_diag.rs485Mode = rs485Mode;
    g_diag.lanMode = 0;

    if (serialMode == OUTPUT_MODE_NMEA && nmeaLine)
    {
        Serial.write((const uint8_t*)line, len);
        ++g_diag.sentencesSerial;
    }

    if (rs485Mode != OUTPUT_MODE_RS485_DISPLAY && ((rs485Mode == OUTPUT_MODE_NMEA && nmeaLine) ||
        (rs485Mode == OUTPUT_MODE_CONTAINER && !nmeaLine)))
    {
        rs485Write(line, len);
        ++g_diag.sentencesRS485;
    }

}

static bool aircraftHasCoordinates(const Aircraft& ac)
{
    return fabsf(ac.lat) > 0.00001f || fabsf(ac.lon) > 0.00001f;
}

static void sendOwnshipHeartbeatNMEA()
{
    char body[96];
    char sentence[120];
    snprintf(body, sizeof(body),
             "PFLAU,0,1,1,1,0,,0,,,*");

    // wrapNMEALine() computes the correct checksum, so trim a manual '*' stub if present.
    const size_t bodyLen = strlen(body);
    if (bodyLen > 0 && body[bodyLen - 1] == '*')
    {
        body[bodyLen - 1] = 0;
    }

    const size_t len = wrapNMEALine(body, sentence, sizeof(sentence));
    sendLineToSelectedOutputs(sentence, len, true);
}

static void sendAircraftNMEA(const Aircraft& ac)
{
    char body[160];
    char sentence[180];
    snprintf(body, sizeof(body),
             "PFLAA,%06lX,%u,%.6f,%.6f,%d,%.1f,%.1f,%d,%.1f,%lu",
             (unsigned long)ac.icao,
             (unsigned)ac.source,
             ac.lat,
             ac.lon,
             ac.altitude,
             ac.speed,
             ac.course,
             ac.rssi,
             ac.snr,
             (unsigned long)(millis() - ac.lastUpdate));

    const size_t len = wrapNMEALine(body, sentence, sizeof(sentence));
    sendLineToSelectedOutputs(sentence, len, true);
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
                                 ac.altitude,
                                 ac.speed,
                                 ac.course,
                                 ac.rssi,
                                 ac.snr,
                                 (unsigned long)(millis() - ac.lastUpdate));
    if (written > 0)
    {
        sendLineToSelectedOutputs(line, (size_t)written, false);
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

#if SOC_GPIO_PIN_RS485_DE >= 0
    pinMode(SOC_GPIO_PIN_RS485_DE, OUTPUT);
    digitalWrite(SOC_GPIO_PIN_RS485_DE, LOW);
#endif

#if SOC_GPIO_PIN_RS485_RX >= 0 && SOC_GPIO_PIN_RS485_TX >= 0
    if (settings == nullptr || settings->rs485_out != OUTPUT_MODE_RS485_DISPLAY)
    {
        RS485Serial.begin(115200, SERIAL_8N1, SOC_GPIO_PIN_RS485_RX, SOC_GPIO_PIN_RS485_TX);
        RS485Serial.setRxBufferSize(1024);
        RS485Serial.setTxBufferSize(1024);
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
    const uint32_t now = millis();
    if ((uint32_t)(now - lastSendMs) < NMEA_UPDATE_INTERVAL_MS)
    {
        return;
    }
    lastSendMs = now;

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

    for (int i = 0; i < MAX_AIRCRAFT; ++i)
    {
        if (!list[i].valid)
        {
            continue;
        }

        if (aircraftHasCoordinates(list[i]))
        {
            sendAircraftNMEA(list[i]);
        }
        sendAircraftContainer(list[i]);
    }

    ++g_diag.batchesSent;
    g_diag.lastBatchMs = now;
}

void NMEA_fini()
{
    if (g_rs485Ready)
    {
        RS485Serial.flush();
        RS485Serial.end();
    }
    g_rs485Ready = false;
    g_diag.rs485Ready = false;
}

bool NMEA_getDiag(NMEADiag& outDiag)
{
    outDiag = g_diag;
    return true;
}
