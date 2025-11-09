#include <stdio.h>
#include <Arduino.h>
#include "SPI.h"
#include <esp_task_wdt.h>
#include <locale.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <TimeLib.h>

// ==== RS485 ====
#define RS485_SERIAL   Serial2
#define RS485_TX_PIN   18
#define RS485_RX_PIN   17
#define RS485_DE_PIN   21
#define RS485_BAUD     115200  //256000 //921600
#define RS485_CONFIG   SERIAL_8N1
#define LED            4

const uint32_t PACKET_HEADER = 0xAABBCCDD;
const uint32_t PACKET_FOOTER = 0xDDCCBBAA;
#define BUFFER_SIZE           310
#define MAX_TRACKING_OBJECTS  12

SemaphoreHandle_t serialMutex;
SemaphoreHandle_t containerMutex;

typedef struct UFO {
    uint8_t   raw[34];
    time_t    timestamp;
    uint8_t   protocol;
    uint32_t  addr;
    uint8_t   addr_type;
    float     latitude;
    float     longitude;
    float     old_latitude;
    float     old_longitude;
    float     altitude;
    float     pressure_altitude;
    float     course;
    float     speed;
    uint8_t   aircraft_type;
    char      callsign[8];
    int       vert_rate;
    int       squawk;
    time_t    timemsg;
    float     vs;
    bool      stealth;
    bool      no_track;
    int8_t    ns[4];
    int8_t    ew[4];
    float     geoid_separation;
    uint16_t  hdop;
    int8_t    rssi;
    float     distance;
    float     bearing;
    int8_t    alarm_level;
    uint8_t   signal_source;
    time_t    seen;
    uint8_t   hour_msg;
    uint8_t   min_msg;
    uint16_t  delay_time_msg;
    float     test_latitude;
    float     test_longitude;
    uint16_t  last_message_signal_strength_dbm;
    uint16_t  last_message_signal_quality_db;
} ufo_t;

ufo_t ThisAircraft;
ufo_t fo, Container[MAX_TRACKING_OBJECTS], EmptyFO, fo_msg, Container_msg[MAX_TRACKING_OBJECTS];

typedef struct __attribute__((packed)) {
    uint32_t addr;
    int      squawk;
    uint8_t  callsign[8];
    float    altitude;
    float    pressure_altitude;
    float    course;
    float    speed;
    float    distance;
    float    bearing;
    int      vert_rate;
    float    latitude;
    float    longitude;
    time_t   timestamp;
    int8_t   rssi;
    uint16_t last_message_signal_strength_dbm;
    uint16_t last_message_signal_quality_db;
    uint8_t   signal_source;
} ufo_net_t;

typedef struct __attribute__((packed)) {
    uint8_t  new_buttton_M;
    bool     new_message;
    bool     message_received;
    bool     confirm_message_M;
    char     msg_resp_M[BUFFER_SIZE];
    uint8_t  Time_Hour_M;
    uint8_t  Time_Minute_M;
    bool     new_SOS_flag_M;
    bool     isValidGNSS_M;
} aux_t;

aux_t AuxData;

typedef struct __attribute__((packed)) {
    ufo_net_t ThisAircraft;
    ufo_net_t Container[MAX_TRACKING_OBJECTS];
    aux_t AuxData;
    uint8_t BUTTON1;
    uint8_t BUTTON2;
} full_packet_net_t;

// CRC
uint16_t crc16_ccitt(const uint8_t* data, size_t len) {
    uint16_t crc = 0x0000;
    for (size_t i = 0; i < len; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; ++j)
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
    }
    return crc;
}

// Копирование для передачи
void net_to_ufo_Container(const ufo_t* src, ufo_net_t* dst)
{
    dst->addr = src->addr;
    dst->squawk = src->squawk;
    memcpy(dst->callsign, src->callsign, 8);
    dst->altitude = src->altitude;
    dst->pressure_altitude = src->pressure_altitude;
    dst->course = src->course;
    dst->speed = src->speed;
    dst->distance = src->distance;
    dst->bearing = src->bearing;
    dst->vert_rate = src->vert_rate;
    dst->latitude = src->latitude;
    dst->longitude = src->longitude;
    dst->timestamp = src->timestamp;
    dst->rssi = src->rssi;
    dst->last_message_signal_strength_dbm = src->last_message_signal_strength_dbm;
    dst->last_message_signal_quality_db = src->last_message_signal_quality_db;
    dst->signal_source = src->signal_source;
}
void net_to_ufo_ThisAircraft(const ufo_t* src, ufo_net_t* dst)
{
    dst->addr = src->addr;
    dst->squawk = src->squawk;
    memcpy(dst->callsign, src->callsign, 8);
    dst->altitude = src->altitude;
    dst->pressure_altitude = src->pressure_altitude;
    dst->course = src->course;
    dst->speed = src->speed;
    dst->vert_rate = src->vert_rate;
    dst->latitude = src->latitude;
    dst->longitude = src->longitude;
}

