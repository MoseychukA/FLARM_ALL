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
//#include <ModbusRTU.h>
//#include <HardwareSerial.h>
#include "SoftRF.h"
#include <SimpleModbusSlave.h>


#if !defined(SERIAL_FLUSH)
#define SERIAL_FLUSH() Serial.flush()
#endif

#define DEBUG 0
#define DEBUG_TIMING 0
 
#define isTimeToDisplay() (millis() - LEDTimeMarker     > 1000)
#define isTimeToExport()  (millis() - ExportTimeMarker  > 1000)




ufo_t ThisAircraft;
ufo_t fo, Container[MAX_TRACKING_OBJECTS], EmptyFO, fo_msg, Container_msg[MAX_TRACKING_OBJECTS];

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
//============================================================================
//typedef struct UFO {
//    time_t    timestamp;
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
//    char      flight[16];
//    int       vert_rate;
//    int       Squawk;
//    time_t    timemsg;
//    float     vs;
//    float     geoid_separation;
//    uint16_t  hdop;
//    int8_t    rssi;
//    float     distance;
//    float     bearing;
//    uint8_t   signal_source;
//    time_t    seen;
//    uint8_t   hour_msg;
//    uint8_t   min_msg;
//    uint16_t  delay_time_msg;
//    uint8_t   callsign[8];
//} ufo_t;

//// Данные для приема
//ufo_t Container[MAX_TRACKING_OBJECTS];
//ufo_t ThisAircraft;

// Дополнительные данные
bool new_flagM;
uint8_t new_buttonM;
bool setMessageReadM;
bool MessageReadM;
bool SOS_Sprite_on_offM;
bool SOS_View_on_offM;
bool new_SOS_flagM;
bool confirm_messageM;
char msg_respM[60];
bool isValidGNSSM;
uint8_t FLYRF_MODE_TESTM;
uint16_t analog_signalM;

// MODBUS регистры - изменяем тип на uint16_t для совместимости
#define HOLDING_REGS_SIZE 10000
uint16_t holdingRegs[HOLDING_REGS_SIZE];

HardwareSerial RS485Serial(1);

// Команды MODBUS
#define CMD_SEND_CONTAINER    0x01
#define CMD_SEND_THISAIRCRAFT 0x02
#define CMD_SEND_ADDITIONAL   0x03
#define CMD_SEND_ANALOG       0x04

// Структура для управления приемом
typedef struct {
    SemaphoreHandle_t data_mutex;
    TaskHandle_t processing_task_handle;
    bool new_data_available[4];
    uint32_t last_receive_time[4];
    uint32_t total_receive_count;
    uint32_t receive_stats[5];
    bool data_integrity_ok;
    uint32_t last_register_write;
    uint16_t previous_command;
} modbus_receive_t;

modbus_receive_t modbus_recv;

// Функция восстановления float из двух uint16_t
float registersToFloat(uint16_t reg1, uint16_t reg2) {
    union {
        float f;
        uint32_t i;
    } converter;

    converter.i = ((uint32_t)reg1 << 16) | reg2;
    return converter.f;
}

// Функция восстановления time_t из двух uint16_t
time_t registersToTime(uint16_t reg1, uint16_t reg2) {
    return ((time_t)reg1 << 16) | reg2;
}

