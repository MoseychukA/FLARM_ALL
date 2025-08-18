//Полная реализация обмена данными между двумя ESP32S3 через RS485 с протоколом MODBUS:

//Код для ПРИЕМНИКА (Slave)

#include <ModbusRTU.h>
#include <HardwareSerial.h>

#define MAX_TRACKING_OBJECTS 8
#define DE_RE_PIN 40
#define RX_PIN 38
#define TX_PIN 39
#define SLAVE_ID 1

// Структура UFO
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
} ufo_t;

ufo_t Container[MAX_TRACKING_OBJECTS];
ufo_t ThisAircraft;

ModbusRTU mb;
HardwareSerial rs485Serial(1);

void setup() {
    Serial.begin(115200);
    delay(1000);

    // Настройка RS485
    pinMode(DE_RE_PIN, OUTPUT);
    digitalWrite(DE_RE_PIN, LOW);

    rs485Serial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);

    // Настройка Modbus Slave
    mb.begin(&rs485Serial, DE_RE_PIN);
    mb.slave(SLAVE_ID);

    // Добавление регистров (1000 регистров)
    mb.addHreg(0, 0, 1000);

    Serial.println("Slave ESP32 инициализирован");
    Serial.printf("Slave ID: %d\n", SLAVE_ID);
    Serial.printf("DE/RE Pin: %d\n", DE_RE_PIN);
    Serial.printf("RX Pin: %d, TX Pin: %d\n", RX_PIN, TX_PIN);
    Serial.println("Ожидание подключения Master...");
}

void loop() {
    // Обработка Modbus запросов
    mb.task();

    // Обработка полученных данных
    processReceivedData();

    delay(10);
}

void processReceivedData() {
    static uint32_t lastProcessTime = 0;
    static uint16_t lastAnalogValue = 0;

    if (millis() - lastProcessTime > 3000) { // Обрабатываем раз в 3 секунды
        // Проверяем аналоговое значение
        uint16_t analogValue = mb.Hreg(999);
        if (analogValue != lastAnalogValue && analogValue > 0) {
            Serial.printf("Получено аналоговое значение: %d\n", analogValue);
            lastAnalogValue = analogValue;
        }

        // Обрабатываем ThisAircraft
        parseThisAircraft();

        // Обрабатываем Container объекты
        parseContainerData();

        lastProcessTime = millis();
    }
}

void parseThisAircraft() {
    // ThisAircraft находится в регистрах 900-949
    uint16_t dataBuffer[50];
    bool dataReceived = false;

    for (int i = 0; i < 50; i++) {
        dataBuffer[i] = mb.Hreg(900 + i);
        if (dataBuffer[i] != 0) {
            dataReceived = true;
        }
    }

    if (dataReceived) {
        modbusDataToUfo(dataBuffer, &ThisAircraft);
        Serial.printf("ThisAircraft получен - Addr: 0x%08X, Lat: %.6f, Lon: %.6f, Flight: %s\n",
            ThisAircraft.addr,
            ThisAircraft.latitude,
            ThisAircraft.longitude,
            ThisAircraft.flight);
    }
}

void parseContainerData() {
    static int lastParsedObject = -1;

    for (int objIndex = 0; objIndex < MAX_TRACKING_OBJECTS; objIndex++) {
        uint16_t startRegister = objIndex * 50; // По 50 регистров на объект

        uint16_t dataBuffer[50];
        bool dataReceived = false;
        uint32_t checksum = 0;

        for (int i = 0; i < 50; i++) {
            dataBuffer[i] = mb.Hreg(startRegister + i);
            checksum += dataBuffer[i];
            if (dataBuffer[i] != 0) {
                dataReceived = true;
            }
        }

        if (dataReceived && objIndex != lastParsedObject) {
            modbusDataToUfo(dataBuffer, &Container[objIndex]);
            Serial.printf("Container[%d] получен - Addr: 0x%08X, Lat: %.6f, Lon: %.6f, Flight: %s, Alt: %.1f\n",
                objIndex,
                Container[objIndex].addr,
                Container[objIndex].latitude,
                Container[objIndex].longitude,
                Container[objIndex].flight,
                Container[objIndex].altitude);
            lastParsedObject = objIndex;
        }
    }
}

