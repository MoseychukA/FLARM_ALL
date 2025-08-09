
//Код для ИСТОЧНИКА (Master)

#include <ModbusMaster.h>
//#include <HardwareSerial.h>
#include <SoftwareSerial.h>
#define MAX_FRAMEBITS (1 + 8 + 1 + 2)

#define MAX_TRACKING_OBJECTS 8
#define DE_RE_PIN 21
#define RX_PIN 17
#define TX_PIN 18
#define SLAVE_ID 1
#define BAUD_RATE 57600
// Структура данных
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
ModbusMaster node;
//HardwareSerial rs485Serial(1);

// Переменные для аналогового сигнала
const int analogPin = 3;
uint16_t analogValue = 0;

// Переменные для состояния кнопок от приемника
bool button_S1_state = false;
bool button_S2_state = false;

void preTransmission() {
    digitalWrite(DE_RE_PIN, HIGH);
    delayMicroseconds(200); // Небольшая задержка для стабилизации
}

void postTransmission() {
    delayMicroseconds(200); // Ждем завершения передачи
    digitalWrite(DE_RE_PIN, LOW);
}



// constants won't change. Used here to set a pin number:
const int ledPin = 4;// the number of the LED pin

// Variables will change:
int ledState = LOW;             // ledState used to set the LED

// Generally, you should use "unsigned long" for variables that hold time
// The value will quickly become too large for an int to store
unsigned long previousMillis = 0;        // will store last time LED was updated

// constants won't change:
const long interval = 1000;           // interval at which to blink (milliseconds)


EspSoftwareSerial::UART rs485Serial;

// Becomes set from ISR / IRQ callback function.
std::atomic<bool> rxPending(false);

void IRAM_ATTR receiveHandler()
{
    rxPending.store(true);
    //esp_schedule();
}




void setup() {
    Serial.begin(115200);
    delay(1000);

    // Настройка RS485
    pinMode(DE_RE_PIN, OUTPUT);
    digitalWrite(DE_RE_PIN, LOW);
    pinMode(ledPin, OUTPUT);
    // Инициализация последовательного порта
   // rs485Serial.begin(BAUD_RATE, SERIAL_8N1, RX_PIN, TX_PIN);

       // Инициализация RS485
    rs485Serial.begin(BAUD_RATE, EspSoftwareSerial::SWSERIAL_8N1, RX_PIN, TX_PIN);
    // Only half duplex this way, but reliable TX timings for high bps
    rs485Serial.enableIntTx(false);
    rs485Serial.onReceive(receiveHandler);



    // Настройка Modbus
    node.begin(SLAVE_ID, rs485Serial);
    node.preTransmission(preTransmission);
    node.postTransmission(postTransmission);

    Serial.println("Master ESP32 инициализирован");
    Serial.printf("Slave ID: %d\n", SLAVE_ID);
    Serial.printf("DE/RE Pin: %d\n", DE_RE_PIN);
    Serial.printf("RX Pin: %d, TX Pin: %d\n", RX_PIN, TX_PIN);

    //// Инициализация тестовых данных
    //initTestData();

    //// Тестовое соединение
    //delay(2000);
    //testConnection();
}

void loop() 
{
    // Передача данных контейнера (по одному объекту за раз)
    static int currentObject = 0;
    sendSingleContainer(currentObject);
    currentObject = (currentObject + 1) % MAX_TRACKING_OBJECTS;
    // Чтение состояния кнопок
    readButtonStates();
    delay(10);

    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= interval)
    {
        // save the last time you blinked the LED
        previousMillis = currentMillis;

        // if the LED is off turn it on and vice-versa:
        if (ledState == LOW)
        {
            ledState = HIGH;
        }
        else
        {
            ledState = LOW;
        }

        // set the LED with the ledState of the variable:
        digitalWrite(ledPin, ledState);

        // Чтение аналогового сигнала
        analogValue = analogRead(analogPin);

        // Передача аналогового сигнала (проще всего для тестирования)
        sendAnalogValue();
        delay(10);
        //// Передача данных контейнера (по одному объекту за раз)
        //static int currentObject = 0;
        //sendSingleContainer(currentObject);
        //currentObject = (currentObject + 1) % MAX_TRACKING_OBJECTS;
    }
}

void testConnection() {
    Serial.println("Тестирование соединения...");

    // Попытка чтения одного регистра
    uint8_t result = node.readHoldingRegisters(0, 1);

    if (result == node.ku8MBSuccess) {
        Serial.println("Соединение установлено успешно!");
    }
    else {
        Serial.printf("Ошибка соединения: %d\n", result);
        printModbusError(result);
    }
}