// Распаковка UFO из буфера регистров
uint16_t unpackUFOFromBuffer(const uint16_t* buffer, ufo_t* obj) {
    uint16_t pos = 0;

    // timestamp (2 регистра)
    obj->timestamp = registersToTime(buffer[pos], buffer[pos + 1]);
    pos += 2;

    // addr (3 регистра)
    obj->addr = ((uint32_t)buffer[pos] << 16) | buffer[pos + 1];
    pos += 2;
    obj->addr_type = buffer[pos++];

    // координаты (8 регистров)
    obj->latitude = registersToFloat(buffer[pos], buffer[pos + 1]); pos += 2;
    obj->longitude = registersToFloat(buffer[pos], buffer[pos + 1]); pos += 2;
    obj->old_latitude = registersToFloat(buffer[pos], buffer[pos + 1]); pos += 2;
    obj->old_longitude = registersToFloat(buffer[pos], buffer[pos + 1]); pos += 2;

    // высоты и параметры полета (10 регистров)
    obj->altitude = registersToFloat(buffer[pos], buffer[pos + 1]); pos += 2;
    obj->pressure_altitude = registersToFloat(buffer[pos], buffer[pos + 1]); pos += 2;
    obj->course = registersToFloat(buffer[pos], buffer[pos + 1]); pos += 2;
    obj->speed = registersToFloat(buffer[pos], buffer[pos + 1]); pos += 2;
    obj->vs = registersToFloat(buffer[pos], buffer[pos + 1]); pos += 2;

    obj->aircraft_type = buffer[pos++];

    // flight (8 регистров)
    for (int i = 0; i < 8; i++) {
        if (pos < 80) { // Защита от переполнения
            obj->flight[i * 2] = buffer[pos] >> 8;
            obj->flight[i * 2 + 1] = buffer[pos] & 0xFF;
            pos++;
        }
    }
    obj->flight[15] = '\0';

    // vert_rate и Squawk (4 регистра)
    if (pos + 1 < 80) {
        obj->vert_rate = ((int32_t)buffer[pos] << 16) | buffer[pos + 1];
        pos += 2;
        obj->Squawk = ((int32_t)buffer[pos] << 16) | buffer[pos + 1];
        pos += 2;
    }

    // timemsg (2 регистра)
    if (pos + 1 < 80) {
        obj->timemsg = registersToTime(buffer[pos], buffer[pos + 1]);
        pos += 2;
    }

    // geoid_separation, distance, bearing (6 регистров)
    if (pos + 5 < 80) {
        obj->geoid_separation = registersToFloat(buffer[pos], buffer[pos + 1]); pos += 2;
        obj->distance = registersToFloat(buffer[pos], buffer[pos + 1]); pos += 2;
        obj->bearing = registersToFloat(buffer[pos], buffer[pos + 1]); pos += 2;
    }

    // остальные параметры
    if (pos + 2 < 80) {
        obj->hdop = buffer[pos++];
        obj->rssi = (int8_t)buffer[pos++];
        obj->signal_source = buffer[pos++];
    }

    // seen (2 регистра)
    if (pos + 1 < 80) {
        obj->seen = registersToTime(buffer[pos], buffer[pos + 1]);
        pos += 2;
    }

    if (pos + 2 < 80) {
        obj->hour_msg = buffer[pos++];
        obj->min_msg = buffer[pos++];
        obj->delay_time_msg = buffer[pos++];
    }

    // callsign (4 регистра)
    for (int i = 0; i < 4 && pos < 80; i++) {
        obj->callsign[i * 2] = buffer[pos] >> 8;
        obj->callsign[i * 2 + 1] = buffer[pos] & 0xFF;
        pos++;
    }

    return pos;
}

// Обработка полученного объекта Container
void processContainerObject(uint8_t index, const uint16_t* data_start, uint16_t data_length) {
    if (index >= MAX_TRACKING_OBJECTS) {
        Serial.printf("Invalid Container index: %d\n", index);
        return;
    }

    uint32_t start_time = micros();

    uint16_t processed = unpackUFOFromBuffer(data_start, &Container[index]);

    if (processed <= data_length) {
        modbus_recv.last_receive_time[0] = millis();
        modbus_recv.data_integrity_ok = true;

        Serial.printf("Container[%d]: lat=%.6f, lon=%.6f, alt=%.1f\n",
            index, Container[index].latitude, Container[index].longitude,
            Container[index].altitude);
    }
    else {
        Serial.printf("Data integrity error for Container[%d]\n", index);
        modbus_recv.data_integrity_ok = false;
    }

    uint32_t process_time = micros() - start_time;
    Serial.printf("Container[%d] processed in %d μs\n", index, process_time);
}

// Обработка полученного ThisAircraft
void processThisAircraft(const uint16_t* data_start, uint16_t data_length) {
    uint32_t start_time = micros();

    uint16_t processed = unpackUFOFromBuffer(data_start, &ThisAircraft);

    if (processed <= data_length) {
        modbus_recv.last_receive_time[1] = millis();
        modbus_recv.data_integrity_ok = true;

        Serial.printf("ThisAircraft: lat=%.6f, lon=%.6f, alt=%.1f\n",
            ThisAircraft.latitude, ThisAircraft.longitude,
            ThisAircraft.altitude);
    }
    else {
        Serial.printf("Data integrity error for ThisAircraft\n");
        modbus_recv.data_integrity_ok = false;
    }

    uint32_t process_time = micros() - start_time;
    Serial.printf("ThisAircraft processed in %d μs\n", process_time);
}

