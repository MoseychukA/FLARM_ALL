#include <stdio.h>                // define I/O functions
#include <Arduino.h>              // define I/O functions
#include "SPI.h"
#include <esp_task_wdt.h>
#include <iostream>
#include <locale.h>
#include <math.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"

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
#include "MAVLinkRF.h"
#include "GDL90.h"
#include "TrafficHelper.h"
#include "ESP32RF.h"
#include <TimeLib.h>
#include <TinyGPS++.h>
#include "ServiceMain.h"
#include "Configuration_ESP32.h"
#include "CoreCommandBuffer.h"    // обработчик входящих по UART команд
#include "Button.h"
#include <HardwareSerial.h>
#include "SoftRF.h"
#include <Wire.h>

#include "LANRF.h"

//const uint16_t dstPort = 10110;       // Порт UDP



//#include <Ethernet2.h>
//#include <EthernetUdp2.h>         // UDP library from: bjoern@cs.stanford.edu 12/30/2008
//
//// Enter a MAC address and IP address for your controller below.
//// The IP address will be dependent on your local network:
//byte mac[] = {
//  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
//};
//
////IPAddress ip(192, 168, 75, 247);
//
//unsigned int localPortLan = 10110;      // local port to listen on
//
//// buffers for receiving and sending data
//char packetBuffer[UDP_TX_PACKET_MAX_SIZE]; //buffer to hold incoming packet,
//char ReplyBuffer[] = "ack now UDP ged";       // a string to send back
//
//
//static EthernetUDP Udp;
//
//// Must connect INT to GPIOxx or not working
//#define MISO_GPIO           13
//#define MOSI_GPIO           11
//#define SCK_GPIO            12
//#define CS_GPIO             19
//#define RST_GPIO            20
//#define INT_GPIO            16

TaskHandle_t Task1, Task2;
SemaphoreHandle_t serialMutex;

int set_air = 0;   //  
bool set_test_coordinate = false; // Признак тестовых ввода текущих координат 
bool set_test_coordinate5 = false; // Признак тестовых ввода текущих координат 

void txrx_test();

int threshold_level_tmp = 300;
#if !defined(SERIAL_FLUSH)
#define SERIAL_FLUSH() Serial.flush()
#endif

#define isTimeToDisplay() (millis() - LEDTimeMarker     > 1000)
#define isTimeToExport()  (millis() - ExportTimeMarker  > 1000)

//=================================================================
#define RS485_SERIAL   Serial2
#define RS485_TX_PIN   18
#define RS485_RX_PIN   17
#define RS485_DE_PIN   21
#define RS485_BAUD     115200// 256000 //921600
#define RS485_CONFIG   SERIAL_8N1
#define LED            4  

const uint32_t PACKET_HEADER = 0xAABBCCDD;
const uint32_t PACKET_FOOTER = 0xDDCCBBAA;

ufo_t ThisAircraft;
extern ufo_t fo, Container[MAX_TRACKING_OBJECTS];

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
    int8_t   rssi_LoRa;
    int8_t   rssi_rp2040;
    uint8_t  signal_source;
} ufo_net_t;

typedef struct __attribute__((packed)) {
    uint8_t  new_button_M;
    bool     new_message;
    bool     message_received;
    bool     confirm_message_M;
    char     msg_resp_M[BUFFER_SIZE];
    uint8_t  Time_Hour_M;
    uint8_t  Time_Minute_M;
    bool     new_SOS_flag_M;
    bool     isValidGNSS_M;
} aux_t;

typedef struct __attribute__((packed)) {
    ufo_net_t ThisAircraft;
    ufo_net_t Container[MAX_TRACKING_OBJECTS];
    aux_t     AuxData;
    uint8_t   BUTTON1;
    uint8_t   BUTTON2;
} full_packet_net_t;

aux_t AuxData;

