/*
  Модуль Mavlink.cpp
  Назначение:
  - Формирование и передача базового набора сообщений MAVLink v1.

  Основные задачи модуля:
  - Передавать HEARTBEAT, GPS_RAW_INT, GLOBAL_POSITION_INT и VFR_HUD для нашего самолета.
  - Передавать сторонние цели из Container как сообщения ADSB_VEHICLE.
  - Работать через те же каналы Serial / RS485, которые уже используются в проекте.
*/

#include "Mavlink.h"
#include <Arduino.h>
#include <math.h>
#include <string.h>
#include "TrafficDB.h"
#include "DeviceInfo.h"
#include "EEPROMRF.h"
#include "GNSS.h"
#include "RS485Display.h"
#include "NMEA.h"

#ifndef DEG_TO_RAD
#define DEG_TO_RAD 0.017453292519943295769236907684886f
#endif

namespace
{
    constexpr uint8_t MAVLINK_STX_V1 = 0xFEU;
    constexpr uint8_t MAV_SYS_ID = 1U;
    constexpr uint8_t MAV_COMP_ID = 1U;
    constexpr uint32_t MAVLINK_PERIOD_MS = 1000UL;

    constexpr uint8_t MAVLINK_MSG_ID_HEARTBEAT = 0U;
    constexpr uint8_t MAVLINK_MSG_ID_GPS_RAW_INT = 24U;
    constexpr uint8_t MAVLINK_MSG_ID_GLOBAL_POSITION_INT = 33U;
    constexpr uint8_t MAVLINK_MSG_ID_VFR_HUD = 74U;
    constexpr uint8_t MAVLINK_MSG_ID_ADSB_VEHICLE = 246U;

    constexpr uint8_t MAVLINK_CRC_HEARTBEAT = 50U;
    constexpr uint8_t MAVLINK_CRC_GPS_RAW_INT = 24U;
    constexpr uint8_t MAVLINK_CRC_GLOBAL_POSITION_INT = 104U;
    constexpr uint8_t MAVLINK_CRC_VFR_HUD = 20U;
    constexpr uint8_t MAVLINK_CRC_ADSB_VEHICLE = 184U;

    constexpr uint8_t MAV_TYPE_FIXED_WING = 1U;
    constexpr uint8_t MAV_AUTOPILOT_INVALID = 8U;
    constexpr uint8_t MAV_MODE_FLAG_CUSTOM_MODE_ENABLED = 1U;
    constexpr uint8_t MAV_STATE_ACTIVE = 4U;
    constexpr uint8_t GPS_FIX_TYPE_NO_GPS = 0U;
    constexpr uint8_t GPS_FIX_TYPE_3D_FIX = 3U;
    constexpr uint8_t ADSB_EMITTER_TYPE_NO_INFO = 0U;
    constexpr uint16_t ADSB_FLAGS_VALID_COORDS = 1U << 0;
    constexpr uint16_t ADSB_FLAGS_VALID_ALTITUDE = 1U << 1;
    constexpr uint16_t ADSB_FLAGS_VALID_HEADING = 1U << 2;
    constexpr uint16_t ADSB_FLAGS_VALID_VELOCITY = 1U << 3;
    constexpr uint16_t ADSB_FLAGS_VALID_CALLSIGN = 1U << 4;
    constexpr uint16_t ADSB_FLAGS_VALID_SQUAWK = 1U << 5;

    static uint8_t g_seq = 0U;
    static uint32_t g_lastSendMs = 0U;

    static inline int32_t degToInt1E7(float value)
    {
        return (int32_t)lroundf(value * 10000000.0f);
    }

    static inline int32_t metersToMillimeters(float value)
    {
        return (int32_t)lroundf(value * 1000.0f);
    }

    static inline float kmhToMps(float value)
    {
        return value * (1000.0f / 3600.0f);
    }

    static inline uint16_t kmhToCmps(float value)
    {
        const float cmps = kmhToMps(value) * 100.0f;
        if (cmps <= 0.0f) return 0U;
        if (cmps >= 65535.0f) return 65535U;
        return (uint16_t)lroundf(cmps);
    }