// Обработка дополнительных данных
void processAdditionalData(const uint16_t* data_start, uint16_t data_length) {
    uint32_t start_time = micros();
    uint16_t pos = 0;

    if (data_length >= 10) 
    {
        new_flagM = data_start[pos++] != 0;
        new_buttonM = data_start[pos++];
        setMessageReadM = data_start[pos++] != 0;
        MessageReadM = data_start[pos++] != 0;
        SOS_Sprite_on_offM = data_start[pos++] != 0;
        SOS_View_on_offM = data_start[pos++] != 0;
        new_SOS_flagM = data_start[pos++] != 0;
        confirm_messageM = data_start[pos++] != 0;
        isValidGNSSM = data_start[pos++] != 0;
        FLYRF_MODE_TESTM = data_start[pos++];

        // msg_resp (30 регистров = 60 символов)
        for (int i = 0; i < 30 && pos < data_length; i++) {
            msg_respM[i * 2] = data_start[pos] >> 8;
            msg_respM[i * 2 + 1] = data_start[pos] & 0xFF;
            pos++;
        }
        msg_respM[59] = '\0';

        modbus_recv.last_receive_time[2] = millis();
    }

    uint32_t process_time = micros() - start_time;
    Serial.printf("Additional data processed in %d μs\n", process_time);
}

// Обработка аналогового сигнала
void processAnalogSignal(const uint16_t* data_start, uint16_t data_length) {
    if (data_length >= 1) {
        analog_signalM = data_start[0];
        modbus_recv.last_receive_time[3] = millis();

        Serial.printf("Analog signal: %d\n", analog_signalM);
    }
}

