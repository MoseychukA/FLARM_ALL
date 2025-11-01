#include <stdio.h>                // define I/O functions
#include <Arduino.h>              // define I/O functions
#include "SPI.h"
#include <esp_task_wdt.h>
#include <iostream>
#include <locale.h>
#include <math.h>

#include "OTA.h"
#include "TimeRF.h"
#include "EEPROMRF.h"
#include "SoC.h"
#include "WiFiRF.h"
#include "WebRF.h"
#include "ESP32RF.h"
#include <TimeLib.h>
#include <TinyGPS++.h>
#include "ServiceMain.h"
#include "Configuration_ESP32.h"
#include "Button.h"
#include <ModbusRTU.h>
#include <HardwareSerial.h>
#include "SoftRF.h"
#include <Wire.h>
#include <Adafruit_INA219.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

ufo_t ThisAircraft;
ufo_t fo, Container[MAX_TRACKING_OBJECTS], EmptyFO, fo_msg, Container_msg[MAX_TRACKING_OBJECTS];

//=======================================================================================
 

#define MAX_TRACKING_OBJECTS 12
#define RS485_SERIAL   Serial1
#define RS485_TX_PIN   39
#define RS485_RX_PIN   38
#define RS485_DE_PIN   40
#define RS485_BAUD     115200
#define RS485_CONFIG   SERIAL_8N1
#define LED            4

#define BTN1_PIN   45
#define BTN2_PIN   18

const uint32_t PACKET_HEADER = 0xAABBCCDD;
const uint32_t PACKET_FOOTER = 0xDDCCBBAA;

volatile bool hasActiveLink = false;
volatile bool viewActiveLink = false;

typedef struct __attribute__((packed)) {
    uint32_t addr;
    int      squawk;
    uint8_t  callsign[8];
    float    altitude;
    float    pressure_altitude;
    float    course;
    float    speed;
    int      vert_rate;
    float    latitude;
    float    longitude;
    int8_t   rssi;
    uint16_t last_message_signal_strength_dbm;
    uint16_t last_message_signal_quality_db;
} ufo_net_t;

typedef struct __attribute__((packed)) {
    bool     new_flag_M;
    uint8_t  new_buttton_M;
    bool     setMessageRead_M;
    bool     MessageRead_M; 
    bool     confirm_message_M;
    uint8_t  Time_Hour_M;
    uint8_t  Time_Minute_M;
    bool     new_SOS_flag_M;
    char     msg_resp_M[170];
    bool     isValidGNSS_M;
} aux_t;

typedef struct __attribute__((packed)) {
    ufo_net_t ThisAircraft;
    ufo_net_t Container[MAX_TRACKING_OBJECTS];
    aux_t AuxData;
    uint8_t BUTTON1;
    uint8_t BUTTON2;
} full_packet_net_t;

SemaphoreHandle_t serialMutex;
SemaphoreHandle_t containerMutex;
SemaphoreHandle_t sendMutex;

aux_t AuxData;

uint8_t BUTTON1 = 0, BUTTON2 = 0;
volatile bool needSendReply = false;
full_packet_net_t replyPacket; // только BUTTON1, BUTTON2 используются
full_packet_net_t lastPacket;
volatile bool packetUpdated = false;
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

void setupRS485()
{
    RS485_SERIAL.setRxBufferSize(1024);
    RS485_SERIAL.begin(RS485_BAUD, RS485_CONFIG, RS485_RX_PIN, RS485_TX_PIN);
    pinMode(RS485_DE_PIN, OUTPUT);
    digitalWrite(RS485_DE_PIN, LOW);
    pinMode(LED, OUTPUT);
    digitalWrite(LED, HIGH);
}

