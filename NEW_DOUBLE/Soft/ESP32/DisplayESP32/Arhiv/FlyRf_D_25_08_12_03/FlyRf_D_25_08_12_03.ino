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
additional_data_t AdditionalData;
ufo_t fo, Container[MAX_TRACKING_OBJECTS], EmptyFO, fo_msg, Container_msg[MAX_TRACKING_OBJECTS];

ModbusRTU mb;
HardwareSerial rs485Serial(1);


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

void processReceivedData();
void parseThisAircraft();
void parseContainerData();
void modbusDataToUfo(uint16_t* buffer, ufo_t* cont_MODBUS);


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
 
  // Настройка RS485
  pinMode(DE_RE_PIN, OUTPUT);
  digitalWrite(DE_RE_PIN, LOW);

  rs485Serial.begin(BAUD_RATE, SERIAL_8N1, RX_PIN, TX_PIN);

  // Настройка Modbus Slave
  mb.begin(&rs485Serial, DE_RE_PIN);
  mb.slave(SLAVE_ID);

  // Добавление регистров (1000 регистров)
  mb.addHreg(0, 0, 1000);

  Serial.println("Slave ESP32 инициализирован");
  Serial.printf("Slave ID: %d\n", SLAVE_ID);
  Serial.printf("DE/RE Pin: %d\n", DE_RE_PIN);
  Serial.printf("RX Pin: %d, TX Pin: %d\n", RX_PIN, TX_PIN);
  Serial.println("Ожидание подключения Master...");

  SoC->WDT_setup();
}

