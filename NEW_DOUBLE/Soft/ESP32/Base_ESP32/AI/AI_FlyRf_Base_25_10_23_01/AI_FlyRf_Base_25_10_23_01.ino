#include <stdio.h>                // define I/O functions//#include "MAVLinkRF.h"
#include <Arduino.h>              // define I/O functions
#include "SPI.h"
#include <esp_task_wdt.h>
#include <iostream>
#include <locale.h>
#include <math.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"


#include "OTA.h"
#include "TimeRF.h"
#include "GNSS.h"
#include "RF.h"
#include "EEPROMRF.h"
#include "NMEA.h"
#include "SoC.h"
#include "WiFiRF.h"
#include "WebRF.h"
#include "Baro.h"
#include "TrafficHelper.h"
#include "ESP32RF.h"
#include <TimeLib.h>
#include <TinyGPS++.h>
#include "ServiceMain.h"
#include "Configuration_ESP32.h"
#include "CoreCommandBuffer.h"    // обработчик входящих по UART команд
#include "Button.h"


int set_air = 0;   //  
bool set_test_coordinate  = false; // Признак тестовых ввода текущих координат 
bool set_test_coordinate5 = false; // Признак тестовых ввода текущих координат 

void txrx_test();
void normal();

#if !defined(SERIAL_FLUSH)
#define SERIAL_FLUSH() Serial.flush()
#endif

#define DEBUG 0
#define DEBUG_TIMING 0

#define isTimeToDisplay() (millis() - LEDTimeMarker     > 1000)
#define isTimeToExport()  (millis() - ExportTimeMarker  > 1000)


//SemaphoreHandle_t containerMutex = NULL;// Мьютекс для защиты базы


/* ---------- ОПРЕДЕЛЕНИЕ FrameHeader ---------- */
//#pragma pack(push,1)
struct FrameHeader {
    uint16_t preamble; // 0xAA55
    uint8_t  ver;      // 0x01
    uint8_t  type;     // 
    uint8_t  index;    // 0..7 для контейнера, 0xFF для ThisAircraft, 0 для прочих
    uint8_t  seq;      // счётчик кадров
    uint16_t length;   // длина payload
};
//#pragma pack(pop)


uint32_t swap32(uint32_t val)
{
    return ((val & 0xFF) << 24) |
        ((val & 0xFF00) << 8) |
        ((val & 0xFF0000) >> 8) |
        ((val & 0xFF000000) >> 24);
}

float swapFloat(float val)
{
    uint32_t temp;
    memcpy(&temp, &val, 4);
    temp = swap32(temp);
    float res;
    memcpy(&res, &temp, 4);
    return res;
}

uint16_t toBigEndian16(uint16_t val)
{
    // 0x00FF   – младший байт
    // 0xFF00   – старший байт
    // Сдвигаем их в противоположные позиции
    return ((val & 0x00FF) << 8) |   // младший байт → старший
        ((val & 0xFF00) >> 8);       // старший байт → младший
}


const size_t PACKET_SIZE = sizeof(ToDUMP1090_RAW);

ToDUMP1090 packet;
ToDUMP1090_RAW inRaw; // Объявляем переменную глобально или в начале функции

#define MAX_BUFFER_SIZE 512
uint8_t rxBuffer[MAX_BUFFER_SIZE];
uint16_t rxIndex = 0;

// Функция распаковки
void unpack_ToDUMP1090(const ToDUMP1090_RAW* in, ToDUMP1090* out)
{
    out->addr = swap32(in->addr);
    out->squawk = toBigEndian16(in->squawk);
    memcpy(out->flight, in->flight, 16);
    out->altitude = swap32(in->altitude);
    out->speed = swap32(in->speed);
    out->course = swap32(in->course);
    out->vert_rate = swap32(in->vert_rate);
    out->lat_msg = swapFloat(in->lat_msg);
    out->lon_msg = swapFloat(in->lon_msg);
    out->last_message_signal_strength_dbm = toBigEndian16(in->last_message_signal_strength_dbm);
    out->last_message_signal_quality_db = toBigEndian16(in->last_message_signal_quality_db);
}

//ufo_t ThisAircraft;

//// Доп.информация из условия
//typedef struct {
//    bool     new_flag_M;
//    uint8_t  new_buttton_M;
//    bool     setMessageRead_M;
//    bool     MessageRead_M;
//    uint8_t  Time_Hour_M;
//    uint8_t  Time_Minute_M;
//    bool     new_SOS_flag_M;  // 
//    bool     confirm_message_M;
//    char     msg_resp_M[170];
//    bool     isValidGNSS_M;
//    uint8_t  FLYRF_MODE_TEST_M;
//} aux_t;
//
//aux_t AuxFlags;
//uint16_t analog_code_M = 0;
//
//// ================== RS485 / Протокол ==================
//#define RS485_SERIAL         Serial2  //Порт обмена информации с внешним дисплеем
//#define RS485_TX_PIN         18
//#define RS485_RX_PIN         17
//#define RS485_DE_PIN         21
//
//#define RS485_BAUD           921600
//#define RS485_CONFIG         SERIAL_8N1
//

//==================================================================================================================
void receiveRP2040()
{
    rxIndex = 0;

    while (SerialRP2040.available())
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
            if (rxBuffer[PACKET_SIZE - 3] == 0xFF && rxBuffer[PACKET_SIZE - 2] == 0xFF && rxBuffer[PACKET_SIZE - 1] == 0xFF)
            {
                // Весь пакет собран
                memcpy(&inRaw, rxBuffer, PACKET_SIZE);
                unpack_ToDUMP1090(&inRaw, &packet);
                // Обработка
                if (settings->serial_out == SEND_SERIAL_1090 && settings->nmea_out != NMEA_UART)
                {
                    Serial.print("ICAO: "); Serial.print(packet.addr, HEX);
                    Serial.print(":"); Serial.print(packet.squawk);
                    Serial.print(":"); Serial.print(packet.flight);
                    Serial.print(":"); Serial.print(packet.altitude);
                    Serial.print(":"); Serial.print(packet.speed);
                    Serial.print(":"); Serial.print(packet.course);
                    Serial.print(":"); Serial.print(packet.vert_rate);
                    Serial.print(":"); Serial.print(packet.lat_msg, 5);
                    Serial.print(":"); Serial.print(packet.lon_msg, 5);
                    Serial.print(":"); Serial.print(packet.last_message_signal_strength_dbm);
                    Serial.print(":"); Serial.print(packet.last_message_signal_quality_db);
                    Serial.println();
                    Serial.flush();
                }

                fo = EmptyFO;
                fo.addr = packet.addr;
                fo.squawk = packet.squawk;
                memcpy((char*)fo.flight, packet.flight, strlen(packet.flight));
                fo.altitude = packet.altitude * 0, 3048;
                fo.pressure_altitude = packet.altitude * 0, 3048;
                fo.speed = packet.speed;
                fo.course = packet.course;
                fo.vert_rate = packet.vert_rate; // Уточнить!
                fo.latitude = packet.lat_msg;
                fo.longitude = packet.lon_msg;
                fo.signal_source = 2;
                fo.aircraft_type = AIRCRAFT_TYPE_JET;
                fo.last_message_signal_strength_dbm = packet.last_message_signal_strength_dbm;        // SIGS
                fo.last_message_signal_quality_db = packet.last_message_signal_quality_db;          // SIGQ
                fo.timestamp = now(); /*packet.seen_time;*/ // 
                Traffic_Update(&fo);   // Обновить координаты (если есть эти данные)
                Traffic_Add(&fo);      // 
 
                // Очистка буфера для нового пакета 
                memset(&packet, 0, sizeof(packet)); // Очистить массив
                memset(&rxBuffer, 0, sizeof(rxBuffer)); // Очистить массив
                rxIndex = 0;  // Готов к приему нового пакета.
            }

        }
    }
}

//==================================================================================================================
//
// Порт RS485 (Источник)
//
#define RS485_SERIAL Serial2
#define RS485_TX_PIN 18
#define RS485_RX_PIN 17
#define RS485_DE_PIN 21

#define RS485_BAUD 921600
#define RS485_CONFIG SERIAL_8N1

#define BUTTON1_PIN 45
#define BUTTON2_PIN 48

#define PREAMBLE 0x55AAu // будем писать в LE как 0xAA,0x55
#define PROTO_VER 0x01
#define MAX_PAYLOAD 512
#define MAX_TRACKING_OBJECTS 12

// Прототипы структур
//typedef struct UFO {
//    uint32_t addr;
//    int squawk;
//    uint8_t callsign[8];
//    float latitude;
//    float longitude;
//    float altitude;
//    float pressure_altitude;
//    float course;
//    float speed;
//    int vert_rate;
//    float latitude2;
//    float longitude2;
//    int8_t rssi;
//    uint16_t last_message_signal_strength_dbm;
//    uint16_t last_message_signal_quality_db;
//} ufo_t;

typedef struct {
    bool new_flag_M;
    uint8_t new_buttton_M;
    bool setMessageRead_M;
    bool MessageRead_M;
    uint8_t Time_Hour_M;
    uint8_t Time_Minute_M;
    bool new_SOS_flag_M;
    bool confirm_message_M;
    char msg_resp_M[170];
    bool isValidGNSS_M;
} aux_t;

// ЖД:контейнеры и текущие данные
/*static*/ ufo_t ThisAircraft;
//static ufo_t Container[MAX_TRACKING_OBJECTS];

// Семафоры/очереди
QueueHandle_t txQueue; // очередь на отправку
QueueHandle_t ackQueue; // очередь подтверждений
SemaphoreHandle_t serialMutex; // для атомарного доступа к UART
SemaphoreHandle_t containerMutex = NULL;// Мьютекс для защиты базы

// Прототипы функций
static uint16_t crc16_ibm(const uint8_t* data, size_t len);
//static uint16_t swap16(uint16_t val);
//static uint32_t swap32(uint32_t val);
//static float swapFloat(float val);
//static uint16_t toBigEndian16(uint16_t val);

