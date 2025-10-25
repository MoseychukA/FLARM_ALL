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

#define RS485_SERIAL         Serial1
#define RS485_TX_PIN         39
#define RS485_RX_PIN         38
#define RS485_DE_PIN         40
#define RS485_BAUD           921600
#define RS485_CONFIG         SERIAL_8N1
#define PREAMBLE             0x55AAu
#define PROTO_VER            0x01
#define PACKET_CMD           0x01
#define PACKET_BTN           0x82
#define MAX_PAYLOAD          512
#define MAX_TRACKING_OBJECTS 12



Adafruit_INA219 ina219;

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


//============================================================================

//typedef struct UFO {
//    uint32_t  addr;
//    int       squawk;
//    uint8_t   callsign[8];
//    float     latitude;
//    float     longitude;
//    float     altitude;
//    float     pressure_altitude;
//    float     course;
//    float     speed;
//    int       vert_rate;
//    float     latitude2;
//    float     longitude2;
//    int8_t    rssi;
//    uint16_t  last_message_signal_strength_dbm;
//    uint16_t  last_message_signal_quality_db;
//} ufo_t;

typedef struct {
    bool     new_flag_M;
    uint8_t  new_buttton_M;
    bool     setMessageRead_M;
    bool     MessageRead_M;
    uint8_t  Time_Hour_M;
    uint8_t  Time_Minute_M;
    bool     new_SOS_flag_M;
    char     msg_resp_M[170];
    bool     isValidGNSS_M;
} aux_t;

//ufo_t Container[MAX_TRACKING_OBJECTS];
//ufo_t ThisAircraft;
aux_t aux;

SemaphoreHandle_t serialMutex;
SemaphoreHandle_t containerMutex;

// Кнопки
volatile uint8_t button1_state = 0;
volatile uint8_t button2_state = 0;

// Флаг наличия связи от источника
volatile bool isDataFromA = false;

// CRC16-IBM
uint16_t crc16_ibm(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++)
            crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : (crc >> 1);
    }
    return crc;
}

void sendBtns() {
    uint8_t btnbuf[2] = {button1_state, button2_state};
    uint16_t preamble = PREAMBLE;
    uint8_t proto = PROTO_VER;
    uint16_t plen = 2;
    xSemaphoreTake(serialMutex, portMAX_DELAY);
    digitalWrite(RS485_DE_PIN, HIGH);
    RS485_SERIAL.write((uint8_t*)&preamble, 2);
    RS485_SERIAL.write(proto);
    RS485_SERIAL.write(PACKET_BTN);
    RS485_SERIAL.write((uint8_t*)&plen, 2);
    RS485_SERIAL.write(btnbuf, 2);
    uint16_t crc = crc16_ibm(btnbuf, 2);
    RS485_SERIAL.write((uint8_t*)&crc, 2);
    RS485_SERIAL.flush();
    digitalWrite(RS485_DE_PIN, LOW);
    xSemaphoreGive(serialMutex);
}

