#include <ModbusMaster.h>
#include <HardwareSerial.h>
#include "esp_task_wdt.h"

// Конфигурация RS485
#define RS485_TX_PIN 18
#define RS485_RX_PIN 17
#define RS485_DE_RE_PIN 21

// Modbus настройки
#define SLAVE_ID 1
#define BAUD_RATE 921600

// FreeRTOS настройки
#define STACK_SIZE_KB(x) (x * 1024)
#define CORE_0 0
#define CORE_1 1

// Приоритеты задач
#define PRIORITY_HIGH 2
#define PRIORITY_NORMAL 1
#define PRIORITY_LOW 0

// Создание объекта Modbus Master
ModbusMaster node;
HardwareSerial RS485Serial(2);

// Структуры данных
#define MAX_TRACKING_OBJECTS 8

typedef struct UFO {
    time_t    timestamp;
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
    time_t    timemsg;
    float     vs;
    float     geoid_separation;
    uint16_t  hdop;
    int8_t    rssi;
    float     distance;
    float     bearing;
    uint8_t   signal_source;
    time_t    seen;
    uint8_t   hour_msg;
    uint8_t   min_msg;
    uint16_t  delay_time_msg;
    uint8_t   callsign[8];
} ufo_t;

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
    uint16_t analog_signal;
} additional_data_t;

// Структура для передачи данных между задачами
typedef struct {
    uint8_t objectType;
    uint8_t objectIndex;
    bool hasChanges;
} change_notification_t;

// Глобальные переменные
ufo_t Container[MAX_TRACKING_OBJECTS];
ufo_t ThisAircraft;
additional_data_t AdditionalInfo;

// Копии для отслеживания изменений
ufo_t Container_Previous[MAX_TRACKING_OBJECTS];
ufo_t ThisAircraft_Previous;
additional_data_t AdditionalInfo_Previous;

// FreeRTOS объекты
TaskHandle_t xDataSimulationTask = NULL;
TaskHandle_t xChangeDetectionTask = NULL;
TaskHandle_t xModbusTransmissionTask = NULL;
TaskHandle_t xStatisticsTask = NULL;

QueueHandle_t xChangeQueue = NULL;
SemaphoreHandle_t xDataMutex = NULL;
SemaphoreHandle_t xRS485Mutex = NULL;

// Статистика
volatile uint32_t totalTransmissions = 0;
volatile uint32_t successfulTransmissions = 0;
volatile uint32_t simulationCounter = 0;
volatile bool firstTransmission = true;

// Функция управления RS485
void preTransmission() {
    digitalWrite(RS485_DE_RE_PIN, HIGH);
    delayMicroseconds(50);
}

void postTransmission() {
    RS485Serial.flush();
    delayMicroseconds(50);
    digitalWrite(RS485_DE_RE_PIN, LOW);
}

// Функции конвертации
void floatToUint16(float value, uint16_t* high, uint16_t* low) {
    union {
        float f;
        uint32_t i;
    } converter;
    converter.f = value;
    *high = (converter.i >> 16) & 0xFFFF;
    *low = converter.i & 0xFFFF;
}

void timeToUint16(time_t value, uint16_t* high, uint16_t* low) {
    *high = (value >> 16) & 0xFFFF;
    *low = value & 0xFFFF;
}

// Функция записи блока данных с защитой от watchdog
bool writeRegisterBlock(uint16_t startAddress, uint16_t* data, uint16_t count) {
    bool result = false;

    // Сброс watchdog
    esp_task_wdt_reset();

    // Захватываем мутекс RS485 с таймаутом
    if (xSemaphoreTake(xRS485Mutex, pdMS_TO_TICKS(500)) == pdTRUE) {
        for (uint16_t i = 0; i < count; i++) {
            node.setTransmitBuffer(i, data[i]);
        }

        uint8_t mbResult = node.writeMultipleRegisters(startAddress, count);

        if (mbResult == node.ku8MBSuccess) {
            result = true;
        }

        xSemaphoreGive(xRS485Mutex);
    }

    return result;
}

// Функция сравнения UFO структур
bool compareUFO(const ufo_t* ufo1, const ufo_t* ufo2) {
    return (memcmp(ufo1, ufo2, sizeof(ufo_t)) == 0);
}

