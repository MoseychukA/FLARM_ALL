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


    // Конфигурация RS485
#define RS485_TX_PIN 39
#define RS485_RX_PIN 38
#define RS485_DE_RE_PIN 40

// Modbus настройки
#define SLAVE_ID 1
#define BAUD_RATE 921600

// FreeRTOS настройки
#define STACK_SIZE_KB(x) (x * 1024)
#define CORE_0 0
#define CORE_1 1

// Приоритеты задач
#define PRIORITY_HIGH 3
#define PRIORITY_NORMAL 2
#define PRIORITY_LOW 1

// Создание объекта Modbus Slave
ModbusRTU mb;
HardwareSerial RS485Serial(1); 

// Структуры данных
//#define MAX_TRACKING_OBJECTS 8

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

typedef struct AdditionalData {
    bool new_flag_M;
    uint8_t new_buttton_M;
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
} additional_data_t;

// Структура для уведомлений об обновлениях
typedef struct {
    uint8_t objectType;
    uint8_t objectIndex;
    TickType_t updateTime;
} update_notification_t;

// Глобальные переменные с защитой мутексами
extern ufo_t Container[MAX_TRACKING_OBJECTS];

additional_data_t AdditionalInfo;

// Копии для отслеживания изменений
ufo_t Container_Previous[MAX_TRACKING_OBJECTS];
ufo_t ThisAircraft_Previous;
additional_data_t AdditionalInfo_Previous;

// FreeRTOS объекты
TaskHandle_t xModbusProcessingTask = NULL;
TaskHandle_t xDataExtractionTask = NULL;
TaskHandle_t xChangeMonitoringTask = NULL;
TaskHandle_t xStatisticsTask = NULL;

QueueHandle_t xUpdateQueue = NULL;
SemaphoreHandle_t xDataMutex = NULL;
SemaphoreHandle_t xModbusMutex = NULL;

// Статистика
volatile uint32_t totalUpdates = 0;
volatile uint32_t containerUpdates = 0;
volatile uint32_t thisAircraftUpdates = 0;
volatile uint32_t additionalInfoUpdates = 0;
volatile uint32_t registerWriteCount = 0;
volatile bool dataUpdated = false;
volatile TickType_t lastDataUpdate = 0;

// Функции конвертации
float uint16ToFloat(uint16_t high, uint16_t low) {
    union {
        float f;
        uint32_t i;
    } converter;
    converter.i = ((uint32_t)high << 16) | low;
    return converter.f;
}

time_t uint16ToTime(uint16_t high, uint16_t low) {
    return ((time_t)high << 16) | low;
}

// Функции сравнения
bool compareUFO(const ufo_t* ufo1, const ufo_t* ufo2) {
    return (memcmp(ufo1, ufo2, sizeof(ufo_t)) == 0);
}

bool compareAdditionalData(const additional_data_t* data1, const additional_data_t* data2) {
    return (memcmp(data1, data2, sizeof(additional_data_t)) == 0);
}

