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
typedef struct {
    bool     new_flag_M;
    uint8_t  new_buttton_M;
    bool     setMessageRead_M;
    bool     MessageRead_M;
    uint8_t  Time_Hour_M;
    uint8_t  Time_Minute_M;
    bool     new_SOS_flag_M;
    bool     confirm_message_M;
    char     msg_resp_M[170];
    bool     isValidGNSS_M;
    uint8_t  FLYRF_MODE_TEST_M;
} aux_t;

aux_t AuxFlags;
uint16_t analog_code_M = 0;

// ================== RS485 / Протокол ==================
#define RS485_SERIAL         Serial1
#define RS485_TX_PIN         39
#define RS485_RX_PIN         38
#define RS485_DE_PIN         40

#define RS485_BAUD         921600
#define RS485_CONFIG         SERIAL_8N1

enum : uint8_t {
    PKT_CONTAINER = 0x01,
    PKT_THISAC = 0x02,
    PKT_ANALOG = 0x03,
    PKT_AUX = 0x04,
    PKT_HEARTBEAT = 0x05,
    PKT_ACK = 0xF0,
    PKT_NACK = 0xF1
};

#pragma pack(push,1)
struct FrameHeader {
    uint16_t preamble; // 0xAA55
    uint8_t  ver;      // 0x01
    uint8_t  type;
    uint8_t  index;    // 0..7, 0xFF, 0
    uint8_t  seq;      // seq от источника
    uint16_t length;   // payload length
};
#pragma pack(pop)

#define PREAMBLE 0x55AAu
#define PROTO_VER 0x01
#define MAX_PAYLOAD 512

static uint16_t crc16_ibm(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j) {
            if (crc & 1) crc = (crc >> 1) ^ 0xA001;
            else crc >>= 1;
        }
    }
    return crc;
}

static void rs485SetTX(bool enable) {
    digitalWrite(RS485_DE_PIN, enable ? HIGH : LOW);
    if (enable) delayMicroseconds(50);
}

// ============== FreeRTOS объекты (ядро 0) ==============
SemaphoreHandle_t serialMutex;

// ============== Парсер и ACK ===========================
static bool parseOneFrame(Stream& s, FrameHeader& hdr, uint8_t* payloadBuf, uint16_t& crc) 
{
    static uint8_t state = 0;
    uint8_t b;
    while (s.available()) 
    {
        b = s.read();
        if (state == 0 && b == 0xAA) { state = 1; continue; }
        if (state == 1 && b == 0x55) 
        {
            size_t need = sizeof(FrameHeader) - 2;
            uint8_t* p = ((uint8_t*)&hdr) + 2;
            size_t got = 0;
            while (got < need) {
                int v = s.read();
                if (v < 0) { state = 2; goto WAIT_CONT; }
                p[got++] = (uint8_t)v;
            }
            state = 3;
        WAIT_CONT:
            if (state != 3) return false;
            if (hdr.ver != PROTO_VER || hdr.length > MAX_PAYLOAD) {
                state = 0; continue;
            }
            for (uint16_t i = 0; i < hdr.length; ++i) {
                int v = -1; while ((v = s.read()) < 0) { delayMicroseconds(50); }
                payloadBuf[i] = (uint8_t)v;
            }
            uint8_t* c = (uint8_t*)&crc;
            for (int i = 0; i < 2; ++i) {
                int v = -1; while ((v = s.read()) < 0) { delayMicroseconds(50); }
                c[i] = (uint8_t)v;
            }
            state = 0;
            return true;
        }
        else 
        {
            state = 0;
        }
    }
    return false;
}

static void sendAckLike(uint8_t type, uint8_t index, uint8_t seq, bool positive) {
    uint8_t payload[4];
    payload[0] = type;
    payload[1] = index;
    payload[2] = seq;
    payload[3] = positive ? 0x06 : 0x15;

    FrameHeader hdr;
    hdr.preamble = PREAMBLE;
    hdr.ver = PROTO_VER;
    hdr.type = positive ? PKT_ACK : PKT_NACK;
    hdr.index = 0;
    hdr.seq = 0;
    hdr.length = sizeof(payload);

    uint8_t header_and_payload[sizeof(FrameHeader) - 2 + sizeof(payload)];
    memcpy(&header_and_payload[0], &hdr.ver, sizeof(FrameHeader) - 2);
    memcpy(&header_and_payload[sizeof(FrameHeader) - 2], payload, sizeof(payload));
    uint16_t crc = crc16_ibm(header_and_payload, sizeof(header_and_payload));

    if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        rs485SetTX(true);
        RS485_SERIAL.write((uint8_t*)&hdr, sizeof(hdr));
        RS485_SERIAL.write(payload, sizeof(payload));
        RS485_SERIAL.write((uint8_t*)&crc, sizeof(crc));
        RS485_SERIAL.flush();
        delayMicroseconds(200);
        rs485SetTX(false);
        xSemaphoreGive(serialMutex);
    }
}