void sendSingleContainer(int objIndex) 
{
    uint16_t startRegister = objIndex * 50;

    // Конвертируем структуру в массив uint16_t для передачи
    uint16_t dataBuffer[50];
    structToModbusData(&container[objIndex], dataBuffer);

    // Передача небольшими блоками по 5 регистров
    bool success = true;
    for (int block = 0; block < 10; block++) 
    {
        uint16_t regAddr = startRegister + (block * 5);

        // Подготавливаем данные для передачи
        node.clearTransmitBuffer();
        for (int i = 0; i < 5; i++) {
            node.setTransmitBuffer(i, dataBuffer[block * 5 + i]);
        }

        uint8_t result = node.writeMultipleRegisters(regAddr, 5);

        if (result != node.ku8MBSuccess) 
        {
            Serial.printf("Ошибка передачи объекта %d, блок %d: %d ", objIndex, block, result);
            printModbusError(result);
            success = false;
            break;
        }
        delay(10); // Увеличиваем задержку между блоками
    }

    if (success) 
    {
        Serial.printf("Объект %d передан успешно\n", objIndex);
    }
}

void sendAnalogValue() {
    uint8_t result = node.writeSingleRegister(450, analogValue);
    if (result == node.ku8MBSuccess) {
        Serial.printf("Аналоговое значение передано: %d\n", analogValue);
    }
    else {
        Serial.printf("Ошибка передачи аналогового значения: %d ", result);
        printModbusError(result);
    }
}

void readButtonStates() 
{
    uint8_t result = node.readHoldingRegisters(451, 2);
    if (result == node.ku8MBSuccess) 
    {
        button_S1_state = (bool)node.getResponseBuffer(0);
        button_S2_state = (bool)node.getResponseBuffer(1);
        Serial.printf("Состояние кнопок - S1: %s, S2: %s\n",
            button_S1_state ? "Нажата" : "Отпущена",
            button_S2_state ? "Нажата" : "Отпущена");
    }
    else 
    {
        Serial.printf("Ошибка чтения состояния кнопок: %d ", result);
        printModbusError(result);
    }
}

void printModbusError(uint8_t errorCode) {
    switch (errorCode) {
    case 0xE0: Serial.println("(Invalid response)"); break;
    case 0xE1: Serial.println("(Timeout)"); break;
    case 0xE2: Serial.println("(Invalid Slave ID)"); break;
    case 0xE3: Serial.println("(Invalid function)"); break;
    case 0xE4: Serial.println("(Response length error)"); break;
    case 0xE5: Serial.println("(Checksum error)"); break;
    default: Serial.printf("(Unknown error: 0x%02X)\n", errorCode); break;
    }
}

