#include "Module1090.h"
#include "TrafficHelper.h"
#include "TimeRF.h"
#include <TimeLib.h>
#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>
#include <Arduino.h>
#include "Memory.h"               // Работа с энергонезависимой памятью
#include "ESP32RF.h"
#include "EEPROMRF.h"
#include <esp_task_wdt.h>

//--------------------------------------------------------------------------------------------------------------------------------
Module1090 moduleDump1090;
ufo_t fo_1090;
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
        Serial.println(sizeof(ToDUMP1090));*/

        SerialOutput.print("PACKET TOO SMALL: ");
        SerialOutput.println(packetSize);

        return;
    }
    esp_task_wdt_reset();
    ToDUMP1090 receivedPacket;

    memcpy(&receivedPacket, packet, packetSize);

    for (int i = 0; i < packetSize; i++)
    {
        SerialOutput.print(packet[i]);
        SerialOutput.print("|");
    }
    SerialOutput.print(" ** ");
    SerialOutput.println(packetSize);
    SerialOutput.println();
    fo_1090 = EmptyFO;

    fo_1090.addr = receivedPacket.addr;                  // Адрес устройства
    //fo_1090.Squawk =  atoi(receivedPacket.squawk);       // Номер, назначаемый диспетчером для обмена с локатор
    //memcpy(fo_1090.flight, receivedPacket.flight, sizeof(receivedPacket.flight));  // Номер рейса
    fo_1090.altitude = receivedPacket.altitude;          // Высота геоид (GPS) метры 
    fo_1090.pressure_altitude = receivedPacket.altitude; // Высота по датчику давления метры
    fo_1090.speed = receivedPacket.speed;                // Скорость км/час
    fo_1090.course = receivedPacket.track;               // Курс в градусах 
    //fo_1090.vert_rate = receivedPacket.vert_rate;        // Скорость подъема или снижения метров в минуту?
    fo_1090.latitude  = receivedPacket.lat_msg;              // Широта
    fo_1090.longitude = receivedPacket.lon_msg;              // Долгота
    //fo_1090.seen = receivedPacket.seen_time;             // Время последней отправки пакета

    fo_1090.timestamp = now();                           // текущее время отправки пакета
    //fo_1090.signal_source = 1;                           // Источник пакета (DUMP1090)
    fo_1090.aircraft_type = 9;                           // Тип воздушного судна
 
    Serial.println("Hex , Squawk, Flight, alt , pres alt, speed, course, vert_rate,   lat   ,    lon   , air_type, Ti");
    Serial.println("--------------------------------------------------------------------------------------------");
    snprintf_P(DUMP1090Buffer, sizeof(DUMP1090Buffer),
        PSTR("%06X,%d,   %d,   %d,    %d,      %8f, %9f, %d,"),
       // PSTR("%06X,%d, %8s,%d,   %d,   %d,    %d,   %d,      %8f, %9f, %d, %d"),
        fo_1090.addr,                   // Адрес устройства
        //fo_1090.Squawk,                 // Номер, назначаемый диспетчером для обмена с локатором.
        //fo_1090.flight,                 // Номер рейса
        (int)fo_1090.altitude,          // Высота геоид (GPS) метры
        (int)fo_1090.pressure_altitude, // Высота по датчику давления метры
        (int)fo_1090.speed,             // Скорость км/час
        (int)fo_1090.course,            // Курс в градусах
        //fo_1090.vert_rate,              // Скорость подъема или снижения метров в минуту?
        fo_1090.latitude,               // Широта
        fo_1090.longitude,              // Долгота
        fo_1090.aircraft_type          // Тип воздушного судна
       // fo_1090.seen                    // Время последней отправки пакета 
        );
       Serial.println(DUMP1090Buffer);
       Serial.println();
       Serial.println();
  
    /* Расчет расстояния, курса и уровня опастности сближения нашего и стороннего самолета*/
    if (fo_1090.latitude != 0 && fo_1090.longitude != 0) // Расчет возможен если получены координаты нашего и стороннего самолета
    {
        Traffic_Update(&fo_1090);   // 
        /* Остальные параметры записываем в базу */
        Traffic_Add(&fo_1090);
        esp_task_wdt_reset();
    }
    else
    {
        moduleDump1090.setNewDUMP_0_Flag(true);
        esp_task_wdt_reset();
    }

 }
void Module1090::setup()
{

	

}

void Module1090::update()
{
 /*   if (settings->d1090 == D1090_UART)
    {*/
        static byte buff[128] = { 0 };
        static int bytesReceived = 0;
        static int writeIndex = 0;
        static byte endOfPacketCounter = 0;
        esp_task_wdt_reset();
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
                        Serial.flush();
                        writeIndex = 0;
                        bytesReceived = 0;
                    }
                }
                else
                {
                    endOfPacketCounter = 0;
                }
            }
            esp_task_wdt_reset();
        }
   // }
}

//--------------------------------------------------------------------------------------------------------------------------------
bool Module1090::getNewDUMP_0_Flag()
{
    return empty_DUMP_flag;
}

void Module1090::setNewDUMP_0_Flag(bool new_flag)
{
    // Сохранить флаг нового сообщения
    empty_DUMP_flag = new_flag;

}

//--------------------------------------------------------------------------------------------------------------------------------
