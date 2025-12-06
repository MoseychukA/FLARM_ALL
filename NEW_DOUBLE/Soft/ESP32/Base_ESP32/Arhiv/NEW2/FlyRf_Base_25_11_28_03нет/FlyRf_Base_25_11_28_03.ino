 #include <stdio.h>                // define I/O functions
#include <Arduino.h>              // define I/O functions
#include "SPI.h"
#include <esp_task_wdt.h>
#include <iostream>
#include <locale.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

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
#include <HardwareSerial.h>

// ----------------- RTOS -----------------
TaskHandle_t Task1, Task2, Task3;
SemaphoreHandle_t serialMutex;

int set_air = 0;   //  
bool set_test_coordinate = false; // Признак тестовых ввода текущих координат 
bool set_test_coordinate5 = false; // Признак тестовых ввода текущих координат 

void txrx_test();
void normal();
int threshold_level_tmp = 300;
#if !defined(SERIAL_FLUSH)
#define SERIAL_FLUSH() Serial.flush()
#endif

#define DEBUG 0
#define DEBUG_TIMING 0

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
    static full_packet_net_t recpkt;
    uint8_t btn1, btn2;
    for (;;)
    {
        // esp_task_wdt_reset();
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
    for (;;)
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

// ================= Кнопка: ISR + задача обработки =================

//1. Глобальные переменные / структура для событий
//(volatile, чтобы и ISR, и задача могли читать)

typedef struct {
    volatile bool event;        // был фронт/спад (по CHANGE, ISR ставит true)
    volatile uint32_t timestamp;// millis ISR
    volatile bool state;        // HIGH (отпущена) / LOW (нажата)
} ButtonEvent_t;

ButtonEvent_t btnEvt = { false, 0, true };

const int BUTTON_PIN = 48;

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
                        if (!longReported) AuxData.new_button_M = 3;
                        waitingSecond = false;
                    }
                    else
                    {
                        if (waitingSecond)
                        {
                             //  Serial.println("DOUBLE");
                            service.set_num_button(2);
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
            // Serial.println("LONG");
            service.set_num_button(3);
            waitingSecond = false;
        }

        // Окно двойного клика, если прошло — это одиночный клик (SHORT)
        if (waitingSecond && (int32_t)(millis() - secondWinEndMs) >= 0)
        {
            waitingSecond = false;
            service.set_num_button(1);
            //  Serial.println("SHORT");
        }

        vTaskDelay(tick);
    }
}