void structToModbusData(Container* cont, uint16_t* buffer) {
    int index = 0;

    // Очищаем буфер
    memset(buffer, 0, 50 * sizeof(uint16_t));

    // raw[8] - 4 регистра (только первые 8 байт)
    for (int i = 0; i < 8 && index < 50; i += 2) {
        buffer[index++] = (cont->raw[i] << 8) | (i + 1 < 8 ? cont->raw[i + 1] : 0);
    }

    // timestamp - 2 регистра
    if (index < 49) {
        buffer[index++] = (cont->timestamp >> 16) & 0xFFFF;
        buffer[index++] = cont->timestamp & 0xFFFF;
    }

    // protocol - 1 регистр
    if (index < 50) {
        buffer[index++] = cont->protocol;
    }

    // addr - 2 регистра
    if (index < 49) {
        buffer[index++] = (cont->addr >> 16) & 0xFFFF;
        buffer[index++] = cont->addr & 0xFFFF;
    }

    // addr_type - 1 регистр
    if (index < 50) {
        buffer[index++] = cont->addr_type;
    }

    // latitude - 2 регистра
    if (index < 49) {
        union { float f; uint32_t i; } lat_union;
        lat_union.f = cont->latitude;
        buffer[index++] = (lat_union.i >> 16) & 0xFFFF;
        buffer[index++] = lat_union.i & 0xFFFF;
    }

    // longitude - 2 регистра
    if (index < 49) {
        union { float f; uint32_t i; } lon_union;
        lon_union.f = cont->longitude;
        buffer[index++] = (lon_union.i >> 16) & 0xFFFF;
        buffer[index++] = lon_union.i & 0xFFFF;
    }

    // old_latitude - 2 регистра
    if (index < 49) {
        union { float f; uint32_t i; } old_lat_union;
        old_lat_union.f = cont->old_latitude;
        buffer[index++] = (old_lat_union.i >> 16) & 0xFFFF;
        buffer[index++] = old_lat_union.i & 0xFFFF;
    }

    // old_longitude - 2 регистра
    if (index < 49) {
        union { float f; uint32_t i; } old_lon_union;
        old_lon_union.f = cont->old_longitude;
        buffer[index++] = (old_lon_union.i >> 16) & 0xFFFF;
        buffer[index++] = old_lon_union.i & 0xFFFF;
    }

    // altitude - 2 регистра
    if (index < 49) {
        union { float f; uint32_t i; } alt_union;
        alt_union.f = cont->altitude;
        buffer[index++] = (alt_union.i >> 16) & 0xFFFF;
        buffer[index++] = alt_union.i & 0xFFFF;
    }

    // pressure_altitude - 2 регистра
    if (index < 49) {
        union { float f; uint32_t i; } palt_union;
        palt_union.f = cont->pressure_altitude;
        buffer[index++] = (palt_union.i >> 16) & 0xFFFF;
        buffer[index++] = palt_union.i & 0xFFFF;
    }

    // course - 2 регистра
    if (index < 49) {
        union { float f; uint32_t i; } course_union;
        course_union.f = cont->course;
        buffer[index++] = (course_union.i >> 16) & 0xFFFF;
        buffer[index++] = course_union.i & 0xFFFF;
    }

    // speed - 2 регистра
    if (index < 49) {
        union { float f; uint32_t i; } speed_union;
        speed_union.f = cont->speed;
        buffer[index++] = (speed_union.i >> 16) & 0xFFFF;
        buffer[index++] = speed_union.i & 0xFFFF;
    }

    // aircraft_type - 1 регистр
    if (index < 50) {
        buffer[index++] = cont->aircraft_type;
    }

    // flight[8] - 4 регистра (только первые 8 символов)
    for (int i = 0; i < 8 && index < 50; i += 2) {
        buffer[index++] = (cont->flight[i] << 8) | cont->flight[i + 1];
    }

    // vert_rate - 2 регистра
    if (index < 49) {
        buffer[index++] = (cont->vert_rate >> 16) & 0xFFFF;
        buffer[index++] = cont->vert_rate & 0xFFFF;
    }

    // Squawk - 2 регистра
    if (index < 49) {
        buffer[index++] = (cont->Squawk >> 16) & 0xFFFF;
        buffer[index++] = cont->Squawk & 0xFFFF;
    }

    // stealth и no_track - 1 регистр
    if (index < 50) {
        buffer[index++] = (cont->stealth ? 1 : 0) | ((cont->no_track ? 1 : 0) << 1);
    }
}

void initTestData() {
    for (int i = 0; i < MAX_TRACKING_OBJECTS; i++) {
        container[i].timestamp = millis();
        container[i].protocol = i + 1;
        container[i].addr = 0x12345600 + i;
        container[i].latitude = 55.7558 + i * 0.001;
        container[i].longitude = 37.6176 + i * 0.001;
        container[i].old_latitude = 55.7550 + i * 0.001;
        container[i].old_longitude = 37.6170 + i * 0.001;
        container[i].altitude = 1000 + i * 100;
        container[i].pressure_altitude = 950 + i * 100;
        container[i].course = 90 + i * 10;
        container[i].speed = 250 + i * 10;
        snprintf(container[i].flight, sizeof(container[i].flight), "SU%03d", 100 + i);
        container[i].aircraft_type = i % 4;
        container[i].timemsg = millis();
        container[i].seen = millis();
        container[i].vs = 100.0 + i * 10;
        container[i].stealth = (i % 2 == 0);
        container[i].no_track = (i % 3 == 0);
        container[i].vert_rate = 500 + i * 50;
        container[i].Squawk = 7000 + i;

        // Инициализация массивов
        for (int j = 0; j < 34; j++) {
            container[i].raw[j] = j + i;
        }

        for (int j = 0; j < 4; j++) {
            container[i].ns[j] = j;
            container[i].ew[j] = j + 10;
        }

        for (int j = 0; j < 8; j++) {
            container[i].callsign[j] = 'A' + j;
        }
    }
}

/*
Основные изменения :
Удалена строка с ku8MBResponseTimeout - эта переменная не существует в данной версии библиотеки
Увеличены задержки в функциях preTransmission() и postTransmission() до 200 микросекунд
Увеличена задержка между блоками передачи до 100мс для более стабильной работы
Код для приемника остается без изменений.Теперь источник должен компилироваться без ошибок.

Рекомендации по отладке :
Подключите устройства через RS485 преобразователи
Проверьте соединения - убедитесь, что A подключено к A, B к B
Запустите сначала приемник, затем источник
Следите за выводом в Serial Monitor обоих устройств
Если проблемы с соединением продолжаются, попробуйте :

    Уменьшить скорость до 4800 бод
    Добавить терминирующие резисторы 120 Ом на концах линии RS485
    Проверить правильность подключения DE / RE пинов
*/

