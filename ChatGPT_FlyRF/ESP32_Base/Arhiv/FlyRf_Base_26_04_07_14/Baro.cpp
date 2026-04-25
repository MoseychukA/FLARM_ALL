#include "Baro.h"

#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include "RF.h"
#include "GNSS.h"

namespace
{
    constexpr uint8_t BMP180_ADDR = 0x77;
    constexpr uint8_t REG_CONTROL = 0xF4;
    constexpr uint8_t REG_RESULT = 0xF6;
    constexpr uint8_t CMD_TEMP = 0x2E;
    constexpr uint8_t OSS = 3;
    constexpr uint8_t CMD_PRESSURE = 0x34 + (OSS << 6);
    constexpr uint32_t BARO_INTERVAL_MS = 1000UL;
    constexpr uint32_t BARO_SEA_LEVEL_PRESSURE_PA = 101325UL;

    struct CalibrationData
    {
        int16_t AC1 = 0;
        int16_t AC2 = 0;
        int16_t AC3 = 0;
        uint16_t AC4 = 0;
        uint16_t AC5 = 0;
        uint16_t AC6 = 0;
        int16_t calB1 = 0;
        int16_t calB2 = 0;
        int16_t MB = 0;
        int16_t MC = 0;
        int16_t MD = 0;
    };

    CalibrationData g_cal = {};
    bool g_sensorReady = false;
    bool g_dataValid = false;
    uint32_t g_lastPollMs = 0;
    uint32_t g_lastUpdateMs = 0;
    float g_temperatureC = 0.0f;
    int32_t g_pressurePa = 0;
    float g_altitudeMeters = 0.0f;

    static bool write8(uint8_t reg, uint8_t value)
    {
        Wire.beginTransmission(BMP180_ADDR);
        Wire.write(reg);
        Wire.write(value);
        return Wire.endTransmission() == 0;
    }

    static bool readBytes(uint8_t reg, uint8_t* data, size_t len)
    {
        if (data == nullptr || len == 0) return false;

        Wire.beginTransmission(BMP180_ADDR);
        Wire.write(reg);
        if (Wire.endTransmission(false) != 0)
        {
            return false;
        }

        const size_t got = Wire.requestFrom((int)BMP180_ADDR, (int)len);
        if (got != len)
        {
            return false;
        }

        for (size_t i = 0; i < len; ++i)
        {
            if (!Wire.available()) return false;
            data[i] = (uint8_t)Wire.read();
        }
        return true;
    }

    static uint16_t readU16(uint8_t reg, bool& ok)
    {
        uint8_t buf[2] = {};
        ok = readBytes(reg, buf, sizeof(buf));
        return (uint16_t)((uint16_t(buf[0]) << 8) | uint16_t(buf[1]));
    }

    static int16_t readS16(uint8_t reg, bool& ok)
    {
        return (int16_t)readU16(reg, ok);
    }

    static bool loadCalibration()
    {
        bool ok = false;
        g_cal.AC1 = readS16(0xAA, ok); if (!ok) return false;
        g_cal.AC2 = readS16(0xAC, ok); if (!ok) return false;
        g_cal.AC3 = readS16(0xAE, ok); if (!ok) return false;
        g_cal.AC4 = readU16(0xB0, ok); if (!ok) return false;
        g_cal.AC5 = readU16(0xB2, ok); if (!ok) return false;
        g_cal.AC6 = readU16(0xB4, ok); if (!ok) return false;
        g_cal.calB1 = readS16(0xB6, ok); if (!ok) return false;
        g_cal.calB2 = readS16(0xB8, ok); if (!ok) return false;
        g_cal.MB  = readS16(0xBA, ok); if (!ok) return false;
        g_cal.MC  = readS16(0xBC, ok); if (!ok) return false;
        g_cal.MD  = readS16(0xBE, ok); if (!ok) return false;
        return true;
    }

    static bool readRawTemperature(int32_t& ut)
    {
        if (!write8(REG_CONTROL, CMD_TEMP)) return false;
        delay(5);

        uint8_t buf[2] = {};
        if (!readBytes(REG_RESULT, buf, sizeof(buf))) return false;
        ut = (int32_t)((uint16_t(buf[0]) << 8) | uint16_t(buf[1]));
        return true;
    }

