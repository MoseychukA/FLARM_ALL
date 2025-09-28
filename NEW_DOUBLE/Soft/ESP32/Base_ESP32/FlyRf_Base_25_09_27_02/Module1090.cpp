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
#include <string.h> // Для memcpy

//
//// Функции обмена с эндian
//uint32_t Module1090::swap32(uint32_t val)
//{
//    return ((val & 0xFF) << 24) |
//        ((val & 0xFF00) << 8) |
//        ((val & 0xFF0000) >> 8) |
//        ((val & 0xFF000000) >> 24);
//}
//
//float Module1090::swapFloat(float val)
//{
//    uint32_t temp;
//    memcpy(&temp, &val, 4);
//    temp = swap32(temp);
//    float res;
//    memcpy(&res, &temp, 4);
//    return res;
//}
//
//// Функция распаковки
//void Module1090::unpack_ToDUMP1090(const ToDUMP1090_RAW* in, ToDUMP1090* out)
//{
//    out->addr = swap32(in->addr);
//    memcpy(out->squawk, in->squawk, 5);
//    memcpy(out->flight, in->flight, 16);
//    out->altitude = swap32(in->altitude);
//    out->speed = swap32(in->speed);
//    out->track = swap32(in->track);
//    out->vert_rate = swap32(in->vert_rate);
//    out->lat_msg = swapFloat(in->lat_msg);
//    out->lon_msg = swapFloat(in->lon_msg);
//    out->seen_time = swap32(in->seen_time);
//}
//
//const size_t PACKET_SIZE = sizeof(ToDUMP1090_RAW);


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
 
   // if (packetSize < sizeof(ToDUMP1090))
   // {

   //     //Serial.print("sizeof ToDUMP1090 ");
   //     //Serial.println(sizeof(ToDUMP1090));

   //     //Serial.print("PACKET TOO SMALL: ");
   //     //Serial.println(packetSize);

   //     return;
   // }
   // esp_task_wdt_reset();
   // ToDUMP1090 receivedPacket;

   // memcpy(&receivedPacket, packet, packetSize);

   ////for (int i = 0; i < packetSize; i++)
   //// {
   ////     Serial.print(packet[i]);
   ////     Serial.print("|");
   //// }
   //// Serial.print(" ** ");
   //// Serial.println(packetSize);
   //// Serial.println();


   // fo = EmptyFO;
   // fo.addr = receivedPacket.addr;                  // Адрес устройства
   // fo.Squawk =  atoi(receivedPacket.squawk);       // Номер, назначаемый диспетчером для обмена с локатор
   // memcpy(fo.flight, receivedPacket.flight, sizeof(receivedPacket.flight));  // Номер рейса
   // fo.altitude = receivedPacket.altitude;          // Высота геоид (GPS) метры 
   // fo.pressure_altitude = receivedPacket.altitude; // Высота по датчику давления метры
   // fo.speed = receivedPacket.speed;                // Скорость км/час
   // fo.course = receivedPacket.track;               // Курс в градусах 
   // fo.vert_rate = receivedPacket.vert_rate;        // Скорость подъема или снижения метров в минуту?
   // fo.latitude  = receivedPacket.lat_msg;          // Широта
   // fo.longitude = receivedPacket.lon_msg;          // Долгота
   // fo.seen = receivedPacket.seen_time;             // Время последней отправки пакета

   // fo.timestamp = now();                           // текущее время отправки пакета
   // fo.signal_source = 1;                           // Источник пакета (DUMP1090)
   // fo.aircraft_type = 9;                           // Тип воздушного судна


   // if (gnss.time.isValid())
   // {
   //     fo.hour_msg = (int)gnss.time.hour();
   //     fo.min_msg = gnss.time.minute();
   // }
 
 
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
  
    ///* Расчет расстояния, курса и уровня опастности сближения нашего и стороннего самолета*/
    //if (fo.latitude != 0 && fo.longitude != 0) // Расчет возможен если получены координаты нашего и стороннего самолета
    //{
    //    if (fo.seen < 20)
    //    {
    //      //  Serial.println(fo.seen);
    //        Traffic_Update(&fo);   // 
    //        /* Остальные параметры записываем в базу */
    //        if (settings->d1090 == D1090_UART_MINI)
    //        {
    //            Traffic_Add(&fo);           // Записать данные по самолету только с координатами
    //        }
    //    }
    //    esp_task_wdt_reset();
    //}
    //else
    //{
    //    moduleDump1090.setNewDUMP_0_Flag(true);
    //    esp_task_wdt_reset();
    //}

    //if (settings->d1090 == D1090_UART_FULL)
    //{
    //    if (fo.seen < 20)
    //    {
    //       // Serial.println(fo.seen);
    //        Traffic_Add(&fo);           // Записать все данные по самолету
    //    }
    //}
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