// Задача обработки данных (выполняется на ядре 0)
void modbusProcessingTask(void* parameters) {
    uint32_t last_register_check = 0;
    uint16_t last_command = 0;

    while (true) {
        uint32_t current_time = millis();

        // Проверяем изменения в регистрах каждые 50 мс
        if (current_time - last_register_check >= PROCESSING_INTERVAL) {
            if (xSemaphoreTake(modbus_recv.data_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {

                // Проверяем заголовок команды
                uint16_t command = holdingRegs[0x1000];
                uint16_t object_index = holdingRegs[0x1001];
                uint16_t data_length = holdingRegs[0x1002];

                // Обрабатываем только новые команды
                if (command != 0 && command != last_command && data_length > 0) {
                    switch (command) {
                    case CMD_SEND_CONTAINER:
                        if (object_index < MAX_TRACKING_OBJECTS) {
                            // Исправляем передачу указателя с приведением типа
                            processContainerObject(object_index,
                                (const uint16_t*)&holdingRegs[0x2000 + (object_index * 100)],
                                data_length);
                            modbus_recv.new_data_available[0] = true;
                        }
                        break;

                    case CMD_SEND_THISAIRCRAFT:
                        processThisAircraft((const uint16_t*)&holdingRegs[0x3000], data_length);
                        modbus_recv.new_data_available[1] = true;
                        break;

                    case CMD_SEND_ADDITIONAL:
                        processAdditionalData((const uint16_t*)&holdingRegs[0x4000], data_length);
                        modbus_recv.new_data_available[2] = true;
                        break;

                    case CMD_SEND_ANALOG:
                        processAnalogSignal((const uint16_t*)&holdingRegs[0x5000], data_length);
                        modbus_recv.new_data_available[3] = true;
                        break;
                    }

                    last_command = command;
                    modbus_recv.total_receive_count++;
                }

                xSemaphoreGive(modbus_recv.data_mutex);
            }

            last_register_check = current_time;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Инициализация MODBUS приемника
void initModbusSlave() {
    pinMode(DE_RE_PIN, OUTPUT);
    digitalWrite(DE_RE_PIN, LOW);

    // Настройка UART
    RS485Serial.begin(BAUD_RATE, SERIAL_8N1, RX_PIN, TX_PIN);
    RS485Serial.setRxBufferSize(1024);
    RS485Serial.setTxBufferSize(1024);


    // Инициализация регистров
    memset(holdingRegs, 0, sizeof(holdingRegs));

    // Инициализация SimpleModbusSlave
    // Приведение типа для совместимости с библиотекой
    modbus_configure(RS485Serial, BAUD_RATE, SERIAL_8N1, SLAVE_ID, DE_RE_PIN/*, HOLDING_REGS_SIZE, (unsigned int*)holdingRegs*/);

    // Инициализация структур управления
    modbus_recv.data_mutex = xSemaphoreCreateMutex();
    memset(modbus_recv.new_data_available, false, sizeof(modbus_recv.new_data_available));
    memset(modbus_recv.last_receive_time, 0, sizeof(modbus_recv.last_receive_time));
    modbus_recv.total_receive_count = 0;
    memset(modbus_recv.receive_stats, 0, sizeof(modbus_recv.receive_stats));
    modbus_recv.data_integrity_ok = false;
    modbus_recv.last_register_write = 0;
    modbus_recv.previous_command = 0;

    // Создание задачи обработки на ядре 0
    xTaskCreatePinnedToCore(
        modbusProcessingTask,
        "MODBUS_RX",
        8192,
        NULL,
        8,
        &modbus_recv.processing_task_handle,
        0  // Ядро 0
    );

    Serial.printf("MODBUS Slave initialized on Core 0, baud rate: %d\n", BAUD_RATE);
}

// API функции
bool getContainerObject(uint8_t index, ufo_t* dest) {
    if (index >= MAX_TRACKING_OBJECTS || dest == nullptr) return false;

    if (xSemaphoreTake(modbus_recv.data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        memcpy(dest, &Container[index], sizeof(ufo_t));
        xSemaphoreGive(modbus_recv.data_mutex);
        return true;
    }
    return false;
}

bool getThisAircraft(ufo_t* dest) {
    if (dest == nullptr) return false;

    if (xSemaphoreTake(modbus_recv.data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        memcpy(dest, &ThisAircraft, sizeof(ufo_t));
        xSemaphoreGive(modbus_recv.data_mutex);
        return true;
    }
    return false;
}

bool getAdditionalData(bool* n_flag, uint8_t* btn, bool* gnss, uint8_t* mode, char* msg) 
{
    if (xSemaphoreTake(modbus_recv.data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (n_flag) *n_flag = new_flagM;
        if (btn) *btn = new_buttonM;
        if (gnss) *gnss = isValidGNSSM;
        if (mode) *mode = FLYRF_MODE_TESTM;
        if (msg) strcpy(msg, msg_respM);
        xSemaphoreGive(modbus_recv.data_mutex);
        return true;
    }
    return false;
}

uint16_t getAnalogSignal() {
    return analog_signalM;
}

uint32_t getLastReceiveTime(uint8_t data_type) {
    if (data_type < 4) {
        return modbus_recv.last_receive_time[data_type];
    }
    return 0;
}

uint32_t getTotalReceiveCount() {
    return modbus_recv.total_receive_count;
}

bool isDataIntegrityOK() {
    return modbus_recv.data_integrity_ok;
}

void printReceiveStats() {
    Serial.printf("MODBUS Receive Stats:\n");
    Serial.printf("  Total received: %d\n", getTotalReceiveCount());
    Serial.printf("  Data integrity: %s\n", isDataIntegrityOK() ? "OK" : "ERROR");
    Serial.printf("  Last receive times:\n");
    Serial.printf("    Container: %d ms ago\n", millis() - getLastReceiveTime(0));
    Serial.printf("    ThisAircraft: %d ms ago\n", millis() - getLastReceiveTime(1));
    Serial.printf("    Additional: %d ms ago\n", millis() - getLastReceiveTime(2));
    Serial.printf("    Analog: %d ms ago\n", millis() - getLastReceiveTime(3));
}



//============================================================================


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

   // pinMode(lmic_pins.nss, INPUT);


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

  delay(100);

  hw_info.display = SoC->Display_setup();

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
  Button* btn = new Button(GPIO_NUM_48, false);

  btn->attachPressDownEventCb(&onButtonPressDownCb, NULL);
  btn->attachDoubleClickEventCb(&onButtonDoubleClickEventCb, NULL);
  btn->attachLongPressStartEventCb(onButtonLongPressStartEventCb, NULL);
 
  
  //============================================================================

    // Инициализация данных
  memset(Container, 0, sizeof(Container));
  memset(&ThisAircraft, 0, sizeof(ThisAircraft));
  memset(msg_respM, 0, sizeof(msg_respM));

  // Инициализация MODBUS slave
  initModbusSlave();

  Serial.println("MODBUS Receiver ready - SimpleModbusSlave");

  //============================================================================


 
  SoC->WDT_setup();
}

void loop()
{
    // Обработка MODBUS запросов - исправляем вызов функции
    modbus_update((unsigned int*)holdingRegs);

    // Вывод статистики каждые 5 секунд
    //static unsigned long lastStatus = 0;
    //if (millis() - lastStatus > 5000) {
    //    printReceiveStats();

    //    // Пример получения данных
    //    ufo_t aircraft;
    //    if (getThisAircraft(&aircraft)) {
    //        Serial.printf("Current ThisAircraft: lat=%.6f, lon=%.6f\n",
    //            aircraft.latitude, aircraft.longitude);
    //    }

    //    Serial.printf("Current analog signal: %d\n", getAnalogSignal()); 

    //    lastStatus = millis();
    //}

    //-------------------------------------------------
    esp_task_wdt_reset();
 
    // Show status info on tiny OLED display
    SoC->Display_loop();

    // Handle DNS
    WiFi_loop();

    // Handle Web
    Web_loop();

    // Handle OTA update.
    OTA_loop();

    SoC->loop();

    Time_loop();

    yield();
}

//===================================================================================




//====================================================================================