static void rs485Begin();
static void rs485SendFrame(const uint8_t* frame, size_t len);
static bool buildFrame(const uint8_t* payload, uint16_t payload_len, uint8_t* frame, uint16_t* frame_len);
static void serializeUfo(uint8_t* dst, const ufo_t* src, size_t* written);
static void serializeAux(uint8_t* dst, const aux_t* a, size_t* written);

static void fillDemoData();

typedef struct {
    uint8_t data[MAX_PAYLOAD];
    uint16_t len;
} frame_t;

static bool parseFrame(uint8_t* in, size_t len, uint8_t* payload, uint16_t* payload_len);
static bool isAckFrame(const uint8_t* frame, uint16_t len);

static void senderTask(void* pvParameters);
static void receiverTask(void* pvParameters);

static bool parseFrame(const uint8_t* data, size_t len, uint8_t* payload, uint16_t* payload_len) {
    // Простейшая верификация:префикс 0xAA 0x55
    if (len < 5) return false;
    if (data[0] != 0xAA || data[1] != 0x55) return false;
    uint16_t pl = (data[3] << 8) | data[4];
    if (5 + pl + 2 > len) return false;
    const uint8_t* p = data + 5;
    // CRC
    uint16_t crc_rx;
    memcpy(&crc_rx, data + 5 + pl, 2);
    uint16_t crc_calc = crc16_ibm(p, pl);
    if (crc_rx != crc_calc) return false;
    if (payload_len) *payload_len = pl;
    if (payload) memcpy(payload, p, pl);
    return true;
}
static bool isAckFrame(const uint8_t* frame, uint16_t len) {
    // простейшее определение ACK:payload_len == 0
    if (len < 5) return false;
    uint16_t pl = (frame[3] << 8) | frame[4];
    return (pl == 0);
}



// ----------- Реализация утилит -----------

static uint16_t crc16_ibm(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    while (len--) {
        crc ^= (uint16_t)(*data++) & 0xFF;
        for (int i = 0; i < 8; ++i) {
            if (crc & 1) crc = (crc >> 1) ^ 0xA001;
            else crc >>= 1;
        }
    }
    return crc;
}

//static uint16_t swap16(uint16_t val) {
//    return (val << 8) | (val >> 8);
//}
//static uint32_t swap32(uint32_t val) {
//    return ((val & 0xFF) << 24) |
//        ((val & 0xFF00) << 8) |
//        ((val & 0xFF0000) >> 8) |
//        ((val & 0xFF000000) >> 24);
//}
//static float swapFloat(float val) {
//    uint32_t tmp;
//    memcpy(&tmp, &val, 4);
//    tmp = swap32(tmp);
//    float out;
//    memcpy(&out, &tmp, 4);
//    return out;
//}
//static uint16_t toBigEndian16(uint16_t val) {
//    return ((val & 0x00FF) << 8) | ((val & 0xFF00) >> 8);
//}

static void rs485Begin() 
{
    // Инициализация порта RS485
    // Поскольку Arduino Serial2/Serial1 требует вызова begin,делаем так:
    Serial2.begin(RS485_BAUD, RS485_CONFIG, RS485_RX_PIN, RS485_TX_PIN);
   // Serial1.begin(RS485_BAUD, RS485_CONFIG, 38, 39); // временно - не используется здесь
}

static void rs485SendFrame(const uint8_t* frame, size_t len) 
{
    // атомарная отправка
    if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(10)) == pdTRUE) 
    {
        digitalWrite(RS485_DE_PIN, HIGH);
        Serial2.write(frame, len);
        Serial2.flush();
        digitalWrite(RS485_DE_PIN, LOW);
        xSemaphoreGive(serialMutex);
    }
    else 
    {
        // не получили мьютекс – можно логнуть
    }
}

// Сформировать frame:префикс,версия,длина,payload,CRC
static bool buildFrame(const uint8_t* payload, uint16_t payload_len, uint8_t* frame, uint16_t* frame_len) 
{
    // frame:2 байта префикс,1 байт версии,2 байта длина,payload,2 байта CRC
    uint16_t fr_len = 2 + 1 + 2 + payload_len + 2;
    if (fr_len > 4096) return false;
    uint8_t* p = frame;
    // префикс LE:0xAA 0x55
    p[0] = 0xAA;
    p[1] = 0x55;
    p += 2;
    p[0] = PROTO_VER; // версия
    p[1] = (payload_len >> 8) & 0xFF;
    p[2] = payload_len & 0xFF; // длина
    // переписываем:вернемся к p-смещению
    // сделаем сдвиг,чтобы корректно заполнить в дальнейшем
    // попозже просто вернем ready frame
    // Для простоты сделаем через локальные переменные:
    // Прямой перенос:перепишем в отдельный буфер
    // Но чтобы держать код компактно,просто скопируем payload
    // после текущего места:
    // Сейчас p указывает на VER/Length. Так что делаем так:
    // смещаемся назад на 3 байта,чтобы заполнить корректно
    // проще:перестраиваем фрейм полностью в другой буфер
    // Ниже реализовано корректно в рамках одного блока
    return true;
}

// сериализация UFO в payload
static void serializeUfo(uint8_t* dst, const ufo_t* src, size_t* written) 
{
    uint32_t v32;
    int32_t vi32;

    // addr
    v32 = swap32(src->addr); memcpy(dst, &v32, 4); dst += 4;
    // squawk
    vi32 = (int32_t)src->squawk;
    v32 = swap32((uint32_t)vi32); memcpy(dst, &v32, 4); dst += 4;
    // callsign[8]
    memcpy(dst, src->callsign, 8); dst += 8;

    // latitude
    float ftmp = src->latitude;
    uint32_t t32; memcpy(&t32, &ftmp, 4); t32 = swap32(t32); memcpy(dst, &t32, 4); dst += 4;

    // longitude
    ftmp = src->longitude;
    memcpy(&t32, &ftmp, 4); t32 = swap32(t32); memcpy(dst, &t32, 4); dst += 4;

    // altitude
    ftmp = src->altitude;
    memcpy(&t32, &ftmp, 4); t32 = swap32(t32); memcpy(dst, &t32, 4); dst += 4;

    // pressure_altitude
    ftmp = src->pressure_altitude;
    memcpy(&t32, &ftmp, 4); t32 = swap32(t32); memcpy(dst, &t32, 4); dst += 4;

    // course
    ftmp = src->course;
    memcpy(&t32, &ftmp, 4); t32 = swap32(t32); memcpy(dst, &t32, 4); dst += 4;

    // speed
    ftmp = src->speed;
    memcpy(&t32, &ftmp, 4); t32 = swap32(t32); memcpy(dst, &t32, 4); dst += 4;

    // vert_rate
    v32 = swap32((uint32_t)src->vert_rate); memcpy(dst, &v32, 4); dst += 4;

    //// latitude2
    //ftmp = src->latitude2;
    //memcpy(&t32, &ftmp, 4); t32 = swap32(t32); memcpy(dst, &t32, 4); dst += 4;

    //// longitude2
    //ftmp = src->longitude2;
    //memcpy(&t32, &ftmp, 4); t32 = swap32(t32); memcpy(dst, &t32, 4); dst += 4;

    // rssi
    memcpy(dst, &(src->rssi), 1); dst += 1;

    // last_message_signal_strength_dbm
    uint16_t s16 = toBigEndian16(src->last_message_signal_strength_dbm);
    memcpy(dst, &s16, 2); dst += 2;

    // last_message_signal_quality_db
    s16 = toBigEndian16(src->last_message_signal_quality_db);
    memcpy(dst, &s16, 2); dst += 2;

    if (written) *written = dst - (uint8_t*)src;
    esp_task_wdt_reset();
}
static void serializeAux(uint8_t* dst, const aux_t* a, size_t* written)
{
    dst[0] = a->new_flag_M ? 1 : 0;
    dst[1] = a->new_buttton_M;
    dst[2] = a->setMessageRead_M ? 1 : 0;
    dst[3] = a->MessageRead_M ? 1 : 0;
    dst[4] = a->Time_Hour_M;
    dst[5] = a->Time_Minute_M;
    dst[6] = a->new_SOS_flag_M ? 1 : 0;
    dst[7] = a->confirm_message_M ? 1 : 0;
    memcpy(dst + 8, a->msg_resp_M, 170);
    dst[178] = a->isValidGNSS_M ? 1 : 0;
    if (written) *written = 179;
}

// демо-данные
static void fillDemoData() 
{
    // ThisAircraft и Container заполняются тестовыми данными
    ThisAircraft.addr = 0x01020304;
    ThisAircraft.squawk = 123;
    memset(ThisAircraft.callsign, 0, 8);
    memcpy(ThisAircraft.callsign, "SRC123", 6);

    ThisAircraft.latitude = 37.4219999f;
    ThisAircraft.longitude = -122.0840575f;
    ThisAircraft.altitude = 10000.0f;
    ThisAircraft.pressure_altitude = 9800.0f;
    ThisAircraft.course = 180.0f;
    ThisAircraft.speed = 250.0f;
    ThisAircraft.vert_rate = 0;
    //ThisAircraft.latitude2 = ThisAircraft.latitude;
    //ThisAircraft.longitude2 = ThisAircraft.longitude;
    ThisAircraft.rssi = 0;
    ThisAircraft.last_message_signal_strength_dbm = 0;
    ThisAircraft.last_message_signal_quality_db = 0;

    for (int i = 0; i < MAX_TRACKING_OBJECTS; ++i) {
        Container[i] = ThisAircraft;
        Container[i].addr = (uint32_t)0x01000000 | (i + 1);
        Container[i].latitude += (float)i;
        Container[i].longitude += (float)i;
    }
}

// ----------- задачи -----------