// ============== Приёмная задача ========================
void RxTask(void* arg) 
{
    FrameHeader hdr;
    uint8_t payload[MAX_PAYLOAD];
    uint16_t rxCrc;
   // delay(1000);
 
    for (;;) 
    {
        if (!RS485_SERIAL.available()) 
        {
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }

        if (!parseOneFrame(RS485_SERIAL, hdr, payload, rxCrc)) 
        {
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }

        uint8_t buf[sizeof(FrameHeader) - 2 + MAX_PAYLOAD];
        memcpy(buf, &hdr.ver, sizeof(FrameHeader) - 2);
        memcpy(buf + sizeof(FrameHeader) - 2, payload, hdr.length);
        uint16_t calc = crc16_ibm(buf, (sizeof(FrameHeader) - 2) + hdr.length);

        bool ok = (calc == rxCrc);

        // ACK/NACK для служебных
        if (hdr.type == PKT_ACK || hdr.type == PKT_NACK) 
        {
            // Приёмник служебные подтверждения от источника не использует
            continue;
        }

        if (!ok) 
        {
            sendAckLike(hdr.type, hdr.index, hdr.seq, false);
          //!!  Serial.printf("[RX] CRC FAIL type=%02X idx=%u seq=%u\r\n", hdr.type, hdr.index, hdr.seq);
            continue;
        }

        // Обработка полезных данных
        switch (hdr.type) 
        {
            case PKT_CONTAINER: 
            {

                if (hdr.index < MAX_TRACKING_OBJECTS && hdr.length == sizeof(ufo_t)) 
                {
                    memcpy(&Container[hdr.index], payload, sizeof(ufo_t));
                    sendAckLike(hdr.type, hdr.index, hdr.seq, true);
                }
                else 
                {
                    sendAckLike(hdr.type, hdr.index, hdr.seq, false);
                }
            } break;

            case PKT_THISAC: 
            {
                if (hdr.length == sizeof(ufo_t))
                {
                    memcpy(&ThisAircraft, payload, sizeof(ufo_t));
                    sendAckLike(hdr.type, hdr.index, hdr.seq, true);
                }
                else 
                {
                    sendAckLike(hdr.type, hdr.index, hdr.seq, false);
                }
            } break;

            case PKT_ANALOG: 
            {
                if (hdr.length == sizeof(uint16_t)) 
                {
                    memcpy(&analog_code_M, payload, sizeof(uint16_t));
                    sendAckLike(hdr.type, hdr.index, hdr.seq, true);
                }
                else 
                {
                    sendAckLike(hdr.type, hdr.index, hdr.seq, false);
                }
            } break;

            case PKT_AUX: 
            {
                if (hdr.length == sizeof(aux_t)) 
                {
                    memcpy(&AuxFlags, payload, sizeof(aux_t));
                    sendAckLike(hdr.type, hdr.index, hdr.seq, true);
                }
                else 
                {
                    sendAckLike(hdr.type, hdr.index, hdr.seq, false);
                }
            } break;

            case PKT_HEARTBEAT: 
            {
                // Можно ничего не делать, только ACK
                sendAckLike(hdr.type, hdr.index, hdr.seq, true);
            } break;

            default: 
            {
                sendAckLike(hdr.type, hdr.index, hdr.seq, false);
            } break;
        }
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
    if (AuxFlags.Time_Hour_M != hour_tmp)
    {
        hour_tmp = AuxFlags.Time_Hour_M;
        service.set_time_hour(AuxFlags.Time_Hour_M);
    }

    if (AuxFlags.Time_Minute_M != minute_tmp)
    {
        minute_tmp = AuxFlags.Time_Minute_M;
        service.set_time_minute(AuxFlags.Time_Minute_M);
       // Serial.printf("%d:%d\r\n", AuxFlags.Time_Hour_M, AuxFlags.Time_Minute_M);
    }

    if (AuxFlags.new_SOS_flag_M != new_SOS_flag_M_tmp)
    {
        new_SOS_flag_M_tmp = AuxFlags.new_SOS_flag_M;
        service.set_SOS_on_off((bool)AuxFlags.new_SOS_flag_M);
    }

    if (AuxFlags.isValidGNSS_M != isValidGNSS_M_tmp)
    {
        isValidGNSS_M_tmp = AuxFlags.isValidGNSS_M;
        service.set_GNSS_on_off((bool)AuxFlags.isValidGNSS_M);
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

  pinMode(RS485_DE_PIN, OUTPUT);
  rs485SetTX(false);

  RS485_SERIAL.begin(RS485_BAUD, RS485_CONFIG, RS485_RX_PIN, RS485_TX_PIN);
  RS485_SERIAL.setRxBufferSize(512);
  RS485_SERIAL.setTxBufferSize(512);
  serialMutex = xSemaphoreCreateMutex();

  // Все задачи на ядре 0
  xTaskCreatePinnedToCore(RxTask, "RxTask", 8192, nullptr, 3, nullptr, 0);
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

    if(!holdProcessed)
    {
        SoC->Display_loop();
    }

    WiFi_loop();
    Web_loop();
    OTA_loop();
    SoC->loop();

    set_packet(); // Получить и записать информацию с базового модуля.

    if (strlen(AuxFlags.msg_resp_M) > 0)
    {
        service.setNewMessageFlag(true);
        Serial.println(AuxFlags.msg_resp_M);
        strncpy(service.msg_tmp_all, AuxFlags.msg_resp_M, strlen(AuxFlags.msg_resp_M));
        //!!strncpy(AuxFlags.msg_resp_M, "", strlen(AuxFlags.msg_resp_M));
        AuxFlags.msg_resp_M[0] = 0;
    }

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
