#ifndef ADSB_PACKET_H
#define ADSB_PACKET_H

#include <Arduino.h>

#pragma pack(push,1)
struct ToDUMP1090 {
    uint32_t addr;        // ICAO address
    uint16_t squawk;      // Squawk code
    char flight[16];      // Flight number
    int altitude;         // Altitude (ft)
    int speed;            // Velocity (knots or km/h depending on parser)
    int track;            // Heading / course
    int vert_rate;        // Vertical rate (ft/min)
    float lat;            // Latitude
    float lon;            // Longitude
    int seen_time;        // Last time packet was seen (s)
    char endOfPacket[3];  // 0xFF 0xFF 0xFF
};
#pragma pack(pop)

#endif
