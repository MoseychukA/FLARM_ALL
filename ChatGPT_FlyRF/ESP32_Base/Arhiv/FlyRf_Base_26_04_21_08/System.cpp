/*
  Модуль System.cpp
  Назначение:
  - Главный координатор проекта, который собирает все подсистемы в единый цикл.

  Основные задачи модуля:
  - Выполнять последовательную инициализацию модулей при старте.
  - Обслуживать основной loop: GNSS, RF, WEB, дисплей, вывод, мосты и сервисные функции.
  - Поддерживать режимы теста, локальные координаты и общую логику времени/обновления.
*/

#include <Arduino.h>
#include <cstring>
#include <cmath>
#include <TimeLib.h>
#include "System.h"
#include "TrafficDB.h"
#include "ESP32RF.h"
#include "RF.h"
#include "Log.h"
#include "NMEA.h"
#include "RP2040Bridge.h"
#include "EEPROMRF.h"
#include "WiFiRF.h"
#include "WebRF.h"
#include "DeviceInfo.h"
#include "OTA.h"
#include "RS485Display.h"
#include "FlyRfSpi.h"
#include "GNSS.h"
#include "Baro.h"
#include "Bluetooth.h"
#include "Tracker.h"

namespace
{
    struct SimTrafficState
    {
        bool active;
        uint32_t addr;
        int squawk;
        char callsign[8];
        double latitude;
        double longitude;
        int16_t offsetSteps;
        int8_t direction;
        uint32_t lastStepMs;
        uint32_t startDelayMs;
    };

    static uint32_t g_lastOwnshipTxMs = 0;
    static uint8_t g_lastModeApplied = 0xFFU;
    static float g_lastModeBaseLat = 9999.0f;
    static float g_lastModeBaseLon = 9999.0f;
    static int16_t g_testFlightOffsetSteps = 0;
    static int8_t g_testFlightDirection = 1;
    static uint32_t g_lastTestFlightStepMs = 0;
    static SimTrafficState g_simTraffic[5] = {};
    static uint32_t g_testTrafficEpochMs = 0;
    static bool g_simTrafficWasActive = false;
    static uint16_t g_testDisplayCourseDeg = 0U;
    static uint32_t g_lastTestDisplayCourseStepMs = 0U;

    constexpr double kEarthRadiusMeters = 6371000.0;
    constexpr float kTestTrackOwnForwardCourseDeg = 70.0f;
    constexpr float kTestTrackStepMeters = 500.0f;
    constexpr int16_t kTestTrackLimitSteps = 40;
    constexpr float kTestTrackSpeedKnots = ((kTestTrackStepMeters / 1852.0f) * 3600.0f) * 0.5f;
    constexpr uint32_t kMotionStepPeriodMs = 2000UL;
    constexpr float kSimTrafficInitialCourseDeg = 40.0f;
    constexpr float kSimTrafficCourseStepDeg = 20.0f;
    constexpr uint32_t kSimTrafficAsyncDelayMs = 5000UL;
    constexpr uint32_t kTestDisplayCourseStepPeriodMs = 3000UL;
    constexpr uint16_t kTestDisplayCourseStepDeg = 5U;
    constexpr float kSimAltitudeMinMeters = 1000.0f;
    constexpr float kSimAltitudeMaxMeters = 8000.0f;
    constexpr float kTestOwnshipAltitudeMeters = 3000.0f;
    constexpr float kDefaultTestBaseLat = 50.0000f;
    constexpr float kDefaultTestBaseLon = 8.0000f;
    constexpr uint32_t kSimAltitudeHalfPeriodMs = 700000UL;
    constexpr uint32_t kSimAltitudeFullPeriodMs = kSimAltitudeHalfPeriodMs * 2UL;
    constexpr size_t kSimTrafficCount = sizeof(g_simTraffic) / sizeof(g_simTraffic[0]);
    constexpr size_t kSimAltitudeProfileCount = kSimTrafficCount + 1U;

    static const uint32_t kSimTrafficAddresses[kSimTrafficCount] = {
        0x0F1111UL, 0x0F2222UL, 0x0F3333UL, 0x0F4444UL, 0x0F5555UL
    };

    static const int kSimTrafficSquawks[kSimTrafficCount] = {
        1521, 1522, 1523, 1524, 1525
    };

