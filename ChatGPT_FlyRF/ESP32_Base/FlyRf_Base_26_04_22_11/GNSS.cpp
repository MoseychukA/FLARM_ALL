/*
  Модуль GNSS.cpp
  Назначение:
  - Прием и разбор NMEA-данных от GNSS-приемника.

  Основные задачи модуля:
  - Инициализировать программный последовательный порт GNSS.
  - Принимать строки NMEA, разбирать координаты, время, высоту и число спутников.
  - Обновлять состояние нашего самолета ThisAircraft по данным навигации.
  - Отслеживать получение первого фикса, потерю данных и таймауты восстановления.
*/

#include "GNSS.h"

#include <SoftwareSerial.h>
#include <TimeLib.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "DeviceInfo.h"
#include "EEPROMRF.h"
#include "RS485Display.h"
#include "Baro.h"

namespace
{
    EspSoftwareSerial::UART Serial_GNSS;  // Объект внешнего интерфейса, экрана, порта или канала связи.
    constexpr auto GNSS_SERIAL_CONFIG = EspSoftwareSerial::SWSERIAL_8N1;  // Объект внешнего интерфейса, экрана, порта или канала связи.

    static char g_line[128] = {};  // Счетчик, индекс, позиция или номер элемента.
    static size_t g_lineLen = 0;  // Счетчик, индекс, позиция или номер элемента.

    static bool g_coordinatesValid = false;  // Логический флаг состояния, разрешения или наличия данных.
    static bool g_timeValid = false;  // Логический флаг состояния, разрешения или наличия данных.
    static bool g_altitudeValid = false;  // Логический флаг состояния, разрешения или наличия данных.
    static bool g_satellitesValid = false;  // Логический флаг состояния, разрешения или наличия данных.
    static uint32_t g_lastCoordMs = 0;  // Временная отметка, интервал или значение тайм-аута.
    static uint32_t g_lastTimeMs = 0;  // Временная отметка, интервал или значение тайм-аута.
    static uint32_t g_lastAltitudeMs = 0;  // Временная отметка, интервал или значение тайм-аута.
    static uint32_t g_lastSatMs = 0;  // Временная отметка, интервал или значение тайм-аута.
    static int g_lastDateYear = 0;  // Параметр геометрии, координаты, размера или угла.
    static int g_lastDateMonth = 0;  // Параметр геометрии, координаты, размера или угла.
    static int g_lastDateDay = 0;  // Параметр геометрии, координаты, размера или угла.
    static bool g_lastDateValid = false;  // Логический флаг состояния, разрешения или наличия данных.
    static float g_lastLatitude = 0.0f;  // Числовой параметр навигации, радиообмена, геометрии или измерения.
    static float g_lastLongitude = 0.0f;  // Числовой параметр навигации, радиообмена, геометрии или измерения.
    static float g_lastAltitude = 0.0f;  // Числовой параметр навигации, радиообмена, геометрии или измерения.
    static float g_lastSpeedKnots = 0.0f;  // Числовой параметр навигации, радиообмена, геометрии или измерения.
    static float g_lastCourseDeg = 0.0f;  // Числовой параметр навигации, радиообмена, геометрии или измерения.
    static float g_lastGeoidSeparation = 0.0f;  // Служебная переменная, используемая для промежуточных вычислений и логики модуля.
    static uint16_t g_lastHdop = 0;  // Параметр геометрии, координаты, размера или угла.
    static uint8_t g_lastSatellites = 0;  // Служебная переменная, используемая для промежуточных вычислений и логики модуля.
    static uint32_t g_bootStartMs = 0;  // Временная отметка, интервал или значение тайм-аута.
    static bool g_everHadCoordinates = false;  // Числовой параметр навигации, радиообмена, геометрии или измерения.

