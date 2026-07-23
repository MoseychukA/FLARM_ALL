#pragma once
#include <Arduino.h>

uint32_t DevID_Mapper(uint32_t id);
uint32_t getChipId();
void DeviceInfo_setProgramVersion(const String& version);
const String& DeviceInfo_programVersion();
String DeviceInfo_programVersionFromFile(const char* filePath);
String DeviceInfo_chipIdHex();

#define FLYRF_MODE_NORMAL      0U
#define IMPUT_COORD_NONE   0U
#define IMPUT_COORD_MANUAL 1U

static inline bool FlyRfMode_usesLocalCoordinates(uint8_t mode)
{
    return mode != FLYRF_MODE_NORMAL;
}


struct LocalAircraftState
{
    uint32_t addr;
    int squawk;
    char callsign[8];
    time_t timestamp;
    float altitude;
    float pressure_altitude;
    float course;
    float speed;
    int vert_rate;
    float latitude;
    float longitude;
    float old_latitude;
    float old_longitude;
    float local_latitude;
    float local_longitude;
    float geoid_separation;
    uint16_t hdop;
    uint8_t aircraft_type;
    int16_t rp2040_gain;
};

extern LocalAircraftState ThisAircraft;
