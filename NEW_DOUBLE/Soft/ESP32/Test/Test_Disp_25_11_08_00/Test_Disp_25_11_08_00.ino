#include <Arduino.h>
#include <SPI.h>
#include <esp_task_wdt.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <TimeLib.h>
#include <HardwareSerial.h>
#include <Wire.h>
#include <Adafruit_INA219.h>
#include "Button.h"

// ================== RS485 / Протокол ==================
#define RS485_SERIAL   Serial1
#define RS485_TX_PIN   39
#define RS485_RX_PIN   38
#define RS485_DE_PIN   40
#define RS485_BAUD     115200
#define RS485_CONFIG   SERIAL_8N1

#define LED       4
#define LED_LCD  21

#define BTN1_PIN 45
#define BTN2_PIN 18

#define BUFFER_SIZE           310
#define MAX_TRACKING_OBJECTS   12
#define RX_WDT_ITER_LIM      4096

const uint32_t PACKET_HEADER = 0xAABBCCDD;
const uint32_t PACKET_FOOTER = 0xDDCCBBAA;

Adafruit_INA219 ina219;
bool ina219_ok = false;

volatile bool hasActiveLink = false;
volatile bool viewActiveLink = false;
volatile bool viewActiveLink_tmp = false;

// ----------------- ДАННЫЕ -----------------
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
    uint8_t  signal_source;
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

typedef struct __attribute__((packed)) {
    ufo_net_t ThisAircraft;
    ufo_net_t Container[MAX_TRACKING_OBJECTS];
    aux_t     AuxData;
    uint8_t   BUTTON1;
    uint8_t   BUTTON2;
} full_packet_net_t;

aux_t AuxData;
uint8_t BUTTON1 = 0, BUTTON2 = 0;

full_packet_net_t replyPacket; // только BUTTON1/BUTTON2 и/или AuxData.new_buttton_M по необходимости
full_packet_net_t lastPacket;
volatile bool packetUpdated = false;

// ----------------- RTOS -----------------
TaskHandle_t Task1, Task2, Task3, Task4;
SemaphoreHandle_t serialMutex;
SemaphoreHandle_t containerMutex;

// ----------------- Прототипы -----------------
uint16_t crc16_ccitt(const uint8_t* data, size_t len);
bool receivePacket_RS485(full_packet_net_t* pkt, uint8_t* btn1, uint8_t* btn2);
void sendImmediateNewButtonM(uint8_t value);
void rxTask(void* param);
void buttonMTask(void* param);
void linkWatchdogTask(void* param);
static void ButtonTask(void* pvParameters);