// Функция извлечения UFO объекта (thread-safe)
void extractUFOObject(uint8_t objectIndex, ufo_t* ufo) {
    const uint16_t baseRegister = (objectIndex < 8) ? (1000 + objectIndex * 100) : 1800;
    uint16_t registerIndex = 0;

    // Сохраняем предыдущее значение
    ufo_t* previousUfo = (objectIndex < 8) ? &Container_Previous[objectIndex] : &ThisAircraft_Previous;
    memcpy(previousUfo, ufo, sizeof(ufo_t));

    // Захватываем мутекс для чтения Modbus регистров
    if (xSemaphoreTake(xModbusMutex, pdMS_TO_TICKS(100)) == pdTRUE) {

        // Очистка структуры
        memset(ufo, 0, sizeof(ufo_t));

        // Извлечение данных
        ufo->timestamp = uint16ToTime(mb.Hreg(baseRegister + registerIndex),
            mb.Hreg(baseRegister + registerIndex + 1));
        registerIndex += 2;

        ufo->addr = ((uint32_t)mb.Hreg(baseRegister + registerIndex) << 16) |
            mb.Hreg(baseRegister + registerIndex + 1);
        registerIndex += 2;

        ufo->addr_type = mb.Hreg(baseRegister + registerIndex++);

        ufo->latitude = uint16ToFloat(mb.Hreg(baseRegister + registerIndex),
            mb.Hreg(baseRegister + registerIndex + 1));
        registerIndex += 2;

        ufo->longitude = uint16ToFloat(mb.Hreg(baseRegister + registerIndex),
            mb.Hreg(baseRegister + registerIndex + 1));
        registerIndex += 2;

        ufo->old_latitude = uint16ToFloat(mb.Hreg(baseRegister + registerIndex),
            mb.Hreg(baseRegister + registerIndex + 1));
        registerIndex += 2;

        ufo->old_longitude = uint16ToFloat(mb.Hreg(baseRegister + registerIndex),
            mb.Hreg(baseRegister + registerIndex + 1));
        registerIndex += 2;

        ufo->altitude = uint16ToFloat(mb.Hreg(baseRegister + registerIndex),
            mb.Hreg(baseRegister + registerIndex + 1));
        registerIndex += 2;

        ufo->pressure_altitude = uint16ToFloat(mb.Hreg(baseRegister + registerIndex),
            mb.Hreg(baseRegister + registerIndex + 1));
        registerIndex += 2;

        ufo->course = uint16ToFloat(mb.Hreg(baseRegister + registerIndex),
            mb.Hreg(baseRegister + registerIndex + 1));
        registerIndex += 2;

        ufo->speed = uint16ToFloat(mb.Hreg(baseRegister + registerIndex),
            mb.Hreg(baseRegister + registerIndex + 1));
        registerIndex += 2;

        ufo->aircraft_type = mb.Hreg(baseRegister + registerIndex++);

        // Flight number
        for (int i = 0; i < 8; i++) {
            uint16_t reg = mb.Hreg(baseRegister + registerIndex++);
            if (i * 2 < 16) ufo->flight[i * 2] = reg & 0xFF;
            if (i * 2 + 1 < 16) ufo->flight[i * 2 + 1] = (reg >> 8) & 0xFF;
        }
        ufo->flight[15] = '\0';

        ufo->vert_rate = ((int32_t)mb.Hreg(baseRegister + registerIndex) << 16) |
            mb.Hreg(baseRegister + registerIndex + 1);
        registerIndex += 2;

        ufo->Squawk = ((int32_t)mb.Hreg(baseRegister + registerIndex) << 16) |
            mb.Hreg(baseRegister + registerIndex + 1);
        registerIndex += 2;

        ufo->timemsg = uint16ToTime(mb.Hreg(baseRegister + registerIndex),
            mb.Hreg(baseRegister + registerIndex + 1));
        registerIndex += 2;

        ufo->vs = uint16ToFloat(mb.Hreg(baseRegister + registerIndex),
            mb.Hreg(baseRegister + registerIndex + 1));
        registerIndex += 2;

        ufo->geoid_separation = uint16ToFloat(mb.Hreg(baseRegister + registerIndex),
            mb.Hreg(baseRegister + registerIndex + 1));
        registerIndex += 2;

        ufo->hdop = mb.Hreg(baseRegister + registerIndex++);
        ufo->rssi = (int8_t)(mb.Hreg(baseRegister + registerIndex++) - 128);

        ufo->distance = uint16ToFloat(mb.Hreg(baseRegister + registerIndex),
            mb.Hreg(baseRegister + registerIndex + 1));
        registerIndex += 2;

        ufo->bearing = uint16ToFloat(mb.Hreg(baseRegister + registerIndex),
            mb.Hreg(baseRegister + registerIndex + 1));
        registerIndex += 2;

        ufo->signal_source = mb.Hreg(baseRegister + registerIndex++);

        ufo->seen = uint16ToTime(mb.Hreg(baseRegister + registerIndex),
            mb.Hreg(baseRegister + registerIndex + 1));
        registerIndex += 2;

        ufo->hour_msg = mb.Hreg(baseRegister + registerIndex++);
        ufo->min_msg = mb.Hreg(baseRegister + registerIndex++);
        ufo->delay_time_msg = mb.Hreg(baseRegister + registerIndex++);

        // Callsign
        for (int i = 0; i < 4; i++) {
            uint16_t reg = mb.Hreg(baseRegister + registerIndex++);
            if (i * 2 < 8) ufo->callsign[i * 2] = reg & 0xFF;
            if (i * 2 + 1 < 8) ufo->callsign[i * 2 + 1] = (reg >> 8) & 0xFF;
        }
        ufo->callsign[7] = '\0';

        xSemaphoreGive(xModbusMutex);

        // Проверяем на изменения
        if (!compareUFO(ufo, previousUfo)) {
            update_notification_t notification;
            notification.objectType = (objectIndex < 8) ? 0 : 1;
            notification.objectIndex = objectIndex;
            notification.updateTime = xTaskGetTickCount();

            xQueueSend(xUpdateQueue, &notification, 0);

            if (objectIndex < 8) {
                containerUpdates++;
            }
            else {
                thisAircraftUpdates++;
            }
            totalUpdates++;
        }
    }
}

