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
static bool parseOneFrame(Stream& s, FrameHeader& hdr, uint8_t* payloadBuf, uint16_t& crc) {
    static uint8_t state = 0;
    uint8_t b;
    while (s.available()) {
        b = s.read();
        if (state == 0 && b == 0xAA) { state = 1; continue; }
        if (state == 1 && b == 0x55) {
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
        else {
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
                    //Serial.printf("[RX] Container[%u]: lat=%.6f lon=%.6f alt=%.1f flight=%s\r\n",
                    //    hdr.index,
                    //    Container[hdr.index].latitude,
                    //    Container[hdr.index].longitude,
                    //    Container[hdr.index].altitude,
                    //    Container[hdr.index].flight);
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
                   //!! Serial.printf("[RX] ThisAircraft: lat=%.6f lon=%.6f alt=%.1f flight=%s\r\n",
                        //ThisAircraft.latitude,
                        //ThisAircraft.longitude,
                        //ThisAircraft.altitude,
                        //ThisAircraft.flight);
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
                    //Serial.printf("[RX] Analog code: %u\r\n", (unsigned)analog_code_M);
                    //service.set_analog_value((unsigned)analog_code_M);
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
                    //Serial.printf("[RX] AUX: new_flag=%d btn=%u MSG='%s' GNSS=%d MODE=%u\r\n",
                    //    (int)AuxFlags.new_flag_M,
                    //    AuxFlags.new_buttton_M,
                    //if (strlen(AuxFlags.msg_resp_M) > 0)
                    //{
                       // Serial.println(AuxFlags.msg_resp_M);
                    //    strncpy(AuxFlags.msg_resp_M, "", strlen(AuxFlags.msg_resp_M));
                    //}
                    //    (int)AuxFlags.isValidGNSS_M,
                    //    AuxFlags.FLYRF_MODE_TEST_M);
                    /*service.set_GNSS_on_off((bool)AuxFlags.isValidGNSS_M);*/
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

 /*   service.set_time_hour(AuxFlags.Time_Hour_M);
    service.set_time_minute(AuxFlags.Time_Minute_M);
    service.set_SOS_on_off((bool)AuxFlags.new_SOS_flag_M);
    service.set_GNSS_on_off((bool)AuxFlags.isValidGNSS_M);*/


}


void setup()
{
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

  SoC->WDT_setup();

  Serial.println("======== Setup END!========");
}



void loop()
{
    esp_task_wdt_reset();
    SoC->Display_loop();
    WiFi_loop();
    Web_loop();
    OTA_loop();
    SoC->loop();

    set_packet(); // Получить и записать информацию с базового модуля.
    if (strlen(AuxFlags.msg_resp_M) > 0)
    {
        Serial.println(AuxFlags.msg_resp_M);
        strncpy(AuxFlags.msg_resp_M, "", strlen(AuxFlags.msg_resp_M));
        AuxFlags.msg_resp_M[0] = 0;
    }
    Time_loop();

    yield();
}

//===================================================================================