    static inline int16_t mpsToCmpsSigned(float value)
    {
        float cmps = value * 100.0f;
        if (cmps < -32768.0f) cmps = -32768.0f;
        if (cmps > 32767.0f) cmps = 32767.0f;
        return (int16_t)lroundf(cmps);
    }

    static inline uint16_t courseToCdeg(float value)
    {
        while (value < 0.0f) value += 360.0f;
        while (value >= 360.0f) value -= 360.0f;
        return (uint16_t)lroundf(value * 100.0f);
    }

    static inline uint16_t courseToDegUInt(float value)
    {
        while (value < 0.0f) value += 360.0f;
        while (value >= 360.0f) value -= 360.0f;
        return (uint16_t)lroundf(value);
    }

    static uint16_t mav_crc_accumulate(uint8_t data, uint16_t crc)
    {
        uint8_t tmp = data ^ (uint8_t)(crc & 0xFFU);
        tmp ^= (uint8_t)(tmp << 4);
        return (uint16_t)((crc >> 8) ^ ((uint16_t)tmp << 8) ^ ((uint16_t)tmp << 3) ^ ((uint16_t)tmp >> 4));
    }

    static uint16_t mav_crc_calculate(const uint8_t* data, size_t len, uint8_t crcExtra)
    {
        uint16_t crc = 0xFFFFU;
        if (data != nullptr)
        {
            for (size_t i = 0; i < len; ++i)
            {
                crc = mav_crc_accumulate(data[i], crc);
            }
        }
        crc = mav_crc_accumulate(crcExtra, crc);
        return crc;
    }

    static void put_u16(uint8_t* p, uint16_t v)
    {
        p[0] = (uint8_t)(v & 0xFFU);
        p[1] = (uint8_t)((v >> 8) & 0xFFU);
    }

    static void put_i16(uint8_t* p, int16_t v)
    {
        put_u16(p, (uint16_t)v);
    }

    static void put_u32(uint8_t* p, uint32_t v)
    {
        p[0] = (uint8_t)(v & 0xFFU);
        p[1] = (uint8_t)((v >> 8) & 0xFFU);
        p[2] = (uint8_t)((v >> 16) & 0xFFU);
        p[3] = (uint8_t)((v >> 24) & 0xFFU);
    }

    static void put_i32(uint8_t* p, int32_t v)
    {
        put_u32(p, (uint32_t)v);
    }

    static void put_u64(uint8_t* p, uint64_t v)
    {
        for (uint8_t i = 0; i < 8U; ++i)
        {
            p[i] = (uint8_t)((v >> (8U * i)) & 0xFFU);
        }
    }

    static void put_float(uint8_t* p, float v)
    {
        uint32_t raw = 0U;
        memcpy(&raw, &v, sizeof(raw));
        put_u32(p, raw);
    }

    static bool isSerialMavlinkEnabled()
    {
        return (settings != nullptr && settings->serial_out == OUTPUT_MODE_MAVLINK);
    }

    static bool isRs485MavlinkEnabled()
    {
        return (settings != nullptr && settings->rs485_out == OUTPUT_MODE_MAVLINK);
    }

    static void rs485WriteMavlink(const uint8_t* data, size_t len)
    {
        if (!isRs485MavlinkEnabled() || data == nullptr || len == 0)
        {
            return;
        }

#if SOC_GPIO_PIN_RS485_DE >= 0
        digitalWrite(SOC_GPIO_PIN_RS485_DE, HIGH);
        delayMicroseconds(100);
#endif
        RS485_SERIAL.write(data, len);
        RS485_SERIAL.flush();
#if SOC_GPIO_PIN_RS485_DE >= 0
        delayMicroseconds(100);
        digitalWrite(SOC_GPIO_PIN_RS485_DE, LOW);
#endif
    }