    static const char* const kSimTrafficCallsigns[kSimTrafficCount] = {
        "AFL1111", "AFL2222", "AFL3333", "AFL4444", "AFL5555"
    };

    static double deg2rad(double value)
    {
        return value * 0.017453292519943295769236907684886;
    }

    static double rad2deg(double value)
    {
        return value * 57.295779513082320876798154814105;
    }

    static float normalizeCourse360(float value)
    {
        while (value >= 360.0f) value -= 360.0f;
        while (value < 0.0f) value += 360.0f;
        return value;
    }

    static void advanceDisplayCourseStep(uint32_t nowMs)
    {
        while ((uint32_t)(nowMs - g_lastTestDisplayCourseStepMs) >= kTestDisplayCourseStepPeriodMs)
        {
            g_lastTestDisplayCourseStepMs += kTestDisplayCourseStepPeriodMs;
            g_testDisplayCourseDeg = (uint16_t)((g_testDisplayCourseDeg + kTestDisplayCourseStepDeg) % 360U);
        }
    }

    static void fillCallsign(char* dst, size_t dstSize, const char* src)
    {
        if (dst == nullptr || dstSize == 0)
        {
            return;
        }

        memset(dst, ' ', dstSize);
        if (src == nullptr)
        {
            return;
        }

        const size_t srcLen = strlen(src);
        const size_t copyLen = (srcLen < dstSize) ? srcLen : dstSize;
        memcpy(dst, src, copyLen);
    }

    static void projectCoordinate(double startLatDeg, double startLonDeg, float bearingDeg, double distanceMeters,
                                  float& outLatDeg, float& outLonDeg)
    {
        if (distanceMeters == 0.0)
        {
            outLatDeg = (float)startLatDeg;
            outLonDeg = (float)startLonDeg;
            return;
        }

        const double angularDistance = distanceMeters / kEarthRadiusMeters;
        const double bearingRad = deg2rad((double)bearingDeg);
        const double lat1 = deg2rad(startLatDeg);
        const double lon1 = deg2rad(startLonDeg);
        const double sinLat1 = sin(lat1);
        const double cosLat1 = cos(lat1);
        const double sinAd = sin(angularDistance);
        const double cosAd = cos(angularDistance);

        const double lat2 = asin(sinLat1 * cosAd + cosLat1 * sinAd * cos(bearingRad));
        const double lon2 = lon1 + atan2(sin(bearingRad) * sinAd * cosLat1,
                                         cosAd - sinLat1 * sin(lat2));

        outLatDeg = (float)rad2deg(lat2);
        outLonDeg = (float)rad2deg(lon2);

        while (outLonDeg > 180.0f) outLonDeg -= 360.0f;
        while (outLonDeg < -180.0f) outLonDeg += 360.0f;
    }

    static time_t currentSystemSeconds()
    {
        const time_t t = now();
        return (t > 0) ? t : (time_t)(millis() / 1000UL);
    }

    static bool ownshipHasAnyCoordinates()
    {
        return (ThisAircraft.latitude != 0.0f || ThisAircraft.longitude != 0.0f ||
                ThisAircraft.old_latitude != 0.0f || ThisAircraft.old_longitude != 0.0f);
    }

    static void resolveTestBaseCoordinates(float& baseLat, float& baseLon)
    {
        if (fabsf(baseLat) > 0.00001f || fabsf(baseLon) > 0.00001f)
        {
            return;
        }

        baseLat = kDefaultTestBaseLat;
        baseLon = kDefaultTestBaseLon;
    }

    static void refreshOwnshipTimestamp()
    {
        ThisAircraft.timestamp = currentSystemSeconds();
    }

    static void applyOwnshipIdentity(uint8_t mode)
    {
        if (FlyRfMode_usesLocalCoordinates(mode))
        {
            ThisAircraft.squawk = 1110;
            fillCallsign(ThisAircraft.callsign, sizeof(ThisAircraft.callsign), "TEST001");
            ThisAircraft.aircraft_type = AIRCRAFT_TYPE_JET;
        }
        else
        {
            ThisAircraft.squawk = 1111;
            fillCallsign(ThisAircraft.callsign, sizeof(ThisAircraft.callsign), "FlyRF");
            ThisAircraft.aircraft_type = settings ? settings->aircraft_type : AIRCRAFT_TYPE_UNKNOWN;
        }
    }