// RS485 TX/RX
static void rs485SetTX(bool enable)
{
    digitalWrite(RS485_DE_PIN, enable ? HIGH : LOW);
    if (enable) delayMicroseconds(50);
}
void setupRS485()
{
    RS485_SERIAL.setRxBufferSize(1024);
    RS485_SERIAL.setTxBufferSize(1024);
    RS485_SERIAL.begin(RS485_BAUD, RS485_CONFIG, RS485_RX_PIN, RS485_TX_PIN);
    pinMode(RS485_DE_PIN, OUTPUT);
    digitalWrite(RS485_DE_PIN, LOW);
    rs485SetTX(false);
}

// Передача
void sendPacket_RS485(const full_packet_net_t* pkt)
{
    const size_t plen = sizeof(full_packet_net_t);
    static uint8_t buf[sizeof(full_packet_net_t)];
    memcpy(buf, pkt, plen);
    uint16_t crc = crc16_ccitt(buf, plen);

    xSemaphoreTake(serialMutex, portMAX_DELAY);
    rs485SetTX(true);
    digitalWrite(LED, LOW);
    RS485_SERIAL.write((uint8_t*)&PACKET_HEADER, sizeof(PACKET_HEADER));
    RS485_SERIAL.write(buf, plen);
    RS485_SERIAL.write((uint8_t*)&crc, sizeof(crc));
    RS485_SERIAL.write((uint8_t*)&PACKET_FOOTER, sizeof(PACKET_FOOTER));
    RS485_SERIAL.flush();
    delayMicroseconds(200);
    rs485SetTX(false);
    digitalWrite(LED, HIGH);
    xSemaphoreGive(serialMutex);
}

// Порционный парсер + быстрый ресинк (аналогично Приемнику)
bool receivePacket_RS485(full_packet_net_t* pkt, uint8_t* btn1, uint8_t* btn2)
{
    static uint8_t buffer[sizeof(full_packet_net_t) + 64];
    static size_t idx = 0;

    // --- "порция" чтения ---
    const int READ_BUDGET = 256;
    int read_left = READ_BUDGET;

    xSemaphoreTake(serialMutex, portMAX_DELAY);
    while (RS485_SERIAL.available() && read_left > 0) 
    { 
        int to_read = RS485_SERIAL.available();
        if (to_read > read_left) to_read = read_left;
        if (to_read + idx > (int)sizeof(buffer)) to_read = sizeof(buffer) - idx;
        if (to_read <= 0) break;
        int n = RS485_SERIAL.readBytes(&buffer[idx], to_read);
        idx += n;
        read_left -= n;
    }
    xSemaphoreGive(serialMutex);

    if (idx < 4) return false;

    // быстрый ресинк по 0xDD CC BB AA
    size_t start = 0;
    for (;;) 
    {
        if (idx - start < 4) 
        {
            if (start > 0) { memmove(buffer, buffer + start, idx - start); idx -= start; }
            return false;
        }
        if (buffer[start] == 0xDD && buffer[start + 1] == 0xCC && buffer[start + 2] == 0xBB && buffer[start + 3] == 0xAA) break;
        start++;
    }
    if (start > 0) 
    {
        memmove(buffer, buffer + start, idx - start);
        idx -= start;
    }

    const size_t frame_len = sizeof(full_packet_net_t) + 8;
    if (idx < frame_len) return false;
    const size_t footer_off = sizeof(full_packet_net_t) + 4 + 2;
    bool footer_ok = (buffer[footer_off] == 0xAA && buffer[footer_off + 1] == 0xBB &&
        buffer[footer_off + 2] == 0xCC && buffer[footer_off + 3] == 0xDD);
    if (!footer_ok) 
    {
        idx = 0;
        return false;
    }

    uint8_t* data = &buffer[4];
    uint16_t crc_rx = *(uint16_t*)&buffer[4 + sizeof(full_packet_net_t)];
    uint16_t crc_calc = crc16_ccitt(data, sizeof(full_packet_net_t));
    if (crc_rx != crc_calc) {
        idx = 0;
        return false;
    }

    memcpy(pkt, data, sizeof(full_packet_net_t));
    if (btn1) *btn1 = pkt->BUTTON1;
    if (btn2) *btn2 = pkt->BUTTON2;
    idx -= frame_len;
    if (idx > 0) memmove(buffer, buffer + frame_len, idx);

    return true;
}