    static void sendFrame(uint8_t msgId, uint8_t crcExtra, const uint8_t* payload, uint8_t payloadLen)
    {
        if (!isSerialMavlinkEnabled() && !isRs485MavlinkEnabled())
        {
            return;
        }

        uint8_t frame[8U + 255U + 2U] = {0};
        frame[0] = MAVLINK_STX_V1;
        frame[1] = payloadLen;
        frame[2] = g_seq++;
        frame[3] = MAV_SYS_ID;
        frame[4] = MAV_COMP_ID;
        frame[5] = msgId;
        if (payloadLen > 0U && payload != nullptr)
        {
            memcpy(&frame[6], payload, payloadLen);
        }
        const uint16_t crc = mav_crc_calculate(&frame[1], 5U + payloadLen, crcExtra);
        frame[6U + payloadLen] = (uint8_t)(crc & 0xFFU);
        frame[7U + payloadLen] = (uint8_t)((crc >> 8) & 0xFFU);
        const size_t totalLen = (size_t)payloadLen + 8U;

        if (isSerialMavlinkEnabled())
        {
            Serial.write(frame, totalLen);
        }
        if (isRs485MavlinkEnabled())
        {
            rs485WriteMavlink(frame, totalLen);
        }
    }

    static bool resolveOwnshipPosition(float& lat, float& lon, float& altMeters)
    {
        lat = ThisAircraft.latitude;
        lon = ThisAircraft.longitude;
        altMeters = ThisAircraft.altitude;

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

        return (fabsf(lat) > 0.00001f || fabsf(lon) > 0.00001f);
    }

    static void extractCallsign(const char* src, char* dst, size_t dstSize)
    {
        if (dst == nullptr || dstSize == 0U)
        {
            return;
        }
        memset(dst, 0, dstSize);
        if (src == nullptr)
        {
            return;
        }
        size_t j = 0U;
        for (size_t i = 0U; src[i] != 0 && j < (dstSize - 1U); ++i)
        {
            if (src[i] == ' ')
            {
                continue;
            }
            dst[j++] = src[i];
        }
        dst[j] = 0;
    }

    static void sendHeartbeat()
    {
        uint8_t payload[9] = {0};
        put_u32(&payload[0], 0U);
        payload[4] = MAV_TYPE_FIXED_WING;
        payload[5] = MAV_AUTOPILOT_INVALID;
        payload[6] = MAV_MODE_FLAG_CUSTOM_MODE_ENABLED;
        payload[7] = MAV_STATE_ACTIVE;
        payload[8] = 3U;
        sendFrame(MAVLINK_MSG_ID_HEARTBEAT, MAVLINK_CRC_HEARTBEAT, payload, sizeof(payload));
    }

    static void sendOwnshipGpsRaw()
    {
        float lat = 0.0f;
        float lon = 0.0f;
        float altMeters = 0.0f;
        const bool hasPos = resolveOwnshipPosition(lat, lon, altMeters);

        uint8_t payload[30] = {0};
        put_u64(&payload[0], (uint64_t)millis() * 1000ULL);
        put_i32(&payload[8], hasPos ? degToInt1E7(lat) : 0);
        put_i32(&payload[12], hasPos ? degToInt1E7(lon) : 0);
        put_i32(&payload[16], metersToMillimeters(altMeters));
        put_u16(&payload[20], 65535U);
        put_u16(&payload[22], 65535U);
        put_u16(&payload[24], kmhToCmps(ThisAircraft.speed));
        put_u16(&payload[26], courseToCdeg(ThisAircraft.course));
        payload[28] = hasPos ? GPS_FIX_TYPE_3D_FIX : GPS_FIX_TYPE_NO_GPS;
        payload[29] = GNSS_satellitesValid() ? GNSS_satellites() : 0U;
        sendFrame(MAVLINK_MSG_ID_GPS_RAW_INT, MAVLINK_CRC_GPS_RAW_INT, payload, sizeof(payload));
    }

    static void sendOwnshipGlobalPosition()
    {
        float lat = 0.0f;
        float lon = 0.0f;
        float altMeters = 0.0f;
        if (!resolveOwnshipPosition(lat, lon, altMeters))
        {
            return;
        }

        const float speedMps = kmhToMps(ThisAircraft.speed);
        const float courseRad = ThisAircraft.course * DEG_TO_RAD;
        const int16_t vx = (int16_t)lroundf(cosf(courseRad) * speedMps * 100.0f);
        const int16_t vy = (int16_t)lroundf(sinf(courseRad) * speedMps * 100.0f);
        const int16_t vz = (int16_t)lroundf(-(float)ThisAircraft.vert_rate * 0.00508f * 100.0f);

        uint8_t payload[28] = {0};
        put_u32(&payload[0], millis());
        put_i32(&payload[4], degToInt1E7(lat));
        put_i32(&payload[8], degToInt1E7(lon));
        put_i32(&payload[12], metersToMillimeters(altMeters));
        put_i32(&payload[16], metersToMillimeters(altMeters));
        put_i16(&payload[20], vx);
        put_i16(&payload[22], vy);
        put_i16(&payload[24], vz);
        put_u16(&payload[26], courseToCdeg(ThisAircraft.course));
        sendFrame(MAVLINK_MSG_ID_GLOBAL_POSITION_INT, MAVLINK_CRC_GLOBAL_POSITION_INT, payload, sizeof(payload));
    }

