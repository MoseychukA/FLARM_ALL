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

//------------------------------------------------------------------------------
// Назначение функции: Выполняет отдельную законченную операцию внутри модуля и используется в общей логике проекта.
// Локальные переменные: функция использует в основном параметры, глобальные объекты или выраженные локальные данные не требуются.
//------------------------------------------------------------------------------
static inline bool FlyRfMode_usesLocalCoordinates(uint8_t mode)
{
    return mode != FLYRF_MODE_NORMAL;
}

struct LocalAircraftState
{
    uint32_t addr;  // Параметр конфигурации интерфейса, адресации или выбранного режима.
    int squawk;  // Параметр геометрии, координаты, размера или угла.
    char callsign[8];  // Служебная переменная, используемая для промежуточных вычислений и логики модуля.
    time_t timestamp;  // Временная отметка, интервал или значение тайм-аута.
    float altitude;  // Числовой параметр навигации, радиообмена, геометрии или измерения.
    float pressure_altitude;  // Числовой параметр навигации, радиообмена, геометрии или измерения.
    float course;  // Числовой параметр навигации, радиообмена, геометрии или измерения.
    float speed;  // Числовой параметр навигации, радиообмена, геометрии или измерения.
    int vert_rate;  // Служебная переменная, используемая для промежуточных вычислений и логики модуля.
    float latitude;  // Числовой параметр навигации, радиообмена, геометрии или измерения.
    float longitude;  // Числовой параметр навигации, радиообмена, геометрии или измерения.
    float old_latitude;  // Числовой параметр навигации, радиообмена, геометрии или измерения.
    float old_longitude;  // Числовой параметр навигации, радиообмена, геометрии или измерения.
    float local_latitude;  // Числовой параметр навигации, радиообмена, геометрии или измерения.
    float local_longitude;  // Числовой параметр навигации, радиообмена, геометрии или измерения.
    float geoid_separation;  // Служебная переменная, используемая для промежуточных вычислений и логики модуля.
    uint16_t hdop;  // Параметр геометрии, координаты, размера или угла.
    uint8_t aircraft_type;  // Параметр геометрии, координаты, размера или угла.
    int16_t rp2040_gain;  // Служебная переменная, используемая для промежуточных вычислений и логики модуля.
};

extern LocalAircraftState ThisAircraft;  // Параметр геометрии, координаты, размера или угла.