    constexpr uint32_t GNSS_COORDINATE_TIMEOUT_MS = 10000UL;  // Временная отметка, интервал или значение тайм-аута.
    constexpr uint32_t GNSS_TIME_TIMEOUT_MS = 10000UL;  // Временная отметка, интервал или значение тайм-аута.
    constexpr uint32_t GNSS_ALTITUDE_TIMEOUT_MS = 10000UL;  // Временная отметка, интервал или значение тайм-аута.
    constexpr uint32_t GNSS_SATELLITES_TIMEOUT_MS = 10000UL;  // Временная отметка, интервал или значение тайм-аута.
    constexpr uint32_t GNSS_INITIAL_WAIT_TIMEOUT_MS = 300000UL;  // Временная отметка, интервал или значение тайм-аута.
    constexpr uint32_t GNSS_RECOVERY_WAIT_TIMEOUT_MS = 10000UL;  // Временная отметка, интервал или значение тайм-аута.

//------------------------------------------------------------------------------
// Назначение функции: Возвращает текущее состояние, параметр или признак модуля.
// Локальные переменные: функция использует в основном параметры, глобальные объекты или выраженные локальные данные не требуются.
//------------------------------------------------------------------------------
    static bool isTestModeActive()
    {
        return (settings != nullptr && FlyRfMode_usesLocalCoordinates(settings->mode));
    }

//------------------------------------------------------------------------------
// Назначение функции: Выполняет отдельную законченную операцию внутри модуля и используется в общей логике проекта.
// Локальные переменные: функция использует в основном параметры, глобальные объекты или выраженные локальные данные не требуются.
//------------------------------------------------------------------------------
    static bool sentenceHasChecksum(const char* line)
    {
        return (line != nullptr && line[0] == '$' && strchr(line, '*') != nullptr);
    }

//------------------------------------------------------------------------------
// Назначение функции: Выполняет отдельную законченную операцию внутри модуля и используется в общей логике проекта.
// Локальные переменные: функция использует в основном параметры, глобальные объекты или выраженные локальные данные не требуются.
//------------------------------------------------------------------------------
    static bool checksumValid(const char* line)
    {
        if (!sentenceHasChecksum(line)) return false;

        const char* star = strchr(line, '*');
        if (star == nullptr || star[1] == '\0' || star[2] == '\0') return false;

        uint8_t sum = 0;  // Служебная переменная, используемая для промежуточных вычислений и логики модуля.
        for (const char* p = line + 1; p < star; ++p)
        {
            sum ^= (uint8_t)(*p);
        }

        char* endPtr = nullptr;  // Служебная переменная, используемая для промежуточных вычислений и логики модуля.
        const long rx = strtol(star + 1, &endPtr, 16);
        if (endPtr == star + 1) return false;

        return sum == (uint8_t)rx;
    }

//------------------------------------------------------------------------------
// Назначение функции: Выполняет отдельную законченную операцию внутри модуля и используется в общей логике проекта.
// Локальные переменные: функция использует в основном параметры, глобальные объекты или выраженные локальные данные не требуются.
//------------------------------------------------------------------------------
    static size_t splitCsv(char* s, char* fields[], size_t maxFields)
    {
        if (!s || !fields || maxFields == 0) return 0;

        size_t count = 0;  // Счетчик, индекс, позиция или номер элемента.
        fields[count++] = s;
        for (char* p = s; *p != '\0' && count < maxFields; ++p)
        {
            if (*p == ',')
            {
                *p = '\0';
                fields[count++] = p + 1;
            }
        }
        return count;
    }

//------------------------------------------------------------------------------
// Назначение функции: Возвращает текущее состояние, параметр или признак модуля.
// Локальные переменные: функция использует в основном параметры, глобальные объекты или выраженные локальные данные не требуются.
//------------------------------------------------------------------------------
    static bool isSentenceType(const char* header, const char* suffix)
    {
        if (!header || !suffix) return false;
        const size_t hLen = strlen(header);
        const size_t sLen = strlen(suffix);
        if (hLen < sLen) return false;
        return strcmp(header + (hLen - sLen), suffix) == 0;
    }

//------------------------------------------------------------------------------
// Назначение функции: Принимает и разбирает входные данные, преобразуя их во внутренний формат.
// Локальные переменные: функция использует в основном параметры, глобальные объекты или выраженные локальные данные не требуются.
//------------------------------------------------------------------------------
    static bool parseNmeaTime(const char* value, int& hh, int& mm, int& ss)
    {
        if (!value || strlen(value) < 6) return false;
        hh = (value[0] - '0') * 10 + (value[1] - '0');
        mm = (value[2] - '0') * 10 + (value[3] - '0');
        ss = (value[4] - '0') * 10 + (value[5] - '0');
        return (hh >= 0 && hh <= 23 && mm >= 0 && mm <= 59 && ss >= 0 && ss <= 60);
    }

//------------------------------------------------------------------------------
// Назначение функции: Принимает и разбирает входные данные, преобразуя их во внутренний формат.
// Локальные переменные: функция использует в основном параметры, глобальные объекты или выраженные локальные данные не требуются.
//------------------------------------------------------------------------------
    static bool parseNmeaDate(const char* value, int& day, int& month, int& year)
    {
        if (!value || strlen(value) < 6) return false;
        day = (value[0] - '0') * 10 + (value[1] - '0');
        month = (value[2] - '0') * 10 + (value[3] - '0');
        const int yy = (value[4] - '0') * 10 + (value[5] - '0');
        year = (yy >= 80) ? (1900 + yy) : (2000 + yy);
        return (day >= 1 && day <= 31 && month >= 1 && month <= 12);
    }

//------------------------------------------------------------------------------
// Назначение функции: Принимает и разбирает входные данные, преобразуя их во внутренний формат.
// Локальные переменные:
// - degBuf: Буфер, текстовая строка или рабочее сообщение.
//------------------------------------------------------------------------------
    static bool parseNmeaCoordinate(const char* value, char hemisphere, bool isLatitude, float& outValue)
    {
        if (!value || !*value) return false;

        char* endPtr = nullptr;  // Служебная переменная, используемая для промежуточных вычислений и логики модуля.
        strtod(value, &endPtr);
        if (endPtr == value) return false;

        const int degWidth = isLatitude ? 2 : 3;  // Параметр геометрии, координаты, размера или угла.
        if ((int)strlen(value) < degWidth + 2) return false;

        char degBuf[4] = {};  // Буфер, текстовая строка или рабочее сообщение.
        memcpy(degBuf, value, (size_t)degWidth);
        const int deg = atoi(degBuf);
        const double minutes = strtod(value + degWidth, nullptr);
        double decimal = (double)deg + minutes / 60.0;

        if (hemisphere == 'S' || hemisphere == 'W')
        {
            decimal = -decimal;
        }

        outValue = (float)decimal;
        return true;
    }

//------------------------------------------------------------------------------
// Назначение функции: Принимает и разбирает входные данные, преобразуя их во внутренний формат.
// Локальные переменные: функция использует в основном параметры, глобальные объекты или выраженные локальные данные не требуются.
//------------------------------------------------------------------------------
    static bool parseNmeaAltitude(const char* value, float& outValue)
    {
        if (!value || !*value) return false;
        char* endPtr = nullptr;  // Служебная переменная, используемая для промежуточных вычислений и логики модуля.
        const double alt = strtod(value, &endPtr);
        if (endPtr == value) return false;
        outValue = (float)alt;
        return true;
    }

//------------------------------------------------------------------------------
// Назначение функции: Принимает и разбирает входные данные, преобразуя их во внутренний формат.
// Локальные переменные: функция использует в основном параметры, глобальные объекты или выраженные локальные данные не требуются.
//------------------------------------------------------------------------------
    static bool parseNmeaFloat(const char* value, float& outValue)
    {
        if (!value || !*value) return false;
        char* endPtr = nullptr;  // Служебная переменная, используемая для промежуточных вычислений и логики модуля.
        const double v = strtod(value, &endPtr);
        if (endPtr == value) return false;
        outValue = (float)v;
        return true;
    }

//------------------------------------------------------------------------------
// Назначение функции: Выполняет отдельную законченную операцию внутри модуля и используется в общей логике проекта.
// Локальные переменные: функция использует в основном параметры, глобальные объекты или выраженные локальные данные не требуются.
//------------------------------------------------------------------------------
    static void refreshRs485Aux()
    {
        aux_t aux = {};  // Параметр геометрии, координаты, размера или угла.
        RS485Display_getOutgoingAux(&aux);
        aux.isValidGNSS_M = g_coordinatesValid;
        if (g_timeValid)
        {
            const time_t t = now();
            aux.Time_Hour_M = (uint8_t)hour(t);
            aux.Time_Minute_M = (uint8_t)minute(t);
        }
        else
        {
            aux.Time_Hour_M = 0;
            aux.Time_Minute_M = 0;
        }
        RS485Display_setOutgoingAux(&aux);
    }

//------------------------------------------------------------------------------
// Назначение функции: Обновляет состояние, применяет настройки или записывает новые значения.
// Локальные переменные: функция использует в основном параметры, глобальные объекты или выраженные локальные данные не требуются.
//------------------------------------------------------------------------------
    static void applyCoordinates(float latitude, float longitude)
    {
        g_lastLatitude = latitude;
        g_lastLongitude = longitude;
        g_coordinatesValid = true;
        g_lastCoordMs = millis();
        g_everHadCoordinates = true;
        GNSS_applyCurrentStateToThisAircraft();
    }

//------------------------------------------------------------------------------
// Назначение функции: Обновляет состояние, применяет настройки или записывает новые значения.
// Локальные переменные: функция использует в основном параметры, глобальные объекты или выраженные локальные данные не требуются.
//------------------------------------------------------------------------------
    static void applyAltitude(float altitudeMeters)
    {
        g_lastAltitude = altitudeMeters;
        g_altitudeValid = true;
        g_lastAltitudeMs = millis();
        GNSS_applyCurrentStateToThisAircraft();
    }

//------------------------------------------------------------------------------
// Назначение функции: Обновляет состояние, применяет настройки или записывает новые значения.
// Локальные переменные: функция использует в основном параметры, глобальные объекты или выраженные локальные данные не требуются.
//------------------------------------------------------------------------------
    static void applySatellites(uint8_t satellites)
    {
        g_lastSatellites = satellites;
        g_satellitesValid = true;
        g_lastSatMs = millis();
    }

//------------------------------------------------------------------------------
// Назначение функции: Обновляет состояние, применяет настройки или записывает новые значения.
// Локальные переменные: функция использует в основном параметры, глобальные объекты или выраженные локальные данные не требуются.
//------------------------------------------------------------------------------
    static void applyUtcDateTime(int year, int month, int day, int hourUtc, int minuteUtc, int secondUtc)
    {
        setTime(hourUtc, minuteUtc, secondUtc, day, month, year);
        g_lastDateYear = year;
        g_lastDateMonth = month;
        g_lastDateDay = day;
        g_lastDateValid = true;
        g_timeValid = true;
        g_lastTimeMs = millis();
    }

//------------------------------------------------------------------------------
// Назначение функции: Принимает и разбирает входные данные, преобразуя их во внутренний формат.
// Локальные переменные:
// - mm: Служебная переменная, используемая для промежуточных вычислений и логики модуля.
// - ss: Служебная переменная, используемая для промежуточных вычислений и логики модуля.
// - month: Параметр геометрии, координаты, размера или угла.
// - year: Параметр геометрии, координаты, размера или угла.
//------------------------------------------------------------------------------
    static void parseRmc(char* fields[], size_t count)
    {
        if (count < 10) return;
        if (!fields[2] || fields[2][0] != 'A') return;

        float lat = 0.0f;  // Числовой параметр навигации, радиообмена, геометрии или измерения.
        float lon = 0.0f;  // Числовой параметр навигации, радиообмена, геометрии или измерения.
        if (parseNmeaCoordinate(fields[3], fields[4] ? fields[4][0] : 'N', true, lat) &&
            parseNmeaCoordinate(fields[5], fields[6] ? fields[6][0] : 'E', false, lon))
        {
            applyCoordinates(lat, lon);
        }

        float speedKnots = 0.0f;  // Числовой параметр навигации, радиообмена, геометрии или измерения.
        if (count > 7 && parseNmeaFloat(fields[7], speedKnots))
        {
            g_lastSpeedKnots = speedKnots;
        }

        float courseDeg = 0.0f;  // Числовой параметр навигации, радиообмена, геометрии или измерения.
        if (count > 8 && parseNmeaFloat(fields[8], courseDeg))
        {
            g_lastCourseDeg = courseDeg;
        }

        int hh = 0, mm = 0, ss = 0;  // Параметр геометрии, координаты, размера или угла.
        int day = 0, month = 0, year = 0;  // Параметр геометрии, координаты, размера или угла.
        if (parseNmeaTime(fields[1], hh, mm, ss) && parseNmeaDate(fields[9], day, month, year))
        {
            applyUtcDateTime(year, month, day, hh, mm, ss);
        }
    }

//------------------------------------------------------------------------------
// Назначение функции: Принимает и разбирает входные данные, преобразуя их во внутренний формат.
// Локальные переменные:
// - mm: Служебная переменная, используемая для промежуточных вычислений и логики модуля.
// - ss: Служебная переменная, используемая для промежуточных вычислений и логики модуля.
//------------------------------------------------------------------------------
    static void parseGga(char* fields[], size_t count)
    {
        if (count < 10) return;
        const int fixQuality = (fields[6] && *fields[6]) ? atoi(fields[6]) : 0;
        const int satellites = (count > 7 && fields[7] && *fields[7]) ? atoi(fields[7]) : 0;
        applySatellites((uint8_t)((satellites < 0) ? 0 : satellites));

        if (count > 8)
        {
            const float hdop = (fields[8] && *fields[8]) ? (float)atof(fields[8]) : 0.0f;
            if (hdop > 0.0f)
            {
                g_lastHdop = (uint16_t)lroundf(hdop * 100.0f);
            }
        }

        if (fixQuality <= 0) return;

        float lat = 0.0f;  // Числовой параметр навигации, радиообмена, геометрии или измерения.
        float lon = 0.0f;  // Числовой параметр навигации, радиообмена, геометрии или измерения.
        if (parseNmeaCoordinate(fields[2], fields[3] ? fields[3][0] : 'N', true, lat) &&
            parseNmeaCoordinate(fields[4], fields[5] ? fields[5][0] : 'E', false, lon))
        {
            applyCoordinates(lat, lon);
        }

        float altitudeMeters = 0.0f;  // Числовой параметр навигации, радиообмена, геометрии или измерения.
        if (parseNmeaAltitude(fields[9], altitudeMeters))
        {
            applyAltitude(altitudeMeters);
        }

        if (count > 11)
        {
            float separationMeters = 0.0f;
            if (parseNmeaAltitude(fields[11], separationMeters))
            {
                g_lastGeoidSeparation = separationMeters;
            }
        }

        if (g_lastDateValid)
        {
            int hh = 0, mm = 0, ss = 0;
            if (parseNmeaTime(fields[1], hh, mm, ss))
            {
                applyUtcDateTime(g_lastDateYear, g_lastDateMonth, g_lastDateDay, hh, mm, ss);
            }
        }
    }

//------------------------------------------------------------------------------
// Назначение функции: Принимает и разбирает входные данные, преобразуя их во внутренний формат.
// Локальные переменные:
// - fields: Служебная переменная, используемая для промежуточных вычислений и логики модуля.
//------------------------------------------------------------------------------
    static void parseSentence(char* line)
    {
        if (!line || !*line) return;
        if (!checksumValid(line)) return;

        char* star = strchr(line, '*');
        if (star) *star = '\0';

        char* fields[20] = {};  // Служебная переменная, используемая для промежуточных вычислений и логики модуля.
        const size_t count = splitCsv(line, fields, 20);
        if (count == 0) return;

        if (isSentenceType(fields[0], "RMC"))
        {
            parseRmc(fields, count);
        }
        else if (isSentenceType(fields[0], "GGA"))
        {
            parseGga(fields, count);
        }
    }
}

