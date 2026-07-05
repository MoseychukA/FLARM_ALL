#pragma once

#include "SoftRF.h"

#pragma pack(push,1)

#pragma pack(push,1)
struct ToDUMP1090
{
    uint32_t  addr;           // ICAO address
    char      squawk[5];      // Squawk
    char      flight[16];     // Flight number	
    int       altitude;       // Altitude
    int       speed;          // Velocity
    int       track;          // Angle of flight
    int       vert_rate;      // Vertical rate.
    float     lat_msg;
    float     lon_msg;      // Coordinated obtained from CPR encoded data
    int       seen_time;      // Time at which the last packet was received
    char      endOfPacket[3]; // 0xFF 0xFF 0xFF
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
    bool getNewDUMP_0_Flag();
    void setNewDUMP_0_Flag(bool new_DUMP_flag);
     
private:
    char DUMP1090Buffer[128];
    bool empty_DUMP_flag = false;

};
//--------------------------------------------------------------------------------------------------------------------------------
extern Module1090 moduleDump1090;
//extern ufo_t fo;