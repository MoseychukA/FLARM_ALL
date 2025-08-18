#include <stdio.h>                // define I/O functions
#include <Arduino.h>              // define I/O functions
#include "SPI.h"
#include <esp_task_wdt.h>
#include <iostream>
#include <locale.h>
#include <math.h>

#include "OTA.h"
#include "TimeRF.h"
#include "RF.h"
#include "EEPROMRF.h"
#include "NMEA.h"
#include "D1090.h"
#include "SoC.h"
#include "WiFiRF.h"
#include "WebRF.h"
#include "TrafficHelper.h"
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

ModbusRTU mb;
HardwareSerial rs485Serial(1);


hardware_info_t hw_info = {
  .model    = DEFAULT_FLYRF_MODEL,
  .revision = 0,
  .soc      = SOC_NONE,
  .rf       = RF_IC_NONE,
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

 /* SoC->Button_setup();*/

  ThisAircraft.addr = SoC->getChipId() & 0x00FFFFFF;

  hw_info.rf = RF_setup();

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



  Traffic_setup();

  SoC->swSer_enableRx(false);

  WiFi_setup();
 
  OTA_setup();
  Web_setup();
  NMEA_setup();

  delay(1000);

  
  SoC->post_init();

 
  // initializing a button
  Button* btn = new Button(GPIO_NUM_48, false);

  btn->attachPressDownEventCb(&onButtonPressDownCb, NULL);
  btn->attachDoubleClickEventCb(&onButtonDoubleClickEventCb, NULL);
  btn->attachLongPressStartEventCb(onButtonLongPressStartEventCb, NULL);
 
  // Настройка RS485
  pinMode(DE_RE_PIN, OUTPUT);
  digitalWrite(DE_RE_PIN, LOW);

  rs485Serial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);

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


  //if (SoC->UART_ops) {
  //   SoC->UART_ops->loop();
  //}


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
            Serial.printf("Получено аналоговое значение: %d\n", analogValue);
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

void parseAdditionalData() 
{
    // Дополнительные данные находятся в регистрах 950-999
    uint16_t dataBuffer[50];
    bool dataReceived = false;

    for (int i = 0; i < 50; i++) {
        dataBuffer[i] = mb.Hreg(950 + i);
        if (dataBuffer[i] != 0) {
            dataReceived = true;
        }
    }

    if (dataReceived) 
    {
        modbusDataToAdditional(dataBuffer, &AdditionalData);
        Serial.println("=== Дополнительные данные получены ===");
        Serial.printf("new_flag: %s\n", AdditionalData.new_flag ? "true" : "false");
        Serial.printf("new_button: %d\n", AdditionalData.new_button);
        Serial.printf("setMessageRead: %s\n", AdditionalData.setMessageRead ? "true" : "false");
        Serial.printf("MessageRead: %s\n", AdditionalData.MessageRead ? "true" : "false");
        Serial.printf("SOS_Sprite_on_off: %s\n", AdditionalData.SOS_Sprite_on_off ? "true" : "false");
        Serial.printf("SOS_View_on_off: %s\n", AdditionalData.SOS_View_on_off ? "true" : "false");
        Serial.printf("new_SOS_flag: %s\n", AdditionalData.new_SOS_flag ? "true" : "false");
        Serial.printf("confirm_message: %s\n", AdditionalData.confirm_message ? "true" : "false");
        Serial.printf("msg_resp: %s\n", AdditionalData.msg_resp);
        Serial.printf("isValidGNSS: %s\n", AdditionalData.isValidGNSS ? "true" : "false");
        Serial.printf("FLYRF_MODE_TEST: %d\n", AdditionalData.FLYRF_MODE_TEST);
        Serial.println("=====================================");
    }
}

void parseThisAircraft() {
    // ThisAircraft находится в регистрах 900-949
    uint16_t dataBuffer[50];
    bool dataReceived = false;

    for (int i = 0; i < 50; i++) {
        dataBuffer[i] = mb.Hreg(900 + i);
        if (dataBuffer[i] != 0) {
            dataReceived = true;
        }
    }

    if (dataReceived) {
        modbusDataToUfo(dataBuffer, &ThisAircraft);
        Serial.printf("ThisAircraft получен - Addr: 0x%08X, Lat: %.6f, Lon: %.6f, Flight: %s\n",
            ThisAircraft.addr,
            ThisAircraft.latitude,
            ThisAircraft.longitude,
            ThisAircraft.flight);
    }
}