    static bool advanceMotionStep(uint32_t nowMs, uint32_t& lastStepMs, int16_t& offsetSteps, int8_t& direction)
    {
        bool changed = false;
        while ((uint32_t)(nowMs - lastStepMs) >= kMotionStepPeriodMs)
        {
            lastStepMs += kMotionStepPeriodMs;
            offsetSteps = (int16_t)(offsetSteps + direction);
            if (offsetSteps >= kTestTrackLimitSteps)
            {
                offsetSteps = kTestTrackLimitSteps;
                direction = -1;
            }
            else if (offsetSteps <= -kTestTrackLimitSteps)
            {
                offsetSteps = -kTestTrackLimitSteps;
                direction = 1;
            }
            changed = true;
        }
        return changed;
    }

    static void computeTrackPosition(float baseLat, float baseLon, float forwardCourseDeg,
                                     int16_t offsetSteps, int8_t direction,
                                     float& outLat, float& outLon, float& outCourse, float& outSpeed)
    {
        const float reverseCourseDeg = normalizeCourse360(forwardCourseDeg + 180.0f);
        const float legBearing = (offsetSteps >= 0) ? forwardCourseDeg : reverseCourseDeg;
        const double distanceMeters = fabs((double)offsetSteps) * (double)kTestTrackStepMeters;
        projectCoordinate((double)baseLat, (double)baseLon, legBearing, distanceMeters, outLat, outLon);
        outCourse = (direction >= 0) ? forwardCourseDeg : reverseCourseDeg;
        outSpeed = kTestTrackSpeedKnots;
    }

    static void computeSimulatedAltitude(uint32_t nowMs, size_t profileIndex,
                                         float& outAltitude, float& outPressureAltitude,
                                         int& outVertRate, float& outVs)
    {
        const float altitudeRange = kSimAltitudeMaxMeters - kSimAltitudeMinMeters;
        if (altitudeRange <= 0.0f || kSimAltitudeHalfPeriodMs == 0UL || kSimAltitudeFullPeriodMs == 0UL)
        {
            outAltitude = kSimAltitudeMinMeters;
            outPressureAltitude = kSimAltitudeMinMeters;
            outVertRate = 0;
            outVs = 0.0f;
            return;
        }

        const size_t normalizedProfile = (kSimAltitudeProfileCount > 0U) ? (profileIndex % kSimAltitudeProfileCount) : 0U;
        const uint32_t phaseMs = (uint32_t)(((uint64_t)normalizedProfile * (uint64_t)kSimAltitudeFullPeriodMs) /
                                            (uint64_t)kSimAltitudeProfileCount);
        const uint32_t cycleMs = (nowMs + phaseMs) % kSimAltitudeFullPeriodMs;
        const bool climbing = cycleMs < kSimAltitudeHalfPeriodMs;
        const float fraction = climbing ? ((float)cycleMs / (float)kSimAltitudeHalfPeriodMs)
                                        : ((float)(cycleMs - kSimAltitudeHalfPeriodMs) / (float)kSimAltitudeHalfPeriodMs);

        outAltitude = climbing ? (kSimAltitudeMinMeters + altitudeRange * fraction)
                               : (kSimAltitudeMaxMeters - altitudeRange * fraction);
        outPressureAltitude = outAltitude;

        const float verticalSpeedMps = altitudeRange / ((float)kSimAltitudeHalfPeriodMs / 1000.0f);
        const float signedVerticalSpeedMps = climbing ? verticalSpeedMps : -verticalSpeedMps;
        outVs = signedVerticalSpeedMps * (_GPS_FEET_PER_METER * 60.0f);
        outVertRate = (int)lroundf(outVs);
    }

    static void clearSimulatedTrafficEntries()
    {
        for (size_t i = 0; i < kSimTrafficCount; ++i)
        {
            for (int j = 0; j < MAX_TRACKING_OBJECTS; ++j)
            {
                if (Container[j].addr == kSimTrafficAddresses[i])
                {
                    Container[j] = EmptyFO;
                }
            }
        }
    }

