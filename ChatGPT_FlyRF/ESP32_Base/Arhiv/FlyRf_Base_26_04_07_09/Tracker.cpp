#include "Tracker.h"

#include <Arduino.h>
#include <SoftwareSerial.h>
#include <TimeLib.h>
#include <math.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "Container.h"
#include "DeviceInfo.h"
#include "EEPROMRF.h"
#include "GNSS.h"

#ifndef PIN_TRACKER_RX
#define PIN_TRACKER_RX 38
#endif

#ifndef PIN_TRACKER_TX
#define PIN_TRACKER_TX 39
#endif

#ifndef SERIAL_TRACKER_SPEED
#define SERIAL_TRACKER_SPEED 19200UL
#endif

#ifndef TRACKER_INPUT_BUFFER_SIZE
#define TRACKER_INPUT_BUFFER_SIZE 390U
#endif

namespace
{
    constexpr uint8_t TRACKER_ADDR_TYPE_ICAO = 1U;

    EspSoftwareSerial::UART SERIAL_TRACKER;
    constexpr auto TRACKER_SERIAL_CONFIG = EspSoftwareSerial::SWSERIAL_8N1;
    static char g_rxBuffer[TRACKER_INPUT_BUFFER_SIZE + 1U] = {};
    static size_t g_rxLen = 0U;
    static uint32_t g_lastRxMs = 0U;
    constexpr uint32_t TRACKER_IDLE_FLUSH_MS = 30U;
    constexpr uint16_t TRACKER_TEXT_MAX_CHARS = 172U;
    constexpr uint16_t TRACKER_FLY_TTL_MINUTES = 20U;


    static void trackerWriteLine(const char* text)
    {
        if (text == nullptr) return;
        SERIAL_TRACKER.print(text);
        SERIAL_TRACKER.print("\r\n");
    }

    static void trackerWriteRawLine(const char* text)
    {
        if (text == nullptr) return;
        const size_t len = strlen(text);
        if (len > 0U)
        {
            SERIAL_TRACKER.write(reinterpret_cast<const uint8_t*>(text), len);
        }
        SERIAL_TRACKER.write('\r');
        SERIAL_TRACKER.write('\n');
    }

    static void trackerReplyOk(const char* cmd)
    {
        char buffer[32];
        if (cmd == nullptr || cmd[0] == '\0')
        {
            trackerWriteLine("OK");
            return;
        }
        snprintf(buffer, sizeof(buffer), "OK %s", cmd);
        trackerWriteLine(buffer);
    }

    static void trackerReplyErrBadParams(const char* cmd)
    {
        char buffer[48];
        if (cmd == nullptr || cmd[0] == '\0')
        {
            trackerWriteLine("ERR|BAD_PARAMS");
            return;
        }
        snprintf(buffer, sizeof(buffer), "ERR %s|BAD_PARAMS", cmd);
        trackerWriteLine(buffer);
    }

    static void trackerReplyUnknownCommand()
    {
        trackerWriteLine("UNKNOWN COMMAND");
    }

    static void trackerReplyTextTooLong()
    {
        trackerWriteLine("The text is very long. !Maximum 172 characters");
    }

    static void trimAscii(char* text)
    {
        if (text == nullptr) return;

        size_t len = strlen(text);
        while (len > 0U && (text[len - 1U] == ' ' || text[len - 1U] == '\t' || text[len - 1U] == '\r' || text[len - 1U] == '\n'))
        {
            text[--len] = '\0';
        }

        char* start = text;
        while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n')
        {
            ++start;
        }

        if (start != text)
        {
            memmove(text, start, strlen(start) + 1U);
        }
    }

    static void strToUpperAscii(char* text)
    {
        if (text == nullptr) return;
        for (; *text != '\0'; ++text)
        {
            *text = (char)toupper((unsigned char)*text);
        }
    }

