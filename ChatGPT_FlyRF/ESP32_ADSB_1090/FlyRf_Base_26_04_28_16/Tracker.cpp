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
    constexpr uint8_t TRACKER_ADDR_TYPE_ICAO = 1U;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.

    EspSoftwareSerial::UART SERIAL_TRACKER;  // Объект интерфейса связи, через который выполняется прием, передача или обслуживание внешнего канала.
    constexpr auto TRACKER_SERIAL_CONFIG = EspSoftwareSerial::SWSERIAL_8N1;  // Объект интерфейса связи, через который выполняется прием, передача или обслуживание внешнего канала.
    static char g_rxBuffer[TRACKER_INPUT_BUFFER_SIZE + 1U] = {};  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
    static size_t g_rxLen = 0U;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
    static uint32_t g_lastRxMs = 0U;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
    constexpr uint32_t TRACKER_IDLE_FLUSH_MS = 30U;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
    constexpr uint16_t TRACKER_TEXT_MAX_CHARS = 172U;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
    constexpr uint16_t TRACKER_FLY_TTL_MINUTES = 20U;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
    constexpr uint16_t TRACKER_TEXT_TTL_MINUTES = 10U;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
    constexpr uint32_t TRACKER_TEXT_AUTO_CLEAR_MS = 10UL * 60UL * 1000UL;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
    constexpr uint32_t TRACKER_SIM_DURATION_MS = 60000UL;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
    constexpr uint32_t TRACKER_SIM_STEP_MS = 1000UL;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
    constexpr size_t TRACKER_SIM_MAX_TARGETS = MAX_TRACKING_OBJECTS;  // Навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта.
    constexpr double TRACKER_EARTH_RADIUS_METERS = 6371000.0;  // Навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта.

    struct TrackerSimTarget
    {
        bool active = false;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
        ufo_t traffic = {};  // Структура данных самолета или цели: хранит параметры борта, используемые при обмене и отображении.
        uint32_t startMs = 0U;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
        uint32_t lastUpdateMs = 0U;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
    };

    static TrackerSimTarget g_simTargets[TRACKER_SIM_MAX_TARGETS] = {};  // Навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта.
    static char g_activeTextMessage[BUFFER_SIZE] = {};  // Текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
    static bool g_hasActiveTextMessage = false;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
    static bool g_newTextMessageFlag = false;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
    static bool g_allowReadConfirm = false;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
    static bool g_messageReceivedFlag = false;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
    static uint8_t g_activeTextMessageId = 0U;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
    static uint32_t g_activeTextMessageStoredMs = 0U;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.


    static bool trackerIsTestMode();
    static uint16_t currentTrackerMinutesOfDay();
    static bool parseUnsignedInRange(const char* text, unsigned long minValue, unsigned long maxValue, unsigned long& outValue, int base);
    static void trackerAutoClearExpiredTextMessage();