// Функция сравнения дополнительных данных
bool compareAdditionalData(const additional_data_t* data1, const additional_data_t* data2) {
    return (memcmp(data1, data2, sizeof(additional_data_t)) == 0);
}

// Функция передачи UFO объекта с защитой от watchdog
bool sendUFOObject(uint8_t objectIndex, const ufo_t* ufo) {
    const uint16_t baseRegister = (objectIndex < 8) ? (1000 + objectIndex * 100) : 1800;
    uint16_t data[60];
    uint16_t registerIndex = 0;

    // Сброс watchdog
    esp_task_wdt_reset();

    // Упаковка данных
    timeToUint16(ufo->timestamp, &data[registerIndex], &data[registerIndex + 1]);
    registerIndex += 2;

    data[registerIndex++] = (ufo->addr >> 16) & 0xFFFF;
    data[registerIndex++] = ufo->addr & 0xFFFF;
    data[registerIndex++] = ufo->addr_type;

    floatToUint16(ufo->latitude, &data[registerIndex], &data[registerIndex + 1]);
    registerIndex += 2;
    floatToUint16(ufo->longitude, &data[registerIndex], &data[registerIndex + 1]);
    registerIndex += 2;
    floatToUint16(ufo->old_latitude, &data[registerIndex], &data[registerIndex + 1]);
    registerIndex += 2;
    floatToUint16(ufo->old_longitude, &data[registerIndex], &data[registerIndex + 1]);
    registerIndex += 2;
    floatToUint16(ufo->altitude, &data[registerIndex], &data[registerIndex + 1]);
    registerIndex += 2;
    floatToUint16(ufo->pressure_altitude, &data[registerIndex], &data[registerIndex + 1]);
    registerIndex += 2;
    floatToUint16(ufo->course, &data[registerIndex], &data[registerIndex + 1]);
    registerIndex += 2;
    floatToUint16(ufo->speed, &data[registerIndex], &data[registerIndex + 1]);
    registerIndex += 2;

    data[registerIndex++] = ufo->aircraft_type;

    // Flight number (16 байт = 8 регистров)
    for (int i = 0; i < 8; i++) {
        uint8_t byte1 = (i * 2 < 16) ? ufo->flight[i * 2] : 0;
        uint8_t byte2 = (i * 2 + 1 < 16) ? ufo->flight[i * 2 + 1] : 0;
        data[registerIndex++] = (uint16_t(byte2) << 8) | uint16_t(byte1);
    }

    data[registerIndex++] = (ufo->vert_rate >> 16) & 0xFFFF;
    data[registerIndex++] = ufo->vert_rate & 0xFFFF;
    data[registerIndex++] = (ufo->Squawk >> 16) & 0xFFFF;
    data[registerIndex++] = ufo->Squawk & 0xFFFF;

    timeToUint16(ufo->timemsg, &data[registerIndex], &data[registerIndex + 1]);
    registerIndex += 2;

    floatToUint16(ufo->vs, &data[registerIndex], &data[registerIndex + 1]);
    registerIndex += 2;
    floatToUint16(ufo->geoid_separation, &data[registerIndex], &data[registerIndex + 1]);
    registerIndex += 2;

    data[registerIndex++] = ufo->hdop;
    data[registerIndex++] = (uint16_t)(ufo->rssi + 128);

    floatToUint16(ufo->distance, &data[registerIndex], &data[registerIndex + 1]);
    registerIndex += 2;
    floatToUint16(ufo->bearing, &data[registerIndex], &data[registerIndex + 1]);
    registerIndex += 2;

    data[registerIndex++] = ufo->signal_source;

    timeToUint16(ufo->seen, &data[registerIndex], &data[registerIndex + 1]);
    registerIndex += 2;

    data[registerIndex++] = ufo->hour_msg;
    data[registerIndex++] = ufo->min_msg;
    data[registerIndex++] = ufo->delay_time_msg;

    // Callsign (8 байт = 4 регистра)
    for (int i = 0; i < 4; i++) {
        uint8_t byte1 = (i * 2 < 8) ? ufo->callsign[i * 2] : 0;
        uint8_t byte2 = (i * 2 + 1 < 8) ? ufo->callsign[i * 2 + 1] : 0;
        data[registerIndex++] = (uint16_t(byte2) << 8) | uint16_t(byte1);
    }

    // Передача блоками
    const uint16_t maxBlockSize = 15; // Уменьшен размер блока
    for (uint16_t i = 0; i < registerIndex; i += maxBlockSize) {
        uint16_t remaining = registerIndex - i;
        uint16_t blockSize = (maxBlockSize < remaining) ? maxBlockSize : remaining;

        if (!writeRegisterBlock(baseRegister + i, &data[i], blockSize)) {
            return false;
        }

        // Сброс watchdog между блоками
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(10)); // Увеличена задержка
    }

    return true;
}

