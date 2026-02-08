#include <stdio.h>                // define I/O functions
#include <Arduino.h>              // define I/O functions
#include "SPI.h"
#include <esp_task_wdt.h>
#include <iostream>
#include <locale.h>
#include <math.h>
//#include "freertos/FreeRTOS.h"
//#include "freertos/task.h"
//#include "freertos/semphr.h"

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
//#include "Button.h"
#include <HardwareSerial.h>
#include "SoftRF.h"
// ----------------- RTOS -----------------
//TaskHandle_t Task1, Task2, Task3;
//SemaphoreHandle_t serialMutex;

//=================================================================
#define RS485_SERIAL   Serial2
#define RS485_TX_PIN   18
#define RS485_RX_PIN   17
#define RS485_DE_PIN   21
#define RS485_BAUD     115200// 256000 //921600
#define RS485_CONFIG   SERIAL_8N1
#define LED            4  

#if !defined(SERIAL_FLUSH)
#define SERIAL_FLUSH() Serial.flush()
#endif

#define DEBUG 0
#define DEBUG_TIMING 0

#define isTimeToDisplay() (millis() - LEDTimeMarker     > 1000)
#define isTimeToExport()  (millis() - ExportTimeMarker  > 1000)

int threshold_level_tmp = 300;
int set_air = 0;   //  
bool set_test_coordinate = false; // Признак тестовых ввода текущих координат 
bool set_test_coordinate5 = false; // Признак тестовых ввода текущих координат 

//// ================ ЗАДАЧА ПЕРЕДАЧИ ================

unsigned long previousMillis = 0;            //  
const long interval = 1000;                  //  
unsigned long thresholdMillis = 0;            //  
const long threshold_interval = 10660;                  //  


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



ufo_t ThisAircraft;
extern ufo_t fo, Container[MAX_TRACKING_OBJECTS];
//aux_t AuxData;

//===============================================================
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

//static void onButtonPressDownCb(void* button_handle, void* usr_data) 
//{
//   service.set_num_button(1);
//}
//
//static void onButtonDoubleClickEventCb(void* button_handle, void* usr_data)
//{
//   service.set_num_button(2);
//}
//
//static void onButtonLongPressStartEventCb(void* button_handle, void* usr_data)
//{
//   service.set_num_button(3);
//}


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



//void printAux(const aux_t* aux)
//{
//    Serial.println("=== AuxData ===");
//    Serial.print("new_buttton_M: "); Serial.println(aux->new_button_M);  // Состояние кнопки
//    Serial.print("new_message: "); Serial.println(aux->new_message);
//    Serial.print("message_received: "); Serial.println(aux->message_received);
//    Serial.print("confirm_message_M"); Serial.println(aux->confirm_message_M);
//    Serial.print("msg_resp_M: "); Serial.println(aux->msg_resp_M);
//    Serial.print("Time_Hour_M: "); Serial.println(aux->Time_Hour_M);
//    Serial.print("Time_Minute_M: "); Serial.println(aux->Time_Minute_M);
//    Serial.print("new_SOS_flag_M: "); Serial.println(aux->new_SOS_flag_M);
//    Serial.print("isValidGNSS_M: "); Serial.println(aux->isValidGNSS_M);
//    vTaskDelay(pdMS_TO_TICKS(4));
//}

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
    btnEvt.timestamp = xTaskGetTickCountFromISR(); // или просто tick
    btnEvt.state = gpio_get_level((gpio_num_t)BUTTON_PIN);
}