//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `trackerWriteLine` и обрабатывает трекер line в контексте модуля Tracker.cpp.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
    static void trackerWriteLine(const char* text)
    {
        if (text == nullptr) return;
        SERIAL_TRACKER.print(text);
        SERIAL_TRACKER.print("\r\n");
    }

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `trackerWriteRawLine` и обрабатывает трекер raw line в контексте модуля Tracker.cpp.
// Локальные переменные: len — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `trackerReplyOk` и обрабатывает трекер reply ok в контексте модуля Tracker.cpp.
// Локальные переменные: buffer — текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
// - buffer: Буфер, текстовая строка или рабочее сообщение.
//------------------------------------------------------------------------------
    static void trackerReplyOk(const char* cmd)
    {
        char buffer[32];  // Текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
        if (cmd == nullptr || cmd[0] == '\0')
        {
            trackerWriteLine("OK");
            return;
        }
        snprintf(buffer, sizeof(buffer), "OK %s", cmd);
        trackerWriteLine(buffer);
    }

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `trackerReplyErrBadParams` и обрабатывает трекер reply err bad params в контексте модуля Tracker.cpp.
// Локальные переменные: buffer — текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
// - buffer: Буфер, текстовая строка или рабочее сообщение.
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `trackerReplyUnknownCommand` и обрабатывает трекер reply unknown command в контексте модуля Tracker.cpp.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
    static void trackerReplyUnknownCommand()
    {
        trackerWriteLine("UNKNOWN COMMAND");
    }

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `trackerReplyTextTooLong` и обрабатывает трекер reply текст too long в контексте модуля Tracker.cpp.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
    static void trackerReplyTextTooLong()
    {
        trackerWriteLine("The text is very long. !Maximum 172 characters");
    }

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `trackerSyncAuxToRs485` и обрабатывает трекер sync aux канал RS485 в контексте модуля Tracker.cpp.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
    static void trackerSyncAuxToRs485()
    {
        aux_t aux = {};  // Навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта.
        RS485Display_getOutgoingAux(&aux);
        aux.new_message = g_newTextMessageFlag;
        aux.message_received = g_messageReceivedFlag;
        aux.confirm_message_M = g_allowReadConfirm;
        memset(aux.msg_resp_M, 0, sizeof(aux.msg_resp_M));
        strncpy(aux.msg_resp_M, g_activeTextMessage, sizeof(aux.msg_resp_M) - 1U);
        RS485Display_setOutgoingAux(&aux);
    }

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `trackerClearActiveTextMessage` и обрабатывает трекер active текст текстовое сообщение в контексте модуля Tracker.cpp.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `trackerCurrentTime` и обрабатывает трекер время в контексте модуля Tracker.cpp.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `trackerParseClockText` и обрабатывает трекер clock текст в контексте модуля Tracker.cpp.
// Локальные переменные: colon — счетчик или индекс: указывает позицию элемента, номер строки, слота или текущую стадию перебора; hourLen — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; minLen — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
// - hourBuf: Временная отметка, интервал или значение тайм-аута.
// - minBuf: Буфер, текстовая строка или рабочее сообщение.
//------------------------------------------------------------------------------
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

        unsigned long hourVal = 0UL;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
        unsigned long minVal = 0UL;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
        if (!parseUnsignedInRange(hourBuf, 0UL, 23UL, hourVal, 10) ||
            !parseUnsignedInRange(minBuf, 0UL, 59UL, minVal, 10))
        {
            return false;
        }

        outHour = (uint8_t)hourVal;
        outMinute = (uint8_t)minVal;
        return true;
    }

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `trackerTextMessageFresh` и обрабатывает трекер текст текстовое сообщение fresh в контексте модуля Tracker.cpp.
// Локальные переменные: uint16_t — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `trackerReplyMessageReceipt` и обрабатывает трекер reply текстовое сообщение receipt в контексте модуля Tracker.cpp.
// Локальные переменные: buffer — текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст; countBuffer — текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
// - buffer: Буфер, текстовая строка или рабочее сообщение.
// - countBuffer: Счетчик, индекс, позиция или номер элемента.
//------------------------------------------------------------------------------
    static void trackerReplyMessageReceipt(unsigned long messageId, size_t acceptedChars)
    {
        char buffer[20];  // Текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
        snprintf(buffer, sizeof(buffer), "#%lu", messageId);
        trackerWriteLine(buffer);

        char countBuffer[20];  // Текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
        snprintf(countBuffer, sizeof(countBuffer), "%u", (unsigned)acceptedChars);
        trackerWriteLine(countBuffer);
    }

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `trackerReplyReadConfirm` и обрабатывает трекер reply подтверждение в контексте модуля Tracker.cpp.
// Локальные переменные: buffer — текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
// - buffer: Буфер, текстовая строка или рабочее сообщение.
//------------------------------------------------------------------------------
    static void trackerReplyReadConfirm(unsigned long messageId)
    {
        char buffer[48];  // Текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
        snprintf(buffer, sizeof(buffer), "#%lu#SET#BTOK", messageId);
        trackerWriteLine(buffer);
    }

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `trackerReplyIncorrectTime` и обрабатывает трекер reply incorrect время в контексте модуля Tracker.cpp.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
    static void trackerReplyIncorrectTime()
    {
        trackerWriteLine("Incorrect time! Please check the time");
    }

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `trackerStoreTextMessage` и обрабатывает трекер store текст текстовое сообщение в контексте модуля Tracker.cpp.
// Локальные переменные: prefixText — текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `trackerAutoClearExpiredTextMessage` и обрабатывает трекер автоматический режим expired текст текстовое сообщение в контексте модуля Tracker.cpp.
// Локальные переменные: uint32_t — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `trimAscii` и обрабатывает trim ascii в контексте модуля Tracker.cpp.
// Локальные переменные: len — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; start — временной параметр или отметка времени: используется для тайм-аутов, задержек, мигания или контроля давности данных.
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `strToUpperAscii` и обрабатывает str upper ascii в контексте модуля Tracker.cpp.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
    static void strToUpperAscii(char* text)
    {
        if (text == nullptr) return;
        for (; *text != '\0'; ++text)
        {
            *text = (char)toupper((unsigned char)*text);
        }
    }

