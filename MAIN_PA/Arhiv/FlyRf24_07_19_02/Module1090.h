#pragma once

#include "SoftRF.h"

#pragma pack(push,1)

#pragma pack(push,1)
struct ToDUMP1090
{
    uint32_t      addr;           // ICAO address
    char          squawk[5];      // Squawk
    char          flight[16];     // Flight number	
    int           altitude;       // Altitude
    int           speed;          // Velocity
    int           track;          // Angle of flight
    int           vert_rate;      // Vertical rate.
    double        lat;
    double        lon;            // Coordinated obtained from CPR encoded data
    int           seen_time;      // Time at which the last packet was received
    char endOfPacket[3];          // 0xFF 0xFF 0xFF
};
#pragma pack(pop)




//struct ToDUMP1090
//{
//
//    uint32_t      addr;               // ICAO address
//    char          squawk[5];          // Squawk
//    char          flight[16];         // Flight number	
//    int           altitude;           // Altitude
//    int           speed;              // Velocity
//    int           track;              // Angle of flight
//    int           vert_rate;          // Vertical rate.
//    double        lat;
//    double        lon;                // Coordinated obtained from CPR encoded data
//    time_t        seen;               // Time at which the last packet was received
//    char endOfPacket[3]; // 0xFF 0xFF 0xFF
//
//
//
//   // uint32_t      addr;           // ICAO address
//   // int           squawk;         // Squawk
//   // //char          flight[16];     // Flight number	
//   // int           altitude;       // Altitude
//   // int           speed;          // Velocity
//   // int           track;          // Angle of flight
//   // int           vert_rate;      // Vertical rate.
//   // double        lat;
//   // double        lon;            // Coordinated obtained from CPR encoded data
//   // time_t        seen;           // Time at which the last packet was received
//   //// uint64_t      timestamp;      // Timestamp at which the last packet was received
//   // char endOfPacket[3]; // 0xFF 0xFF 0xFF
//
//
//   // uint32_t      addr;              // ICAO address
//   // char          flight[16];        // Flight number	
//   // int           altitude;          // Altitude
//   // int           speed;             // Velocity
//   // int           track;             // Angle of flight
//   // //int           vert_rate;       // Vertical rate.
//   //// time_t        seenLatLon;      // Time at which the last lat long was calculated
//   // //uint64_t      timestamp;       // Timestamp at which the last packet was received
//   //// uint64_t      timestampLatLon; // Timestamp at which the last lat long was calculated
//   // double        lat;
//   // double        lon;               // Coordinated obtained from CPR encoded data
//   // uint8_t       signal_source;     // Источник сигнала
//   // time_t        seen;              // Time at which the last packet was received
//   // unsigned int  pSignal;           // Уровень сигнала 
//
//   // char endOfPacket[3]; // 0xFF 0xFF 0xFF
//};
#pragma pack(pop)
//--------------------------------------------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------------------------------------
class Module1090
{
public:
    Module1090();

    void setup();
    void update();                                                // обновить данные
    void ParsePacket(const byte* packet, int packetSize);
     
private:

};
//--------------------------------------------------------------------------------------------------------------------------------
extern Module1090 moduleDump1090;