//===============================================================================================
void setup()
{
    pinMode(LED, OUTPUT);        // устанавливает режим работы
    digitalWrite(LED, HIGH);

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

    if (resetInfo)
    {
    Serial.println(""); Serial.print(F("Reset reason: ")); Serial.println(resetInfo->reason);
    }
    Serial.println(SoC->getResetReason());
    Serial.print(F("Free heap size: ")); Serial.println(SoC->getFreeHeap());
    Serial.println(SoC->getResetInfo()); Serial.println("");

    SERIAL_FLUSH();

    EEPROM_setup();

    ThisAircraft.addr = SoC->getChipId() & 0x00FFFFFF;

    hw_info.rf = RF_setup();

    delay(100);

    hw_info.baro = Baro_setup();

    hw_info.display = SoC->Display_setup();

    hw_info.gnss = GNSS_setup();
    ThisAircraft.aircraft_type = settings->aircraft_type;
 
    ThisAircraft.protocol = settings->rf_protocol;
    ThisAircraft.stealth  = settings->stealth;
    ThisAircraft.no_track = settings->no_track;
    ThisAircraft.squawk = 1111;

    String ThisAircraft_callsign = String(SoC->getChipId() & 0x00FFFFFFU, HEX);
    ThisAircraft_callsign.toUpperCase();
    memset(ThisAircraft.callsign, 0, sizeof(ThisAircraft.callsign));
    strncpy(ThisAircraft.callsign, ThisAircraft_callsign.c_str(), sizeof(ThisAircraft.callsign) - 1);



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

  //------------------------------------------------------------------------------

    setupRS485();

    serialMutex = xSemaphoreCreateMutex();
    service.initContainerMutex();

    xTaskCreatePinnedToCore(rxTask, "RX", 8192, NULL, 1, &Task1, 1);
    xTaskCreatePinnedToCore(receiveRP2040, "RP2040", 8192, NULL, 2, &Task2, 1);
    xTaskCreatePinnedToCore(ButtonTask, "BtnTask", 4096, NULL, 1, &Task3, 1);

    Serial.print("Sizeof full_packet_net_t: "); Serial.println(sizeof(full_packet_net_t));

    if (settings->threshold_level != threshold_level_tmp)
    {
        threshold_level_tmp = settings->threshold_level;
        values[0] = settings->threshold_level;
        sendPacketToRP2040(values, 1);
    }

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

    if (service.get_num_button()!=0)
    {
        Serial.printf("Принят ответ: new_button_M=%u \r\n", service.get_num_button());
    }

    esp_task_wdt_reset();
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
          ThisAircraft.altitude = 25000.0;

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
//// Функция вывода текущей позиции
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

        Serial.printf(" САМОЛЕТ %d: РАЗВОРОТ НА 180° | НОВЫЙ КУРС: %.1f° \n",
            aircraft5[aircraftIndex].id, aircraft5[aircraftIndex].course);
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
        Serial.printf("  Координаты: %.6f°, %.6f°\n",
            aircraft5[aircraftIndex].latitude, aircraft5[aircraftIndex].longitude);
        Serial.printf("  Курс: %.1f°\n", aircraft5[aircraftIndex].course);
        Serial.printf("  Пройденное расстояние: %.0f из %.0f метров\n",
            aircraft5[aircraftIndex].currentDistance, aircraft5[aircraftIndex].totalDistance);
        Serial.printf("  Направление: %s\n",
            aircraft5[aircraftIndex].movingForward ? "Вперед" : "Назад");
    }
}
//================================================================================================

void txrx_test()
{
    bool success = false;

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
            memcpy((char*)fo.callsign, fly1, strlen(fly1));
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
            if (service.lockContainer())
            {
                Traffic_Update(&fo);
                Traffic_Add(&fo);
                service.unlockContainer();
            }

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
            memcpy((char*)fo.callsign, fly2, strlen(fly2));
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
            if (service.lockContainer())
            {
                Traffic_Update(&fo);
                Traffic_Add(&fo);
                service.unlockContainer();
            }
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
            memcpy((char*)fo.callsign, fly3, strlen(fly3));
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
            if (service.lockContainer())
            {
                Traffic_Update(&fo);
                Traffic_Add(&fo);
                service.unlockContainer();
            }

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
            memcpy((char*)fo.callsign, fly4, strlen(fly4));
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
            if (service.lockContainer())
            {
                Traffic_Update(&fo);
                Traffic_Add(&fo);
                service.unlockContainer();
            }
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
            memcpy((char*)fo.callsign, fly5, strlen(fly5));
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
            if (service.lockContainer())
            {
                Traffic_Update(&fo);
                Traffic_Add(&fo);
                service.unlockContainer();
            }
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
            memcpy((char*)fo.callsign, fly4, strlen(fly4));
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
            if (service.lockContainer())
            {
                Traffic_Update(&fo);
                Traffic_Add(&fo);
                service.unlockContainer();
            }
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
            memcpy((char*)fo.callsign, fly4, strlen(fly4));
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
            if (service.lockContainer())
            {
                Traffic_Update(&fo);
                Traffic_Add(&fo);
                service.unlockContainer();
            }
            //******************************************************************************************************************
            break;
        default:
            break;
        }
        TxPosUpdMarker = millis();
    }


    Baro_loop();
    RF_Transmit(RF_Encode(&ThisAircraft), true);
    success = RF_Receive();
    Traffic_loop();

    if (isTimeToDisplay())
    {
        LEDTimeMarker = millis();
    }

    if (isTimeToExport()) 
    {
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
