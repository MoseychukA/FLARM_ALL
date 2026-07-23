/*
  Модуль Tracker.cpp
  Назначение:
  - Подключение внешнего трекера по отдельному последовательному каналу и обработка его команд.

  Основные задачи модуля:
  - Принимать команды формата #id#GET/SET#CMD#...
  - Обрабатывать запросы версии, текста, текущего самолета и базы целей.
  - Принимать команду SET FLY, преобразовывать параметры цели и добавлять ее в общую базу.
  - Возвращать ответы OK/ERR и обслуживать имитацию полета цели по команде трекера.
*/

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
#include "RS485Display.h"

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
    constexpr uint16_t TRACKER_TEXT_TTL_MINUTES = 10U;
    constexpr uint32_t TRACKER_TEXT_AUTO_CLEAR_MS = 10UL * 60UL * 1000UL;
    constexpr uint32_t TRACKER_SIM_DURATION_MS = 600UL * 1000UL;
    constexpr uint32_t TRACKER_SIM_STEP_MS = 1000UL;
    constexpr size_t TRACKER_SIM_MAX_TARGETS = MAX_TRACKING_OBJECTS;
    constexpr double TRACKER_EARTH_RADIUS_METERS = 6371000.0;

    struct TrackerSimTarget
    {
        bool active = false;
        ufo_t traffic = {};  // Структура данных самолета или цели: хранит параметры борта, используемые при обмене и отображении.
        uint32_t startMs = 0U;
        uint32_t lastUpdateMs = 0U;
    };

    static TrackerSimTarget g_simTargets[TRACKER_SIM_MAX_TARGETS] = {};
    static char g_activeTextMessage[BUFFER_SIZE] = {};  // Текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
    static bool g_hasActiveTextMessage = false;
    static bool g_newTextMessageFlag = false;
    static bool g_allowReadConfirm = false;
    static bool g_messageReceivedFlag = false;
    static uint8_t g_activeTextMessageId = 0U;
    static uint32_t g_activeTextMessageStoredMs = 0U;


    static bool trackerIsTestMode();
    static uint16_t currentTrackerMinutesOfDay();
    static bool parseUnsignedInRange(const char* text, unsigned long minValue, unsigned long maxValue, unsigned long& outValue, int base);
    static void trackerAutoClearExpiredTextMessage();


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

// - buffer: Буфер, текстовая строка или рабочее сообщение.
    static void trackerReplyOk(const char* cmd)
    {
        char buffer[32];  // Текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
        if (cmd == nullptr || cmd[0] == '\0')
        {
            trackerWriteLine("OK ");
            return;
        }
        snprintf(buffer, sizeof(buffer), "OK %s|OK", cmd);
        trackerWriteLine(buffer);
    }

    static void trackerReplyGetOk()
    {
        trackerWriteLine("OK ");
    }

// - buffer: Буфер, текстовая строка или рабочее сообщение.
    static void trackerReplyErrBadParams(const char* cmd)
    {
        char buffer[48];  // Текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
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
        trackerWriteLine("ERR UNKNOWN COMMAND");
    }

    static void trackerReplyTextTooLong()
    {
        trackerWriteLine("The text is very long. !Maximum 172 characters");
    }

    static void trackerSyncAuxToRs485()
    {
        aux_t aux = {};
        RS485Display_getOutgoingAux(&aux);
        aux.new_message = g_newTextMessageFlag;
        aux.message_received = g_messageReceivedFlag;
        aux.confirm_message_M = g_allowReadConfirm;
        memset(aux.msg_resp_M, 0, sizeof(aux.msg_resp_M));
        strncpy(aux.msg_resp_M, g_activeTextMessage, sizeof(aux.msg_resp_M) - 1U);
        RS485Display_setOutgoingAux(&aux);
    }

    static void trackerClearActiveTextMessage(bool markReceived)
    {
        g_hasActiveTextMessage = false;
        g_newTextMessageFlag = false;
        g_allowReadConfirm = false;
        g_messageReceivedFlag = markReceived;
        g_activeTextMessageId = 0U;
        g_activeTextMessageStoredMs = 0U;
        g_activeTextMessage[0] = '\0';
        trackerSyncAuxToRs485();
    }

    static bool trackerCurrentTime(uint8_t& outHour, uint8_t& outMinute)
    {
        if (trackerIsTestMode())
        {
            outHour = 10U;
            outMinute = 10U;
            return true;
        }

        if (GNSS_timeValid())
        {
            const time_t t = now();
            outHour = (uint8_t)hour(t);
            outMinute = (uint8_t)minute(t);
            return true;
        }

        outHour = 10U;
        outMinute = 20U;
        return false;
    }