//=============================================================
//#include <ModbusMaster.h>
//#include <HardwareSerial.h>
//
//#define MAX_TRACKING_OBJECTS 8
//#define DE_RE_PIN 21
//#define RX_PIN 17
//#define TX_PIN 18
//#define SLAVE_ID 1
//
//// Структура данных
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
//ModbusMaster node;
//HardwareSerial rs485Serial(1);
//
//// Переменные для аналогового сигнала
//const int analogPin = 1;
//uint16_t analogValue = 0;
//
//// Переменные для состояния кнопок от приемника
//bool button_S1_state = false;
//bool button_S2_state = false;
//
//void preTransmission() {
//    digitalWrite(DE_RE_PIN, HIGH);
//    delayMicroseconds(100); // Небольшая задержка для стабилизации
//}
//
//void postTransmission() {
//    delayMicroseconds(100); // Ждем завершения передачи
//    digitalWrite(DE_RE_PIN, LOW);
//}
//
//void setup() {
//    Serial.begin(115200);
//    delay(1000);
//
//    // Настройка RS485
//    pinMode(DE_RE_PIN, OUTPUT);
//    digitalWrite(DE_RE_PIN, LOW);
//
//    // Инициализация последовательного порта
//    rs485Serial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
//
//    // Настройка Modbus
//    node.begin(SLAVE_ID, rs485Serial);
//    node.preTransmission(preTransmission);
//    node.postTransmission(postTransmission);
//
//    // Увеличиваем таймауты
//    node.ku8MBResponseTimeout = 2000; // 2 секунды
//
//    Serial.println("Master ESP32 инициализирован");
//    Serial.printf("Slave ID: %d\n", SLAVE_ID);
//    Serial.printf("DE/RE Pin: %d\n", DE_RE_PIN);
//    Serial.printf("RX Pin: %d, TX Pin: %d\n", RX_PIN, TX_PIN);
//
//    // Инициализация тестовых данных
//    initTestData();
//
//    // Тестовое соединение
//    delay(2000);
//    testConnection();
//}
//
//void loop() {
//    // Чтение аналогового сигнала
//    analogValue = analogRead(analogPin);
//
//    // Передача аналогового сигнала (проще всего для тестирования)
//    sendAnalogValue();
//    delay(500);
//
//    // Чтение состояния кнопок
//    readButtonStates();
//    delay(500);
//
//    // Передача данных контейнера (по одному объекту за раз)
//    static int currentObject = 0;
//    sendSingleContainer(currentObject);
//    currentObject = (currentObject + 1) % MAX_TRACKING_OBJECTS;
//
//    delay(1000); // Пауза между циклами
//}
//
//void testConnection() {
//    Serial.println("Тестирование соединения...");
//
//    // Попытка чтения одного регистра
//    uint8_t result = node.readHoldingRegisters(0, 1);
//
//    if (result == node.ku8MBSuccess) {
//        Serial.println("Соединение установлено успешно!");
//    }
//    else {
//        Serial.printf("Ошибка соединения: %d\n", result);
//        printModbusError(result);
//    }
//}
//
//void sendSingleContainer(int objIndex) {
//    uint16_t startRegister = objIndex * 50;
//
//    // Конвертируем структуру в массив uint16_t для передачи
//    uint16_t dataBuffer[50];
//    structToModbusData(&container[objIndex], dataBuffer);
//
//    // Передача небольшими блоками по 5 регистров
//    bool success = true;
//    for (int block = 0; block < 10; block++) {
//        uint16_t regAddr = startRegister + (block * 5);
//
//        // Подготавливаем данные для передачи
//        node.clearTransmitBuffer();
//        for (int i = 0; i < 5; i++) {
//            node.setTransmitBuffer(i, dataBuffer[block * 5 + i]);
//        }
//
//        uint8_t result = node.writeMultipleRegisters(regAddr, 5);
//
//        if (result != node.ku8MBSuccess) {
//            Serial.printf("Ошибка передачи объекта %d, блок %d: %d ", objIndex, block, result);
//            printModbusError(result);
//            success = false;
//            break;
//        }
//        delay(50); // Увеличиваем задержку между блоками
//    }
//
//    if (success) {
//        Serial.printf("Объект %d передан успешно\n", objIndex);
//    }
//}
//
//void sendAnalogValue() {
//    uint8_t result = node.writeSingleRegister(450, analogValue);
//    if (result == node.ku8MBSuccess) {
//        Serial.printf("Аналоговое значение передано: %d\n", analogValue);
//    }
//    else {
//        Serial.printf("Ошибка передачи аналогового значения: %d ", result);
//        printModbusError(result);
//    }
//}
//
//void readButtonStates() {
//    uint8_t result = node.readHoldingRegisters(451, 2);
//    if (result == node.ku8MBSuccess) {
//        button_S1_state = (bool)node.getResponseBuffer(0);
//        button_S2_state = (bool)node.getResponseBuffer(1);
//        Serial.printf("Состояние кнопок - S1: %s, S2: %s\n",
//            button_S1_state ? "Нажата" : "Отпущена",
//            button_S2_state ? "Нажата" : "Отпущена");
//    }
//    else {
//        Serial.printf("Ошибка чтения состояния кнопок: %d ", result);
//        printModbusError(result);
//    }
//}
//
//void printModbusError(uint8_t errorCode) {
//    switch (errorCode) {
//    case 0xE0: Serial.println("(Invalid response)"); break;
//    case 0xE1: Serial.println("(Timeout)"); break;
//    case 0xE2: Serial.println("(Invalid Slave ID)"); break;
//    case 0xE3: Serial.println("(Invalid function)"); break;
//    case 0xE4: Serial.println("(Response length error)"); break;
//    case 0xE5: Serial.println("(Checksum error)"); break;
//    default: Serial.printf("(Unknown error: 0x%02X)\n", errorCode); break;
//    }
//}
//
//void structToModbusData(Container* cont, uint16_t* buffer) {
//    int index = 0;
//
//    // Очищаем буфер
//    memset(buffer, 0, 50 * sizeof(uint16_t));
//
//    // raw[8] - 4 регистра (только первые 8 байт)
//    for (int i = 0; i < 8 && index < 50; i += 2) {
//        buffer[index++] = (cont->raw[i] << 8) | (i + 1 < 8 ? cont->raw[i + 1] : 0);
//    }
//
//    // timestamp - 2 регистра
//    if (index < 49) {
//        buffer[index++] = (cont->timestamp >> 16) & 0xFFFF;
//        buffer[index++] = cont->timestamp & 0xFFFF;
//    }
//
//    // protocol - 1 регистр
//    if (index < 50) {
//        buffer[index++] = cont->protocol;
//    }
//
//    // addr - 2 регистра
//    if (index < 49) {
//        buffer[index++] = (cont->addr >> 16) & 0xFFFF;
//        buffer[index++] = cont->addr & 0xFFFF;
//    }
//
//    // addr_type - 1 регистр
//    if (index < 50) {
//        buffer[index++] = cont->addr_type;
//    }
//
//    // latitude - 2 регистра
//    if (index < 49) {
//        union { float f; uint32_t i; } lat_union;
//        lat_union.f = cont->latitude;
//        buffer[index++] = (lat_union.i >> 16) & 0xFFFF;
//        buffer[index++] = lat_union.i & 0xFFFF;
//    }
//
//    // longitude - 2 регистра
//    if (index < 49) {
//        union { float f; uint32_t i; } lon_union;
//        lon_union.f = cont->longitude;
//        buffer[index++] = (lon_union.i >> 16) & 0xFFFF;
//        buffer[index++] = lon_union.i & 0xFFFF;
//    }
//
//    // old_latitude - 2 регистра
//    if (index < 49) {
//        union { float f; uint32_t i; } old_lat_union;
//        old_lat_union.f = cont->old_latitude;
//        buffer[index++] = (old_lat_union.i >> 16) & 0xFFFF;
//        buffer[index++] = old_lat_union.i & 0xFFFF;
//    }
//
//    // old_longitude - 2 регистра
//    if (index < 49) {
//        union { float f; uint32_t i; } old_lon_union;
//        old_lon_union.f = cont->old_longitude;
//        buffer[index++] = (old_lon_union.i >> 16) & 0xFFFF;
//        buffer[index++] = old_lon_union.i & 0xFFFF;
//    }
//
//    // altitude - 2 регистра
//    if (index < 49) {
//        union { float f; uint32_t i; } alt_union;
//        alt_union.f = cont->altitude;
//        buffer[index++] = (alt_union.i >> 16) & 0xFFFF;
//        buffer[index++] = alt_union.i & 0xFFFF;
//    }
//
//    // pressure_altitude - 2 регистра
//    if (index < 49) {
//        union { float f; uint32_t i; } palt_union;
//        palt_union.f = cont->pressure_altitude;
//        buffer[index++] = (palt_union.i >> 16) & 0xFFFF;
//        buffer[index++] = palt_union.i & 0xFFFF;
//    }
//
//    // course - 2 регистра
//    if (index < 49) {
//        union { float f; uint32_t i; } course_union;
//        course_union.f = cont->course;
//        buffer[index++] = (course_union.i >> 16) & 0xFFFF;
//        buffer[index++] = course_union.i & 0xFFFF;
//    }
//
//    // speed - 2 регистра
//    if (index < 49) {
//        union { float f; uint32_t i; } speed_union;
//        speed_union.f = cont->speed;
//        buffer[index++] = (speed_union.i >> 16) & 0xFFFF;
//        buffer[index++] = speed_union.i & 0xFFFF;
//    }
//
//    // aircraft_type - 1 регистр
//    if (index < 50) {
//        buffer[index++] = cont->aircraft_type;
//    }
//
//    // flight[8] - 4 регистра (только первые 8 символов)
//    for (int i = 0; i < 8 && index < 50; i += 2) {
//        buffer[index++] = (cont->flight[i] << 8) | cont->flight[i + 1];
//    }
//
//    // vert_rate - 2 регистра
//    if (index < 49) {
//        buffer[index++] = (cont->vert_rate >> 16) & 0xFFFF;
//        buffer[index++] = cont->vert_rate & 0xFFFF;
//    }
//
//    // Squawk - 2 регистра
//    if (index < 49) {
//        buffer[index++] = (cont->Squawk >> 16) & 0xFFFF;
//        buffer[index++] = cont->Squawk & 0xFFFF;
//    }
//
//    // stealth и no_track - 1 регистр
//    if (index < 50) {
//        buffer[index++] = (cont->stealth ? 1 : 0) | ((cont->no_track ? 1 : 0) << 1);
//    }
//}
//
//void initTestData() {
//    for (int i = 0; i < MAX_TRACKING_OBJECTS; i++) {
//        container[i].timestamp = millis();
//        container[i].protocol = i + 1;
//        container[i].addr = 0x12345600 + i;
//        container[i].latitude = 55.7558 + i * 0.001;
//        container[i].longitude = 37.6176 + i * 0.001;
//        container[i].old_latitude = 55.7550 + i * 0.001;
//        container[i].old_longitude = 37.6170 + i * 0.001;
//        container[i].altitude = 1000 + i * 100;
//        container[i].pressure_altitude = 950 + i * 100;
//        container[i].course = 90 + i * 10;
//        container[i].speed = 250 + i * 10;
//        snprintf(container[i].flight, sizeof(container[i].flight), "SU%03d", 100 + i);
//        container[i].aircraft_type = i % 4;
//        container[i].timemsg = millis();
//        container[i].seen = millis();
//        container[i].vs = 100.0 + i * 10;
//        container[i].stealth = (i % 2 == 0);
//        container[i].no_track = (i % 3 == 0);
//        container[i].vert_rate = 500 + i * 50;
//        container[i].Squawk = 7000 + i;
//
//        // Инициализация массивов
//        for (int j = 0; j < 34; j++) {
//            container[i].raw[j] = j + i;
//        }
//    }
//}
//

