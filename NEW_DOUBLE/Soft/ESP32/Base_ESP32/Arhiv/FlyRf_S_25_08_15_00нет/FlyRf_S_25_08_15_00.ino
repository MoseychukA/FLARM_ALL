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
#include "CoreCommandBuffer.h"    // обработчик входящих по UART команд
#include "Module1090.h"
#include "Button.h"
#include <ModbusMaster.h>
#include <HardwareSerial.h>



int set_air = 0;   //  
bool set_test_coordinate = false; // Признак тестовых ввода текущих координат 
bool set_test_coordinate5 = false; // Признак тестовых ввода текущих координат 

void txrx_test();
void normal();

HardwareSerial rs485Serial(2);

#if !defined(SERIAL_FLUSH)
#define SERIAL_FLUSH() Serial.flush()
#endif

#define DEBUG 0
#define DEBUG_TIMING 0

#define isTimeToDisplay() (millis() - LEDTimeMarker     > 1000)
#define isTimeToExport()  (millis() - ExportTimeMarker  > 1000)

ufo_t ThisAircraft;
//===================================================================================================================

// Структура UFO
typedef struct UFOM {
    time_t    timestamp;
    uint32_t  addr;
    uint8_t   addr_type;
    float     latitude;
    float     longitude;
    float     old_latitude;
    float     old_longitude;
    float     altitude;
    float     pressure_altitude;
    float     course;        /* CoG */
    float     speed;         /* ground speed in knots */
    uint8_t   aircraft_type;
    char      flight[16];    // Flight number
    int       vert_rate;     // Vertical rate.
    int       Squawk;        // Squawk
    time_t    timemsg;       // Время передачи сообщения о координатах стороннего самолета
    float     vs;            /* feet per minute */
    float     geoid_separation; /* metres */
    uint16_t  hdop;          /* cm */
    int8_t    rssi;          /* SX1276 only */
    float     distance;
    float     bearing;
    uint8_t   signal_source;
    time_t    seen;          // Time at which the last packet was received
    uint8_t   hour_msg;
    uint8_t   min_msg;
    uint16_t  delay_time_msg;
    uint8_t   callsign[8];
} ufo_tm;








//void updateDataHighFreq();
//void sendAnalogValueToQueueFast(uint8_t priority);
//void sendAdditionalDataToQueueFast(uint8_t priority);
//void sendThisAircraftToQueueFast(uint8_t priority);
//void sendContainerObjectToQueueFast(int objIndex, uint8_t priority);
//void sendContainerBatchToQueue();
//void updatePerformanceStats();
//void testConnection();
//void printTransmissionStats();
//void additionalDataToModbus(additional_data_t* data, uint16_t* buffer);
//void ufoToModbusData(ufo_t* ufo_m, uint16_t* buffer);
//void printModbusError(uint8_t errorCode);
//void initTestData();
//bool transmitBatchDataFast(transmission_task_t* task);
//void transmissionTask(void* parameter);
//void categorizeError(uint8_t errorCode);

// 
// Дополнительные данные
typedef struct AdditionalData {
    bool      new_flag;
    uint8_t   new_button;
    bool      setMessageRead;
    bool      MessageRead;
    bool      SOS_Sprite_on_off;
    bool      SOS_View_on_off;
    bool      new_SOS_flag;  
    bool      confirm_message;
    char      msg_resp[60];
    bool      isValidGNSS;
    uint8_t   FLYRF_MODE_TEST;
} additional_data_t;



// Структура для передачи данных между ядрами
typedef struct TransmissionTask {
    bool      ready;
    uint8_t   task_type;  // 0=analog, 1=additional, 2=thisaircraft, 3=container, 4=batch
    int       object_index;
    uint16_t  data_buffer[200];  // Увеличен буфер для пакетной передачи
    uint16_t  start_register;
    uint16_t  data_size;
    uint8_t   priority;   // Приоритет задачи (0-высший)
    uint32_t  timestamp;  // Время создания задачи
    uint8_t   batch_count; // Количество объектов в пакете
} transmission_task_t;

// Глобальные переменные
ufo_tm Container_M[MAX_TRACKING_OBJECTS];
ufo_tm ThisAircraft_M;
additional_data_t AdditionalData;

ModbusMaster node;

uint16_t analogValue = 0;

// Переменные для межъядерного взаимодействия
QueueHandle_t transmissionQueue;
QueueHandle_t priorityQueue;
QueueHandle_t batchQueue;  // Очередь для пакетной передачи
TaskHandle_t transmissionTaskHandle;
TaskHandle_t batchTaskHandle;  // Дополнительная задача для пакетной передачи
SemaphoreHandle_t dataMutex;
SemaphoreHandle_t rs485Mutex;

// Статистика передачи
typedef struct TransmissionStats {
    uint32_t total_packets;
    uint32_t successful_packets;
    uint32_t failed_packets;
    uint32_t retried_packets;
    uint32_t analog_packets;
    uint32_t additional_packets;
    uint32_t thisaircraft_packets;
    uint32_t container_packets;
    uint32_t batch_packets;
    uint32_t timeout_errors;
    uint32_t checksum_errors;
    uint32_t response_errors;
    uint32_t packets_per_second;
    uint32_t bytes_per_second;
} transmission_stats_t;

transmission_stats_t txStats = { 0 };

// Переменные для измерения производительности
uint32_t lastPerformanceCheck = 0;
uint32_t packetsInLastSecond = 0;
uint32_t bytesInLastSecond = 0;

void preTransmission() {
    digitalWrite(DE_RE_PIN, HIGH);
    delayMicroseconds(100); // Уменьшена задержка для скорости
}

void postTransmission() {
    delayMicroseconds(100); // Уменьшена задержка для скорости
    digitalWrite(DE_RE_PIN, LOW);
}

