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


#include "OTA.h"
#include "TimeRF.h"
#include "EEPROMRF.h"
#include "SoC.h"
#include "WiFiRF.h"
#include "WebRF.h"
#include "ESP32RF.h"
#include <TimeLib.h>
#include "TrafficHelper.h"
#include "ServiceMain.h"
#include "Configuration_ESP32.h"
#include "Button.h"
#include <HardwareSerial.h>
#include "SoftRF.h"
#include <Wire.h>
#include <Adafruit_INA219.h>

Adafruit_INA219 ina219;

// ================== RS485 / Протокол ==================
#define RS485_SERIAL         Serial1
#define RS485_TX_PIN         39
#define RS485_RX_PIN         38
#define RS485_DE_PIN         40

#define RS485_BAUD       256000 //921600
#define RS485_CONFIG      SERIAL_8N1

#define LED 4

#define BTN1_PIN 45
#define BTN2_PIN 18


//============================================================================


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


// Доп служебная структура
typedef struct __attribute__((packed)) {
   // bool     new_flag_M;         // Пока свободен
    uint8_t  new_buttton_M;      // Состояние кнопки
    bool     new_message;        // Флаг прихода нового сообщения
    bool     message_received;   // Сообщение дисплеем получено 
    bool     confirm_message_M;  // Подтвердить прочтение сообщения
    char     msg_resp_M[170];    // Строка сообщения
    uint8_t  Time_Hour_M;        // Время час
    uint8_t  Time_Minute_M;      // Время минуты
    bool     new_SOS_flag_M;     // Флаг SOS
    bool     isValidGNSS_M;      // Флаг получения координат
} aux_t;

aux_t AuxData;

uint8_t BUTTON1 = 0, BUTTON2 = 0;

typedef struct __attribute__((packed)) {
    ufo_net_t ThisAircraft; 
    ufo_net_t Container[MAX_TRACKING_OBJECTS];
    aux_t AuxData;
    uint8_t BUTTON1;
    uint8_t BUTTON2;
} full_packet_net_t;


hardware_info_t hw_info = {
  .model    = DEFAULT_FLYRF_MODEL,
  .revision = 0,
  .soc      = SOC_NONE,
  .display  = DISPLAY_NONE,
};


//============================================================================

uint16_t crc16_ccitt(const uint8_t* data, size_t len);
bool receivePacket_RS485(full_packet_net_t* pkt, uint8_t* btn1, uint8_t* btn2);
void net_to_ufo_Container(const ufo_net_t* src, ufo_t* dst);
void net_to_ufo_ThisAircraft(const ufo_net_t* src, ufo_t* dst);

void rxTask(void* param);
void buttonsTask(void* param);
void replyTask(void* param);
void linkWatchdogTask(void* param);

TaskHandle_t Task1;
TaskHandle_t Task2;
TaskHandle_t Task3;
TaskHandle_t Task4;
TaskHandle_t Task5;
TaskHandle_t Task6;


SemaphoreHandle_t serialPrintMutex; 
SemaphoreHandle_t serialMutex;
SemaphoreHandle_t containerMutex;
SemaphoreHandle_t sendMutex;

ufo_t ThisAircraft;
ufo_t fo, Container[MAX_TRACKING_OBJECTS], EmptyFO, fo_msg, Container_msg[MAX_TRACKING_OBJECTS];


const uint32_t PACKET_HEADER = 0xAABBCCDD;
const uint32_t PACKET_FOOTER = 0xDDCCBBAA;

volatile bool hasActiveLink = false;
volatile bool viewActiveLink = false;
volatile bool needSendReply = false;

full_packet_net_t replyPacket; // только BUTTON1,BUTTON2 используются
full_packet_net_t lastPacket;
volatile bool packetUpdated = false;

uint16_t crc16_ccitt(const uint8_t* data, size_t len)
{
    uint16_t crc = 0x0000;
    for (size_t i = 0; i < len; ++i) 
    {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; ++j)
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
    }
    return crc;
}