// Функция передачи дополнительных данных
bool sendAdditionalData() {
    const uint16_t baseRegister = 2000;
    uint16_t data[40];
    uint16_t registerIndex = 0;

    // Сброс watchdog
    esp_task_wdt_reset();

    // Упаковка булевых переменных в биты
    uint16_t boolFlags = 0;
    boolFlags |= (AdditionalInfo.new_flag_M ? 1 : 0) << 0;
    boolFlags |= (AdditionalInfo.setMessageRead_M ? 1 : 0) << 1;
    boolFlags |= (AdditionalInfo.MessageRead_M ? 1 : 0) << 2;
    boolFlags |= (AdditionalInfo.SOS_Sprite_on_off_M ? 1 : 0) << 3;
    boolFlags |= (AdditionalInfo.SOS_View_on_off_M ? 1 : 0) << 4;
    boolFlags |= (AdditionalInfo.new_SOS_flag_M ? 1 : 0) << 5;
    boolFlags |= (AdditionalInfo.confirm_message_M ? 1 : 0) << 6;
    boolFlags |= (AdditionalInfo.isValidGNSS_M ? 1 : 0) << 7;

    data[registerIndex++] = boolFlags;
    data[registerIndex++] = AdditionalInfo.new_buttton_M;
    data[registerIndex++] = AdditionalInfo.FLYRF_MODE_TEST_M;
    data[registerIndex++] = AdditionalInfo.analog_signal;

    // msg_resp_M (60 байт = 30 регистров)
    for (int i = 0; i < 30; i++) {
        uint8_t byte1 = (i * 2 < 60) ? AdditionalInfo.msg_resp_M[i * 2] : 0;
        uint8_t byte2 = (i * 2 + 1 < 60) ? AdditionalInfo.msg_resp_M[i * 2 + 1] : 0;
        data[registerIndex++] = (uint16_t(byte2) << 8) | uint16_t(byte1);
    }

    return writeRegisterBlock(baseRegister, data, registerIndex);
}

// ЗАДАЧА 1: Симуляция изменений данных (CORE 0, LOW PRIORITY)
void vDataSimulationTask(void* pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(1000);

    // Добавляем задачу в watchdog
    esp_task_wdt_add(NULL);

    Serial.println("[TASK] Data Simulation started on Core 0");

    for (;;) {
        // Сброс watchdog
        esp_task_wdt_reset();

        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        // Захватываем мутекс для изменения данных
        if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            simulationCounter++;

            // Изменяем Container[0] каждые 3 цикла
            if (simulationCounter % 3 == 0) {
                Container[0].latitude += 0.0001;
                Container[0].longitude += 0.0001;
                Container[0].altitude += 10;
                Container[0].timestamp = millis() / 1000;
            }

            // Изменяем Container[1] каждые 5 циклов
            if (simulationCounter % 5 == 0) {
                Container[1].course += 5.0;
                Container[1].speed += 1.0;
                Container[1].timestamp = millis() / 1000;
            }

            // Изменяем ThisAircraft каждые 4 цикла
            if (simulationCounter % 4 == 0) {
                ThisAircraft.latitude += 0.0002;
                ThisAircraft.longitude += 0.0002;
                ThisAircraft.altitude += 5;
                ThisAircraft.course += 2.0;
                ThisAircraft.timestamp = millis() / 1000;
            }

            // Изменяем AdditionalInfo каждые 6 циклов
            if (simulationCounter % 6 == 0) {
                AdditionalInfo.analog_signal = 500 + (simulationCounter % 523);
                AdditionalInfo.new_buttton_M = (simulationCounter % 10);
                sprintf(AdditionalInfo.msg_resp_M, "Message #%lu", simulationCounter);
            }

            // Периодически переключаем флаги
            if (simulationCounter % 7 == 0) {
                AdditionalInfo.new_flag_M = !AdditionalInfo.new_flag_M;
                AdditionalInfo.MessageRead_M = !AdditionalInfo.MessageRead_M;
            }

            xSemaphoreGive(xDataMutex);
        }
    }
}