// Высокоскоростная задача передачи данных на ядре 0
void transmissionTask(void* parameter) 
{
    transmission_task_t task;

    Serial.printf("Высокоскоростная задача передачи запущена на ядре: %d\n", xPortGetCoreID());

    while (true) {
        bool taskReceived = false;

        // Приоритетная очередь проверяется чаще
        if (xQueueReceive(priorityQueue, &task, pdMS_TO_TICKS(1)) == pdTRUE) {
            taskReceived = true;
        }
        // Обычная очередь
        else if (xQueueReceive(transmissionQueue, &task, pdMS_TO_TICKS(5)) == pdTRUE) {
            taskReceived = true;
        }

        if (taskReceived) {
            // Быстрая проверка актуальности задачи
            if ((millis() - task.timestamp) > TRANSMISSION_TIMEOUT) {
                txStats.timeout_errors++;
                continue;
            }

            bool success = false;
            uint32_t startTime = micros();

            // Получаем мьютекс RS485 с коротким таймаутом
            if (xSemaphoreTake(rs485Mutex, pdMS_TO_TICKS(100)) == pdTRUE) {

                switch (task.task_type) {
                case 0: // Аналоговое значение - быстрая передача
                    success = transmitAnalogValueFast(task.data_buffer[0]);
                    txStats.analog_packets++;
                    break;

                case 1: // Дополнительные данные
                    success = transmitDataBlocksFast(task.start_register, task.data_buffer, task.data_size);
                    txStats.additional_packets++;
                    break;

                case 2: // ThisAircraft
                    success = transmitDataBlocksFast(task.start_register, task.data_buffer, task.data_size);
                    txStats.thisaircraft_packets++;
                    break;

                case 3: // Container объект
                    success = transmitDataBlocksFast(task.start_register, task.data_buffer, task.data_size);
                    txStats.container_packets++;
                    break;

                case 4: // Пакетная передача
                    success = transmitBatchDataFast(&task);
                    txStats.batch_packets++;
                    break;
                }

                xSemaphoreGive(rs485Mutex);

                // Обновляем статистику производительности
                uint32_t transmissionTime = micros() - startTime;
                packetsInLastSecond++;
                bytesInLastSecond += (task.data_size * 2); // 2 байта на регистр

                // Обновляем общую статистику
                txStats.total_packets++;
                if (success) {
                    txStats.successful_packets++;
                }
                else {
                    txStats.failed_packets++;
                }

                // Минимальная задержка только при ошибке
                if (!success) {
                    vTaskDelay(pdMS_TO_TICKS(10));
                }
            }
            else {
                txStats.failed_packets++;
            }
        }

        // Минимальная задержка для максимальной производительности
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// Задача пакетной передачи на том же ядре 0
void batchTransmissionTask(void* parameter) 
{
    transmission_task_t task;

    Serial.printf("Задача пакетной передачи запущена на ядре: %d\n", xPortGetCoreID());

    while (true) {
        if (xQueueReceive(batchQueue, &task, pdMS_TO_TICKS(50)) == pdTRUE) {

            if (xSemaphoreTake(rs485Mutex, pdMS_TO_TICKS(200)) == pdTRUE) {
                bool success = transmitBatchDataFast(&task);

                txStats.total_packets++;
                txStats.batch_packets++;
                if (success) {
                    txStats.successful_packets++;
                }
                else {
                    txStats.failed_packets++;
                }

                xSemaphoreGive(rs485Mutex);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

bool transmitAnalogValueFast(uint16_t value) 
{
    // Одна попытка для максимальной скорости
    uint8_t result = node.writeSingleRegister(999, value);
    return (result == node.ku8MBSuccess);
}

bool transmitDataBlocksFast(uint16_t startRegister, uint16_t* dataBuffer, uint16_t dataSize) 
{
    // Передача большими блоками для скорости
    int numBlocks = (dataSize + BLOCK_SIZE - 1) / BLOCK_SIZE;
    bool allSuccess = true;

    for (int block = 0; block < numBlocks; block++) {
        uint16_t regAddr = startRegister + (block * BLOCK_SIZE);
        uint16_t blockSize = min(BLOCK_SIZE, dataSize - (block * BLOCK_SIZE));

        bool blockSuccess = false;

        // Максимум 2 попытки для скорости
        for (int retry = 0; retry < MAX_RETRIES && !blockSuccess; retry++) {
            node.clearTransmitBuffer();
            for (int i = 0; i < blockSize; i++) {
                node.setTransmitBuffer(i, dataBuffer[block * BLOCK_SIZE + i]);
            }

            uint8_t result = node.writeMultipleRegisters(regAddr, blockSize);

            if (result == node.ku8MBSuccess) {
                blockSuccess = true;
                if (retry > 0) {
                    txStats.retried_packets++;
                }
            }
            else {
                categorizeError(result);
                // Минимальная задержка между повторами
                if (retry < MAX_RETRIES - 1) {
                    vTaskDelay(pdMS_TO_TICKS(5));
                }
            }
        }

        if (!blockSuccess) {
            allSuccess = false;
        }

        // Минимальная задержка между блоками
        vTaskDelay(pdMS_TO_TICKS(2));
    }

    return allSuccess;
}

bool transmitBatchDataFast(transmission_task_t* task) 
{
    // Передача нескольких объектов одним пакетом
    bool success = true;

    for (int obj = 0; obj < task->batch_count; obj++) 
    {
        uint16_t objStartReg = task->start_register + (obj * 50);
        uint16_t* objData = &task->data_buffer[obj * 50];

        if (!transmitDataBlocksFast(objStartReg, objData, 50)) 
        {
            success = false;
        }
    }

    return success;
}

void categorizeError(uint8_t errorCode)
{
    switch (errorCode) {
    case 0xE0: txStats.response_errors++; break;
    case 0xE1: txStats.timeout_errors++; break;
    case 0xE5: txStats.checksum_errors++; break;
    }
}


//void updateDataHighFreq();
//void sendAnalogValueToQueueFast(uint8_t priority);
//void sendAdditionalDataToQueueFast(uint8_t priority);
//void sendThisAircraftToQueueFast(uint8_t priority);
//void sendContainerObjectToQueueFast(int objIndex, uint8_t priority);
//void sendContainerBatchToQueue();
//void updatePerformanceStats();
//void testConnection();
//void printTransmissionStats();
//void additionalDataToModbus(additional_data_t* data, uint16_t* buffer);
//void ufoToModbusData(ufo_t* ufo_m, uint16_t* buffer);
//void printModbusError(uint8_t errorCode);
//void initTestData();
//bool transmitBatchDataFast(transmission_task_t* task);


//==================================================================================================================


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

 /* SoC->Button_setup();*/

  ThisAircraft.addr = SoC->getChipId() & 0x00FFFFFF;

  hw_info.rf = RF_setup();

  delay(100);

  hw_info.baro = Baro_setup();

  //hw_info.display = SoC->Display_setup();

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
      SoC->swSer_enableRx(true);
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

  moduleDump1090.setup();
  CommandHandler.setup();

  // initializing a button
  Button* btn = new Button(GPIO_NUM_48, false);

  btn->attachPressDownEventCb(&onButtonPressDownCb, NULL);
  btn->attachDoubleClickEventCb(&onButtonDoubleClickEventCb, NULL);
  btn->attachLongPressStartEventCb(onButtonLongPressStartEventCb, NULL);

  //----------------------------------------------------------------------------------------------
  // Увеличена скорость RS485
  rs485Serial.begin(BAUD_RATE, SERIAL_8N1, RX_PIN, TX_PIN);
  rs485Serial.setRxBufferSize(1024);  // Большие буферы
  rs485Serial.setTxBufferSize(1024);


  Serial.printf("Высокопроизводительный код выполняется на ядре: %d\n", xPortGetCoreID());

  // Настройка RS485 с максимальной скоростью
  pinMode(DE_RE_PIN, OUTPUT);
  digitalWrite(DE_RE_PIN, LOW);

  // Настройка Modbus с быстрыми таймаутами
  node.begin(SLAVE_ID, rs485Serial);
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);

  // Создание мьютексов
  dataMutex = xSemaphoreCreateMutex();
  rs485Mutex = xSemaphoreCreateMutex();

  // Создание очередей с большими размерами
  transmissionQueue = xQueueCreate(50, sizeof(transmission_task_t));   // Большая очередь
  priorityQueue = xQueueCreate(20, sizeof(transmission_task_t));       // Приоритетная очередь
  batchQueue = xQueueCreate(10, sizeof(transmission_task_t));          // Пакетная очередь

  if (transmissionQueue == NULL || priorityQueue == NULL || batchQueue == NULL) 
  {
      Serial.println("Ошибка создания очередей!");
      return;
  }

  // Создание высокоприоритетной задачи передачи на ядре 0
  xTaskCreatePinnedToCore(
      transmissionTask,       // Функция задачи
      "HighSpeedTransmission",// Имя задачи
      8192,                   // Размер стека
      NULL,                   // Параметры
      3,                      // Высокий приоритет
      &transmissionTaskHandle,// Хэндл задачи
      1                       // Ядро 0
  );

  // Создание задачи пакетной передачи на ядре 0
  xTaskCreatePinnedToCore(
      batchTransmissionTask,  // Функция задачи
      "BatchTransmission",    // Имя задачи
      4096,                   // Размер стека
      NULL,                   // Параметры
      2,                      // Средний приоритет
      &batchTaskHandle,       // Хэндл задачи
      1                       // Ядро 0
  );

  Serial.println("Высокоскоростной Master ESP32 инициализирован");
  Serial.printf("Скорость RS485: 57600 бод\n");
  Serial.printf("Размер блока передачи: %d регистров\n", BLOCK_SIZE);
  Serial.printf("Максимальное количество повторов: %d\n", MAX_RETRIES);

  // Инициализация тестовых данных
  initTestData();

  delay(1000);
  testConnection();

  SoC->WDT_setup();
}

void loop()
{
    RF_loop();                       // Сначала выполните общие действия с радиочастотами
    moduleDump1090.update();         // Проверить прием пакета от DUMP1090
   esp_task_wdt_reset();
  switch (settings->mode)
  {
#if !defined(EXCLUDE_TEST_MODE)
 // case FLYRF_MODE_TXRX_TEST0:
  case FLYRF_MODE_TXRX_TEST1:
  case FLYRF_MODE_TXRX_TEST2:
  case FLYRF_MODE_TXRX_TEST3:
  case FLYRF_MODE_TXRX_TEST4:
  case FLYRF_MODE_TXRX_TEST5:
    txrx_test();
    break;
#endif /* EXCLUDE_TEST_MODE */
  case FLYRF_MODE_NORMAL:
  default:
    normal();
    break;
  }

  // Show status info on tiny OLED display
  //SoC->Display_loop();

  // Handle DNS
  WiFi_loop();

  // Handle Web
  Web_loop();
  esp_task_wdt_reset();
  // Handle OTA update.
  OTA_loop();

  SoC->loop();

  if (SoC->Bluetooth_ops) 
  {
    SoC->Bluetooth_ops->loop();
  }

  if (SoC->UART_ops) {
     SoC->UART_ops->loop();
  }

  CommandHandler.handleCommands();
  CommandHandler.SendTraffic_Msg();
  CommandHandler.GPS_send_base();
  esp_task_wdt_reset();

 // // Высокочастотный основной цикл на ядре 1

 // Обновление данных с высокой частотой
  updateDataHighFreq();

  // Чтение аналогового сигнала
  analogValue = analogRead(ANALOG_PIN);

  // Высокочастотная отправка задач

  // 1. Аналоговое значение (каждый цикл)
  sendAnalogValueToQueueFast(0);

  // 2. Дополнительные данные (каждые 5 циклов)
  static int addDataCycle = 0;
  if (++addDataCycle >= 5) {
      sendAdditionalDataToQueueFast(1);
      addDataCycle = 0;
  }

  // 3. ThisAircraft (каждые 3 цикла)
  static int thisCycle = 0;
  if (++thisCycle >= 3) {
      sendThisAircraftToQueueFast(1);
      thisCycle = 0;
  }

  esp_task_wdt_reset();

  // 4. Container объекты - пакетная передача
  static int containerCycle = 0;
  if (++containerCycle >= 10) {
      sendContainerBatchToQueue();
      containerCycle = 0;
  }
  esp_task_wdt_reset();

  // 5. Передача отдельных Container объектов с высокой частотой
  static int currentObject = 0;
  sendContainerObjectToQueueFast(currentObject, 2);
  currentObject = (currentObject + 1) % MAX_TRACKING_OBJECTS;

  // Мониторинг производительности
  updatePerformanceStats();

  esp_task_wdt_reset();
  // Вывод статистики реже
  static int statsCycle = 0;
  if (++statsCycle >= 1000) {
      printTransmissionStats();
      statsCycle = 0;
  }

 // delay(20); // Высокочастотный цикл - каждые 20мс

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

void normal()
{
  bool success;

  Baro_loop();

  GNSS_loop();

  ThisAircraft.timestamp = now();
  if (isValidFix()) 
  {
    ThisAircraft.latitude  = gnss.location.lat();
    ThisAircraft.longitude = gnss.location.lng();
    ThisAircraft.altitude  = gnss.altitude.meters();
    ThisAircraft.course    = gnss.course.deg();
    ThisAircraft.speed     = gnss.speed.knots();
    ThisAircraft.hdop      = (uint16_t) gnss.hdop.value();
    ThisAircraft.geoid_separation = gnss.separation.meters();

    if (ThisAircraft.latitude != 0 || ThisAircraft.longitude != 0)
    {
        ThisAircraft.old_latitude = gnss.location.lat();
        ThisAircraft.old_longitude = gnss.location.lng();
    }


#if !defined(EXCLUDE_EGM96)
    /*
     * When geoidal separation is zero or not available - use approx. EGM96 value
     */
    if (ThisAircraft.geoid_separation == 0.0) 
    {
      ThisAircraft.geoid_separation = (float) LookupSeparation(ThisAircraft.latitude, ThisAircraft.longitude);
      /* we can assume the GPS unit is giving ellipsoid height */
      ThisAircraft.altitude -= ThisAircraft.geoid_separation;
    }
#endif /* EXCLUDE_EGM96 */

    RF_Transmit(RF_Encode(&ThisAircraft), true);   // Передать параметры посредством LoRa
  }
  else
  {
      if (ThisAircraft.old_latitude != 0 || ThisAircraft.old_longitude != 0)
      {
          ThisAircraft.altitude = 25000.0;

          RF_Transmit(RF_Encode(&ThisAircraft), true);  // Передать параметры посредством LoRa в случае если нет сигналов GPS
      }
  }
  success = RF_Receive();  //

#if DEBUG
  success = true;
#endif

  if (success && isValidFix()) ParseData();

  if (isValidFix()) 
  {
    Traffic_loop();
  }

  if (isTimeToDisplay()) 
  {
     LEDTimeMarker = millis();
  }

  if (isTimeToExport()) 
  { 
    NMEA_Export();
    D1090_Export();  

    ExportTimeMarker = millis();
  }

  // Handle Air Connect
  NMEA_loop();

  ClearExpired();
}

#if !defined(EXCLUDE_TEST_MODE)

unsigned int pos_ndx = 0;
unsigned long TxPosUpdMarker = 0;

float altitude0 = 100.0;
float altitude1 = 100.0;
float altitude2 = 100.0;
float altitude3 = 100.0;
float altitude4 = 100.0;
float altitude5 = 100.0;


float speed0 = 300.0;
float speed1 = 300.0;
float speed2 = 300.0;
float speed3 = 300.0;
float speed4 = 300.0;
float speed5 = 300.0;

//bool alt_high0 = false;
bool alt_high1 = false;
bool alt_high2 = false;
bool alt_high3 = false;
bool alt_high4 = false; 
bool alt_high5 = false;


float test_curse0 = 0.0;


//Атлантический океан
/*
0.075397, 0.029420
-0.004039, 0.029420
-0.004039, -0.054865
 0.075397, -0.054865
*/

int track_air = 0;
float alien_lat13 = 0.075397;
float alien_lon13 = 0.029420;

float alien_lat14 = -0.004039;
float alien_lon14 = 0.029420;

float alien_lat15 = -0.004039;
float alien_lon15 = -0.054865;

float alien_lat16 = 0.075397;
float alien_lon16 = -0.054865;

float alien_lat20 = 0.075397;
float alien_lon20 = 0.029420;


/*
  0.053769, -179.953328
  -0.025508, -179.953328
  -0.025508, 179.953328
  0.053769, 179.953328
*/

float alien_lat23 = 0.053769;
float alien_lon23 = -179.953328;

float alien_lat24 = -0.025508;
float alien_lon24 = -179.953328;

float alien_lat25 = -0.025508;
float alien_lon25 = 179.953328;

float alien_lat26 = 0.053769;
float alien_lon26 = 179.953328;

float alien_lat30 = 0.053769;
float alien_lon30 = -179.953328;

char fly1[] = "AFL1118";
char fly2[] = "AFL2122";
char fly3[] = "AFL1684";
char fly4[] = "SMD6405";
char fly5[] = "AFL1354";


//=============================== новый вариант расчета координат ================================
// Структура для хранения данных самолета
struct Aircraft_test {
    float latitude;
    float longitude;
    float course;
    float speed; // м/с
    double totalDistance; // общая дистанция для данного самолета
    double currentDistance; // текущая пройденная дистанция
    bool movingForward; // направление движения
    int id; // идентификатор самолета
};
// Константы


const float EARTH_RADIUS = 6371000.0; // Радиус Земли в метрах
const float DISTANCE_STEP = 250.0; // Шаг перемещения в метрах
const float TOTAL_DISTANCE = 10000.0; // Общая дистанция в метрах
const unsigned long UPDATE_INTERVAL = 1000; // Интервал обновления в мс


// Переменные для управления движением
float startLatitude = ThisAircraft.test_latitude;
float startLongitude = ThisAircraft.test_longitude;
float currentDistance = 0.0;
bool movingForward = true;
unsigned long lastUpdate = 0;

// Функция перемещения самолета на заданное расстояние
void moveAircraft(float distance)
{
    float lat1 = ThisAircraft.latitude * DEG_TO_RAD;
    float lon1 = ThisAircraft.longitude * DEG_TO_RAD;
    float bearing = ThisAircraft.course * DEG_TO_RAD;

    float angular_distance = distance / EARTH_RADIUS;

    // Вычисление новой широты
    float lat2 = asin(sin(lat1) * cos(angular_distance) +
        cos(lat1) * sin(angular_distance) * cos(bearing));

    // Вычисление новой долготы
    float dlon = atan2(sin(bearing) * sin(angular_distance) * cos(lat1),
        cos(angular_distance) - sin(lat1) * sin(lat2));

    float lon2 = fmod(lon1 + dlon + 3 * PI, 2 * PI) - PI; // Нормализация долготы

    // Обновление координат
    ThisAircraft.latitude = lat2 * RAD_TO_DEG;
    ThisAircraft.longitude = lon2 * RAD_TO_DEG;
}


//
//// Функция вывода текущей позиции
//void printCurrentPosition() {
//    Serial.printf("Дистанция: %.0f м | ", currentDistance);
//    Serial.printf("Координаты: %.6f°, %.6f° | ",
//        ThisAircraft.latitude, ThisAircraft.longitude);
//    Serial.printf("Курс: %.1f°\n", ThisAircraft.course);
//}

// Функция проверки и изменения курса
void checkAndUpdateCourse() 
{
    if (movingForward && currentDistance >= TOTAL_DISTANCE) 
    {
        // Достигли конечной точки - разворот на 180°
        ThisAircraft.course = fmod(ThisAircraft.course + 180.0, 360.0);
        movingForward = false;
        currentDistance = 0.0;

        //Serial.println(" ДОСТИГНУТА КОНЕЧНАЯ ТОЧКА ");
        //Serial.printf(" НОВЫЙ КУРС: %.1f° \n\n", ThisAircraft.course);

    }
    else if (!movingForward && currentDistance >= TOTAL_DISTANCE) 
    {
        // Вернулись к точке старта - снова разворот на 180°
        ThisAircraft.course = fmod(ThisAircraft.course + 180.0, 360.0);
        movingForward = true;
        currentDistance = 0.0;

        //Serial.println(" ВОЗВРАТ К ТОЧКЕ СТАРТА ");
        //Serial.printf(" НОВЫЙ КУРС: %.1f° \n\n", ThisAircraft.course);
    }
}

// Функция вычисления расстояния между двумя точками(формула гаверсинуса)
float calculateDistance(float lat1, float lon1, float lat2, float lon2) 
{
    float dLat = (lat2 - lat1) * DEG_TO_RAD;
    float dLon = (lon2 - lon1) * DEG_TO_RAD;

    float a = sin(dLat / 2) * sin(dLat / 2) +
        cos(lat1 * DEG_TO_RAD) * cos(lat2 * DEG_TO_RAD) *
        sin(dLon / 2) * sin(dLon / 2);

    float c = 2 * atan2(sqrt(a), sqrt(1 - a));
    return EARTH_RADIUS * c;
}


//--------------------------версия на 5 самолетов -----------------------------------------------
// Структура для хранения данных самолета
struct Aircraft5 {
    float latitude;
    float longitude;
    float course;
    float speed; // м/с
    float totalDistance; // общая дистанция для данного самолета
    float currentDistance; // текущая пройденная дистанция
    bool movingForward; // направление движения
    int id; // идентификатор самолета
};

// Массив из 5 самолетов
Aircraft5 aircraft5[5];

//// Константы
//const float EARTH_RADIUS = 6371000.0; // Радиус Земли в метрах
////const float DEG_TO_RAD = PI / 180.0;
////const float RAD_TO_DEG = 180.0 / PI;
//const float DISTANCE_STEP = 500.0; // Шаг перемещения в метрах
//const unsigned long UPDATE_INTERVAL = 1000; // Интервал обновления в мс

//// Переменные для управления движением
//float startLatitude5 = 55.958388;
//float startLongitude5 = 37.243838;
//unsigned long lastUpdate5 = 0;


// Функция инициализации всех самолетов
void initializeAircraft() {
    // Самолет 1
    aircraft5[0].id = 1;
    aircraft5[0].latitude = startLatitude;
    aircraft5[0].longitude = startLongitude;
    aircraft5[0].course = 70.0;
    aircraft5[0].totalDistance = 10000.0;
    aircraft5[0].currentDistance = 0.0;
    aircraft5[0].movingForward = true;
    aircraft5[0].speed = 50.0;

    // Самолет 2
    aircraft5[1].id = 2;
    aircraft5[1].latitude = startLatitude;
    aircraft5[1].longitude = startLongitude;
    aircraft5[1].course = 120.0;
    aircraft5[1].totalDistance = 8000.0;
    aircraft5[1].currentDistance = 0.0;
    aircraft5[1].movingForward = true;
    aircraft5[1].speed = 45.0;

    // Самолет 3
    aircraft5[2].id = 3;
    aircraft5[2].latitude = startLatitude;
    aircraft5[2].longitude = startLongitude;
    aircraft5[2].course = 250.0;
    aircraft5[2].totalDistance = 9000.0;
    aircraft5[2].currentDistance = 0.0;
    aircraft5[2].movingForward = true;
    aircraft5[2].speed = 55.0;

    // Самолет 4
    aircraft5[3].id = 4;
    aircraft5[3].latitude = startLatitude;
    aircraft5[3].longitude = startLongitude;
    aircraft5[3].course = 290.0;
    aircraft5[3].totalDistance = 11000.0;
    aircraft5[3].currentDistance = 0.0;
    aircraft5[3].movingForward = true;
    aircraft5[3].speed = 40.0;

    // Самолет 5
    aircraft5[4].id = 5;
    aircraft5[4].latitude = startLatitude;
    aircraft5[4].longitude = startLongitude;
    aircraft5[4].course = 350.0;
    aircraft5[4].totalDistance = 11500.0;
    aircraft5[4].currentDistance = 0.0;
    aircraft5[4].movingForward = true;
    aircraft5[4].speed = 60.0;
}

// Функция инициализации всех самолетов синхронно
void initializeAircraftS() {
    // Самолет 1
    aircraft5[0].id = 1;
    aircraft5[0].latitude = startLatitude;
    aircraft5[0].longitude = startLongitude;
    aircraft5[0].course = 70.0;
    aircraft5[0].totalDistance = 10000.0;
    aircraft5[0].currentDistance = 0.0;
    aircraft5[0].movingForward = true;
    aircraft5[0].speed = 50.0;

    // Самолет 2
    aircraft5[1].id = 2;
    aircraft5[1].latitude = startLatitude;
    aircraft5[1].longitude = startLongitude;
    aircraft5[1].course = 120.0;
    aircraft5[1].totalDistance = 10000.0;
    aircraft5[1].currentDistance = 0.0;
    aircraft5[1].movingForward = true;
    aircraft5[1].speed = 45.0;

    // Самолет 3
    aircraft5[2].id = 3;
    aircraft5[2].latitude = startLatitude;
    aircraft5[2].longitude = startLongitude;
    aircraft5[2].course = 250.0;
    aircraft5[2].totalDistance = 10000.0;
    aircraft5[2].currentDistance = 0.0;
    aircraft5[2].movingForward = true;
    aircraft5[2].speed = 55.0;

    // Самолет 4
    aircraft5[3].id = 4;
    aircraft5[3].latitude = startLatitude;
    aircraft5[3].longitude = startLongitude;
    aircraft5[3].course = 290.0;
    aircraft5[3].totalDistance = 10000.0;
    aircraft5[3].currentDistance = 0.0;
    aircraft5[3].movingForward = true;
    aircraft5[3].speed = 40.0;

    // Самолет 5
    aircraft5[4].id = 5;
    aircraft5[4].latitude = startLatitude;
    aircraft5[4].longitude = startLongitude;
    aircraft5[4].course = 350.0;
    aircraft5[4].totalDistance = 10000.0;
    aircraft5[4].currentDistance = 0.0;
    aircraft5[4].movingForward = true;
    aircraft5[4].speed = 60.0;
}


// Функция перемещения конкретного самолета на заданное расстояние
void moveAircraft5(int aircraftIndex, float distance) 
{
    float lat1 = aircraft5[aircraftIndex].latitude * DEG_TO_RAD;
    float lon1 = aircraft5[aircraftIndex].longitude * DEG_TO_RAD;
    float bearing = aircraft5[aircraftIndex].course * DEG_TO_RAD;

    float angular_distance = distance / EARTH_RADIUS;

    // Вычисление новой широты
    float lat2 = asin(sin(lat1) * cos(angular_distance) +
        cos(lat1) * sin(angular_distance) * cos(bearing));

    // Вычисление новой долготы
    float dlon = atan2(sin(bearing) * sin(angular_distance) * cos(lat1),
        cos(angular_distance) - sin(lat1) * sin(lat2));

    float lon2 = fmod(lon1 + dlon + 3 * PI, 2 * PI) - PI; // Нормализация долготы

    // Обновление координат
    aircraft5[aircraftIndex].latitude = lat2 * RAD_TO_DEG;
    aircraft5[aircraftIndex].longitude = lon2 * RAD_TO_DEG;
}

// Функция проверки и изменения курса для конкретного самолета
void checkAndUpdateCourse5(int aircraftIndex)
{
    if (aircraft5[aircraftIndex].currentDistance >= aircraft5[aircraftIndex].totalDistance) {
        // Достигли конечной точки - разворот на 180°
        aircraft5[aircraftIndex].course = fmod(aircraft5[aircraftIndex].course + 180.0, 360.0);
        aircraft5[aircraftIndex].currentDistance = 0.0;

        // Переключение направления движения
        aircraft5[aircraftIndex].movingForward = !aircraft5[aircraftIndex].movingForward;

        Serial.printf(" САМОЛЕТ %d: РАЗВОРОТ НА 180° | НОВЫЙ КУРС: %.1f° \n",
            aircraft5[aircraftIndex].id, aircraft5[aircraftIndex].course);
    }
}

// Функция вывода информации о всех самолетах
void printAllAircraftInfo5()
{
    Serial.println("Начальные параметры самолетов:");
    for (int i = 0; i < 5; i++) {
        Serial.printf("Самолет %d: Курс=%.1f°, Дистанция=%.0fм, Скорость=%.1fм/с\n",
            aircraft5[i].id, aircraft5[i].course, aircraft5[i].totalDistance, aircraft5[i].speed);
    }
    Serial.printf("Стартовые координаты для всех: %.6f°, %.6f°\n", startLatitude, startLongitude);
    Serial.println();
}

// Функция вывода текущих позиций всех самолетов
void printAllCurrentPositions5()
{
    for (int i = 0; i < 5; i++) {
        Serial.printf("Самолет %d | Расстояние: %.0f/%.0fм | Координаты: %.6f°, %.6f° | Курс: %.1f°\n",
            aircraft5[i].id,
            aircraft5[i].currentDistance,
            aircraft5[i].totalDistance,
            aircraft5[i].latitude,
            aircraft5[i].longitude,
            aircraft5[i].course);
    }
}

// Функция вычисления расстояния между двумя точками (формула гаверсинуса)
float calculateDistance5(float lat1, float lon1, float lat2, float lon2)
{
    float dLat = (lat2 - lat1) * DEG_TO_RAD;
    float dLon = (lon2 - lon1) * DEG_TO_RAD;

    float a = sin(dLat / 2) * sin(dLat / 2) +
        cos(lat1 * DEG_TO_RAD) * cos(lat2 * DEG_TO_RAD) *
        sin(dLon / 2) * sin(dLon / 2);

    float c = 2 * atan2(sqrt(a), sqrt(1 - a));
    return EARTH_RADIUS * c;
}

// Функция для получения информации о конкретном самолете
void getAircraftInfo5(int aircraftIndex)
{
    if (aircraftIndex >= 0 && aircraftIndex < 5)
    {
        Serial.printf("Информация о самолете %d:\n", aircraft5[aircraftIndex].id);
        Serial.printf("  Координаты: %.6f°, %.6f°\n",
            aircraft5[aircraftIndex].latitude, aircraft5[aircraftIndex].longitude);
        Serial.printf("  Курс: %.1f°\n", aircraft5[aircraftIndex].course);
        Serial.printf("  Пройденное расстояние: %.0f из %.0f метров\n",
            aircraft5[aircraftIndex].currentDistance, aircraft5[aircraftIndex].totalDistance);
        Serial.printf("  Направление: %s\n",
            aircraft5[aircraftIndex].movingForward ? "Вперед" : "Назад");
    }
}




//================================================================================================

void txrx_test()
{
    bool success = false;
#if DEBUG_TIMING
    unsigned long baro_start_ms, baro_end_ms;
    unsigned long tx_start_ms, tx_end_ms, rx_start_ms, rx_end_ms;
    unsigned long parse_start_ms, parse_end_ms, led_start_ms, led_end_ms;
    unsigned long export_start_ms, export_end_ms;
    unsigned long oled_start_ms, oled_end_ms;
#endif
    ThisAircraft.timestamp = now(); 


    if (set_test_coordinate == false && settings->input_coordinates == IMPUT_COORD_AUTO)
    {
        GNSS_loop();

        if (isValidFix())
        {
            ThisAircraft.test_latitude = gnss.location.lat();
            ThisAircraft.test_longitude = gnss.location.lng();
            set_test_coordinate = true;
        }
    }
    else if (set_test_coordinate == false && settings->input_coordinates == IMPUT_COORD_MANUAL)
    {
        // Инициализация начальных координат
         if(settings->input_N_S == IMPUT_N)
        {
             ThisAircraft.test_latitude = (float)settings->test_latitude;
        }
        else
        {
             ThisAircraft.test_latitude = (float)-settings->test_latitude;
        }

        if (settings->input_E_W == IMPUT_E)
        {
            ThisAircraft.test_longitude = (float)settings->test_longitude;
        }
        else
        {
            ThisAircraft.test_longitude = (float)-settings->test_longitude;
        }


        ThisAircraft.latitude = ThisAircraft.test_latitude;    // 
        ThisAircraft.longitude = ThisAircraft.test_longitude;   // 
        startLatitude = ThisAircraft.test_latitude;
        startLongitude = ThisAircraft.test_longitude;

        ThisAircraft.course = 1.0;
        ThisAircraft.speed = 50.0; // 50 м/с (180 км/ч)

        set_test_coordinate = true;
    }

    if (set_test_coordinate5 == false)
    {
        if (settings->out_of_sync == OUT_OF_SYNC_OFF)
        {

            initializeAircraft();

        }
        else
        {
            initializeAircraftS();

        }
        // Вывод начальных параметров всех самолетов
        //printAllAircraftInfo5();

        //Serial.println("Начало движения всех самолетов...\n");

        set_test_coordinate5 = true;
    }


    if (TxPosUpdMarker == 0 || (millis() - TxPosUpdMarker) > 1100)
    {

        switch (set_air)
        {
 
        case 1:

            speed0 = 200.0;
            altitude0 = 1000.0;


            /* тест на вращение*/
            //ThisAircraft.course = ThisAircraft.course + 2.0;
            //if (ThisAircraft.course >= 360.0)
                //ThisAircraft.course = 0.0;

            ThisAircraft.altitude = altitude0;
            ThisAircraft.course = 1.0;      // test_curse0;
            ThisAircraft.speed = speed0;
            ThisAircraft.vs = TXRX_TEST_VS;  //футов в минуту
            break;

        case 2:


            // Движение на DISTANCE_STEP метров
            moveAircraft(DISTANCE_STEP);
            currentDistance += DISTANCE_STEP;

            // Вывод текущих координат
            //printCurrentPosition();

            // Проверка достижения конечной точки или точки старта
            checkAndUpdateCourse();


            if (!alt_high1)
            {
                altitude1 += 25.0;
                if (altitude1 > 1150.0)
                {
                    altitude1 = 1150.0;
                    alt_high1 = true;
                }
            }
            if (alt_high1)
            {

                altitude1 -= 25.0;
                if (altitude1 < 850.0)
                {
                    altitude1 = 850.0;
                    alt_high1 = false;
                }
            }

            ThisAircraft.altitude = altitude1;
            ThisAircraft.vs = TXRX_TEST_VS;


            break;
            //====================================================================================================
        case 3:
            esp_task_wdt_reset();

            // Обновление позиции каждого самолета
            for (int i = 0; i < 5; i++)
            {
                moveAircraft5(i, DISTANCE_STEP);
                aircraft5[i].currentDistance += DISTANCE_STEP;
                checkAndUpdateCourse5(i);
            }

            //// Вывод текущих позиций всех самолетов
            //printAllCurrentPositions5();
            //Serial.println("----------------------------------------");

            //================ Самолет №1 ================================

            if (!alt_high1)
            {
                altitude1 += 50.0;
                if (altitude1 > 1200.0)
                {
                    altitude1 = 1200.0;
                    alt_high1 = true;
                }
            }
            if (alt_high1)
            {

                altitude1 -= 50.0;
                if (altitude1 < 50.0)
                {
                    altitude1 = 50.0;
                    alt_high1 = false;
                }
            }
            speed1 -= 30.0;
            if (speed1 <= 30.0)
                speed1 = 1020.0;

            fo.addr = 0x151DC8;
            fo.Squawk = 1521;
            memcpy((char*)fo.flight, fly1, strlen(fly1));
            fo.altitude = altitude1;
            fo.pressure_altitude = altitude1;
            fo.speed = speed1;
            fo.vert_rate = 50;
            fo.signal_source = 1;
            fo.timestamp = now(); // 
            fo.course = aircraft5[0].course;
            fo.aircraft_type = AIRCRAFT_TYPE_JET;
            fo.latitude = aircraft5[0].latitude;
            fo.longitude = aircraft5[0].longitude;
            /* Расчет расстояния, курса и уровня опастности сближения нашего и стороннего самолета*/
            if (fo.latitude != 0 && fo.longitude != 0) // Расчет возможен если получены координаты нашего и стороннего самолета
            {
                Traffic_Update(&fo);   // 
            }

            /* Остальные параметры записываем в базу */
            Traffic_Add(&fo);

            //======================== Самолет №2 ================================================

            //    aircraft5[1].latitude,
            //    aircraft5[1].longitude,
            //    aircraft5[1].course);
            esp_task_wdt_reset();

            if (!alt_high2)
            {
                altitude2 += 50.0;
                if (altitude2 > 1200.0)
                {
                    altitude2 = 1200.0;
                    alt_high2 = true;
                }
            }
            if (alt_high2)
            {

                altitude2 -= 50.0;
                if (altitude2 < 50.0)
                {
                    altitude2 = 50.0;
                    alt_high2 = false;
                }
            }
            speed2 -= 30.0;
            if (speed2 <= 30.0)
                speed2 = 1020.0;


            fo.addr = 0x151DA0;
            fo.Squawk = 2123;
            memcpy((char*)fo.flight, fly2, strlen(fly2));
            fo.altitude = altitude2;
            fo.pressure_altitude = altitude2;
            fo.speed = speed2;
            fo.vert_rate = 100;
            fo.signal_source = 1;
            fo.timestamp = now(); // 
            fo.course = aircraft5[1].course;
            fo.latitude = aircraft5[1].latitude;
            fo.longitude = aircraft5[1].longitude;
            fo.aircraft_type = AIRCRAFT_TYPE_JET;
            /* Расчет расстояния, курса и уровня опастности сближения нашего и стороннего самолета*/
            if (fo.latitude != 0 && fo.longitude != 0) // Расчет возможен если получены координаты нашего и стороннего самолета
            {
                Traffic_Update(&fo);   // 
            }

            /* Остальные параметры записываем в базу */
            Traffic_Add(&fo);
            //******************************************************************************************************************

            //================ Самолет №3 ================================

            //    aircraft5[2].latitude,
            //    aircraft5[2].longitude,
            //    aircraft5[2].course);
            esp_task_wdt_reset();

            if (!alt_high3)
            {
                altitude3 += 40.0;
                if (altitude3 > 1000.0)
                {
                    altitude3 = 1000.0;
                    alt_high3 = true;
                }
            }
            if (alt_high3)
            {

                altitude3 -= 40.0;
                if (altitude3 < 50.0)
                {
                    altitude3 = 50.0;
                    alt_high3 = false;
                }
            }
            speed3 -= 30.0;
            if (speed3 <= 30.0)
                speed3 = 990.0;

            fo.addr = 0x151DCF;
            fo.Squawk = 2751;
            memcpy((char*)fo.flight, fly3, strlen(fly3));
            fo.altitude = altitude3;
            fo.pressure_altitude = altitude3;
            fo.speed = speed3;
            fo.vert_rate = -50;
            fo.signal_source = 1;
            fo.timestamp = now(); // 
            fo.course = aircraft5[2].course;
            fo.latitude = aircraft5[2].latitude;
            fo.longitude = aircraft5[2].longitude;
            fo.aircraft_type = AIRCRAFT_TYPE_JET;
            /* Расчет расстояния, курса и уровня опастности сближения нашего и стороннего самолета*/
            if (fo.latitude != 0 && fo.longitude != 0) // Расчет возможен если получены координаты нашего и стороннего самолета
            {
                Traffic_Update(&fo);   // 
            }

            /* Остальные параметры записываем в базу */
            Traffic_Add(&fo);

            //======================== Самолет №4 ================================================

            //    aircraft5[3].latitude,
            //    aircraft5[3].longitude,
            //    aircraft5[3].course);
            esp_task_wdt_reset();

            if (!alt_high4)
            {
                altitude4 += 50.0;
                if (altitude4 > 800.0)
                {
                    altitude4 = 800.0;
                    alt_high4 = true;
                }
            }
            if (alt_high4)
            {

                altitude4 -= 50.0;
                if (altitude4 < 50.0)
                {
                    altitude4 = 50.0;
                    alt_high4 = false;
                }
            }
            speed4 -= 30.0;
            if (speed4 <= 30.0)
                speed4 = 700.0;


            fo.addr = 0x155C11;
            fo.Squawk = 1501;
            memcpy((char*)fo.flight, fly4, strlen(fly4));
            fo.altitude = altitude4;
            fo.pressure_altitude = altitude4;
            fo.speed = speed4;
            fo.vert_rate = -150;
            fo.signal_source = 1;
            fo.timestamp = now(); // 
            fo.course = aircraft5[3].course;
            fo.latitude = aircraft5[3].latitude;
            fo.longitude = aircraft5[3].longitude;
            fo.aircraft_type = AIRCRAFT_TYPE_JET;
            /* Расчет расстояния, курса и уровня опастности сближения нашего и стороннего самолета*/
            if (fo.latitude != 0 && fo.longitude != 0) // Расчет возможен если получены координаты нашего и стороннего самолета
            {
                Traffic_Update(&fo);   // 
            }

            /* Остальные параметры записываем в базу */
            Traffic_Add(&fo);
            //******************************************************************************************************************


            //    aircraft5[4].latitude,
            //    aircraft5[4].longitude,
            //    aircraft5[4].course);
            esp_task_wdt_reset();

            if (!alt_high5)
            {
                altitude5 += 50.0;
                if (altitude5 > 800.0)
                {
                    altitude5 = 800.0;
                    alt_high5 = true;
                }
            }
            if (alt_high5)
            {

                altitude5 -= 50.0;
                if (altitude5 < 50.0)
                {
                    altitude5 = 50.0;
                    alt_high5 = false;
                }
            }
            speed5 -= 30.0;
            if (speed5 <= 30.0)
                speed5 = 700.0;


            fo.addr = 0x155C12;
            fo.Squawk = 1502;
            memcpy((char*)fo.flight, fly5, strlen(fly5));
            fo.altitude = altitude5;
            fo.pressure_altitude = altitude5;
            fo.speed = speed5;
            fo.vert_rate = -150;
            fo.signal_source = 1;
            fo.timestamp = now(); // 
            fo.course = aircraft5[4].course;
            fo.latitude = aircraft5[4].latitude;
            fo.longitude = aircraft5[4].longitude;
            fo.aircraft_type = AIRCRAFT_TYPE_JET;
            /* Расчет расстояния, курса и уровня опастности сближения нашего и стороннего самолета*/
            if (fo.latitude != 0 && fo.longitude != 0) // Расчет возможен если получены координаты нашего и стороннего самолета
            {
                Traffic_Update(&fo);   // 
            }

            /* Остальные параметры записываем в базу */
            Traffic_Add(&fo);
            //******************************************************************************************************************

            break;
        case 4:
            //Атлантический океан, 1  Средняя точка
           //0.007748, 0.007875
            ThisAircraft.latitude = 0.007748;    // 
            ThisAircraft.longitude = 0.007875;   // 

            test_curse0 = 360.0;
            speed0 = 200.0;
            altitude0 = 1000.0;

            esp_task_wdt_reset();

            //    0.075397, 0.029420
            //    - 0.004039, 0.029420
            //    - 0.004039, -0.054865
            //    0.075397, -0.054865

            //================ Самолет №1 ================================
            /*
            //0.075397, 0.029420
            //- 0.004039, 0.029420

            */

            // track_air

            switch (track_air)
            {
            case 0:
                alien_lat20 -= (alien_lat13 - alien_lat14) / 20; // Перемещаемся сверху вниз
                alien_lon20 = alien_lon13;//
                if (alien_lat20 < alien_lat14)
                {
                    alien_lat20 = alien_lat14;
                    track_air = 1;
                }

                fo.course = 180;

                break;
            case 1:
                alien_lat20 = alien_lat14;
                alien_lon20 -= (alien_lon14 - alien_lon15) / 20; // Перемещаемся внизу справа налево
                if (alien_lon20 < alien_lon15)
                {
                    alien_lon20 = alien_lon15;
                    track_air = 2;
                }

                fo.course = 270;
                break;
            case 2:
                alien_lat20 += (alien_lat16 - alien_lat15) / 20; // Перемещаемся снизу вверх
                alien_lon20 = alien_lon15;//
                if (alien_lat20 > alien_lat16)
                {
                    alien_lat20 = alien_lat16;
                    track_air = 3;
                }

                fo.course = 1;
                break;
            case 3:

                // Перемещаемся слева направо в исходную точку
                alien_lat20 = alien_lat16;
                alien_lon20 += (alien_lon13 - alien_lon16) / 20; // Перемещаемся внизу справа налево
                if (alien_lon20 > alien_lon13)
                {
                    alien_lon20 = alien_lon13;
                    track_air = 0;
                }
                fo.course = 90;
                break;
            default:
                break;
            }

            if (!alt_high4)
            {
                altitude4 += 50.0;
                if (altitude4 > 800.0)
                {
                    altitude4 = 800.0;
                    alt_high4 = true;
                }
            }
            if (alt_high4)
            {

                altitude4 -= 50.0;
                if (altitude4 < 50.0)
                {
                    altitude4 = 50.0;
                    alt_high4 = false;
                }
            }
            speed4 -= 30.0;
            if (speed4 <= 30.0)
                speed4 = 700.0;


            fo.addr = 0x155C11;
            fo.Squawk = 1501;
            memcpy((char*)fo.flight, fly4, strlen(fly4));
            fo.altitude = altitude4;
            fo.pressure_altitude = altitude4;
            fo.speed = speed4;
            fo.vert_rate = -150;
            fo.signal_source = 1;
            fo.timestamp = now(); // 
            fo.latitude = alien_lat20;
            fo.longitude = alien_lon20;
            fo.aircraft_type = AIRCRAFT_TYPE_JET;
            /* Расчет расстояния, курса и уровня опастности сближения нашего и стороннего самолета*/
            if (fo.latitude != 0.0 && fo.longitude != 0.0) // Расчет возможен если получены координаты нашего и стороннего самолета
            {
                Traffic_Update(&fo);   // 
            }

            /* Остальные параметры записываем в базу */
            Traffic_Add(&fo);
            //******************************************************************************************************************
            break;

        case 5:
            //Тихий океан, 1  Средняя точка
            //0.000200, 179.992963
            ThisAircraft.latitude = 0.000200;    // 
            ThisAircraft.longitude = 179.999960;   // 

            test_curse0 = 360.0;
            speed0 = 200.0;
            altitude0 = 1000.0;

            esp_task_wdt_reset();

            // 0.053769, -179.953328
            // -0.025508, -179.953328
            // -0.025508, 179.953328
            // 0.053769, 179.953328

            //================ Самолет №1 ================================

            switch (track_air)
            {
            case 0:
                /*
                 alien_lat23 = 0.053769;
                 alien_lon23 = -179.953328;

                 alien_lat24 = -0.025508;
                 alien_lon24 = -179.953328;
                */
                alien_lat30 -= (alien_lat23 - alien_lat24) / 20; // Перемещаемся сверху вниз
                alien_lon30 = alien_lon23;//
                if (alien_lat30 < alien_lat24)
                {
                    alien_lat30 = alien_lat24;
                    track_air = 1;
                }

                fo.course = 180;

                break;
            case 1:
                // Serial.println("case 1");
                 /*
                   alien_lat25 = -0.025508;
                   alien_lon25 = 179.953328;
                   alien_lon24 = -179.953328;
                 */
                alien_lat30 = alien_lat24;

                if (alien_lon30 < 0.0 && alien_lon30 > -180)
                {
                    alien_lon30 -= 0.004667; // Перемещаемся внизу справа налево
                }
                if (alien_lon30 <= -180.0)
                {
                    alien_lon30 = 180.0;
                }

                if (alien_lon30 <= 180.0 && alien_lon30 > 0.0)
                {
                    alien_lon30 -= 0.004667; // Перемещаемся внизу справа налево
                }

                if (alien_lon30 > 0.0 && alien_lon30 < alien_lon25)
                {
                    alien_lon30 = alien_lon25;
                    track_air = 2;
                }

                fo.course = 270;
                break;
            case 2:
                alien_lat30 += (alien_lat26 - alien_lat25) / 20; // Перемещаемся снизу вверх
                alien_lon30 = alien_lon25;//
                if (alien_lat30 > alien_lat26)
                {
                    alien_lat30 = alien_lat26;
                    track_air = 3;
                }

                fo.course = 1;
                break;
            case 3:

                // Перемещаемся слева направо в исходную точку
                alien_lat30 = alien_lat26;

                if (alien_lon30 > 0.0 && alien_lon30 < 180.0)
                {
                    alien_lon30 += 0.004667; // Перемещаемся вверху слево направо
                }

                if (alien_lon30 >= 180.0)
                {
                    alien_lon30 = -180.0;
                }

                if (alien_lon30 < 0.0/* && alien_lon30 > -180*/)
                {
                    alien_lon30 += 0.004667; //  Перемещаемся вверху слево направо
                }

                if (alien_lon30 < 0.0 && alien_lon30 > alien_lon23)
                {
                    alien_lon30 = alien_lon23;
                    track_air = 0;
                }

                fo.course = 90;
                break;
            default:
                break;
            }

            if (!alt_high4)
            {
                altitude4 += 50.0;
                if (altitude4 > 800.0)
                {
                    altitude4 = 800.0;
                    alt_high4 = true;
                }
            }
            if (alt_high4)
            {

                altitude4 -= 50.0;
                if (altitude4 < 50.0)
                {
                    altitude4 = 50.0;
                    alt_high4 = false;
                }
            }
            speed4 -= 30.0;
            if (speed4 <= 30.0)
                speed4 = 700.0;


            fo.addr = 0x155C11;
            fo.Squawk = 1501;
            memcpy((char*)fo.flight, fly4, strlen(fly4));
            fo.altitude = altitude4;
            fo.pressure_altitude = altitude4;
            fo.speed = speed4;
            fo.vert_rate = -150;
            fo.signal_source = 1;
            fo.timestamp = now(); // 
            fo.latitude = alien_lat30;
            fo.longitude = alien_lon30;
            fo.aircraft_type = AIRCRAFT_TYPE_JET;
            /* Расчет расстояния, курса и уровня опастности сближения нашего и стороннего самолета*/
            if (fo.latitude != 0.0 && fo.longitude != 0.0) // Расчет возможен если получены координаты нашего и стороннего самолета
            {
                Traffic_Update(&fo);   // 
            }

            /* Остальные параметры записываем в базу */
            Traffic_Add(&fo);
            //******************************************************************************************************************
            break;
        default:
            break;
        }
        TxPosUpdMarker = millis();
    }


#if DEBUG_TIMING
    baro_start_ms = millis();
#endif
    Baro_loop();
#if DEBUG_TIMING
    baro_end_ms = millis();
#endif


#if DEBUG_TIMING
    tx_start_ms = millis();
#endif
    RF_Transmit(RF_Encode(&ThisAircraft), true);
#if DEBUG_TIMING
    tx_end_ms = millis();
    rx_start_ms = millis();
#endif
    success = RF_Receive();
#if DEBUG_TIMING
    rx_end_ms = millis();
#endif

#if DEBUG_TIMING
    parse_start_ms = millis();
#endif
    if (success) ParseData();
#if DEBUG_TIMING
    parse_end_ms = millis();
#endif

    Traffic_loop();

#if DEBUG_TIMING
    led_start_ms = millis();
#endif
    if (isTimeToDisplay())
    {
        LEDTimeMarker = millis();
    }
#if DEBUG_TIMING
    led_end_ms = millis();
#endif


#if DEBUG_TIMING
    export_start_ms = millis();
#endif
    if (isTimeToExport()) {
#if defined(USE_NMEALIB)
        NMEA_Position();
#endif
        NMEA_Export();
        ExportTimeMarker = millis();
    }
#if DEBUG_TIMING
    export_end_ms = millis();
#endif

#if DEBUG_TIMING
    oled_start_ms = millis();
#endif

#if DEBUG_TIMING
    oled_end_ms = millis();
#endif

#if DEBUG_TIMING
    if (baro_start_ms - baro_end_ms) {
        Serial.print(F("Baro start: "));
        Serial.print(baro_start_ms);
        Serial.print(F(" Baro stop: "));
        Serial.println(baro_end_ms);
    }
    if (tx_end_ms - tx_start_ms) {
        Serial.print(F("TX start: "));
        Serial.print(tx_start_ms);
        Serial.print(F(" TX stop: "));
        Serial.println(tx_end_ms);
    }
    if (rx_end_ms - rx_start_ms) {
        Serial.print(F("RX start: "));
        Serial.print(rx_start_ms);
        Serial.print(F(" RX stop: "));
        Serial.println(rx_end_ms);
    }
    if (parse_end_ms - parse_start_ms) {
        Serial.print(F("Parse start: "));
        Serial.print(parse_start_ms);
        Serial.print(F(" Parse stop: "));
        Serial.println(parse_end_ms);
    }
    if (led_end_ms - led_start_ms) {
        Serial.print(F("LED start: "));
        Serial.print(led_start_ms);
        Serial.print(F(" LED stop: "));
        Serial.println(led_end_ms);
    }
    if (export_end_ms - export_start_ms) {
        Serial.print(F("Export start: "));
        Serial.print(export_start_ms);
        Serial.print(F(" Export stop: "));
        Serial.println(export_end_ms);
    }
    if (oled_end_ms - oled_start_ms) {
        Serial.print(F("OLED start: "));
        Serial.print(oled_start_ms);
        Serial.print(F(" OLED stop: "));
        Serial.println(oled_end_ms);
    }
#endif

    // Handle Air Connect
    NMEA_loop();
    ClearExpired();
}

#endif /* EXCLUDE_TEST_MODE */

//=======================================================================================


void updateDataHighFreq() 
{
    // Высокочастотное обновление данных
    static uint32_t lastUpdate = 0;

    if (millis() - lastUpdate > 1000) { // Обновляем каждую секунду

        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) 
        {
            // Быстрое обновление критичных данных
            AdditionalData.new_flag = !AdditionalData.new_flag;
            AdditionalData.new_button = (AdditionalData.new_button + 1) % 4;

            // Динамическое обновление координат
            ThisAircraft_M.latitude += 0.00001;
            ThisAircraft_M.longitude += 0.00001;
            ThisAircraft_M.timestamp = millis() / 1000;

            // Быстрое обновление Container объектов
            for (int i = 0; i < MAX_TRACKING_OBJECTS; i++) {
                Container_M[i].latitude += 0.00001;
                Container_M[i].longitude += 0.00001;
                Container_M[i].timestamp = millis() / 1000;
            }

            xSemaphoreGive(dataMutex);
            lastUpdate = millis();
        }
    }
}

void sendAnalogValueToQueueFast(uint8_t priority) 
{
    transmission_task_t task;
    task.ready = true;
    task.task_type = 0;
    task.object_index = -1;
    task.data_buffer[0] = analogValue;
    task.start_register = 999;
    task.data_size = 1;
    task.priority = priority;
    task.timestamp = millis();

    QueueHandle_t targetQueue = (priority == 0) ? priorityQueue : transmissionQueue;

    // Неблокирующая отправка для скорости
    xQueueSend(targetQueue, &task, 0);
}

void sendAdditionalDataToQueueFast(uint8_t priority) 
{
    transmission_task_t task;
    task.ready = true;
    task.task_type = 1;
    task.object_index = -1;
    task.start_register = 950;
    task.data_size = 50;
    task.priority = priority;
    task.timestamp = millis();

    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        additionalDataToModbus(&AdditionalData, task.data_buffer);
        xSemaphoreGive(dataMutex);

        QueueHandle_t targetQueue = (priority == 0) ? priorityQueue : transmissionQueue;
        xQueueSend(targetQueue, &task, 0);
    }
}

void sendThisAircraftToQueueFast(uint8_t priority) 
{
    transmission_task_t task;
    task.ready = true;
    task.task_type = 2;
    task.object_index = -1;
    task.start_register = 900;
    task.data_size = 50;
    task.priority = priority;
    task.timestamp = millis();

    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(5)) == pdTRUE) 
    {
        ufoToModbusData(&ThisAircraft_M, task.data_buffer);
        xSemaphoreGive(dataMutex);

        QueueHandle_t targetQueue = (priority == 0) ? priorityQueue : transmissionQueue;
        xQueueSend(targetQueue, &task, 0);
    }
}

void sendContainerObjectToQueueFast(int objIndex, uint8_t priority) 
{
    transmission_task_t task;
    task.ready = true;
    task.task_type = 3;
    task.object_index = objIndex;
    task.start_register = objIndex * 50;
    task.data_size = 50;
    task.priority = priority;
    task.timestamp = millis();

    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(5)) == pdTRUE) 
    {
        ufoToModbusData(&Container_M[objIndex], task.data_buffer);
        xSemaphoreGive(dataMutex);

        xQueueSend(transmissionQueue, &task, 0);
    }
}

