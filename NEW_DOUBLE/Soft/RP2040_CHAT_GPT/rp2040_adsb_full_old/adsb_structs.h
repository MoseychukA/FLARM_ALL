
// adsb_structs.h
#pragma once
#include <Arduino.h>

#pragma pack(push,1)

//// Структура для отправки в ESP32
//struct ToDUMP1090 {
//    uint32_t  addr;           // ICAO address
//    uint16_t  squawk;         // Squawk
//    char      flight[16];     // Flight number
//    int       altitude;       // Altitude (метры)
//    int       speed;          // Velocity (км/ч)
//    int       track;          // Course (градусы)
//    int       vert_rate;      // Vertical rate (ft/min или м/мин)
//    float     lat;            // Latitude
//    float     lon;            // Longitude
//    int       seen_time;      // Last packet timestamp (ms)
//    char      endOfPacket[3]; // 0xFF 0xFF 0xFF
//};
//#pragma pack(pop)


//// Внутреннее представление декодированного пакета ADS-B
//struct DecodedADSB {
//    uint32_t addr;              // ICAO
//    uint16_t Squawk;            // Squawk code
//    char     flight[16];        // Flight number
//    int      altitude;          // Altitude
//    int      pressure_altitude; // Pressure altitude
//    int      speed;             // Speed
//    int      course;            // Track/course
//    int      vert_rate;         // Vertical rate
//    float    lat_msg;           // Latitude
//    float    lon_msg;           // Longitude
//    int      seen_time;         // Reception time
//    unsigned long timestamp;    // System timestamp
//};





//#ifndef ADSB_STRUCTS_H
//#define ADSB_STRUCTS_H
//
//#include <Arduino.h>
//
//#pragma pack(push,1)
//
///// Структура для передачи расшифрованного пакета ADS-B в ESP32S3
//struct ToDUMP1090 {
//    uint32_t addr;        // ICAO address
//    uint16_t squawk;      // Squawk code
//    char flight[16];      // Flight number
//    int altitude;         // Altitude (ft)
//    int speed;            // Velocity (knots)
//    int track;            // Heading / course (deg)
//    int vert_rate;        // Vertical rate (ft/min)
//    float lat;            // Latitude
//    float lon;            // Longitude
//    int seen_time;        // Last time packet was seen (s)
//    char endOfPacket[3];  // Маркер конца: 0xFF 0xFF 0xFF
//};
//
///// Структура внутреннего декодированного ADS-B пакета (как результат парсинга)
//struct DecodedADSB {
//    uint32_t addr;
//    uint16_t squawk;
//    char flight[16];
//    int altitude;
//    int speed;
//    int track;
//    int vert_rate;
//    float lat;
//    float lon;
//    int seen_time;
//};
//
//#pragma pack(pop)
//
//#endif // ADSB_STRUCTS_H