bool receivePacket_RS485(full_packet_net_t* pkt, uint8_t* btn1, uint8_t* btn2)
{
    static uint8_t buffer[sizeof(full_packet_net_t) + 32];
    static size_t idx = 0;

    while (RS485_SERIAL.available()) 
    {
        int bytes = RS485_SERIAL.available();
        if (bytes + idx > sizeof(buffer)) bytes = sizeof(buffer) - idx;
        int n = RS485_SERIAL.readBytes(&buffer[idx], bytes);
        idx += n;
    }
    vTaskDelay(5);
    while (idx >= sizeof(full_packet_net_t) + 8)
    {
        bool header_ok = (buffer[0] == 0xDD && buffer[1] == 0xCC && buffer[2] == 0xBB && buffer[3] == 0xAA);
        size_t footer_off = sizeof(full_packet_net_t) + 4 + 2;
        bool footer_ok = (buffer[footer_off] == 0xAA && buffer[footer_off + 1] == 0xBB &&
            buffer[footer_off + 2] == 0xCC && buffer[footer_off + 3] == 0xDD);

        if (header_ok && footer_ok) 
        {
            uint8_t* data = &buffer[4];
            uint16_t crc_rx = *(uint16_t*)&buffer[4 + sizeof(full_packet_net_t)];
            uint16_t crc_calc = crc16_ccitt(data, sizeof(full_packet_net_t));
            if (crc_rx == crc_calc) {
                memcpy(pkt, data, sizeof(full_packet_net_t));
                if (btn1) *btn1 = pkt->BUTTON1;
                if (btn2) *btn2 = pkt->BUTTON2;
                size_t msgLen = sizeof(full_packet_net_t) + 8;
                idx -= msgLen;
                if (idx)
                    memmove(buffer, buffer + msgLen, idx);
                else
                    idx = 0;
                return true;
            }
        }
        memmove(buffer, buffer + 1, --idx);
        vTaskDelay(5);
    }
    return false;
}

void sendPacket_RS485_reply(const full_packet_net_t* pkt) 
{
    const size_t plen = sizeof(full_packet_net_t);
    static uint8_t buf[sizeof(full_packet_net_t)];
    memcpy(buf, pkt, plen);
    uint16_t crc = crc16_ccitt(buf, plen);

    xSemaphoreTake(serialMutex, portMAX_DELAY);
    digitalWrite(RS485_DE_PIN, HIGH);
    digitalWrite(LED, LOW);
    vTaskDelay(15);
   // delay(15);
    RS485_SERIAL.write((uint8_t*)&PACKET_HEADER, sizeof(PACKET_HEADER));
    RS485_SERIAL.write(buf, plen);
    RS485_SERIAL.write((uint8_t*)&crc, sizeof(crc));
    RS485_SERIAL.write((uint8_t*)&PACKET_FOOTER, sizeof(PACKET_FOOTER));
    RS485_SERIAL.flush();
    vTaskDelay(15);
    //delay(15);
    digitalWrite(RS485_DE_PIN, LOW);
    digitalWrite(LED, HIGH);
    xSemaphoreGive(serialMutex);
   // Serial.println("Ответ отправлен!");
}

void net_to_ufo(const ufo_net_t* src, ufo_t* dst)
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
    dst->rssi = src->rssi;
    dst->last_message_signal_strength_dbm = src->last_message_signal_strength_dbm;
    dst->last_message_signal_quality_db = src->last_message_signal_quality_db;
    vTaskDelay(5);
}