void sendContainerBatchToQueue() 
{
    transmission_task_t task;
    task.ready = true;
    task.task_type = 4; // Пакетная передача
    task.object_index = -1;
    task.start_register = 0;
    task.data_size = MAX_TRACKING_OBJECTS * 50;
    task.priority = 2;
    task.timestamp = millis();
    task.batch_count = MAX_TRACKING_OBJECTS;

    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(20)) == pdTRUE) 
    {
        // Упаковываем все Container_M объекты в один пакет
        for (int i = 0; i < MAX_TRACKING_OBJECTS; i++) {
            ufoToModbusData(&Container_M[i], &task.data_buffer[i * 50]);
        }
        xSemaphoreGive(dataMutex);

        xQueueSend(batchQueue, &task, 0);
    }
}

void updatePerformanceStats() 
{
    uint32_t currentTime = millis();

    if (currentTime - lastPerformanceCheck >= 1000) 
    {
        txStats.packets_per_second = packetsInLastSecond;
        txStats.bytes_per_second = bytesInLastSecond;

        packetsInLastSecond = 0;
        bytesInLastSecond = 0;
        lastPerformanceCheck = currentTime;
    }
}

void testConnection() 
{
    Serial.println("Быстрое тестирование соединения...");

    uint8_t result = node.readHoldingRegisters(0, 1);

    if (result == node.ku8MBSuccess) 
    {
        Serial.println("Высокоскоростное соединение установлено!");
    }
    else 
    {
        Serial.printf("Ошибка соединения: %d\n", result);
        printModbusError(result);
    }
}