bool receivePacket_RS485(full_packet_net_t* pkt, uint8_t* btn1, uint8_t* btn2)
{
    static uint8_t buffer[sizeof(full_packet_net_t) + 32];
    static size_t idx = 0;

    // Прочитать все байты за раз (без длинных while)
    int bytes = RS485_SERIAL.available();
    if (bytes) 
    {
        if (bytes + idx > sizeof(buffer)) bytes = sizeof(buffer) - idx;
        int n = RS485_SERIAL.readBytes(&buffer[idx], bytes);
        idx += n;
    }

 
    const size_t plen = sizeof(full_packet_net_t);

    // Разбор только одного пакета за вызов (никаких while!)
    if (idx >= plen + 10)
    {
 
        size_t footer_off = 4 + plen + 2;
        bool header_ok = (buffer[0] == 0xDD && buffer[1] == 0xCC && buffer[2] == 0xBB && buffer[3] == 0xAA);
        bool footer_ok = (buffer[footer_off] == 0xAA && buffer[footer_off + 1] == 0xBB &&
            buffer[footer_off + 2] == 0xCC && buffer[footer_off + 3] == 0xDD);

        if (header_ok && footer_ok)
        {
            uint8_t* data = &buffer[4];
            uint16_t crc_rx = *(uint16_t*)&buffer[4 + plen];
            uint16_t crc_calc = crc16_ccitt(data, plen);
            if (crc_rx == crc_calc)
            {
                memcpy(pkt, data, plen);
                if (btn1) *btn1 = pkt->BUTTON1;
                if (btn2) *btn2 = pkt->BUTTON2;

                // Сдвиг/очистка принятых байт
                size_t msgLen = plen + 4 + 2 + 4;
                idx -= msgLen;
                if (idx) memmove(buffer, buffer + msgLen, idx);
                else idx = 0;
                return true;
            }
        }
        // Если невалидный пакет — двигаем окно только на 1 байт!
        memmove(buffer, buffer + 1, --idx);
    }
    return false;
}

void sendImmediateNewButtonM(uint8_t value)
{
    full_packet_net_t replyPacket = {};
    replyPacket.AuxData.new_buttton_M = value;
    replyPacket.BUTTON1 = BUTTON1;
    replyPacket.BUTTON2 = BUTTON2;

    const size_t plen = sizeof(full_packet_net_t);
    static uint8_t buf[sizeof(full_packet_net_t)];
    memcpy(buf, &replyPacket, plen);
    uint16_t crc = crc16_ccitt(buf, plen);

    xSemaphoreTake(serialMutex, portMAX_DELAY);
    rs485SetTX(true);
    //digitalWrite(RS485_DE_PIN, HIGH);
    //delay(2);
    RS485_SERIAL.write((uint8_t*)&PACKET_HEADER, sizeof(PACKET_HEADER));
    RS485_SERIAL.write(buf, plen);
    RS485_SERIAL.write((uint8_t*)&crc, sizeof(crc));
    RS485_SERIAL.write((uint8_t*)&PACKET_FOOTER, sizeof(PACKET_FOOTER));
    RS485_SERIAL.flush();
    delayMicroseconds(200);
    rs485SetTX(false);
    //delay(2);
    //digitalWrite(RS485_DE_PIN, LOW);
    xSemaphoreGive(serialMutex);

   // Serial.print("SRV: send new_buttton_M = "); Serial.println(value);
}




//=============================================================================
// копирование полей чужого самолета
void net_to_ufo_Container(const ufo_net_t* src, ufo_t* dst)
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

// копирование полей нашего самолета
void net_to_ufo_ThisAircraft(const ufo_net_t* src, ufo_t* dst)
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


