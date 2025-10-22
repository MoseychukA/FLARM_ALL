#pragma once

#include "SoftRF.h"

#pragma pack(push,1)

//// Структура RAW для обмена по UART (сPacked обязательно)
//struct __attribute__((packed)) ToDUMP1090_RAW {
//    uint32_t  addr;
//    char      squawk[5];
//    char      flight[16];
//    int32_t   altitude;
//    int32_t   speed;
//    int32_t   course;
//    int32_t   vert_rate;
//    float     lat_msg;
//    float     lon_msg;
//    int32_t   seen_time;
//    char      endOfPacket[3];
//};
//
//// Оконечная структура для работы
//struct ToDUMP1090 {
//    uint32_t  addr;
//    char      squawk[5];
//    char      flight[16];
//    int32_t   altitude;
//    int32_t   speed;
//    int32_t   course;
//    int32_t   vert_rate;
//    float     lat_msg;
//    float     lon_msg;
//    int32_t   seen_time;
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
    bool getNewDUMP_0_Flag();
    void setNewDUMP_0_Flag(bool new_DUMP_flag);
    //void unpack_ToDUMP1090(const ToDUMP1090_RAW* in, ToDUMP1090* out);
    //uint32_t swap32(uint32_t val);
    //float swapFloat(float val);
     
private:
    char DUMP1090Buffer[128];
    bool empty_DUMP_flag = false;

};
//--------------------------------------------------------------------------------------------------------------------------------
extern Module1090 moduleDump1090;
//extern ufo_t fo;