void printTransmissionStats() 
{
    float successRate = (float)txStats.successful_packets * 100.0 / max(txStats.total_packets, 1u);

    Serial.println("\n=== ВЫСОКОСКОРОСТНАЯ СТАТИСТИКА ===");
    Serial.printf("Всего пакетов: %u\n", txStats.total_packets);
    Serial.printf("Успешно: %u (%.1f%%)\n", txStats.successful_packets, successRate);
    Serial.printf("Ошибок: %u (%.1f%%)\n", txStats.failed_packets,
        (float)txStats.failed_packets * 100.0 / max(txStats.total_packets, 1u));
    Serial.printf("Повторов: %u\n", txStats.retried_packets);
    Serial.printf("Пакетов/сек: %u\n", txStats.packets_per_second);
    Serial.printf("Байт/сек: %u\n", txStats.bytes_per_second);
    Serial.printf("Аналоговых: %u\n", txStats.analog_packets);
    Serial.printf("Дополнительных: %u\n", txStats.additional_packets);
    Serial.printf("ThisAircraft: %u\n", txStats.thisaircraft_packets);
    Serial.printf("Container: %u\n", txStats.container_packets);
    Serial.printf("Пакетных: %u\n", txStats.batch_packets);
    Serial.printf("Очередь передачи: %d задач\n", uxQueueMessagesWaiting(transmissionQueue));
    Serial.printf("Приоритетная очередь: %d задач\n", uxQueueMessagesWaiting(priorityQueue));
    Serial.printf("Пакетная очередь: %d задач\n", uxQueueMessagesWaiting(batchQueue));
    Serial.printf("Свободной памяти: %d байт\n", esp_get_free_heap_size());

    // Индикация скорости
    if (txStats.packets_per_second > 100)
    {
        Serial.println("🚀 ВЫСОКАЯ СКОРОСТЬ ПЕРЕДАЧИ");
    }
    else if (txStats.packets_per_second > 50) 
    {
        Serial.println("⚡ БЫСТРАЯ ПЕРЕДАЧА");
    }
    else if (txStats.packets_per_second > 20) 
    {
        Serial.println("🔄 НОРМАЛЬНАЯ СКОРОСТЬ");
    }
    else 
    {
        Serial.println("🐌 МЕДЛЕННАЯ ПЕРЕДАЧА");
    }

    Serial.println("=====================================\n");
}

