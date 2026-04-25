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
    int8_t    rssi_LoRa;
    int       rssi;
    float     distance;
    float     bearing;
    int8_t    alarm_level;
    union { uint8_t signal_source; TrafficSource source; };
    time_t    seen;
    uint8_t   hour_msg;
    uint8_t   min_msg;
    uint16_t  delay_time_msg;
    int8_t    rssi_rp2040;
    float     local_latitude;
    float     local_longitude;
    bool      valid;
    float     snr;
} ufo_t;

using Aircraft = ufo_t;

extern unsigned long UpdateTrafficTimeMarker;
extern ufo_t fo;
extern ufo_t Container[MAX_TRACKING_OBJECTS];
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

extern ContainerManager TrafficDB;

bool Traffic_Add(ufo_t *fop);
void Traffic_Update(ufo_t *fop);
void Traffic_loop();
void ClearExpired();
int Traffic_Count();