// - hourBuf: Временная отметка, интервал или значение тайм-аута.
// - minBuf: Буфер, текстовая строка или рабочее сообщение.
    static bool trackerParseClockText(const char* text, uint8_t& outHour, uint8_t& outMinute)
    {
        if (text == nullptr || *text == '\0')
        {
            return false;
        }

        const char* colon = strchr(text, ':');
        if (colon == nullptr)
        {
            return false;
        }

        char hourBuf[4] = {};  // Текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
        char minBuf[4] = {};  // Текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
        const size_t hourLen = (size_t)(colon - text);
        const size_t minLen = strlen(colon + 1);
        if (hourLen == 0U || hourLen > 2U || minLen == 0U || minLen > 2U)
        {
            return false;
        }

        memcpy(hourBuf, text, hourLen);
        memcpy(minBuf, colon + 1, minLen);

        unsigned long hourVal = 0UL;
        unsigned long minVal = 0UL;
        if (!parseUnsignedInRange(hourBuf, 0UL, 23UL, hourVal, 10) ||
            !parseUnsignedInRange(minBuf, 0UL, 59UL, minVal, 10))
        {
            return false;
        }

        outHour = (uint8_t)hourVal;
        outMinute = (uint8_t)minVal;
        return true;
    }

    static bool trackerTextMessageFresh(uint8_t msgHour, uint8_t msgMinute)
    {
        if (msgHour > 23U || msgMinute > 59U)
        {
            return false;
        }

        if (trackerIsTestMode())
        {
            return (msgHour == 10U) && (msgMinute >= 10U) && (msgMinute <= 20U);
        }

        const uint16_t nowMin = currentTrackerMinutesOfDay();
        const uint16_t msgMin = (uint16_t)(msgHour * 60U + msgMinute);
        if (msgMin > nowMin)
        {
            return false;
        }

        const uint16_t ageMin = (uint16_t)(nowMin - msgMin);
        return ageMin <= TRACKER_TEXT_TTL_MINUTES;
    }

// - buffer: Буфер, текстовая строка или рабочее сообщение.
// - countBuffer: Счетчик, индекс, позиция или номер элемента.
    static void trackerReplyMessageReceipt(unsigned long messageId, size_t acceptedChars)
    {
        char buffer[20];  // Текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
        snprintf(buffer, sizeof(buffer), "#%lu", messageId);
        trackerWriteLine(buffer);

        char countBuffer[20];  // Текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
        snprintf(countBuffer, sizeof(countBuffer), "%u", (unsigned)acceptedChars);
        trackerWriteLine(countBuffer);
    }