    static bool parseUnsignedInRange(const char* text, unsigned long minValue, unsigned long maxValue, unsigned long& outValue, int base = 10)
    {
        if (text == nullptr || *text == '\0') return false;

        char* endPtr = nullptr;
        const unsigned long value = strtoul(text, &endPtr, base);
        if (endPtr == text || (endPtr != nullptr && *endPtr != '\0')) return false;
        if (value < minValue || value > maxValue) return false;
        outValue = value;
        return true;
    }

    static bool parseFloatStrict(const char* text, float& outValue)
    {
        if (text == nullptr || *text == '\0') return false;
        char* endPtr = nullptr;
        const float value = strtof(text, &endPtr);
        if (endPtr == text || (endPtr != nullptr && *endPtr != '\0')) return false;
        outValue = value;
        return true;
    }

    static size_t utf8Length(const char* text)
    {
        if (text == nullptr) return 0U;

        size_t count = 0U;
        const uint8_t* p = reinterpret_cast<const uint8_t*>(text);
        while (*p != 0U)
        {
            if ((*p & 0xC0U) != 0x80U)
            {
                ++count;
            }
            ++p;
        }
        return count;
    }

    static uint8_t nmeaChecksumBody(const char* body)
    {
        uint8_t checksum = 0U;
        if (body == nullptr) return 0U;
        while (*body != '\0')
        {
            checksum ^= (uint8_t)(*body++);
        }
        return checksum;
    }

    static void trackerWriteNmeaBody(const char* body)
    {
        if (body == nullptr || body[0] == '\0') return;
        char line[196];
        const uint8_t cs = nmeaChecksumBody(body);
        snprintf(line, sizeof(line), "$%s*%02X\r\n", body, cs);
        SERIAL_TRACKER.print(line);
    }

    static uint16_t currentTrackerMinutesOfDay()
    {
        if (GNSS_timeValid())
        {
            const time_t t = now();
            return (uint16_t)((hour(t) * 60) + minute(t));
        }
        return (uint16_t)((10 * 60) + 20);
    }

    static bool trackerMessageFresh(uint8_t msgHour, uint8_t msgMinute)
    {
        if (msgHour > 23U || msgMinute > 59U) return false;

        const uint16_t nowMin = currentTrackerMinutesOfDay();
        const uint16_t msgMin = (uint16_t)(msgHour * 60U + msgMinute);
        uint16_t ageMin = (nowMin >= msgMin) ? (uint16_t)(nowMin - msgMin)
                                             : (uint16_t)(1440U + nowMin - msgMin);
        return ageMin <= TRACKER_FLY_TTL_MINUTES;
    }

    static void fillCallsignTracker(char* dst, size_t dstSize, const char* src)
    {
        if (dst == nullptr || dstSize == 0U) return;
        memset(dst, ' ', dstSize);
        if (src == nullptr) return;

        size_t out = 0U;
        for (size_t i = 0U; src[i] != '\0' && out < dstSize; ++i)
        {
            const char c = src[i];
            if (c == ',' || c == '#' || c == '\r' || c == '\n') break;
            dst[out++] = c;
        }
    }

    static bool parseCsvFields(char* text, char* fields[], size_t expectedCount)
    {
        if (text == nullptr || fields == nullptr || expectedCount == 0U) return false;

        size_t count = 0U;
        char* savePtr = nullptr;
        for (char* token = strtok_r(text, ",", &savePtr); token != nullptr; token = strtok_r(nullptr, ",", &savePtr))
        {
            trimAscii(token);
            if (count >= expectedCount) return false;
            fields[count++] = token;
        }
        return count == expectedCount;
    }