//============================================================================
// Прототипы
uint16_t crc16_ccitt(const uint8_t* data, size_t len);
void sendPacket_RS485(const full_packet_net_t* pkt);
bool receivePacket_RS485(full_packet_net_t* pkt, uint8_t* btn1, uint8_t* btn2);
void net_to_ufo_Container(const ufo_t* src, ufo_net_t* dst);
void net_to_ufo_ThisAircraft(const ufo_t* src, ufo_net_t* dst);

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

// ============================ DUMP1090 ==========================================
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
    memcpy(out->callsign, in->callsign, 16);
    out->altitude = swap32(in->altitude);
    out->speed = swap32(in->speed);
    out->course = swap32(in->course);
    out->vert_rate = swap32(in->vert_rate);
    out->lat_msg = swapFloat(in->lat_msg);
    out->lon_msg = swapFloat(in->lon_msg);
    out->rssi_rp2040 = in->rssi_rp2040;
}

//===================================================================================================================

// копирование полей чужого самолета
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
    dst->rssi_LoRa = src->rssi_LoRa;
    dst->rssi_rp2040 = src->rssi_rp2040;
    dst->signal_source = src->signal_source;
}

// копирование полей нашего самолета
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


uint16_t crc16_ccitt(const uint8_t* data, size_t len)
{
    uint16_t crc = 0x0000;
    for (size_t i = 0; i < len; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; ++j)
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
    }
    return crc;
}


// ============== ПЕРЕДАЧА ПАКЕТА ===================
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


// ================ ПРИЁМ ОТВЕТНОГО ПАКЕТА ================
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

    const size_t frame_len = sizeof(full_packet_net_t) + 10;
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


//// ================ ЗАДАЧА ПЕРЕДАЧИ ================

unsigned long previousMillis = 0;            //  
const long interval = 1000;                  //  
unsigned long thresholdMillis = 0;            //  
const long threshold_interval = 10660;                  //  


void txTask_test()
{
    static full_packet_net_t packet = {};

    net_to_ufo_ThisAircraft(&ThisAircraft, &packet.ThisAircraft);

    for (int i = 0; i < MAX_TRACKING_OBJECTS; ++i)
    {
        net_to_ufo_Container(&Container[i], &packet.Container[i]);
    }
 
    // Доп.флаги
    AuxData.new_message = service.getNewMessageFlag();               // Получить признак прихода нового сообщения 
    AuxData.confirm_message_M = service.get_confirm_message();       // Разрешить отправить подтверждение прочтения сообщения.
    AuxData.Time_Hour_M = service.get_time_hour();
    AuxData.Time_Minute_M = service.get_time_minute();
    AuxData.new_SOS_flag_M = digitalRead(SOC_GPIO_PIN_SOS);          // Получить признак состояния кнопки SOS;

    if (service.getNewMessageFlag())
    {
        // Безопасное копирование в msg_resp_M
        strlcpy(AuxData.msg_resp_M, CommandHandler.msg_tmp_all, sizeof(AuxData.msg_resp_M));

        // Очистить исходный буфер
        memset(CommandHandler.msg_tmp_all, 0, sizeof(CommandHandler.msg_tmp_all));

        service.setNewMessageFlag(false);
    }

    AuxData.isValidGNSS_M = (bool)service.get_GNSS_on_off(); 

    memcpy(&packet.AuxData, &AuxData, sizeof(aux_t));
    packet.BUTTON1 = 0x01;
    packet.BUTTON2 = 0x02;
    sendPacket_RS485(&packet);
}

