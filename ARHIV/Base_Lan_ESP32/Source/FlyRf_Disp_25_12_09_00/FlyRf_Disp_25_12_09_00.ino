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
#include "EEPROMRF.h"
#include "SoC.h" 
#include "WiFiRF.h"
#include "WebRF.h"
#include "ESP32RF.h"
#include <TimeLib.h>
#include "TrafficHelper.h"
#include "ServiceMain.h"
#include "Configuration_ESP32.h"
#include <HardwareSerial.h>
#include "SoftRF.h"
#include <Wire.h>
#include <Adafruit_INA219.h>

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
#define BTN2_PIN 7

//================================= Управление включения устройства =============================

// Пин,на котором нужен 5‑секундный импульс
#define PULSE_PIN GPIO_NUM_20             // GPIO20 (проверьте,что он поддерживает RTC GPIO)
#define TRIGGER_PIN GPIO_NUM_18          // Пин,по которому снаружи подаётся низкий уровень при отключении питания
#define POWER_ON_PIN GPIO_NUM_17          // Поддержка ключа питания
const unsigned long PULSE_ON_MS = 3500;   // 5 сек (включение)
const unsigned long PULSE_OFF_MS = 6000;  // 6 сек (выключение)
#define DEBOUNCE_DELAY 30                 // Задержка антидребезга в мс
bool wasDisabled = false;

//========================================================================================


const uint32_t PACKET_HEADER = 0xAABBCCDD;
const uint32_t PACKET_FOOTER = 0xDDCCBBAA;

Adafruit_INA219 ina219;
bool ina219_ok = false;

volatile bool hasActiveLink = false;
volatile bool viewActiveLink = false;
volatile bool viewActiveLink_tmp = false;


//============================================================================
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
    int8_t   rssi_LoRa;
    int8_t   rssi_rp2040;
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

hardware_info_t hw_info = {
  .model    = DEFAULT_FLYRF_MODEL,
  .revision = 0,
  .soc      = SOC_NONE,
  .display  = DISPLAY_NONE,
};


//============================================================================

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
    dst->rssi_LoRa = src->rssi_LoRa;
    dst->rssi_rp2040 = src->rssi_rp2040;
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

//========================================================================================

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