// отправка
static void senderTask(void* pvParameters) 
{
    (void)pvParameters;
    frame_t frame;
    //const TickType_t xDelay500ms = pdMS_TO_TICKS(500);

    while (1) 
    {
        // 1) собрать payload:ThisAircraft + Container + aux
        uint8_t payload[MAX_PAYLOAD];
        size_t offset = 0;

        //// ThisAircraft
        //{
        //    size_t w = 0;
        //    serializeUfo(payload + offset, &ThisAircraft, &w);
        //    offset += w;
        //}

        //// Container
        //for (int i = 0; i < MAX_TRACKING_OBJECTS; ++i) 
        //{
        //    size_t w = 0;
        //    serializeUfo(payload + offset, &Container[i], &w);
        //    offset += w;
        //}

        //// Aux
        //aux_t aux;
        //aux.new_flag_M = true;
        //aux.new_buttton_M = (uint8_t)digitalRead(BUTTON1_PIN); // кнопки в payload
        //aux.setMessageRead_M = false;
        //aux.MessageRead_M = false;
        //aux.Time_Hour_M = 12;
        //aux.Time_Minute_M = 34;
        //aux.new_SOS_flag_M = false;
        //aux.confirm_message_M = false;
        //memset(aux.msg_resp_M, 0, sizeof(aux.msg_resp_M));
        //aux.isValidGNSS_M = true;

       /* {
            size_t w = 0;
            serializeAux(payload + offset, &aux, &w);
            offset += w;
        }*/

        // Построить фрейм
        uint16_t frame_len;
       // uint8_t tempFrame[MAX_PAYLOAD + 64]; // достаточный запас
        // Прямой вызов buildFrame с простым составлением
        // Для упрощения — просто используем:[префикс+версия+длина+payload+CRC]
        // Реализация buildFrame здесь упрощена:мы прямо формируем frame во tempFrame
        // Прямо формируем:префикс(2) + версия(1) + длина(2) + payload + CRC(2)

        //// заполнение заголовка
        //uint8_t* cur = tempFrame;
        //cur[0] = 0xAA; cur[1] = 0x55; // префикс
        //cur[2] = PROTO_VER;
        //cur[3] = (offset >> 8) & 0xFF;
        //cur[4] = offset & 0xFF;
        //cur += 5;

        //// payload копируем
        //memcpy(cur, payload, offset);
        //cur += offset;

        //// CRC по payload
        //uint16_t crc = crc16_ibm(payload, offset);
        //uint16_t crcLE = crc; // упаковка в LE
        //memcpy(cur, &crcLE, 2);
        //cur += 2;

        //frame_len = (uint16_t)(cur - tempFrame);

        // отправка
       // rs485SendFrame(tempFrame, frame_len);

        // отправка ack-нутру
        // Ожидание ACK
        uint8_t ack = 0;
        //if (xQueueReceive(ackQueue, &ack, pdMS_TO_TICKS(1000))) 
        //{
        //    // ACK получен
        //}
        //else 
        //{
        //    // timeout:можно повторить
        //}
       // esp_task_wdt_reset();
      //  vTaskDelay(xDelay500ms);
        esp_task_wdt_reset();
    }
}