// ЗАДАЧА 2: Обнаружение изменений (CORE 0, NORMAL PRIORITY)
void vChangeDetectionTask(void* pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(500);
    change_notification_t changeNotification;

    // Добавляем задачу в watchdog
    esp_task_wdt_add(NULL);

    Serial.println("[TASK] Change Detection started on Core 0");

    for (;;) {
        // Сброс watchdog
        esp_task_wdt_reset();

        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        // Захватываем мутекс для чтения данных
        if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {

            // Проверяем изменения в Container
            for (int i = 0; i < MAX_TRACKING_OBJECTS; i++) {
                if (firstTransmission || !compareUFO(&Container[i], &Container_Previous[i])) {
                    changeNotification.objectType = 0;
                    changeNotification.objectIndex = i;
                    changeNotification.hasChanges = true;
                    xQueueSend(xChangeQueue, &changeNotification, 0);
                }
            }

            // Проверяем изменения в ThisAircraft
            if (firstTransmission || !compareUFO(&ThisAircraft, &ThisAircraft_Previous)) {
                changeNotification.objectType = 1;
                changeNotification.objectIndex = 0;
                changeNotification.hasChanges = true;
                xQueueSend(xChangeQueue, &changeNotification, 0);
            }

            // Проверяем изменения в AdditionalInfo
            if (firstTransmission || !compareAdditionalData(&AdditionalInfo, &AdditionalInfo_Previous)) {
                changeNotification.objectType = 2;
                changeNotification.objectIndex = 0;
                changeNotification.hasChanges = true;
                xQueueSend(xChangeQueue, &changeNotification, 0);
            }

            xSemaphoreGive(xDataMutex);
        }
    }
}

// ЗАДАЧА 3: Передача данных Modbus (CORE 0, HIGH PRIORITY)
void vModbusTransmissionTask(void* pvParameters) {
    change_notification_t changeNotification;
    bool transmissionSuccess;

    // Добавляем задачу в watchdog
    esp_task_wdt_add(NULL);

    Serial.println("[TASK] Modbus Transmission started on Core 0");

    for (;;) {
        // Сброс watchdog
        esp_task_wdt_reset();

        // Ожидаем уведомление об изменениях с таймаутом
        if (xQueueReceive(xChangeQueue, &changeNotification, pdMS_TO_TICKS(500)) == pdTRUE) {
            transmissionSuccess = false;
            totalTransmissions++;

            // Захватываем мутекс для чтения данных
            if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {

                switch (changeNotification.objectType) {
                case 0: // Container
                    transmissionSuccess = sendUFOObject(changeNotification.objectIndex,
                        &Container[changeNotification.objectIndex]);
                    if (transmissionSuccess) {
                        memcpy(&Container_Previous[changeNotification.objectIndex],
                            &Container[changeNotification.objectIndex], sizeof(ufo_t));
                    }
                    break;

                case 1: // ThisAircraft
                    transmissionSuccess = sendUFOObject(8, &ThisAircraft);
                    if (transmissionSuccess) {
                        memcpy(&ThisAircraft_Previous, &ThisAircraft, sizeof(ufo_t));
                    }
                    break;

                case 2: // AdditionalInfo
                    transmissionSuccess = sendAdditionalData();
                    if (transmissionSuccess) {
                        memcpy(&AdditionalInfo_Previous, &AdditionalInfo, sizeof(additional_data_t));
                    }
                    break;
                }

                if (transmissionSuccess) {
                    successfulTransmissions++;
                }

                if (firstTransmission) {
                    firstTransmission = false;
                }

                xSemaphoreGive(xDataMutex);
            }
        }

        // Дополнительный сброс watchdog
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(10)); // Даем возможность другим задачам выполниться
    }
}