// ----------- Задачи RX/TX -----------

void txTask(void* param)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    static full_packet_net_t packet = {};

    for (;;)
    {
        net_to_ufo_ThisAircraft(&ThisAircraft, &packet.ThisAircraft);

        xSemaphoreTake(containerMutex, portMAX_DELAY);
        for (int i = 0; i < MAX_TRACKING_OBJECTS; ++i)
            net_to_ufo_Container(&Container[i], &packet.Container[i]);
        xSemaphoreGive(containerMutex);

        memcpy(&packet.AuxData, &AuxData, sizeof(aux_t));
        packet.BUTTON1 = 0x01;
        packet.BUTTON2 = 0x02;

        sendPacket_RS485(&packet);
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(950));
    }
}

void rxTask(void* param)
{
    static full_packet_net_t recpkt;
    uint8_t btn1, btn2;
    for (;;)
    {
        if (receivePacket_RS485(&recpkt, &btn1, &btn2))
        {
            // Обработка мгновенного сигнала new_buttton_M
            if (recpkt.AuxData.new_buttton_M != 0)
            {
                Serial.print("MASTER: получено new_buttton_M=");
                Serial.println(recpkt.AuxData.new_buttton_M);
                // Тут ваша обработка кнопки или ответной логики
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// ----------- Остальное (setup/loop/печать) -----------

void printThisThisAircraft(const ufo_t* ac)
{
    Serial.printf("%06X:%d:%8s:%.0f:%.0f:%.0f:%.0f:%d:%.6f:%.6f\r\n",
        ac->addr, ac->squawk, ac->callsign, ac->altitude, ac->altitude, ac->speed,
        ac->course, ac->vert_rate, ac->latitude, ac->longitude
    );
}

void print_ThisContainer(const ufo_t* ac)
{
    Serial.printf("%06X:%d:%8s:%.0f:%.0f:%.0f:%.0f:%d:%.6f:%.6f:%d:%d:%d:%d\r\n",
        ac->addr, ac->squawk, ac->callsign, ac->altitude, ac->altitude, ac->speed,
        ac->course, ac->vert_rate, ac->latitude, ac->longitude,
        ac->rssi, ac->last_message_signal_strength_dbm, ac->last_message_signal_quality_db, ac->signal_source
    );
}

void printContainer(const ufo_t* arr, int n)
{
    for (int i = 0; i < n; ++i)
    {
        Serial.print("Container[");
        Serial.print(i); Serial.print("]:");
        print_ThisContainer(&arr[i]);
    }
}

void printAux(const aux_t* aux)
{
    Serial.println("=== AuxData ===");
    Serial.print("new_buttton_M: "); Serial.println(aux->new_buttton_M);
    Serial.print("new_message: "); Serial.println(aux->new_message);
    Serial.print("message_received: "); Serial.println(aux->message_received);
    Serial.print("confirm_message_M: "); Serial.println(aux->confirm_message_M);
    Serial.print("msg_resp_M: "); Serial.println(aux->msg_resp_M);
    Serial.print("Time_Hour_M: "); Serial.println(aux->Time_Hour_M);
    Serial.print("Time_Minute_M: "); Serial.println(aux->Time_Minute_M);
    Serial.print("new_SOS_flag_M: "); Serial.println(aux->new_SOS_flag_M);
    Serial.print("isValidGNSS_M: "); Serial.println(aux->isValidGNSS_M);
}

void setup()
{
    Serial.begin(115200);
    delay(500);

    pinMode(LED, OUTPUT);
    digitalWrite(LED, HIGH);

    serialMutex = xSemaphoreCreateMutex();
    containerMutex = xSemaphoreCreateMutex();

    setupRS485();
    xTaskCreatePinnedToCore(txTask, "TX", 8192, NULL, 2, NULL, 0);
    xTaskCreatePinnedToCore(rxTask, "RX", 8192, NULL, 1, NULL, 0);

    Serial.print("Sizeof full_packet_net_t: "); Serial.println(sizeof(full_packet_net_t));
    esp_task_wdt_init(10, false);

    Serial.println("** Setup END **");
}

void loop()
{
    static bool wdt_loop_registered = false;
    if (!wdt_loop_registered)
    {
        esp_task_wdt_add(NULL); // текущая задача - loopTask
        wdt_loop_registered = true;
    }
    esp_task_wdt_reset();

    // uncomment to see debug printout each second:
    /*
    static unsigned long previousMillis = 0;
    const long interval = 1000;
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval)
    {
        previousMillis = currentMillis;
        Serial.print("ThisAircraft:");
        printThisThisAircraft(&ThisAircraft);
        Serial.println("--------------------------------------------------------------------");
        printContainer(Container, MAX_TRACKING_OBJECTS);
        printAux(&AuxData);
        Serial.println("===========================================================================");
    }
    */
    delay(10);
}




//===============================================================================================
//#include <stdio.h>                // define I/O functions//#include "MAVLinkRF.h"
//#include <Arduino.h>              // define I/O functions
//#include "SPI.h"
//#include <esp_task_wdt.h>
//#include <iostream>
//#include <locale.h>
//#include <math.h>
//#include "freertos/FreeRTOS.h"
//#include "freertos/semphr.h"
//
//#include <TimeLib.h>
//
//TaskHandle_t Task1;
//TaskHandle_t Task2;
//
//SemaphoreHandle_t serialMutex;
//SemaphoreHandle_t containerMutex;
//
//int set_air = 0;   //  
//bool set_test_coordinate  = false; // Признак тестовых ввода текущих координат 
//bool set_test_coordinate5 = false; // Признак тестовых ввода текущих координат 
//
//
//#if !defined(SERIAL_FLUSH)
//#define SERIAL_FLUSH() Serial.flush()
//#endif
//
//#define RS485_SERIAL   Serial2
//#define RS485_TX_PIN   18
//#define RS485_RX_PIN   17
//#define RS485_DE_PIN   21
//#define RS485_BAUD     256000 //921600
//#define RS485_CONFIG   SERIAL_8N1
//#define LED            4
//
//const uint32_t PACKET_HEADER = 0xAABBCCDD;
//const uint32_t PACKET_FOOTER = 0xDDCCBBAA;
//
//#define BUFFER_SIZE           310
//#define MAX_TRACKING_OBJECTS   12
////#define RX_WDT_ITER_LIM      4096
//
//
//// ----------------- ДАННЫЕ -----------------
//typedef struct UFO {
//    uint8_t   raw[34];
//    time_t    timestamp;
//    uint8_t   protocol;
//    uint32_t  addr;
//    uint8_t   addr_type;
//    float     latitude;
//    float     longitude;
//    float     old_latitude;
//    float     old_longitude;
//    float     altitude;
//    float     pressure_altitude;
//    float     course;
//    float     speed;
//    uint8_t   aircraft_type;
//    char      callsign[8];
//    int       vert_rate;
//    int       squawk;
//    time_t    timemsg;
//    float     vs;
//    bool      stealth;
//    bool      no_track;
//    int8_t    ns[4];
//    int8_t    ew[4];
//    float     geoid_separation;
//    uint16_t  hdop;
//    int8_t    rssi;
//    float     distance;
//    float     bearing;
//    int8_t    alarm_level;
//    uint8_t   signal_source;
//    time_t    seen;
//    uint8_t   hour_msg;
//    uint8_t   min_msg;
//    uint16_t  delay_time_msg;
//    float     test_latitude;
//    float     test_longitude;
//    uint16_t  last_message_signal_strength_dbm;
//    uint16_t  last_message_signal_quality_db;
//} ufo_t;
//
//ufo_t ThisAircraft;
//ufo_t fo, Container[MAX_TRACKING_OBJECTS], EmptyFO, fo_msg, Container_msg[MAX_TRACKING_OBJECTS];
//
//
//
//typedef struct __attribute__((packed)) {
//    uint32_t addr;
//    int      squawk;
//    uint8_t  callsign[8];
//    float    altitude;
//    float    pressure_altitude;
//    float    course;
//    float    speed;
//    float    distance;
//    float    bearing;
//    int      vert_rate;
//    float    latitude;
//    float    longitude;
//    time_t   timestamp;
//    int8_t   rssi;
//    uint16_t last_message_signal_strength_dbm;
//    uint16_t last_message_signal_quality_db;
//    uint8_t   signal_source;
//} ufo_net_t;
//
//
//// Доп служебная структура
//typedef struct __attribute__((packed)) {
//    // bool     new_flag_M;            // Пока свободен
//    uint8_t  new_buttton_M;            // Состояние кнопки
//    bool     new_message;              // Флаг прихода нового сообщения
//    bool     message_received;         // Сообщение дисплеем получено 
//    bool     confirm_message_M;        // Подтвердить прочтение сообщения
//    char     msg_resp_M[BUFFER_SIZE];  // Строка сообщения
//    uint8_t  Time_Hour_M;              // Время час
//    uint8_t  Time_Minute_M;            // Время минуты
//    bool     new_SOS_flag_M;           // Флаг SOS
//    bool     isValidGNSS_M;            // Флаг получения координат
//} aux_t;
//
//
//aux_t AuxData;
//
//typedef struct __attribute__((packed)) {
//    ufo_net_t ThisAircraft;
//    ufo_net_t Container[MAX_TRACKING_OBJECTS];
//    aux_t AuxData;
//    uint8_t BUTTON1;
//    uint8_t BUTTON2;
//} full_packet_net_t;
//
//
////============================================================================
//// Прототипы
//uint16_t crc16_ccitt(const uint8_t* data, size_t len);
//void sendPacket_RS485(const full_packet_net_t* pkt);
//bool receivePacket_RS485(full_packet_net_t* pkt, uint8_t* btn1, uint8_t* btn2);
//void net_to_ufo_Container(const ufo_t* src, ufo_net_t* dst);
//void net_to_ufo_ThisAircraft(const ufo_t* src, ufo_net_t* dst);
//
//uint32_t swap32(uint32_t val)
//{
//    return ((val & 0xFF) << 24) |
//        ((val & 0xFF00) << 8) |
//        ((val & 0xFF0000) >> 8) |
//        ((val & 0xFF000000) >> 24);
//}
//
//float swapFloat(float val)
//{
//    uint32_t temp;
//    memcpy(&temp, &val, 4);
//    temp = swap32(temp);
//    float res;
//    memcpy(&res, &temp, 4);
//    return res;
//}
//
//
//uint16_t toBigEndian16(uint16_t val)
//{
//    // 0x00FF   – младший байт
//    // 0xFF00   – старший байт
//    // Сдвигаем их в противоположные позиции
//    return ((val & 0x00FF) << 8) |   // младший байт → старший
//        ((val & 0xFF00) >> 8);       // старший байт → младший
//}
//
//
////===================================================================================================================
//
//// копирование полей чужого самолета
//void net_to_ufo_Container(const ufo_t* src, ufo_net_t* dst)
//{
//    dst->addr = src->addr;
//    dst->squawk = src->squawk;
//    memcpy(dst->callsign, src->callsign, 8);
//    dst->altitude = src->altitude;
//    dst->pressure_altitude = src->pressure_altitude;
//    dst->course = src->course;
//    dst->speed = src->speed;
//    dst->distance = src->distance;
//    dst->bearing = src->bearing;
//    dst->vert_rate = src->vert_rate;
//    dst->latitude = src->latitude;
//    dst->longitude = src->longitude;
//    dst->timestamp = src->timestamp;
//    dst->rssi = src->rssi;
//    dst->last_message_signal_strength_dbm = src->last_message_signal_strength_dbm;
//    dst->last_message_signal_quality_db = src->last_message_signal_quality_db;
//    dst->signal_source = src->signal_source;
//}
//
//// копирование полей нашего самолета
//void net_to_ufo_ThisAircraft(const ufo_t* src, ufo_net_t* dst)
//{
//    dst->addr = src->addr;
//    dst->squawk = src->squawk;
//    memcpy(dst->callsign, src->callsign, 8);
//    dst->altitude = src->altitude;
//    dst->pressure_altitude = src->pressure_altitude;
//    dst->course = src->course;
//    dst->speed = src->speed;
//    dst->vert_rate = src->vert_rate;
//    dst->latitude = src->latitude;
//    dst->longitude = src->longitude;
//}
//
//
//static void rs485SetTX(bool enable)
//{
//    digitalWrite(RS485_DE_PIN, enable ? HIGH : LOW);
//    if (enable) delayMicroseconds(50);
//}
//
//void setupRS485()
//{
//    RS485_SERIAL.setRxBufferSize(1024);
//    RS485_SERIAL.begin(RS485_BAUD, RS485_CONFIG, RS485_RX_PIN, RS485_TX_PIN);
//    pinMode(RS485_DE_PIN, OUTPUT);
//    digitalWrite(RS485_DE_PIN, LOW);
//    rs485SetTX(false);
//}
//
//
//uint16_t crc16_ccitt(const uint8_t* data, size_t len) 
//{
//    uint16_t crc = 0x0000;
//    for (size_t i = 0; i < len; ++i) {
//        crc ^= (uint16_t)data[i] << 8;
//        for (uint8_t j = 0; j < 8; ++j)
//            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
//    }
//    return crc;
//}
//
//void sendPacket_RS485(const full_packet_net_t* pkt) 
//{
//    const size_t plen = sizeof(full_packet_net_t);
//    static uint8_t buf[sizeof(full_packet_net_t)];    memcpy(buf, pkt, plen);
//    uint16_t crc = crc16_ccitt(buf, plen);
//    esp_task_wdt_reset();
//    xSemaphoreTake(serialMutex, portMAX_DELAY);
//    rs485SetTX(true);
//    digitalWrite(LED, LOW);
//    RS485_SERIAL.write((uint8_t*)&PACKET_HEADER, sizeof(PACKET_HEADER));
//    RS485_SERIAL.write(buf, plen);
//    RS485_SERIAL.write((uint8_t*)&crc, sizeof(crc));
//    RS485_SERIAL.write((uint8_t*)&PACKET_FOOTER, sizeof(PACKET_FOOTER));
//    RS485_SERIAL.flush();
//    delayMicroseconds(200);
//    rs485SetTX(false);
//    digitalWrite(LED, HIGH);
//    esp_task_wdt_reset();
//    xSemaphoreGive(serialMutex);
//  //  Serial.println("Пакет отправлен!");
//}
//
//bool receivePacket_RS485(full_packet_net_t* pkt, uint8_t* btn1, uint8_t* btn2) 
//{
//    static uint8_t buffer[sizeof(full_packet_net_t) + 32];
//    static size_t idx = 0;
//    while (RS485_SERIAL.available()) 
//    {
//        int bytes = RS485_SERIAL.available();
//        if (bytes + idx > sizeof(buffer)) bytes = sizeof(buffer) - idx;
//        int n = RS485_SERIAL.readBytes(&buffer[idx], bytes);
//        idx += n;
//    }
//    esp_task_wdt_reset();
//    while (idx >= sizeof(full_packet_net_t) + 8)
//    {
//        bool header_ok = (buffer[0] == 0xDD && buffer[1] == 0xCC &&
//            buffer[2] == 0xBB && buffer[3] == 0xAA);
//        size_t footer_off = sizeof(full_packet_net_t) + 4 + 2;
//        bool footer_ok = (buffer[footer_off] == 0xAA && buffer[footer_off + 1] == 0xBB &&
//            buffer[footer_off + 2] == 0xCC && buffer[footer_off + 3] == 0xDD);
//
//        if (header_ok && footer_ok) 
//        {
//            uint8_t* data = &buffer[4];
//            uint16_t crc_rx = *(uint16_t*)&buffer[4 + sizeof(full_packet_net_t)];
//            uint16_t crc_calc = crc16_ccitt(data, sizeof(full_packet_net_t));
//            if (crc_rx == crc_calc) 
//            {
//                memcpy(pkt, data, sizeof(full_packet_net_t));
//                //if (btn1) *btn1 = pkt->BUTTON1;
//                //if (btn2) *btn2 = pkt->BUTTON2;
//                size_t msgLen = sizeof(full_packet_net_t) + 8;
//                idx -= msgLen;
//                if (idx)
//                    memmove(buffer, buffer + msgLen, idx);
//                else
//                    idx = 0;
//                return true;
//            }
//        }
//        memmove(buffer, buffer + 1, --idx);
//        esp_task_wdt_reset();
//    }
//    return false;
//}
//
//void txTask(void* param) 
//{
//    esp_task_wdt_add(NULL); // текущая задача
//    TickType_t xLastWakeTime = xTaskGetTickCount();
//    static full_packet_net_t packet = {};
// 
//    for (;;) 
//    {
//        esp_task_wdt_reset();
//  
//          net_to_ufo_ThisAircraft(&ThisAircraft, &packet.ThisAircraft);
//
//        xSemaphoreTake(containerMutex, portMAX_DELAY);
//
//        for (int i = 0; i < MAX_TRACKING_OBJECTS; ++i)
//        {
//            net_to_ufo_Container(&Container[i], &packet.Container[i]);
//        }
//
//        xSemaphoreGive(containerMutex);
//        // Доп.флаги
//        //AuxData.new_message = service.getNewMessageFlag();               // Получить признак прихода нового сообщения 
//        //AuxData.confirm_message_M = service.get_confirm_message();       // Разрешить отправить подтверждение прочтения сообщения.
//        //AuxData.Time_Hour_M = service.get_time_hour();
//        //AuxData.Time_Minute_M = service.get_time_minute();
//        ////AuxData.new_SOS_flag_M = digitalRead(SOC_GPIO_PIN_SOS);          // Получить признак состояния кнопки SOS;
//        //if (service.getNewMessageFlag())                                 // Отправить сообщение
//        //{
//        //    memset(AuxData.msg_resp_M, 0, sizeof(AuxData.msg_resp_M));
//        //    //strncpy(AuxData.msg_resp_M, CommandHandler.msg_tmp_all, strlen(CommandHandler.msg_tmp_all));
//        //    //memset(CommandHandler.msg_tmp_all, 0, sizeof(CommandHandler.msg_tmp_all));
//        //    service.setNewMessageFlag(false); // Сбросить флаг нового сообщения
//        //}
//        //AuxData.isValidGNSS_M = (bool)service.get_GNSS_on_off();
//
//        memcpy(&packet.AuxData, &AuxData, sizeof(aux_t));
//        packet.BUTTON1 = 0x01;
//        packet.BUTTON2 = 0x02;
//        sendPacket_RS485(&packet);
//        esp_task_wdt_reset();
//        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(950));
// 
//    }
//}
//
//void rxTask(void* param) 
//{
//    esp_task_wdt_add(NULL); // текущая задача
//    static full_packet_net_t recpkt;
//    uint8_t btn1, btn2;
//    for (;;) 
//    {
//        esp_task_wdt_reset();
//        if (receivePacket_RS485(&recpkt, &btn1, &btn2)) 
//        {
//            // Сразу ответ по новым событиям new_buttton_M!
//            if (recpkt.AuxData.new_buttton_M != 0)
//            {
//                Serial.print("MASTER: получено new_buttton_M=");
//                Serial.println(recpkt.AuxData.new_buttton_M);
//                //service.set_num_buttton(recpkt.AuxData.new_buttton_M);
//                // сбрасывать на MASTER не нужно, просто реагируй и дожидайся следующего значения
//            }
//
//            //if (btn1 == 0 || btn2 == 0)
//            //{
//            //    service.set_num_buttton(btn1);
//            //    Serial.print("Button states from Slave: ");
//            //    Serial.print(btn1);
//            //    Serial.print(' ');
//            //    Serial.println(btn2);
//            //}
//        }
//        vTaskDelay(pdMS_TO_TICKS(20));
//    }
//}
//
//
////==================================================================================================================
//
//
//unsigned long previousMillis = 0;            //  
//const long interval = 1000;                  //  
//
//
//
//void printThisThisAircraft(const ufo_t* ac)
//{
//    Serial.printf("%06X:%d:%8s:%.0f:%.0f:%.0f:%.0f:%d:%.6f:%.6f\r\n",
//        ac->addr,                   // Адрес устройства стороннего самолета
//        ac->squawk,                 // Номер, назначаемый диспетчером для обмена с локатором. 
//        ac->callsign,               // Номер рейса
//        ac->altitude,
//        ac->altitude,
//        ac->speed,
//        ac->course,
//        ac->vert_rate,
//        ac->latitude,
//        ac->longitude
//    );
//}
//
//
//void print_ThisContainer(const ufo_t* ac)
//{
//    Serial.printf("%06X:%d:%8s:%.0f:%.0f:%.0f:%.0f:%d:%.6f:%.6f:%d:%d:%d:%d\r\n",
//        ac->addr,                   // Адрес устройства стороннего самолета
//        ac->squawk,                 // Номер, назначаемый диспетчером для обмена с локатором. 
//        ac->callsign,               // Номер рейса
//        ac->altitude,
//        ac->altitude,
//        ac->speed,
//        ac->course,
//        ac->vert_rate,
//        ac->latitude,
//        ac->longitude,
//        ac->rssi,
//        ac->last_message_signal_strength_dbm,
//        ac->last_message_signal_quality_db,
//        ac->signal_source
//    );
//}
//
//
//void printContainer(const ufo_t* arr, int n)
//{
//    for (int i = 0; i < n; ++i)
//    {
//        Serial.print("Container[");
//        Serial.print(i); Serial.print("]:");
//        print_ThisContainer(&arr[i]);
//    }
//    esp_task_wdt_reset();
//}
//
//
//
//void printAux(const aux_t* aux)
//{
//    Serial.println("=== AuxData ===");
//    Serial.print("new_buttton_M: "); Serial.println(aux->new_buttton_M);  // Состояние кнопки
//    Serial.print("new_message: "); Serial.println(aux->new_message);
//    Serial.print("message_received: "); Serial.println(aux->message_received);
//    Serial.print("confirm_message_M"); Serial.println(aux->confirm_message_M);
//    Serial.print("msg_resp_M: "); Serial.println(aux->msg_resp_M);
//    Serial.print("Time_Hour_M: "); Serial.println(aux->Time_Hour_M);
//    Serial.print("Time_Minute_M: "); Serial.println(aux->Time_Minute_M);
//    Serial.print("new_SOS_flag_M: "); Serial.println(aux->new_SOS_flag_M);
//    Serial.print("isValidGNSS_M: "); Serial.println(aux->isValidGNSS_M);
//}
//
//
//
////===============================================================================================
//
//
//
//void setup()
//{
//    Serial.begin(115200);
//    delay(500);
//
//    Serial.println("Setup start"); 
//    SERIAL_FLUSH();
//   
//    //----------------------------------------------------------------------------------------------
// 
//        pinMode(LED, OUTPUT);
//        digitalWrite(LED, HIGH);
//
//        serialMutex = xSemaphoreCreateMutex();
//        containerMutex = xSemaphoreCreateMutex();
//
//        setupRS485();
//        xTaskCreatePinnedToCore(txTask, "TX", 8192, NULL, 2, &Task1, 0);
//        xTaskCreatePinnedToCore(rxTask, "RX", 8192, NULL, 1, &Task2, 0);
//
//        Serial.print("Sizeof full_packet_net_t: "); Serial.println(sizeof(full_packet_net_t));
//
//
//        // Все задачи на ядре 0
////------------------------------------------------------------------------
//    esp_task_wdt_init(10, false); // таймаут 10 сек, reset chip=true
//  
//    Serial.println("** Setup END **");
//}
//
//bool new_SOS_flag_M_tmp = false;
//
//void loop()
//{
//    static bool wdt_loop_registered = false;
//    if (!wdt_loop_registered)
//    {
//        esp_task_wdt_add(NULL); // текущая задача - loopTask
//        wdt_loop_registered = true;
//    }
//
//
//    esp_task_wdt_reset();
//
//
//  unsigned long currentMillis = millis();
//
//  /*  if (currentMillis - previousMillis >= interval)
//    {
//        previousMillis = currentMillis;
//        Serial.print("ThisAircraft:");
//        printThisThisAircraft(&ThisAircraft);
//        Serial.println("--------------------------------------------------------------------");
//        printContainer(Container, MAX_TRACKING_OBJECTS);
//        printAux(&AuxData);
//        Serial.println("===========================================================================");
// 
//    }*/
//
//}
//
