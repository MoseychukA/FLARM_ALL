#include "Module1090.h"
#include "TrafficHelper.h"
#include "TimeRF.h"
#include <TimeLib.h>
#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>
#include <Arduino.h>
#include "Configuration_ESP32.h"
#include "Memory.h"               // ������ � ����������������� �������
#include "ESP32RF.h"
#include "EEPROMRF.h"

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

 /*       Serial.print("sizeof ToDUMP1090 ");
        Serial.println(sizeof(ToDUMP1090));
        SerialOutput.print("PACKET TOO SMALL: ");
        SerialOutput.println(packetSize);*/

        return;
    }

    ToDUMP1090 receivedPacket;

    memcpy(&receivedPacket, packet, packetSize);

    //for (int i = 0; i < packetSize; i++)
    //{
    //    SerialOutput.print(packet[i]);
    //    SerialOutput.print("|");
    //}
    //SerialOutput.print(" ** ");
    //SerialOutput.println(packetSize);
    //SerialOutput.println();

    fo.addr = receivedPacket.addr;                  // ����� ����������
    fo.Squawk =  atoi(receivedPacket.squawk);       // �����, ����������� ����������� ��� ������ � �������
    memcpy(fo.flight, receivedPacket.flight, sizeof(receivedPacket.flight));  // ����� �����
    fo.altitude = receivedPacket.altitude;          // ������ ����� (GPS) ����� 
    fo.pressure_altitude = receivedPacket.altitude; // ������ �� ������� �������� �����
    fo.speed = receivedPacket.speed;                // �������� ��/���
    fo.course = receivedPacket.track;               // ���� � �������� 
    fo.vert_rate = receivedPacket.vert_rate;        // �������� ������� ��� �������� ������ � ������?
    fo.latitude  = atof(receivedPacket.strLat_msg);  // ������
    fo.longitude = atof(receivedPacket.strLon_msg);  // �������

    //fo.latitude  = receivedPacket.lat_msg;              // ������
    //fo.longitude = receivedPacket.lon_msg;              // �������
    fo.seen = receivedPacket.seen_time;             // ����� ��������� �������� ������

    fo.timestamp = now();                           // ������� ����� �������� ������
    fo.signal_source = 1;                           // �������� ������ (DUMP1090)
    fo.aircraft_type = 9;                           // ��� ���������� �����
 
    Serial.println("Hex , Squawk, Flight, alt , pres alt, speed, course, vert_rate,   lat   ,    lon   , air_type, Ti");
    Serial.println("--------------------------------------------------------------------------------------------");
    snprintf_P(DUMP1090Buffer, sizeof(DUMP1090Buffer),
        PSTR("%06X,%d, %8s,%d,   %d,   %d,    %d,   %d,      %8f, %9f, %d, %d"),
        fo.addr,                   // ����� ����������
        fo.Squawk,                 // �����, ����������� ����������� ��� ������ � ���������.
        fo.flight,                 // ����� �����
        (int)fo.altitude,          // ������ ����� (GPS) �����
        (int)fo.pressure_altitude, // ������ �� ������� �������� �����
        (int)fo.speed,             // �������� ��/���
        (int)fo.course,            // ���� � ��������
        fo.vert_rate,              // �������� ������� ��� �������� ������ � ������?
        fo.latitude,               // ������
        fo.longitude,              // �������
        fo.aircraft_type,          // ��� ���������� �����
        fo.seen                    // ����� ��������� �������� ������ 
        );
       Serial.println(DUMP1090Buffer);
       Serial.println();
       Serial.println();
  
    /* ������ ����������, ����� � ������ ���������� ��������� ������ � ���������� ��������*/
    if (fo.latitude != 0 && fo.longitude != 0) // ������ �������� ���� �������� ���������� ������ � ���������� ��������
    {
        Traffic_Update(&fo);   // 
    }

    /* ��������� ��������� ���������� � ���� */
    Traffic_Add(&fo);
}
void Module1090::setup()
{

	

}

void Module1090::update()
{
    if (settings->d1090 == D1090_UART)
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
}


//--------------------------------------------------------------------------------------------------------------------------------