// ================ ЗАДАЧА ПРИЁМА ================
 
 
void rxTask(void* param)
{
    esp_task_wdt_add(NULL); // текущая задача - loopTask
    static full_packet_net_t recpkt;
    uint8_t btn1, btn2;
    for (;;)
    {
        esp_task_wdt_reset();
        if (receivePacket_RS485(&recpkt, &btn1, &btn2))
        {
            // Обработка мгновенного сигнала new_button_M
            if (recpkt.AuxData.new_button_M != 0)
            {
                service.set_num_button(recpkt.AuxData.new_button_M);
               //   Serial.printf("Принят ответ: BUTTON1=%u BUTTON2=%u new_button_M=%u \r\n",  recpkt.BUTTON1, recpkt.BUTTON2, recpkt.AuxData.new_button_M);
                  // Здесь можно сразу отреагировать на приём
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}


//==================================================================================================================

void receiveRP2040(void* param)
{
    esp_task_wdt_add(NULL); // текущая задача - loopTask
    for (;;)
    {

        rxIndex = 0;
        esp_task_wdt_reset();
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

                    //Обработка
                    fo = EmptyFO;
                    fo.addr = packet.addr;
                    fo.squawk = packet.squawk;
                    memcpy((char*)fo.callsign, packet.callsign, 8/*strlen(packet.callsign)*/);
                    fo.altitude = packet.altitude * 0.3048;
                    fo.pressure_altitude = packet.altitude * 0.3048;
                    fo.speed = packet.speed;
                    fo.course = packet.course;
                    fo.vert_rate = packet.vert_rate; // Уточнить!
                    fo.latitude = packet.lat_msg;
                    fo.longitude = packet.lon_msg;
                    fo.rssi_rp2040 = packet.rssi_rp2040;
                    fo.signal_source = 1;
                    fo.aircraft_type = AIRCRAFT_TYPE_JET;
                    fo.timestamp = now(); /*packet.seen_time;*/ // 

                    if(service.lockContainer())
                    {
                        Traffic_Update(&fo);
                        Traffic_Add(&fo);
                        service.unlockContainer();
                    }

                    // Очистка буфера для нового пакета 
                    memset(&packet, 0, sizeof(packet)); // Очистить массив
                    memset(&rxBuffer, 0, sizeof(rxBuffer)); // Очистить массив
                    rxIndex = 0;  // Готов к приему нового пакета.
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

//============================ Передача числа настройки ШИМ в RP2040 ===============================================

#define START_MARK 0x55
#define END_MARK  0xAAAA
uint16_t values[3] = {};  // Тест

void sendPacketToRP2040(uint16_t* data, uint8_t count)
{
    SerialRP2040.write(START_MARK);         // стартовый байт (0x55)
    SerialRP2040.write(count);              // сколько чисел

    for (uint8_t i = 0; i < count; ++i)
    {
        // Отправим big-endian (старший байт первым)
        SerialRP2040.write(highByte(data[i]));
        SerialRP2040.write(lowByte(data[i]));
    }
    // Конечный маркер, big-endian
    SerialRP2040.write(highByte(END_MARK));
    SerialRP2040.write(lowByte(END_MARK));
    SerialRP2040.flush();
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


void printThisThisAircraft(const ufo_t* ac)
{
    Serial.printf("%06X:%d:%8s:%.0f:%.0f:%.0f:%.0f:%d:%.6f:%.6f\r\n",
        ac->addr,                   // Адрес устройства стороннего самолета
        ac->squawk,                 // Номер, назначаемый диспетчером для обмена с локатором. 
        ac->callsign,               // Номер рейса
        ac->altitude,
        ac->altitude,
        ac->speed,
        ac->course,
        ac->vert_rate,
        ac->latitude,
        ac->longitude
    );
    vTaskDelay(pdMS_TO_TICKS(4));
}


void print_ThisContainer(const ufo_t* ac)
{
    Serial.printf("%06X:%d:%8s:%.0f:%.0f:%.0f:%.0f:%d:%.6f:%.6f:%d:%d:%d\r\n",
        ac->addr,                   // Адрес устройства стороннего самолета
        ac->squawk,                 // Номер, назначаемый диспетчером для обмена с локатором. 
        ac->callsign,               // Номер рейса
        ac->altitude,
        ac->altitude,
        ac->speed,
        ac->course,
        ac->vert_rate,
        ac->latitude,
        ac->longitude,
        ac->rssi_LoRa,
        ac->rssi_rp2040,
        ac->signal_source
    );
    vTaskDelay(pdMS_TO_TICKS(4));
}


void printContainer(const ufo_t* arr, int n)
{
    for (int i = 0; i < n; ++i)
    {
        Serial.print("Container[");
        Serial.print(i); Serial.print("]:");
        print_ThisContainer(&arr[i]);
    }
    vTaskDelay(pdMS_TO_TICKS(4));
}



void printAux(const aux_t* aux)
{
    Serial.println("=== AuxData ===");
    Serial.print("new_buttton_M: "); Serial.println(aux->new_button_M);  // Состояние кнопки
    Serial.print("new_message: "); Serial.println(aux->new_message);
    Serial.print("message_received: "); Serial.println(aux->message_received);
    Serial.print("confirm_message_M"); Serial.println(aux->confirm_message_M);
    Serial.print("msg_resp_M: "); Serial.println(aux->msg_resp_M);
    Serial.print("Time_Hour_M: "); Serial.println(aux->Time_Hour_M);
    Serial.print("Time_Minute_M: "); Serial.println(aux->Time_Minute_M);
    Serial.print("new_SOS_flag_M: "); Serial.println(aux->new_SOS_flag_M);
    Serial.print("isValidGNSS_M: "); Serial.println(aux->isValidGNSS_M);
    vTaskDelay(pdMS_TO_TICKS(4));
}

// ================= Кнопка: задача обработки =================
// ----------------- Параметры -----------------
const int   BUTTON_PIN = 48;   // Пин кнопки (подтягивается к VCC через pull‑up)
const uint32_t DEBOUNCE_MS = 20;   // анти‑дребезг (мс)
const uint32_t LONG_PRESS_MS = 800;  // порог длительного удержания (мс)
const uint32_t DOUBLE_CLICK_MAX_MS = 400; // максимум между двумя кликами (мс)

// ----------------- Переменные задачи -----------------
TaskHandle_t Task3 = nullptr;

// ----------------- Состояния FSM -----------------
enum BtnState { IDLE, PRESSED, RELEASED };

void ButtonTask(void* pvParameters) {
    pinMode(BUTTON_PIN, INPUT_PULLUP);   // Кнопка замыкает в GND

    BtnState   state = IDLE;
    uint32_t   lastChangeTime = 0;          // время последней стабилизации уровня
    uint32_t   pressStartTime = 0;          // когда кнопка была нажата
    uint32_t   firstReleaseTime = 0;        // время первого отпускания (для двойного)
    uint8_t    clickCount = 0;              // сколько раз уже отпустили
    bool       longPressDetected = false;   // был ли уже зафиксирован Long press

    for (;;) {
        bool rawPressed = (digitalRead(BUTTON_PIN) == LOW); // LOW = нажата
        uint32_t now = millis();

        /* ------------------- Дебаунс ------------------- */
        if (rawPressed != (state == PRESSED)) {              // уровень изменился
            if (now - lastChangeTime >= DEBOUNCE_MS) 
            {
                lastChangeTime = now;                        // запомнили момент стабилизации

                if (rawPressed) 
                {                                            // ---------- Нажата ----------
                    state = PRESSED;
                    pressStartTime = now;
                    longPressDetected = false;               // сбрасываем флаг при новом нажатии
                }
                else 
                {                                           // ---------- Отпущена ----------
                    state = RELEASED;
                    // Если уже зафиксирован long press, игнорируем клик полностью
                    if (longPressDetected) {
                        // Сразу переходим в IDLE – клик не считается
                        clickCount = 0;
                        longPressDetected = false;
                        state = IDLE;
                    }
                    else {
                        // Обычный клик – учитываем
                        clickCount++;
                        firstReleaseTime = now;
                    }
                }
            }
        }

        /* ------------------- Обработка состояний ------------------- */
        switch (state) {

        case PRESSED: {
            // Проверяем длительное удержание
            if (now - pressStartTime >= LONG_PRESS_MS && !longPressDetected) 
            {
                service.set_num_button(3);
               // onLongPress();                     // сразу оповещаем о длительном удержании
                longPressDetected = true;            // помечаем, что событие уже сгенерировано
            }
            break;
        }

        case RELEASED: {
            // Если уже был long press – ничего не делаем (см. выше)
            if (longPressDetected) 
            {
                // Уже сбросились в IDLE, так что сюда не попадаем
                break;
            }

            // Ожидаем возможность второго клика
            if (clickCount == 1) 
            {
                // Если прошёл таймаут без второго клика → одиночный клик
                if (now - firstReleaseTime > DOUBLE_CLICK_MAX_MS) 
                {
                    service.set_num_button(1);
                   // onSingleClick();
                    clickCount = 0;
                    state = IDLE;
                }
                // иначе остаёмся в RELEASED, ждём второй клик
            }
            else if (clickCount == 2) 
            {
                // Два клика за короткое время → двойной
                service.set_num_button(2);
               // onDoubleClick();
                clickCount = 0;
                state = IDLE;
            }
            break;
        }

        case IDLE:
        default:
            // Ничего не делаем
            break;
        }

        // Немного «отдыхаем», чтобы не загружать процессор
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    // На практике сюда никогда не попадаем
    vTaskDelete(nullptr);
}


//void setupEthernetAndUDP()
//{
//    IPAddress localIP(settings->g_localIP[0], settings->g_localIP[1], settings->g_localIP[2], settings->g_localIP[3]);
//    IPAddress gatewayIP(settings->g_gatewayIP[0], settings->g_gatewayIP[1], settings->g_gatewayIP[2], settings->g_gatewayIP[3]);
//    IPAddress subnetMaskUDP(settings->g_subnetMask[0], settings->g_subnetMask[1], settings->g_subnetMask[2], settings->g_subnetMask[3]);
//    IPAddress dnsServer(settings->g_dns_server[0], settings->g_dns_server[1], settings->g_dns_server[2], settings->g_dns_server[3]);
//    Ethernet.begin(mac, localIP, dnsServer, gatewayIP, subnetMask);
//    Udp.begin(localPortLan);
//    Serial.print(F("UDP listening on port "));
//    Serial.println(localPortLan);
//}


//===============================================================================================
void setup()
{
    pinMode(LED, OUTPUT);             // устанавливает режим работы
    digitalWrite(LED, HIGH);
    //pinMode(CS_GPIO, OUTPUT);         // устанавливает режим работы
    //digitalWrite(CS_GPIO, HIGH);
    //pinMode(RST_GPIO, OUTPUT);        // устанавливает режим работы
    //digitalWrite(RST_GPIO, HIGH);
    pinMode(46, OUTPUT);              // устанавливает режим работы
    digitalWrite(46, HIGH);
    //pinMode(INT_GPIO, INPUT);         // устанавливает режим работы

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

    SERIAL_FLUSH();

    EEPROM_setup();

    ThisAircraft.addr = SoC->getChipId() & 0x00FFFFFF;

    hw_info.rf = RF_setup();

    delay(100);

    hw_info.display = SoC->Display_setup();

#if !defined(EXCLUDE_MAVLINK)
    if (settings->mode == FLYRF_MODE_UAV)
    {
        Serial.begin(57600);
        MAVLink_setup();
        ThisAircraft.aircraft_type = AIRCRAFT_TYPE_UAV;
    }
    else
#endif /* EXCLUDE_MAVLINK */

    if (settings->input_coordinates == IMPUT_COORD_MANUAL)
    {
        ThisAircraft.local_latitude = settings->local_latitude;
        ThisAircraft.local_longitude = settings->local_longitude;
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
  
    SoC->post_init();

    heap_caps_malloc_extmem_enable(8000); //Use PSRAM for memory requests larger than 1,000 bytes
    CommandHandler.setup();

  //------------------------------------------------------------------------------

    setupRS485();

    serialMutex = xSemaphoreCreateMutex();
    service.initContainerMutex();

    xTaskCreatePinnedToCore(rxTask, "RX", 8192, NULL, 1, &Task1, 0);
    xTaskCreatePinnedToCore(receiveRP2040, "RP2040", 8192, NULL, 2, &Task2, 0);
 
      // Создаём задачу, привязанную к ядру 0
    xTaskCreatePinnedToCore(
        ButtonTask,        // функция задачи
        "BtnTask",         // имя задачи (для отладки)
        4096,              // стек (байт)
        nullptr,           // параметр задачи
        1,                 // приоритет
        &Task3,            // дескриптор
        0                  // ядро (0 или 1)
    );

 
    if (settings->threshold_level != threshold_level_tmp)
    {
        threshold_level_tmp = settings->threshold_level;
        values[0] = settings->threshold_level;
        sendPacketToRP2040(values, 1);
    }

    LAN_setup();
    //setupEthernetAndUDP();

    delay(500);

    Serial.println("** Setup END **");
    SoC->WDT_setup();
}

void loop()
{
    static bool wdt_loop_registered = false;
    if (!wdt_loop_registered)
    {
        esp_task_wdt_add(NULL); // текущая задача - loopTask
        wdt_loop_registered = true;
    }

    RF_loop();                       // Сначала выполните общие действия с радиочастотами

    txrx_test();

    SoC->loop();

    SoC->Display_loop();
 
    WiFi_loop();   // Handle DNS
 
    Web_loop();  // Handle Web

    OTA_loop();  // Handle OTA update.

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
            Serial.print("ThisAircraft:");
            printThisThisAircraft(&ThisAircraft);
            Serial.println("--------------------------------------------------------------------");
            printContainer(Container, MAX_TRACKING_OBJECTS);
            Serial.println("===========================================================================");
        }

        if (settings->serial_out == SEND_SERIAL_TECHNICAL_INFO && settings->nmea_out != NMEA_UART)
        {
            Serial.print("ThisAircraft:");
            printThisThisAircraft(&ThisAircraft);
            Serial.println("--------------------------------------------------------------------");
            printContainer(Container, MAX_TRACKING_OBJECTS);
            printAux(&AuxData);
            Serial.println("===========================================================================");
        }

         txTask_test();
    }

    LAN_loop();
    //// if there's data available, read a packet
    //int packetSize = Udp.parsePacket();
    //if (packetSize)
    //{
    //    Serial.print("Received packet of size ");
    //    Serial.println(packetSize);
    //    Serial.print("From ");
    //    IPAddress remote = Udp.remoteIP();
    //    for (int i = 0; i < 4; i++)
    //    {
    //        Serial.print(remote[i], DEC);
    //        if (i < 3)
    //        {
    //            Serial.print(".");
    //        }
    //    }
    //    Serial.print(", port ");
    //    Serial.println(Udp.remotePort());

    //    // read the packet into packetBufffer
    //    Udp.read(packetBuffer, UDP_TX_PACKET_MAX_SIZE);
    //    Serial.println("Contents:");
    //    Serial.println(packetBuffer);

    //    // send a reply, to the IP address and port that sent us the packet we received
    //    Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
    //    Udp.write(ReplyBuffer);
    //    Udp.endPacket();
    //}
  esp_task_wdt_reset();
  Time_loop();

  yield();
}



#if !defined(EXCLUDE_MAVLINK)
void uav()
{
    bool success = false;

    PickMAVLinkFix();

    MAVLinkTimeSync();
    MAVLinkSetWiFiPower();

    ThisAircraft.timestamp = now();

    if (isValidMAVFix())
    {
        ThisAircraft.latitude = the_aircraft.location.gps_lat / 1e7;
        ThisAircraft.longitude = the_aircraft.location.gps_lon / 1e7;
        ThisAircraft.altitude = the_aircraft.location.gps_alt / 1000.0;
        ThisAircraft.course = the_aircraft.location.gps_cog;
        ThisAircraft.speed = (the_aircraft.location.gps_vog / 100.0) / _GPS_MPS_PER_KNOT;
        ThisAircraft.hdop = the_aircraft.location.gps_hdop;
        ThisAircraft.pressure_altitude = the_aircraft.location.baro_alt;

        RF_Transmit(RF_Encode(&ThisAircraft), true);
    }

    success = RF_Receive();

    if (success && isValidMAVFix()) ParseData();

    if (isTimeToExport() && isValidMAVFix()) {
        MAVLinkShareTraffic();
        ExportTimeMarker = millis();
    }

    ClearExpired();
}
#endif /* EXCLUDE_MAVLINK */





#if !defined(EXCLUDE_TEST_MODE)

unsigned int pos_ndx = 0;
unsigned long TxPosUpdMarker = 0;

//================================================================================================

void txrx_test()
{
    bool success = false;

    ThisAircraft.timestamp = now();

        // Инициализация начальных координат
        if (settings->input_N_S == IMPUT_N)
        {
            ThisAircraft.local_latitude = (float)settings->local_latitude;
        }
        else
        {
            ThisAircraft.local_latitude = (float)-settings->local_latitude;
        }

        if (settings->input_E_W == IMPUT_E)
        {
            ThisAircraft.local_longitude = (float)settings->local_longitude;
        }
        else
        {
            ThisAircraft.local_longitude = (float)-settings->local_longitude;
        }

        //ThisAircraft.squawk = 1110;
        //memcpy(ThisAircraft.callsign, fly0, strlen(fly0));
        ThisAircraft.latitude = ThisAircraft.local_latitude;    // 
        ThisAircraft.longitude = ThisAircraft.local_longitude;   // 
        //startLatitude = ThisAircraft.local_latitude;
        //startLongitude = ThisAircraft.local_longitude;

        ThisAircraft.course = 1.0;
        ThisAircraft.speed = 0; // 50 м/с (180 км/ч)
       // set_test_coordinate = true;

    //if (TxPosUpdMarker == 0 || (millis() - TxPosUpdMarker) > 1100)
    //{

    //    fo.addr = 0x151DC8;
    //    fo.squawk = 1521;
    //    memcpy((char*)fo.callsign, fly1, strlen(fly1));
    //    fo.altitude = 0;
    //    fo.pressure_altitude = 0;
    //    fo.speed = 0;
    //    fo.vert_rate = 0;
    //    fo.signal_source = 1;
    //    fo.timestamp = now(); // 
    //    fo.course = 0;
    //    fo.aircraft_type = AIRCRAFT_TYPE_JET;
    //    fo.latitude = aircraft5[0].latitude;
    //    fo.longitude = aircraft5[0].longitude;
    //    /* Расчет расстояния, курса и уровня опастности сближения нашего и стороннего самолета*/
    //    if (fo.latitude != 0 && fo.longitude != 0) // Расчет возможен если получены координаты нашего и стороннего самолета
    //    {
    //        Traffic_Update(&fo);
    //    }

    //    /* Остальные параметры записываем в базу */
    //    Traffic_Add(&fo);
    //    TxPosUpdMarker = millis();
    //}

    //RF_Transmit(RF_Encode(&ThisAircraft), true);
    success = RF_Receive();
    if (success) ParseData();
    Traffic_loop();

    if (isTimeToDisplay())
    {
        LEDTimeMarker = millis();
    }

    if (isTimeToExport()) {
#if defined(USE_NMEALIB)
        NMEA_Position();
#endif
        NMEA_Export();
        ExportTimeMarker = millis();
    }
    // Handle Air Connect
    NMEA_loop();
    ClearExpired();
}

#endif /* EXCLUDE_TEST_MODE */