// ----------------- Утилиты копирования -----------------
void net_to_ufo_Container(const ufo_net_t* src, ufo_t* dst) {
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

void net_to_ufo_ThisAircraft(const ufo_net_t* src, ufo_t* dst) {
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

// ----------------- RS485 -----------------
static void rs485SetTX(bool enable) {
    digitalWrite(RS485_DE_PIN, enable ? HIGH : LOW);
    if (enable) delayMicroseconds(50);
}

void setupRS485() {
    RS485_SERIAL.setRxBufferSize(2048);
    RS485_SERIAL.setTxBufferSize(2048);
    RS485_SERIAL.begin(RS485_BAUD, RS485_CONFIG, RS485_RX_PIN, RS485_TX_PIN);
    pinMode(RS485_DE_PIN, OUTPUT);
    digitalWrite(RS485_DE_PIN, LOW);
    rs485SetTX(false);
}

// ----------------- CRC -----------------
uint16_t crc16_ccitt(const uint8_t* data, size_t len) {
    uint16_t crc = 0x0000;
    for (size_t i = 0; i < len; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; ++j)
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
    }
    return crc;
}

// ----------------- ПРИЁМ (порционный + быстрый ресинк) -----------------
bool receivePacket_RS485(full_packet_net_t* pkt, uint8_t* btn1, uint8_t* btn2) 
{
    static uint8_t buffer[sizeof(full_packet_net_t) + 64];
    static size_t idx = 0;

    const int READ_BUDGET = 256; // ограничение чтения за вызов
    int read_left = READ_BUDGET;

    xSemaphoreTake(serialMutex, portMAX_DELAY);
    while (RS485_SERIAL.available() && read_left > 0) 
    {
        int to_read = RS485_SERIAL.available();
        if (to_read > read_left) to_read = read_left;
        if ((size_t)to_read + idx > sizeof(buffer)) to_read = sizeof(buffer) - idx;
        if (to_read <= 0) break;
        int n = RS485_SERIAL.readBytes(&buffer[idx], to_read);
        idx += n;
        read_left -= n;
    }
    xSemaphoreGive(serialMutex);

    if (idx < 4) return false;

    // быстрый ресинк по заголовку 0xDD CC BB AA
    size_t start = 0;
    for (;;) 
    {
        if (idx - start < 4) 
        {
            if (start > 0) { memmove(buffer, buffer + start, idx - start); idx -= start; }
            return false;
        }
        if (buffer[start] == 0xDD && buffer[start + 1] == 0xCC &&
            buffer[start + 2] == 0xBB && buffer[start + 3] == 0xAA) 
        {
            break;
        }
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
        idx = 0; // мусор — очистка
        return false;
    }

    uint8_t* data = &buffer[4];
    uint16_t crc_rx = *(uint16_t*)&buffer[4 + sizeof(full_packet_net_t)];
    uint16_t crc_calc = crc16_ccitt(data, sizeof(full_packet_net_t));
    if (crc_rx != crc_calc) 
    {
        idx = 0; // битый кадр
        return false;
    }

    memcpy(pkt, data, sizeof(full_packet_net_t));
    if (btn1) *btn1 = pkt->BUTTON1;
    if (btn2) *btn2 = pkt->BUTTON2;

    idx -= frame_len;
    if (idx > 0) memmove(buffer, buffer + frame_len, idx);

    return true;
}

// ----------------- Мгновенная отправка new_buttton_M -----------------
void sendImmediateNewButtonM(uint8_t value) {
    full_packet_net_t reply = {};
    reply.AuxData.new_buttton_M = value;
    reply.BUTTON1 = BUTTON1;
    reply.BUTTON2 = BUTTON2;

    const size_t plen = sizeof(full_packet_net_t);
    static uint8_t buf[sizeof(full_packet_net_t)];
    memcpy(buf, &reply, plen);
    uint16_t crc = crc16_ccitt(buf, plen);

    xSemaphoreTake(serialMutex, portMAX_DELAY);
    rs485SetTX(true);
    RS485_SERIAL.write((uint8_t*)&PACKET_HEADER, sizeof(PACKET_HEADER));
    RS485_SERIAL.write(buf, plen);
    RS485_SERIAL.write((uint8_t*)&crc, sizeof(crc));
    RS485_SERIAL.write((uint8_t*)&PACKET_FOOTER, sizeof(PACKET_FOOTER));
    RS485_SERIAL.flush();
    delayMicroseconds(200);
    rs485SetTX(false);
    xSemaphoreGive(serialMutex);
}

// ----------------- Задачи -----------------
void rxTask(void* param) {
    static full_packet_net_t packet;
    uint8_t btn1, btn2;
    esp_task_wdt_add(NULL); // регистрируем только RX в WDT
    for (;;) {
        esp_task_wdt_reset();
        if (receivePacket_RS485(&packet, &btn1, &btn2)) {
            digitalWrite(LED, LOW);

            // Обновление локальных структур
            net_to_ufo_ThisAircraft(&packet.ThisAircraft, &ThisAircraft);
            for (int i = 0; i < MAX_TRACKING_OBJECTS; ++i) 
            {
                net_to_ufo_Container(&packet.Container[i], &Container[i]);
            }

            memcpy(&AuxData, &packet.AuxData, sizeof(aux_t));

            // Состояние связи
            hasActiveLink = true;
            viewActiveLink = true;

            memcpy(&lastPacket, &packet, sizeof(full_packet_net_t));
            packetUpdated = true;

            digitalWrite(LED, HIGH);
        }
        vTaskDelay(pdMS_TO_TICKS(15));
    }
}

void buttonMTask(void* param) {
    for (;;) {
        // Здесь вы обновляете AuxData.new_buttton_M как вам нужно (из кнопок/сервиса и т.п.)
        // В примере — из события в другой задаче (ButtonTask) меняется AuxData.new_buttton_M
        if (AuxData.new_buttton_M != 0) {
            uint8_t val = AuxData.new_buttton_M;
            sendImmediateNewButtonM(val);
            vTaskDelay(pdMS_TO_TICKS(10));
            AuxData.new_buttton_M = 0;
        }
        BUTTON1 = digitalRead(BTN1_PIN);
        BUTTON2 = digitalRead(BTN2_PIN);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void linkWatchdogTask(void* param) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    static uint32_t last = millis();
    viewActiveLink = true;
    for (;;) {
        if (hasActiveLink) {
            last = millis();
            hasActiveLink = false; // сброс — если за период не придёт следующий пакет, признаем линк потерян
            viewActiveLink = true;
        }
        if (millis() - last > 3000) {
            if (viewActiveLink) {
                Serial.println("Потеря связи с базой");
                viewActiveLink = false;
                digitalWrite(LED, LOW);
            }
        }
        if (viewActiveLink != viewActiveLink_tmp) {
            viewActiveLink_tmp = viewActiveLink;
            if (viewActiveLink) {
                Serial.println("Связь с базой установлена");
            }
        }
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(500));
    }
}

// ================= Кнопка: ISR + задача обработки =================
 
//1. Глобальные переменные / структура для событий
//(volatile, чтобы и ISR, и задача могли читать)

typedef struct {
    volatile bool event;        // был фронт/спад (по CHANGE, ISR ставит true)
    volatile uint32_t timestamp;// millis ISR
    volatile bool state;        // HIGH (отпущена) / LOW (нажата)
} ButtonEvent_t;

ButtonEvent_t btnEvt = { false, 0, true };

const int BUTTON_PIN = 45;

//2. ISR — только фиксирует факт события и время
IRAM_ATTR void onButtonChange() 
{
    btnEvt.event = true;
    btnEvt.timestamp = millis(); // на ESP32 millis в ISR годится (три линии времени); если глючит — используйте xTaskGetTickCountFromISR()
    btnEvt.state = digitalRead(BUTTON_PIN);
}

//3. Задача обработки(ButtonTask) — вся логика таймингов
void ButtonTask(void* pvParameters)
{
    const uint16_t DEBOUNCE_MS = 35;
    const uint16_t DOUBLE_WIN_MS = 350;
    const uint16_t LONG_PRESS_MS = 2000;

    bool lastStable = true;
    bool stable = true;
    uint32_t lastChangeMs = 0;
    uint32_t pressStartMs = 0;

    bool waitingSecond = false;
    uint32_t secondWinEndMs = 0;

    bool longReported = false;
    volatile bool event;
    volatile uint32_t ts;
    volatile bool state;

    const TickType_t tick = pdMS_TO_TICKS(5);

    for (;;)
    {
        // 1. Смотрим, был ли фронт/спад (флаг в ISR)
        event = btnEvt.event;
        ts = btnEvt.timestamp;
        state = btnEvt.state;

        if (event) 
        {
            // сбрасываем флаг (atomic, но на ESP32 это ok)
            btnEvt.event = false;
            // Обработка антидребезга (таймер)
            if (millis() - lastChangeMs >= DEBOUNCE_MS) 
            {
                lastChangeMs = millis();
                lastStable = stable;
                stable = (state == HIGH); // true - отпущена (INPUT_PULLUP)
                // Переход отпущена->нажата
                if (lastStable && !stable) 
                {
                    pressStartMs = millis();
                    longReported = false;
                }
                // Переход нажата->отпущена
                else if (!lastStable && stable)
                {
                    uint32_t dur = millis() - pressStartMs;
                    if (dur >= LONG_PRESS_MS) 
                    {
                        if (!longReported) AuxData.new_buttton_M = 3;
                        waitingSecond = false;
                    }
                    else 
                    {
                        if (waitingSecond) 
                        {
                            AuxData.new_buttton_M = 2;
                            Serial.println("DOUBLE");
                            waitingSecond = false;
                        }
                        else 
                        {
                            waitingSecond = true;
                            secondWinEndMs = millis() + DOUBLE_WIN_MS;
                        }
                    }
                }
            }
        }

        // Если кнопка удерживается — возможен LONG по времени
        if (!stable && !longReported && (millis() - pressStartMs >= LONG_PRESS_MS)) 
        {
            longReported = true;
            AuxData.new_buttton_M = 3;
            Serial.println("LONG");
            waitingSecond = false;
        }

        // Окно двойного клика, если прошло — это одиночный клик (SHORT)
        if (waitingSecond && (int32_t)(millis() - secondWinEndMs) >= 0) 
        {
            waitingSecond = false;
            AuxData.new_buttton_M = 1;
            Serial.println("SHORT");
        }

        vTaskDelay(tick);
    }
}


//==============================================================================
// ----------------- Вспомогательные -----------------
int voltageToPercent(float voltage) {
    const float minV = 4.2;
    const float maxV = 4.6;
    if (voltage <= minV) return 0;
    if (voltage >= maxV) return 100;
    return round((voltage - minV) * 100.0 / (maxV - minV));
}

void printThisThisAircraft(const ufo_t* ac) {
    Serial.printf("%06X:%d:%8s:%.0f:%.0f:%.0f:%.0f:%d:%.6f:%.6f\r",
        ac->addr, ac->squawk, ac->callsign, ac->altitude, ac->altitude, ac->speed,
        ac->course, ac->vert_rate, ac->latitude, ac->longitude);
}

void print_ThisContainer(const ufo_t* ac) {
    Serial.printf("%06X:%d:%8s:%.0f:%.0f:%.0f:%.0f:%d:%.6f:%.6f:%d:%d:%d:%d\r",
        ac->addr, ac->squawk, ac->callsign, ac->altitude, ac->altitude, ac->speed,
        ac->course, ac->vert_rate, ac->latitude, ac->longitude, ac->rssi,
        ac->last_message_signal_strength_dbm, ac->last_message_signal_quality_db,
        ac->signal_source);
}

void printContainer(const ufo_t* arr, int n) {
    for (int i = 0; i < n; ++i) {
        Serial.print("Container["); Serial.print(i); Serial.print("]:");
        print_ThisContainer(&arr[i]);
    }
}

void printAux(const aux_t* aux) {
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

// ----------------- Setup/Loop -----------------
unsigned long previousMillis = 0;
const long interval = 1000;

void setup() {
    pinMode(LED_LCD, OUTPUT);
    digitalWrite(LED_LCD, LOW);
    pinMode(LED, OUTPUT);
    digitalWrite(LED, HIGH);
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    Serial.begin(115200);
    delay(300);

    String ver_soft = __FILE__;
    int val_srt = ver_soft.lastIndexOf('\\');
    if (val_srt >= 0) ver_soft.remove(0, val_srt + 1);
    val_srt = ver_soft.lastIndexOf('.');
    if (val_srt >= 0) ver_soft.remove(val_srt);
    Serial.println(ver_soft);

    Wire.begin(); // или Wire.begin(SDA, SCL) при нестандартных пинах

    ina219.begin(); // у старых версий возвращает void

// Проба адреса INA219 (обычно 0x40)
    const uint8_t INA219_ADDR = 0x40;
    Wire.beginTransmission(INA219_ADDR);
    ina219_ok = (Wire.endTransmission() == 0);

    if (!ina219_ok) {
        Serial.println("INA219 init failed or not found at 0x40. Skip measurements.");
    }

    digitalWrite(LED_LCD, HIGH);

    ThisAircraft = EmptyFO;
    for (int i = 0; i < MAX_TRACKING_OBJECTS; i++) Container[i] = EmptyFO;

    setupRS485();

    Serial.print("Sizeof full_packet_net_t: "); Serial.println(sizeof(full_packet_net_t));

    serialMutex = xSemaphoreCreateMutex();
    containerMutex = xSemaphoreCreateMutex();

    pinMode(BUTTON_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), onButtonChange, CHANGE);

    // Разносим задачи по ядрам: RX/LINK — core 0, кнопочные — core 1
    xTaskCreatePinnedToCore(rxTask, "RX", 8192, NULL, 2, &Task1, 0);
    xTaskCreatePinnedToCore(linkWatchdogTask, "LINK", 2048, NULL, 1, &Task2, 0);
    xTaskCreatePinnedToCore(buttonMTask, "ButtonM", 4096, NULL, 2, &Task3, 1);
    xTaskCreatePinnedToCore(ButtonTask, "BtnTask", 4096, NULL, 1, &Task4, 1);

    // Инициализируем TWDT, регистрируем потом только нужные (rxTask и loop)
    esp_task_wdt_init(10, false);

    Serial.println("======== Setup END!========");
}

void loop() {
    static bool wdt_loop_registered = false;
    if (!wdt_loop_registered) { esp_task_wdt_add(NULL); wdt_loop_registered = true; }
    esp_task_wdt_reset();

    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
  /*      previousMillis = currentMillis;
        Serial.print("ThisAircraft:"); printThisThisAircraft(&ThisAircraft);
        Serial.println("--------------------------------------------");
        printContainer(Container, MAX_TRACKING_OBJECTS);
        printAux(&AuxData);
        Serial.println("============================================");*/

        if (ina219_ok) {
            float shunt_voltage_mV = ina219.getShuntVoltage_mV();
            float bus_voltage_V = ina219.getBusVoltage_V();
            float current_mA = ina219.getCurrent_mA();
            float load_voltage = bus_voltage_V + (shunt_voltage_mV / 1000.0);
            int voltage_percent = voltageToPercent(load_voltage);
            // ... ваша логика
        }

    }
    delay(50);
}





