#include "Module1090.h"
#include "TrafficHelper.h"
#include "TimeRF.h"
#include <TimeLib.h>
#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>
#include <Arduino.h>
#include "Configuration_ESP32.h"
#include "Memory.h"               // –абота с энергонезависимой пам€тью

//--------------------------------------------------------------------------------------------------------------------------------
Module1090 moduleDump1090;
//--------------------------------------------------------------------------------------------------------------------------------
Module1090::Module1090()
{

}

//--------------------------------------------------------------------------------------------------------------------------------
void Module1090::ParsePacket(const byte* packet, int packetSize)
{
    if (packetSize < sizeof(ToDUMP1090))
    {
        //SerialOutput.print("PACKET TOO SMALL: ");
        //SerialOutput.println(packetSize);
        return;
    }

    ToDUMP1090 receivedPacket;
    memcpy(&receivedPacket, packet, packetSize);

  /*  for (int i = 0; i < packetSize; i++)
    {
        SerialOutput.print(packet[i]);
        SerialOutput.print("|");
    }
    SerialOutput.print(" ** ");
    SerialOutput.println(packetSize);*/


    fo.addr = receivedPacket.addr;
    fo.latitude = receivedPacket.lat;
    fo.longitude = receivedPacket.lon;
    fo.altitude = receivedPacket.altitude;
    fo.pressure_altitude = receivedPacket.altitude;
    fo.speed = receivedPacket.speed;
    fo.signal_source = receivedPacket.signal_source;
    fo.timestamp = now(); // 
    fo.seen = receivedPacket.seen;
    fo.course = receivedPacket.track;
    fo.pSignal = receivedPacket.pSignal;

    /* –асчет рассто€ни€, курса и уровн€ опастности сближени€ нашего и стороннего самолета*/
    if (fo.latitude != 0 && fo.longitude != 0) // –асчет возможен если получены координаты нашего и стороннего самолета
    {
        Traffic_Update(&fo);   // 
    }

    /* ќстальные параметры записываем в базу */
    Traffic_Add(&fo);
}
void Module1090::setup()
{

	

}

void Module1090::update()
{
    static byte buff[128] = { 0 };
    static int bytesReceived = 0;
    static int writeIndex = 0;
    static byte endOfPacketCounter = 0;

    while (SerialOutput.available())
    {
        byte ch = (byte)SerialOutput.read();

        buff[writeIndex++] = ch;
        bytesReceived++;

        if (writeIndex >= sizeof(buff))
        {
            writeIndex = 0;
            bytesReceived = 0;
            memset(buff, 0, sizeof(buff));
        }
        else
        {
            if (ch == 0xFF)
            {
                if (++endOfPacketCounter >= 3)
                {
                    ParsePacket(buff, bytesReceived);
                    memset(buff, 0, sizeof(buff));
                    writeIndex = 0;
                    bytesReceived = 0;
                }
            }
            else
            {
                endOfPacketCounter = 0;
            }
        }
    }

}


//--------------------------------------------------------------------------------------------------------------------------------