    static void resetTestFlightState(float baseLat, float baseLon, uint8_t mode)
    {
        g_lastModeApplied = mode;
        g_lastModeBaseLat = baseLat;
        g_lastModeBaseLon = baseLon;
        g_testFlightOffsetSteps = 0;
        g_testFlightDirection = 1;
        g_lastTestFlightStepMs = millis();
        g_testTrafficEpochMs = g_lastTestFlightStepMs;
        g_testDisplayCourseDeg = 0U;
        g_lastTestDisplayCourseStepMs = g_lastTestFlightStepMs;
        clearSimulatedTrafficEntries();

        for (size_t i = 0; i < kSimTrafficCount; ++i)
        {
            g_simTraffic[i].active = false;
            g_simTraffic[i].addr = kSimTrafficAddresses[i];
            g_simTraffic[i].squawk = kSimTrafficSquawks[i];
            fillCallsign(g_simTraffic[i].callsign, sizeof(g_simTraffic[i].callsign), kSimTrafficCallsigns[i]);
            g_simTraffic[i].latitude = baseLat;
            g_simTraffic[i].longitude = baseLon;
            g_simTraffic[i].offsetSteps = 1;
            g_simTraffic[i].direction = 1;
            g_simTraffic[i].lastStepMs = g_testTrafficEpochMs;
            g_simTraffic[i].startDelayMs = (mode == FLYRF_MODE_TXRX_TEST4) ? (uint32_t)(i * kSimTrafficAsyncDelayMs) : 0UL;
        }
    }

    static void serviceSimulatedTraffic(float baseLat, float baseLon, uint8_t mode)
    {
        const bool simulatedTrafficMode = (mode == FLYRF_MODE_TXRX_TEST3 || mode == FLYRF_MODE_TXRX_TEST4);
        if (!simulatedTrafficMode)
        {
            if (g_simTrafficWasActive)
            {
                clearSimulatedTrafficEntries();
                g_simTrafficWasActive = false;
            }
            return;
        }

        g_simTrafficWasActive = true;
        const uint32_t nowMs = millis();
        const time_t nowTs = (time_t)(nowMs / 1000UL);

        for (size_t i = 0; i < kSimTrafficCount; ++i)
        {
            SimTrafficState& state = g_simTraffic[i];
            if ((uint32_t)(nowMs - g_testTrafficEpochMs) < state.startDelayMs)
            {
                continue;
            }

            if (!state.active)
            {
                state.active = true;
                state.lastStepMs = g_testTrafficEpochMs + state.startDelayMs;
                state.offsetSteps = 1;
                state.direction = 1;
            }

            advanceMotionStep(nowMs, state.lastStepMs, state.offsetSteps, state.direction);

            float lat = baseLat;
            float lon = baseLon;
            float course = 0.0f;
            float speed = 0.0f;
            float altitude = kSimAltitudeMinMeters;
            float pressureAltitude = kSimAltitudeMinMeters;
            float vs = 0.0f;
            int vertRate = 0;
            const float forwardCourse = normalizeCourse360(kSimTrafficInitialCourseDeg + (float)i * kSimTrafficCourseStepDeg);
            computeTrackPosition(baseLat, baseLon, forwardCourse,
                                 state.offsetSteps, state.direction,
                                 lat, lon, course, speed);
            computeSimulatedAltitude(nowMs, i + 1U, altitude, pressureAltitude, vertRate, vs);
            state.latitude = lat;
            state.longitude = lon;

            ufo_t traffic = EmptyFO;
            traffic.addr = state.addr & 0x00FFFFFFUL;
            traffic.protocol = settings ? settings->rf_protocol : 0U;
            traffic.addr_type = ADDR_TYPE_ICAO;
            traffic.latitude = lat;
            traffic.longitude = lon;
            traffic.altitude = altitude;
            traffic.pressure_altitude = pressureAltitude;
            traffic.course = course;
            traffic.speed = speed;
            traffic.aircraft_type = AIRCRAFT_TYPE_JET;
            traffic.squawk = state.squawk;
            memcpy(traffic.callsign, state.callsign, sizeof(traffic.callsign));
            traffic.vert_rate = vertRate;
            traffic.vs = vs;
            traffic.timestamp = nowTs;
            traffic.timemsg = nowTs;
            traffic.seen = nowTs;
            traffic.rssi_LoRa = 0;
            traffic.rssi = 0;
            traffic.snr = 0.0f;
            traffic.signal_source = TRAFFIC_SOURCE_FLARM_LORA;
            traffic.source = TRAFFIC_SOURCE_FLARM_LORA;
            traffic.valid = true;

            Traffic_Update(&traffic);
            Traffic_Add(&traffic);
        }
    }