//------------------------------------------------------------------------------
// Назначение функции: Инициализирует модуль, подготавливает ресурсы и стартовые параметры.
// Локальные переменные: функция использует в основном параметры, глобальные объекты или выраженные локальные данные не требуются.
//------------------------------------------------------------------------------
void GNSS_setup()
{
    g_bootStartMs = millis();
    Serial_GNSS.begin(GNSS_SERIAL_BAUD, GNSS_SERIAL_CONFIG, PIN_GNSS_RX, PIN_GNSS_TX, false, 256);
    Serial_GNSS.enableIntTx(false);

    Serial.printf("[SETUP] GNSS RX=%d TX=%d baud=%lu\r\n", PIN_GNSS_RX, PIN_GNSS_TX, (unsigned long)GNSS_SERIAL_BAUD);
    GNSS_applyCurrentStateToThisAircraft();
    refreshRs485Aux();
}

//------------------------------------------------------------------------------
// Назначение функции: Периодически обслуживает модуль в основном цикле проекта.
// Локальные переменные: функция использует в основном параметры, глобальные объекты или выраженные локальные данные не требуются.
//------------------------------------------------------------------------------
void GNSS_loop()
{
    while (Serial_GNSS.available() > 0)
    {
        const int ch = Serial_GNSS.read();
        if (ch < 0) break;

        if (ch == '\r')
        {
            continue;
        }

        if (ch == '\n')
        {
            if (g_lineLen > 0)
            {
                g_line[g_lineLen] = '\0';
                parseSentence(g_line);
                g_lineLen = 0;
                g_line[0] = '\0';
            }
            continue;
        }

        if (g_lineLen < (sizeof(g_line) - 1))
        {
            g_line[g_lineLen++] = (char)ch;
        }
        else
        {
            g_lineLen = 0;
            g_line[0] = '\0';
        }
    }

    const uint32_t nowMs = millis();
    bool stateChanged = false;  // Параметр геометрии, координаты, размера или угла.
    if (g_coordinatesValid && (uint32_t)(nowMs - g_lastCoordMs) > GNSS_COORDINATE_TIMEOUT_MS)
    {
        g_coordinatesValid = false;
        stateChanged = true;
    }
    if (g_timeValid && (uint32_t)(nowMs - g_lastTimeMs) > GNSS_TIME_TIMEOUT_MS)
    {
        g_timeValid = false;
        stateChanged = true;
    }
    if (g_altitudeValid && (uint32_t)(nowMs - g_lastAltitudeMs) > GNSS_ALTITUDE_TIMEOUT_MS)
    {
        g_altitudeValid = false;
        stateChanged = true;
    }
    if (g_satellitesValid && (uint32_t)(nowMs - g_lastSatMs) > GNSS_SATELLITES_TIMEOUT_MS)
    {
        g_satellitesValid = false;
        g_lastSatellites = 0;
    }
    if (stateChanged)
    {
        GNSS_applyCurrentStateToThisAircraft();
    }

    refreshRs485Aux();
}