// Функция извлечения дополнительных данных (thread-safe)
void extractAdditionalData() {
    const uint16_t baseRegister = 2000;
    uint16_t registerIndex = 0;

    // Сохраняем предыдущее значение
    memcpy(&AdditionalInfo_Previous, &AdditionalInfo, sizeof(AdditionalInfo));

    // Захватываем мутекс для чтения Modbus регистров
    if (xSemaphoreTake(xModbusMutex, pdMS_TO_TICKS(100)) == pdTRUE) {

        // Очистка структуры
        memset(&AdditionalInfo, 0, sizeof(additional_data_t));

        // Извлечение данных
        uint16_t boolFlags = mb.Hreg(baseRegister + registerIndex++);
        AdditionalInfo.new_flag_M = (boolFlags & (1 << 0)) != 0;
        AdditionalInfo.setMessageRead_M = (boolFlags & (1 << 1)) != 0;
        AdditionalInfo.MessageRead_M = (boolFlags & (1 << 2)) != 0;
        AdditionalInfo.SOS_Sprite_on_off_M = (boolFlags & (1 << 3)) != 0;
        AdditionalInfo.SOS_View_on_off_M = (boolFlags & (1 << 4)) != 0;
        AdditionalInfo.new_SOS_flag_M = (boolFlags & (1 << 5)) != 0;
        AdditionalInfo.confirm_message_M = (boolFlags & (1 << 6)) != 0;
        AdditionalInfo.isValidGNSS_M = (boolFlags & (1 << 7)) != 0;

        AdditionalInfo.new_buttton_M = mb.Hreg(baseRegister + registerIndex++);
        AdditionalInfo.FLYRF_MODE_TEST_M = mb.Hreg(baseRegister + registerIndex++);
        AdditionalInfo.analog_signal_M = mb.Hreg(baseRegister + registerIndex++);

        // msg_resp_M
        for (int i = 0; i < 30; i++) {
            uint16_t reg = mb.Hreg(baseRegister + registerIndex++);
            if (i * 2 < 60) AdditionalInfo.msg_resp_M[i * 2] = reg & 0xFF;
            if (i * 2 + 1 < 60) AdditionalInfo.msg_resp_M[i * 2 + 1] = (reg >> 8) & 0xFF;
        }
        AdditionalInfo.msg_resp_M[59] = '\0';

        xSemaphoreGive(xModbusMutex);

        // Проверяем на изменения
        if (!compareAdditionalData(&AdditionalInfo, &AdditionalInfo_Previous)) {
            update_notification_t notification;
            notification.objectType = 2;
            notification.objectIndex = 0;
            notification.updateTime = xTaskGetTickCount();

            xQueueSend(xUpdateQueue, &notification, 0);

            additionalInfoUpdates++;
            totalUpdates++;
        }
    }
}

// Callback функция для записи регистров (thread-safe)
uint16_t cbWrite(TRegister* reg, uint16_t val) {
    registerWriteCount++;
    dataUpdated = true;
    lastDataUpdate = xTaskGetTickCount();

    return val;
}

// Callback функция для чтения регистров
uint16_t cbRead(TRegister* reg, uint16_t val) {
    return val;
}

// ЗАДАЧА 1: Обработка Modbus (CORE 0, HIGH PRIORITY)
void vModbusProcessingTask(void* pvParameters) {
    const TickType_t xFrequency = pdMS_TO_TICKS(1);

    Serial.println("[TASK] Modbus Processing started on Core 0");

    for (;;) {
        mb.task();
        vTaskDelay(xFrequency);
    }
}