    static void sendOwnshipVfrHud()
    {
        uint8_t payload[20] = {0};
        put_float(&payload[0], kmhToMps(ThisAircraft.speed));
        put_float(&payload[4], kmhToMps(ThisAircraft.speed));
        put_i16(&payload[8], (int16_t)courseToDegUInt(ThisAircraft.course));
        put_u16(&payload[10], 0U);
        put_float(&payload[12], ThisAircraft.altitude);
        put_float(&payload[16], (float)ThisAircraft.vert_rate * 0.00508f);
        sendFrame(MAVLINK_MSG_ID_VFR_HUD, MAVLINK_CRC_VFR_HUD, payload, sizeof(payload));
    }

    static void sendTrafficAdsb(const Aircraft& ac)
    {
        if (!ac.valid || ac.addr == 0U)
        {
            return;
        }
        if (fabsf(ac.lat) <= 0.00001f && fabsf(ac.lon) <= 0.00001f)
        {
            return;
        }

        uint8_t payload[38] = {0};
        put_u32(&payload[0], ac.addr & 0xFFFFFFUL);
        put_i32(&payload[4], degToInt1E7(ac.lat));
        put_i32(&payload[8], degToInt1E7(ac.lon));
        put_i32(&payload[12], metersToMillimeters(ac.altitude));
        put_u16(&payload[16], courseToCdeg(ac.course));
        put_u16(&payload[18], kmhToCmps(ac.speed));
        put_i16(&payload[20], mpsToCmpsSigned((float)ac.vert_rate * 0.00508f));

        char callsign[9] = {0};
        extractCallsign(ac.callsign, callsign, sizeof(callsign));
        memcpy(&payload[22], callsign, sizeof(callsign));
        payload[31] = ADSB_EMITTER_TYPE_NO_INFO;
        payload[32] = 1U;

        uint16_t flags = ADSB_FLAGS_VALID_COORDS | ADSB_FLAGS_VALID_ALTITUDE |
                         ADSB_FLAGS_VALID_HEADING | ADSB_FLAGS_VALID_VELOCITY;
        if (callsign[0] != 0)
        {
            flags |= ADSB_FLAGS_VALID_CALLSIGN;
        }
        if (ac.squawk > 0)
        {
            flags |= ADSB_FLAGS_VALID_SQUAWK;
        }
        put_u16(&payload[33], flags);
        put_u16(&payload[35], (uint16_t)((ac.squawk > 0) ? ac.squawk : 0));
        payload[37] = 0U;

        sendFrame(MAVLINK_MSG_ID_ADSB_VEHICLE, MAVLINK_CRC_ADSB_VEHICLE, payload, sizeof(payload));
    }
}

void Mavlink_setup()
{
    g_seq = 0U;
    g_lastSendMs = 0U;
}

void Mavlink_loop()
{
    if (!isSerialMavlinkEnabled() && !isRs485MavlinkEnabled())
    {
        return;
    }

    const uint32_t nowMs = millis();
    if ((uint32_t)(nowMs - g_lastSendMs) < MAVLINK_PERIOD_MS)
    {
        return;
    }
    g_lastSendMs = nowMs;

    sendHeartbeat();
    sendOwnshipGpsRaw();
    sendOwnshipGlobalPosition();
    sendOwnshipVfrHud();

    const Aircraft* list = TrafficDB.getList();
    if (list == nullptr)
    {
        return;
    }

    for (int i = 0; i < MAX_AIRCRAFT; ++i)
    {
        sendTrafficAdsb(list[i]);
    }
}

void Mavlink_fini()
{
}
 