void loop()
{
    // Обработка Modbus запросов
    mb.task();

    // Обработка полученных данных
    processReceivedData();

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

void processReceivedData() {
    static uint32_t lastProcessTime = 0;
    static uint16_t lastAnalogValue = 0;

    if (millis() - lastProcessTime > 1000) 
    { // Обрабатываем раз в 1 секунды
        // Проверяем аналоговое значение
        uint16_t analogValue = mb.Hreg(999);
        if (analogValue != lastAnalogValue && analogValue > 0) 
        {
           // Serial.printf("Получено аналоговое значение: %d\n", analogValue);
            lastAnalogValue = analogValue;
            service.set_analog_value(analogValue);
        }

        // Обрабатываем дополнительные данные
        parseAdditionalData();

        // Обрабатываем ThisAircraft
        parseThisAircraft();

        // Обрабатываем Container объекты
        parseContainerData();

        lastProcessTime = millis();
    }
}


void parseAdditionalData() {
    // Дополнительные данные находятся в регистрах 950-999 (50 регистров = 2 блока по 25)
    uint16_t dataBuffer[50];
    bool dataReceived = false;
    uint32_t checksum = 0;

    // Проверяем наличие данных в обоих блоках
    for (int i = 0; i < 50; i++) {
        dataBuffer[i] = mb.Hreg(950 + i);
        checksum += dataBuffer[i];
        if (dataBuffer[i] != 0) {
            dataReceived = true;
        }
    }

    static uint32_t lastChecksum = 0;

    if (dataReceived && checksum != lastChecksum) 
    {
        modbusDataToAdditional(dataBuffer, &AdditionalData);
        //Serial.println("\n=== Дополнительные данные получены (блоками по 25) ===");
        //Serial.printf("Блок 1: регистры 950-974\n");
        //Serial.printf("Блок 2: регистры 975-999\n");
        //Serial.printf("new_flag: %s\n", AdditionalData.new_flag ? "true" : "false");
        //Serial.printf("new_button: %d\n", AdditionalData.new_button);
        //Serial.printf("setMessageRead: %s\n", AdditionalData.setMessageRead ? "true" : "false");
        //Serial.printf("MessageRead: %s\n", AdditionalData.MessageRead ? "true" : "false");
        //Serial.printf("SOS_Sprite_on_off: %s\n", AdditionalData.SOS_Sprite_on_off ? "true" : "false");
        //Serial.printf("SOS_View_on_off: %s\n", AdditionalData.SOS_View_on_off ? "true" : "false");
        //Serial.printf("new_SOS_flag: %s\n", AdditionalData.new_SOS_flag ? "true" : "false");
        //Serial.printf("confirm_message: %s\n", AdditionalData.confirm_message ? "true" : "false");
        //Serial.printf("msg_resp: %s\n", AdditionalData.msg_resp);
        //Serial.printf("isValidGNSS: %s\n", AdditionalData.isValidGNSS ? "true" : "false");
        //Serial.printf("FLYRF_MODE_TEST: %d\n", AdditionalData.FLYRF_MODE_TEST);
        //Serial.printf("Checksum: %u\n", checksum);
        //Serial.println("=======================================================\n");

        lastChecksum = checksum;
    }
}

void parseThisAircraft() {
    // ThisAircraft находится в регистрах 900-949 (50 регистров = 2 блока по 25)
    uint16_t dataBuffer[50];
    bool dataReceived = false;
    uint32_t checksum = 0;

    // Проверяем наличие данных в обоих блоках
    for (int i = 0; i < 50; i++) {
        dataBuffer[i] = mb.Hreg(900 + i);
        checksum += dataBuffer[i];
        if (dataBuffer[i] != 0) {
            dataReceived = true;
        }
    }

    static uint32_t lastThisAircraftChecksum = 0;

    if (dataReceived && checksum != lastThisAircraftChecksum) 
    {
        modbusDataToUfo(dataBuffer, &ThisAircraft);
        //Serial.printf("ThisAircraft получен (блоками по 25) - Addr: 0x%08X, Lat: %.6f, Lon: %.6f, Flight: %s, Alt: %.1f\n",
        //    ThisAircraft.addr,
        //    ThisAircraft.latitude,
        //    ThisAircraft.longitude,
        //    ThisAircraft.flight,
        //    ThisAircraft.altitude);
        //Serial.printf("  Блок 1: регистры 900-924, Блок 2: регистры 925-949\n");

        lastThisAircraftChecksum = checksum;
    }
}

void parseContainerData() {
    static int lastParsedObject = -1;
    static uint32_t lastContainerChecksum[MAX_TRACKING_OBJECTS] = { 0 };

    for (int objIndex = 0; objIndex < MAX_TRACKING_OBJECTS; objIndex++) {
        uint16_t startRegister = objIndex * 50; // По 50 регистров на объект (2 блока по 25)

        uint16_t dataBuffer[50];
        bool dataReceived = false;
        uint32_t checksum = 0;

        // Проверяем наличие данных в обоих блоках объекта
        for (int i = 0; i < 50; i++) {
            dataBuffer[i] = mb.Hreg(startRegister + i);
            checksum += dataBuffer[i];
            if (dataBuffer[i] != 0) {
                dataReceived = true;
            }
        }

        if (dataReceived && checksum != lastContainerChecksum[objIndex]) {
            modbusDataToUfo(dataBuffer, &Container[objIndex]);
            //Serial.printf("Container[%d] получен (блоками по 25) - Addr: 0x%08X, Lat: %.6f, Lon: %.6f, Flight: %s, Alt: %.1f\n",
            //    objIndex,
            //    Container[objIndex].addr,
            //    Container[objIndex].latitude,
            //    Container[objIndex].longitude,
            //    Container[objIndex].flight,
            //    Container[objIndex].altitude);
            //Serial.printf("  Блок 1: регистры %d-%d, Блок 2: регистры %d-%d\n",
            //    startRegister, startRegister + 24,
            //    startRegister + 25, startRegister + 49);

            lastContainerChecksum[objIndex] = checksum;
        }
    }
}

void modbusDataToAdditional(uint16_t* buffer, additional_data_t* data) {
    int index = 0;

    // Распаковываем boolean значения из первого регистра
    uint16_t flags = buffer[index++];
    data->new_flag = (flags & 0x0001) != 0;
    data->setMessageRead = (flags & 0x0002) != 0;
    data->MessageRead = (flags & 0x0004) != 0;
    data->SOS_Sprite_on_off = (flags & 0x0008) != 0;
    data->SOS_View_on_off = (flags & 0x0010) != 0;
    data->new_SOS_flag = (flags & 0x0020) != 0;
    data->confirm_message = (flags & 0x0040) != 0;
    data->isValidGNSS = (flags & 0x0080) != 0;

    // new_button - 1 регистр
    data->new_button = (uint8_t)buffer[index++];

    // FLYRF_MODE_TEST - 1 регистр
    data->FLYRF_MODE_TEST = (uint8_t)buffer[index++];

    // msg_resp[60] - 30 регистров
    for (int i = 0; i < 60; i += 2) {
        data->msg_resp[i] = (buffer[index] >> 8) & 0xFF;
        data->msg_resp[i + 1] = buffer[index] & 0xFF;
        index++;
    }

    // Убеждаемся, что строка завершается нулем
    data->msg_resp[59] = '\0';
}

void modbusDataToUfo(uint16_t* buffer, ufo_t* ufo) {
    int index = 0;

    // timestamp - 2 регистра
    ufo->timestamp = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
    index += 2;

    // addr - 2 регистра
    ufo->addr = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
    index += 2;

    // addr_type - 1 регистр
    ufo->addr_type = (uint8_t)buffer[index++];

    // latitude - 2 регистра
    union { float f; uint32_t i; } lat_union;
    lat_union.i = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
    ufo->latitude = lat_union.f;
    index += 2;

    // longitude - 2 регистра
    union { float f; uint32_t i; } lon_union;
    lon_union.i = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
    ufo->longitude = lon_union.f;
    index += 2;

    // old_latitude - 2 регистра
    union { float f; uint32_t i; } old_lat_union;
    old_lat_union.i = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
    ufo->old_latitude = old_lat_union.f;
    index += 2;

    // old_longitude - 2 регистра
    union { float f; uint32_t i; } old_lon_union;
    old_lon_union.i = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
    ufo->old_longitude = old_lon_union.f;
    index += 2;

    // altitude - 2 регистра
    union { float f; uint32_t i; } alt_union;
    alt_union.i = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
    ufo->altitude = alt_union.f;
    index += 2;

    // pressure_altitude - 2 регистра
    union { float f; uint32_t i; } palt_union;
    palt_union.i = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
    ufo->pressure_altitude = palt_union.f;
    index += 2;

    // course - 2 регистра
    union { float f; uint32_t i; } course_union;
    course_union.i = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
    ufo->course = course_union.f;
    index += 2;

    // speed - 2 регистра
    union { float f; uint32_t i; } speed_union;
    speed_union.i = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
    ufo->speed = speed_union.f;
    index += 2;

    // aircraft_type - 1 регистр
    ufo->aircraft_type = (uint8_t)buffer[index++];

    // flight[16] - 8 регистров
    for (int i = 0; i < 16; i += 2) {
        ufo->flight[i] = (buffer[index] >> 8) & 0xFF;
        ufo->flight[i + 1] = buffer[index] & 0xFF;
        index++;
    }
    // Убеждаемся, что строка завершается нулем
    ufo->flight[15] = '\0';

    // vert_rate - 2 регистра
    ufo->vert_rate = ((int32_t)buffer[index] << 16) | buffer[index + 1];
    index += 2;

    // Squawk - 2 регистра
    ufo->Squawk = ((int32_t)buffer[index] << 16) | buffer[index + 1];
    index += 2;

    // timemsg - 2 регистра
    ufo->timemsg = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
    index += 2;

    // vs - 2 регистра
    union { float f; uint32_t i; } vs_union;
    vs_union.i = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
    ufo->vs = vs_union.f;
    index += 2;

    // Инициализируем остальные поля
    ufo->geoid_separation = 0.0;
    ufo->hdop = 0;
    ufo->rssi = 0;
    ufo->distance = 0.0;
    ufo->bearing = 0.0;
    ufo->signal_source = 0;
    ufo->seen = ufo->timestamp;
    ufo->hour_msg = (ufo->timestamp / 3600) % 24;
    ufo->min_msg = (ufo->timestamp / 60) % 60;
    ufo->delay_time_msg = 0;

    for (int i = 0; i < 8; i++) {
        ufo->callsign[i] = 0;
    }
}

// Функция для отображения подробной статистики
void printDetailedStatistics() 
{
    static uint32_t lastStatsTime = 0;

    if (millis() - lastStatsTime > 15000) { // Каждые 15 секунд
        Serial.println("\n=============== ПОДРОБНАЯ СТАТИСТИКА ===============");

        // Аналоговое значение
        uint16_t analogValue = mb.Hreg(999);
        Serial.printf("Аналоговое значение: %d\n", analogValue);

        // Дополнительные данные
        Serial.println("\n--- Дополнительные данные ---");
        Serial.printf("new_flag: %s, new_button: %d, FLYRF_MODE_TEST: %d\n",
            AdditionalData.new_flag ? "true" : "false",
            AdditionalData.new_button,
            AdditionalData.FLYRF_MODE_TEST);
        Serial.printf("isValidGNSS: %s, SOS активен: %s\n",
            AdditionalData.isValidGNSS ? "true" : "false",
            AdditionalData.SOS_Sprite_on_off ? "true" : "false");

        // ThisAircraft
        if (ThisAircraft.addr != 0) {
            Serial.println("\n--- ThisAircraft ---");
            Serial.printf("Addr: 0x%08X, Type: %d\n", ThisAircraft.addr, ThisAircraft.aircraft_type);
            Serial.printf("Позиция: %.6f, %.6f, Alt: %.1f м\n",
                ThisAircraft.latitude, ThisAircraft.longitude, ThisAircraft.altitude);
            Serial.printf("Курс: %.1f°, Скорость: %.1f узл, Вертик.скорость: %d фт/мин\n",
                ThisAircraft.course, ThisAircraft.speed, ThisAircraft.vert_rate);
            Serial.printf("Рейс: %s, Squawk: %d\n", ThisAircraft.flight, ThisAircraft.Squawk);
        }

        // Container объекты
        Serial.println("\n--- Container объекты ---");
        int validObjects = 0;
        for (int i = 0; i < MAX_TRACKING_OBJECTS; i++) {
            if (Container[i].addr != 0) {
                validObjects++;
                Serial.printf("Container[%d]: Addr=0x%08X, Pos=(%.6f,%.6f), Alt=%.1f, Flight=%s\n",
                    i, Container[i].addr, Container[i].latitude, Container[i].longitude,
                    Container[i].altitude, Container[i].flight);
            }
        }

        Serial.printf("\nВсего активных объектов: %d из %d\n", validObjects, MAX_TRACKING_OBJECTS);
        Serial.printf("Блоков по %d регистров обработано успешно\n", BLOCK_SIZE);
        Serial.println("====================================================\n");

        lastStatsTime = millis();
    }
}

/*
Основные изменения :
Размер блока увеличен до 25 регистров(#define BLOCK_SIZE 25)
Каждые 50 регистров передаются 2 блоками по 25 регистров каждый
Увеличены задержки между блоками до 100мс для стабильности
Добавлены контрольные суммы в приемнике для отслеживания изменений
Детальная диагностика с указанием диапазонов регистров для каждого блока
Улучшенная статистика в приемнике
Структура передачи блоками :
Container объекты(0 - 399) :

    Container[0] : Блок 1 (0 - 24), Блок 2 (25 - 49)
    Container[1] : Блок 1 (50 - 74), Блок 2 (75 - 99)
    ...и так далее
    ThisAircraft(900 - 949) :

    Блок 1 : регистры 900 - 924
    Блок 2 : регистры 925 - 949
    Дополнительные данные(950 - 999) :

    Блок 1 : регистры 950 - 974
    Блок 2 : регистры 975 - 999
    Это обеспечивает более эффективную передачу данных и лучшую совместимость с различными Modbus устройствами.

    */


