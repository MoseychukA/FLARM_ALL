/*
  Модуль Mavlink.cpp
  Назначение:
  - Формирование и передача базового набора сообщений MAVLink v1.

  Основные задачи модуля:
  - Передавать HEARTBEAT, GPS_RAW_INT, GLOBAL_POSITION_INT и VFR_HUD для нашего самолета.
  - Передавать сторонние цели из Container как сообщения ADSB_VEHICLE.
  - Работать через каналы Serial / RS485 согласно настройкам WEB-интерфейса.
  - Кодировать и декодировать MAVLink-кадры для радиоканала LMIC LoRa.
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

/*
 * Отдельный физический RF-профиль MAVLink over LoRa.
 *
 * В отличие от временного варианта через OGNTP, здесь LMIC получает
 * собственный дескриптор протокола с LoRa-модуляцией, отдельным SyncWord
 * и собственными таймингами. Частотный план может совпадать по полосе с OGN,
 * но это уже не профиль OGNTP, а самостоятельный профиль радиоканала MAVLink.
 */
const rf_proto_desc_t mavlink_lora_proto_desc = {
    "MAVLORA",
    RF_PROTOCOL_MAVLINK,
    RF_MODULATION_TYPE_LORA,
    RF_PREAMBLE_TYPE_AA,
    1,
    {0x2D, 0, 0, 0, 0, 0, 0, 0},
    1,
    0x4D41564CUL,
    RF_PAYLOAD_DIRECT,
    MAVLINK_LORA_PAYLOAD_SIZE,
    0,
    RF_CHECKSUM_TYPE_NONE,
    0,
    RF_BITRATE_100KBPS,
    RF_FREQUENCY_DEVIATION_NONE,
    RF_WHITENING_NONE,
    RF_RX_BANDWIDTH_SS_125KHZ,
    120,
    RF_TIMING_INTERVAL,
    900,
    1700,
    {450, 850},
    {950, 1350}
};

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

    static inline float int1E7ToDeg(int32_t value)
    {
        return ((float)value) / 10000000.0f;
    }

    static inline int32_t metersToMillimeters(float value)
    {
        return (int32_t)lroundf(value * 1000.0f);
    }

    static inline float millimetersToMeters(int32_t value)
    {
        return ((float)value) / 1000.0f;
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

    static inline float cmpsToKmh(uint16_t value)
    {
        return (((float)value) / 100.0f) * 3.6f;
    }

    static inline int16_t mpsToCmpsSigned(float value)
    {
        float cmps = value * 100.0f;
        if (cmps < -32768.0f) cmps = -32768.0f;
        if (cmps > 32767.0f) cmps = 32767.0f;
        return (int16_t)lroundf(cmps);
    }

    static inline int16_t vertRateFpmToCmps(int value)
    {
        return mpsToCmpsSigned((float)value * 0.00508f);
    }

    static inline int vertRateCmpsToFpm(int16_t value)
    {
        return (int)lroundf((((float)value) / 100.0f) / 0.00508f);
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

    static inline float cdegToCourse(uint16_t value)
    {
        return ((float)value) / 100.0f;
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

    static uint16_t read_u16(const uint8_t* p)
    {
        return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
    }

    static int16_t read_i16(const uint8_t* p)
    {
        return (int16_t)read_u16(p);
    }

    static uint32_t read_u32(const uint8_t* p)
    {
        return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    }

    static int32_t read_i32(const uint8_t* p)
    {
        return (int32_t)read_u32(p);
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

    static bool buildFrame(uint8_t msgId, uint8_t crcExtra, const uint8_t* payload, uint8_t payloadLen,
                           uint8_t* outFrame, size_t outCapacity, size_t& outLen)
    {
        outLen = 0U;
        if (outFrame == nullptr)
        {
            return false;
        }

        const size_t totalLen = (size_t)payloadLen + 8U;
        if (outCapacity < totalLen)
        {
            return false;
        }

        outFrame[0] = MAVLINK_STX_V1;
        outFrame[1] = payloadLen;
        outFrame[2] = g_seq++;
        outFrame[3] = MAV_SYS_ID;
        outFrame[4] = MAV_COMP_ID;
        outFrame[5] = msgId;
        if (payloadLen > 0U && payload != nullptr)
        {
            memcpy(&outFrame[6], payload, payloadLen);
        }
        const uint16_t crc = mav_crc_calculate(&outFrame[1], 5U + payloadLen, crcExtra);
        outFrame[6U + payloadLen] = (uint8_t)(crc & 0xFFU);
        outFrame[7U + payloadLen] = (uint8_t)((crc >> 8) & 0xFFU);
        outLen = totalLen;
        return true;
    }

    static void sendFrame(uint8_t msgId, uint8_t crcExtra, const uint8_t* payload, uint8_t payloadLen)
    {
        if (!isSerialMavlinkEnabled() && !isRs485MavlinkEnabled())
        {
            return;
        }

        /*
         * Восстановленный путь вывода MAVLink как в варианте FlyRf_Base_26_04_21_15:
         * кадр собирается напрямую в локальном буфере и затем без промежуточных
         * преобразований отправляется в Serial и/или RS485.
         */
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

    static void writeCallsignField(const char* src, uint8_t* dst9)
    {
        if (dst9 == nullptr)
        {
            return;
        }
        memset(dst9, 0, 9U);
        char callsign[9] = {0};
        extractCallsign(src, callsign, sizeof(callsign));
        memcpy(dst9, callsign, sizeof(callsign));
    }

    static void readCallsignField(const uint8_t* src9, char* dst8)
    {
        if (src9 == nullptr || dst8 == nullptr)
        {
            return;
        }
        memset(dst8, ' ', 8U);
        for (uint8_t i = 0U; i < 8U; ++i)
        {
            const uint8_t ch = src9[i];
            if (ch == 0U)
            {
                break;
            }
            dst8[i] = (char)ch;
        }
    }

    static bool buildAdsbPayloadFromAircraft(const ufo_t& ac, uint8_t* payload, size_t payloadSize)
    {
        if (payload == nullptr || payloadSize < 38U)
        {
            return false;
        }
        if (ac.addr == 0U)
        {
            return false;
        }
        if (fabsf(ac.latitude) <= 0.00001f && fabsf(ac.longitude) <= 0.00001f)
        {
            return false;
        }

        memset(payload, 0, payloadSize);
        put_u32(&payload[0], ac.addr & 0xFFFFFFUL);
        put_i32(&payload[4], degToInt1E7(ac.latitude));
        put_i32(&payload[8], degToInt1E7(ac.longitude));
        put_i32(&payload[12], metersToMillimeters(ac.altitude));
        put_u16(&payload[16], courseToCdeg(ac.course));
        put_u16(&payload[18], kmhToCmps(ac.speed));
        put_i16(&payload[20], vertRateFpmToCmps(ac.vert_rate));
        writeCallsignField(ac.callsign, &payload[22]);
        payload[31] = ADSB_EMITTER_TYPE_NO_INFO;
        payload[32] = 1U;

        uint16_t flags = ADSB_FLAGS_VALID_COORDS | ADSB_FLAGS_VALID_ALTITUDE |
                         ADSB_FLAGS_VALID_HEADING | ADSB_FLAGS_VALID_VELOCITY;
        if (payload[22] != 0U)
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
        return true;
    }

    static bool buildOwnshipAdsbPayload(uint8_t* payload, size_t payloadSize)
    {
        if (payload == nullptr || payloadSize < 38U)
        {
            return false;
        }

        ufo_t own = {};  // Структура данных самолета или цели: хранит параметры борта, используемые при обмене и отображении.
        own.addr = ThisAircraft.addr;
        own.squawk = ThisAircraft.squawk;
        memcpy(own.callsign, ThisAircraft.callsign, sizeof(own.callsign));
        own.altitude = ThisAircraft.altitude;
        own.pressure_altitude = ThisAircraft.pressure_altitude;
        own.course = ThisAircraft.course;
        own.speed = ThisAircraft.speed;
        own.vert_rate = ThisAircraft.vert_rate;
        own.aircraft_type = ThisAircraft.aircraft_type;

        float lat = 0.0f;
        float lon = 0.0f;
        float altMeters = 0.0f;
        if (!resolveOwnshipPosition(lat, lon, altMeters))
        {
            return false;
        }

        own.latitude = lat;
        own.longitude = lon;
        own.altitude = altMeters;
        if (own.pressure_altitude == 0.0f)
        {
            own.pressure_altitude = altMeters;
        }
        if (own.addr == 0U)
        {
            return false;
        }

        return buildAdsbPayloadFromAircraft(own, payload, payloadSize);
    }

    static bool validateFrameAndGetPayload(const uint8_t* frame, uint8_t expectedMsgId,
                                           uint8_t expectedPayloadLen, uint8_t crcExtra,
                                           const uint8_t*& payload)
    {
        payload = nullptr;
        if (frame == nullptr)
        {
            return false;
        }
        if (frame[0] != MAVLINK_STX_V1 || frame[1] != expectedPayloadLen || frame[5] != expectedMsgId)
        {
            return false;
        }

        const uint16_t rxCrc = read_u16(&frame[6U + expectedPayloadLen]);
        const uint16_t calcCrc = mav_crc_calculate(&frame[1], 5U + expectedPayloadLen, crcExtra);
        if (rxCrc != calcCrc)
        {
            return false;
        }

        payload = &frame[6];
        return true;
    }

// - payload: Параметр геометрии, координаты, размера или угла.
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

// - payload: Параметр геометрии, координаты, размера или угла.
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

// - payload: Параметр геометрии, координаты, размера или угла.
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

// - payload: Параметр геометрии, координаты, размера или угла.
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

// - payload: Параметр геометрии, координаты, размера или угла.
    static void sendTrafficAdsb(const Aircraft& ac)
    {
        uint8_t payload[38] = {0};
        if (!buildAdsbPayloadFromAircraft(ac, payload, sizeof(payload)))
        {
            return;
        }
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

// - payload: Параметр геометрии, координаты, размера или угла.
size_t mavlink_lora_encode(void* pkt, ufo_t* this_aircraft)
{
    if (pkt == nullptr)
    {
        return 0U;
    }

    uint8_t payload[38] = {0};
    uint8_t frame[MAVLINK_LORA_PAYLOAD_SIZE] = {0};
    size_t outLen = 0U;

    bool payloadReady = false;
    if (this_aircraft != nullptr)
    {
        ufo_t own = *this_aircraft;  // Параметр радиоканала или протокола: описывает частоту, мощность, профиль, режим передачи или текущее состояние RF.
        if (own.addr != 0U && (fabsf(own.latitude) > 0.00001f || fabsf(own.longitude) > 0.00001f))
        {
            payloadReady = buildAdsbPayloadFromAircraft(own, payload, sizeof(payload));
        }
    }
    if (!payloadReady)
    {
        payloadReady = buildOwnshipAdsbPayload(payload, sizeof(payload));
    }
    if (!payloadReady)
    {
        return 0U;
    }

    if (!buildFrame(MAVLINK_MSG_ID_ADSB_VEHICLE, MAVLINK_CRC_ADSB_VEHICLE,
                    payload, sizeof(payload), frame, sizeof(frame), outLen))
    {
        return 0U;
    }

    memcpy(pkt, frame, outLen);
    return outLen;
}

bool mavlink_lora_decode(void* pkt, ufo_t* this_aircraft, ufo_t* fop)
{
    if (pkt == nullptr || fop == nullptr)
    {
        return false;
    }

    const uint8_t* payload = nullptr;
    if (!validateFrameAndGetPayload((const uint8_t*)pkt, MAVLINK_MSG_ID_ADSB_VEHICLE,
                                    38U, MAVLINK_CRC_ADSB_VEHICLE, payload))
    {
        return false;
    }

    memset(fop, 0, sizeof(*fop));
    fop->protocol = RF_PROTOCOL_MAVLINK;
    fop->addr = read_u32(&payload[0]) & 0x00FFFFFFUL;
    if (fop->addr == 0U)
    {
        return false;
    }
    if (this_aircraft != nullptr && fop->addr == this_aircraft->addr)
    {
        return false;
    }

    fop->addr_type = ADDR_TYPE_ICAO;
    fop->latitude = int1E7ToDeg(read_i32(&payload[4]));
    fop->longitude = int1E7ToDeg(read_i32(&payload[8]));
    fop->altitude = millimetersToMeters(read_i32(&payload[12]));
    fop->pressure_altitude = fop->altitude;
    fop->course = cdegToCourse(read_u16(&payload[16]));
    fop->speed = cmpsToKmh(read_u16(&payload[18]));
    fop->vert_rate = vertRateCmpsToFpm(read_i16(&payload[20]));
    readCallsignField(&payload[22], fop->callsign);
    fop->aircraft_type = AIRCRAFT_TYPE_UNKNOWN;
    fop->squawk = (int)read_u16(&payload[35]);
    fop->signal_source = TRAFFIC_SOURCE_FLARM_LORA;
    fop->source = TRAFFIC_SOURCE_FLARM_LORA;
    fop->timestamp = (time_t)((millis() / 1000UL));
    fop->timemsg = fop->timestamp;
    fop->seen = fop->timestamp;
    fop->valid = true;
    return true;
}