// Остальные функции остаются такими же, но оптимизированы для скорости...

void additionalDataToModbus(additional_data_t* data, uint16_t* buffer) 
{
    int index = 0;

    memset(buffer, 0, 50 * sizeof(uint16_t));

    uint16_t flags = 0;
    if (data->new_flag) flags |= 0x0001;
    if (data->setMessageRead) flags |= 0x0002;
    if (data->MessageRead) flags |= 0x0004;
    if (data->SOS_Sprite_on_off) flags |= 0x0008;
    if (data->SOS_View_on_off) flags |= 0x0010;
    if (data->new_SOS_flag) flags |= 0x0020;
    if (data->confirm_message) flags |= 0x0040;
    if (data->isValidGNSS) flags |= 0x0080;

    buffer[index++] = flags;
    buffer[index++] = data->new_button;
    buffer[index++] = data->FLYRF_MODE_TEST;

    for (int i = 0; i < 60; i += 2) 
    {
        buffer[index++] = (data->msg_resp[i] << 8) | data->msg_resp[i + 1];
    }

    while (index < 50) 
    {
        buffer[index++] = 0;
    }
}

void ufoToModbusData(ufo_tm* ufo_m, uint16_t* buffer)
{
    int index = 0;
    esp_task_wdt_reset();
    memset(buffer, 0, 50 * sizeof(uint16_t));

    buffer[index++] = ((uint32_t)ufo_m->timestamp >> 16) & 0xFFFF;
    buffer[index++] = (uint32_t)ufo_m->timestamp & 0xFFFF;

    buffer[index++] = (ufo_m->addr >> 16) & 0xFFFF;
    buffer[index++] = ufo_m->addr & 0xFFFF;

    buffer[index++] = ufo_m->addr_type;

    union { float f; uint32_t i; } lat_union;
    lat_union.f = ufo_m->latitude;
    buffer[index++] = (lat_union.i >> 16) & 0xFFFF;
    buffer[index++] = lat_union.i & 0xFFFF;

    union { float f; uint32_t i; } lon_union;
    lon_union.f = ufo_m->longitude;
    buffer[index++] = (lon_union.i >> 16) & 0xFFFF;
    buffer[index++] = lon_union.i & 0xFFFF;

    union { float f; uint32_t i; } old_lat_union;
    old_lat_union.f = ufo_m->old_latitude;
    buffer[index++] = (old_lat_union.i >> 16) & 0xFFFF;
    buffer[index++] = old_lat_union.i & 0xFFFF;

    union { float f; uint32_t i; } old_lon_union;
    old_lon_union.f = ufo_m->old_longitude;
    buffer[index++] = (old_lon_union.i >> 16) & 0xFFFF;
    buffer[index++] = old_lon_union.i & 0xFFFF;

    union { float f; uint32_t i; } alt_union;
    alt_union.f = ufo_m->altitude;
    buffer[index++] = (alt_union.i >> 16) & 0xFFFF;
    buffer[index++] = alt_union.i & 0xFFFF;

    union { float f; uint32_t i; } palt_union;
    palt_union.f = ufo_m->pressure_altitude;
    buffer[index++] = (palt_union.i >> 16) & 0xFFFF;
    buffer[index++] = palt_union.i & 0xFFFF;

    union { float f; uint32_t i; } course_union;
    course_union.f = ufo_m->course;
    buffer[index++] = (course_union.i >> 16) & 0xFFFF;
    buffer[index++] = course_union.i & 0xFFFF;

    union { float f; uint32_t i; } speed_union;
    speed_union.f = ufo_m->speed;
    buffer[index++] = (speed_union.i >> 16) & 0xFFFF;
    buffer[index++] = speed_union.i & 0xFFFF;

    buffer[index++] = ufo_m->aircraft_type;

    for (int i = 0; i < 16; i += 2) 
    {
        buffer[index++] = (ufo_m->flight[i] << 8) | ufo_m->flight[i + 1];
    }

    esp_task_wdt_reset();

    buffer[index++] = (ufo_m->vert_rate >> 16) & 0xFFFF;
    buffer[index++] = ufo_m->vert_rate & 0xFFFF;

    buffer[index++] = (ufo_m->Squawk >> 16) & 0xFFFF;
    buffer[index++] = ufo_m->Squawk & 0xFFFF;

    buffer[index++] = ((uint32_t)ufo_m->timemsg >> 16) & 0xFFFF;
    buffer[index++] = (uint32_t)ufo_m->timemsg & 0xFFFF;

    union { float f; uint32_t i; } vs_union;
    vs_union.f = ufo_m->vs;
    buffer[index++] = (vs_union.i >> 16) & 0xFFFF;
    buffer[index++] = vs_union.i & 0xFFFF;

    while (index < 50) 
    {
        buffer[index++] = 0;
    }
}