    static bool handleSetFly(char* params)
    {
        if (params == nullptr || *params == '\0')
        {
            trackerReplyErrBadParams("FLY");
            return false;
        }

        char work[TRACKER_INPUT_BUFFER_SIZE + 1U];
        strncpy(work, params, sizeof(work) - 1U);
        work[sizeof(work) - 1U] = '\0';

        char* fields[13] = {};
        if (!parseCsvFields(work, fields, 13U))
        {
            trackerReplyErrBadParams("FLY");
            return false;
        }

        unsigned long addr = 0UL;
        unsigned long squawk = 0UL;
        unsigned long aircraftType = 0UL;
        unsigned long hourMsg = 0UL;
        unsigned long minuteMsg = 0UL;
        float altitude = 0.0f;
        float pressureAltitude = 0.0f;
        float speed = 0.0f;
        float course = 0.0f;
        float latitude = 0.0f;
        float longitude = 0.0f;
        long vertRate = 0L;

        if (!parseUnsignedInRange(fields[0], 1UL, 0xFFFFFFUL, addr, 16) ||
            !parseUnsignedInRange(fields[1], 0UL, 9999UL, squawk, 10) ||
            !parseFloatStrict(fields[3], altitude) ||
            !parseFloatStrict(fields[4], pressureAltitude) ||
            !parseFloatStrict(fields[5], speed) ||
            !parseFloatStrict(fields[6], course) ||
            !parseUnsignedInRange(fields[10], 0UL, 15UL, aircraftType, 10) ||
            !parseUnsignedInRange(fields[11], 0UL, 23UL, hourMsg, 10) ||
            !parseUnsignedInRange(fields[12], 0UL, 59UL, minuteMsg, 10))
        {
            trackerReplyErrBadParams("FLY");
            return false;
        }

        if (fields[2] == nullptr || strlen(fields[2]) == 0U || strlen(fields[2]) > 8U)
        {
            trackerReplyErrBadParams("FLY");
            return false;
        }

        if (fields[7] == nullptr || *fields[7] == '\0')
        {
            trackerReplyErrBadParams("FLY");
            return false;
        }
        char* endVert = nullptr;
        vertRate = strtol(fields[7], &endVert, 10);
        if (endVert == fields[7] || (endVert != nullptr && *endVert != '\0'))
        {
            trackerReplyErrBadParams("FLY");
            return false;
        }

        if (!parseFloatStrict(fields[8], latitude) || !parseFloatStrict(fields[9], longitude))
        {
            trackerReplyErrBadParams("FLY");
            return false;
        }

        if (!trackerMessageFresh((uint8_t)hourMsg, (uint8_t)minuteMsg))
        {
            trackerReplyErrBadParams("FLY");
            return false;
        }

        ufo_t traffic = EmptyFO;
        traffic.addr = (uint32_t)(addr & 0x00FFFFFFUL);
        traffic.protocol = (settings != nullptr) ? settings->rf_protocol : 0U;
        traffic.addr_type = TRACKER_ADDR_TYPE_ICAO;
        traffic.squawk = (int)squawk;
        fillCallsignTracker(traffic.callsign, sizeof(traffic.callsign), fields[2]);
        traffic.altitude = altitude;
        traffic.pressure_altitude = pressureAltitude;
        traffic.speed = speed;
        traffic.course = course;
        traffic.vert_rate = (int)vertRate;
        traffic.latitude = latitude;
        traffic.longitude = longitude;
        traffic.aircraft_type = (uint8_t)aircraftType;
        traffic.hour_msg = (uint8_t)hourMsg;
        traffic.min_msg = (uint8_t)minuteMsg;
        traffic.timestamp = (time_t)(millis() / 1000UL);
        traffic.timemsg = traffic.timestamp;
        traffic.seen = traffic.timestamp;
        traffic.signal_source = 2U;
        traffic.source = (TrafficSource)2U;
        traffic.rssi = 0;
        traffic.rssi_LoRa = 0;
        traffic.rssi_rp2040 = 0;
        traffic.snr = 0.0f;
        traffic.valid = true;

        (void)Traffic_Add(&traffic);
        trackerReplyOk("FLY");
        return true;
    }