//------------------------------------------------------------------------------
// Назначение функции: Завершает работу модуля и освобождает его ресурсы.
// Локальные переменные: функция использует в основном параметры, глобальные объекты или выраженные локальные данные не требуются.
//------------------------------------------------------------------------------
void GNSS_fini()
{
    Serial_GNSS.flush();
    Serial_GNSS.end();
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет отдельную законченную операцию внутри модуля и используется в общей логике проекта.
// Локальные переменные: функция использует в основном параметры, глобальные объекты или выраженные локальные данные не требуются.
//------------------------------------------------------------------------------
void GNSS_applyCurrentStateToThisAircraft()
{
    if (isTestModeActive())
    {
        if (settings != nullptr)
        {
            ThisAircraft.latitude = settings->local_latitude;
            ThisAircraft.longitude = settings->local_longitude;
            ThisAircraft.local_latitude = settings->local_latitude;
            ThisAircraft.local_longitude = settings->local_longitude;
            if (ThisAircraft.latitude != 0.0f || ThisAircraft.longitude != 0.0f)
            {
                ThisAircraft.old_latitude = ThisAircraft.latitude;
                ThisAircraft.old_longitude = ThisAircraft.longitude;
            }
            ThisAircraft.speed = 0.0f;
            ThisAircraft.course = 0.0f;
            ThisAircraft.hdop = 0;
            ThisAircraft.geoid_separation = 0.0f;
        }
    }
    else if (g_coordinatesValid)
    {
        ThisAircraft.latitude = g_lastLatitude;
        ThisAircraft.longitude = g_lastLongitude;
        ThisAircraft.local_latitude = g_lastLatitude;
        ThisAircraft.local_longitude = g_lastLongitude;
        ThisAircraft.speed = g_lastSpeedKnots;
        ThisAircraft.course = g_lastCourseDeg;
        ThisAircraft.hdop = g_lastHdop;
        ThisAircraft.geoid_separation = g_lastGeoidSeparation;
        if (ThisAircraft.latitude != 0.0f || ThisAircraft.longitude != 0.0f)
        {
            ThisAircraft.old_latitude = ThisAircraft.latitude;
            ThisAircraft.old_longitude = ThisAircraft.longitude;
        }
    }
    else if (ThisAircraft.old_latitude != 0.0f || ThisAircraft.old_longitude != 0.0f)
    {
        ThisAircraft.latitude = ThisAircraft.old_latitude;
        ThisAircraft.longitude = ThisAircraft.old_longitude;
        ThisAircraft.local_latitude = ThisAircraft.old_latitude;
        ThisAircraft.local_longitude = ThisAircraft.old_longitude;
    }
    else
    {
        ThisAircraft.latitude = 0.0f;
        ThisAircraft.longitude = 0.0f;
        ThisAircraft.local_latitude = 0.0f;
        ThisAircraft.local_longitude = 0.0f;
        ThisAircraft.speed = 0.0f;
        ThisAircraft.course = 0.0f;
        ThisAircraft.hdop = 0;
        ThisAircraft.geoid_separation = 0.0f;
    }

    if (g_altitudeValid)
    {
        ThisAircraft.altitude = g_lastAltitude;
    }
    else if (Baro_available())
    {
        ThisAircraft.altitude = Baro_altitudeMeters();
    }
    else
    {
        ThisAircraft.altitude = 0.0f;
    }

    if (Baro_available())
    {
        ThisAircraft.pressure_altitude = Baro_altitudeMeters();
    }
    else if (g_altitudeValid)
    {
        ThisAircraft.pressure_altitude = g_lastAltitude;
    }
    else
    {
        ThisAircraft.pressure_altitude = 0.0f;
    }
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет отдельную законченную операцию внутри модуля и используется в общей логике проекта.
// Локальные переменные: функция использует в основном параметры, глобальные объекты или выраженные локальные данные не требуются.
//------------------------------------------------------------------------------
bool GNSS_coordinatesValid()
{
    return g_coordinatesValid;
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет отдельную законченную операцию внутри модуля и используется в общей логике проекта.
// Локальные переменные: функция использует в основном параметры, глобальные объекты или выраженные локальные данные не требуются.
//------------------------------------------------------------------------------
bool GNSS_timeValid()
{
    return g_timeValid;
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет отдельную законченную операцию внутри модуля и используется в общей логике проекта.
// Локальные переменные: функция использует в основном параметры, глобальные объекты или выраженные локальные данные не требуются.
//------------------------------------------------------------------------------
bool GNSS_hasFix()
{
    return g_coordinatesValid;
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет отдельную законченную операцию внутри модуля и используется в общей логике проекта.
// Локальные переменные: функция использует в основном параметры, глобальные объекты или выраженные локальные данные не требуются.
//------------------------------------------------------------------------------
bool GNSS_altitudeValid()
{
    return g_altitudeValid;
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет отдельную законченную операцию внутри модуля и используется в общей логике проекта.
// Локальные переменные: функция использует в основном параметры, глобальные объекты или выраженные локальные данные не требуются.
//------------------------------------------------------------------------------
bool GNSS_satellitesValid()
{
    return g_satellitesValid;
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет отдельную законченную операцию внутри модуля и используется в общей логике проекта.
// Локальные переменные: функция использует в основном параметры, глобальные объекты или выраженные локальные данные не требуются.
//------------------------------------------------------------------------------
uint32_t GNSS_lastFixMs()
{
    return g_lastCoordMs;
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет отдельную законченную операцию внутри модуля и используется в общей логике проекта.
// Локальные переменные: функция использует в основном параметры, глобальные объекты или выраженные локальные данные не требуются.
//------------------------------------------------------------------------------
float GNSS_latitude()
{
    return g_lastLatitude;
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет отдельную законченную операцию внутри модуля и используется в общей логике проекта.
// Локальные переменные: функция использует в основном параметры, глобальные объекты или выраженные локальные данные не требуются.
//------------------------------------------------------------------------------
float GNSS_longitude()
{
    return g_lastLongitude;
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет отдельную законченную операцию внутри модуля и используется в общей логике проекта.
// Локальные переменные: функция использует в основном параметры, глобальные объекты или выраженные локальные данные не требуются.
//------------------------------------------------------------------------------
float GNSS_altitudeMeters()
{
    return g_lastAltitude;
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет отдельную законченную операцию внутри модуля и используется в общей логике проекта.
// Локальные переменные: функция использует в основном параметры, глобальные объекты или выраженные локальные данные не требуются.
//------------------------------------------------------------------------------
uint8_t GNSS_satellites()
{
    return g_lastSatellites;
}


//------------------------------------------------------------------------------
// Назначение функции: Выполняет отдельную законченную операцию внутри модуля и используется в общей логике проекта.
// Локальные переменные: функция использует в основном параметры, глобальные объекты или выраженные локальные данные не требуются.
//------------------------------------------------------------------------------
bool GNSS_waitingForInitialFix()
{
    if (isTestModeActive()) return false;
    if (g_coordinatesValid) return false;
    if (g_everHadCoordinates) return false;
    return (uint32_t)(millis() - g_bootStartMs) < GNSS_INITIAL_WAIT_TIMEOUT_MS;
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет отдельную законченную операцию внутри модуля и используется в общей логике проекта.
// Локальные переменные: функция использует в основном параметры, глобальные объекты или выраженные локальные данные не требуются.
//------------------------------------------------------------------------------
bool GNSS_waitingForRecovery()
{
    if (isTestModeActive()) return false;
    if (g_coordinatesValid) return false;
    if (!g_everHadCoordinates) return false;
    return (uint32_t)(millis() - g_lastCoordMs) < GNSS_RECOVERY_WAIT_TIMEOUT_MS;
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет отдельную законченную операцию внутри модуля и используется в общей логике проекта.
// Локальные переменные: функция использует в основном параметры, глобальные объекты или выраженные локальные данные не требуются.
//------------------------------------------------------------------------------
bool GNSS_noDataTimeout()
{
    if (isTestModeActive()) return false;
    if (g_coordinatesValid) return false;
    if (!g_everHadCoordinates)
    {
        return (uint32_t)(millis() - g_bootStartMs) >= GNSS_INITIAL_WAIT_TIMEOUT_MS;
    }
    return (uint32_t)(millis() - g_lastCoordMs) >= GNSS_RECOVERY_WAIT_TIMEOUT_MS;
}