//===========================================================================
//#include <ModbusMaster.h>
//#include <HardwareSerial.h>
//
//#define MAX_TRACKING_OBJECTS 8
//#define DE_RE_PIN 21
//#define RX_PIN 17
//#define TX_PIN 18
//
//// Структура данных
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
//ModbusMaster node;
//HardwareSerial rs485Serial(1);
//
//// Переменные для аналогового сигнала
//const int analogPin = 1; // GPIO1 для аналогового входа
//uint16_t analogValue = 0;
//
//// Переменные для состояния кнопок от приемника
//bool button_S1_state = false;
//bool button_S2_state = false;
//
//void preTransmission() {
//    digitalWrite(DE_RE_PIN, HIGH);
//}
//
//void postTransmission() {
//    digitalWrite(DE_RE_PIN, LOW);
//}
//
//void setup() {
//    Serial.begin(115200);
//
//    // Настройка RS485
//    pinMode(DE_RE_PIN, OUTPUT);
//    digitalWrite(DE_RE_PIN, LOW);
//
//    rs485Serial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
//
//    // Настройка Modbus
//    node.begin(1, rs485Serial); // Slave ID = 1
//    node.preTransmission(preTransmission);
//    node.postTransmission(postTransmission);
//
//    Serial.println("Master ESP32 инициализирован");
//
//    // Инициализация тестовых данных
//    initTestData();
//}
//
//void loop() {
//    // Чтение аналогового сигнала
//    analogValue = analogRead(analogPin);
//
//    // Передача данных контейнера
//    sendContainerData();
//    delay(100);
//
//    // Передача аналогового сигнала
//    sendAnalogValue();
//    delay(100);
//
//    // Чтение состояния кнопок
//    readButtonStates();
//    delay(100);
//
//    delay(1000); // Пауза между циклами
//}
//
//void sendContainerData() {
//    for (int objIndex = 0; objIndex < MAX_TRACKING_OBJECTS; objIndex++) {
//        uint16_t startRegister = objIndex * 50; // Уменьшим до 50 регистров на объект
//
//        // Конвертируем структуру в массив uint16_t для передачи
//        uint16_t dataBuffer[50];
//        structToModbusData(&container[objIndex], dataBuffer);
//
//        // Передача блоками по 10 регистров
//        for (int block = 0; block < 5; block++) {
//            uint16_t regAddr = startRegister + (block * 10);
//
//            // Подготавливаем данные для передачи
//            node.clearTransmitBuffer();
//            for (int i = 0; i < 10; i++) {
//                node.setTransmitBuffer(i, dataBuffer[block * 10 + i]);
//            }
//
//            uint8_t result = node.writeMultipleRegisters(regAddr, 10);
//
//            if (result != node.ku8MBSuccess) {
//                Serial.printf("Ошибка передачи объекта %d, блок %d: %d\n", objIndex, block, result);
//            }
//            delay(10);
//        }
//
//        Serial.printf("Объект %d передан\n", objIndex);
//        delay(50);
//    }
//}
//
//void sendAnalogValue() {
//    uint8_t result = node.writeSingleRegister(450, analogValue); // Регистр 450 для аналогового значения
//    if (result == node.ku8MBSuccess) {
//        Serial.printf("Аналоговое значение передано: %d\n", analogValue);
//    }
//    else {
//        Serial.printf("Ошибка передачи аналогового значения: %d\n", result);
//    }
//}
//
//void readButtonStates() {
//    uint8_t result = node.readHoldingRegisters(451, 2); // Регистры 451-452 для кнопок
//    if (result == node.ku8MBSuccess) {
//        button_S1_state = (bool)node.getResponseBuffer(0);
//        button_S2_state = (bool)node.getResponseBuffer(1);
//        Serial.printf("Состояние кнопок - S1: %s, S2: %s\n",
//            button_S1_state ? "Нажата" : "Отпущена",
//            button_S2_state ? "Нажата" : "Отпущена");
//    }
//    else {
//        Serial.printf("Ошибка чтения состояния кнопок: %d\n", result);
//    }
//}
//
//void structToModbusData(Container* cont, uint16_t* buffer) {
//    int index = 0;
//
//    // Очищаем буфер
//    memset(buffer, 0, 50 * sizeof(uint16_t));
//
//    // raw[34] - берем только первые 8 байт (4 регистра)
//    for (int i = 0; i < 8 && index < 50; i += 2) {
//        buffer[index++] = (cont->raw[i] << 8) | (i + 1 < 8 ? cont->raw[i + 1] : 0);
//    }
//
//    // timestamp - 2 регистра
//    if (index < 49) {
//        buffer[index++] = (cont->timestamp >> 16) & 0xFFFF;
//        buffer[index++] = cont->timestamp & 0xFFFF;
//    }
//
//    // protocol - 1 регистр
//    if (index < 50) {
//        buffer[index++] = cont->protocol;
//    }
//
//    // addr - 2 регистра
//    if (index < 49) {
//        buffer[index++] = (cont->addr >> 16) & 0xFFFF;
//        buffer[index++] = cont->addr & 0xFFFF;
//    }
//
//    // addr_type - 1 регистр
//    if (index < 50) {
//        buffer[index++] = cont->addr_type;
//    }
//
//    // latitude - 2 регистра
//    if (index < 49) {
//        union { float f; uint32_t i; } lat_union;
//        lat_union.f = cont->latitude;
//        buffer[index++] = (lat_union.i >> 16) & 0xFFFF;
//        buffer[index++] = lat_union.i & 0xFFFF;
//    }
//
//    // longitude - 2 регистра
//    if (index < 49) {
//        union { float f; uint32_t i; } lon_union;
//        lon_union.f = cont->longitude;
//        buffer[index++] = (lon_union.i >> 16) & 0xFFFF;
//        buffer[index++] = lon_union.i & 0xFFFF;
//    }
//
//    // old_latitude - 2 регистра
//    if (index < 49) {
//        union { float f; uint32_t i; } old_lat_union;
//        old_lat_union.f = cont->old_latitude;
//        buffer[index++] = (old_lat_union.i >> 16) & 0xFFFF;
//        buffer[index++] = old_lat_union.i & 0xFFFF;
//    }
//
//    // old_longitude - 2 регистра
//    if (index < 49) {
//        union { float f; uint32_t i; } old_lon_union;
//        old_lon_union.f = cont->old_longitude;
//        buffer[index++] = (old_lon_union.i >> 16) & 0xFFFF;
//        buffer[index++] = old_lon_union.i & 0xFFFF;
//    }
//
//    // altitude - 2 регистра
//    if (index < 49) {
//        union { float f; uint32_t i; } alt_union;
//        alt_union.f = cont->altitude;
//        buffer[index++] = (alt_union.i >> 16) & 0xFFFF;
//        buffer[index++] = alt_union.i & 0xFFFF;
//    }
//
//    // pressure_altitude - 2 регистра
//    if (index < 49) {
//        union { float f; uint32_t i; } palt_union;
//        palt_union.f = cont->pressure_altitude;
//        buffer[index++] = (palt_union.i >> 16) & 0xFFFF;
//        buffer[index++] = palt_union.i & 0xFFFF;
//    }
//
//    // course - 2 регистра
//    if (index < 49) {
//        union { float f; uint32_t i; } course_union;
//        course_union.f = cont->course;
//        buffer[index++] = (course_union.i >> 16) & 0xFFFF;
//        buffer[index++] = course_union.i & 0xFFFF;
//    }
//
//    // speed - 2 регистра
//    if (index < 49) {
//        union { float f; uint32_t i; } speed_union;
//        speed_union.f = cont->speed;
//        buffer[index++] = (speed_union.i >> 16) & 0xFFFF;
//        buffer[index++] = speed_union.i & 0xFFFF;
//    }
//
//    // aircraft_type - 1 регистр
//    if (index < 50) {
//        buffer[index++] = cont->aircraft_type;
//    }
//
//    // flight[16] - берем только первые 8 символов (4 регистра)
//    for (int i = 0; i < 8 && index < 50; i += 2) {
//        buffer[index++] = (cont->flight[i] << 8) | cont->flight[i + 1];
//    }
//
//    // vert_rate - 2 регистра
//    if (index < 49) {
//        buffer[index++] = (cont->vert_rate >> 16) & 0xFFFF;
//        buffer[index++] = cont->vert_rate & 0xFFFF;
//    }
//
//    // Squawk - 2 регистра
//    if (index < 49) {
//        buffer[index++] = (cont->Squawk >> 16) & 0xFFFF;
//        buffer[index++] = cont->Squawk & 0xFFFF;
//    }
//
//    // stealth и no_track - 1 регистр
//    if (index < 50) {
//        buffer[index++] = (cont->stealth ? 1 : 0) | ((cont->no_track ? 1 : 0) << 1);
//    }
//}
//
//void initTestData() {
//    for (int i = 0; i < MAX_TRACKING_OBJECTS; i++) {
//        container[i].timestamp = millis();
//        container[i].protocol = i + 1;
//        container[i].addr = 0x12345600 + i;
//        container[i].latitude = 55.7558 + i * 0.001;
//        container[i].longitude = 37.6176 + i * 0.001;
//        container[i].old_latitude = 55.7550 + i * 0.001;
//        container[i].old_longitude = 37.6170 + i * 0.001;
//        container[i].altitude = 1000 + i * 100;
//        container[i].pressure_altitude = 950 + i * 100;
//        container[i].course = 90 + i * 10;
//        container[i].speed = 250 + i * 10;
//        snprintf(container[i].flight, sizeof(container[i].flight), "SU%03d", 100 + i);
//        container[i].aircraft_type = i % 4;
//        container[i].timemsg = millis();
//        container[i].seen = millis();
//        container[i].vs = 100.0 + i * 10;
//        container[i].stealth = (i % 2 == 0);
//        container[i].no_track = (i % 3 == 0);
//        container[i].vert_rate = 500 + i * 50;
//        container[i].Squawk = 7000 + i;
//
//        // Инициализация массивов
//        for (int j = 0; j < 34; j++) {
//            container[i].raw[j] = j + i;
//        }
//
//        for (int j = 0; j < 4; j++) {
//            container[i].ns[j] = j;
//            container[i].ew[j] = j + 10;
//        }
//
//        for (int j = 0; j < 8; j++) {
//            container[i].callsign[j] = 'A' + j;
//        }
//    }
//}
/*
Основные изменения:
Исправлена функция writeMultipleRegisters() - теперь используется правильный синтаксис с setTransmitBuffer()
Уменьшено количество регистров на объект до 50 (вместо 100) для упрощения
Исправлена работа с float значениями - используются union для корректного преобразования
Изменены адреса регистров:
Объекты: 0-399 (по 50 регистров на объект)
Аналоговое значение: регистр 450
Кнопки: регистры 451-452
Карта регистров:
0-49: Объект 0
50-99: Объект 1
100-149: Объект 2
150-199: Объект 3
200-249: Объект 4
250-299: Объект 5
300-349: Объект 6
350-399: Объект 7
450: Аналоговое значение
451: Состояние кнопки S1
452: Состояние кнопки S2
Теперь код должен компилироваться без ошибок.
*/