// - buffer: Буфер, текстовая строка или рабочее сообщение.
    static void trackerReplyReadConfirm(unsigned long messageId)
    {
        char buffer[48];  // Текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
        snprintf(buffer, sizeof(buffer), "#%lu#SET#BTOK", messageId);
        trackerWriteLine(buffer);
    }

    static void trackerReplyIncorrectTime()
    {
        trackerWriteLine("Incorrect time! Please check the time");
    }

    static void trackerStoreTextMessage(unsigned long messageId, const char* timePrefix, const char* text)
    {
        const char* prefixText = (timePrefix != nullptr && timePrefix[0] != '\0') ? timePrefix : "--:--";

        memset(g_activeTextMessage, 0, sizeof(g_activeTextMessage));
        strncpy(g_activeTextMessage, prefixText, sizeof(g_activeTextMessage) - 1U);
        if (text != nullptr && text[0] != '\0')
        {
            strncat(g_activeTextMessage, " ", sizeof(g_activeTextMessage) - strlen(g_activeTextMessage) - 1U);
            strncat(g_activeTextMessage, text, sizeof(g_activeTextMessage) - strlen(g_activeTextMessage) - 1U);
        }

        g_hasActiveTextMessage = true;
        g_newTextMessageFlag = true;
        g_allowReadConfirm = true;
        g_messageReceivedFlag = false;
        g_activeTextMessageId = (uint8_t)messageId;
        g_activeTextMessageStoredMs = millis();
        trackerSyncAuxToRs485();
    }

    static void trackerAutoClearExpiredTextMessage()
    {
        if (!g_hasActiveTextMessage || g_activeTextMessageStoredMs == 0U)
        {
            return;
        }

        const uint32_t nowMs = millis();
        if ((uint32_t)(nowMs - g_activeTextMessageStoredMs) >= TRACKER_TEXT_AUTO_CLEAR_MS)
        {
            trackerClearActiveTextMessage(false);
        }
    }

    static void trimAscii(char* text)
    {
        if (text == nullptr) return;

        size_t len = strlen(text);
        while (len > 0U && (text[len - 1U] == ' ' || text[len - 1U] == '\t' || text[len - 1U] == '\r' || text[len - 1U] == '\n'))
        {
            text[--len] = '\0';
        }

        char* start = text;  // Текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
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

// - line: Счетчик, индекс, позиция или номер элемента.
    static void trackerWriteNmeaBody(const char* body)
    {
        if (body == nullptr || body[0] == '\0') return;
        char line[196];  // Текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
        const uint8_t cs = nmeaChecksumBody(body);
        snprintf(line, sizeof(line), "$%s*%02X\r\n", body, cs);
        SERIAL_TRACKER.print(line);
    }

    static bool trackerIsTestMode()
    {
        return settings != nullptr && settings->mode != FLYRF_MODE_NORMAL;
    }

    static uint16_t currentTrackerMinutesOfDay()
    {
        if (trackerIsTestMode())
        {
            return (uint16_t)((10U * 60U) + 10U);
        }

        if (GNSS_timeValid())
        {
            const time_t t = now();
            return (uint16_t)((hour(t) * 60) + minute(t));
        }
        return (uint16_t)((10U * 60U) + 20U);
    }

    static bool trackerMessageFresh(uint8_t msgHour, uint8_t msgMinute)
    {
        if (msgHour > 23U || msgMinute > 59U) return false;

        if (trackerIsTestMode())
        {
            return (msgHour == 10U) && (msgMinute >= 10U) && (msgMinute <= 20U);
        }

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

    static void trackerProjectCoordinate(double startLatDeg, double startLonDeg, float bearingDeg, double distanceMeters,
                                         float& outLatDeg, float& outLonDeg)
    {
        if (distanceMeters == 0.0)
        {
            outLatDeg = (float)startLatDeg;
            outLonDeg = (float)startLonDeg;
            return;
        }

        const double angularDistance = distanceMeters / TRACKER_EARTH_RADIUS_METERS;
        const double bearingRad = (double)bearingDeg * (PI / 180.0);
        const double lat1 = startLatDeg * (PI / 180.0);
        const double lon1 = startLonDeg * (PI / 180.0);
        const double sinLat1 = sin(lat1);
        const double cosLat1 = cos(lat1);
        const double sinAd = sin(angularDistance);
        const double cosAd = cos(angularDistance);

        const double lat2 = asin(sinLat1 * cosAd + cosLat1 * sinAd * cos(bearingRad));
        const double lon2 = lon1 + atan2(sin(bearingRad) * sinAd * cosLat1,
                                         cosAd - sinLat1 * sin(lat2));

        outLatDeg = (float)(lat2 * (180.0 / PI));
        outLonDeg = (float)(lon2 * (180.0 / PI));

        while (outLonDeg > 180.0f) outLonDeg -= 360.0f;
        while (outLonDeg < -180.0f) outLonDeg += 360.0f;
    }

    static TrackerSimTarget* trackerFindSimTargetByAddr(uint32_t addr)
    {
        for (size_t i = 0; i < TRACKER_SIM_MAX_TARGETS; ++i)
        {
            if (g_simTargets[i].active && g_simTargets[i].traffic.addr == addr)
            {
                return &g_simTargets[i];
            }
        }
        return nullptr;
    }

    static TrackerSimTarget* trackerAcquireSimTarget(uint32_t addr)
    {
        if (TrackerSimTarget* existing = trackerFindSimTargetByAddr(addr))
        {
            return existing;
        }

        for (size_t i = 0; i < TRACKER_SIM_MAX_TARGETS; ++i)
        {
            if (!g_simTargets[i].active)
            {
                return &g_simTargets[i];
            }
        }

        size_t oldestIndex = 0U;
        uint32_t oldestStart = g_simTargets[0].startMs;
        for (size_t i = 1; i < TRACKER_SIM_MAX_TARGETS; ++i)
        {
            if (g_simTargets[i].startMs < oldestStart)
            {
                oldestStart = g_simTargets[i].startMs;
                oldestIndex = i;
            }
        }
        return &g_simTargets[oldestIndex];
    }

    static void trackerStartSimFlight(const ufo_t& traffic)
    {
        TrackerSimTarget* slot = trackerAcquireSimTarget(traffic.addr);
        if (slot == nullptr)
        {
            return;
        }

        const uint32_t nowMs = millis();
        slot->active = true;
        slot->traffic = traffic;
        slot->startMs = nowMs;
        slot->lastUpdateMs = nowMs;
        slot->traffic.timestamp = (time_t)(nowMs / 1000UL);
        slot->traffic.timemsg = slot->traffic.timestamp;
        slot->traffic.seen = slot->traffic.timestamp;
        slot->traffic.valid = true;

        (void)Traffic_Add(&slot->traffic);
    }

    static void trackerServiceSimFlights()
    {
        const uint32_t nowMs = millis();
        for (size_t i = 0; i < TRACKER_SIM_MAX_TARGETS; ++i)
        {
            TrackerSimTarget& target = g_simTargets[i];
            if (!target.active)
            {
                continue;
            }

            if ((uint32_t)(nowMs - target.startMs) >= TRACKER_SIM_DURATION_MS)
            {
                target.active = false;
                continue;
            }

            while ((uint32_t)(nowMs - target.lastUpdateMs) >= TRACKER_SIM_STEP_MS)
            {
                const uint32_t nextUpdateMs = target.lastUpdateMs + TRACKER_SIM_STEP_MS;
                const float dtSec = (float)(nextUpdateMs - target.lastUpdateMs) / 1000.0f;
                target.lastUpdateMs = nextUpdateMs;

                const double distanceMeters = ((double)target.traffic.speed / 3.6) * (double)dtSec;
                float newLat = target.traffic.latitude;
                float newLon = target.traffic.longitude;
                trackerProjectCoordinate((double)target.traffic.latitude, (double)target.traffic.longitude,
                                         target.traffic.course, distanceMeters, newLat, newLon);
                target.traffic.latitude = newLat;
                target.traffic.longitude = newLon;
                target.traffic.timestamp = (time_t)(target.lastUpdateMs / 1000UL);
                target.traffic.timemsg = target.traffic.timestamp;
                target.traffic.seen = target.traffic.timestamp;
                target.traffic.valid = true;

                (void)Traffic_Add(&target.traffic);
            }
        }
    }

    static bool handleSetTxt(unsigned long messageId, char* params)
    {
        if (params == nullptr)
        {
            trackerReplyErrBadParams("TXT");
            return false;
        }

        char local[TRACKER_INPUT_BUFFER_SIZE + 1U];
        strncpy(local, params, sizeof(local) - 1U);
        local[sizeof(local) - 1U] = '\0';
        trimAscii(local);

        char* textPart = local;
        char* timePart = strrchr(local, '#');
        if (timePart != nullptr)
        {
            *timePart = '\0';
            ++timePart;
            trimAscii(timePart);
        }
        trimAscii(textPart);

        if (textPart[0] == '\0' || timePart == nullptr || *timePart == '\0')
        {
            trackerReplyErrBadParams("TXT");
            return false;
        }

        uint8_t msgHour = 0U;
        uint8_t msgMinute = 0U;
        if (!trackerParseClockText(timePart, msgHour, msgMinute))
        {
            trackerReplyErrBadParams("TXT");
            return false;
        }
        if (!trackerTextMessageFresh(msgHour, msgMinute))
        {
            trackerReplyIncorrectTime();
            return false;
        }

        if (utf8Length(textPart) > TRACKER_TEXT_MAX_CHARS)
        {
            trackerReplyTextTooLong();
            char clipped[TRACKER_INPUT_BUFFER_SIZE + 1U] = {};
            const unsigned char* src = reinterpret_cast<const unsigned char*>(textPart);
            size_t chars = 0U;
            size_t out = 0U;
            while (*src != 0U && chars < TRACKER_TEXT_MAX_CHARS && out < sizeof(clipped) - 1U)
            {
                size_t cpLen = 1U;
                if ((*src & 0x80U) == 0x00U) cpLen = 1U;
                else if ((*src & 0xE0U) == 0xC0U) cpLen = 2U;
                else if ((*src & 0xF0U) == 0xE0U) cpLen = 3U;
                else if ((*src & 0xF8U) == 0xF0U) cpLen = 4U;
                if (out + cpLen >= sizeof(clipped)) break;
                for (size_t i = 0; i < cpLen && src[i] != 0U; ++i)
                {
                    clipped[out++] = (char)src[i];
                }
                src += cpLen;
                ++chars;
            }
            clipped[out] = '\0';
            strncpy(textPart, clipped, sizeof(local) - 1U);
            textPart[sizeof(local) - 1U] = '\0';
        }

        trackerStoreTextMessage(messageId, timePart, textPart);
        const size_t acceptedChars = utf8Length(textPart);
        trackerReplyMessageReceipt(messageId, acceptedChars);
        return true;
    }

// - work: Параметр геометрии, координаты, размера или угла.
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
        if (trackerIsTestMode())
        {
            traffic.hour_msg = 10U;
            traffic.min_msg = 10U;
        }
        else
        {
            traffic.hour_msg = (uint8_t)hourMsg;
            traffic.min_msg = (uint8_t)minuteMsg;
        }
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

        trackerStartSimFlight(traffic);
            trackerReplyOk("FLY");
            return true;
        }

// - body: Параметр геометрии, координаты, размера или угла.
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

// - body: Параметр геометрии, координаты, размера или угла.
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
                     "%06lX,%d,%8s,%d,%d,%d,%d,%d,%.6f,%.6f,%u,%u,%u,%u",
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

// - versionBuf: Буфер, текстовая строка или рабочее сообщение.
    static void handleGetCommand(const char* cmd)
    {
        if (cmd == nullptr || *cmd == '\0')
        {
            trackerReplyUnknownCommand();
            return;
        }

        if (strcmp(cmd, "VER") == 0)
        {
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

            char answer[80] = {};
            snprintf(answer, sizeof(answer), "OK |%s", versionBuf);
            trackerWriteLine(answer);
            return;
        }
        if (strcmp(cmd, "FLY") == 0)
        {
            trackerReplyGetOk();
            sendThisAircraftToTracker();
            return;
        }
        if (strcmp(cmd, "BASE") == 0)
        {
            trackerReplyGetOk();
            sendBaseToTracker();
            return;
        }

        trackerReplyUnknownCommand();
    }

    static void handleSetCommand(unsigned long messageId, const char* cmd, char* params)
    {
        if (cmd == nullptr || *cmd == '\0')
        {
            trackerReplyUnknownCommand();
            return;
        }

        if (strcmp(cmd, "TXT") == 0)
        {
            handleSetTxt(messageId, params);
            return;
        }
        if (strcmp(cmd, "CLEARMAIL") == 0)
        {
            trackerClearActiveTextMessage(false);
            trackerReplyOk("CLEARMAIL");
            return;
        }
        if (strcmp(cmd, "FLY") == 0)
        {
            handleSetFly(params);
            return;
        }
        if (strcmp(cmd, "GSM") == 0)
        {
            // В точной структуре внешнего дисплея отдельного поля GSM/test нет.
            // Команду подтверждаем, чтобы не ломать обмен с трекером.
            if (params == nullptr || *params == '\0')
            {
                trackerReplyErrBadParams("GSM");
                return;
            }
            trackerReplyOk("GSM");
            return;
        }
        if (strcmp(cmd, "BTOK") == 0)
        {
            trackerClearActiveTextMessage(true);
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

        // CoreCommandBuffer from the original firmware treats the first field
        // as an opaque message identifier and never rejects a command because
        // of its value.  Keep that wire-compatible behaviour: GET/SET command
        // decoding must not depend on the sender's numbering scheme.
        char* messageIdEnd = nullptr;
        unsigned long messageId = strtoul(fields[0], &messageIdEnd, 10);
        if (messageIdEnd == fields[0])
        {
            messageId = 0UL;
        }

        char action[8] = {};
        char commandName[16] = {};
        strncpy(action, fields[1], sizeof(action) - 1U);
        strncpy(commandName, fields[2], sizeof(commandName) - 1U);
        strToUpperAscii(action);
        strToUpperAscii(commandName);

        if (strcmp(action, "GET") == 0)
        {
            handleGetCommand(commandName);
            return;
        }
        if (strcmp(action, "SET") == 0)
        {
            handleSetCommand(messageId, commandName, (fieldCount >= 4U) ? fields[3] : nullptr);
            return;
        }

        char textParams[TRACKER_INPUT_BUFFER_SIZE + 1U] = {};
        snprintf(textParams, sizeof(textParams), "%s#%s", fields[1], fields[2]);
        handleSetTxt(messageId, textParams);
        return;

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
    memset(g_simTargets, 0, sizeof(g_simTargets));
    trackerClearActiveTextMessage(false);
    g_rxLen = 0U;
    g_rxBuffer[0] = '\0';
    g_lastRxMs = millis();
    SERIAL_TRACKER.begin(SERIAL_TRACKER_SPEED, TRACKER_SERIAL_CONFIG, PIN_TRACKER_RX, PIN_TRACKER_TX, false, 256);
    SERIAL_TRACKER.enableIntTx(false);
}

void Tracker_loop()
{
    trackerServiceSimFlights();
    trackerAutoClearExpiredTextMessage();

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

bool Tracker_confirmActiveTextMessage()
{
    if (!g_hasActiveTextMessage || !g_allowReadConfirm)
    {
        return false;
    }

    const uint8_t messageIdToConfirm = g_activeTextMessageId;
    trackerReplyReadConfirm(messageIdToConfirm);
    trackerClearActiveTextMessage(true);
    return true;
}

bool Tracker_hasActiveTextMessage()
{
    return g_hasActiveTextMessage && g_activeTextMessage[0] != '\0';
}

const char* Tracker_getActiveTextMessage()
{
    return g_activeTextMessage;
}

void Tracker_fini()
{
    SERIAL_TRACKER.flush();
}