    static void sendThisAircraftToTracker()
    {
        char body[160];
        snprintf(body, sizeof(body),
                 "%06lX,%d,%d,%d,%d,%.6f,%.6f,%u",
                 (unsigned long)(ThisAircraft.addr & 0x00FFFFFFUL),
                 (int)lroundf(ThisAircraft.altitude),
                 (int)lroundf(ThisAircraft.pressure_altitude),
                 (int)lroundf(ThisAircraft.speed),
                 (int)lroundf(ThisAircraft.course),
                 ThisAircraft.latitude,
                 ThisAircraft.longitude,
                 (unsigned int)ThisAircraft.aircraft_type);
        trackerWriteNmeaBody(body);
    }

    static void sendBaseToTracker()
    {
        for (int i = 0; i < MAX_TRACKING_OBJECTS; ++i)
        {
            if (Container[i].addr == 0U) continue;

            char callsign[9];
            memset(callsign, 0, sizeof(callsign));
            memcpy(callsign, Container[i].callsign, sizeof(Container[i].callsign));
            for (int j = (int)sizeof(callsign) - 2; j >= 0; --j)
            {
                if (callsign[j] == ' ') callsign[j] = '\0';
                else break;
            }

            char body[192];
            snprintf(body, sizeof(body),
                     "%06lX,%d,%s,%d,%d,%d,%d,%d,%.6f,%.6f,%u,%u,%u,%u",
                     (unsigned long)(Container[i].addr & 0x00FFFFFFUL),
                     Container[i].squawk,
                     callsign,
                     (int)lroundf(Container[i].altitude),
                     (int)lroundf(Container[i].pressure_altitude),
                     (int)lroundf(Container[i].speed),
                     (int)lroundf(Container[i].course),
                     Container[i].vert_rate,
                     Container[i].latitude,
                     Container[i].longitude,
                     (unsigned int)Container[i].aircraft_type,
                     (unsigned int)Container[i].signal_source,
                     (unsigned int)Container[i].hour_msg,
                     (unsigned int)Container[i].min_msg);
            trackerWriteNmeaBody(body);
        }
    }

    static void handleGetCommand(const char* cmd)
    {
        if (cmd == nullptr || *cmd == '\0')
        {
            trackerReplyUnknownCommand();
            return;
        }

        if (strcmp(cmd, "VER") == 0)
        {
            trackerReplyOk("VER");
            SERIAL_TRACKER.flush();
            delay(2);

            char versionBuf[64] = {};
            const String& version = DeviceInfo_programVersion();
            version.toCharArray(versionBuf, sizeof(versionBuf));

            if (versionBuf[0] == '\0')
            {
                strncpy(versionBuf, "FlyRf_Base", sizeof(versionBuf) - 1U);
                versionBuf[sizeof(versionBuf) - 1U] = '\0';
            }

            for (size_t i = 0U; versionBuf[i] != '\0'; ++i)
            {
                const char c = versionBuf[i];
                const bool ok = ((c >= '0' && c <= '9') ||
                                 (c >= 'A' && c <= 'Z') ||
                                 (c >= 'a' && c <= 'z') ||
                                 c == '_' || c == '-' || c == '.');
                if (!ok)
                {
                    versionBuf[i] = '_';
                }
            }

            trackerWriteRawLine(versionBuf);
            return;
        }
        if (strcmp(cmd, "FLY") == 0)
        {
            trackerReplyOk("FLY");
            sendThisAircraftToTracker();
            return;
        }
        if (strcmp(cmd, "BASE") == 0)
        {
            trackerReplyOk("BASE");
            sendBaseToTracker();
            return;
        }

        trackerReplyUnknownCommand();
    }