    static bool readRawPressure(int32_t& up)
    {
        if (!write8(REG_CONTROL, CMD_PRESSURE)) return false;
        delay(26);

        uint8_t buf[3] = {};
        if (!readBytes(REG_RESULT, buf, sizeof(buf))) return false;
        up = ((((int32_t)buf[0] << 16) | ((int32_t)buf[1] << 8) | (int32_t)buf[2]) >> (8 - OSS));
        return true;
    }

    static bool measureBmp180(float& outTempC, int32_t& outPressurePa, float& outAltitudeMeters)
    {
        if (!g_sensorReady) return false;

        int32_t UT = 0;
        int32_t UP = 0;
        if (!readRawTemperature(UT)) return false;
        if (!readRawPressure(UP)) return false;

        const int32_t X1t = ((UT - (int32_t)g_cal.AC6) * (int32_t)g_cal.AC5) >> 15;
        const int32_t X2t = ((int32_t)g_cal.MC << 11) / (X1t + (int32_t)g_cal.MD);
        const int32_t B5 = X1t + X2t;
        const int32_t T = (B5 + 8) >> 4;

        const int32_t B6 = B5 - 4000;
        int32_t X1 = ((int32_t)g_cal.calB2 * ((B6 * B6) >> 12)) >> 11;
        int32_t X2 = ((int32_t)g_cal.AC2 * B6) >> 11;
        int32_t X3 = X1 + X2;
        const int32_t B3 = ((((int32_t)g_cal.AC1 * 4 + X3) << OSS) + 2) >> 2;

        X1 = ((int32_t)g_cal.AC3 * B6) >> 13;
        X2 = ((int32_t)g_cal.calB1 * ((B6 * B6) >> 12)) >> 16;
        X3 = ((X1 + X2) + 2) >> 2;
        const uint32_t B4 = ((uint32_t)g_cal.AC4 * (uint32_t)(X3 + 32768)) >> 15;
        const uint32_t B7 = ((uint32_t)(UP - B3)) * (uint32_t)(50000UL >> OSS);

        int32_t P = 0;
        if (B7 < 0x80000000UL)
        {
            P = (int32_t)((B7 << 1) / B4);
        }
        else
        {
            P = (int32_t)((B7 / B4) << 1);
        }

        X1 = ((P >> 8) * (P >> 8) * 3038L) >> 16;
        X2 = (-7357L * P) >> 16;
        P = P + ((X1 + X2 + 3791L) >> 4);

        outTempC = (float)T * 0.1f;
        outPressurePa = P;
        outAltitudeMeters = 44330.0f * (1.0f - powf((float)P / (float)BARO_SEA_LEVEL_PRESSURE_PA, 0.19029495f));
        return true;
    }
}

void Baro_setup()
{
    Wire.begin(GPIO_PIN_SDA, GPIO_PIN_SCL);
    Wire.setClock(100000UL);

    g_sensorReady = loadCalibration();
    g_dataValid = false;
    g_lastPollMs = 0;
    g_lastUpdateMs = 0;

    if (g_sensorReady)
    {
        Serial.println(F("[SETUP] BMP180 ready"));
    }
    else
    {
        Serial.println(F("[SETUP] BMP180 not detected"));
    }
}

void Baro_loop()
{
    if (!g_sensorReady) return;

    const uint32_t nowMs = millis();
    if ((uint32_t)(nowMs - g_lastPollMs) < BARO_INTERVAL_MS)
    {
        return;
    }
    g_lastPollMs = nowMs;

    float tempC = 0.0f;
    int32_t pressurePa = 0;
    float altitudeMeters = 0.0f;
    if (!measureBmp180(tempC, pressurePa, altitudeMeters))
    {
        g_dataValid = false;
        return;
    }

    g_temperatureC = tempC;
    g_pressurePa = pressurePa;
    g_altitudeMeters = altitudeMeters;
    g_lastUpdateMs = millis();
    g_dataValid = true;

    GNSS_applyCurrentStateToThisAircraft();
}

void Baro_fini()
{
    // Wire on ESP32 does not need explicit shutdown here.
}

bool Baro_available()
{
    return g_sensorReady && g_dataValid;
}

float Baro_temperatureC()
{
    return g_temperatureC;
}

int32_t Baro_pressurePa()
{
    return g_pressurePa;
}

float Baro_altitudeMeters()
{
    return g_altitudeMeters;
}

uint32_t Baro_lastUpdateMs()
{
    return g_lastUpdateMs;
}