//#include <stdio.h>                // define I/O functions
//#include <Arduino.h>              // define I/O functions
//#include "SPI.h"
//#include <esp_task_wdt.h>
//#include <iostream>
//#include <locale.h>
//#include <math.h>
//#include <stdint.h>
//#include "freertos/FreeRTOS.h"
//#include "freertos/task.h"
//#include "freertos/semphr.h"
//#include <TimeLib.h>
//#include <HardwareSerial.h>
//#include <Wire.h>
//#include <Adafruit_INA219.h>
//
//#include "Button.h"
//
//Adafruit_INA219 ina219;
//
//// ================== RS485 / Протокол ==================
//#define RS485_SERIAL         Serial1
//#define RS485_TX_PIN         39
//#define RS485_RX_PIN         38
//#define RS485_DE_PIN         40
//
//#define RS485_BAUD      115200// 256000 //921600
//#define RS485_CONFIG      SERIAL_8N1
//
//#define LED 4
//#define LED_LCD 21
//
//#define BTN1_PIN 45
//#define BTN2_PIN 18
//
//#define BUFFER_SIZE  310
//#define MAX_TRACKING_OBJECTS 12
//
//const uint32_t PACKET_HEADER = 0xAABBCCDD;
//const uint32_t PACKET_FOOTER = 0xDDCCBBAA;
//
//#define RX_WDT_ITER_LIM 4096
//
//volatile bool hasActiveLink = false;
//volatile bool viewActiveLink = false;
//volatile bool viewActiveLink_tmp = false;
//
//
////============================================================================
//
//
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
//    float     speed;         /* скорость относительно земли в узлах */
//    uint8_t   aircraft_type;
//    char      callsign[8];    // Flight number
//    int       vert_rate;      // Vertical rate.
//    int       squawk;         // squawk
//    time_t    timemsg;        // Время передачи сообщения о координатах стороннего самолета
//
//    float     vs; /* feet per minute */
//
//    bool      stealth;
//    bool      no_track;
//
//    int8_t    ns[4];
//    int8_t    ew[4];
//
//    float     geoid_separation; /* metres */
//    uint16_t  hdop; /* cm */
//    int8_t    rssi; /* SX1276 only */
//
//    /* 'legacy' specific data */
//    float     distance;
//    float     bearing;
//    int8_t    alarm_level;
//
//    uint8_t   signal_source;
//    time_t    seen;           // Time at which the last packet was received
//    uint8_t   hour_msg;
//    uint8_t   min_msg;
//    uint16_t  delay_time_msg;
//
//    float     test_latitude;
//    float     test_longitude;
//    uint16_t  last_message_signal_strength_dbm;        // SIGS
//    uint16_t  last_message_signal_quality_db;          // SIGQ
//} ufo_t;
//
//
//ufo_t ThisAircraft;
//ufo_t fo, Container[MAX_TRACKING_OBJECTS], EmptyFO, fo_msg, Container_msg[MAX_TRACKING_OBJECTS];
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
//   // bool     new_flag_M;             // Пока свободен
//    uint8_t  new_buttton_M;            // Состояние кнопки (0,1,2,3)
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
//aux_t AuxData;
//
//uint8_t BUTTON1 = 0, BUTTON2 = 0;
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
//full_packet_net_t replyPacket; // только BUTTON1,BUTTON2 используются
//full_packet_net_t lastPacket;
//volatile bool packetUpdated = false;
//
//
////============================================================================
//
//uint16_t crc16_ccitt(const uint8_t* data, size_t len);
//bool receivePacket_RS485(full_packet_net_t* pkt, uint8_t* btn1, uint8_t* btn2);
//void net_to_ufo_Container(const ufo_net_t* src, ufo_t* dst);
//void net_to_ufo_ThisAircraft(const ufo_net_t* src, ufo_t* dst);
//
//void rxTask(void* param);
//void buttonMTask(void* param);
//void linkWatchdogTask(void* param);
//static void ButtonTask(void* pvParameters);
//
//
//TaskHandle_t Task1;
//TaskHandle_t Task2;
//TaskHandle_t Task3;
//TaskHandle_t Task4;
//
//
//SemaphoreHandle_t serialPrintMutex; 
//SemaphoreHandle_t serialMutex;
//SemaphoreHandle_t containerMutex;
//SemaphoreHandle_t sendMutex;
//
//
//uint16_t crc16_ccitt(const uint8_t* data, size_t len)
//{
//    uint16_t crc = 0x0000;
//    for (size_t i = 0; i < len; ++i) 
//    {
//        crc ^= (uint16_t)data[i] << 8;
//        for (uint8_t j = 0; j < 8; ++j)
//            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
//    }
//    return crc;
//}
//
//
//
////=============================================================================
//// копирование полей чужого самолета
//void net_to_ufo_Container(const ufo_net_t* src, ufo_t* dst)
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
//void net_to_ufo_ThisAircraft(const ufo_net_t* src, ufo_t* dst)
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
////========================================================================================
//
//static void rs485SetTX(bool enable)
//{
//    digitalWrite(RS485_DE_PIN, enable ? HIGH : LOW);
//    if (enable) delayMicroseconds(50);
//}
//
//void setupRS485()
//{
//    RS485_SERIAL.setRxBufferSize(2048);
//    RS485_SERIAL.setTxBufferSize(2048);
//    RS485_SERIAL.begin(RS485_BAUD, RS485_CONFIG, RS485_RX_PIN, RS485_TX_PIN);
//    pinMode(RS485_DE_PIN, OUTPUT);
//    digitalWrite(RS485_DE_PIN, LOW);
//    rs485SetTX(false);
//}
//
//bool receivePacket_RS485(full_packet_net_t* pkt, uint8_t* btn1, uint8_t* btn2) 
//{
//    static uint8_t buffer[sizeof(full_packet_net_t) + 64];
//    static size_t idx = 0;
//
//    // Прочесть всё, что есть, в буфер
//    xSemaphoreTake(serialMutex, portMAX_DELAY);
//    while (RS485_SERIAL.available()) 
//    {
//        int bytes = RS485_SERIAL.available();
//        if (bytes + idx > sizeof(buffer)) bytes = sizeof(buffer) - idx;
//        int n = RS485_SERIAL.readBytes(&buffer[idx], bytes);
//        idx += n;
//    }
//    xSemaphoreGive(serialMutex);
//
//    int safe_counter = RX_WDT_ITER_LIM;
//    while (idx >= sizeof(full_packet_net_t) + 8 && safe_counter-- > 0) 
//    {
//        // Сброс по watchdog
//        if (safe_counter % 128 == 0) esp_task_wdt_reset();
//
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
//                if (btn1) *btn1 = pkt->BUTTON1;
//                if (btn2) *btn2 = pkt->BUTTON2;
//                size_t msgLen = sizeof(full_packet_net_t) + 8;
//                idx -= msgLen;
//                if (idx)
//                    memmove(buffer, buffer + msgLen, idx);
//                else
//                    idx = 0;
//                return true;
//            }
//        }
//        // Быстрый сброс мусора: если safe_counter на исходе или нет header долго — чистим!
//        if (safe_counter < RX_WDT_ITER_LIM / 2) 
//        {
//            Serial.println("RX: long loop, buffer dropped!");
//            idx = 0;
//            break;
//        }
//        memmove(buffer, buffer + 1, --idx);
//    }
//    if (safe_counter <= 0)
//    {
//        Serial.println("RX: while overload, buffer cleared!");
//        idx = 0;
//    }
//    return false;
//}
//
//
//void rxTask(void* param) 
//{
//    static full_packet_net_t packet;
//    uint8_t btn1, btn2;
//    esp_task_wdt_add(NULL);
//    for (;;) 
//    {
//        esp_task_wdt_reset();
//        if (receivePacket_RS485(&packet, &btn1, &btn2))
//        {
//            //Serial.println("Packet accepted!");
//            digitalWrite(LED, LOW);
//            net_to_ufo_ThisAircraft(&packet.ThisAircraft, &ThisAircraft);
//            for (int i = 0; i < MAX_TRACKING_OBJECTS; ++i)
//            {
//                net_to_ufo_Container(&packet.Container[i], &Container[i]);
//            }
//
//            //AuxData.message_received = service.getNewMessageFlag();
//            memcpy(&AuxData, &packet.AuxData, sizeof(aux_t));
//            // Формируем ответ
//            replyPacket.BUTTON1 = BUTTON1;
//            replyPacket.BUTTON2 = BUTTON2;
//            // Serial.print("Button1 = "); Serial.println(btn1);
//            hasActiveLink = true;
//            viewActiveLink = true;
//
//            // Обновляем последний принятый пакет
//            memcpy(&lastPacket, &packet, sizeof(full_packet_net_t));
//            packetUpdated = true;
//            digitalWrite(LED, HIGH); // 
//        }
//        vTaskDelay(pdMS_TO_TICKS(15));
//    }
//}
//
//
//void sendImmediateNewButtonM(uint8_t value)
//{
//    full_packet_net_t replyPacket = {};
//    replyPacket.AuxData.new_buttton_M = value;
//    replyPacket.BUTTON1 = BUTTON1;
//    replyPacket.BUTTON2 = BUTTON2;
//
//    const size_t plen = sizeof(full_packet_net_t);
//    static uint8_t buf[sizeof(full_packet_net_t)];
//    memcpy(buf, &replyPacket, plen);
//    uint16_t crc = crc16_ccitt(buf, plen);
//
//    xSemaphoreTake(serialMutex, portMAX_DELAY);
//    rs485SetTX(true);
//    RS485_SERIAL.write((uint8_t*)&PACKET_HEADER, sizeof(PACKET_HEADER));
//    RS485_SERIAL.write(buf, plen);
//    RS485_SERIAL.write((uint8_t*)&crc, sizeof(crc));
//    RS485_SERIAL.write((uint8_t*)&PACKET_FOOTER, sizeof(PACKET_FOOTER));
//    RS485_SERIAL.flush();
//    delayMicroseconds(200);
//    rs485SetTX(false);
//    xSemaphoreGive(serialMutex);
//}
//
//
//// быстрая задача: немедленная отправка new_buttton_M от AuxData
//void buttonMTask(void* param) 
//{
//    esp_task_wdt_add(NULL);
//    for (;;)
//    {
//        esp_task_wdt_reset();
//        //AuxData.new_buttton_M = service.get_num_buttton();
//        if (AuxData.new_buttton_M != 0)
//        {
//            uint8_t val = AuxData.new_buttton_M;
//            sendImmediateNewButtonM(val);
//            vTaskDelay(pdMS_TO_TICKS(10));
//            AuxData.new_buttton_M = 0;
//        }
//
//        BUTTON1 = digitalRead(BTN1_PIN); 
//        BUTTON2 = digitalRead(BTN2_PIN);
//        vTaskDelay(pdMS_TO_TICKS(20));
//    }
//}
//
//
//void linkWatchdogTask(void* param)
//{
//    esp_task_wdt_add(NULL);
//    TickType_t xLastWakeTime = xTaskGetTickCount();
//    static uint32_t last = millis();
//    viewActiveLink = true;
//    for (;;)
//    {
//        esp_task_wdt_reset();
//        if (hasActiveLink)
//        {
//            last = millis();
//            hasActiveLink = false; // будет сброшен,если не поступит новый пакет
//            viewActiveLink = true;
//        }
//        if (millis() - last > 3000)
//        {
//            if (viewActiveLink)
//            {
//                Serial.println("Потеря связи с базой");
//                viewActiveLink = false;
//                digitalWrite(LED, LOW);
//
//                // Потеря связи
//                // Здесь можно включать аварийные индикаторы!
//            }
//        }
//        if (viewActiveLink != viewActiveLink_tmp)
//        {
//            viewActiveLink_tmp = viewActiveLink;
//            //service.set_connection_base(viewActiveLink);
//            if (viewActiveLink)
//            {
//                Serial.println("Связь с базой установлена");
//            }
//        }
// 
//        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(500));
//    }
//}
//
//
////============================================================================
//
//// ESP32S3 - обработка 3 видов нажатий через прерывание(CHANGE),вывод в Serial в задаче на ядре 0
//
//const int BUTTON_PIN = 45;
//const unsigned long DEBOUNCE_MS = 15;
//const unsigned long DOUBLE_CLICK_MS = 300;
//const unsigned long LONG_PRESS_MS = 2000;
//
//// ISR-состояния
//volatile bool buttonPressed = false;
//volatile unsigned long pressStartTime = 0;
//volatile unsigned long lastInterruptTime = 0;
//
//volatile bool waitingForSecondClick = false;
//volatile int shortClickCount = 0;
//volatile unsigned long windowEndTime = 0;
//
//volatile bool evtShort = false;
//volatile bool evtDouble = false;
//volatile bool evtLong = false;
//
//// Новый флаг и время для LONG (для текущего нажатия)
//volatile bool longDetected = false;
//
//IRAM_ATTR void onButtonChange()
//{
//    unsigned long t = millis();
//    int state = digitalRead(BUTTON_PIN);
//
//    // дребезг
//    if (t - lastInterruptTime < DEBOUNCE_MS) return;
//    lastInterruptTime = t;
//
//    if (state == LOW) {
//        // кнопка нажата
//        if (!buttonPressed) 
//        {
//            buttonPressed = true;
//            pressStartTime = t;
//            longDetected = false; // новый нажим,LONG ещё не был
//        }
//    }
//    else {
//        // кнопка отпущена
//        if (buttonPressed) 
//        {
//            unsigned long duration = t - pressStartTime;
//            buttonPressed = false;
//
//            // LONG обрабатывается в задаче,здесь только SHORT/DOUBLE
//            if (duration >= LONG_PRESS_MS) 
//            {
//                // НЕ обрабатываем здесь LONG,чтобы не дублировать
//                // (LONG будет сгенерирован в задаче)
//            }
//            else 
//            {
//                // короткое нажатие
//                if (waitingForSecondClick && shortClickCount == 1) 
//                {
//                    evtDouble = true;
//                    waitingForSecondClick = false;
//                    shortClickCount = 0;
//                }
//                else 
//                {
//                    shortClickCount = 1;
//                    waitingForSecondClick = true;
//                    windowEndTime = t + DOUBLE_CLICK_MS;
//                }
//            }
//        }
//    }
//
//    // обработка окна двойного клика внутри ISR остается только таймер окна
//    if (waitingForSecondClick && t > windowEndTime)
//    {
//        evtShort = true;
//        waitingForSecondClick = false;
//        shortClickCount = 0;
//    }
//}
//
//// Задача на ядре 0
//static void ButtonTask(void* pvParameters)
//{
//    esp_task_wdt_add(NULL);
//    pinMode(BUTTON_PIN, INPUT_PULLUP);
//    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), onButtonChange, CHANGE);
//
//    for (;;) 
//    {
//        esp_task_wdt_reset();
//        // LONG обработка:запускаем примерно через LONG_PRESS_MS после начала нажатия
//        if (buttonPressed && !longDetected) 
//        {
//            if (millis() - pressStartTime >= LONG_PRESS_MS) 
//            {
//                longDetected = true;
//                evtLong = true;
//                // длинное нажатие отменяет двойной клик
//                waitingForSecondClick = false;
//                shortClickCount = 0;
//            }
//        }
//
//        // Обработка событий
//        if (evtLong) 
//        {
//            evtLong = false;
//            Serial.println("LONG");
//            //service.set_num_buttton(3);
//            AuxData.new_buttton_M = 3;
//        }
//
//        if (evtDouble)
//        {
//            evtDouble = false;
//            Serial.println("DOUBLE_SHORT");
//            //service.set_num_buttton(2);
//            AuxData.new_buttton_M = 2;
//        }
//
//        if (evtShort) 
//        {
//            evtShort = false;
//            Serial.println("SHORT");
//            //service.set_num_buttton(1);
//            AuxData.new_buttton_M = 1;
//        }
//
//        // Тайм-аут окна двойного клика:одиночное нажатие зафиксировано
//        if (waitingForSecondClick && (millis() >= windowEndTime)) 
//        {
//            evtShort = true;
//            waitingForSecondClick = false;
//            shortClickCount = 0;
//        }
//
//        vTaskDelay(pdMS_TO_TICKS(5));
//    }
//}
//
//
////===============================================================================
//bool isValidGNSS_M_tmp = false;
//bool new_SOS_flag_M_tmp = false;
//uint8_t hour_tmp = -1;
//uint8_t minute_tmp = -1;
//
//
//void set_packet()
//{
//    if (AuxData.Time_Hour_M != hour_tmp)
//    {
//        hour_tmp = AuxData.Time_Hour_M; 
//        //service.set_time_hour(AuxData.Time_Hour_M);
//    }
//
//    if (AuxData.Time_Minute_M != minute_tmp)
//    {
//        minute_tmp = AuxData.Time_Minute_M;
//        //service.set_time_minute(AuxData.Time_Minute_M);
//       // Serial.printf("%d:%d\r\n", AuxFlags.Time_Hour_M, AuxFlags.Time_Minute_M);
//    }
//
//    if (AuxData.new_SOS_flag_M != new_SOS_flag_M_tmp)
//    {
//        new_SOS_flag_M_tmp = AuxData.new_SOS_flag_M;
//        //service.set_SOS_on_off((bool)AuxData.new_SOS_flag_M);
//    }
//
//    if (AuxData.isValidGNSS_M != isValidGNSS_M_tmp)
//    {
//        isValidGNSS_M_tmp = AuxData.isValidGNSS_M;
//        //service.set_GNSS_on_off((bool)AuxData.isValidGNSS_M);
//       // Serial.printf("AuxFlags.isValidGNSS_M %d \r\n", AuxFlags.isValidGNSS_M);
//    }
//
//    if (AuxData.new_message)
//    {
//        if (strlen(AuxData.msg_resp_M) > 0)
//        {
//            //service.setNewMessageFlag(true);    // Отправить дисплею флаг получения нового сообщения
//            //service.setMessageRead(true);       // Установить признак о том что новое сообщение получено в выносном дисплее
//            //strncpy(service.msg_tmp_all, AuxData.msg_resp_M, strlen(AuxData.msg_resp_M));
//            memset(AuxData.msg_resp_M, 0, sizeof(AuxData.msg_resp_M));
//        }
//    }
//}
////================================= Управление включения устройства =============================
//
//const uint8_t PIN_BTN = 7;
//const uint8_t PIN_POWER = 17;
//const uint8_t PIN_PULSE = 20;
//
//const unsigned long HOLD_TIME_MS = 3000;   // 3 сек для кнопки
//const unsigned long PULSE_ON_MS = 5000;   // 5 сек (включение)
//const unsigned long PULSE_OFF_MS = 6000;   // 6 сек (выключение)
//
//bool deviceOn = false;
//bool btnPrevState = HIGH;
//unsigned long btnPressStart = 0;
//bool holdProcessed = false;
//
//enum State {
//    Idle,
//    WaitOffPulse
//};
//State state = Idle;
//
//// Для импульса
//bool pulseActive = false;
//unsigned long pulseStart = 0;
//unsigned long currentPulseDuration = 0;
//
//// ==== функции импульса ====
//void startPulse(unsigned long duration) 
//{
//    digitalWrite(PIN_PULSE, HIGH);
//    pulseActive = true;
//    pulseStart = millis();
//    currentPulseDuration = duration;
//}
//
//bool updatePulse(unsigned long now) 
//{
//    if (pulseActive && (now - pulseStart >= currentPulseDuration)) 
//    {
//        digitalWrite(PIN_PULSE, LOW);
//        pulseActive = false;
//        return true;
//    }
//    return false;
//}
//
//
//void power_Off() 
//{
//    unsigned long now = millis();
//    bool btnState = digitalRead(PIN_BTN);
//
//    switch (state) 
//    {
//    case Idle:
//        // Реакция на новое нажатие кнопки
//        if (btnPrevState == HIGH && btnState == LOW) 
//        {
//            btnPressStart = now;
//            holdProcessed = false;
//        }
//        // Кнопка удерживается
//        if (btnState == LOW) 
//        {
//            if (!holdProcessed && deviceOn && (now - btnPressStart >= HOLD_TIME_MS)) 
//            {
//                holdProcessed = true;
//               // SoC->View_powerOff();
//                // Импульс 6 сек, потом выключение без задержек
//                startPulse(PULSE_OFF_MS);
//                state = WaitOffPulse;
//            }
//        }
//        break;
//
//    case WaitOffPulse:
//        if (updatePulse(now)) 
//        {
//            digitalWrite(PIN_POWER, LOW);
//            deviceOn = false;
//            state = Idle;
//        }
//        break;
//    }
//
//    btnPrevState = btnState;
//}
//
//// Функция преобразования напряжения к процентам (от 4,2В до 4,6В)
//int voltageToPercent(float voltage) 
//{
//    const float minV = 4.2;
//    const float maxV = 4.6;
//    if (voltage <= minV) return 0;
//    if (voltage >= maxV) return 100;
//    return round((voltage - minV) * 100.0 / (maxV - minV));
//}
//
//
////==================================================================================================================
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
//     );
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
////===============================================================================================
//
//
//unsigned long previousMillis = 0;            // will store last time LED was updated
//const long interval = 1000;                  // interval at which to blink (milliseconds)
//
//void setup()
//{
//    pinMode(LED_LCD, OUTPUT);
//    digitalWrite(LED_LCD, LOW);
// 
//    pinMode(PIN_BTN, INPUT_PULLUP);
//    pinMode(PIN_POWER, OUTPUT);
//    pinMode(PIN_PULSE, OUTPUT);
//
//    digitalWrite(PIN_POWER, LOW);
//    digitalWrite(PIN_PULSE, LOW);
//
//    pinMode(LED, OUTPUT);
//    digitalWrite(LED, HIGH);
// 
//    Serial.begin(115200);
//    delay(500);
// 
//    String ver_soft = __FILE__;
//    int val_srt = ver_soft.lastIndexOf('\\');
//    ver_soft.remove(0, val_srt + 1);
//    val_srt = ver_soft.lastIndexOf('.');
//    ver_soft.remove(val_srt);
//    Serial.println(ver_soft);
//    //service.saveVer(ver_soft);  // Сохранить строку с текущей версией.
//
//    Serial.flush();
//
//    digitalWrite(LED_LCD, HIGH);
//
//
//    // Проверим, зажата ли кнопка при старте (3 с)
//    if (digitalRead(PIN_BTN) == LOW)
//    {
//        unsigned long tStart = millis();
//        while (digitalRead(PIN_BTN) == LOW)
//        {
//            if (millis() - tStart >= HOLD_TIME_MS)
//            {
//                // Кнопка удерживалась ≥3 сек при старте — ВКЛ!
//                digitalWrite(PIN_POWER, HIGH);
//                deviceOn = true;
//                // Импульс 5 сек
//                digitalWrite(PIN_PULSE, HIGH);
//                delay(PULSE_ON_MS);
//                digitalWrite(PIN_PULSE, LOW);
//                break;
//            }
//            // Маленькая пауза для экономии CPU
//            delay(10);
//        }
//    }
// 
//    ina219.begin();
//
//    ThisAircraft = EmptyFO;
//
//    for (int i = 0; i < MAX_TRACKING_OBJECTS; i++)
//    {
//        Container[i] = EmptyFO;
//    }
//
//
//    setupRS485();
//
//    Serial.print("Sizeof full_packet_net_t:"); Serial.println(sizeof(full_packet_net_t));
//
//
//  //============================================================================
//    serialPrintMutex = xSemaphoreCreateMutex();
//    serialMutex = xSemaphoreCreateMutex();
//    containerMutex = xSemaphoreCreateMutex();
//    sendMutex = xSemaphoreCreateMutex();
//
//    // Запуск задач на Core 0
//
//    xTaskCreatePinnedToCore(rxTask, "RX", 8192, NULL, 3, &Task1, 0);              // Прием пакетов с базы
//    xTaskCreatePinnedToCore(linkWatchdogTask, "LINK", 1024, NULL, 1, &Task2, 0);  // Контроль соединения с базой
//    xTaskCreatePinnedToCore(buttonMTask, "ButtonM", 8192, NULL, 1, &Task3, 0);    // быстрая задача: немедленная отправка new_buttton_M от AuxData
//    xTaskCreatePinnedToCore(ButtonTask, "BtnTask", 4096, NULL, 2, &Task4, 0);     // Обработка кнопок
//
//  //============================================================================
//   esp_task_wdt_init(10, false); // таймаут 10 сек,reset chip=true
//
//   Serial.println("======== Setup END!========");
//}
//
//
//
//void loop()
//{
//    // Регистрация TWDT для loopTask один раз
//    static bool wdt_loop_registered = false;
//    if (!wdt_loop_registered)
//    {
//        esp_task_wdt_add(NULL); // текущая задача - loopTask
//        wdt_loop_registered = true;
//    }
//
//    power_Off();
//
//    set_packet(); // Получить и записать информацию с базового модуля.
//
//     unsigned long currentMillis = millis();
//
//    if (currentMillis - previousMillis >= interval)
//    {
//        previousMillis = currentMillis;
//
//            Serial.print("ThisAircraft:");
//            printThisThisAircraft(&ThisAircraft);
//            Serial.println("--------------------------------------------------------------------");
//            printContainer(Container, MAX_TRACKING_OBJECTS);
//            printAux(&AuxData);
//            Serial.println("====================================================================");
//            Serial.println();
// 
//
//        float shunt_voltage_mV = ina219.getShuntVoltage_mV(); // Напряжение на шунте (милливольты)
//        float bus_voltage_V = ina219.getBusVoltage_V();       // Напряжение на шине (вольты)
//        float current_mA = ina219.getCurrent_mA();            // Ток (миллиамперы)
//        float load_voltage = bus_voltage_V + (shunt_voltage_mV / 1000); // Точное напряжение на нагрузке
//        int voltage_percent = voltageToPercent(load_voltage);
//
//        //service.set_voltage_value(load_voltage);
//        //service.set_current_value(current_mA);
//    }
//
//    yield();
//}
//
////===================================================================================