    static void applyLocalCoordinateMode()
    {
        if (settings == nullptr)
        {
            return;
        }

        const uint8_t mode = settings->mode;
        float baseLat = settings->local_latitude;
        float baseLon = settings->local_longitude;
        resolveTestBaseCoordinates(baseLat, baseLon);
        const bool needReset = (mode != g_lastModeApplied) ||
                               (fabsf(baseLat - g_lastModeBaseLat) > 0.000001f) ||
                               (fabsf(baseLon - g_lastModeBaseLon) > 0.000001f);
        if (needReset)
        {
            resetTestFlightState(baseLat, baseLon, mode);
        }

        applyOwnshipIdentity(mode);

        float outLat = baseLat;
        float outLon = baseLon;
        float outCourse = 0.0f;
        float outSpeed = 0.0f;
        float outAltitude = kSimAltitudeMinMeters;
        float outPressureAltitude = kSimAltitudeMinMeters;
        float outVs = 0.0f;
        int outVertRate = 0;
        const uint32_t nowMs = millis();

        if (mode == FLYRF_MODE_TXRX_TEST2)
        {
            advanceMotionStep(nowMs, g_lastTestFlightStepMs, g_testFlightOffsetSteps, g_testFlightDirection);
            computeTrackPosition(baseLat, baseLon, kTestTrackOwnForwardCourseDeg,
                                 g_testFlightOffsetSteps, g_testFlightDirection,
                                 outLat, outLon, outCourse, outSpeed);
        }

        // В тестовых режимах курс нашего самолета всегда равен 0,
        // независимо от направления имитации движения.
        outCourse = 0.0f;

        outAltitude = kTestOwnshipAltitudeMeters;
        outPressureAltitude = kTestOwnshipAltitudeMeters;
        outVs = 0.0f;
        outVertRate = 0;

        ThisAircraft.latitude = outLat;
        ThisAircraft.longitude = outLon;
        ThisAircraft.local_latitude = outLat;
        ThisAircraft.local_longitude = outLon;
        ThisAircraft.old_latitude = outLat;
        ThisAircraft.old_longitude = outLon;
        ThisAircraft.altitude = outAltitude;
        ThisAircraft.pressure_altitude = outPressureAltitude;
        ThisAircraft.vert_rate = outVertRate;
        ThisAircraft.course = outCourse;
        ThisAircraft.speed = outSpeed;
        ThisAircraft.hdop = 0;
        ThisAircraft.geoid_separation = 0.0f;

        serviceSimulatedTraffic(baseLat, baseLon, mode);
    }

    static void serviceOwnshipTransmit()
    {
        const uint32_t nowMs = millis();
        if ((uint32_t)(nowMs - g_lastOwnshipTxMs) < 1000UL)
        {
            return;
        }

        g_lastOwnshipTxMs = nowMs;
        refreshOwnshipTimestamp();

        if (settings != nullptr && !FlyRfMode_usesLocalCoordinates(settings->mode))
        {
            applyOwnshipIdentity(settings->mode);
            if (GNSS_coordinatesValid())
            {
                if (ThisAircraft.latitude != 0.0f || ThisAircraft.longitude != 0.0f)
                {
                    ThisAircraft.old_latitude = ThisAircraft.latitude;
                    ThisAircraft.old_longitude = ThisAircraft.longitude;
                }
                RF_TransmitThisAircraft(true);
                return;
            }

            if (ThisAircraft.old_latitude != 0.0f || ThisAircraft.old_longitude != 0.0f)
            {
                ThisAircraft.latitude = ThisAircraft.old_latitude;
                ThisAircraft.longitude = ThisAircraft.old_longitude;
                RF_TransmitThisAircraft(true);
            }
            return;
        }

        if (ownshipHasAnyCoordinates())
        {
            RF_TransmitThisAircraft(true);
        }
    }
}

float SystemDisplayCourseDeg()
{
    return normalizeCourse360(ThisAircraft.course);
}

