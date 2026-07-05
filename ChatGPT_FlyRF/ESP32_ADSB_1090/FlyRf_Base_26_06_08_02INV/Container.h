/*
  Модуль Container.h
  Назначение:
  - Описание структуры базы сторонних самолетов и ее интерфейса.

  Что содержит файл:
  - Описание структуры ufo_t и сетевых представлений данных.
  - Глобальные объекты ThisAircraft/Container/EmptyFO, используемые по проекту.
  - Константы размеров базы и таймаутов устаревания записей.
  - Объявления функций добавления, обновления и очистки базы.
*/

#pragma once
#include <Arduino.h>
#include <time.h>
#include "TrafficTypes.h"
//#include "RF.h"

// Общие константы и перечисления, которые используются по проекту
// для описания типа адреса, типа воздушного судна и пересчета единиц.
#ifndef _GPS_MPH_PER_KNOT
#define _GPS_MPH_PER_KNOT 1.15077945
#endif
#ifndef _GPS_MPS_PER_KNOT
#define _GPS_MPS_PER_KNOT 0.51444444
#endif
#ifndef _GPS_KMPH_PER_KNOT
#define _GPS_KMPH_PER_KNOT 1.852
#endif
#ifndef _GPS_MILES_PER_METER
#define _GPS_MILES_PER_METER 0.00062137112
#endif
#ifndef _GPS_KM_PER_METER
#define _GPS_KM_PER_METER 0.001
#endif
#ifndef _GPS_FEET_PER_METER
#define _GPS_FEET_PER_METER 3.2808399
#endif

enum
{
    ADDR_TYPE_RANDOM,
    ADDR_TYPE_ICAO,
    ADDR_TYPE_FLARM,
    ADDR_TYPE_ANONYMOUS,
    ADDR_TYPE_P3I,
    ADDR_TYPE_FANET
};

enum
{
    AIRCRAFT_TYPE_UNKNOWN,
    AIRCRAFT_TYPE_GLIDER,
    AIRCRAFT_TYPE_TOWPLANE,
    AIRCRAFT_TYPE_HELICOPTER,
    AIRCRAFT_TYPE_PARACHUTE,
    AIRCRAFT_TYPE_DROPPLANE,
    AIRCRAFT_TYPE_HANGGLIDER,
    AIRCRAFT_TYPE_PARAGLIDER,
    AIRCRAFT_TYPE_POWERED,
    AIRCRAFT_TYPE_JET,
    AIRCRAFT_TYPE_UFO,
    AIRCRAFT_TYPE_BALLOON,
    AIRCRAFT_TYPE_ZEPPELIN,
    AIRCRAFT_TYPE_UAV,
    AIRCRAFT_TYPE_RESERVED,
    AIRCRAFT_TYPE_STATIC
};

enum
{
    ALARM_LEVEL_NONE,
    ALARM_LEVEL_LOW,
    ALARM_LEVEL_IMPORTANT,
    ALARM_LEVEL_URGENT
};

#define MAX_TRACKING_OBJECTS 12
#define MAX_AIRCRAFT MAX_TRACKING_OBJECTS
#define ENTRY_EXPIRATION_TIME 25UL
#define TRAFFIC_VECTOR_UPDATE_INTERVAL 2UL

#ifndef CONTAINER_STALE_TIMEOUT_MS
#define CONTAINER_STALE_TIMEOUT_MS (ENTRY_EXPIRATION_TIME * 1000UL)
#endif

typedef struct UFO {
    uint8_t   raw[34];
    union { time_t timestamp; uint32_t lastUpdate; };
    uint8_t   protocol;
    union { uint32_t addr; uint32_t icao; };
    uint8_t   addr_type;
    union { float latitude; float lat; };
    union { float longitude; float lon; };
    float     old_latitude;
    float     old_longitude;
    float     altitude;
    float     pressure_altitude;
    float     course;
    float     speed;
    uint8_t   aircraft_type;
    char      callsign[8];
    int       vert_rate;
    int       squawk;
    time_t    timemsg;
    float     vs;
    bool      stealth;
    bool      no_track;
    float     geoid_separation;
    uint16_t  hdop;
    int8_t    rssi_LoRa;  // Параметр радиоканала или протокола: описывает частоту, мощность, профиль, режим передачи или текущее состояние RF.
    int       rssi;  // Параметр радиоканала или протокола: описывает частоту, мощность, профиль, режим передачи или текущее состояние RF.
    float     distance;
    float     bearing;
    int8_t    alarm_level;
    union { uint8_t signal_source; TrafficSource source; };
    time_t    seen;
    uint8_t   hour_msg;
    uint8_t   min_msg;
    uint16_t  delay_time_msg;
    int8_t    rssi_rp2040;  // Параметр радиоканала или протокола: описывает частоту, мощность, профиль, режим передачи или текущее состояние RF.
    float     local_latitude;
    float     local_longitude;
    bool      valid;
    float     snr;  // Параметр радиоканала или протокола: описывает частоту, мощность, профиль, режим передачи или текущее состояние RF.
} ufo_t;

using Aircraft = ufo_t;

extern unsigned long UpdateTrafficTimeMarker;  // Структура данных самолета или цели: хранит параметры борта, используемые при обмене и отображении.
extern ufo_t fo;  // Структура данных самолета или цели: хранит параметры борта, используемые при обмене и отображении.
extern ufo_t Container[MAX_TRACKING_OBJECTS];  // Контейнер данных, таблица, база или вспомогательный массив.
extern ufo_t EmptyFO;

class ContainerManager
{
public:
    void init();
    bool update(const Aircraft& ac);
    bool updateFromCandidate(const TrafficCandidate& candidate);
    void removeStale(uint32_t nowMs = millis());
    Aircraft* getList();
    const Aircraft* getList() const;
    int getCount() const;
};

extern ContainerManager TrafficDB;  // Контейнер данных, таблица, база или вспомогательный массив.

bool Traffic_Add(ufo_t *fop);
void Traffic_Update(ufo_t *fop);
void Traffic_loop();
void ClearExpired();
int Traffic_Count();