    static void handleSetCommand(const char* cmd, char* params)
    {
        if (cmd == nullptr || *cmd == '\0')
        {
            trackerReplyUnknownCommand();
            return;
        }

        if (strcmp(cmd, "TXT") == 0)
        {
            const size_t textLen = utf8Length(params != nullptr ? params : "");
            if (textLen > TRACKER_TEXT_MAX_CHARS)
            {
                trackerReplyTextTooLong();
                return;
            }
            trackerReplyOk("TXT");
            return;
        }
        if (strcmp(cmd, "FLY") == 0)
        {
            handleSetFly(params);
            return;
        }
        if (strcmp(cmd, "GSM") == 0)
        {
            trackerReplyOk("GSM");
            return;
        }
        if (strcmp(cmd, "BTOK") == 0)
        {
            trackerReplyOk("BTOK");
            return;
        }

        trackerReplyUnknownCommand();
    }

    static void processTrackerMessage(char* message)
    {
        if (message == nullptr) return;
        trimAscii(message);
        if (message[0] == '\0') return;
        if (message[0] != '#')
        {
            trackerReplyUnknownCommand();
            return;
        }

        char* fields[4] = {};
        size_t fieldCount = 0U;
        char* p = message + 1;
        fields[fieldCount++] = p;
        while (*p != '\0' && fieldCount < 4U)
        {
            if (*p == '#')
            {
                *p = '\0';
                fields[fieldCount++] = p + 1;
            }
            ++p;
        }

        if (fieldCount < 3U)
        {
            trackerReplyUnknownCommand();
            return;
        }

        trimAscii(fields[0]);
        trimAscii(fields[1]);
        trimAscii(fields[2]);
        if (fieldCount >= 4U) trimAscii(fields[3]);

        unsigned long messageId = 0UL;
        if (!parseUnsignedInRange(fields[0], 1UL, 99UL, messageId, 10))
        {
            trackerReplyUnknownCommand();
            return;
        }
        (void)messageId;

        strToUpperAscii(fields[1]);
        strToUpperAscii(fields[2]);

        if (strcmp(fields[1], "GET") == 0)
        {
            handleGetCommand(fields[2]);
            return;
        }
        if (strcmp(fields[1], "SET") == 0)
        {
            handleSetCommand(fields[2], (fieldCount >= 4U) ? fields[3] : nullptr);
            return;
        }

        trackerReplyUnknownCommand();
    }

    static void finalizeIncomingFrame()
    {
        if (g_rxLen == 0U) return;
        g_rxBuffer[g_rxLen] = '\0';
        processTrackerMessage(g_rxBuffer);
        g_rxLen = 0U;
        g_rxBuffer[0] = '\0';
    }
}

void Tracker_setup()
{
    g_rxLen = 0U;
    g_rxBuffer[0] = '\0';
    g_lastRxMs = millis();
    SERIAL_TRACKER.begin(SERIAL_TRACKER_SPEED, TRACKER_SERIAL_CONFIG, PIN_TRACKER_RX, PIN_TRACKER_TX, false, 256);
    SERIAL_TRACKER.enableIntTx(false);
}

void Tracker_loop()
{
    while (SERIAL_TRACKER.available() > 0)
    {
        const int value = SERIAL_TRACKER.read();
        if (value < 0) break;

        g_lastRxMs = millis();
        const char ch = (char)value;
        if (ch == '\r' || ch == '\n')
        {
            finalizeIncomingFrame();
            continue;
        }

        if (g_rxLen < TRACKER_INPUT_BUFFER_SIZE)
        {
            g_rxBuffer[g_rxLen++] = ch;
        }
        else
        {
            g_rxLen = 0U;
            g_rxBuffer[0] = '\0';
            trackerReplyTextTooLong();
        }
    }

    if (g_rxLen > 0U && (uint32_t)(millis() - g_lastRxMs) >= TRACKER_IDLE_FLUSH_MS)
    {
        finalizeIncomingFrame();
    }
}

void Tracker_fini()
{
    SERIAL_TRACKER.flush();
}
