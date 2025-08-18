#include <stdio.h>                // define I/O functions//#include "MAVLinkRF.h"
#include <Arduino.h>              // define I/O functions
#include "SPI.h"
#include <esp_task_wdt.h>
#include <iostream>
#include <locale.h>
#include <math.h>

#include "OTA.h"
#include "TimeRF.h"
#include "GNSS.h"
#include "RF.h"
#include "EEPROMRF.h"
#include "NMEA.h"
#include "D1090.h"
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
#include "Module1090.h"
#include "Button.h"
#include <ModbusRTU.h>
#include <HardwareSerial.h>

ModbusRTU mb;
HardwareSerial rs485Serial(1);
// Переменные для кнопок
bool lastS1State = false;
bool lastS2State = false;


int set_air = 0;   //  
bool set_test_coordinate = false; // Признак тестовых ввода текущих координат 
bool set_test_coordinate5 = false; // Признак тестовых ввода текущих координат 

#if !defined(SERIAL_FLUSH)
#define SERIAL_FLUSH() Serial.flush()
#endif

#define DEBUG 0
#define DEBUG_TIMING 0

#define isTimeToDisplay() (millis() - LEDTimeMarker     > 1000)
#define isTimeToExport()  (millis() - ExportTimeMarker  > 1000)

ufo_t ThisAircraft;

#define MAX_TRACKING_OBJECTS 8
 
struct ContainerMODBUS {
    uint8_t   raw[34];
    uint32_t  timestamp;
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
    char      flight[16];
    int       vert_rate;
    int       Squawk;
    uint32_t  timemsg;
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
    uint32_t  seen;
    unsigned int pSignal;
    uint8_t   hour_msg;
    uint8_t   min_msg;
    uint16_t  delay_time_msg;
    uint8_t   callsign[8];
    float     test_latitude;
    float     test_longitude;
};

ContainerMODBUS container[MAX_TRACKING_OBJECTS];


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

  //ThisAircraft.aircraft_type = settings->aircraft_type;
 
  //ThisAircraft.protocol = settings->rf_protocol;
  //ThisAircraft.stealth  = settings->stealth;
  //ThisAircraft.no_track = settings->no_track;


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

  //if (psramInit() == false)
  //    Serial.println("PSRAM failed to initialize");
  //else
  //    Serial.println("PSRAM initialized");

  //Serial.printf("PSRAM Size available (bytes): %d\r\n", ESP.getFreePsram());

  heap_caps_malloc_extmem_enable(8000); //Use PSRAM for memory requests larger than 1,000 bytes

  //moduleDump1090.setup();


  // initializing a button
  Button* btn = new Button(GPIO_NUM_48, false);

  btn->attachPressDownEventCb(&onButtonPressDownCb, NULL);
  btn->attachDoubleClickEventCb(&onButtonDoubleClickEventCb, NULL);
  btn->attachLongPressStartEventCb(onButtonLongPressStartEventCb, NULL);

  // Настройка кнопок
  pinMode(BUTTON_S1_PIN, INPUT_PULLUP);
  pinMode(BUTTON_S2_PIN, INPUT_PULLUP);

  // Настройка RS485
  pinMode(DE_RE_PIN, OUTPUT);
  digitalWrite(DE_RE_PIN, LOW);

  // Инициализация последовательного порта
  rs485Serial.begin(SERIAL_RS485_SPEED, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);

  // Настройка Modbus Slave
  mb.begin(&rs485Serial, DE_RE_PIN);
  mb.slave(SLAVE_ID);

  // Инициализация регистров (500 регистров)
  mb.addHreg(0, 0, 500);
 
  SoC->WDT_setup();
}

void loop()
{

    // Обработка Modbus запросов
    mb.task();

    // Обновление состояния кнопок
    updateButtonStates();

    // Проверка и обработка полученных данных
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

 
  if (SoC->UART_ops) {
     SoC->UART_ops->loop();
  }

  Time_loop();

  yield();
}



//===========================================================================
void updateButtonStates()
{
    bool currentS1 = !digitalRead(BUTTON_S1_PIN); // Инвертируем из-за INPUT_PULLUP
    bool currentS2 = !digitalRead(BUTTON_S2_PIN);


    if (currentS1 != lastS1State || currentS2 != lastS2State)
    {
        // Serial.printf("Обновление кнопок\n");
        mb.Hreg(451, currentS1 ? 1 : 0); // Регистр 451 для кнопки S1
        mb.Hreg(452, currentS2 ? 1 : 0); // Регистр 452 для кнопки S2

        //Serial.printf("Обновление кнопок - S1: %s, S2: %s\n",
        //    currentS1 ? "Нажата" : "Отпущена",
        //    currentS2 ? "Нажата" : "Отпущена");

        lastS1State = currentS1;
        lastS2State = currentS2;
    }
}

