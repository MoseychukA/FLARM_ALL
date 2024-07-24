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

        Serial.print("sizeof ToDUMP1090 ");
        Serial.println(sizeof(ToDUMP1090));
        SerialOutput.print("PACKET TOO SMALL: ");
        SerialOutput.println(packetSize);

       /* for (int i = 0; i < packetSize; i++)
        {
            SerialOutput.print(packet[i]);
            SerialOutput.print("|");
        }
        SerialOutput.print(" ** ");
        SerialOutput.println(packetSize);*/

        return;
    }

    ToDUMP1090 receivedPacket;

    memcpy(&receivedPacket, packet, packetSize);

   /* for (int i = 0; i < packetSize; i++)
    {
        SerialOutput.print(packet[i]);
        SerialOutput.print("|");
    }
    SerialOutput.print(" ** ");
    SerialOutput.println(packetSize);*/

    /*
        uint32_t      addr;           // ICAO address
        int           squawk;         // Squawk
        char          flight[16];     // Flight number	
        int           altitude;       // Altitude
        int           speed;          // Velocity
        int           track;          // Angle of flight
        int           vert_rate;      // Vertical rate.
        double        lat;
        double        lon;            // Coordinated obtained from CPR encoded data
        time_t        seen;           // Time at which the last packet was received
    */


    //fo.addr = receivedPacket.addr;
    //fo.latitude = receivedPacket.lat;
    //fo.longitude = receivedPacket.lon;
    //fo.altitude = receivedPacket.altitude;
    //fo.pressure_altitude = receivedPacket.altitude;
    //fo.speed = receivedPacket.speed;
    //fo.signal_source = 1;
    //fo.timestamp = now(); // 
    //fo.seen = receivedPacket.seen;
    //fo.course = receivedPacket.track;


    fo.addr = receivedPacket.addr;
    fo.Squawk =  atoi(receivedPacket.squawk);
    memcpy(fo.flight, receivedPacket.flight, sizeof(receivedPacket.flight));
    fo.pressure_altitude = receivedPacket.altitude;
    fo.speed = receivedPacket.speed;
    fo.course = receivedPacket.track;
    fo.vert_rate = receivedPacket.vert_rate;
    fo.latitude = receivedPacket.lat;
    fo.longitude = receivedPacket.lon;
    fo.seen = receivedPacket.seen_time;
    fo.timestamp = now(); // 
    fo.signal_source = 1;
    fo.aircraft_type = 9;
 
   // int squawk_tmp = atoi(receivedPacket.squawk);

    //fo.addr = receivedPacket.addr;
    ////fo.Squawk = receivedPacket.squawk;
    //fo.latitude = receivedPacket.lat;
    //fo.longitude = receivedPacket.lon;
    ////fo.altitude = receivedPacket.altitude;
    //fo.pressure_altitude = receivedPacket.altitude;
    //fo.speed = receivedPacket.speed;
    //fo.course = receivedPacket.track;
    //fo.vert_rate = receivedPacket.vert_rate;
    //fo.seen = receivedPacket.seen;

    //fo.signal_source = 1;
    //fo.timestamp = now(); // 
  
   // fo.pSignal = receivedPacket.pSignal;


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
                  
                    for (int i = 0; i < bytesReceived; i++)
                    {
                        SerialOutput.print(buff[i]);
                        SerialOutput.print("|");
                    }
                    SerialOutput.print(" ** ");
                    SerialOutput.println(bytesReceived);

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
