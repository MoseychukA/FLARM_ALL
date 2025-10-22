#ifndef DUMP1090_ADAPTER_H
#define DUMP1090_ADAPTER_H

#include "packet_decoder.h" // Aircraft1090

struct ToDUMP1090
{
    uint32_t  addr;           // ICAO address
    uint16_t  squawk;         // Squawk
    char      flight[16];     // Flight number	
    int32_t   altitude;       // Altitude
    int32_t   speed;          // Velocity
    int32_t   track;          // Angle
    int32_t   vert_rate;      // Vertical rate
    float     lat;
    float     lon;            // Coordinates
    int32_t   seen_time;      // Time last seen
    uint8_t   endOfPacket[3]; // 0xFF 0xFF 0xFF
};

ToDUMP1090 ConvertToDump1090(const Aircraft1090 &ac);

#endif