void processReceivedData() 
{
    static uint32_t lastProcessTime = 0;
    static uint16_t lastAnalogValue = 0;

    if (millis() - lastProcessTime > 1000) 
    { // Обрабатываем раз в 2 секунды
        // Проверяем аналоговое значение
        uint16_t analogValue = mb.Hreg(450);
        if (analogValue != lastAnalogValue) 
        {
            Serial.printf("Получено аналоговое значение: %d\n", analogValue);
            lastAnalogValue = analogValue;
        }

        // Обрабатываем данные контейнера
        parseContainerData();

        lastProcessTime = millis();
    }
}

void parseContainerData() 
{
    for (int objIndex = 0; objIndex < MAX_TRACKING_OBJECTS; objIndex++) 
    {
        uint16_t startRegister = objIndex * 50; // По 50 регистров на объект

        // Собираем данные из регистров
        uint16_t dataBuffer[50];
        bool dataReceived = false;
       // Serial.printf("Объект получен\n");
        for (int i = 0; i < 50; i++) 
        {
            dataBuffer[i] = mb.Hreg(startRegister + i);
            if (dataBuffer[i] != 0) 
            {
                dataReceived = true;
            }
        }

        if (dataReceived) 
        {
            // Конвертируем Modbus данные обратно в структуру
            modbusDataToStruct(dataBuffer, &container[objIndex]);

            Serial.printf("Объект %d получен - Protocol: %d, Addr: 0x%08X, Lat: %.6f, Lon: %.6f, Flight: %s\n",
                objIndex,
                container[objIndex].protocol,
                container[objIndex].addr,
                container[objIndex].latitude,
                container[objIndex].longitude,
                container[objIndex].flight);
        }
    }
}

void modbusDataToStruct(uint16_t* buffer, ContainerMODBUS* cont)
{
    int index = 0;
    Serial.printf("Объект получен\n");
    // raw[8] - 4 регистра (только первые 8 байт)
    for (int i = 0; i < 8; i += 2) 
    {
        cont->raw[i] = (buffer[index] >> 8) & 0xFF;
        if (i + 1 < 8) {
            cont->raw[i + 1] = buffer[index] & 0xFF;
        }
        index++;
    }

    // timestamp - 2 регистра
    cont->timestamp = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
    index += 2;

    // protocol - 1 регистр
    cont->protocol = (uint8_t)buffer[index++];

    // addr - 2 регистра
    cont->addr = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
    index += 2;

    // addr_type - 1 регистр
    cont->addr_type = (uint8_t)buffer[index++];

    // latitude - 2 регистра
    union { float f; uint32_t i; } lat_union;
    lat_union.i = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
    cont->latitude = lat_union.f;
    index += 2;

    // longitude - 2 регистра
    union { float f; uint32_t i; } lon_union;
    lon_union.i = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
    cont->longitude = lon_union.f;
    index += 2;

    // old_latitude - 2 регистра
    union { float f; uint32_t i; } old_lat_union;
    old_lat_union.i = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
    cont->old_latitude = old_lat_union.f;
    index += 2;

    // old_longitude - 2 регистра
    union { float f; uint32_t i; } old_lon_union;
    old_lon_union.i = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
    cont->old_longitude = old_lon_union.f;
    index += 2;

    // altitude - 2 регистра
    union { float f; uint32_t i; } alt_union;
    alt_union.i = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
    cont->altitude = alt_union.f;
    index += 2;

    // pressure_altitude - 2 регистра
    union { float f; uint32_t i; } palt_union;
    palt_union.i = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
    cont->pressure_altitude = palt_union.f;
    index += 2;

    // course - 2 регистра
    union { float f; uint32_t i; } course_union;
    course_union.i = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
    cont->course = course_union.f;
    index += 2;

    // speed - 2 регистра
    union { float f; uint32_t i; } speed_union;
    speed_union.i = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
    cont->speed = speed_union.f;
    index += 2;

    // aircraft_type - 1 регистр
    cont->aircraft_type = (uint8_t)buffer[index++];

    // flight[8] - 4 регистра (только первые 8 символов)
    for (int i = 0; i < 8; i += 2) 
    {
        cont->flight[i] = (buffer[index] >> 8) & 0xFF;
        cont->flight[i + 1] = buffer[index] & 0xFF;
        index++;
    }
    // Добавляем завершающий нуль
    cont->flight[8] = '\0';

    // vert_rate - 2 регистра
    cont->vert_rate = ((int32_t)buffer[index] << 16) | buffer[index + 1];
    index += 2;

    // Squawk - 2 регистра
    cont->Squawk = ((int32_t)buffer[index] << 16) | buffer[index + 1];
    index += 2;

    // stealth и no_track - 1 регистр
    if (index < 50) {
        cont->stealth = (buffer[index] & 0x01) ? true : false;
        cont->no_track = (buffer[index] & 0x02) ? true : false;
        index++;
    }

    // Инициализируем остальные поля
    cont->timemsg = cont->timestamp;
    cont->seen = cont->timestamp;
    cont->vs = 0.0;

    // Очищаем остальные массивы
    for (int i = 8; i < 34; i++) {
        cont->raw[i] = 0;
    }

    for (int i = 8; i < 16; i++) {
        cont->flight[i] = '\0';
    }
}


//==============================================================================================