// ЗАДАЧА 2: Извлечение данных (CORE 0, NORMAL PRIORITY)
void vDataExtractionTask(void* pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(200);

    Serial.println("[TASK] Data Extraction started on Core 0");

    for (;;) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        if (dataUpdated && ((xTaskGetTickCount() - lastDataUpdate) > pdMS_TO_TICKS(100))) {

            if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {

                for (int i = 0; i < MAX_TRACKING_OBJECTS; i++) {
                    extractUFOObject(i, &Container[i]);
                }
                extractUFOObject(8, &ThisAircraft);
                extractAdditionalData();

                dataUpdated = false;

                xSemaphoreGive(xDataMutex);
            }
        }
    }
}

// ЗАДАЧА 3: Мониторинг изменений (CORE 0, NORMAL PRIORITY)
void vChangeMonitoringTask(void* pvParameters) {
    update_notification_t notification;

    Serial.println("[TASK] Change Monitoring started on Core 0");

    for (;;) {
        if (xQueueReceive(xUpdateQueue, &notification, pdMS_TO_TICKS(1000)) == pdTRUE) {

            switch (notification.objectType) {
            case 0:
                Serial.print("[UPDATE] Container[");
                Serial.print(notification.objectIndex);
                Serial.println("] updated");
                break;
            case 1:
                Serial.println("[UPDATE] ThisAircraft updated");
                break;
            case 2:
                Serial.println("[UPDATE] AdditionalInfo updated");
                break;
            }
        }
    }
}

