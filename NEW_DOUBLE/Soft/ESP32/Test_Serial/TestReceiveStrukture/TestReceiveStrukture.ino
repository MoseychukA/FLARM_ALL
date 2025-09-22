#include <Arduino.h>
#include <string.h> // Для memcpy


// Структура RAW для обмена по UART (сPacked обязательно)
struct __attribute__((packed)) ToDUMP1090_RAW {
    uint32_t  addr;
    char      squawk[5];
    char      flight[16];
    int32_t   altitude;
    int32_t   speed;
    int32_t   track;
    int32_t   vert_rate;
    float     lat_msg;
    float     lon_msg;
    int32_t   seen_time;
    char      endOfPacket[3];
};

// Оконечная структура для работы
struct ToDUMP1090 {
    uint32_t  addr;
    char      squawk[5];
    char      flight[16];
    int32_t   altitude;
    int32_t   speed;
    int32_t   track;
    int32_t   vert_rate;
    float     lat_msg;
    float     lon_msg;
    int32_t   seen_time;
};

// Функции обмена с эндian
uint32_t swap32(uint32_t val) {
    return ((val & 0xFF) << 24) |
        ((val & 0xFF00) << 8) |
        ((val & 0xFF0000) >> 8) |
        ((val & 0xFF000000) >> 24);
}

float swapFloat(float val) {
    uint32_t temp;
    memcpy(&temp, &val, 4);
    temp = swap32(temp);
    float res;
    memcpy(&res, &temp, 4);
    return res;
}

// Функция распаковки
void unpack_ToDUMP1090(const ToDUMP1090_RAW* in, ToDUMP1090* out) {
    out->addr = swap32(in->addr);
    memcpy(out->squawk, in->squawk, 5);
    memcpy(out->flight, in->flight, 16);
    out->altitude = swap32(in->altitude);
    out->speed = swap32(in->speed);
    out->track = swap32(in->track);
    out->vert_rate = swap32(in->vert_rate);
    out->lat_msg = swapFloat(in->lat_msg);
    out->lon_msg = swapFloat(in->lon_msg);
    out->seen_time = swap32(in->seen_time);
}

const size_t PACKET_SIZE = sizeof(ToDUMP1090_RAW);

#define SerialRP2040            Serial1
#define SERIAL_RP2040_SPEED     921600
#define SOC_GPIO_PIN_RP2040_RX  40
#define SOC_GPIO_PIN_RP2040_TX  41
#define SERIAL_IN_BITS          SERIAL_8N1

ToDUMP1090 packet;
ToDUMP1090_RAW inRaw; // Объявляем переменную глобально или в начале функции

#define MAX_BUFFER_SIZE 128
uint8_t rxBuffer[MAX_BUFFER_SIZE];
uint16_t rxIndex = 0;



void setup() 
{
    Serial.begin(921600); // для мониторинга
    Serial.println("Start");
    Serial1.begin(SERIAL_RP2040_SPEED, SERIAL_IN_BITS, SOC_GPIO_PIN_RP2040_RX, SOC_GPIO_PIN_RP2040_TX);
    pinMode(4, OUTPUT);
    digitalWrite(4, HIGH);
    Serial.println("Start End");

}

void loop() 
{

    if(SerialRP2040.available())
    {
        uint8_t b = SerialRP2040.read();

        if (rxIndex < MAX_BUFFER_SIZE)
        {
            rxBuffer[rxIndex++] = b;
        }
        else
        {
            // Если переполнение — сбросить
            rxIndex = 0;
        }

        // Проверка достижения размера пакета
        if (rxIndex >= PACKET_SIZE)
        {
            // Проверка маркера конца
            if (rxBuffer[PACKET_SIZE - 3] == 0xFF && rxBuffer[PACKET_SIZE - 2] == 0xFF && rxBuffer[PACKET_SIZE - 1] == 0xFF)
            {
                digitalWrite(4, LOW);
                // Весь пакет собран
                memcpy(&inRaw, rxBuffer, PACKET_SIZE);
                unpack_ToDUMP1090(&inRaw, &packet);
              //   Обработка
                Serial.print("ICAO: "); Serial.println(packet.addr, HEX);
                Serial.print("Flight: "); Serial.println(packet.flight);
                Serial.print("Squawk: "); Serial.println(packet.squawk);
                Serial.print("Altitude: "); Serial.println(packet.altitude);
                Serial.print("Speed: "); Serial.println(packet.speed);
                Serial.print("Track: "); Serial.println(packet.track);
                Serial.print("Lat: "); Serial.println(packet.lat_msg, 5);
                Serial.print("Lon: "); Serial.println(packet.lon_msg, 5);
                Serial.print("SeenTime: "); Serial.println(packet.seen_time);
                Serial.println();
                Serial.flush();
                digitalWrite(4, HIGH);


                // Очистка буфера для нового пакета
                rxIndex = 0;
            }
        }
    }

  //  vTaskDelay(pdMS_TO_TICKS(10));  // обновление 10 Гц




 //   if (Serial1.available() >= PACKET_SIZE) 
 //   {
 //       ToDUMP1090_RAW inRaw;
 //       size_t n = Serial1.readBytes((uint8_t*)&inRaw, PACKET_SIZE);
 //       if (n == PACKET_SIZE &&
 //           (uint8_t)inRaw.endOfPacket[0] == 0xFF &&
 //           (uint8_t)inRaw.endOfPacket[1] == 0xFF &&
 //           (uint8_t)inRaw.endOfPacket[2] == 0xFF
 //           ) {
 //           ToDUMP1090 packet;
 //           unpack_ToDUMP1090(&inRaw, &packet);

 //           // Вывод данных
 //           Serial.print("ICAO: "); Serial.println(packet.addr,HEX);
 ///*           Serial.print("Flight: "); Serial.println(packet.flight);
 //           Serial.print("Squawk: "); Serial.println(packet.squawk);
 //           Serial.print("Altitude: "); Serial.println(packet.altitude);
 //           Serial.print("Speed: "); Serial.println(packet.speed);
 //           Serial.print("Track: "); Serial.println(packet.track);
 //           Serial.print("Lat: "); Serial.println(packet.lat_msg, 5);
 //           Serial.print("Lon: "); Serial.println(packet.lon_msg, 5);
 //           Serial.print("SeenTime: "); Serial.println(packet.seen_time);*/
 //           Serial.flush();
 //       }
 //   }
}