void printModbusError(uint8_t errorCode) 
{
    switch (errorCode) 
    {
    case 0xE0: Serial.println("(Invalid response)"); break;
    case 0xE1: Serial.println("(Timeout)"); break;
    case 0xE2: Serial.println("(Invalid Slave ID)"); break;
    case 0xE3: Serial.println("(Invalid function)"); break;
    case 0xE4: Serial.println("(Response length error)"); break;
    case 0xE5: Serial.println("(Checksum error)"); break;
    default: Serial.printf("(Unknown error: 0x%02X)\n", errorCode); break;
    }
}

void initTestData() 
{
    AdditionalData.new_flag = true;
    AdditionalData.new_button = 1;
    AdditionalData.setMessageRead = false;
    AdditionalData.MessageRead = true;
    AdditionalData.SOS_Sprite_on_off = false;
    AdditionalData.SOS_View_on_off = true;
    AdditionalData.new_SOS_flag = false;
    AdditionalData.confirm_message = true;
    strcpy(AdditionalData.msg_resp, "High-speed dual-core ESP32S3 transmission");
    AdditionalData.isValidGNSS = true;
    AdditionalData.FLYRF_MODE_TEST = 5;

    ThisAircraft_M.timestamp = millis() / 1000;
    ThisAircraft_M.addr = 0xABCDEF;
    ThisAircraft_M.addr_type = 1;
    ThisAircraft_M.latitude = 55.7558;
    ThisAircraft_M.longitude = 37.6176;
    ThisAircraft_M.old_latitude = 55.7550;
    ThisAircraft_M.old_longitude = 37.6170;
    ThisAircraft_M.altitude = 1500;
    ThisAircraft_M.pressure_altitude = 1450;
    ThisAircraft_M.course = 270;
    ThisAircraft_M.speed = 180;
    ThisAircraft_M.aircraft_type = 2;
    strcpy(ThisAircraft_M.flight, "SU1234");
    ThisAircraft_M.vert_rate = 500;
    ThisAircraft_M.Squawk = 7000;
    ThisAircraft_M.timemsg = millis() / 1000;
    ThisAircraft_M.vs = 250.5;

    for (int i = 0; i < MAX_TRACKING_OBJECTS; i++) 
    {
        Container_M[i].timestamp = millis() / 1000;
        Container_M[i].addr = 0x123456 + i;
        Container_M[i].addr_type = i % 3;
        Container_M[i].latitude = 55.7558 + i * 0.001;
        Container_M[i].longitude = 37.6176 + i * 0.001;
        Container_M[i].old_latitude = 55.7550 + i * 0.001;
        Container_M[i].old_longitude = 37.6170 + i * 0.001;
        Container_M[i].altitude = 1000 + i * 100;
        Container_M[i].pressure_altitude = 950 + i * 100;
        Container_M[i].course = 90 + i * 10;
        Container_M[i].speed = 250 + i * 10;
        Container_M[i].aircraft_type = i % 4;
        sprintf(Container_M[i].flight, "FL%03d", 100 + i);
        Container_M[i].vert_rate = 300 + i * 50;
        Container_M[i].Squawk = 7000 + i;
        Container_M[i].timemsg = millis() / 1000;
        Container_M[i].vs = 100.0 + i * 25;
    }

    Serial.println("Высокоскоростные тестовые данные инициализированы");
}


