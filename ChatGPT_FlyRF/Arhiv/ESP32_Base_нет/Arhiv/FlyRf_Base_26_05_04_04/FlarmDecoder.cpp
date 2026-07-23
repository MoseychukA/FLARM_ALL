/*
  Модуль FlarmDecoder.cpp
  Назначение:
  - Разбор сырых пакетов FLARM/LoRa и извлечение кандидата цели.

  Основные задачи модуля:
  - Проверять валидность полезной нагрузки.
  - Декодировать координаты, адрес и параметры движения цели.
  - Передавать корректно разобранные данные в общий механизм пополнения Container.
  - Накоплять диагностическую статистику успешных и отклоненных пакетов.
*/

#include "FlarmDecoder.h"
#include "TrafficDB.h"
#include <string.h>

static FlarmDecodeDiag g_diag = {};  // Логический флаг состояния: показывает, разрешена ли операция, активен ли режим или есть ли данные.

//------------------------------------------------------------------------------
// Назначение функции: Возвращает uniform pattern, рассчитанное или считанное по текущему состоянию модуля.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
static bool isUniformPattern(const uint8_t* data, size_t len, uint8_t value)
{
    for (size_t i = 0; i < len; ++i)
    {
        if (data[i] != value)
        {
            return false;
        }
    }
    return true;
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `clampf` и обрабатывает clampf в контексте модуля FlarmDecoder.cpp.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
static float clampf(float v, float minV, float maxV)
{
    if (v < minV) return minV;
    if (v > maxV) return maxV;
    return v;
}

//------------------------------------------------------------------------------
// Назначение функции: Проверяет candidate на корректность и возвращает результат проверки для общей логики проекта.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
static bool validateCandidate(const TrafficCandidate& candidate)
{
    if (!candidate.valid || candidate.address == 0 || candidate.address == 0xFFFFFFUL)
    {
        return false;
    }

    if (candidate.altitude < 0 || candidate.altitude > 60000)
    {
        return false;
    }

    if (candidate.lat < -90.0f || candidate.lat > 90.0f)
    {
        return false;
    }

    if (candidate.lon < -180.0f || candidate.lon > 180.0f)
    {
        return false;
    }

    return true;
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `FlarmDecoder_decodeCandidate` и обрабатывает flarm decoder candidate в контексте модуля FlarmDecoder.cpp.
// Локальные переменные: int16_t — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; uint16_t — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; uint8_t — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; lat — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта; lon — навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта.
//------------------------------------------------------------------------------
bool FlarmDecoder_decodeCandidate(const uint8_t* data, size_t len, int rssi, float snr, TrafficCandidate& outCandidate)
{
    memset(&outCandidate, 0, sizeof(outCandidate));

    if (data == nullptr || len < 10)
    {
        ++g_diag.rejectCount;
        ++g_diag.shortPacketCount;
        return false;
    }

    if (isUniformPattern(data, len, 0x00) || isUniformPattern(data, len, 0xFF))
    {
        ++g_diag.rejectCount;
        ++g_diag.emptyPatternCount;
        return false;
    }

    const uint32_t icao = ((uint32_t)data[0] << 16) |
                          ((uint32_t)data[1] << 8) |
                          ((uint32_t)data[2]);

    if (icao == 0 || icao == 0xFFFFFFUL)
    {
        ++g_diag.rejectCount;
        ++g_diag.invalidFieldCount;
        return false;
    }

    const int16_t latRaw = (int16_t)(((uint16_t)data[3] << 8) | data[4]);
    const int16_t lonRaw = (int16_t)(((uint16_t)data[5] << 8) | data[6]);
    const uint16_t altRaw = ((uint16_t)data[7] << 4) | ((data[8] >> 4) & 0x0F);
    const uint8_t speedRaw = (uint8_t)(((data[8] & 0x0F) << 4) | ((data[9] >> 4) & 0x0F));
    const uint16_t courseRaw = (uint16_t)((data[9] & 0x0F) * 36U);

    float lat = 50.0f + ((float)latRaw / 10000.0f);
    float lon = 8.0f + ((float)lonRaw / 10000.0f);

    if (lat < -90.0f || lat > 90.0f || lon < -180.0f || lon > 180.0f)
    {
        lat = 50.0f + clampf((float)latRaw / 10000.0f, -2.5f, 2.5f);
        lon = 8.0f + clampf((float)lonRaw / 10000.0f, -2.5f, 2.5f);
        ++g_diag.positionClampCount;
    }

    outCandidate.address = icao;
    outCandidate.lat = lat;
    outCandidate.lon = lon;
    outCandidate.altitude = (int)altRaw * 2;
    outCandidate.speed = clampf((float)speedRaw, 0.0f, 400.0f);
    outCandidate.course = (float)(courseRaw % 360U);
    outCandidate.timestampMs = millis();
    outCandidate.rssi = rssi;
    outCandidate.snr = snr;
    outCandidate.source = TRAFFIC_SOURCE_FLARM_LORA;
    outCandidate.valid = true;

    if (!validateCandidate(outCandidate))
    {
        ++g_diag.rejectCount;
        ++g_diag.invalidFieldCount;
        memset(&outCandidate, 0, sizeof(outCandidate));
        return false;
    }

    ++g_diag.candidateCount;
    return true;
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `FlarmDecoder_process` и обрабатывает flarm decoder в контексте модуля FlarmDecoder.cpp.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
bool FlarmDecoder_process(const uint8_t* data, size_t len, int rssi, float snr)
{
    TrafficCandidate candidate = {};  // Логический флаг состояния: показывает, разрешена ли операция, активен ли режим или есть ли данные.
    if (!FlarmDecoder_decodeCandidate(data, len, rssi, snr, candidate))
    {
        return false;
    }

    TrafficDB.updateFromCandidate(candidate);

    ++g_diag.okCount;
    g_diag.lastIcao = candidate.address;
    g_diag.lastLength = (uint8_t)len;
    g_diag.lastRssi = rssi;
    g_diag.lastSnr = snr;
    g_diag.lastDecodeMs = candidate.timestampMs;
    return true;
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `FlarmDecoder_decodeCount` и обрабатывает flarm decoder count в контексте модуля FlarmDecoder.cpp.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
uint32_t FlarmDecoder_decodeCount()
{
    return g_diag.okCount;
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `FlarmDecoder_rejectCount` и обрабатывает flarm decoder reject count в контексте модуля FlarmDecoder.cpp.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
uint32_t FlarmDecoder_rejectCount()
{
    return g_diag.rejectCount;
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `FlarmDecoder_getDiag` и обрабатывает flarm decoder diag в контексте модуля FlarmDecoder.cpp.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
bool FlarmDecoder_getDiag(FlarmDecodeDiag& outDiag)
{
    outDiag = g_diag;
    return (g_diag.okCount != 0U) || (g_diag.rejectCount != 0U) || (g_diag.candidateCount != 0U);
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `FlarmDecoder_resetStats` и обрабатывает flarm decoder stats в контексте модуля FlarmDecoder.cpp.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
void FlarmDecoder_resetStats()
{
    memset(&g_diag, 0, sizeof(g_diag));
}
