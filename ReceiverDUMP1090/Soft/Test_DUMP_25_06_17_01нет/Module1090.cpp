#include "Module1090.h"
#include "TrafficHelper.h"
#include "TimeRF.h"
#include <TimeLib.h>
#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>
#include <Arduino.h>
#include "Configuration_ESP32.h"
#include "ESP32RF.h"
#include "EEPROMRF.h"
#include <esp_task_wdt.h>
#include "GNSS.h"

//--------------------------------------------------------------------------------------------------------------------------------
Module1090 moduleDump1090;
//ufo_t fo;
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

        //SerialOutput.print("PACKET TOO SMALL: ");
        //SerialOutput.println(packetSize);

        return;
    }
    esp_task_wdt_reset();
    ToDUMP1090 receivedPacket;

    memcpy(&receivedPacket, packet, packetSize);

  /*  for (int i = 0; i < packetSize; i++)
    {
        SerialOutput.print(packet[i]);
        SerialOutput.print("|");
    }
    SerialOutput.print(" ** ");
    SerialOutput.println(packetSize);
    SerialOutput.println();*/


    fo = EmptyFO;

    fo.addr = receivedPacket.addr;                  // Адрес устройства
    fo.Squawk =  atoi(receivedPacket.squawk);       // Номер, назначаемый диспетчером для обмена с локатор
    memcpy(fo.flight, receivedPacket.flight, sizeof(receivedPacket.flight));  // Номер рейса
    fo.altitude = receivedPacket.altitude;          // Высота геоид (GPS) метры 
    fo.pressure_altitude = receivedPacket.altitude; // Высота по датчику давления метры
    fo.speed = receivedPacket.speed;                // Скорость км/час
    fo.course = receivedPacket.track;               // Курс в градусах 
    fo.vert_rate = receivedPacket.vert_rate;        // Скорость подъема или снижения метров в минуту?
    fo.latitude  = receivedPacket.lat_msg;          // Широта
    fo.longitude = receivedPacket.lon_msg;          // Долгота
    fo.seen = receivedPacket.seen_time;             // Время последней отправки пакета

    fo.timestamp = now();                           // текущее время отправки пакета
    fo.signal_source = 1;                           // Источник пакета (DUMP1090)
    fo.aircraft_type = 9;                           // Тип воздушного судна


    if (gnss.time.isValid())
    {
        fo.hour_msg = (int)gnss.time.hour();
        fo.min_msg = gnss.time.minute();
    }
 
 
    //Serial.println("Hex , Squawk, Flight, alt , pres alt, speed, course, vert_rate,   lat   ,    lon   , air_type, Ti");
    //Serial.println("--------------------------------------------------------------------------------------------");
    //snprintf_P(DUMP1090Buffer, sizeof(DUMP1090Buffer),
    //    PSTR("%06X,%d, %8s,%d,   %d,   %d,    %d,   %d,      %8f, %9f, %d, %d"),
    //    fo.addr,                   // Адрес устройства
    //    fo.Squawk,                 // Номер, назначаемый диспетчером для обмена с локатором.
    //    fo.flight,                 // Номер рейса
    //    (int)fo.altitude,          // Высота геоид (GPS) метры
    //    (int)fo.pressure_altitude, // Высота по датчику давления метры
    //    (int)fo.speed,             // Скорость км/час
    //    (int)fo.course,            // Курс в градусах
    //    fo.vert_rate,              // Скорость подъема или снижения метров в минуту?
    //    fo.latitude,               // Широта
    //    fo.longitude,              // Долгота
    //    fo.aircraft_type,          // Тип воздушного судна
    //    fo.seen                    // Время последней отправки пакета 
    //    );
    //   Serial.println(DUMP1090Buffer);
    //   Serial.println();
    //   Serial.println();
  
    /* Расчет расстояния, курса и уровня опастности сближения нашего и стороннего самолета*/
    if (fo.latitude != 0 && fo.longitude != 0) // Расчет возможен если получены координаты нашего и стороннего самолета
    {
        if (fo.seen < 20)
        {
          //  Serial.println(fo.seen);
            Traffic_Update(&fo);   // 
            /* Остальные параметры записываем в базу */
            if (settings->d1090 == D1090_UART_MINI)
            {
                Traffic_Add(&fo);           // Записать данные по самолету только с координатами
            }
        }
        esp_task_wdt_reset();
    }
    else
    {
        moduleDump1090.setNewDUMP_0_Flag(true);
        esp_task_wdt_reset();
    }

    if (settings->d1090 == D1090_UART_FULL)
    {
        if (fo.seen < 20)
        {
           // Serial.println(fo.seen);
            Traffic_Add(&fo);           // Записать все данные по самолету
        }
    }
 }
void Module1090::setup()
{
 /*   digitalWrite(SOC_GPIO_PIN_LED, LOW);
    delay(100);
    digitalWrite(SOC_GPIO_PIN_LED, HIGH);
    delay(100);
    digitalWrite(SOC_GPIO_PIN_LED, LOW);
    delay(100);
    digitalWrite(SOC_GPIO_PIN_LED, HIGH);*/
}

void Module1090::update()
{
    if (settings->d1090 == D1090_UART_MINI || settings->d1090 == D1090_UART_FULL)
    {
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
    }
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
