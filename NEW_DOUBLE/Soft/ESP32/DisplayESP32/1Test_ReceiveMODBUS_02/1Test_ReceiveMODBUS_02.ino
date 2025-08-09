//Реализация обмена данными между двумя ESP32S3 через RS485 с использованием протокола MODBUS:

//Код для ПРИЕМНИКА (Slave)


//=========================================================================================
#include <ModbusRTU.h>
#include <HardwareSerial.h>

#define MAX_TRACKING_OBJECTS 8
#define DE_RE_PIN 40
#define RX_PIN 38
#define TX_PIN 39
#define BUTTON_S1_PIN 45
#define BUTTON_S2_PIN 48
#define SLAVE_ID 1
#define BAUD_RATE 57600
// Структура данных (та же, что и в источнике)
struct Container {
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

Container container[MAX_TRACKING_OBJECTS];
ModbusRTU mb;
HardwareSerial rs485Serial(1);

// Переменные для кнопок
bool lastS1State = false;
bool lastS2State = false;

void setup() {
    Serial.begin(115200);
    delay(1000);

    // Настройка кнопок
    pinMode(BUTTON_S1_PIN, INPUT_PULLUP);
    pinMode(BUTTON_S2_PIN, INPUT_PULLUP);

    // Настройка RS485
    pinMode(DE_RE_PIN, OUTPUT);
    digitalWrite(DE_RE_PIN, LOW);

    // Инициализация последовательного порта
    rs485Serial.begin(BAUD_RATE, SERIAL_8N1, RX_PIN, TX_PIN);

    // Настройка Modbus Slave
    mb.begin(&rs485Serial, DE_RE_PIN);
    mb.slave(SLAVE_ID);

    // Инициализация регистров (500 регистров)
    mb.addHreg(0, 0, 500);

    Serial.println("Slave ESP32 инициализирован");
    Serial.printf("Slave ID: %d\n", SLAVE_ID);
    Serial.printf("DE/RE Pin: %d\n", DE_RE_PIN);
    Serial.printf("RX Pin: %d, TX Pin: %d\n", RX_PIN, TX_PIN);

    // Инициализация кнопок
    updateButtonStates();
}

void loop() 
{
    // Обработка Modbus запросов
    mb.task();

    // Обновление состояния кнопок
    updateButtonStates();

    // Проверка и обработка полученных данных
    processReceivedData();

    delay(10);
}

void updateButtonStates()
{
    bool currentS1 = !digitalRead(BUTTON_S1_PIN); // Инвертируем из-за INPUT_PULLUP
    bool currentS2 = !digitalRead(BUTTON_S2_PIN);

  
    if (currentS1 != lastS1State || currentS2 != lastS2State) 
    {
       // Serial.printf("Обновление кнопок\n");
        mb.Hreg(451, currentS1 ? 1 : 0); // Регистр 451 для кнопки S1
        mb.Hreg(452, currentS2 ? 1 : 0); // Регистр 452 для кнопки S2

        Serial.printf("Обновление кнопок - S1: %s, S2: %s\n",
            currentS1 ? "Нажата" : "Отпущена",
            currentS2 ? "Нажата" : "Отпущена");

        lastS1State = currentS1;
        lastS2State = currentS2;
    }
}

void processReceivedData() {
    static uint32_t lastProcessTime = 0;
    static uint16_t lastAnalogValue = 0;

    if (millis() - lastProcessTime > 2000) { // Обрабатываем раз в 2 секунды
        // Проверяем аналоговое значение
        uint16_t analogValue = mb.Hreg(450);
        if (analogValue != lastAnalogValue) {
            Serial.printf("Получено аналоговое значение: %d\n", analogValue);
            lastAnalogValue = analogValue;
        }

        // Обрабатываем данные контейнера
        parseContainerData();

        lastProcessTime = millis();
    }
}

void parseContainerData() {
    for (int objIndex = 0; objIndex < MAX_TRACKING_OBJECTS; objIndex++) {
        uint16_t startRegister = objIndex * 50; // По 50 регистров на объект

        // Собираем данные из регистров
        uint16_t dataBuffer[50];
        bool dataReceived = false;

        for (int i = 0; i < 50; i++) {
            dataBuffer[i] = mb.Hreg(startRegister + i);
            if (dataBuffer[i] != 0) {
                dataReceived = true;
            }
        }

        if (dataReceived) {
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

void modbusDataToStruct(uint16_t* buffer, Container* cont) {
    int index = 0;

    // raw[8] - 4 регистра (только первые 8 байт)
    for (int i = 0; i < 8; i += 2) {
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
    for (int i = 0; i < 8; i += 2) {
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
//#include <ModbusRTU.h>
//#include <HardwareSerial.h>
//
//#define MAX_TRACKING_OBJECTS 8
//#define DE_RE_PIN 40
//#define RX_PIN 39
//#define TX_PIN 38
//#define BUTTON_S1_PIN 2
//#define BUTTON_S2_PIN 3
//
//// Структура данных (та же, что и в источнике)
//struct Container {
//    uint8_t   raw[34];
//    uint32_t  timestamp;
//    uint8_t   protocol;
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
//    uint32_t  timemsg;
//    float     vs;
//    bool      stealth;
//    bool      no_track;
//    int8_t    ns[4];
//    int8_t    ew[4];
//    float     geoid_separation;
//    uint16_t  hdop;
//    int8_t    rssi;
//    float     distance;
//    float     bearing;
//    int8_t    alarm_level;
//    uint8_t   signal_source;
//    uint32_t  seen;
//    unsigned int pSignal;
//    uint8_t   hour_msg;
//    uint8_t   min_msg;
//    uint16_t  delay_time_msg;
//    uint8_t   callsign[8];
//    float     test_latitude;
//    float     test_longitude;
//};
//
//Container container[MAX_TRACKING_OBJECTS];
//ModbusRTU mb;
//HardwareSerial rs485Serial(1);
//
//// Переменные для кнопок
//bool lastS1State = false;
//bool lastS2State = false;
//
//void setup() {
//    Serial.begin(115200);
//
//    // Настройка кнопок
//    pinMode(BUTTON_S1_PIN, INPUT_PULLUP);
//    pinMode(BUTTON_S2_PIN, INPUT_PULLUP);
//
//    // Настройка RS485
//    pinMode(DE_RE_PIN, OUTPUT);
//    digitalWrite(DE_RE_PIN, LOW);
//
//    rs485Serial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
//
//    // Настройка Modbus Slave
//    mb.begin(&rs485Serial, DE_RE_PIN);
//    mb.slave(1);
//
//    // Инициализация регистров (500 регистров)
//    mb.addHreg(0, 0, 500);
//
//    Serial.println("Slave ESP32 инициализирован");
//}
//
//void loop() {
//    // Обработка Modbus запросов
//    mb.task();
//
//    // Обновление состояния кнопок
//    updateButtonStates();
//
//    // Проверка и обработка полученных данных
//    processReceivedData();
//
//    delay(10);
//}
//
//void updateButtonStates() {
//    bool currentS1 = !digitalRead(BUTTON_S1_PIN); // Инвертируем из-за INPUT_PULLUP
//    bool currentS2 = !digitalRead(BUTTON_S2_PIN);
//
//    if (currentS1 != lastS1State || currentS2 != lastS2State) {
//        mb.Hreg(451, currentS1 ? 1 : 0); // Регистр 451 для кнопки S1
//        mb.Hreg(452, currentS2 ? 1 : 0); // Регистр 452 для кнопки S2
//
//        Serial.printf("Обновление кнопок - S1: %s, S2: %s\n",
//            currentS1 ? "Нажата" : "Отпущена",
//            currentS2 ? "Нажата" : "Отпущена");
//
//        lastS1State = currentS1;
//        lastS2State = currentS2;
//    }
//}
//
//void processReceivedData() {
//    static uint32_t lastProcessTime = 0;
//
//    if (millis() - lastProcessTime > 1000) { // Обрабатываем раз в секунду
//        // Проверяем аналоговое значение
//        uint16_t analogValue = mb.Hreg(450);
//        if (analogValue > 0) {
//            Serial.printf("Получено аналоговое значение: %d\n", analogValue);
//        }
//
//        // Обрабатываем данные контейнера
//        parseContainerData();
//
//        lastProcessTime = millis();
//    }
//}
//
//void parseContainerData() {
//    for (int objIndex = 0; objIndex < MAX_TRACKING_OBJECTS; objIndex++) {
//        uint16_t startRegister = objIndex * 50; // По 50 регистров на объект
//
//        // Собираем данные из регистров
//        uint16_t dataBuffer[50];
//        bool dataReceived = false;
//
//        for (int i = 0; i < 50; i++) {
//            dataBuffer[i] = mb.Hreg(startRegister + i);
//            if (dataBuffer[i] != 0) {
//                dataReceived = true;
//            }
//        }
//
//        if (dataReceived) {
//            // Конвертируем Modbus данные обратно в структуру
//            modbusDataToStruct(dataBuffer, &container[objIndex]);
//
//            Serial.printf("Объект %d получен - Protocol: %d, Addr: 0x%08X, Lat: %.6f, Lon: %.6f, Flight: %s\n",
//                objIndex,
//                container[objIndex].protocol,
//                container[objIndex].addr,
//                container[objIndex].latitude,
//                container[objIndex].longitude,
//                container[objIndex].flight);
//        }
//    }
//}
//
//void modbusDataToStruct(uint16_t* buffer, Container* cont) {
//    int index = 0;
//
//    // raw[8] - 4 регистра (только первые 8 байт)
//    for (int i = 0; i < 8; i += 2) {
//        cont->raw[i] = (buffer[index] >> 8) & 0xFF;
//        if (i + 1 < 8) {
//            cont->raw[i + 1] = buffer[index] & 0xFF;
//        }
//        index++;
//    }
//
//    // timestamp - 2 регистра
//    cont->timestamp = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
//    index += 2;
//
//    // protocol - 1 регистр
//    cont->protocol = (uint8_t)buffer[index++];
//
//    // addr - 2 регистра
//    cont->addr = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
//    index += 2;
//
//    // addr_type - 1 регистр
//    cont->addr_type = (uint8_t)buffer[index++];
//
//    // latitude - 2 регистра
//    union { float f; uint32_t i; } lat_union;
//    lat_union.i = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
//    cont->latitude = lat_union.f;
//    index += 2;
//
//    // longitude - 2 регистра
//    union { float f; uint32_t i; } lon_union;
//    lon_union.i = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
//    cont->longitude = lon_union.f;
//    index += 2;
//
//    // old_latitude - 2 регистра
//    union { float f; uint32_t i; } old_lat_union;
//    old_lat_union.i = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
//    cont->old_latitude = old_lat_union.f;
//    index += 2;
//
//    // old_longitude - 2 регистра
//    union { float f; uint32_t i; } old_lon_union;
//    old_lon_union.i = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
//    cont->old_longitude = old_lon_union.f;
//    index += 2;
//
//    // altitude - 2 регистра
//    union { float f; uint32_t i; } alt_union;
//    alt_union.i = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
//    cont->altitude = alt_union.f;
//    index += 2;
//
//    // pressure_altitude - 2 регистра
//    union { float f; uint32_t i; } palt_union;
//    palt_union.i = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
//    cont->pressure_altitude = palt_union.f;
//    index += 2;
//
//    // course - 2 регистра
//    union { float f; uint32_t i; } course_union;
//    course_union.i = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
//    cont->course = course_union.f;
//    index += 2;
//
//    // speed - 2 регистра
//    union { float f; uint32_t i; } speed_union;
//    speed_union.i = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
//    cont->speed = speed_union.f;
//    index += 2;
//
//    // aircraft_type - 1 регистр
//    cont->aircraft_type = (uint8_t)buffer[index++];
//
//    // flight[8] - 4 регистра (только первые 8 символов)
//    for (int i = 0; i < 8; i += 2) {
//        cont->flight[i] = (buffer[index] >> 8) & 0xFF;
//        cont->flight[i + 1] = buffer[index] & 0xFF;
//        index++;
//    }
//    // Добавляем завершающий нуль
//    cont->flight[8] = '\0';
//
//    // vert_rate - 2 регистра
//    cont->vert_rate = ((int32_t)buffer[index] << 16) | buffer[index + 1];
//    index += 2;
//
//    // Squawk - 2 регистра
//    cont->Squawk = ((int32_t)buffer[index] << 16) | buffer[index + 1];
//    index += 2;
//
//    // stealth и no_track - 1 регистр
//    if (index < 50) {
//        cont->stealth = (buffer[index] & 0x01) ? true : false;
//        cont->no_track = (buffer[index] & 0x02) ? true : false;
//        index++;
//    }
//
//    // Инициализируем остальные поля значениями по умолчанию
//    cont->timemsg = cont->timestamp;
//    cont->seen = cont->timestamp;
//    cont->vs = 0.0;
//    cont->geoid_separation = 0.0;
//    cont->hdop = 0;
//    cont->rssi = 0;
//    cont->distance = 0.0;
//    cont->bearing = 0.0;
//    cont->alarm_level = 0;
//    cont->signal_source = 0;
//    cont->pSignal = 0;
//    cont->hour_msg = 0;
//    cont->min_msg = 0;
//    cont->delay_time_msg = 0;
//    cont->test_latitude = 0.0;
//    cont->test_longitude = 0.0;
//
//    // Инициализируем массивы
//    for (int i = 8; i < 34; i++) {
//        cont->raw[i] = 0;
//    }
//
//    for (int i = 0; i < 4; i++) {
//        cont->ns[i] = 0;
//        cont->ew[i] = 0;
//    }
//
//    for (int i = 0; i < 8; i++) {
//        cont->callsign[i] = 0;
//    }
//
//    // Заполняем остальную часть flight нулями
//    for (int i = 8; i < 16; i++) {
//        cont->flight[i] = '\0';
//    }
//}
//
///*
//Основные исправления :
//Исправлена сигнатура функции modbusDataToStruct() - теперь правильно принимает указатели
//Исправлена работа с float значениями - используются union для корректного преобразования
//Добавлены явные приведения типов для устранения предупреждений
//Ограничена обработка массивов в соответствии с размером буфера(50 регистров)
//Добавлена инициализация всех полей структуры значениями по умолчанию
//Исправлена работа с массивами - добавлены завершающие нули для строк
//Библиотеки для установки :
//В Arduino IDE установите :
//
//ModbusRTU by Alexander Emelianov
//Карта регистров(обновленная) :
//    0 - 49 : Объект 0
//    50 - 99 : Объект 1
//    100 - 149 : Объект 2
//    150 - 199 : Объект 3
//    200 - 249 : Объект 4
//    250 - 299 : Объект 5
//    300 - 349 : Объект 6
//    350 - 399 : Объект 7
//    450 : Аналоговое значение от источника
//    451 : Состояние кнопки S1
//    452 : Состояние кнопки S2
//    Код теперь должен компилироваться без ошибок для ESP32S3.
//
//    */
////========================================================================================================

//
//#include <ModbusRTU.h>
//#include <HardwareSerial.h>
//
//#define MAX_TRACKING_OBJECTS 8
//#define DE_RE_PIN 40
//#define RX_PIN 39
//#define TX_PIN 38
//#define BUTTON_S1_PIN 2
//#define BUTTON_S2_PIN 3
//
//// Структура данных (та же, что и в источнике)
//struct Container {
//    uint8_t   raw[34];
//    time_t    timestamp;
//    uint8_t   protocol;
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
//    bool      stealth;
//    bool      no_track;
//    int8_t    ns[4];
//    int8_t    ew[4];
//    float     geoid_separation;
//    uint16_t  hdop;
//    int8_t    rssi;
//    float     distance;
//    float     bearing;
//    int8_t    alarm_level;
//    uint8_t   signal_source;
//    time_t    seen;
//    unsigned int pSignal;
//    uint8_t   hour_msg;
//    uint8_t   min_msg;
//    uint16_t  delay_time_msg;
//    uint8_t   callsign[8];
//    float     test_latitude;
//    float     test_longitude;
//};
//
//Container container[MAX_TRACKING_OBJECTS];
//ModbusRTU mb;
//HardwareSerial rs485Serial(1);
//
//// Массив для хранения Modbus регистров
//uint16_t holdingRegs[1000];
//
//// Переменные для кнопок
//bool lastS1State = false;
//bool lastS2State = false;
//
//void setup() {
//    Serial.begin(115200);
//
//    // Настройка кнопок
//    pinMode(BUTTON_S1_PIN, INPUT_PULLUP);
//    pinMode(BUTTON_S2_PIN, INPUT_PULLUP);
//
//    // Настройка RS485
//    pinMode(DE_RE_PIN, OUTPUT);
//    digitalWrite(DE_RE_PIN, LOW);
//
//    rs485Serial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
//
//    // Настройка Modbus Slave
//    mb.begin(&rs485Serial, DE_RE_PIN);
//    mb.slave(1);
//
//    // Инициализация регистров
//    mb.addHreg(0, 0, 1000);
//
//    Serial.println("Slave ESP32 инициализирован");
//}
//
//void loop() {
//    // Обработка Modbus запросов
//    mb.task();
//
//    // Обновление состояния кнопок
//    updateButtonStates();
//
//    // Проверка и обработка полученных данных
//    processReceivedData();
//
//    delay(10);
//}
//
//void updateButtonStates() {
//    bool currentS1 = !digitalRead(BUTTON_S1_PIN); // Инвертируем из-за INPUT_PULLUP
//    bool currentS2 = !digitalRead(BUTTON_S2_PIN);
//
//    if (currentS1 != lastS1State || currentS2 != lastS2State) {
//        mb.Hreg(901, currentS1 ? 1 : 0);
//        mb.Hreg(902, currentS2 ? 1 : 0);
//
//        Serial.printf("Обновление кнопок - S1: %s, S2: %s\n",
//                      currentS1 ? "Нажата" : "Отпущена",
//                      currentS2 ? "Нажата" : "Отпущена");
//
//        lastS1State = currentS1;
//        lastS2State = currentS2;
//    }
//}
//
//void processReceivedData() {
//    static uint32_t lastProcessTime = 0;
//
//    if (millis() - lastProcessTime > 1000) { // Обрабатываем раз в секунду
//        // Проверяем аналоговое значение
//        uint16_t analogValue = mb.Hreg(900);
//        if (analogValue > 0) {
//            Serial.printf("Получено аналоговое значение: %d\n", analogValue);
//        }
//
//        // Обрабатываем данные контейнера
//        parseContainerData();
//
//        lastProcessTime = millis();
//    }
//}
//
//void parseContainerData() {
//    for (int objIndex = 0; objIndex < MAX_TRACKING_OBJECTS; objIndex++) {
//        uint16_t startRegister = objIndex * 100;
//
//        // Собираем данные из регистров
//        uint16_t dataBuffer[100];
//        bool dataReceived = false;
//
//        for (int i = 0; i < 100; i++) {
//            dataBuffer[i] = mb.Hreg(startRegister + i);
//            if (dataBuffer[i] != 0) {
//                dataReceived = true;
//            }
//        }
//
//        if (dataReceived) {
//            // Конвертируем Modbus данные обратно в структуру
//            modbusDataToStruct(dataBuffer, &container[objIndex]);
//
//            Serial.printf("Объект %d получен - Protocol: %d, Addr: 0x%08X, Lat: %.6f, Lon: %.6f, Flight: %s\n",
//                         objIndex,
//                         container[objIndex].protocol,
//                         container[objIndex].addr,
//                         container[objIndex].latitude,
//                         container[objIndex].longitude,
//                         container[objIndex].flight);
//        }
//    }
//}
//
//void modbusDataToStruct(uint16_t buffer, Container cont) {
//    int index = 0;
//
//    // raw[34] - 17 регистров
//    for (int i = 0; i < 34; i += 2) {
//        cont->raw[i] = (buffer[index] >> 8) & 0xFF;
//        if (i + 1 < 34) {
//            cont->raw[i + 1] = buffer[index] & 0xFF;
//        }
//        index++;
//    }
//
//    // timestamp - 2 регистра
//    cont->timestamp = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
//    index += 2;
//
//    // protocol - 1 регистр
//    cont->protocol = buffer[index++];
//
//    // addr - 2 регистра
//    cont->addr = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
//    index += 2;
//
//    // addr_type - 1 регистр
//    cont->addr_type = buffer[index++];
//
//    // Координаты и другие float значения
//    memcpy(&cont->latitude, &buffer[index], 4); index += 2;
//    memcpy(&cont->longitude, &buffer[index], 4); index += 2;
//    memcpy(&cont->old_latitude, &buffer[index], 4); index += 2;
//    memcpy(&cont->old_longitude, &buffer[index], 4); index += 2;
//    memcpy(&cont->altitude, &buffer[index], 4); index += 2;
//    memcpy(&cont->pressure_altitude, &buffer[index], 4); index += 2;
//    memcpy(&cont->course, &buffer[index], 4); index += 2;
//    memcpy(&cont->speed, &buffer[index], 4); index += 2;
//
//    // aircraft_type - 1 регистр
//    cont->aircraft_type = buffer[index++];
//
//    // flight[16] - 8 регистров
//    for (int i = 0; i < 16; i += 2) {
//        cont->flight[i] = (buffer[index] >> 8) & 0xFF;
//        cont->flight[i + 1] = buffer[index] & 0xFF;
//        index++;
//    }
//
//    // Остальные поля можно добавить по аналогии
//}
//
///*
//Библиотеки для установки
//
//Для источника (Master):
//
//ModbusMaster by Doc Walker
//
//
//Для приемника (Slave):
//
//ModbusRTU by Alexander Emelianov
//
//
//Особенности реализации:
//
//Источник (Master):
//   - Отправляет данные всех 8 объектов контейнера
//   - Передает аналоговое значение в регистр 900
//   - Читает состояние кнопок из регистров 901-902
//
//Приемник (Slave):
//   - Принимает данные контейнера в регистры 0-799 (по 100 регистров на объект)
//   - Обновляет состояние кнопок в регистрах 901-902
//   - Принимает аналоговое значение в регистр 900
//
//Карта регистров:
//   - 0-99: Объект 0
//   - 100-199: Объект 1
//   - ...
//   - 700-799: Объект 7
//   - 900: Аналоговое значение
//   - 901: Состояние кнопки S1
//   - 902: Состояние кнопки S2
//
//Код обеспечивает надежную передачу больших объемов данных через RS485 с использованием стандартного протокола MODBUS.
//*/