// ----------------- Задачи -----------------
void rxTask(void* param) 
{
    static full_packet_net_t packet;
    uint8_t btn1, btn2;
    esp_task_wdt_add(NULL); // регистрируем только RX в WDT
    for (;;) 
    {
        esp_task_wdt_reset();
        if (receivePacket_RS485(&packet, &btn1, &btn2))
        {
            digitalWrite(LED, LOW);

            // Обновление локальных структур
            net_to_ufo_ThisAircraft(&packet.ThisAircraft, &ThisAircraft);
            for (int i = 0; i < MAX_TRACKING_OBJECTS; ++i)
            {
                net_to_ufo_Container(&packet.Container[i], &Container[i]);
            }

            memcpy(&AuxData, &packet.AuxData, sizeof(aux_t));

             // Формируем ответ
            replyPacket.BUTTON1 = BUTTON1;
            replyPacket.BUTTON2 = BUTTON2;
            // Serial.print("Button1 = "); Serial.println(btn1);
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


// ----------------- Мгновенная отправка new_buttton_M -----------------
void sendImmediateNewButtonM(uint8_t value) 
{
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

// быстрая задача: немедленная отправка new_buttton_M от AuxData
void buttonMTask(void* param) 
{
    for (;;) 
    {
        // Здесь вы обновляете AuxData.new_buttton_M как вам нужно (из кнопок/сервиса и т.п.)
        // В примере — из события в другой задаче (ButtonTask) меняется AuxData.new_buttton_M
 
        if (AuxData.new_buttton_M != 0) 
        {
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

void linkWatchdogTask(void* param) 
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    static uint32_t last = millis();
    viewActiveLink = true;
    for (;;) 
    {
        if (hasActiveLink) 
        {
            last = millis();
            hasActiveLink = false; // сброс — если за период не придёт следующий пакет, признаем линк потерян
            viewActiveLink = true;
        }
        if (millis() - last > 3000) 
        {
            if (viewActiveLink) 
            {
                Serial.println("Потеря связи с базой");
                viewActiveLink = false;
                digitalWrite(LED, LOW);
                for (int i = 0; i < MAX_TRACKING_OBJECTS; i++)
                {
                    Container[i] = EmptyFO;
                }
                //service.set_time_hour(0);
                //service.set_time_minute(0);
            }
        }
        if (viewActiveLink != viewActiveLink_tmp) 
        {
            viewActiveLink_tmp = viewActiveLink;
            service.set_connection_base(viewActiveLink);
            if (viewActiveLink) 
            {
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

IRAM_ATTR void onButtonChange()
{
    btnEvt.event = true;
    btnEvt.timestamp = xTaskGetTickCountFromISR(); // или просто tick
    btnEvt.state = gpio_get_level((gpio_num_t)BUTTON_PIN);
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
                            //  Serial.println("DOUBLE");
                            service.set_num_buttton(2);
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
            // Serial.println("LONG");
            service.set_num_buttton(3);
            waitingSecond = false;
        }

        // Окно двойного клика, если прошло — это одиночный клик (SHORT)
        if (waitingSecond && (int32_t)(millis() - secondWinEndMs) >= 0)
        {
            waitingSecond = false;
            AuxData.new_buttton_M = 1;
            service.set_num_buttton(1);
            //  Serial.println("SHORT");
        }

        vTaskDelay(tick);
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
    pinMode(POWER_ON_PIN, OUTPUT);
    digitalWrite(POWER_ON_PIN, HIGH);

    pinMode(SOC_GPIO_PIN_TFT_LED, OUTPUT);
    digitalWrite(SOC_GPIO_PIN_TFT_LED, LOW);
 
    pinMode(PULSE_PIN, OUTPUT);
    digitalWrite(PULSE_PIN, LOW);

    pinMode(LED, OUTPUT);
    digitalWrite(LED, HIGH);

    pinMode(TRIGGER_PIN, INPUT_PULLUP); // ожидаем активный LOW на pin 18
    pinMode(BTN1_PIN, INPUT_PULLUP);
    pinMode(BTN1_PIN, INPUT_PULLUP);

 
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


    EEPROM_setup();
    delay(100);

    digitalWrite(SOC_GPIO_PIN_TFT_LED, HIGH);

     SoC->Display_setup();

   //  delay(PULSE_ON_MS);
  
   //  digitalWrite(PULSE_PIN, LOW);

    WiFi_setup();
 
    OTA_setup();
    Web_setup();
    delay(500);
 
    Wire.begin(); // или Wire.begin(SDA, SCL) при нестандартных пинах

    ina219.begin(); // у старых версий возвращает void

// Проба адреса INA219 (обычно 0x40)
    const uint8_t INA219_ADDR = 0x40;
    Wire.beginTransmission(INA219_ADDR);
    ina219_ok = (Wire.endTransmission() == 0);

    if (!ina219_ok) {
        Serial.println("INA219 init failed or not found at 0x40. Skip measurements.");
    }

    ThisAircraft = EmptyFO;

    for (int i = 0; i < MAX_TRACKING_OBJECTS; i++)
    {
        Container[i] = EmptyFO;
    }


    setupRS485();

    Serial.print("Sizeof full_packet_net_t:"); Serial.println(sizeof(full_packet_net_t));


  //============================================================================

    serialMutex = xSemaphoreCreateMutex();
    containerMutex = xSemaphoreCreateMutex();

    pinMode(BUTTON_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), onButtonChange, CHANGE);

    // Разносим задачи по ядрам: RX/LINK — core 0, кнопочные — core 1
    xTaskCreatePinnedToCore(rxTask, "RX", 8192, NULL, 2, &Task1, 0);
    xTaskCreatePinnedToCore(linkWatchdogTask, "LINK", 2048, NULL, 1, &Task2, 0);
    xTaskCreatePinnedToCore(buttonMTask, "ButtonM", 4096, NULL, 2, &Task3, 1);
    xTaskCreatePinnedToCore(ButtonTask, "BtnTask", 4096, NULL, 1, &Task4, 1);

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

    if (!wasDisabled && digitalRead(TRIGGER_PIN) == LOW)
    {
        digitalWrite(POWER_ON_PIN, HIGH);
        delay(DEBOUNCE_DELAY);
        if (digitalRead(TRIGGER_PIN) == LOW)
        {
           // Состояние стабильно
            wasDisabled = true; // Чтобы не повторять процесс
            SoC->View_powerOff(); // вызов сообщения об отключении питания устройства
            digitalWrite(PULSE_PIN, HIGH);
            delay(PULSE_OFF_MS);
            digitalWrite(PULSE_PIN, LOW);
            delay(100);
            digitalWrite(POWER_ON_PIN, LOW);
         }
    }
    else
    {

        SoC->Display_loop();

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

            if (ina219_ok)
            {
                digitalWrite(POWER_ON_PIN, LOW);
                delay(20);
                float shunt_voltage_mV = ina219.getShuntVoltage_mV(); // Напряжение на шунте (милливольты)
                float bus_voltage_V = ina219.getBusVoltage_V();       // Напряжение на шине (вольты)
                float current_mA = ina219.getCurrent_mA();            // Ток (миллиамперы)
                float load_voltage = bus_voltage_V + (shunt_voltage_mV / 1000); // Точное напряжение на нагрузке
                int voltage_percent = voltageToPercent(load_voltage);
                digitalWrite(POWER_ON_PIN, HIGH);
                service.set_voltage_value(load_voltage);
                service.set_current_value(current_mA);
            }
        }

        Traffic_loop();
        Time_loop();
        ClearExpired();
    }
    yield();

}

//===================================================================================