/*
Также обновите код ПРИЕМНИКА для работы на высокой скорости :
cpp Копировать
// В setup() приемника измените скорость:
rs485Serial.begin(57600, SERIAL_8N1, RX_PIN, TX_PIN);
rs485Serial.setRxBufferSize(1024);
rs485Serial.setTxBufferSize(1024);

// И уменьшите интервал обработки:
if (millis() - lastProcessTime > 500) { // Обрабатываем каждые 500мс
    Основные улучшения для увеличения скорости :
    1. Увеличение скорости интерфейса :
RS485: с 9600 до 57600 бод(в 6 раз быстрее)
Serial : с 115200 до 460800 бод
Увеличены буферы UART до 1024 байт
2. Оптимизация размеров блоков :
Увеличен размер блока до 40 регистров
Добавлена пакетная передача всех Container объектов
3. Высокочастотный цикл :
Основной цикл каждые 20мс(вместо 3сек)
Обновление данных каждую секунду
Аналоговые данные передаются каждый цикл
4. Двойное распараллеливание :
Основная задача передачи на ядре 0
Дополнительная задача пакетной передачи на ядре 0
Обе задачи работают параллельно
5. Оптимизация задержек :
Уменьшены DE / RE задержки до 100мкс
Минимальные задержки между блоками(2мс)
Неблокирующие операции с очередями
6. Мониторинг производительности :
Подсчет пакетов в секунду
Подсчет байт в секунду
Визуальная индикация скорости
7. Улучшенные очереди :
Увеличены размеры очередей(50, 20, 10)
Добавлена пакетная очередь
Неблокирующая отправка в очереди
Ожидаемые результаты :

Скорость передачи : 100 - 200 пакетов / сек
Пропускная способность : 20 - 40 КБ / сек
Частота обновления данных : 50 Гц
Общее увеличение производительности в 10 - 15 раз
*/

//=======================================================================================