void buttonsTask(void* param) 
{
    for (;;) 
    {
        BUTTON1 = digitalRead(BTN1_PIN);
        BUTTON2 = digitalRead(BTN2_PIN);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void rxTask(void* param)
{
    full_packet_net_t recpkt;
    uint8_t btn1, btn2;
    for (;;) 
    {
        if (receivePacket_RS485(&recpkt, &btn1, &btn2))
        {
            net_to_ufo(&recpkt.ThisAircraft, &ThisAircraft);
            for (int i = 0; i < MAX_TRACKING_OBJECTS; ++i)
            {
                net_to_ufo(&recpkt.Container[i], &Container[i]);
            }
            memcpy(&AuxData, &recpkt.AuxData, sizeof(aux_t));
            // Формируем только кнопочный ответ
            xSemaphoreTake(sendMutex, portMAX_DELAY);
            replyPacket.BUTTON1 = BUTTON1;
            replyPacket.BUTTON2 = BUTTON2;
            needSendReply = true;
            xSemaphoreGive(sendMutex);
           // Serial.print("Принят пакет, кнопки; "); Serial.print(BUTTON1); Serial.print(' '); Serial.println(BUTTON2);
        }
        memcpy(&lastPacket, &recpkt, sizeof(full_packet_net_t));
        packetUpdated = true;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void replyTask(void* param) 
{
    for (;;)
    {
        if (needSendReply) 
        {
            xSemaphoreTake(sendMutex, portMAX_DELAY);
            sendPacket_RS485_reply(&replyPacket);
            needSendReply = false;
            xSemaphoreGive(sendMutex);
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void linkWatchdogTask(void* param) 
{
    static uint32_t last = millis();
    for (;;) 
    {
        if (hasActiveLink) 
        {
            last = millis();
            hasActiveLink = false; // будет сброшен, если не поступит новый пакет
           // viewActiveLink = true;
        }
        if (millis() - last > 2000)
        {
 /*           if (viewActiveLink)
            {*/
                Serial.println("Потеря связи с базой");
                //viewActiveLink = false;
            //}

            // Потеря связи
            // Здесь можно включать аварийные индикаторы!
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}


//
//bool receivePacket_RS485(full_packet_t* pkt)
//{
//    static uint8_t buffer[sizeof(full_packet_t) + 32];
//    static size_t idx = 0;
//
//    // Прочитать все накопившиеся байты из UART
//    while (RS485_SERIAL.available()) {
//        int bytes = RS485_SERIAL.available();
//        // Переполнения буфера не будет
//        if (bytes + idx > sizeof(buffer)) bytes = sizeof(buffer) - idx;
//        int n = RS485_SERIAL.readBytes(&buffer[idx], bytes);
//        idx += n;
//    }
//    // Serial.print("IDX: "); Serial.println(idx);
//     //// Отладка: дамп начала буфера (первые 24 байта + последние 8)
//     //if (idx > 0) 
//     //{
//     //    Serial.print("HEX DUMP START: ");
//     //    for (size_t i = 0; i < 24 && i < idx; ++i) Serial.printf("%02X ", buffer[i]);
//     //    Serial.println();
//     //    // Для отладки можно вывести и конец буфера — так увидим footer
//     //    if (idx > (sizeof(full_packet_t) + 4 + 2 + 4 - 8)) 
//     //    {
//     //        Serial.print("HEX DUMP END: ");
//     //        for (size_t i = idx - 8; i < idx; ++i) Serial.printf("%02X ", buffer[i]);
//     //        Serial.println();
//     //    }
//     //}
//
//     // Проверка наличия полного пакета
//    while (idx >= sizeof(full_packet_t) + 8) // header(4)+packet+crc(2)+footer(4)
//    {
//        // Проверяем little endian header (0xAABBCCDD → DD CC BB AA)
//        bool header_ok = (buffer[0] == 0xDD && buffer[1] == 0xCC && buffer[2] == 0xBB && buffer[3] == 0xAA);
//        size_t footer_off = sizeof(full_packet_t) + 4 + 2;
//        // Проверяем footer — должен быть (0xDDCCBBAA → AA BB CC DD)
//        bool footer_ok = (buffer[footer_off] == 0xAA && buffer[footer_off + 1] == 0xBB &&
//            buffer[footer_off + 2] == 0xCC && buffer[footer_off + 3] == 0xDD);
//
//        //Serial.print("HEADER OK: "); Serial.println(header_ok ? "YES" : "NO");
//        //Serial.print("FOOTER OK: "); Serial.println(footer_ok ? "YES" : "NO");
//
//        if (header_ok && footer_ok) {
//            // CRC и содержимое
//            uint8_t* data = &buffer[4];
//            uint16_t crc_rx = *(uint16_t*)&buffer[4 + sizeof(full_packet_t)];
//            uint16_t crc_calc = crc16_ccitt(data, sizeof(full_packet_t));
//            //Serial.print("CRC RX: "); Serial.println(crc_rx, HEX);
//            //Serial.print("CRC CALC: "); Serial.println(crc_calc, HEX);
//
//            if (crc_rx == crc_calc) {
//                memcpy(pkt, data, sizeof(full_packet_t));
//                //Serial.println("=== Packet accepted! ===");
//                //// Распечатать важные поля (покажи кнопки и что-то по содержимому)
//                //Serial.print("BUTTON1: "); Serial.println(pkt->BUTTON1);
//                //Serial.print("BUTTON2: "); Serial.println(pkt->BUTTON2);
//                //Serial.print("ThisAircraft.lat: "); Serial.println(pkt->ThisAircraft.latitude, 8);
//                //Serial.print("AuxData.Time_Hour_M: "); Serial.println(pkt->AuxData.Time_Hour_M);
//                //// и т.д. по желанию
//
//                // Сдвигаем буфер: вдруг следом следующий пакет!
//                size_t msgLen = sizeof(full_packet_t) + 8;
//                idx -= msgLen;
//                if (idx)
//                    memmove(buffer, buffer + msgLen, idx);
//                else
//                    idx = 0;
//                return true;
//            }
//            else {
//                Serial.println("!!! CRC ERROR !!!");
//            }
//        } // if header/footer
//
//        // Если не нашли header/footer/CRC — смещаем буфер на 1 байт
//        memmove(buffer, buffer + 1, --idx);
//    }
//    return false;
//}
//
//// Опрос кнопок
//void buttonsTask(void* param)
//{
//    for (;;)
//    {
//        BUTTON1 = digitalRead(BTN1_PIN);
//        BUTTON2 = digitalRead(BTN2_PIN);
//        vTaskDelay(pdMS_TO_TICKS(50));
//    }
//}
//
//// Прием и формирование ответа (теперь только после запроса мастер)
//void rxTask(void* param) 
//{
//    full_packet_t packet;
//    for (;;) {
//        if (receivePacket_RS485(&packet)) {
//            xSemaphoreTake(containerMutex, portMAX_DELAY);
//            // Вот здесь обновляются твои переменные!
//            memcpy(&ThisAircraft, &packet.ThisAircraft, sizeof(ufo_t));
//            memcpy(&Container, &packet.Container, sizeof(Container));
//            memcpy(&AuxData, &packet.AuxData, sizeof(aux_t));
//            xSemaphoreGive(containerMutex);
//
//            // Если нужны кнопки:
//            uint8_t btn1 = packet.BUTTON1;
//            uint8_t btn2 = packet.BUTTON2;
//
//            //// Готовим ответ
//            //memcpy(&replyPacket.ThisAircraft, &ThisAircraft, sizeof(ufo_t));
//            //memcpy(&replyPacket.Container, &Container, sizeof(Container));
//            //memcpy(&replyPacket.AuxData, &AuxData, sizeof(aux_t));
//            replyPacket.BUTTON1 = BUTTON1;
//            replyPacket.BUTTON2 = BUTTON2;
//            needSendReply = true;
//            // xSemaphoreGive(containerMutex);
//            dataReceivedFlag = true;
//        }
//        vTaskDelay(pdMS_TO_TICKS(5));
//    }
//}
//
//// Ответ только по запросу!
//void replyTask(void* param)
//{
//    for (;;)
//    {
//        if (needSendReply)
//        {
//            sendPacket_RS485(&replyPacket);
//            needSendReply = false;
//        }
//        vTaskDelay(pdMS_TO_TICKS(2));
//    }
//}



void printThisAircraft(const ufo_net_t* ac) 
{
    Serial.printf("%06X:%d:%8s:%.0f:%.0f:%.0f:%.0f:%d:%.6f:%.6f:%d:%d:%d\r\n",
        ac->addr,                   // Адрес устройства стороннего самолета
        ac->squawk,                 // Номер, назначаемый диспетчером для обмена с локатором. 
        ac->callsign,                  // Номер рейса
        ac->altitude,
        ac->altitude,
        ac->speed,
        ac->course,
        ac->vert_rate,
        ac->latitude,
        ac->longitude,
        ac->rssi,
        ac->last_message_signal_strength_dbm,
        ac->last_message_signal_quality_db);
    vTaskDelay(5);
}

void printAux(const aux_t* aux) 
{
    Serial.println("=== AuxData ===");
    Serial.print("new_flag_M: "); Serial.println(aux->new_flag_M);
    Serial.print("new_buttton_M: "); Serial.println(aux->new_buttton_M);
    Serial.print("setMessageRead_M: "); Serial.println(aux->setMessageRead_M);
    Serial.print("MessageRead_M: "); Serial.println(aux->MessageRead_M);
    Serial.print("Time_Hour_M: "); Serial.println(aux->Time_Hour_M);
    Serial.print("Time_Minute_M: "); Serial.println(aux->Time_Minute_M);
    Serial.print("new_SOS_flag_M: "); Serial.println(aux->new_SOS_flag_M);
    Serial.print("msg_resp_M: "); Serial.println(aux->msg_resp_M);
    Serial.print("isValidGNSS_M: "); Serial.println(aux->isValidGNSS_M);
    vTaskDelay(5);
}

void printContainer(const ufo_net_t* arr, int n) 
{
    for (int i = 0; i < n; ++i) 
    {
        Serial.print("Container["); 
        Serial.print(i); Serial.print("]:");
        printThisAircraft(&arr[i]);
        vTaskDelay(5);
    }
}


//=======================================================================================
Adafruit_INA219 ina219;

#if !defined(SERIAL_FLUSH)
#define SERIAL_FLUSH() Serial.flush()
#endif

#define DEBUG 0
#define DEBUG_TIMING 0
 
#define isTimeToDisplay() (millis() - LEDTimeMarker     > 1000)
#define isTimeToExport()  (millis() - ExportTimeMarker  > 1000)

hardware_info_t hw_info = {
  .model    = DEFAULT_FLYRF_MODEL,
  .revision = 0,
  .soc      = SOC_NONE,
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


bool isValidGNSS_M_tmp = false;
bool new_SOS_flag_M_tmp = false;
uint8_t hour_tmp = 10;
uint8_t minute_tmp = 10;


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



// Функция преобразования напряжения к процентам (от 3,5В до 4,6В)
int voltageToPercent(float voltage) 
{
    const float minV = 3.5;
    const float maxV = 4.6;
    if (voltage <= minV) return 0;
    if (voltage >= maxV) return 100;
    return round((voltage - minV) * 100.0 / (maxV - minV));
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
    pinMode(BTN1_PIN, INPUT);
    pinMode(BTN2_PIN, INPUT);
    pinMode(LED, OUTPUT);
    digitalWrite(LED, HIGH);
   
 
    rst_info* resetInfo;
    Serial.begin(115200);
    vTaskDelay(500);
   // delay(500);
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

  delay(100);

  digitalWrite(SOC_GPIO_PIN_TFT_LED, HIGH);
  hw_info.display = SoC->Display_setup();

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
              vTaskDelay(PULSE_ON_MS);
             // delay(PULSE_ON_MS);
              digitalWrite(PIN_PULSE, LOW);
              break;
          }
          // Маленькая пауза для экономии CPU
          delay(10);
      }
  }
 
  ThisAircraft.aircraft_type = settings->aircraft_type;
 
  ThisAircraft.protocol = settings->rf_protocol;
  ThisAircraft.stealth  = settings->stealth;
  ThisAircraft.no_track = settings->no_track;

  if (settings->input_coordinates == IMPUT_COORD_MANUAL)
  {
      ThisAircraft.test_latitude = settings->test_latitude;
      ThisAircraft.test_longitude = settings->test_longitude;
  }

  SoC->swSer_enableRx(false);

  WiFi_setup();
 
  OTA_setup();
  Web_setup();
  delay(500);
    
  SoC->post_init();
 
  // initializing a button
  Button* btn = new Button(GPIO_NUM_45, false);

  btn->attachPressDownEventCb(&onButtonPressDownCb, NULL);
  btn->attachDoubleClickEventCb(&onButtonDoubleClickEventCb, NULL);
  btn->attachLongPressStartEventCb(onButtonLongPressStartEventCb, NULL);
 
  
  //============================================================================


  serialMutex = xSemaphoreCreateMutex();
  containerMutex = xSemaphoreCreateMutex();
  sendMutex = xSemaphoreCreateMutex();

  setupRS485();

  Serial.print("Sizeof full_packet_net_t: "); Serial.println(sizeof(full_packet_net_t));
  Serial.println("Start End");

  xTaskCreatePinnedToCore(rxTask, "RX", 8192, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(buttonsTask, "BTN", 2048, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(replyTask, "REPLY", 2048, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(linkWatchdogTask, "LINK", 1024, NULL, 1, NULL, 1);

  //============================================================================

  ina219.begin();
   //ina219.setCalibration_32V_2A(); // По умолчанию калибровка на 32В и 2А. Меняйте, если нужно.

  SoC->WDT_setup();

  Serial.println("======== Setup END!========");
}



void loop()
{
    esp_task_wdt_reset();
    power_Off();
    set_packet(); // Получить и записать информацию с базового модуля.

    if(!holdProcessed)
    {
        SoC->Display_loop();
    }

    WiFi_loop();
    Web_loop();
    OTA_loop();
    SoC->loop();

    if (settings->serial_out == SEND_SERIAL_INFO)
    {
        static uint32_t tmr = 0;

        if (hasActiveLink && packetUpdated && millis() - tmr > 1000)
        {
            tmr = millis();
            packetUpdated = false;
            Serial.println("==== Последний принятый пакет ====");
            Serial.print("ThisAircraft :");
            printThisAircraft(&lastPacket.ThisAircraft);
            printContainer(lastPacket.Container, MAX_TRACKING_OBJECTS);
            printAux(&lastPacket.AuxData);
            //Serial.print("Button1: "); Serial.println(lastPacket.BUTTON1);
            //Serial.print("Button2: "); Serial.println(lastPacket.BUTTON2);
            Serial.println("==================================");
        }
    }
    //if (strlen(AuxFlags.msg_resp_M) > 0)
    //{
    //    service.setNewMessageFlag(true);
    //    Serial.println(AuxFlags.msg_resp_M);
    //    strncpy(service.msg_tmp_all, AuxFlags.msg_resp_M, strlen(AuxFlags.msg_resp_M));
    //    //!!strncpy(AuxFlags.msg_resp_M, "", strlen(AuxFlags.msg_resp_M));
    //    AuxFlags.msg_resp_M[0] = 0;
    //}

    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= interval)
    {
        previousMillis = currentMillis;

        float shunt_voltage_mV = ina219.getShuntVoltage_mV(); // Напряжение на шунте (милливольты)
        float bus_voltage_V = ina219.getBusVoltage_V();       // Напряжение на шине (вольты)
        float current_mA = ina219.getCurrent_mA();            // Ток (миллиамперы)
        float load_voltage = bus_voltage_V + (shunt_voltage_mV / 1000); // Точное напряжение на нагрузке
        int voltage_percent = voltageToPercent(load_voltage);

        service.set_voltage_value(load_voltage);
        service.set_current_value(current_mA);

        //Serial.print("Напряжение шины: "); Serial.print(bus_voltage_V); Serial.println(" V");
        //Serial.print("Напряжение на шунте: "); Serial.print(shunt_voltage_mV); Serial.println(" mV");
        //Serial.print("Ток: "); Serial.print(current_mA); Serial.println(" mA");
        //Serial.print("Напряжение на нагрузке: "); Serial.print(load_voltage); Serial.println(" V");
        //Serial.println("-----");
    }


    // Пример управления выходом по кнопкам (если нужно)
    if (BUTTON1 == LOW || BUTTON2 == LOW)
    {
        digitalWrite(LED, LOW); // если хотите управлять пьезо/светодиодом
    }
    else
    {
        digitalWrite(LED, HIGH);
    }



    Time_loop();

    yield();
}

//===================================================================================
