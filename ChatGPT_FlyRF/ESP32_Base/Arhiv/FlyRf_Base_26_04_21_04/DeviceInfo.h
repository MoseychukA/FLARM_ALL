/*
  Модуль DeviceInfo.h
  Назначение:
  - Общие определения, связанные с идентификатором устройства,
    режимами работы и состоянием нашего самолета.

  Что содержит файл:
  - Объявления функций получения Chip ID и версии программы.
  - Константы режимов работы устройства.
  - Структуру LocalAircraftState для параметров нашего самолета.
*/

#pragma once
#include <Arduino.h>
#include <time.h>

uint32_t DevID_Mapper(uint32_t id);
uint32_t getChipId();
void DeviceInfo_setProgramVersion(const String& version);
const String& DeviceInfo_programVersion();
String DeviceInfo_programVersionFromFile(const char* filePath);
String DeviceInfo_chipIdHex();

#define IMPUT_COORD_NONE   0U
#define IMPUT_COORD_MANUAL 1U
#define IMPUT_COORD_GNSS   2U

#define FLYRF_MODE_NORMAL      0U
#define FLYRF_MODE_TXRX_TEST1  1U
#define FLYRF_MODE_TXRX_TEST2  2U
#define FLYRF_MODE_TXRX_TEST3  3U
#define FLYRF_MODE_TXRX_TEST4  4U
#define FLYRF_MODE_TXRX_TEST_MAX FLYRF_MODE_TXRX_TEST4
#define FLYRF_MODE_TXRX_TEST5  5U /* obsolete, reserved for compatibility */

#define DEVICE_MODE_NORMAL FLYRF_MODE_NORMAL
#define DEVICE_MODE_TEST   FLYRF_MODE_TXRX_TEST1

#define TEST_MODE_STATIC    0U
#define TEST_MODE_RESERVED1 1U
#define TEST_MODE_RESERVED2 2U

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
