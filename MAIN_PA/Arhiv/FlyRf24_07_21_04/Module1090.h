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

    char      strLat_msg[8];
    char      strLon_msg[9];

    //float         lat;
    //float         lon;            // Coordinated obtained from CPR encoded data
    //double        lat;
    //double        lon;            // Coordinated obtained from CPR encoded data
    int           seen_time;      // Time at which the last packet was received
    char endOfPacket[3];          // 0xFF 0xFF 0xFF
};
#pragma pack(pop)


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
    char DUMP1090Buffer[128];
};
//--------------------------------------------------------------------------------------------------------------------------------
extern Module1090 moduleDump1090;