// ЗАДАЧА 4: Статистика и вывод данных (CORE 0, LOW PRIORITY)
void vStatisticsTask(void* pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(2000);

    Serial.println("[TASK] Statistics started on Core 0");

    for (;;) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        Serial.println();
        Serial.println("=== FREERTOS MODBUS SLAVE STATISTICS ===");
        Serial.print("Core ID: ");
        Serial.println(xPortGetCoreID());
        Serial.print("Total registers written: ");
        Serial.println(registerWriteCount);
        Serial.print("Total data updates: ");
        Serial.println(totalUpdates);
        Serial.print("Container updates: ");
        Serial.println(containerUpdates);
        Serial.print("ThisAircraft updates: ");
        Serial.println(thisAircraftUpdates);
        Serial.print("AdditionalInfo updates: ");
        Serial.println(additionalInfoUpdates);
        Serial.print("Queue messages waiting: ");
        Serial.println(uxQueueMessagesWaiting(xUpdateQueue));
        Serial.print("Free heap: ");
        Serial.println(ESP.getFreeHeap());

        // Показываем текущие данные
        if (totalUpdates > 0) {
            Serial.println();
            Serial.println("=== CURRENT DATA ===");

            if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {

                if (ThisAircraft.addr != 0) {
                    Serial.println(">>> ThisAircraft <<<");
                    Serial.print("  Addr: 0x");
                    Serial.println(ThisAircraft.addr, HEX);
                    Serial.print("  Lat: ");
                    Serial.println(ThisAircraft.latitude, 6);
                    Serial.print("  Lon: ");
                    Serial.println(ThisAircraft.longitude, 6);
                    Serial.print("  Alt: ");
                    Serial.println(ThisAircraft.altitude, 1);
                    Serial.print("  Course: ");
                    Serial.println(ThisAircraft.course, 1);
                    Serial.print("  Flight: '");
                    Serial.print(ThisAircraft.flight);
                    Serial.println("'");
                }

                if (Container[0].addr != 0) {
                    Serial.println(">>> Container[0] <<<");
                    Serial.print("  Addr: 0x");
                    Serial.println(Container[0].addr, HEX);
                    Serial.print("  Lat: ");
                    Serial.println(Container[0].latitude, 6);
                    Serial.print("  Lon: ");
                    Serial.println(Container[0].longitude, 6);
                    Serial.print("  Alt: ");
                    Serial.println(Container[0].altitude, 1);
                    Serial.print("  Flight: '");
                    Serial.print(Container[0].flight);
                    Serial.println("'");
                }

                if (AdditionalInfo.analog_signal_M != 0) {
                    Serial.println(">>> Additional Data <<<");
                    Serial.print("  analog_signal: ");
                    Serial.println(AdditionalInfo.analog_signal_M);
                    Serial.print("  new_buttton_M: ");
                    Serial.println(AdditionalInfo.new_buttton_M);
                    Serial.print("  new_flag_M: ");
                    Serial.println(AdditionalInfo.new_flag_M ? "true" : "false");
                    if (strlen(AdditionalInfo.msg_resp_M) > 0) {
                        Serial.print("  msg_resp_M: '");
                        Serial.print(AdditionalInfo.msg_resp_M);
                        Serial.println("'");
                    }
                }

                xSemaphoreGive(xDataMutex);
            }
        }

        Serial.println("==========================================");
    }
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

  Serial.println("=== FREERTOS MODBUS SLAVE STARTING ===");
  Serial.print("Setup running on core: ");
  Serial.println(xPortGetCoreID());

  // Настройка RS485
  pinMode(RS485_DE_RE_PIN, OUTPUT);
  digitalWrite(RS485_DE_RE_PIN, LOW);

  // Инициализация Serial1 для RS485
  RS485Serial.begin(BAUD_RATE, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);

  // Настройка Modbus Slave
  mb.begin(&RS485Serial, RS485_DE_RE_PIN);
  mb.slave(SLAVE_ID);

  // Добавляем holding registers
  mb.addHreg(0, 0, 3000);

  // Устанавливаем callback функции
  mb.onSetHreg(0, cbWrite, 3000);
  mb.onGetHreg(0, cbRead, 3000);

  // Создание мутексов
  xDataMutex = xSemaphoreCreateMutex();
  xModbusMutex = xSemaphoreCreateMutex();

  if (xDataMutex == NULL || xModbusMutex == NULL) {
      Serial.println("ERROR: Failed to create mutexes!");
      while (1) delay(1000);
  }

  // Создание очереди для уведомлений об обновлениях
  xUpdateQueue = xQueueCreate(20, sizeof(update_notification_t));
  if (xUpdateQueue == NULL) {
      Serial.println("ERROR: Failed to create update queue!");
      while (1) delay(1000);
  }

  // Инициализация данных
  memset(&Container, 0, sizeof(Container));
  memset(&ThisAircraft, 0, sizeof(ThisAircraft));
  memset(&AdditionalInfo, 0, sizeof(AdditionalInfo));

  memset(&Container_Previous, 0, sizeof(Container_Previous));
  memset(&ThisAircraft_Previous, 0, sizeof(ThisAircraft_Previous));
  memset(&AdditionalInfo_Previous, 0, sizeof(AdditionalInfo_Previous));

  // Сброс статистики
  totalUpdates = 0;
  containerUpdates = 0;
  thisAircraftUpdates = 0;
  additionalInfoUpdates = 0;
  registerWriteCount = 0;
  dataUpdated = false;

  // Создание задач на ядре 0
  BaseType_t xReturned;

  xReturned = xTaskCreatePinnedToCore(
      vModbusProcessingTask,
      "ModbusProcessing",
      STACK_SIZE_KB(3),
      NULL,
      PRIORITY_HIGH,
      &xModbusProcessingTask,
      CORE_0
  );
  if (xReturned != pdPASS) {
      Serial.println("ERROR: Failed to create Modbus Processing task!");
  }

  xReturned = xTaskCreatePinnedToCore(
      vDataExtractionTask,
      "DataExtraction",
      STACK_SIZE_KB(4),
      NULL,
      PRIORITY_NORMAL,
      &xDataExtractionTask,
      CORE_0
  );
  if (xReturned != pdPASS) {
      Serial.println("ERROR: Failed to create Data Extraction task!");
  }

  xReturned = xTaskCreatePinnedToCore(
      vChangeMonitoringTask,
      "ChangeMonitoring",
      STACK_SIZE_KB(3),
      NULL,
      PRIORITY_NORMAL,
      &xChangeMonitoringTask,
      CORE_0
  );
  if (xReturned != pdPASS) {
      Serial.println("ERROR: Failed to create Change Monitoring task!");
  }

  xReturned = xTaskCreatePinnedToCore(
      vStatisticsTask,
      "Statistics",
      STACK_SIZE_KB(4),
      NULL,
      PRIORITY_LOW,
      &xStatisticsTask,
      CORE_0
  );
  if (xReturned != pdPASS) {
      Serial.println("ERROR: Failed to create Statistics task!");
  }

  Serial.println("All tasks created successfully!");
  Serial.println("FreeRTOS Modbus Slave ready on Core 0");
  Serial.println("=========================================");

  //============================================================================


 
  SoC->WDT_setup();
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

    Time_loop();

    yield();
}

//===================================================================================