void modbusDataToUfo(uint16_t* buffer, ufo_t* ufo) {
    int index = 0;

    // timestamp - 2 регистра
    ufo->timestamp = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
    index += 2;

    // addr - 2 регистра
    ufo->addr = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
    index += 2;

    // addr_type - 1 регистр
    ufo->addr_type = (uint8_t)buffer[index++];

    // latitude - 2 регистра
    union { float f; uint32_t i; } lat_union;
    lat_union.i = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
    ufo->latitude = lat_union.f;
    index += 2;

    // longitude - 2 регистра
    union { float f; uint32_t i; } lon_union;
    lon_union.i = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
    ufo->longitude = lon_union.f;
    index += 2;

    // old_latitude - 2 регистра
    union { float f; uint32_t i; } old_lat_union;
    old_lat_union.i = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
    ufo->old_latitude = old_lat_union.f;
    index += 2;

    // old_longitude - 2 регистра
    union { float f; uint32_t i; } old_lon_union;
    old_lon_union.i = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
    ufo->old_longitude = old_lon_union.f;
    index += 2;

    // altitude - 2 регистра
    union { float f; uint32_t i; } alt_union;
    alt_union.i = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
    ufo->altitude = alt_union.f;
    index += 2;

    // pressure_altitude - 2 регистра
    union { float f; uint32_t i; } palt_union;
    palt_union.i = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
    ufo->pressure_altitude = palt_union.f;
    index += 2;

    // course - 2 регистра
    union { float f; uint32_t i; } course_union;
    course_union.i = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
    ufo->course = course_union.f;
    index += 2;

    // speed - 2 регистра
    union { float f; uint32_t i; } speed_union;
    speed_union.i = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
    ufo->speed = speed_union.f;
    index += 2;

    // aircraft_type - 1 регистр
    ufo->aircraft_type = (uint8_t)buffer[index++];

    // flight[16] - 8 регистров
    for (int i = 0; i < 16; i += 2) {
        ufo->flight[i] = (buffer[index] >> 8) & 0xFF;
        ufo->flight[i + 1] = buffer[index] & 0xFF;
        index++;
    }
    // Убеждаемся, что строка завершается нулем
    ufo->flight[15] = '\0';

    // vert_rate - 2 регистра
    ufo->vert_rate = ((int32_t)buffer[index] << 16) | buffer[index + 1];
    index += 2;

    // Squawk - 2 регистра
    ufo->Squawk = ((int32_t)buffer[index] << 16) | buffer[index + 1];
    index += 2;

    // timemsg - 2 регистра
    ufo->timemsg = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
    index += 2;

    // vs - 2 регистра
    union { float f; uint32_t i; } vs_union;
    vs_union.i = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
    ufo->vs = vs_union.f;
    index += 2;

    // geoid_separation - 2 регистра
    if (index < 48) {
        union { float f; uint32_t i; } geoid_union;
        geoid_union.i = ((uint32_t)buffer[index] << 16) | buffer[index + 1];
        ufo->geoid_separation = geoid_union.f;
        index += 2;
    }
    else {
        ufo->geoid_separation = 0.0;
    }

    // hdop - 1 регистр
    if (index < 50) {
        ufo->hdop = buffer[index++];
    }
    else {
        ufo->hdop = 0;
    }

    // rssi - используем младший байт следующего регистра
    if (index < 50) {
        ufo->rssi = (int8_t)(buffer[index] & 0xFF);
        index++;
    }
    else {
        ufo->rssi = 0;
    }

    // Инициализируем остальные поля значениями по умолчанию
    ufo->distance = 0.0;
    ufo->bearing = 0.0;
    ufo->signal_source = 0;
    ufo->seen = ufo->timestamp;
    ufo->hour_msg = (ufo->timemsg / 3600) % 24;
    ufo->min_msg = (ufo->timemsg / 60) % 60;
    ufo->delay_time_msg = 0;

    // Очищаем callsign
    for (int i = 0; i < 8; i++) {
        ufo->callsign[i] = 0;
    }
}

// Функция для отображения статистики
void printStatistics() {
    static uint32_t lastStatsTime = 0;

    if (millis() - lastStatsTime > 10000) { // Каждые 10 секунд
        Serial.println("\n=== СТАТИСТИКА ПРИЕМНИКА ===");

        // Аналоговое значение
        uint16_t analogValue = mb.Hreg(999);
        Serial.printf("Аналоговое значение: %d\n", analogValue);

        // ThisAircraft
        if (ThisAircraft.addr != 0) {
            Serial.printf("ThisAircraft: Addr=0x%08X, Lat=%.6f, Lon=%.6f, Alt=%.1f, Flight=%s\n",
                ThisAircraft.addr, ThisAircraft.latitude, ThisAircraft.longitude,
                ThisAircraft.altitude, ThisAircraft.flight);
        }

        // Container объекты
        int validObjects = 0;
        for (int i = 0; i < MAX_TRACKING_OBJECTS; i++) {
            if (Container[i].addr != 0) {
                validObjects++;
                Serial.printf("Container[%d]: Addr=0x%08X, Lat=%.6f, Lon=%.6f, Alt=%.1f, Flight=%s\n",
                    i, Container[i].addr, Container[i].latitude, Container[i].longitude,
                    Container[i].altitude, Container[i].flight);
            }
        }

        Serial.printf("Всего активных объектов: %d\n", validObjects);
        Serial.println("==============================\n");

        lastStatsTime = millis();
    }
}

//// Альтернативная функция loop с периодической статистикой
//void loop() {
//    // Обработка Modbus запросов
//    mb.task();
//
//    // Обработка полученных данных
//    processReceivedData();
//
//    // Отображение статистики (опционально)
//    // printStatistics();
//
//    delay(10);
//}

/*
Основные изменения для библиотеки ModbusRTU :
Изменен include на #include <ModbusRTU.h>
Новая инициализация Modbus :
cpp Копировать
mb.begin(&rs485Serial, DE_RE_PIN);
mb.slave(SLAVE_ID);
mb.addHreg(0, 0, 1000);
Доступ к регистрам через mb.Hreg(address)
Упрощенная обработка без callback функций
Добавлена функция статистики для отладки
Установка библиотеки :
В Arduino IDE перейдите в Sketch → Include Library → Manage Libraries и найдите :

ModbusRTU by Alexander Emelianov
Особенности работы :
Библиотека автоматически обрабатывает Modbus запросы
Регистры доступны напрямую через mb.Hreg()
Не требуются callback функции
Поддерживает до 1000 holding регистров
Автоматическое управление DE / RE пином
Карта регистров(без изменений) :
    0 - 49 : Container[0]
    50 - 99 : Container[1]
    100 - 149 : Container[2]
    150 - 199 : Container[3]
    200 - 249 : Container[4]
    250 - 299 : Container[5]
    300 - 349 : Container[6]
    350 - 399 : Container[7]
    900 - 949 : ThisAircraft
    999 : Аналоговое значение
    Код теперь использует библиотеку ModbusRTU by Alexander Emelianov и должен работать стабильно с ESP32S3.
    */