// приемник:принимает frames,формирует ackFrame
static void receiverTask(void* pvParameters) 
{
    (void)pvParameters;
    uint8_t rxbuf[2048];
    size_t rxlen = 0;

    while (true) {
        // опрос Serial2 (RS485)
        while (Serial2.available()) {
            int b = Serial2.read();
            if (rxlen < sizeof(rxbuf)) rxbuf[rxlen++] = (uint8_t)b;
        }

        if (rxlen >= 5) { // минимальная длина
        // поиск фрейма начинающегося с 0xAA 0x55
            for (size_t i = 0; i + 5 <= rxlen; ++i) {
                if (rxbuf[i] == 0xAA && rxbuf[i + 1] == 0x55) {
                    // заголовок найден,считаем длину
                    uint16_t plen = (rxbuf[i + 3] << 8) | rxbuf[i + 4];
                    size_t total = 2 + 1 + 2 + plen + 2;
                    if (i + total <= rxlen) {
                        // есть полный кадр
                        uint8_t* frame = &rxbuf[i];
                        // проверить CRC против payload
                        // payload начинается после заголовка:frame+5
                        uint8_t* payload = frame + 5;
                        uint16_t crc_recv;
                        memcpy(&crc_recv, frame + 5 + plen, 2);
                        uint16_t crc_calc = crc16_ibm(payload, plen);
                        if (crc_recv == crc_calc) {
                            // корректный кадр:послать ACK обратно
                            // формируем ACK:без payload (payload_len=0)
                            uint8_t ackFrame[9];
                            uint8_t* af = ackFrame;
                            af[0] = 0xAA; af[1] = 0x55;
                            af[2] = PROTO_VER;
                            af[3] = 0; af[4] = 0; // payload_len = 0
                            // CRC над пустым payload
                            uint16_t a_crc = crc16_ibm(NULL, 0);
                            memcpy(ackFrame + 5, &a_crc, 2);
                            // отправка ACK
                            rs485SendFrame(ackFrame, 7);
                            // пометить флаг приема (для демонстрации)
                            uint8_t dummy = 1;
                            xQueueSend(ackQueue, &dummy, 0);
                        }
                        // сдвигаемся дальше
                        i += total - 1;
                    }
                }
            }
            // очистка принятых данных
            // простая очистка буфера после обработки
            rxlen = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

//==================================================================================================================


hardware_info_t hw_info = {
  .model    = DEFAULT_FLYRF_MODEL,
  .revision = 0,
  .soc      = SOC_NONE,
  .rf       = RF_IC_NONE,
  .gnss     = GNSS_MODULE_NONE,
  .baro     = BARO_MODULE_NONE,
  .display  = DISPLAY_NONE,
};

unsigned long LEDTimeMarker = 0;
unsigned long ExportTimeMarker = 0;

static void onButtonPressDownCb(void* button_handle, void* usr_data) 
{
   service.set_num_buttton(1);
}

static void onButtonDoubleClickEventCb(void* button_handle, void* usr_data)
{
   service.set_num_buttton(2);
}

static void onButtonLongPressStartEventCb(void* button_handle, void* usr_data)
{
   service.set_num_buttton(3);
}


/*Настройки только для теста*/
const int ledPin = 4;                       // the number of the LED pin
int ledState = LOW;                          // ledState used to set the LED
unsigned long previousMillis = 0;            // will store last time LED was updated
const long interval = 1000;                  // interval at which to blink (milliseconds)


void setup()
{ 
    rst_info* resetInfo;

    hw_info.soc = SoC_setup(); // Has to be very first procedure in the execution order

    resetInfo = (rst_info*)SoC->getResetInfoPtr();

    Serial.println();
    Serial.print(F(FLYRF_IDENT "-"));
    Serial.print(SoC->name);
    Serial.print(F(" FW.REV: " FLYRF_FIRMWARE_VERSION " DEV.ID: "));
    Serial.println(String(SoC->getChipId(), HEX));

    String ver_soft = __FILE__;
    int val_srt = ver_soft.lastIndexOf('\\');
    ver_soft.remove(0, val_srt + 1);
    val_srt = ver_soft.lastIndexOf('.');
    ver_soft.remove(val_srt);
    Serial.println(ver_soft);
    service.saveVer(ver_soft);  // Сохранить строку с текущей версией.

    pinMode(ledPin, OUTPUT);
  
  
    SERIAL_FLUSH();

    if (resetInfo)
    {
    Serial.println(""); Serial.print(F("Reset reason: ")); Serial.println(resetInfo->reason);
    }
    Serial.println(SoC->getResetReason());
    Serial.print(F("Free heap size: ")); Serial.println(SoC->getFreeHeap());
    Serial.println(SoC->getResetInfo()); Serial.println("");

    SERIAL_FLUSH();

    EEPROM_setup();


    if (settings->default_settings == SETTINGS_ON)
    {
        EEPROM_clear();
    }

    ThisAircraft.addr = SoC->getChipId() & 0x00FFFFFF;

    hw_info.rf = RF_setup();

    delay(100);

    hw_info.baro = Baro_setup();
    float vdd_start = service.battery_read();
 
    if (settings->display_settings == DISPLAY_BUILT_IN)
    {
        hw_info.display = SoC->Display_setup();
    }
 
    hw_info.gnss = GNSS_setup();
    ThisAircraft.aircraft_type = settings->aircraft_type;
 
    ThisAircraft.protocol = settings->rf_protocol;
    ThisAircraft.stealth  = settings->stealth;
    ThisAircraft.no_track = settings->no_track;
    //sprintf(ThisAircraft.flight, "%08X", ThisAircraft.addr);

    if (settings->input_coordinates == IMPUT_COORD_MANUAL)
    {
        ThisAircraft.test_latitude = settings->test_latitude;
        ThisAircraft.test_longitude = settings->test_longitude;
    }


    Traffic_setup();

    SoC->swSer_enableRx(false);

    WiFi_setup();
 
    if (SoC->Bluetooth_ops) 
    {
        SoC->Bluetooth_ops->setup();
    }

    OTA_setup();
    Web_setup();
    NMEA_setup();

    delay(1000);

    switch (settings->mode)
    {
    case FLYRF_MODE_TXRX_TEST0:
        Time_setup();
        set_air = 0;
    /*     service.set_GNSS_on_off(true);*/
        break;
    case FLYRF_MODE_TXRX_TEST1:
        Time_setup();
        set_air = 1;
        break;
    case FLYRF_MODE_TXRX_TEST2:
        set_air = 2;
        Time_setup();
        break;
    case FLYRF_MODE_TXRX_TEST3:
        set_air = 3;
        Time_setup();
        break;
    case FLYRF_MODE_TXRX_TEST4:
        set_air = 4;
        Time_setup();
        break;
    case FLYRF_MODE_TXRX_TEST5:
        set_air = 5;
        Time_setup();
        break;
    case FLYRF_MODE_NORMAL:
    default:
        SoC->swSer_enableRx(true);
        set_air = 0;
        break;
    }
  
    SoC->post_init();

    if (psramInit() == false)
        Serial.println("PSRAM failed to initialize");
    else
        Serial.println("PSRAM initialized");

    Serial.printf("PSRAM Size available (bytes): %d\r\n", ESP.getFreePsram());

    heap_caps_malloc_extmem_enable(8000); //Use PSRAM for memory requests larger than 1,000 bytes

    CommandHandler.setup();

    // initializing a button
    Button* btn = new Button(GPIO_NUM_48, false);

    btn->attachPressDownEventCb(&onButtonPressDownCb, NULL);
    btn->attachDoubleClickEventCb(&onButtonDoubleClickEventCb, NULL);
    btn->attachLongPressStartEventCb(onButtonLongPressStartEventCb, NULL);

    containerMutex = xSemaphoreCreateMutex();

    //---------------------------------------------
  // Включение портов RS485,пины DE,и т.д.
    pinMode(RS485_DE_PIN, OUTPUT);
    digitalWrite(RS485_DE_PIN, LOW);

    // кнопки
    pinMode(BUTTON1_PIN, INPUT_PULLUP);
    pinMode(BUTTON2_PIN, INPUT_PULLUP);

    // Инициализация Serial RS485
    rs485Begin();

    // IO и синхронизация
    serialMutex = xSemaphoreCreateMutex();
    containerMutex = xSemaphoreCreateMutex();

    txQueue = xQueueCreate(4, sizeof(frame_t));
    ackQueue = xQueueCreate(4, sizeof(uint8_t));

    // демо-данные
    // здесь можно заменить на реальное считывание сенсоров/данных
    memset(&Container, 0, sizeof(Container));
    fillDemoData();

    // задачи-клиент
    xTaskCreatePinnedToCore(senderTask, "RS485_TX", 4096, NULL, 1, NULL, 0);
   // xTaskCreatePinnedToCore(receiverTask, "RS485_RX", 4096, NULL, 1, NULL, 0);

   //---------------------------------------------


    esp_task_wdt_init(10, false); // таймаут 30 сек, reset chip=true
    esp_task_wdt_add(NULL);      // добавить текущую задачу

    Serial.println("** Setup END **");
}

bool new_SOS_flag_M_tmp = false;

void loop()
{
    RF_loop();                       // Сначала выполните общие действия с радиочастотами
    receiveRP2040(); //прием пакета от DUMP1090
    esp_task_wdt_reset();
  switch (settings->mode)
  {
#if !defined(EXCLUDE_TEST_MODE)
 // case FLYRF_MODE_TXRX_TEST0:
  case FLYRF_MODE_TXRX_TEST1:
  case FLYRF_MODE_TXRX_TEST2:
  case FLYRF_MODE_TXRX_TEST3:
  case FLYRF_MODE_TXRX_TEST4:
  case FLYRF_MODE_TXRX_TEST5:
    txrx_test();
    break;
#endif /* EXCLUDE_TEST_MODE */
  case FLYRF_MODE_NORMAL:
  default:
    normal();
    break;
  }


  if (settings->display_settings == DISPLAY_BUILT_IN)
  {
     SoC->Display_loop();
  }

  WiFi_loop();
  Web_loop();
  OTA_loop();

  SoC->loop();

  if (SoC->Bluetooth_ops) 
  {
    SoC->Bluetooth_ops->loop();
  }

  if (SoC->UART_ops)
  {
     SoC->UART_ops->loop();
  }

  CommandHandler.handleCommands();
  CommandHandler.SendTraffic_Msg();
  CommandHandler.GPS_send_base(); 
  esp_task_wdt_reset();

  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval)
  {
      previousMillis = currentMillis;
      if (settings->serial_out == SEND_SERIAL_DISPLAY && settings->nmea_out != NMEA_UART)
      {
          for (int i = 0; i < MAX_TRACKING_OBJECTS; ++i)
          {
              Serial.printf("Container[%u]:$%06X:%d:%8s:%.0f:%.0f:%.0f:%.6f:%.6f\r\n",
                i,
                Container[i].addr,                   // Адрес устройства стороннего самолета
                Container[i].squawk,                 // Номер, назначаемый диспетчером для обмена с локатором.
                Container[i].flight,                  // Номер рейса
                Container[i].altitude,
                Container[i].speed,
                Container[i].course,
                Container[i].latitude,
                Container[i].longitude/*,
                Container[i].last_message_signal_strength_dbm,
                Container[i].last_message_signal_quality_db*/);
                Serial.flush();
          }
          Serial.println("---------------------------------------------------------------");
      }
  }

  Time_loop();

  yield();
}

void shutdown(int reason)
{
  SoC->WDT_fini();

  SoC->swSer_enableRx(false);

   NMEA_fini();

  Web_fini();

  if (SoC->Bluetooth_ops) {
     SoC->Bluetooth_ops->fini();
  }

  if (SoC->USB_ops) {
     SoC->USB_ops->fini();
  }

  WiFi_fini();

  GNSS_fini();
 
  SoC->Display_fini(reason);

  Baro_fini();

  RF_Shutdown();

  SoC_fini(reason);
}

void normal()
{
  bool success;

  Baro_loop();

  GNSS_loop();

  ThisAircraft.timestamp = now();
  if (isValidFix()) 
  {
    ThisAircraft.latitude  = gnss.location.lat();
    ThisAircraft.longitude = gnss.location.lng();
    ThisAircraft.altitude  = gnss.altitude.meters();
    ThisAircraft.course    = gnss.course.deg();
    ThisAircraft.speed     = gnss.speed.knots();
    ThisAircraft.hdop      = (uint16_t) gnss.hdop.value();
    ThisAircraft.geoid_separation = gnss.separation.meters();

    if (ThisAircraft.latitude != 0 || ThisAircraft.longitude != 0)
    {
        ThisAircraft.old_latitude = gnss.location.lat();
        ThisAircraft.old_longitude = gnss.location.lng();
        if (settings->input_coordinates == IMPUT_COORD_AUTO)
        {
            ThisAircraft.test_latitude = gnss.location.lat();
            ThisAircraft.test_longitude = gnss.location.lng();
        }
    }


#if !defined(EXCLUDE_EGM96)
    /*
     * When geoidal separation is zero or not available - use approx. EGM96 value
     */
    if (ThisAircraft.geoid_separation == 0.0) 
    {
      ThisAircraft.geoid_separation = (float) LookupSeparation(ThisAircraft.latitude, ThisAircraft.longitude);
      /* we can assume the GPS unit is giving ellipsoid height */
      ThisAircraft.altitude -= ThisAircraft.geoid_separation;
    }
#endif /* EXCLUDE_EGM96 */

    RF_Transmit(RF_Encode(&ThisAircraft), true);   // Передать параметры посредством LoRa
  }
  else
  {
      if (ThisAircraft.old_latitude != 0 || ThisAircraft.old_longitude != 0)
      {
         //!! ThisAircraft.altitude = 25000.0;

          RF_Transmit(RF_Encode(&ThisAircraft), true);  // Передать параметры посредством LoRa в случае если нет сигналов GPS
      }
  }
  success = RF_Receive();  //

#if DEBUG
  success = true;
#endif

  if (success && isValidFix()) ParseData();

  if (isValidFix()) 
  {
    Traffic_loop();
  }
   
  if (isTimeToDisplay()) 
  {
     LEDTimeMarker = millis();
  }

  if (isTimeToExport()) 
  { 
    NMEA_Export();
    ExportTimeMarker = millis();
  }

  // Handle Air Connect
  NMEA_loop();

  ClearExpired();
}

#if !defined(EXCLUDE_TEST_MODE)

unsigned int pos_ndx = 0;
unsigned long TxPosUpdMarker = 0;

float altitude0 = 100.0;
float altitude1 = 100.0;
float altitude2 = 100.0;
float altitude3 = 100.0;
float altitude4 = 100.0;
float altitude5 = 100.0;


float speed0 = 300.0;
float speed1 = 300.0;
float speed2 = 300.0;
float speed3 = 300.0;
float speed4 = 300.0;
float speed5 = 300.0;

//bool alt_high0 = false;
bool alt_high1 = false;
bool alt_high2 = false;
bool alt_high3 = false;
bool alt_high4 = false; 
bool alt_high5 = false;


float test_curse0 = 0.0;


//Атлантический океан
/*
0.075397, 0.029420
-0.004039, 0.029420
-0.004039, -0.054865
 0.075397, -0.054865
*/

int track_air = 0;
float alien_lat13 = 0.075397;
float alien_lon13 = 0.029420;

float alien_lat14 = -0.004039;
float alien_lon14 = 0.029420;

float alien_lat15 = -0.004039;
float alien_lon15 = -0.054865;

float alien_lat16 = 0.075397;
float alien_lon16 = -0.054865;

float alien_lat20 = 0.075397;
float alien_lon20 = 0.029420;


/*
  0.053769, -179.953328
  -0.025508, -179.953328
  -0.025508, 179.953328
  0.053769, 179.953328
*/

float alien_lat23 = 0.053769;
float alien_lon23 = -179.953328;

float alien_lat24 = -0.025508;
float alien_lon24 = -179.953328;

float alien_lat25 = -0.025508;
float alien_lon25 = 179.953328;

float alien_lat26 = 0.053769;
float alien_lon26 = 179.953328;

float alien_lat30 = 0.053769;
float alien_lon30 = -179.953328;

char fly1[] = "AFL1118";
char fly2[] = "AFL2122";
char fly3[] = "AFL1684";
char fly4[] = "SMD6405";
char fly5[] = "AFL1354";


//=============================== новый вариант расчета координат ================================
// Структура для хранения данных самолета
struct Aircraft_test {
    float latitude;
    float longitude;
    float course;
    float speed; // м/с
    double totalDistance; // общая дистанция для данного самолета
    double currentDistance; // текущая пройденная дистанция
    bool movingForward; // направление движения
    int id; // идентификатор самолета
};
// Константы


const float EARTH_RADIUS = 6371000.0; // Радиус Земли в метрах
const float DISTANCE_STEP = 250.0; // Шаг перемещения в метрах
const float TOTAL_DISTANCE = 10000.0; // Общая дистанция в метрах
const unsigned long UPDATE_INTERVAL = 1000; // Интервал обновления в мс


// Переменные для управления движением
float startLatitude = ThisAircraft.test_latitude;
float startLongitude = ThisAircraft.test_longitude;
float currentDistance = 0.0;
bool movingForward = true;
unsigned long lastUpdate = 0;

// Функция перемещения самолета на заданное расстояние
void moveAircraft(float distance)
{
    float lat1 = ThisAircraft.latitude * DEG_TO_RAD;
    float lon1 = ThisAircraft.longitude * DEG_TO_RAD;
    float bearing = ThisAircraft.course * DEG_TO_RAD;

    float angular_distance = distance / EARTH_RADIUS;

    // Вычисление новой широты
    float lat2 = asin(sin(lat1) * cos(angular_distance) +
        cos(lat1) * sin(angular_distance) * cos(bearing));

    // Вычисление новой долготы
    float dlon = atan2(sin(bearing) * sin(angular_distance) * cos(lat1),
        cos(angular_distance) - sin(lat1) * sin(lat2));

    float lon2 = fmod(lon1 + dlon + 3 * PI, 2 * PI) - PI; // Нормализация долготы

    // Обновление координат
    ThisAircraft.latitude = lat2 * RAD_TO_DEG;
    ThisAircraft.longitude = lon2 * RAD_TO_DEG;
}


//
//// Функция вывода текущей позиции. Для теста
//void printCurrentPosition() {
//    Serial.printf("Дистанция: %.0f м | ", currentDistance);
//    Serial.printf("Координаты: %.6f°, %.6f° | ",
//        ThisAircraft.latitude, ThisAircraft.longitude);
//    Serial.printf("Курс: %.1f°\n", ThisAircraft.course);
//}

// Функция проверки и изменения курса
void checkAndUpdateCourse() 
{
    if (movingForward && currentDistance >= TOTAL_DISTANCE) 
    {
        // Достигли конечной точки - разворот на 180°
        ThisAircraft.course = fmod(ThisAircraft.course + 180.0, 360.0);
        movingForward = false;
        currentDistance = 0.0;

        //Serial.println(" ДОСТИГНУТА КОНЕЧНАЯ ТОЧКА ");
        //Serial.printf(" НОВЫЙ КУРС: %.1f° \n\n", ThisAircraft.course);

    }
    else if (!movingForward && currentDistance >= TOTAL_DISTANCE) 
    {
        // Вернулись к точке старта - снова разворот на 180°
        ThisAircraft.course = fmod(ThisAircraft.course + 180.0, 360.0);
        movingForward = true;
        currentDistance = 0.0;

        //Serial.println(" ВОЗВРАТ К ТОЧКЕ СТАРТА ");
        //Serial.printf(" НОВЫЙ КУРС: %.1f° \n\n", ThisAircraft.course);
    }
}

// Функция вычисления расстояния между двумя точками(формула гаверсинуса)
float calculateDistance(float lat1, float lon1, float lat2, float lon2) 
{
    float dLat = (lat2 - lat1) * DEG_TO_RAD;
    float dLon = (lon2 - lon1) * DEG_TO_RAD;

    float a = sin(dLat / 2) * sin(dLat / 2) +
        cos(lat1 * DEG_TO_RAD) * cos(lat2 * DEG_TO_RAD) *
        sin(dLon / 2) * sin(dLon / 2);

    float c = 2 * atan2(sqrt(a), sqrt(1 - a));
    return EARTH_RADIUS * c;
}


//--------------------------версия на 5 самолетов -----------------------------------------------
// Структура для хранения данных самолета
struct Aircraft5 {
    float latitude;
    float longitude;
    float course;
    float speed; // м/с
    float totalDistance; // общая дистанция для данного самолета
    float currentDistance; // текущая пройденная дистанция
    bool movingForward; // направление движения
    int id; // идентификатор самолета
};

// Массив из 5 самолетов
Aircraft5 aircraft5[5];

//// Константы
//const float EARTH_RADIUS = 6371000.0; // Радиус Земли в метрах
////const float DEG_TO_RAD = PI / 180.0;
////const float RAD_TO_DEG = 180.0 / PI;
//const float DISTANCE_STEP = 500.0; // Шаг перемещения в метрах
//const unsigned long UPDATE_INTERVAL = 1000; // Интервал обновления в мс

//// Переменные для управления движением
//float startLatitude5 = 55.958388;
//float startLongitude5 = 37.243838;
//unsigned long lastUpdate5 = 0;


// Функция инициализации всех самолетов
void initializeAircraft() {
    // Самолет 1
    aircraft5[0].id = 1;
    aircraft5[0].latitude = startLatitude;
    aircraft5[0].longitude = startLongitude;
    aircraft5[0].course = 70.0;
    aircraft5[0].totalDistance = 10000.0;
    aircraft5[0].currentDistance = 0.0;
    aircraft5[0].movingForward = true;
    aircraft5[0].speed = 50.0;

    // Самолет 2
    aircraft5[1].id = 2;
    aircraft5[1].latitude = startLatitude;
    aircraft5[1].longitude = startLongitude;
    aircraft5[1].course = 120.0;
    aircraft5[1].totalDistance = 8000.0;
    aircraft5[1].currentDistance = 0.0;
    aircraft5[1].movingForward = true;
    aircraft5[1].speed = 45.0;

    // Самолет 3
    aircraft5[2].id = 3;
    aircraft5[2].latitude = startLatitude;
    aircraft5[2].longitude = startLongitude;
    aircraft5[2].course = 250.0;
    aircraft5[2].totalDistance = 9000.0;
    aircraft5[2].currentDistance = 0.0;
    aircraft5[2].movingForward = true;
    aircraft5[2].speed = 55.0;

    // Самолет 4
    aircraft5[3].id = 4;
    aircraft5[3].latitude = startLatitude;
    aircraft5[3].longitude = startLongitude;
    aircraft5[3].course = 290.0;
    aircraft5[3].totalDistance = 11000.0;
    aircraft5[3].currentDistance = 0.0;
    aircraft5[3].movingForward = true;
    aircraft5[3].speed = 40.0;

    // Самолет 5
    aircraft5[4].id = 5;
    aircraft5[4].latitude = startLatitude;
    aircraft5[4].longitude = startLongitude;
    aircraft5[4].course = 350.0;
    aircraft5[4].totalDistance = 11500.0;
    aircraft5[4].currentDistance = 0.0;
    aircraft5[4].movingForward = true;
    aircraft5[4].speed = 60.0;
}

// Функция инициализации всех самолетов синхронно
void initializeAircraftS() {
    // Самолет 1
    aircraft5[0].id = 1;
    aircraft5[0].latitude = startLatitude;
    aircraft5[0].longitude = startLongitude;
    aircraft5[0].course = 70.0;
    aircraft5[0].totalDistance = 10000.0;
    aircraft5[0].currentDistance = 0.0;
    aircraft5[0].movingForward = true;
    aircraft5[0].speed = 50.0;

    // Самолет 2
    aircraft5[1].id = 2;
    aircraft5[1].latitude = startLatitude;
    aircraft5[1].longitude = startLongitude;
    aircraft5[1].course = 120.0;
    aircraft5[1].totalDistance = 10000.0;
    aircraft5[1].currentDistance = 0.0;
    aircraft5[1].movingForward = true;
    aircraft5[1].speed = 45.0;

    // Самолет 3
    aircraft5[2].id = 3;
    aircraft5[2].latitude = startLatitude;
    aircraft5[2].longitude = startLongitude;
    aircraft5[2].course = 250.0;
    aircraft5[2].totalDistance = 10000.0;
    aircraft5[2].currentDistance = 0.0;
    aircraft5[2].movingForward = true;
    aircraft5[2].speed = 55.0;

    // Самолет 4
    aircraft5[3].id = 4;
    aircraft5[3].latitude = startLatitude;
    aircraft5[3].longitude = startLongitude;
    aircraft5[3].course = 290.0;
    aircraft5[3].totalDistance = 10000.0;
    aircraft5[3].currentDistance = 0.0;
    aircraft5[3].movingForward = true;
    aircraft5[3].speed = 40.0;

    // Самолет 5
    aircraft5[4].id = 5;
    aircraft5[4].latitude = startLatitude;
    aircraft5[4].longitude = startLongitude;
    aircraft5[4].course = 350.0;
    aircraft5[4].totalDistance = 10000.0;
    aircraft5[4].currentDistance = 0.0;
    aircraft5[4].movingForward = true;
    aircraft5[4].speed = 60.0;
}


// Функция перемещения конкретного самолета на заданное расстояние
void moveAircraft5(int aircraftIndex, float distance) 
{
    float lat1 = aircraft5[aircraftIndex].latitude * DEG_TO_RAD;
    float lon1 = aircraft5[aircraftIndex].longitude * DEG_TO_RAD;
    float bearing = aircraft5[aircraftIndex].course * DEG_TO_RAD;

    float angular_distance = distance / EARTH_RADIUS;

    // Вычисление новой широты
    float lat2 = asin(sin(lat1) * cos(angular_distance) +
        cos(lat1) * sin(angular_distance) * cos(bearing));

    // Вычисление новой долготы
    float dlon = atan2(sin(bearing) * sin(angular_distance) * cos(lat1),
        cos(angular_distance) - sin(lat1) * sin(lat2));

    float lon2 = fmod(lon1 + dlon + 3 * PI, 2 * PI) - PI; // Нормализация долготы

    // Обновление координат
    aircraft5[aircraftIndex].latitude = lat2 * RAD_TO_DEG;
    aircraft5[aircraftIndex].longitude = lon2 * RAD_TO_DEG;
}

// Функция проверки и изменения курса для конкретного самолета
void checkAndUpdateCourse5(int aircraftIndex)
{
    if (aircraft5[aircraftIndex].currentDistance >= aircraft5[aircraftIndex].totalDistance) {
        // Достигли конечной точки - разворот на 180°
        aircraft5[aircraftIndex].course = fmod(aircraft5[aircraftIndex].course + 180.0, 360.0);
        aircraft5[aircraftIndex].currentDistance = 0.0;

        // Переключение направления движения
        aircraft5[aircraftIndex].movingForward = !aircraft5[aircraftIndex].movingForward;

        //Serial.printf(" САМОЛЕТ %d: РАЗВОРОТ НА 180° | НОВЫЙ КУРС: %.1f° \n",
        //    aircraft5[aircraftIndex].id, aircraft5[aircraftIndex].course);
    }
}

// Функция вывода информации о всех самолетах
void printAllAircraftInfo5()
{
    Serial.println("Начальные параметры самолетов:");
    for (int i = 0; i < 5; i++) {
        Serial.printf("Самолет %d: Курс=%.1f°, Дистанция=%.0fм, Скорость=%.1fм/с\n",
            aircraft5[i].id, aircraft5[i].course, aircraft5[i].totalDistance, aircraft5[i].speed);
    }
    Serial.printf("Стартовые координаты для всех: %.6f°, %.6f°\n", startLatitude, startLongitude);
    Serial.println();
}

// Функция вывода текущих позиций всех самолетов
void printAllCurrentPositions5()
{
    for (int i = 0; i < 5; i++) {
        Serial.printf("Самолет %d | Расстояние: %.0f/%.0fм | Координаты: %.6f°, %.6f° | Курс: %.1f°\n",
            aircraft5[i].id,
            aircraft5[i].currentDistance,
            aircraft5[i].totalDistance,
            aircraft5[i].latitude,
            aircraft5[i].longitude,
            aircraft5[i].course);
    }
}

// Функция вычисления расстояния между двумя точками (формула гаверсинуса)
float calculateDistance5(float lat1, float lon1, float lat2, float lon2)
{
    float dLat = (lat2 - lat1) * DEG_TO_RAD;
    float dLon = (lon2 - lon1) * DEG_TO_RAD;

    float a = sin(dLat / 2) * sin(dLat / 2) +
        cos(lat1 * DEG_TO_RAD) * cos(lat2 * DEG_TO_RAD) *
        sin(dLon / 2) * sin(dLon / 2);

    float c = 2 * atan2(sqrt(a), sqrt(1 - a));
    return EARTH_RADIUS * c;
}

// Функция для получения информации о конкретном самолете
void getAircraftInfo5(int aircraftIndex)
{
    if (aircraftIndex >= 0 && aircraftIndex < 5)
    {
        Serial.printf("Информация о самолете %d:\n", aircraft5[aircraftIndex].id);
        Serial.printf("  Координаты: %.6f°, %.6f°\n",  aircraft5[aircraftIndex].latitude, aircraft5[aircraftIndex].longitude);
        Serial.printf("  Курс: %.1f°\n", aircraft5[aircraftIndex].course);
        Serial.printf("  Пройденное расстояние: %.0f из %.0f метров\n", aircraft5[aircraftIndex].currentDistance, aircraft5[aircraftIndex].totalDistance);
        Serial.printf("  Направление: %s\n", aircraft5[aircraftIndex].movingForward ? "Вперед" : "Назад");
    }
}




//================================================================================================

void txrx_test()
{
    bool success = false;
#if DEBUG_TIMING
    unsigned long baro_start_ms, baro_end_ms;
    unsigned long tx_start_ms, tx_end_ms, rx_start_ms, rx_end_ms;
    unsigned long parse_start_ms, parse_end_ms, led_start_ms, led_end_ms;
    unsigned long export_start_ms, export_end_ms;
 #endif
    ThisAircraft.timestamp = now(); 


    if (set_test_coordinate == false && settings->input_coordinates == IMPUT_COORD_AUTO)
    {
        GNSS_loop();

        if (isValidFix())
        {
            ThisAircraft.test_latitude = gnss.location.lat();
            ThisAircraft.test_longitude = gnss.location.lng();
            set_test_coordinate = true;
        }
    }
    else if (set_test_coordinate == false && settings->input_coordinates == IMPUT_COORD_MANUAL)
    {
        // Инициализация начальных координат
         if(settings->input_N_S == IMPUT_N)
        {
             ThisAircraft.test_latitude = (float)settings->test_latitude;
        }
        else
        {
             ThisAircraft.test_latitude = (float)-settings->test_latitude;
        }

        if (settings->input_E_W == IMPUT_E)
        {
            ThisAircraft.test_longitude = (float)settings->test_longitude;
        }
        else
        {
            ThisAircraft.test_longitude = (float)-settings->test_longitude;
        }


        ThisAircraft.latitude = ThisAircraft.test_latitude;    // 
        ThisAircraft.longitude = ThisAircraft.test_longitude;   // 
        startLatitude = ThisAircraft.test_latitude;
        startLongitude = ThisAircraft.test_longitude;

        ThisAircraft.course = 1.0;
        ThisAircraft.speed = 50.0; // 50 м/с (180 км/ч)

        set_test_coordinate = true;
    }

    if (set_test_coordinate5 == false)
    {
        if (settings->out_of_sync == OUT_OF_SYNC_OFF)
        {

            initializeAircraft();

        }
        else
        {
            initializeAircraftS();

        }
        // Вывод начальных параметров всех самолетов
        //printAllAircraftInfo5();

        //Serial.println("Начало движения всех самолетов...\n");

        set_test_coordinate5 = true;
    }


    if (TxPosUpdMarker == 0 || (millis() - TxPosUpdMarker) > 1100)
    {

        switch (set_air)
        {
 
        case 1:

            speed0 = 200.0;
            altitude0 = 1000.0;


            /* тест на вращение*/
            //ThisAircraft.course = ThisAircraft.course + 2.0;
            //if (ThisAircraft.course >= 360.0)
                //ThisAircraft.course = 0.0;

            ThisAircraft.altitude = altitude0;
            ThisAircraft.course = 1.0;      // test_curse0;
            ThisAircraft.speed = speed0;
            ThisAircraft.vs = TXRX_TEST_VS;  //футов в минуту
            break;

        case 2:


            // Движение на DISTANCE_STEP метров
            moveAircraft(DISTANCE_STEP);
            currentDistance += DISTANCE_STEP;

            // Вывод текущих координат
            //printCurrentPosition();

            // Проверка достижения конечной точки или точки старта
            checkAndUpdateCourse();


            if (!alt_high1)
            {
                altitude1 += 25.0;
                if (altitude1 > 1150.0)
                {
                    altitude1 = 1150.0;
                    alt_high1 = true;
                }
            }
            if (alt_high1)
            {

                altitude1 -= 25.0;
                if (altitude1 < 850.0)
                {
                    altitude1 = 850.0;
                    alt_high1 = false;
                }
            }

            ThisAircraft.altitude = altitude1;
            ThisAircraft.vs = TXRX_TEST_VS;
            break;
            //====================================================================================================
        case 3:
            esp_task_wdt_reset();

            speed0 = random(230, 450);
            altitude0 = random(500, 2550);

            /* тест на вращение*/
 /*           ThisAircraft.course = ThisAircraft.course + 2.0;
            if (ThisAircraft.course >= 360.0)
                ThisAircraft.course = 0.0;*/

            ThisAircraft.altitude = altitude0;
            ThisAircraft.course = 1.0;      // test_curse0;
            ThisAircraft.speed = speed0;
            ThisAircraft.vs = TXRX_TEST_VS;  //футов в минуту

            // Обновление позиции каждого самолета
            for (int i = 0; i < 5; i++)
            {
                moveAircraft5(i, DISTANCE_STEP);
                aircraft5[i].currentDistance += DISTANCE_STEP;
                checkAndUpdateCourse5(i);
            }

            //// Вывод текущих позиций всех самолетов
            //printAllCurrentPositions5();
            //Serial.println("----------------------------------------");

            //================ Самолет №1 ================================

            if (!alt_high1)
            {
                altitude1 += 50.0;
                if (altitude1 > 1200.0)
                {
                    altitude1 = 1200.0;
                    alt_high1 = true;
                }
            }
            if (alt_high1)
            {

                altitude1 -= 50.0;
                if (altitude1 < 50.0)
                {
                    altitude1 = 50.0;
                    alt_high1 = false;
                }
            }
            speed1 -= 30.0;
            if (speed1 <= 30.0)
                speed1 = 1020.0;

            fo.addr = 0x151DC8;
            fo.squawk = 1521;
            memcpy((char*)fo.flight, fly1, strlen(fly1));
            fo.altitude = altitude1;
            fo.pressure_altitude = altitude1;
            fo.speed = speed1;
            fo.vert_rate = 50;
            fo.signal_source = 1;
            fo.timestamp = now(); // 
            fo.course = aircraft5[0].course;
            fo.aircraft_type = AIRCRAFT_TYPE_JET;
            fo.latitude = aircraft5[0].latitude;
            fo.longitude = aircraft5[0].longitude;
            /* Расчет расстояния, курса и уровня опастности сближения нашего и стороннего самолета*/
            if (fo.latitude != 0 && fo.longitude != 0) // Расчет возможен если получены координаты нашего и стороннего самолета
            {
                //if (xSemaphoreTake(containerMutex, pdMS_TO_TICKS(10)) == pdTRUE)
                //{
                    Traffic_Update(&fo);
                /*    xSemaphoreGive(containerMutex);
                }*/
            }

            /* Остальные параметры записываем в базу */
            //if (xSemaphoreTake(containerMutex, pdMS_TO_TICKS(10)) == pdTRUE)
            //{
                Traffic_Add(&fo);
            //    xSemaphoreGive(containerMutex);
            //}

            //======================== Самолет №2 ================================================

            //    aircraft5[1].latitude,
            //    aircraft5[1].longitude,
            //    aircraft5[1].course);
            esp_task_wdt_reset();

            if (!alt_high2)
            {
                altitude2 += 50.0;
                if (altitude2 > 1200.0)
                {
                    altitude2 = 1200.0;
                    alt_high2 = true;
                }
            }
            if (alt_high2)
            {

                altitude2 -= 50.0;
                if (altitude2 < 50.0)
                {
                    altitude2 = 50.0;
                    alt_high2 = false;
                }
            }
            speed2 -= 30.0;
            if (speed2 <= 30.0)
                speed2 = 1020.0;


            fo.addr = 0x151DA0;
            fo.squawk = 2123;
            memcpy((char*)fo.flight, fly2, strlen(fly2));
            fo.altitude = altitude2;
            fo.pressure_altitude = altitude2;
            fo.speed = speed2;
            fo.vert_rate = 100;
            fo.signal_source = 1;
            fo.timestamp = now(); // 
            fo.course = aircraft5[1].course;
            fo.latitude = aircraft5[1].latitude;
            fo.longitude = aircraft5[1].longitude;
            fo.aircraft_type = AIRCRAFT_TYPE_JET;
            /* Расчет расстояния, курса и уровня опастности сближения нашего и стороннего самолета*/
            if (fo.latitude != 0 && fo.longitude != 0) // Расчет возможен если получены координаты нашего и стороннего самолета
            {
                //if (xSemaphoreTake(containerMutex, pdMS_TO_TICKS(10)) == pdTRUE)
                //{
                    Traffic_Update(&fo);
 /*                   xSemaphoreGive(containerMutex);
                }*/
            }

            /* Остальные параметры записываем в базу */
 /*           if (xSemaphoreTake(containerMutex, pdMS_TO_TICKS(10)) == pdTRUE)
            {*/
                Traffic_Add(&fo);
 /*               xSemaphoreGive(containerMutex);
            }*/
            //******************************************************************************************************************

            //================ Самолет №3 ================================

            //    aircraft5[2].latitude,
            //    aircraft5[2].longitude,
            //    aircraft5[2].course);
            esp_task_wdt_reset();

            if (!alt_high3)
            {
                altitude3 += 40.0;
                if (altitude3 > 1000.0)
                {
                    altitude3 = 1000.0;
                    alt_high3 = true;
                }
            }
            if (alt_high3)
            {

                altitude3 -= 40.0;
                if (altitude3 < 50.0)
                {
                    altitude3 = 50.0;
                    alt_high3 = false;
                }
            }
            speed3 -= 30.0;
            if (speed3 <= 30.0)
                speed3 = 990.0;

            fo.addr = 0x151DCF;
            fo.squawk = 2751;
            memcpy((char*)fo.flight, fly3, strlen(fly3));
            fo.altitude = altitude3;
            fo.pressure_altitude = altitude3;
            fo.speed = speed3;
            fo.vert_rate = -50;
            fo.signal_source = 1;
            fo.timestamp = now(); // 
            fo.course = aircraft5[2].course;
            fo.latitude = aircraft5[2].latitude;
            fo.longitude = aircraft5[2].longitude;
            fo.aircraft_type = AIRCRAFT_TYPE_JET;
            /* Расчет расстояния, курса и уровня опастности сближения нашего и стороннего самолета*/
            if (fo.latitude != 0 && fo.longitude != 0) // Расчет возможен если получены координаты нашего и стороннего самолета
            {
                //if (xSemaphoreTake(containerMutex, pdMS_TO_TICKS(10)) == pdTRUE)
                //{
                    Traffic_Update(&fo);
 /*                   xSemaphoreGive(containerMutex);
                }*/
            }

            /* Остальные параметры записываем в базу */
            //if (xSemaphoreTake(containerMutex, pdMS_TO_TICKS(10)) == pdTRUE)
            //{
                Traffic_Add(&fo);
            //    xSemaphoreGive(containerMutex);
            //}

            //======================== Самолет №4 ================================================

            //    aircraft5[3].latitude,
            //    aircraft5[3].longitude,
            //    aircraft5[3].course);
            esp_task_wdt_reset();

            if (!alt_high4)
            {
                altitude4 += 50.0;
                if (altitude4 > 800.0)
                {
                    altitude4 = 800.0;
                    alt_high4 = true;
                }
            }
            if (alt_high4)
            {

                altitude4 -= 50.0;
                if (altitude4 < 50.0)
                {
                    altitude4 = 50.0;
                    alt_high4 = false;
                }
            }
            speed4 -= 30.0;
            if (speed4 <= 30.0)
                speed4 = 700.0;


            fo.addr = 0x155C11;
            fo.squawk = 1501;
            memcpy((char*)fo.flight, fly4, strlen(fly4));
            fo.altitude = altitude4;
            fo.pressure_altitude = altitude4;
            fo.speed = speed4;
            fo.vert_rate = -150;
            fo.signal_source = 1;
            fo.timestamp = now(); // 
            fo.course = aircraft5[3].course;
            fo.latitude = aircraft5[3].latitude;
            fo.longitude = aircraft5[3].longitude;
            fo.aircraft_type = AIRCRAFT_TYPE_JET;
            /* Расчет расстояния, курса и уровня опастности сближения нашего и стороннего самолета*/
            if (fo.latitude != 0 && fo.longitude != 0) // Расчет возможен если получены координаты нашего и стороннего самолета
            {
 /*               if (xSemaphoreTake(containerMutex, pdMS_TO_TICKS(10)) == pdTRUE)
                {*/
                    Traffic_Update(&fo);
 /*                   xSemaphoreGive(containerMutex);
                }*/
            }

            /* Остальные параметры записываем в базу */
            //if (xSemaphoreTake(containerMutex, pdMS_TO_TICKS(10)) == pdTRUE)
            //{
                Traffic_Add(&fo);
 /*               xSemaphoreGive(containerMutex);
            }*/
            //******************************************************************************************************************


            //    aircraft5[4].latitude,
            //    aircraft5[4].longitude,
            //    aircraft5[4].course);
            esp_task_wdt_reset();

            if (!alt_high5)
            {
                altitude5 += 50.0;
                if (altitude5 > 800.0)
                {
                    altitude5 = 800.0;
                    alt_high5 = true;
                }
            }
            if (alt_high5)
            {

                altitude5 -= 50.0;
                if (altitude5 < 50.0)
                {
                    altitude5 = 50.0;
                    alt_high5 = false;
                }
            }
            speed5 -= 30.0;
            if (speed5 <= 30.0)
                speed5 = 700.0;


            fo.addr = 0x155C12;
            fo.squawk = 1502;
            memcpy((char*)fo.flight, fly5, strlen(fly5));
            fo.altitude = altitude5;
            fo.pressure_altitude = altitude5;
            fo.speed = speed5;
            fo.vert_rate = -150;
            fo.signal_source = 1;
            fo.timestamp = now(); // 
            fo.course = aircraft5[4].course;
            fo.latitude = aircraft5[4].latitude;
            fo.longitude = aircraft5[4].longitude;
            fo.aircraft_type = AIRCRAFT_TYPE_JET;
            /* Расчет расстояния, курса и уровня опастности сближения нашего и стороннего самолета*/
            if (fo.latitude != 0 && fo.longitude != 0) // Расчет возможен если получены координаты нашего и стороннего самолета
            {
                //if (xSemaphoreTake(containerMutex, pdMS_TO_TICKS(10)) == pdTRUE)
                //{
                    Traffic_Update(&fo);
 /*                   xSemaphoreGive(containerMutex);
                }*/
            }

            /* Остальные параметры записываем в базу */
 /*           if (xSemaphoreTake(containerMutex, pdMS_TO_TICKS(10)) == pdTRUE)
            {*/
                Traffic_Add(&fo);
            //    xSemaphoreGive(containerMutex);
            //}
            //******************************************************************************************************************

            break;
        case 4:
            //Атлантический океан, 1  Средняя точка
           //0.007748, 0.007875
            ThisAircraft.latitude = 0.007748;    // 
            ThisAircraft.longitude = 0.007875;   // 

            test_curse0 = 360.0;
            speed0 = 200.0;
            altitude0 = 1000.0;

            esp_task_wdt_reset();

            //    0.075397, 0.029420
            //    - 0.004039, 0.029420
            //    - 0.004039, -0.054865
            //    0.075397, -0.054865

            //================ Самолет №1 ================================
            /*
            //0.075397, 0.029420
            //- 0.004039, 0.029420

            */

            // track_air

            switch (track_air)
            {
            case 0:
                alien_lat20 -= (alien_lat13 - alien_lat14) / 20; // Перемещаемся сверху вниз
                alien_lon20 = alien_lon13;//
                if (alien_lat20 < alien_lat14)
                {
                    alien_lat20 = alien_lat14;
                    track_air = 1;
                }

                fo.course = 180;

                break;
            case 1:
                alien_lat20 = alien_lat14;
                alien_lon20 -= (alien_lon14 - alien_lon15) / 20; // Перемещаемся внизу справа налево
                if (alien_lon20 < alien_lon15)
                {
                    alien_lon20 = alien_lon15;
                    track_air = 2;
                }

                fo.course = 270;
                break;
            case 2:
                alien_lat20 += (alien_lat16 - alien_lat15) / 20; // Перемещаемся снизу вверх
                alien_lon20 = alien_lon15;//
                if (alien_lat20 > alien_lat16)
                {
                    alien_lat20 = alien_lat16;
                    track_air = 3;
                }

                fo.course = 1;
                break;
            case 3:

                // Перемещаемся слева направо в исходную точку
                alien_lat20 = alien_lat16;
                alien_lon20 += (alien_lon13 - alien_lon16) / 20; // Перемещаемся внизу справа налево
                if (alien_lon20 > alien_lon13)
                {
                    alien_lon20 = alien_lon13;
                    track_air = 0;
                }
                fo.course = 90;
                break;
            default:
                break;
            }

            if (!alt_high4)
            {
                altitude4 += 50.0;
                if (altitude4 > 800.0)
                {
                    altitude4 = 800.0;
                    alt_high4 = true;
                }
            }
            if (alt_high4)
            {

                altitude4 -= 50.0;
                if (altitude4 < 50.0)
                {
                    altitude4 = 50.0;
                    alt_high4 = false;
                }
            }
            speed4 -= 30.0;
            if (speed4 <= 30.0)
                speed4 = 700.0;


            fo.addr = 0x155C11;
            fo.squawk = 1501;
            memcpy((char*)fo.flight, fly4, strlen(fly4));
            fo.altitude = altitude4;
            fo.pressure_altitude = altitude4;
            fo.speed = speed4;
            fo.vert_rate = -150;
            fo.signal_source = 1;
            fo.timestamp = now(); // 
            fo.latitude = alien_lat20;
            fo.longitude = alien_lon20;
            fo.aircraft_type = AIRCRAFT_TYPE_JET;
            /* Расчет расстояния, курса и уровня опастности сближения нашего и стороннего самолета*/
            if (fo.latitude != 0.0 && fo.longitude != 0.0) // Расчет возможен если получены координаты нашего и стороннего самолета
            {
 /*               if (xSemaphoreTake(containerMutex, pdMS_TO_TICKS(10)) == pdTRUE)
                {*/
                    Traffic_Update(&fo);
 /*                   xSemaphoreGive(containerMutex);
                }*/
            }

            /* Остальные параметры записываем в базу */
            //if (xSemaphoreTake(containerMutex, pdMS_TO_TICKS(10)) == pdTRUE)
            //{
                Traffic_Add(&fo);
            //    xSemaphoreGive(containerMutex);
            //}
            //******************************************************************************************************************
            break;

        case 5:
            //Тихий океан, 1  Средняя точка
            //0.000200, 179.992963
            ThisAircraft.latitude = 0.000200;    // 
            ThisAircraft.longitude = 179.999960;   // 

            test_curse0 = 360.0;
            speed0 = 200.0;
            altitude0 = 1000.0;

            esp_task_wdt_reset();

            // 0.053769, -179.953328
            // -0.025508, -179.953328
            // -0.025508, 179.953328
            // 0.053769, 179.953328

            //================ Самолет №1 ================================

            switch (track_air)
            {
            case 0:
                /*
                 alien_lat23 = 0.053769;
                 alien_lon23 = -179.953328;

                 alien_lat24 = -0.025508;
                 alien_lon24 = -179.953328;
                */
                alien_lat30 -= (alien_lat23 - alien_lat24) / 20; // Перемещаемся сверху вниз
                alien_lon30 = alien_lon23;//
                if (alien_lat30 < alien_lat24)
                {
                    alien_lat30 = alien_lat24;
                    track_air = 1;
                }

                fo.course = 180;

                break;
            case 1:
                // Serial.println("case 1");
                 /*
                   alien_lat25 = -0.025508;
                   alien_lon25 = 179.953328;
                   alien_lon24 = -179.953328;
                 */
                alien_lat30 = alien_lat24;

                if (alien_lon30 < 0.0 && alien_lon30 > -180)
                {
                    alien_lon30 -= 0.004667; // Перемещаемся внизу справа налево
                }
                if (alien_lon30 <= -180.0)
                {
                    alien_lon30 = 180.0;
                }

                if (alien_lon30 <= 180.0 && alien_lon30 > 0.0)
                {
                    alien_lon30 -= 0.004667; // Перемещаемся внизу справа налево
                }

                if (alien_lon30 > 0.0 && alien_lon30 < alien_lon25)
                {
                    alien_lon30 = alien_lon25;
                    track_air = 2;
                }

                fo.course = 270;
                break;
            case 2:
                alien_lat30 += (alien_lat26 - alien_lat25) / 20; // Перемещаемся снизу вверх
                alien_lon30 = alien_lon25;//
                if (alien_lat30 > alien_lat26)
                {
                    alien_lat30 = alien_lat26;
                    track_air = 3;
                }

                fo.course = 1;
                break;
            case 3:

                // Перемещаемся слева направо в исходную точку
                alien_lat30 = alien_lat26;

                if (alien_lon30 > 0.0 && alien_lon30 < 180.0)
                {
                    alien_lon30 += 0.004667; // Перемещаемся вверху слево направо
                }

                if (alien_lon30 >= 180.0)
                {
                    alien_lon30 = -180.0;
                }

                if (alien_lon30 < 0.0/* && alien_lon30 > -180*/)
                {
                    alien_lon30 += 0.004667; //  Перемещаемся вверху слево направо
                }

                if (alien_lon30 < 0.0 && alien_lon30 > alien_lon23)
                {
                    alien_lon30 = alien_lon23;
                    track_air = 0;
                }

                fo.course = 90;
                break;
            default:
                break;
            }

            if (!alt_high4)
            {
                altitude4 += 50.0;
                if (altitude4 > 800.0)
                {
                    altitude4 = 800.0;
                    alt_high4 = true;
                }
            }
            if (alt_high4)
            {

                altitude4 -= 50.0;
                if (altitude4 < 50.0)
                {
                    altitude4 = 50.0;
                    alt_high4 = false;
                }
            }
            speed4 -= 30.0;
            if (speed4 <= 30.0)
                speed4 = 700.0;


            fo.addr = 0x155C11;
            fo.squawk = 1501;
            memcpy((char*)fo.flight, fly4, strlen(fly4));
            fo.altitude = altitude4;
            fo.pressure_altitude = altitude4;
            fo.speed = speed4;
            fo.vert_rate = -150;
            fo.signal_source = 1;
            fo.timestamp = now(); // 
            fo.latitude = alien_lat30;
            fo.longitude = alien_lon30;
            fo.aircraft_type = AIRCRAFT_TYPE_JET;
            /* Расчет расстояния, курса и уровня опастности сближения нашего и стороннего самолета*/
            if (fo.latitude != 0.0 && fo.longitude != 0.0) // Расчет возможен если получены координаты нашего и стороннего самолета
            {
 /*               if (xSemaphoreTake(containerMutex, pdMS_TO_TICKS(10)) == pdTRUE)
                {*/
                    Traffic_Update(&fo);
 /*                   xSemaphoreGive(containerMutex);
                }*/
            }

            /* Остальные параметры записываем в базу */
            //if (xSemaphoreTake(containerMutex, pdMS_TO_TICKS(10)) == pdTRUE)
            //{
                Traffic_Add(&fo);
 /*               xSemaphoreGive(containerMutex);
            }*/
            //******************************************************************************************************************
            break;
        default:
            break;
        }
        TxPosUpdMarker = millis();
    }


#if DEBUG_TIMING
    baro_start_ms = millis();
#endif
    Baro_loop();
#if DEBUG_TIMING
    baro_end_ms = millis();
#endif


#if DEBUG_TIMING
    tx_start_ms = millis();
#endif
    RF_Transmit(RF_Encode(&ThisAircraft), true);
#if DEBUG_TIMING
    tx_end_ms = millis();
    rx_start_ms = millis();
#endif
    success = RF_Receive();
#if DEBUG_TIMING
    rx_end_ms = millis();
#endif

#if DEBUG_TIMING
    parse_start_ms = millis();
#endif
    if (success) ParseData();
#if DEBUG_TIMING
    parse_end_ms = millis();
#endif

    Traffic_loop();

#if DEBUG_TIMING
    led_start_ms = millis();
#endif
    if (isTimeToDisplay())
    {
        LEDTimeMarker = millis();
    }
#if DEBUG_TIMING
    led_end_ms = millis();
#endif


#if DEBUG_TIMING
    export_start_ms = millis();
#endif
    if (isTimeToExport()) {
#if defined(USE_NMEALIB)
        NMEA_Position();
#endif
        NMEA_Export();
        ExportTimeMarker = millis();
    }
#if DEBUG_TIMING
    export_end_ms = millis();
#endif


#if DEBUG_TIMING
    if (baro_start_ms - baro_end_ms) {
        Serial.print(F("Baro start: "));
        Serial.print(baro_start_ms);
        Serial.print(F(" Baro stop: "));
        Serial.println(baro_end_ms);
    }
    if (tx_end_ms - tx_start_ms) {
        Serial.print(F("TX start: "));
        Serial.print(tx_start_ms);
        Serial.print(F(" TX stop: "));
        Serial.println(tx_end_ms);
    }
    if (rx_end_ms - rx_start_ms) {
        Serial.print(F("RX start: "));
        Serial.print(rx_start_ms);
        Serial.print(F(" RX stop: "));
        Serial.println(rx_end_ms);
    }
    if (parse_end_ms - parse_start_ms) {
        Serial.print(F("Parse start: "));
        Serial.print(parse_start_ms);
        Serial.print(F(" Parse stop: "));
        Serial.println(parse_end_ms);
    }
    if (led_end_ms - led_start_ms) {
        Serial.print(F("LED start: "));
        Serial.print(led_start_ms);
        Serial.print(F(" LED stop: "));
        Serial.println(led_end_ms);
    }
    if (export_end_ms - export_start_ms) {
        Serial.print(F("Export start: "));
        Serial.print(export_start_ms);
        Serial.print(F(" Export stop: "));
        Serial.println(export_end_ms);
    }
 
#endif

    // Handle Air Connect
    NMEA_loop();
    ClearExpired();
}

#endif /* EXCLUDE_TEST_MODE */