//3. Задача обработки(ButtonTask) — вся логика таймингов
void ButtonTask(void* pvParameters)
{
    //const uint16_t DEBOUNCE_MS = 35;
    //const uint16_t DOUBLE_WIN_MS = 350;
    //const uint16_t LONG_PRESS_MS = 2000;

    //bool lastStable = true;
    //bool stable = true;
    //uint32_t lastChangeMs = 0;
    //uint32_t pressStartMs = 0;

    //bool waitingSecond = false;
    //uint32_t secondWinEndMs = 0;

    //bool longReported = false;
    //volatile bool event;
    //volatile uint32_t ts;
    //volatile bool state;

    //const TickType_t tick = pdMS_TO_TICKS(5);

    //esp_task_wdt_add(NULL); // текущая задача - loopTask

    //for (;;)
    //{
    //    esp_task_wdt_reset();
    //    // 1. Смотрим, был ли фронт/спад (флаг в ISR)
    //    event = btnEvt.event;
    //    ts = btnEvt.timestamp;
    //    state = btnEvt.state;

    //    if (event)
    //    {
    //        // сбрасываем флаг (atomic, но на ESP32 это ok)
    //        btnEvt.event = false;
    //        // Обработка антидребезга (таймер)
    //        if (millis() - lastChangeMs >= DEBOUNCE_MS)
    //        {
    //            lastChangeMs = millis();
    //            lastStable = stable;
    //            stable = (state == HIGH); // true - отпущена (INPUT_PULLUP)
    //            // Переход отпущена->нажата
    //            if (lastStable && !stable)
    //            {
    //                pressStartMs = millis();
    //                longReported = false;
    //            }
    //            // Переход нажата->отпущена
    //            else if (!lastStable && stable)
    //            {
    //                uint32_t dur = millis() - pressStartMs;
    //                if (dur >= LONG_PRESS_MS)
    //                {
    //                    if (!longReported) AuxData.new_button_M = 3;
    //                    waitingSecond = false;
    //                }
    //                else
    //                {
    //                    if (waitingSecond)
    //                    {
    //                        //  Serial.println("DOUBLE");
    //                        service.set_num_button(2);
    //                        waitingSecond = false;
    //                    }
    //                    else
    //                    {
    //                        waitingSecond = true;
    //                        secondWinEndMs = millis() + DOUBLE_WIN_MS;
    //                    }
    //                }
    //            }
    //        }
    //    }

    //    // Если кнопка удерживается — возможен LONG по времени
    //    if (!stable && !longReported && (millis() - pressStartMs >= LONG_PRESS_MS))
    //    {
    //        longReported = true;
    //        // Serial.println("LONG");
    //        service.set_num_button(3);
    //        waitingSecond = false;
    //    }

    //    // Окно двойного клика, если прошло — это одиночный клик (SHORT)
    //    if (waitingSecond && (int32_t)(millis() - secondWinEndMs) >= 0)
    //    {
    //        waitingSecond = false;
    //        service.set_num_button(1);
    //        //  Serial.println("SHORT");
    //    }

    //    vTaskDelay(tick);
    //}
}

//===============================================================================================

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

  if (settings->default_settings == SETTINGS_ON)
  {
      EEPROM_clear();
  }

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
 /*     SoC->swSer_enableRx(true);*/
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

  //// initializing a button
  //Button* btn = new Button(GPIO_NUM_48, false);

  //btn->attachPressDownEventCb(&onButtonPressDownCb, NULL);
  //btn->attachDoubleClickEventCb(&onButtonDoubleClickEventCb, NULL);
  //btn->attachLongPressStartEventCb(onButtonLongPressStartEventCb, NULL);
 
      //xTaskCreatePinnedToCore(rxTask, "RX", 8192, NULL, 1, &Task1, 0);
    //xTaskCreatePinnedToCore(receiveRP2040, "RP2040", 8192, NULL, 2, &Task2, 0);
  //  xTaskCreatePinnedToCore(ButtonTask, "BtnTask", 4096, NULL, 1, &Task3, 0);
  //  Serial.print("Sizeof full_packet_net_t: "); Serial.println(sizeof(full_packet_net_t));

    if (settings->threshold_level != threshold_level_tmp)
    {
        threshold_level_tmp = settings->threshold_level;
        //values[0] = settings->threshold_level;
        //sendPacketToRP2040(values, 1);
    }

 
    SoC->WDT_setup();

    Serial.println("================ Setup End =======================");
}

void loop()
{

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
         //!! printAux(&AuxData);
          Serial.println("===========================================================================");
      }

      //txTask_test();
  }

  if (service.get_num_button() != 0)
  {
      Serial.printf("Принят ответ: new_button_M=%u \r\n", service.get_num_button());
  }

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