void SystemSetup()
{
    Serial.println();
    Serial.println(F("Start setup"));
    Serial.print(F("[SETUP] Version: "));
    Serial.println(DeviceInfo_programVersion());

    Log_setup();
    Serial.println(F("[SETUP] Log ready"));

    TrafficDB.init();
    Serial.println(F("[SETUP] Container ready"));

    EEPROM_setup();
    Serial.println(F("[SETUP] EEPROM ready"));

    if (settings != nullptr)
    {
        ThisAircraft.addr = getChipId() & 0x00FFFFFFUL;
        ThisAircraft.squawk = 1111;
        memset(ThisAircraft.callsign, ' ', sizeof(ThisAircraft.callsign));
        memcpy(ThisAircraft.callsign, "FlyRF", 5);
        ThisAircraft.timestamp = 0;
        ThisAircraft.altitude = 0.0f;
        ThisAircraft.pressure_altitude = 0.0f;
        ThisAircraft.speed = 0.0f;
        ThisAircraft.course = 0.0f;
        ThisAircraft.vert_rate = 0;
        ThisAircraft.old_latitude = 0.0f;
        ThisAircraft.old_longitude = 0.0f;
        ThisAircraft.geoid_separation = 0.0f;
        ThisAircraft.hdop = 0;
        ThisAircraft.aircraft_type = settings->aircraft_type;
        applyOwnshipIdentity(settings->mode);
        if (FlyRfMode_usesLocalCoordinates(settings->mode))
        {
            ThisAircraft.latitude = settings->local_latitude;
            ThisAircraft.longitude = settings->local_longitude;
            ThisAircraft.local_latitude = settings->local_latitude;
            ThisAircraft.local_longitude = settings->local_longitude;
            settings->input_coordinates = IMPUT_COORD_MANUAL;
        }
        else
        {
            ThisAircraft.latitude = 0.0f;
            ThisAircraft.longitude = 0.0f;
            ThisAircraft.local_latitude = 0.0f;
            ThisAircraft.local_longitude = 0.0f;
            settings->input_coordinates = IMPUT_COORD_GNSS;
        }
        ThisAircraft.rp2040_gain = settings->threshold_level;
    }
    Serial.printf("[SETUP] Local coordinates: %.5f, %.5f\r\n", settings ? settings->local_latitude : 0.0f, settings ? settings->local_longitude : 0.0f);
    Serial.printf("[SETUP] RP2040 gain: %d\r\n", (int)ThisAircraft.rp2040_gain);

    FlyRfSpiSetup();

    GNSS_setup();
    Serial.println(F("[SETUP] GNSS setup done"));

    Baro_setup();
    Serial.println(F("[SETUP] Baro setup done"));

    Display_setup();
    Serial.println(F("[SETUP] Display ready"));

    const byte rfType = RF_setup();
    if (rfType != RF_IC_NONE)
    {
        Serial.println(F("[SETUP] LoRa setup done"));
    }
    else
    {
        Serial.println(F("[SETUP] LoRa module not detected"));
    }

    RP2040Bridge_setup();
    Serial.println(F("[SETUP] RP2040 setup done"));

    WiFi_setup();
    Serial.println(F("[SETUP] WiFi setup done"));

    Web_setup();
    Serial.println(F("[SETUP] Web setup done"));

    Bluetooth_setup();
    Serial.println(F("[SETUP] Bluetooth setup done"));

    RS485Display_setup();
    Serial.println(F("[SETUP] RS485 setup done"));

    NMEA_setup();
    Serial.println(F("[SETUP] NMEA setup done"));

    Tracker_setup();
    Serial.println(F("[SETUP] Tracker setup done"));

    OTA_setup();
    Serial.println(F("[SETUP] OTA setup done"));

    Serial.println(F("Setup End"));
}

void SystemLoop()
{
    static uint32_t lastDisplayLoopMs = 0;

    GNSS_loop();
    Baro_loop();
    if (settings != nullptr && FlyRfMode_usesLocalCoordinates(settings->mode))
    {
        applyLocalCoordinateMode();
    }
    else
    {
        g_lastModeApplied = 0xFFU;
        applyOwnshipIdentity(FLYRF_MODE_NORMAL);
        serviceSimulatedTraffic(settings ? settings->local_latitude : 0.0f,
                                settings ? settings->local_longitude : 0.0f,
                                FLYRF_MODE_NORMAL);
    }
    serviceOwnshipTransmit();
    TrafficDB.removeStale();
    RF_loop();
    ParseData();
    RP2040Bridge_loop();
    WiFi_loop();
    Web_loop();
    Tracker_loop();
    RS485Display_loop();
    NMEA_loop();
    Bluetooth_loop();
    OTA_loop();

    const uint32_t nowMs = millis();
    if ((uint32_t)(nowMs - lastDisplayLoopMs) >= 300UL)
    {
        lastDisplayLoopMs = nowMs;
        Display_loop();
    }

    delay(1);
}
