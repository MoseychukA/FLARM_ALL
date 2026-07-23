#include "FlarmDecoder.h"
#include "TrafficDB.h"
#include <string.h>

static FlarmDecodeDiag g_diag = {};

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

static float clampf(float v, float minV, float maxV)
{
    if (v < minV) return minV;
    if (v > maxV) return maxV;
    return v;
}

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

bool FlarmDecoder_process(const uint8_t* data, size_t len, int rssi, float snr)
{
    TrafficCandidate candidate = {};
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

uint32_t FlarmDecoder_decodeCount()
{
    return g_diag.okCount;
}

uint32_t FlarmDecoder_rejectCount()
{
    return g_diag.rejectCount;
}

bool FlarmDecoder_getDiag(FlarmDecodeDiag& outDiag)
{
    outDiag = g_diag;
    return (g_diag.okCount != 0U) || (g_diag.rejectCount != 0U) || (g_diag.candidateCount != 0U);
}

void FlarmDecoder_resetStats()
{
    memset(&g_diag, 0, sizeof(g_diag));
}