void parseContainerData() {
    static int lastParsedObject = -1;

    for (int objIndex = 0; objIndex < MAX_TRACKING_OBJECTS; objIndex++) {
        uint16_t startRegister = objIndex * 50; // По 50 регистров на объект

        uint16_t dataBuffer[50];
        bool dataReceived = false;

        for (int i = 0; i < 50; i++) {
            dataBuffer[i] = mb.Hreg(startRegister + i);
            if (dataBuffer[i] != 0) {
                dataReceived = true;
            }
        }

        if (dataReceived && objIndex != lastParsedObject) {
            modbusDataToUfo(dataBuffer, &Container[objIndex]);
            Serial.printf("Container[%d] получен - Addr: 0x%08X, Lat: %.6f, Lon: %.6f, Flight: %s, Alt: %.1f\n",
                objIndex,
                Container[objIndex].addr,
                Container[objIndex].latitude,
                Container[objIndex].longitude,
                Container[objIndex].flight,
                Container[objIndex].altitude);
            lastParsedObject = objIndex;
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

void modbusDataToUfo(uint16_t* buffer, ufo_t* cont_MODBUS) 
{
    int index = 0;

    // timestamp - 2 регистра
    cont_MODBUS->timestamp = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
    index += 2;

    // addr - 2 регистра
    cont_MODBUS->addr = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
    index += 2;

    // addr_type - 1 регистр
    cont_MODBUS->addr_type = (uint8_t)buffer[index++];

    // latitude - 2 регистра
    union { float f; uint32_t i; } lat_union;
    lat_union.i = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
    cont_MODBUS->latitude = lat_union.f;
    index += 2;

    // longitude - 2 регистра
    union { float f; uint32_t i; } lon_union;
    lon_union.i = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
    cont_MODBUS->longitude = lon_union.f;
    index += 2;

    // old_latitude - 2 регистра
    union { float f; uint32_t i; } old_lat_union;
    old_lat_union.i = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
    cont_MODBUS->old_latitude = old_lat_union.f;
    index += 2;

    // old_longitude - 2 регистра
    union { float f; uint32_t i; } old_lon_union;
    old_lon_union.i = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
    cont_MODBUS->old_longitude = old_lon_union.f;
    index += 2;

    // altitude - 2 регистра
    union { float f; uint32_t i; } alt_union;
    alt_union.i = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
    cont_MODBUS->altitude = alt_union.f;
    index += 2;

    // pressure_altitude - 2 регистра
    union { float f; uint32_t i; } palt_union;
    palt_union.i = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
    cont_MODBUS->pressure_altitude = palt_union.f;
    index += 2;

    // course - 2 регистра
    union { float f; uint32_t i; } course_union;
    course_union.i = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
    cont_MODBUS->course = course_union.f;
    index += 2;

    // speed - 2 регистра
    union { float f; uint32_t i; } speed_union;
    speed_union.i = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
    cont_MODBUS->speed = speed_union.f;
    index += 2;

    // aircraft_type - 1 регистр
    cont_MODBUS->aircraft_type = (uint8_t)buffer[index++];

    // flight[16] - 8 регистров
    for (int i = 0; i < 16; i += 2) 
    {
        cont_MODBUS->flight[i] = (buffer[index] >> 8) & 0xFF;
        cont_MODBUS->flight[i + 1] = buffer[index] & 0xFF;
        index++;
    }
    // Убеждаемся, что строка завершается нулем
    cont_MODBUS->flight[15] = '\0';

    // vert_rate - 2 регистра
    cont_MODBUS->vert_rate = ((int32_t)buffer[index] << 16) | buffer[index + 1];
    index += 2;

    // Squawk - 2 регистра
    cont_MODBUS->Squawk = ((int32_t)buffer[index] << 16) | buffer[index + 1];
    index += 2;

    // Инициализируем остальные поля
    cont_MODBUS->timemsg = cont_MODBUS->timestamp;
    cont_MODBUS->vs = 0.0;
    cont_MODBUS->geoid_separation = 0.0;
    cont_MODBUS->hdop = 0;
    cont_MODBUS->rssi = 0;
    cont_MODBUS->distance = 0.0;
    cont_MODBUS->bearing = 0.0;
    cont_MODBUS->signal_source = 0;
    cont_MODBUS->seen = cont_MODBUS->timestamp;
    cont_MODBUS->hour_msg = (cont_MODBUS->timestamp / 3600) % 24;
    cont_MODBUS->min_msg = (cont_MODBUS->timestamp / 60) % 60;
    cont_MODBUS->delay_time_msg = 0;

    for (int i = 0; i < 8; i++) 
    {
        cont_MODBUS->callsign[i] = 0;
    }
}


/*
Обновленная карта регистров :
0 - 49 : Container[0]
50 - 99 : Container[1]
100 - 149 : Container[2]
150 - 199 : Container[3]
200 - 249 : Container[4]
250 - 299 : Container[5]
300 - 349 : Container[6]
350 - 399 : Container[7]
900 - 949 : ThisAircraft
950 - 999 : Дополнительные данные
950 : Упакованные флаги(new_flag, setMessageRead, MessageRead, SOS_Sprite_on_off, SOS_View_on_off, new_SOS_flag, confirm_message, isValidGNSS)
951 : new_button
952 : FLYRF_MODE_TEST
953 - 982 : msg_resp[60](30 регистров)
999 : Аналоговое значение
Теперь система передает все необходимые дополнительные данные от источника к приемнику.
*/