//------------------------------------------------------------------------------
// Назначение функции: Разбирает unsigned in диапазон просмотра, выделяет из входной строки поля и преобразует их в рабочие значения проекта.
// Локальные переменные: endPtr — временной параметр или отметка времени: используется для тайм-аутов, задержек, мигания или контроля давности данных; base — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
//------------------------------------------------------------------------------
    static bool parseUnsignedInRange(const char* text, unsigned long minValue, unsigned long maxValue, unsigned long& outValue, int base = 10)
    {
        if (text == nullptr || *text == '\0') return false;

        char* endPtr = nullptr;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
        const unsigned long value = strtoul(text, &endPtr, base);
        if (endPtr == text || (endPtr != nullptr && *endPtr != '\0')) return false;
        if (value < minValue || value > maxValue) return false;
        outValue = value;
        return true;
    }

//------------------------------------------------------------------------------
// Назначение функции: Разбирает float strict, выделяет из входной строки поля и преобразует их в рабочие значения проекта.
// Локальные переменные: endPtr — временной параметр или отметка времени: используется для тайм-аутов, задержек, мигания или контроля давности данных; value — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
//------------------------------------------------------------------------------
    static bool parseFloatStrict(const char* text, float& outValue)
    {
        if (text == nullptr || *text == '\0') return false;
        char* endPtr = nullptr;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
        const float value = strtof(text, &endPtr);
        if (endPtr == text || (endPtr != nullptr && *endPtr != '\0')) return false;
        outValue = value;
        return true;
    }

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `utf8Length` и обрабатывает utf8 length в контексте модуля Tracker.cpp.
// Локальные переменные: count — счетчик или индекс: указывает позицию элемента, номер строки, слота или текущую стадию перебора; uint8_t — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
//------------------------------------------------------------------------------
    static size_t utf8Length(const char* text)
    {
        if (text == nullptr) return 0U;

        size_t count = 0U;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
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

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `nmeaChecksumBody` и обрабатывает nmea контрольную сумму body в контексте модуля Tracker.cpp.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
    static uint8_t nmeaChecksumBody(const char* body)
    {
        uint8_t checksum = 0U;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
        if (body == nullptr) return 0U;
        while (*body != '\0')
        {
            checksum ^= (uint8_t)(*body++);
        }
        return checksum;
    }

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `trackerWriteNmeaBody` и обрабатывает трекер nmea body в контексте модуля Tracker.cpp.
// Локальные переменные: line — текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст; uint8_t — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
// - line: Счетчик, индекс, позиция или номер элемента.
//------------------------------------------------------------------------------
    static void trackerWriteNmeaBody(const char* body)
    {
        if (body == nullptr || body[0] == '\0') return;
        char line[196];  // Текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
        const uint8_t cs = nmeaChecksumBody(body);
        snprintf(line, sizeof(line), "$%s*%02X\r\n", body, cs);
        SERIAL_TRACKER.print(line);
    }

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `trackerIsTestMode` и обрабатывает трекер test режим в контексте модуля Tracker.cpp.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
    static bool trackerIsTestMode()
    {
        return settings != nullptr && settings->mode != FLYRF_MODE_NORMAL;
    }

//------------------------------------------------------------------------------
// Назначение функции: Возвращает трекер minutes day, рассчитанное или считанное по текущему состоянию модуля.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `trackerMessageFresh` и обрабатывает трекер текстовое сообщение fresh в контексте модуля Tracker.cpp.
// Локальные переменные: uint16_t — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `fillCallsignTracker` и обрабатывает fill callsign трекер в контексте модуля Tracker.cpp.
// Локальные переменные: out — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
//------------------------------------------------------------------------------
    static void fillCallsignTracker(char* dst, size_t dstSize, const char* src)
    {
        if (dst == nullptr || dstSize == 0U) return;
        memset(dst, ' ', dstSize);
        if (src == nullptr) return;

        size_t out = 0U;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
        for (size_t i = 0U; src[i] != '\0' && out < dstSize; ++i)
        {
            const char c = src[i];
            if (c == ',' || c == '#' || c == '\r' || c == '\n') break;
            dst[out++] = c;
        }
    }

//------------------------------------------------------------------------------
// Назначение функции: Разбирает csv fields, выделяет из входной строки поля и преобразует их в рабочие значения проекта.
// Локальные переменные: count — счетчик или индекс: указывает позицию элемента, номер строки, слота или текущую стадию перебора; savePtr — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
//------------------------------------------------------------------------------
    static bool parseCsvFields(char* text, char* fields[], size_t expectedCount)
    {
        if (text == nullptr || fields == nullptr || expectedCount == 0U) return false;

        size_t count = 0U;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
        char* savePtr = nullptr;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
        for (char* token = strtok_r(text, ",", &savePtr); token != nullptr; token = strtok_r(nullptr, ",", &savePtr))
        {
            trimAscii(token);
            if (count >= expectedCount) return false;
            fields[count++] = token;
        }
        return count == expectedCount;
    }

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `trackerProjectCoordinate` и обрабатывает трекер project coordinate в контексте модуля Tracker.cpp.
// Локальные переменные: angularDistance — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; bearingRad — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; lat1 — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; lon1 — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; sinLat1 — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; cosLat1 — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта.
//------------------------------------------------------------------------------
    static void trackerProjectCoordinate(double startLatDeg, double startLonDeg, float bearingDeg, double distanceMeters,
                                         float& outLatDeg, float& outLonDeg)
    {
        if (distanceMeters == 0.0)
        {
            outLatDeg = (float)startLatDeg;
            outLonDeg = (float)startLonDeg;
            return;
        }

        const double angularDistance = distanceMeters / TRACKER_EARTH_RADIUS_METERS;  // Навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта.
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

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `trackerFindSimTargetByAddr` и обрабатывает трекер find sim цель адрес в контексте модуля Tracker.cpp.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `trackerAcquireSimTarget` и обрабатывает трекер acquire sim цель в контексте модуля Tracker.cpp.
// Локальные переменные: oldestIndex — счетчик или индекс: указывает позицию элемента, номер строки, слота или текущую стадию перебора.
//------------------------------------------------------------------------------
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

        size_t oldestIndex = 0U;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
        uint32_t oldestStart = g_simTargets[0].startMs;  // Временной параметр или отметка времени: используется для тайм-аутов, задержек, мигания или контроля давности данных.
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

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `trackerStartSimFlight` и обрабатывает трекер sim flight в контексте модуля Tracker.cpp.
// Локальные переменные: slot — счетчик или индекс: указывает позицию элемента, номер строки, слота или текущую стадию перебора; uint32_t — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `trackerServiceSimFlights` и обрабатывает трекер service sim flights в контексте модуля Tracker.cpp.
// Локальные переменные: uint32_t — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Назначение функции: Обрабатывает текст: принимает событие, выбирает нужную ветку логики и запускает действия модуля.
// Локальные переменные: local — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; textPart — текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст; timePart — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; acceptedChars — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
// - local: Служебная переменная, используемая для промежуточных вычислений и логики модуля.
// - clipped: Служебная переменная, используемая для промежуточных вычислений и логики модуля.
//------------------------------------------------------------------------------
    static bool handleSetTxt(unsigned long messageId, char* params)
    {
        if (params == nullptr)
        {
            trackerReplyErrBadParams("TXT");
            return false;
        }

        char local[TRACKER_INPUT_BUFFER_SIZE + 1U];  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
        strncpy(local, params, sizeof(local) - 1U);
        local[sizeof(local) - 1U] = '\0';
        trimAscii(local);

        char* textPart = local;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
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

        uint8_t msgHour = 0U;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
        uint8_t msgMinute = 0U;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
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

//------------------------------------------------------------------------------
// Назначение функции: Обрабатывает fly: принимает событие, выбирает нужную ветку логики и запускает действия модуля.
// Локальные переменные: work — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; altitude — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; pressureAltitude — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; speed — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; course — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; latitude — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта.
// - work: Параметр геометрии, координаты, размера или угла.
// - fields: Служебная переменная, используемая для промежуточных вычислений и логики модуля.
//------------------------------------------------------------------------------
    static bool handleSetFly(char* params)
    {
        if (params == nullptr || *params == '\0')
        {
            trackerReplyErrBadParams("FLY");
            return false;
        }

        char work[TRACKER_INPUT_BUFFER_SIZE + 1U];  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
        strncpy(work, params, sizeof(work) - 1U);
        work[sizeof(work) - 1U] = '\0';

        char* fields[13] = {};  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
        if (!parseCsvFields(work, fields, 13U))
        {
            trackerReplyErrBadParams("FLY");
            return false;
        }

        unsigned long addr = 0UL;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
        unsigned long squawk = 0UL;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
        unsigned long aircraftType = 0UL;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
        unsigned long hourMsg = 0UL;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
        unsigned long minuteMsg = 0UL;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
        float altitude = 0.0f;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
        float pressureAltitude = 0.0f;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
        float speed = 0.0f;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
        float course = 0.0f;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
        float latitude = 0.0f;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
        float longitude = 0.0f;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
        long vertRate = 0L;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.

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
        char* endVert = nullptr;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
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

        ufo_t traffic = EmptyFO;  // Навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта.
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

//------------------------------------------------------------------------------
// Назначение функции: Формирует и отправляет this самолет трекер через нужный интерфейс связи или вывода.
// Локальные переменные: body — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта.
// - body: Параметр геометрии, координаты, размера или угла.
//------------------------------------------------------------------------------
    static void sendThisAircraftToTracker()
    {
        char body[160];  // Навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта.
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

//------------------------------------------------------------------------------
// Назначение функции: Формирует и отправляет базу трекер через нужный интерфейс связи или вывода.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
// - callsign: Служебная переменная, используемая для промежуточных вычислений и логики модуля.
// - body: Параметр геометрии, координаты, размера или угла.
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Назначение функции: Обрабатывает command: принимает событие, выбирает нужную ветку логики и запускает действия модуля.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
// - versionBuf: Буфер, текстовая строка или рабочее сообщение.
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Назначение функции: Обрабатывает command: принимает событие, выбирает нужную ветку логики и запускает действия модуля.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
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
        if (strcmp(cmd, "FLY") == 0)
        {
            handleSetFly(params);
            return;
        }
        if (strcmp(cmd, "GSM") == 0)
        {
            // В точной структуре внешнего дисплея отдельного поля GSM/test нет.
            // Команду подтверждаем, чтобы не ломать обмен с трекером.
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

//------------------------------------------------------------------------------
// Назначение функции: Обрабатывает трекер текстовое сообщение: принимает событие, выбирает нужную ветку логики и запускает действия модуля.
// Локальные переменные: fieldCount — счетчик или индекс: указывает позицию элемента, номер строки, слота или текущую стадию перебора; p — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
// - fields: Служебная переменная, используемая для промежуточных вычислений и логики модуля.
//------------------------------------------------------------------------------
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

        char* fields[4] = {};  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
        size_t fieldCount = 0U;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
        char* p = message + 1;  // Временной параметр или отметка времени: используется для тайм-аутов, задержек, мигания или контроля давности данных.
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

        unsigned long messageId = 0UL;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
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
            handleSetCommand(messageId, fields[2], (fieldCount >= 4U) ? fields[3] : nullptr);
            return;
        }

        trackerReplyUnknownCommand();
    }

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `finalizeIncomingFrame` и обрабатывает finalize incoming кадр в контексте модуля Tracker.cpp.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
    static void finalizeIncomingFrame()
    {
        if (g_rxLen == 0U) return;
        g_rxBuffer[g_rxLen] = '\0';
        processTrackerMessage(g_rxBuffer);
        g_rxLen = 0U;
        g_rxBuffer[0] = '\0';
    }
}

//------------------------------------------------------------------------------
// Назначение функции: Инициализирует трекер, подготавливает связанные объекты и включает работу соответствующего узла.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Назначение функции: Обслуживает трекер в основном цикле: проверяет события, обновляет состояние и запускает нужные действия.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `Tracker_confirmActiveTextMessage` и обрабатывает трекер подтверждение active текст текстовое сообщение в контексте модуля Tracker.cpp.
// Локальные переменные: uint8_t — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
//------------------------------------------------------------------------------
bool Tracker_confirmActiveTextMessage()
{
    if (!g_hasActiveTextMessage || !g_allowReadConfirm)
    {
        return false;
    }

    const uint8_t messageIdToConfirm = g_activeTextMessageId;  // Логический флаг состояния: показывает, разрешена ли операция, активен ли режим или есть ли данные.
    trackerReplyReadConfirm(messageIdToConfirm);
    trackerClearActiveTextMessage(true);
    return true;
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `Tracker_hasActiveTextMessage` и обрабатывает трекер active текст текстовое сообщение в контексте модуля Tracker.cpp.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
bool Tracker_hasActiveTextMessage()
{
    return g_hasActiveTextMessage && g_activeTextMessage[0] != '\0';
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `Tracker_getActiveTextMessage` и обрабатывает трекер active текст текстовое сообщение в контексте модуля Tracker.cpp.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
const char* Tracker_getActiveTextMessage()
{
    return g_activeTextMessage;
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `Tracker_fini` и обрабатывает трекер fini в контексте модуля Tracker.cpp.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
void Tracker_fini()
{
    SERIAL_TRACKER.flush();
}
