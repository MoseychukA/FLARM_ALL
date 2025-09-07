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
//
//// Данные для приема
//ufo_t Container[MAX_TRACKING_OBJECTS];
//ufo_t ThisAircraft;

// Дополнительные данные
bool new_flag_M;
uint8_t new_button_M;
bool setMessageRead_M;
bool MessageRead_M;
bool SOS_Sprite_on_off_M;
bool SOS_View_on_off_M;
bool new_SOS_flag_M;
bool confirm_message_M;
char msg_resp_M[60];
bool isValidGNSS_M;
uint8_t FLYRF_MODE_TEST_M;
uint16_t analog_signal_M;

// MODBUS объекты
ModbusRTU mb;
HardwareSerial RS485Serial(1);

// Массивы регистров MODBUS
uint16_t holdingRegs[10000];
uint16_t inputRegs[1000];

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
    bool register_changed;
} modbus_receive_t;

modbus_receive_t modbus_recv;

// Функция безопасного получения адреса из TAddress
uint16_t getAddressValue(TAddress addr) {
    // TAddress может быть различного типа в зависимости от версии библиотеки
    // Приводим к uint16_t через memcpy для безопасности
    uint16_t result = 0;
    if (sizeof(TAddress) == sizeof(uint16_t)) {
        memcpy(&result, &addr, sizeof(uint16_t));
    }
    else if (sizeof(TAddress) == sizeof(uint32_t)) {
        uint32_t temp;
        memcpy(&temp, &addr, sizeof(uint32_t));
        result = (uint16_t)(temp & 0xFFFF);
    }
    else {
        // Попытка прямого приведения для других случаев
        result = *((uint16_t*)&addr);
    }
    return result;
}

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
        if (pos < 80) {
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

    if (processed <= data_length) 
    {
        modbus_recv.last_receive_time[0] = millis();
        modbus_recv.data_integrity_ok = true;

        Serial.printf("Container[%d]: lat=%.6f, lon=%.6f, alt=%.1f, addr=0x%08X\n",
            index, Container[index].latitude, Container[index].longitude,
            Container[index].altitude, Container[index].addr);
    }
    else 
    {
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

    if (processed <= data_length) 
    {
        modbus_recv.last_receive_time[1] = millis();
        modbus_recv.data_integrity_ok = true;

        Serial.printf("ThisAircraft: lat=%.6f, lon=%.6f, alt=%.1f, speed=%.1f\n",
            ThisAircraft.latitude, ThisAircraft.longitude,
            ThisAircraft.altitude, ThisAircraft.speed);
    }
    else 
    {
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

    if (data_length >= 10) {
        new_flag_M = data_start[pos++] != 0;
        new_button_M = data_start[pos++];
        setMessageRead_M = data_start[pos++] != 0;
        MessageRead_M = data_start[pos++] != 0;
        SOS_Sprite_on_off_M = data_start[pos++] != 0;
        SOS_View_on_off_M = data_start[pos++] != 0;
        new_SOS_flag_M = data_start[pos++] != 0;
        confirm_message_M = data_start[pos++] != 0;
        isValidGNSS_M = data_start[pos++] != 0;
        FLYRF_MODE_TEST_M = data_start[pos++];

        // msg_resp (30 регистров = 60 символов)
        for (int i = 0; i < 30 && pos < data_length; i++) {
            msg_resp_M[i * 2] = data_start[pos] >> 8;
            msg_resp_M[i * 2 + 1] = data_start[pos] & 0xFF;
            pos++;
        }
        msg_resp_M[59] = '\0';

        modbus_recv.last_receive_time[2] = millis();
    }

    uint32_t process_time = micros() - start_time;

    Serial.printf("Additional data: flag=%d, btn=%d, GNSS=%d, mode=%d (processed in %d μs)\n",
        new_flag_M, new_button_M, isValidGNSS_M, FLYRF_MODE_TEST_M, process_time);

    if (strlen(msg_resp_M) > 0) {
        Serial.printf("Message: %.20s...\n", msg_resp_M);
    }
}

// Обработка аналогового сигнала
void processAnalogSignal(const uint16_t* data_start, uint16_t data_length) 
{
    if (data_length >= 1) 
    {
        analog_signal_M = data_start[0];
        modbus_recv.last_receive_time[3] = millis();

        Serial.printf("****Analog signal: %d\n", analog_signal_M);
    }
}

// Исправленные callback функции для ModbusRTU
uint16_t cbWriteHolding(TRegister* reg, uint16_t val) {
    uint16_t address = getAddressValue(reg->address);
    if (address < 10000) {
        holdingRegs[address] = val;
        modbus_recv.register_changed = true;
        modbus_recv.last_register_write = millis();
        return val;
    }
    return 0;
}

uint16_t cbReadHolding(TRegister* reg, uint16_t val) {
    uint16_t address = getAddressValue(reg->address);
    if (address < 10000) {
        return holdingRegs[address];
    }
    return 0;
}

uint16_t cbReadInput(TRegister* reg, uint16_t val) {
    uint16_t address = getAddressValue(reg->address);
    if (address < 1000) {
        return inputRegs[address];
    }
    return 0;
}

// Задача обработки данных (выполняется на ядре 0)
void modbusProcessingTask(void* parameters) {
    uint32_t last_register_check = 0;
    uint16_t last_command = 0;

    while (true) {
        uint32_t current_time = millis();

        // Проверяем изменения в регистрах при наличии флага изменений
        if (modbus_recv.register_changed &&
            (current_time - last_register_check >= PROCESSING_INTERVAL)) {

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
                            processContainerObject(object_index,
                                &holdingRegs[0x2000 + (object_index * 100)],
                                data_length);
                            modbus_recv.new_data_available[0] = true;
                        }
                        break;

                    case CMD_SEND_THISAIRCRAFT:
                        processThisAircraft(&holdingRegs[0x3000], data_length);
                        modbus_recv.new_data_available[1] = true;
                        break;

                    case CMD_SEND_ADDITIONAL:
                        processAdditionalData(&holdingRegs[0x4000], data_length);
                        modbus_recv.new_data_available[2] = true;
                        break;

                    case CMD_SEND_ANALOG:
                        processAnalogSignal(&holdingRegs[0x5000], data_length);
                        modbus_recv.new_data_available[3] = true;
                        break;

                    default:
                        Serial.printf("Unknown command: %d\n", command);
                        break;
                    }

                    last_command = command;
                    modbus_recv.total_receive_count++;

                    // Обновление статистики
                    static uint8_t stat_index = 0;
                    modbus_recv.receive_stats[stat_index] = current_time - modbus_recv.last_register_write;
                    stat_index = (stat_index + 1) % 5;
                }

                modbus_recv.register_changed = false;
                xSemaphoreGive(modbus_recv.data_mutex);
            }

            last_register_check = current_time;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Инициализация MODBUS приемника
void initModbusSlave() 
{
    pinMode(DE_RE_PIN, OUTPUT);
    digitalWrite(DE_RE_PIN, LOW);

    // Настройка UART с высокой скоростью
    RS485Serial.begin(BAUD_RATE, SERIAL_8N1, RX_PIN, TX_PIN);
    RS485Serial.setRxBufferSize(2048);
    RS485Serial.setTxBufferSize(1024);

    // Инициализация ModbusRTU
    mb.begin(&RS485Serial, DE_RE_PIN);
    mb.slave(SLAVE_ID);

    // Инициализация регистров
    memset(holdingRegs, 0, sizeof(holdingRegs));
    memset(inputRegs, 0, sizeof(inputRegs));

    // Добавление диапазонов регистров
    mb.addHreg(0x1000, 0, 3000);      // Заголовки команд и данные
    mb.addHreg(0x2000, 0, 800);       // Container data (8 objects * 100 registers)
    mb.addHreg(0x3000, 0, 100);       // ThisAircraft data
    mb.addHreg(0x4000, 0, 50);        // Additional data
    mb.addHreg(0x5000, 0, 10);        // Analog signal

    mb.addIreg(0x0000, 0, 1000);      // Input registers

    // Установка callback функций с правильными сигнатурами
    mb.onSetHreg(0x1000, cbWriteHolding, 9000);
    mb.onGetHreg(0x1000, cbReadHolding, 9000);
    mb.onGetIreg(0x0000, cbReadInput, 1000);

    // Инициализация структур управления
    modbus_recv.data_mutex = xSemaphoreCreateMutex();
    memset(modbus_recv.new_data_available, false, sizeof(modbus_recv.new_data_available));
    memset(modbus_recv.last_receive_time, 0, sizeof(modbus_recv.last_receive_time));
    modbus_recv.total_receive_count = 0;
    memset(modbus_recv.receive_stats, 0, sizeof(modbus_recv.receive_stats));
    modbus_recv.data_integrity_ok = false;
    modbus_recv.last_register_write = 0;
    modbus_recv.previous_command = 0;
    modbus_recv.register_changed = false;

    // Создание задачи обработки на ядре 0
    xTaskCreatePinnedToCore(
        modbusProcessingTask,
        "MODBUS_RX",
        8192,
        NULL,
        8,  // Высокий приоритет
        &modbus_recv.processing_task_handle,
        0  // Ядро 0
    );

    Serial.printf("MODBUS Slave initialized on Core 0, baud rate: %d\n", BAUD_RATE);
    Serial.printf("Max block size: %d registers, processing interval: %d ms\n",
        MAX_MODBUS_BLOCK, PROCESSING_INTERVAL);

    // Отладочная информация о TAddress
    Serial.printf("TAddress size: %d bytes\n", sizeof(TAddress));
}

// API функции для других задач
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

bool getAdditionalData(bool* n_flag, uint8_t* btn, bool* gnss, uint8_t* mode, char* msg) {
    if (xSemaphoreTake(modbus_recv.data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (n_flag) *n_flag = new_flag_M;
        if (btn) *btn = new_button_M;
        if (gnss) *gnss = isValidGNSS_M;
        if (mode) *mode = FLYRF_MODE_TEST_M;
        if (msg) strcpy(msg, msg_resp_M);
        xSemaphoreGive(modbus_recv.data_mutex);
        return true;
    }
    return false;
}

uint16_t getAnalogSignal() 
{
    return analog_signal_M;
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

uint32_t getAverageProcessTime() {
    uint32_t total = 0;
    for (int i = 0; i < 5; i++) {
        total += modbus_recv.receive_stats[i];
    }
    return total / 5;
}

bool isDataIntegrityOK() 
{
    return modbus_recv.data_integrity_ok;
}

void printReceiveStats() 
{
    Serial.printf("MODBUS Receive Stats:\n");
    Serial.printf("  Total received: %d\n", getTotalReceiveCount());
    Serial.printf("  Data integrity: %s\n", isDataIntegrityOK() ? "OK" : "ERROR");
    Serial.printf("  Avg process time: %d ms\n", getAverageProcessTime());
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
    memset(msg_resp_M, 0, sizeof(msg_resp_M));
    
    // Инициализация MODBUS slave
    initModbusSlave();
    
    Serial.println("MODBUS Receiver ready - ModbusRTU library");

  //============================================================================


 
  SoC->WDT_setup();
}

void loop()
{
    // Обработка MODBUS запросов
    mb.task();
    
    // Вывод статистики каждые 5 секунд
    static unsigned long lastStatus = 0;
    if (millis() - lastStatus > 5000) 
    {
        printReceiveStats();
        
        // Пример получения данных
        ufo_t aircraft;
        if (getThisAircraft(&aircraft)) 
        {
            Serial.printf("Current ThisAircraft: lat=%.6f, lon=%.6f, alt=%.1f\n", aircraft.latitude, aircraft.longitude, aircraft.altitude);
        }
        
        Serial.printf("Current analog signal: %d\n", getAnalogSignal());
        
        lastStatus = millis();
    }

    //============================================================
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