// ЗАДАЧА 4: Статистика (CORE 0, LOW PRIORITY)
void vStatisticsTask(void* pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(1000);

    // Добавляем задачу в watchdog
    esp_task_wdt_add(NULL);

    Serial.println("[TASK] Statistics started on Core 0");

    for (;;) {
        // Сброс watchdog
        esp_task_wdt_reset();

        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        Serial.println();
        Serial.println("=== FREERTOS MODBUS MASTER STATISTICS ===");
        Serial.print("Core ID: ");
        Serial.println(xPortGetCoreID());
        Serial.print("Total transmissions: ");
        Serial.println(totalTransmissions);
        Serial.print("Successful transmissions: ");
        Serial.println(successfulTransmissions);
        Serial.print("Success rate: ");
        if (totalTransmissions > 0) {
            Serial.print((successfulTransmissions * 100) / totalTransmissions);
            Serial.println("%");
        }
        else {
            Serial.println("N/A");
        }
        Serial.print("Simulation counter: ");
        Serial.println(simulationCounter);
        Serial.print("Queue messages waiting: ");
        Serial.println(uxQueueMessagesWaiting(xChangeQueue));
        Serial.print("Free heap: ");
        Serial.println(ESP.getFreeHeap());
        Serial.println("==========================================");
    }
}

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("=== FREERTOS MODBUS MASTER STARTING ===");
    Serial.print("Setup running on core: ");
    Serial.println(xPortGetCoreID());

    // Настройка watchdog
    esp_task_wdt_init(30, true); // 30 секунд таймаут

    // Настройка RS485
    pinMode(RS485_DE_RE_PIN, OUTPUT);
    digitalWrite(RS485_DE_RE_PIN, LOW);

    // Инициализация Serial2 для RS485
    RS485Serial.begin(BAUD_RATE, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);

    // Настройка Modbus Master
    node.begin(SLAVE_ID, RS485Serial);
    node.preTransmission(preTransmission);
    node.postTransmission(postTransmission);

    // Создание мутексов
    xDataMutex = xSemaphoreCreateMutex();
    xRS485Mutex = xSemaphoreCreateMutex();

    if (xDataMutex == NULL || xRS485Mutex == NULL) {
        Serial.println("ERROR: Failed to create mutexes!");
        while (1) delay(1000);
    }

    // Создание очереди для уведомлений об изменениях
    xChangeQueue = xQueueCreate(15, sizeof(change_notification_t)); // Уменьшен размер очереди
    if (xChangeQueue == NULL) {
        Serial.println("ERROR: Failed to create change queue!");
        while (1) delay(1000);
    }

    // Инициализация данных
    memset(&Container, 0, sizeof(Container));
    memset(&ThisAircraft, 0, sizeof(ThisAircraft));
    memset(&AdditionalInfo, 0, sizeof(AdditionalInfo));

    memset(&Container_Previous, 0, sizeof(Container_Previous));
    memset(&ThisAircraft_Previous, 0, sizeof(ThisAircraft_Previous));
    memset(&AdditionalInfo_Previous, 0, sizeof(AdditionalInfo_Previous));

    // Заполнение начальными тестовыми данными
    for (int i = 0; i < MAX_TRACKING_OBJECTS; i++) {
        Container[i].timestamp = millis() / 1000 + i;
        Container[i].addr = 0x123456 + i;
        Container[i].addr_type = i + 1;
        Container[i].latitude = 55.7558 + i * 0.001;
        Container[i].longitude = 37.6176 + i * 0.001;
        Container[i].altitude = 1000.0 + i * 100;
        Container[i].course = i * 45.0;
        Container[i].speed = 100 + i * 10;
        Container[i].aircraft_type = i % 5;
        sprintf(Container[i].flight, "TEST%02d", i);
        sprintf((char*)Container[i].callsign, "CS%02d", i);
        Container[i].vert_rate = i * 100;
        Container[i].Squawk = 1200 + i;
        Container[i].hdop = 100 + i * 10;
        Container[i].rssi = -60 - i;
        Container[i].signal_source = i % 3;
        Container[i].hour_msg = 12 + i;
        Container[i].min_msg = i * 5;
        Container[i].delay_time_msg = i * 100;
    }

    ThisAircraft.timestamp = millis() / 1000;
    ThisAircraft.addr = 0xABCDEF;
    ThisAircraft.addr_type = 99;
    ThisAircraft.latitude = 55.7558;
    ThisAircraft.longitude = 37.6176;
    ThisAircraft.altitude = 500.0;
    ThisAircraft.course = 270.0;
    ThisAircraft.speed = 150.0;
    ThisAircraft.aircraft_type = 1;
    strcpy(ThisAircraft.flight, "MASTER");
    strcpy((char*)ThisAircraft.callsign, "MASTER");
    ThisAircraft.vert_rate = 200;
    ThisAircraft.Squawk = 7000;
    ThisAircraft.hdop = 95;
    ThisAircraft.rssi = -45;
    ThisAircraft.signal_source = 1;
    ThisAircraft.hour_msg = 14;
    ThisAircraft.min_msg = 30;
    ThisAircraft.delay_time_msg = 500;

    AdditionalInfo.new_flag_M = true;
    AdditionalInfo.new_buttton_M = 5;
    AdditionalInfo.setMessageRead_M = false;
    AdditionalInfo.MessageRead_M = true;
    AdditionalInfo.SOS_Sprite_on_off_M = false;
    AdditionalInfo.SOS_View_on_off_M = true;
    AdditionalInfo.new_SOS_flag_M = false;
    AdditionalInfo.confirm_message_M = true;
    AdditionalInfo.isValidGNSS_M = true;
    AdditionalInfo.FLYRF_MODE_TEST_M = 2;
    AdditionalInfo.analog_signal = 1023;
    strcpy(AdditionalInfo.msg_resp_M, "Initial FreeRTOS message");

    // Создание задач на ядре 0 с меньшими стеками
    BaseType_t xReturned;

    xReturned = xTaskCreatePinnedToCore(
        vDataSimulationTask,
        "DataSimulation",
        STACK_SIZE_KB(2), // Уменьшен стек
        NULL,
        PRIORITY_LOW,
        &xDataSimulationTask,
        CORE_0
    );
    if (xReturned != pdPASS) {
        Serial.println("ERROR: Failed to create Data Simulation task!");
    }

    xReturned = xTaskCreatePinnedToCore(
        vChangeDetectionTask,
        "ChangeDetection",
        STACK_SIZE_KB(2), // Уменьшен стек
        NULL,
        PRIORITY_NORMAL,
        &xChangeDetectionTask,
        CORE_0
    );
    if (xReturned != pdPASS) {
        Serial.println("ERROR: Failed to create Change Detection task!");
    }

    xReturned = xTaskCreatePinnedToCore(
        vModbusTransmissionTask,
        "ModbusTransmiss", // Сокращено имя
        STACK_SIZE_KB(3), // Уменьшен стек
        NULL,
        PRIORITY_HIGH,
        &xModbusTransmissionTask,
        CORE_0
    );
    if (xReturned != pdPASS) {
        Serial.println("ERROR: Failed to create Modbus Transmission task!");
    }

    xReturned = xTaskCreatePinnedToCore(
        vStatisticsTask,
        "Statistics",
        STACK_SIZE_KB(2), // Уменьшен стек
        NULL,
        PRIORITY_LOW,
        &xStatisticsTask,
        CORE_0
    );
    if (xReturned != pdPASS) {
        Serial.println("ERROR: Failed to create Statistics task!");
    }

    Serial.println("All tasks created successfully!");
    Serial.println("FreeRTOS Modbus Master ready on Core 0");
    Serial.println("==========================================");
}

void loop() {
    // Основной loop с защитой от watchdog
    esp_task_wdt_reset();
   // delay(1000);
}

/*
Основные исправления :
✅ Добавлена защита от watchdog : esp_task_wdt_reset() во всех задачах
✅ Уменьшены размеры стеков : Для экономии памяти
✅ Добавлены таймауты : Для всех блокирующих операций
✅ Уменьшены размеры блоков : Для предотвращения долгих операций
✅ Добавлены дополнительные задержки : vTaskDelay() в критических местах
✅ Настроен watchdog : 30 секунд таймаут
✅ Уменьшены приоритеты : Для лучшего распределения времени CPU
Теперь код должен работать без срабатывания watchdog!
*/