void rxTask(void* param)
{
    // регистрация TWDT
    esp_task_wdt_add(NULL);

    full_packet_net_t packet;
    uint8_t btn1 = 0, btn2 = 0;
    for (;;)
    {
        esp_task_wdt_reset();
  
        if (receivePacket_RS485(&packet, &btn1, &btn2))
        {
            digitalWrite(LED, LOW);
            net_to_ufo_ThisAircraft(&packet.ThisAircraft, &ThisAircraft);
            for (int i = 0; i < MAX_TRACKING_OBJECTS; ++i)
            {
                net_to_ufo_Container(&packet.Container[i], &Container[i]);
                Traffic_Add(&Container[i]);
            }

            AuxData.message_received = service.getNewMessageFlag();
            memcpy(&AuxData, &packet.AuxData, sizeof(aux_t));
            // Формируем ответ
            replyPacket.BUTTON1 = BUTTON1;
            replyPacket.BUTTON2 = BUTTON2;
           // Serial.print("Button1 = "); Serial.println(btn1);
            needSendReply = true;
            hasActiveLink = true;
            viewActiveLink = true;

            // Обновляем последний принятый пакет
            memcpy(&lastPacket, &packet, sizeof(full_packet_net_t));
            packetUpdated = true;
            digitalWrite(LED, HIGH); // 
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}


void buttonsTask(void* param)
{
 
    esp_task_wdt_add(NULL);

    for (;;)
    {
        esp_task_wdt_reset();
        BUTTON1 = digitalRead(BTN1_PIN); //service.get_num_buttton();
        BUTTON2 = digitalRead(BTN2_PIN);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// быстрая задача: немедленная отправка new_buttton_M от AuxData
void buttonMTask(void* param) 
{
    esp_task_wdt_add(NULL);
    for (;;)
    {
        esp_task_wdt_reset();
        if (AuxData.new_buttton_M != 0)
        {
            uint8_t val = AuxData.new_buttton_M;
            sendImmediateNewButtonM(val);
            AuxData.new_buttton_M = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}



void sendPacket_RS485_reply(const full_packet_net_t* pkt)
{
    esp_task_wdt_add(NULL);
    const size_t plen = sizeof(full_packet_net_t);
    static uint8_t buf[sizeof(full_packet_net_t)];
    memcpy(buf, pkt, plen);
    uint16_t crc = crc16_ccitt(buf, plen);

    if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(50)) == pdTRUE)
    {
        rs485SetTX(true);
      //  digitalWrite(LED, LOW);
        RS485_SERIAL.write((uint8_t*)&PACKET_HEADER, sizeof(PACKET_HEADER));
        RS485_SERIAL.write(buf, plen);
        RS485_SERIAL.write((uint8_t*)&crc, sizeof(crc));
        RS485_SERIAL.write((uint8_t*)&PACKET_FOOTER, sizeof(PACKET_FOOTER));
        RS485_SERIAL.flush();
        delayMicroseconds(200);
        rs485SetTX(false);
      //  digitalWrite(LED, HIGH); // 
        //if (service.get_num_buttton()!=0)
        //{
        //    Serial.println("**** sendPacket");
        //}
        xSemaphoreGive(serialMutex);
    }
    esp_task_wdt_reset();
}

void replyTask(void* param)
{
    esp_task_wdt_add(NULL);

    for (;;)
    {
        esp_task_wdt_reset();
        if (needSendReply)
        {
            //xSemaphoreTake(sendMutex, portMAX_DELAY);
            sendPacket_RS485_reply(&replyPacket);
            needSendReply = false;
            //xSemaphoreGive(sendMutex);
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}


void linkWatchdogTask(void* param)
{
    esp_task_wdt_add(NULL);
    TickType_t xLastWakeTime = xTaskGetTickCount();
    static uint32_t last = millis();
    viewActiveLink = true;
    for (;;)
    {
        esp_task_wdt_reset();
        if (hasActiveLink)
        {
            last = millis();
            hasActiveLink = false; // будет сброшен,если не поступит новый пакет
            viewActiveLink = true;
        }
        if (millis() - last > 3000)
        {
            if (viewActiveLink)
            {
                Serial.println("Потеря связи с базой");
                viewActiveLink = false;
                digitalWrite(LED, LOW);

                // Потеря связи
                // Здесь можно включать аварийные индикаторы!
            }
        }
        service.set_connection_base(viewActiveLink);
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(500));
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

//============================================================================


static void rs485SetTX(bool enable) 
{
    digitalWrite(RS485_DE_PIN, enable ? HIGH : LOW);
    if (enable) delayMicroseconds(50);
}

void setupRS485()
{
    RS485_SERIAL.setRxBufferSize(1024);
    RS485_SERIAL.begin(RS485_BAUD, RS485_CONFIG, RS485_RX_PIN, RS485_TX_PIN);
    pinMode(RS485_DE_PIN, OUTPUT);
    digitalWrite(RS485_DE_PIN, LOW);
    rs485SetTX(false);
}

//============================================================================

// ESP32S3 - обработка 3 видов нажатий через прерывание(CHANGE),вывод в Serial в задаче на ядре 0

const int BUTTON_PIN = 45;
const unsigned long DEBOUNCE_MS = 15;
const unsigned long DOUBLE_CLICK_MS = 300;
const unsigned long LONG_PRESS_MS = 2000;

// ISR-состояния
volatile bool buttonPressed = false;
volatile unsigned long pressStartTime = 0;
volatile unsigned long lastInterruptTime = 0;

volatile bool waitingForSecondClick = false;
volatile int shortClickCount = 0;
volatile unsigned long windowEndTime = 0;

volatile bool evtShort = false;
volatile bool evtDouble = false;
volatile bool evtLong = false;

// Новый флаг и время для LONG (для текущего нажатия)
volatile bool longDetected = false;

IRAM_ATTR void onButtonChange()
{
    unsigned long t = millis();
    int state = digitalRead(BUTTON_PIN);

    // дребезг
    if (t - lastInterruptTime < DEBOUNCE_MS) return;
    lastInterruptTime = t;

    if (state == LOW) {
        // кнопка нажата
        if (!buttonPressed) {
            buttonPressed = true;
            pressStartTime = t;
            longDetected = false; // новый нажим,LONG ещё не был
        }
    }
    else {
        // кнопка отпущена
        if (buttonPressed) 
        {
            unsigned long duration = t - pressStartTime;
            buttonPressed = false;

            // LONG обрабатывается в задаче,здесь только SHORT/DOUBLE
            if (duration >= LONG_PRESS_MS) 
            {
                // НЕ обрабатываем здесь LONG,чтобы не дублировать
                // (LONG будет сгенерирован в задаче)
            }
            else 
            {
                // короткое нажатие
                if (waitingForSecondClick && shortClickCount == 1) 
                {
                    evtDouble = true;
                    waitingForSecondClick = false;
                    shortClickCount = 0;
                }
                else 
                {
                    shortClickCount = 1;
                    waitingForSecondClick = true;
                    windowEndTime = t + DOUBLE_CLICK_MS;
                }
            }
        }
    }

    // обработка окна двойного клика внутри ISR остается только таймер окна
    if (waitingForSecondClick && t > windowEndTime) {
        evtShort = true;
        waitingForSecondClick = false;
        shortClickCount = 0;
    }
}

// Задача на ядре 0
static void ButtonTask(void* pvParameters)
{
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), onButtonChange, CHANGE);

    for (;;) 
    {
        // LONG обработка:запускаем примерно через LONG_PRESS_MS после начала нажатия
        if (buttonPressed && !longDetected) 
        {
            if (millis() - pressStartTime >= LONG_PRESS_MS) 
            {
                longDetected = true;
                evtLong = true;
                // длинное нажатие отменяет двойной клик
                waitingForSecondClick = false;
                shortClickCount = 0;
            }
        }

        // Обработка событий
        if (evtLong) 
        {
            evtLong = false;
           // Serial.println("LONG");
            service.set_num_buttton(3);
            needSendReply = true;
        }

        if (evtDouble)
        {
            evtDouble = false;
           // Serial.println("DOUBLE_SHORT");
            service.set_num_buttton(2);
            needSendReply = true;
        }

        if (evtShort) 
        {
            evtShort = false;
           // Serial.println("SHORT");
            service.set_num_buttton(1);
            needSendReply = true;
        }

        // Тайм-аут окна двойного клика:одиночное нажатие зафиксировано
        if (waitingForSecondClick && (millis() >= windowEndTime)) 
        {
            evtShort = true;
            waitingForSecondClick = false;
            shortClickCount = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}


//===============================================================================
bool isValidGNSS_M_tmp = false;
bool new_SOS_flag_M_tmp = false;
uint8_t hour_tmp = -1;
uint8_t minute_tmp = -1;


void set_packet()
{
    if (AuxData.Time_Hour_M != hour_tmp)
    {
        hour_tmp = AuxData.Time_Hour_M; 
        service.set_time_hour(AuxData.Time_Hour_M);
    }

    if (AuxData.Time_Minute_M != minute_tmp)
    {
        minute_tmp = AuxData.Time_Minute_M;
        service.set_time_minute(AuxData.Time_Minute_M);
       // Serial.printf("%d:%d\r\n", AuxFlags.Time_Hour_M, AuxFlags.Time_Minute_M);
    }

    if (AuxData.new_SOS_flag_M != new_SOS_flag_M_tmp)
    {
        new_SOS_flag_M_tmp = AuxData.new_SOS_flag_M;
        service.set_SOS_on_off((bool)AuxData.new_SOS_flag_M);
    }

    if (AuxData.isValidGNSS_M != isValidGNSS_M_tmp)
    {
        isValidGNSS_M_tmp = AuxData.isValidGNSS_M;
        service.set_GNSS_on_off((bool)AuxData.isValidGNSS_M);
       // Serial.printf("AuxFlags.isValidGNSS_M %d \r\n", AuxFlags.isValidGNSS_M);
    }

    if (AuxData.new_message)
    {
        if (strlen(AuxData.msg_resp_M) > 0)
        {
            service.setNewMessageFlag(true);    // Отправить дисплею флаг получения нового сообщения
            service.setMessageRead(true);       // Установить признак о том что новое сообщение получено в выносном дисплее
            strncpy(service.msg_tmp_all, AuxData.msg_resp_M, strlen(AuxData.msg_resp_M));
            memset(AuxData.msg_resp_M, 0, sizeof(AuxData.msg_resp_M));
        }
    }
}
//================================= Управление включения устройства =============================

const uint8_t PIN_BTN = 7;
const uint8_t PIN_POWER = 17;
const uint8_t PIN_PULSE = 20;

const unsigned long HOLD_TIME_MS = 3000;   // 3 сек для кнопки
const unsigned long PULSE_ON_MS = 5000;   // 5 сек (включение)
const unsigned long PULSE_OFF_MS = 6000;   // 6 сек (выключение)

bool deviceOn = false;
bool btnPrevState = HIGH;
unsigned long btnPressStart = 0;
bool holdProcessed = false;

enum State {
    Idle,
    WaitOffPulse
};
State state = Idle;

// Для импульса
bool pulseActive = false;
unsigned long pulseStart = 0;
unsigned long currentPulseDuration = 0;

// ==== функции импульса ====
void startPulse(unsigned long duration) 
{
    digitalWrite(PIN_PULSE, HIGH);
    pulseActive = true;
    pulseStart = millis();
    currentPulseDuration = duration;
}

bool updatePulse(unsigned long now) 
{
    if (pulseActive && (now - pulseStart >= currentPulseDuration)) 
    {
        digitalWrite(PIN_PULSE, LOW);
        pulseActive = false;
        return true;
    }
    return false;
}


void power_Off() 
{
    unsigned long now = millis();
    bool btnState = digitalRead(PIN_BTN);

    switch (state) 
    {
    case Idle:
        // Реакция на новое нажатие кнопки
        if (btnPrevState == HIGH && btnState == LOW) 
        {
            btnPressStart = now;
            holdProcessed = false;
        }
        // Кнопка удерживается
        if (btnState == LOW) 
        {
            if (!holdProcessed && deviceOn && (now - btnPressStart >= HOLD_TIME_MS)) 
            {
                holdProcessed = true;
                SoC->View_powerOff();
                // Импульс 6 сек, потом выключение без задержек
                startPulse(PULSE_OFF_MS);
                state = WaitOffPulse;
            }
        }
        break;

    case WaitOffPulse:
        if (updatePulse(now)) 
        {
            digitalWrite(PIN_POWER, LOW);
            deviceOn = false;
            state = Idle;
        }
        break;
    }

    btnPrevState = btnState;
}

// Функция преобразования напряжения к процентам (от 4,2В до 4,6В)
int voltageToPercent(float voltage) 
{
    const float minV = 4.2;
    const float maxV = 4.6;
    if (voltage <= minV) return 0;
    if (voltage >= maxV) return 100;
    return round((voltage - minV) * 100.0 / (maxV - minV));
}


//==================================================================================================================

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
}


void print_ThisContainer(const ufo_t* ac)
{
    Serial.printf("%06X:%d:%8s:%.0f:%.0f:%.0f:%.0f:%d:%.6f:%.6f:%d:%d:%d:%d\r\n",
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
        ac->rssi,
        ac->last_message_signal_strength_dbm,
        ac->last_message_signal_quality_db,
        ac->signal_source
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
    esp_task_wdt_reset();
}


void printAux(const aux_t* aux)
{
    Serial.println("=== AuxData ===");
    Serial.print("new_buttton_M: "); Serial.println(aux->new_buttton_M);  // Состояние кнопки
    Serial.print("new_message: "); Serial.println(aux->new_message);
    Serial.print("message_received: "); Serial.println(aux->message_received);
    Serial.print("confirm_message_M"); Serial.println(aux->confirm_message_M);
    Serial.print("msg_resp_M: "); Serial.println(aux->msg_resp_M);
    Serial.print("Time_Hour_M: "); Serial.println(aux->Time_Hour_M);
    Serial.print("Time_Minute_M: "); Serial.println(aux->Time_Minute_M);
    Serial.print("new_SOS_flag_M: "); Serial.println(aux->new_SOS_flag_M);
    Serial.print("isValidGNSS_M: "); Serial.println(aux->isValidGNSS_M);
}

//===============================================================================================


unsigned long previousMillis = 0;            // will store last time LED was updated
const long interval = 1000;                  // interval at which to blink (milliseconds)

void setup()
{
    pinMode(SOC_GPIO_PIN_TFT_LED, OUTPUT);
    digitalWrite(SOC_GPIO_PIN_TFT_LED, LOW);
 
    pinMode(PIN_BTN, INPUT_PULLUP);
    pinMode(PIN_POWER, OUTPUT);
    pinMode(PIN_PULSE, OUTPUT);

    digitalWrite(PIN_POWER, LOW);
    digitalWrite(PIN_PULSE, LOW);

    pinMode(LED, OUTPUT);
    digitalWrite(LED, HIGH);
 
    //pinMode(BUTTON_PIN, INPUT_PULLUP);

    Serial.begin(115200);
    delay(500);
    hw_info.soc = SoC_setup(); // Has to be very first procedure in the execution order

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

    Serial.flush();

    EEPROM_setup();

    if (settings->default_settings == SETTINGS_ON)
    {
        EEPROM_clear();
    }

    delay(100);

    digitalWrite(SOC_GPIO_PIN_TFT_LED, HIGH);

     SoC->Display_setup();

    // Проверим, зажата ли кнопка при старте (3 с)
    if (digitalRead(PIN_BTN) == LOW)
    {
        unsigned long tStart = millis();
        while (digitalRead(PIN_BTN) == LOW)
        {
            if (millis() - tStart >= HOLD_TIME_MS)
            {
                // Кнопка удерживалась ≥3 сек при старте — ВКЛ!
                digitalWrite(PIN_POWER, HIGH);
                deviceOn = true;
                // Импульс 5 сек
                digitalWrite(PIN_PULSE, HIGH);
                delay(PULSE_ON_MS);
                digitalWrite(PIN_PULSE, LOW);
                break;
            }
            // Маленькая пауза для экономии CPU
            delay(10);
        }
    }

    WiFi_setup();
 
    OTA_setup();
    Web_setup();
    delay(500);
  
    //attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), onButtonChange, CHANGE);

 
    ina219.begin();

    ThisAircraft = EmptyFO;

    for (int i = 0; i < MAX_TRACKING_OBJECTS; i++)
    {
        Container[i] = EmptyFO;
    }

  //============================================================================
    serialPrintMutex = xSemaphoreCreateMutex();
    serialMutex = xSemaphoreCreateMutex();
    containerMutex = xSemaphoreCreateMutex();
    sendMutex = xSemaphoreCreateMutex();

    setupRS485();

    Serial.print("Sizeof full_packet_net_t:"); Serial.println(sizeof(full_packet_net_t));
    Serial.println("Start End");

    // Запуск задач на Core 0
    xTaskCreatePinnedToCore(rxTask, "RX", 8192, NULL, 2, &Task1, 0);
    xTaskCreatePinnedToCore(buttonsTask, "BTN", 2048, NULL, 1, &Task2, 0);
    xTaskCreatePinnedToCore(replyTask, "REPLY", 2048, NULL, 1, &Task3, 0);
    xTaskCreatePinnedToCore(linkWatchdogTask, "LINK", 1024, NULL, 1, &Task4, 0);
    xTaskCreatePinnedToCore(ButtonTask, "BtnTask", 4096, NULL, 1, &Task5, 0);
    xTaskCreatePinnedToCore(buttonMTask, "ButtonM", 2048, NULL, 1, &Task6, 0);


  //============================================================================
   esp_task_wdt_init(10, false); // таймаут 10 сек,reset chip=true

   Serial.println("======== Setup END!========");
}



void loop()
{
    // Регистрация TWDT для loopTask один раз
    static bool wdt_loop_registered = false;
    if (!wdt_loop_registered)
    {
        esp_task_wdt_add(NULL); // текущая задача - loopTask
        wdt_loop_registered = true;
    }

  //  button_processing();

    power_Off();

    if(!holdProcessed)
    {
        SoC->Display_loop();
    }

    WiFi_loop();
    Web_loop();
    OTA_loop();

    set_packet(); // Получить и записать информацию с базового модуля.

     unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= interval)
    {
        previousMillis = currentMillis;

        if (/*hasActiveLink && packetUpdated &&*/ settings->serial_out == SEND_SERIAL_INFO)
        {
            Serial.print("ThisAircraft:");
            printThisThisAircraft(&ThisAircraft);
            Serial.println("--------------------------------------------------------------------");
            printContainer(Container, MAX_TRACKING_OBJECTS);
            printAux(&AuxData);
            Serial.println("====================================================================");
            Serial.println();
        }

        float shunt_voltage_mV = ina219.getShuntVoltage_mV(); // Напряжение на шунте (милливольты)
        float bus_voltage_V = ina219.getBusVoltage_V();       // Напряжение на шине (вольты)
        float current_mA = ina219.getCurrent_mA();            // Ток (миллиамперы)
        float load_voltage = bus_voltage_V + (shunt_voltage_mV / 1000); // Точное напряжение на нагрузке
        int voltage_percent = voltageToPercent(load_voltage);

        service.set_voltage_value(load_voltage);
        service.set_current_value(current_mA);
    }

    Traffic_loop();
    Time_loop();
    ClearExpired();

    yield();
}

//===================================================================================