void rxTask(void *pvParameters) {
    uint8_t header[6], rxbuf[MAX_PAYLOAD+2];
    while (1) {
        if (RS485_SERIAL.available() >= 6) {
            RS485_SERIAL.readBytes(header, 6);
            if (header[0]==0xAA && header[1]==0x55 && header[3]==PACKET_CMD) {
                uint16_t plen = *(uint16_t*)(header+4);
                if ((plen+2) <= sizeof(rxbuf)) {
                    RS485_SERIAL.readBytes(rxbuf, plen+2);
                    uint16_t exp_crc = *(uint16_t*)(rxbuf+plen);
                    if (crc16_ibm(rxbuf, plen) == exp_crc) {
                        size_t n = 0;
                        xSemaphoreTake(containerMutex, portMAX_DELAY);
                        memcpy(Container, rxbuf+n, sizeof(Container)); n += sizeof(Container);
                        memcpy(&ThisAircraft, rxbuf+n, sizeof(ThisAircraft)); n += sizeof(ThisAircraft);
                        memcpy(&aux, rxbuf+n, sizeof(aux));
                        xSemaphoreGive(containerMutex);
                        isDataFromA = true;
                        // Данные успешно получены, flag isDataFromA выставлен
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void txTask(void *pvParameters) {
    while(1) {
        // button1_state и button2_state можно обновлять здесь или через gpio, пример —
        // button1_state = digitalRead(PIN_BUTTON1);
        // button2_state = digitalRead(PIN_BUTTON2);
        sendBtns();
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_task_wdt_reset();
    }
}



//============================================================================

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
    //if (AuxFlags.Time_Hour_M != hour_tmp)
    //{
    //    hour_tmp = AuxFlags.Time_Hour_M;
    //    service.set_time_hour(AuxFlags.Time_Hour_M);
    //}

    //if (AuxFlags.Time_Minute_M != minute_tmp)
    //{
    //    minute_tmp = AuxFlags.Time_Minute_M;
    //    service.set_time_minute(AuxFlags.Time_Minute_M);
    //   // Serial.printf("%d:%d\r\n", AuxFlags.Time_Hour_M, AuxFlags.Time_Minute_M);
    //}

    //if (AuxFlags.new_SOS_flag_M != new_SOS_flag_M_tmp)
    //{
    //    new_SOS_flag_M_tmp = AuxFlags.new_SOS_flag_M;
    //    service.set_SOS_on_off((bool)AuxFlags.new_SOS_flag_M);
    //}

    //if (AuxFlags.isValidGNSS_M != isValidGNSS_M_tmp)
    //{
    //    isValidGNSS_M_tmp = AuxFlags.isValidGNSS_M;
    //    service.set_GNSS_on_off((bool)AuxFlags.isValidGNSS_M);
    //   // Serial.printf("AuxFlags.isValidGNSS_M %d \r\n", AuxFlags.isValidGNSS_M);
    //}
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
 
    rst_info* resetInfo;
    Serial.begin(115200);
    delay(500);
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
              delay(PULSE_ON_MS);
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

   RS485_SERIAL.begin(RS485_BAUD, RS485_CONFIG, RS485_RX_PIN, RS485_TX_PIN);
    pinMode(RS485_DE_PIN, OUTPUT); digitalWrite(RS485_DE_PIN, LOW);
    serialMutex = xSemaphoreCreateMutex();
    containerMutex = xSemaphoreCreateMutex();

    // Настройте пины для кнопок при необходимости:
    // pinMode(PIN_BUTTON1, INPUT_PULLUP);
    // pinMode(PIN_BUTTON2, INPUT_PULLUP);

    xTaskCreatePinnedToCore(rxTask, "rxTask", 4096, NULL, 2, NULL, 0);
    xTaskCreatePinnedToCore(txTask, "txTask", 2048, NULL, 1, NULL, 0);


/*
  pinMode(RS485_DE_PIN, OUTPUT);
  rs485SetTX(false);

  RS485_SERIAL.begin(RS485_BAUD, RS485_CONFIG, RS485_RX_PIN, RS485_TX_PIN);
  RS485_SERIAL.setRxBufferSize(512);
  RS485_SERIAL.setTxBufferSize(512);
  serialMutex = xSemaphoreCreateMutex();

  // Все задачи на ядре 0
  xTaskCreatePinnedToCore(RxTask, "RxTask", 8192, nullptr, 3, nullptr, 0);
  //============================================================================
*/
  ina219.begin();
   //ina219.setCalibration_32V_2A(); // По умолчанию калибровка на 32В и 2А. Меняйте, если нужно.

  SoC->WDT_setup();

  Serial.println("======== Setup END!========");
}



void loop()
{
    esp_task_wdt_reset();
    power_Off();

    if(!holdProcessed)
    {
        SoC->Display_loop();
    }

    WiFi_loop();
    Web_loop();
    OTA_loop();
    SoC->loop();

    //set_packet(); // Получить и записать информацию с базового модуля.

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

        if (settings->serial_out == SEND_SERIAL_INFO)
        {
            for (int i = 0; i < MAX_TRACKING_OBJECTS; ++i)
            {
                Serial.printf("Container[%u]:%06X:%d:%8s:%.0f:%.0f:%.0f:%.6f:%.6f\r\n",
                    i,
                    Container[i].addr,                   // Адрес устройства стороннего самолета
                    Container[i].squawk,                 // Номер, назначаемый диспетчером для обмена с локатором. 
                    Container[i].flight,                  // Номер рейса
                    Container[i].altitude,
                    Container[i].speed,
                    Container[i].course,
    /*                   Container[i].vert_rate,*/
                    Container[i].latitude,
                    Container[i].longitude/*,
                    Container[i].last_message_signal_strength_dbm,
                    Container[i].last_message_signal_quality_db*/
                );
                Serial.flush();
            }
            Serial.println("---------------------------------------------------------------");
        }

        float shunt_voltage_mV = ina219.getShuntVoltage_mV(); // Напряжение на шунте (милливольты)
        float bus_voltage_V = ina219.getBusVoltage_V();       // Напряжение на шине (вольты)
        float current_mA = ina219.getCurrent_mA();            // Ток (миллиамперы)
        float load_voltage = bus_voltage_V + (shunt_voltage_mV / 1000); // Точное напряжение на нагрузке
        int voltage_percent = voltageToPercent(load_voltage);

        service.set_voltage_value(load_voltage);
        service.set_current_value(current_mA);

        Serial.print("Напряжение шины: "); Serial.print(bus_voltage_V); Serial.println(" V");
        Serial.print("Напряжение на шунте: "); Serial.print(shunt_voltage_mV); Serial.println(" mV");
        Serial.print("Ток: "); Serial.print(current_mA); Serial.println(" mA");
        Serial.print("Напряжение на нагрузке: "); Serial.print(load_voltage); Serial.println(" V");
        Serial.println("-----");
    }


    Time_loop();

    yield();
}

//===================================================================================