//!!
//
//
//ToDUMP1090 packet;
//ToDUMP1090_RAW inRaw; // Объявляем переменную глобально или в начале функции
//
//#define MAX_BUFFER_SIZE 128
//uint8_t rxBuffer[MAX_BUFFER_SIZE];
//uint16_t rxIndex = 0;

void Module1090::update()
{
 /*   if (settings->d1090 == D1090_UART_MINI || settings->d1090 == D1090_UART_FULL)
    {*/
        //static byte buff[256] = { 0 };
        //static int bytesReceived = 0;
        //static int writeIndex = 0;
        //static byte endOfPacketCounter = 0;
        esp_task_wdt_reset();

        //while (SerialRP2040.available()) 
        //{
        //    uint8_t b = SerialRP2040.read();

        //    if (rxIndex < MAX_BUFFER_SIZE) 
        //    {
        //        rxBuffer[rxIndex++] = b;
        //    }
        //    else 
        //    {
        //        // Если переполнение — сбросить
        //        rxIndex = 0;
        //    }

        //    // Проверка достижения размера пакета
        //    if (rxIndex >= PACKET_SIZE) 
        //    {
        //        // Проверка маркера конца
        //        if (rxBuffer[PACKET_SIZE - 3] == 0xFF &&
        //            rxBuffer[PACKET_SIZE - 2] == 0xFF &&
        //            rxBuffer[PACKET_SIZE - 1] == 0xFF) {
        //            // Весь пакет собран
        //            memcpy(&inRaw, rxBuffer, PACKET_SIZE);
        //            unpack_ToDUMP1090(&inRaw, &packet);
        //            // Обработка
        //            Serial.print("ICAO: "); Serial.println(packet.addr, HEX);
        //            Serial.print("Flight: "); Serial.println(packet.flight);
        //            // Очистка буфера для нового пакета
        //            rxIndex = 0;
        //        }
        //    }
        //}







        //while (SerialRP2040.available()) 
        //{
        //    uint8_t b = SerialRP2040.read();
        //    if (rxCount >= sizeof(rxBuf)) rxCount = 0; // чтоб не ушли за память

        //    rxBuf[rxCount++] = b;
        //   
        //    // Ищем 3xFF в конце пакета
        //    if (rxCount >= PACKET_SIZE) 
        //    {
        //        if (rxBuf[PACKET_SIZE - 3] == 0xFF && rxBuf[PACKET_SIZE - 2] == 0xFF && rxBuf[PACKET_SIZE - 1] == 0xFF) 
        //        {
        //            memcpy(&inRaw, rxBuf, PACKET_SIZE);
        //            unpack_ToDUMP1090(&inRaw, &packet);
        //            // обработка
        //            Serial.print("ICAO: "); Serial.println(packet.addr, HEX);
        //            //Serial.print("Flight: "); Serial.println(packet.flight);
        //            //Serial.print("Squawk: "); Serial.println(packet.squawk);
        //            //Serial.print("Altitude: "); Serial.println(packet.altitude);
        //            //Serial.print("Speed: "); Serial.println(packet.speed);
        //            //Serial.print("Track: "); Serial.println(packet.track);
        //            //Serial.print("Lat: "); Serial.println(packet.lat_msg, 5);
        //            //Serial.print("Lon: "); Serial.println(packet.lon_msg, 5);
        //            //Serial.print("SeenTime: "); Serial.println(packet.seen_time);
        //            Serial.println();
        //            Serial.flush();
        //            rxCount = 0;
        //        }
        //    }
        //}








        //if (SerialRP2040.available() >= PACKET_SIZE)
        //{
        //    ToDUMP1090_RAW inRaw;
        //    size_t n = SerialRP2040.readBytes((uint8_t*)&inRaw, PACKET_SIZE);
        //    if (n == PACKET_SIZE &&
        //        (uint8_t)inRaw.endOfPacket[0] == 0xFF &&
        //        (uint8_t)inRaw.endOfPacket[1] == 0xFF &&
        //        (uint8_t)inRaw.endOfPacket[2] == 0xFF
        //        ) {
        //        ToDUMP1090 packet;
        //        unpack_ToDUMP1090(&inRaw, &packet);

        //        // Вывод данных
        //        Serial.print("ICAO: "); Serial.println(packet.addr, HEX);
        //                   Serial.print("Flight: "); Serial.println(packet.flight);
        //                   //Serial.print("Squawk: "); Serial.println(packet.squawk);
        //                   //Serial.print("Altitude: "); Serial.println(packet.altitude);
        //                   //Serial.print("Speed: "); Serial.println(packet.speed);
        //                   //Serial.print("Track: "); Serial.println(packet.track);
        //                   //Serial.print("Lat: "); Serial.println(packet.lat_msg, 5);
        //                   //Serial.print("Lon: "); Serial.println(packet.lon_msg, 5);
        //                   //Serial.print("SeenTime: "); Serial.println(packet.seen_time);
        //        Serial.flush();
        //    }
        //}




 
        //if (SerialRP2040.available()/* >= PACKET_SIZE*/)
        //{
        //    uint8_t buffer[PACKET_SIZE];
        //    size_t read_bytes = SerialRP2040.readBytes(buffer, PACKET_SIZE);

        //    if (read_bytes == PACKET_SIZE) 
        //    {
        //        // Копируем байты в структуру
        //        memcpy(&packet, buffer, PACKET_SIZE);

        //        // Проверяем маркер конца пакета
        //        if ((uint8_t)packet.endOfPacket[0] == 0xFF &&
        //            (uint8_t)packet.endOfPacket[1] == 0xFF &&
        //            (uint8_t)packet.endOfPacket[2] == 0xFF) 
        //        {
        //            ParsePacket(buff, bytesReceived);
        //            memset(buff, 0, sizeof(buff));
        //            SerialRP2040.flush();
        //            writeIndex = 0;
        //            bytesReceived = 0;
        //        }
        //        else {
        //            // Ошибка: неверный конец пакета
        //        }
        //    }
        //}




        //while (SerialRP2040.available())
        //{
        //    byte ch = (byte)SerialRP2040.read();

        //    buff[writeIndex++] = ch;
        //    bytesReceived++;
        //    if (writeIndex >= sizeof(buff))
        //    {
        //        writeIndex = 0;
        //        bytesReceived = 0;
        //        memset(buff, 0, sizeof(buff)); 
        //    }
        //    else
        //    {
        //        if (ch == 0xFF)
        //        {
        //            if (++endOfPacketCounter >= 3)
        //            {
        //                //Serial.print(ch, HEX);
        //                //Serial.println(endOfPacketCounter);
        //                ParsePacket(buff, bytesReceived);
        //                memset(buff, 0, sizeof(buff));
        //                SerialRP2040.flush();
        //                writeIndex = 0;
        //                bytesReceived = 0;
        //            }
        //        }
        //        else
        //        {
        //            endOfPacketCounter = 0;
        //        }
        //    }
        //    esp_task_wdt_reset();
        //}
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
