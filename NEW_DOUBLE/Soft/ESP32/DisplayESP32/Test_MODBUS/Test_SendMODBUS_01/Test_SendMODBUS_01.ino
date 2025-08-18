//Полная реализация обмена данными между двумя ESP32S3 через RS485 с протоколом MODBUS:

//Код для ИСТОЧНИКА (Master)


#include <ModbusMaster.h>
#include <HardwareSerial.h>

#define MAX_TRACKING_OBJECTS 8
#define DE_RE_PIN 21
#define RX_PIN 17
#define TX_PIN 18
#define SLAVE_ID 1
#define ANALOG_PIN 3

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

ufo_t Container[MAX_TRACKING_OBJECTS];
ufo_t ThisAircraft;
additional_data_t AdditionalData;

ModbusMaster node;
HardwareSerial rs485Serial(1);

uint16_t analogValue = 0;

void preTransmission() {
    digitalWrite(DE_RE_PIN, HIGH);
    delayMicroseconds(200);
}

void postTransmission() {
    delayMicroseconds(200);
    digitalWrite(DE_RE_PIN, LOW);
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    // Настройка RS485
    pinMode(DE_RE_PIN, OUTPUT);
    digitalWrite(DE_RE_PIN, LOW);

    rs485Serial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);

    // Настройка Modbus
    node.begin(SLAVE_ID, rs485Serial);
    node.preTransmission(preTransmission);
    node.postTransmission(postTransmission);

    Serial.println("Master ESP32 инициализирован");

    // Инициализация тестовых данных
    initTestData();

    delay(2000);
    testConnection();
}

void loop() 
{
    // Обновление данных
    updateData();

    // Чтение аналогового сигнала
    analogValue = random(2600, 2615);
    // analogRead(ANALOG_PIN);

    // Передача аналогового значения
    sendAnalogValue();
    delay(200);

    // Передача дополнительных данных
    sendAdditionalData();
    delay(200);

    // Передача ThisAircraft
    sendThisAircraft();
    delay(200);

    // Передача Container объектов (по одному за раз)
    static int currentObject = 0;
    sendContainerObject(currentObject);
    currentObject = (currentObject + 1) % MAX_TRACKING_OBJECTS;
    initTestData();
    delay(500);
}

void updateData() 
{
    // Симуляция изменения дополнительных данных
    static uint32_t lastUpdate = 0;

    if (millis() - lastUpdate > 2000) { // Обновляем каждые 2 секунд
        AdditionalData.new_flag = !AdditionalData.new_flag;
        AdditionalData.new_button = (AdditionalData.new_button + 1) % 4;
        AdditionalData.SOS_Sprite_on_off = !AdditionalData.SOS_Sprite_on_off;
        AdditionalData.isValidGNSS = !AdditionalData.isValidGNSS;

        // Обновляем сообщение
        snprintf(AdditionalData.msg_resp, sizeof(AdditionalData.msg_resp),  "MSG_%lu", millis() / 1000);

        lastUpdate = millis();
    }
}



void testConnection() {
    Serial.println("Тестирование соединения...");
    uint8_t result = node.readHoldingRegisters(0, 1);

    if (result == node.ku8MBSuccess) {
        Serial.println("Соединение установлено!");
    }
    else {
        Serial.printf("Ошибка соединения: %d\n", result);
        printModbusError(result);
    }
}

void sendAnalogValue() {
    uint8_t result = node.writeSingleRegister(999, analogValue);
    if (result == node.ku8MBSuccess) {
        Serial.printf("Аналоговое значение передано: %d\n", analogValue);
    }
    else {
        Serial.printf("Ошибка передачи аналогового значения: %d ", result);
        printModbusError(result);
    }
}

void sendAdditionalData() {
    Serial.println("Передача дополнительных данных...");

    // Дополнительные данные занимают регистры 950-999 (50 регистров)
    uint16_t dataBuffer[50];
    additionalDataToModbus(&AdditionalData, dataBuffer);

    // Передача блоками по 5 регистров
    bool success = true;
    for (int block = 0; block < 10; block++) {
        uint16_t regAddr = 950 + (block * 5);

        node.clearTransmitBuffer();
        for (int i = 0; i < 5; i++) {
            node.setTransmitBuffer(i, dataBuffer[block * 5 + i]);
        }

        uint8_t result = node.writeMultipleRegisters(regAddr, 5);

        if (result != node.ku8MBSuccess) {
            Serial.printf("Ошибка передачи доп.данных, блок %d: %d ", block, result);
            printModbusError(result);
            success = false;
            break;
        }
        delay(50);
    }

    if (success) {
        Serial.println("Дополнительные данные переданы успешно");
    }
}


void sendThisAircraft() {
    Serial.println("Передача ThisAircraft...");

    // ThisAircraft занимает регистры 900-949 (50 регистров)
    uint16_t dataBuffer[50];
    ufoToModbusData(&ThisAircraft, dataBuffer);

    // Передача блоками по 5 регистров
    bool success = true;
    for (int block = 0; block < 10; block++) 
    {
        uint16_t regAddr = 900 + (block * 5);

        node.clearTransmitBuffer();
        for (int i = 0; i < 5; i++) {
            node.setTransmitBuffer(i, dataBuffer[block * 5 + i]);
        }

        uint8_t result = node.writeMultipleRegisters(regAddr, 5);

        if (result != node.ku8MBSuccess) {
            Serial.printf("Ошибка передачи ThisAircraft, блок %d: %d ", block, result);
            printModbusError(result);
            success = false;
            break;
        }
        delay(50);
    }

    if (success) {
        Serial.println("ThisAircraft передан успешно");
    }
}

void sendContainerObject(int objIndex) {
    Serial.printf("Передача Container[%d]...\n", objIndex);

    // Container объекты занимают регистры 0-399 (по 50 регистров на объект)
    uint16_t startRegister = objIndex * 50;
    uint16_t dataBuffer[50];
    ufoToModbusData(&Container[objIndex], dataBuffer);

    // Передача блоками по 5 регистров
    bool success = true;
    for (int block = 0; block < 10; block++) {
        uint16_t regAddr = startRegister + (block * 5);

        node.clearTransmitBuffer();
        for (int i = 0; i < 5; i++) {
            node.setTransmitBuffer(i, dataBuffer[block * 5 + i]);
        }

        uint8_t result = node.writeMultipleRegisters(regAddr, 5);

        if (result != node.ku8MBSuccess) {
            Serial.printf("Ошибка передачи Container[%d], блок %d: %d ", objIndex, block, result);
            printModbusError(result);
            success = false;
            break;
        }
        delay(50);
    }

    if (success) {
        Serial.printf("Container[%d] передан успешно\n", objIndex);
    }
}

void additionalDataToModbus(additional_data_t* data, uint16_t* buffer) {
    int index = 0;

    // Очищаем буфер
    memset(buffer, 0, 50 * sizeof(uint16_t));

    // Упаковываем boolean значения в первый регистр
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

    // new_button - 1 регистр
    buffer[index++] = data->new_button;

    // FLYRF_MODE_TEST - 1 регистр
    buffer[index++] = data->FLYRF_MODE_TEST;

    // msg_resp[60] - 30 регистров
    for (int i = 0; i < 60; i += 2) 
    {
        buffer[index++] = (data->msg_resp[i] << 8) | data->msg_resp[i + 1];
    }

    // Заполняем оставшиеся регистры нулями
    while (index < 50) 
    {
        buffer[index++] = 0;
    }
}



void ufoToModbusData(ufo_t* ufo, uint16_t* buffer) {
    int index = 0;

    // Очищаем буфер
    memset(buffer, 0, 50 * sizeof(uint16_t));

    // timestamp - 2 регистра
    buffer[index++] = ((uint32_t)ufo->timestamp >> 16) & 0xFFFF;
    buffer[index++] = (uint32_t)ufo->timestamp & 0xFFFF;

    // addr - 2 регистра
    buffer[index++] = (ufo->addr >> 16) & 0xFFFF;
    buffer[index++] = ufo->addr & 0xFFFF;

    // addr_type - 1 регистр
    buffer[index++] = ufo->addr_type;

    // latitude - 2 регистра
    union { float f; uint32_t i; } lat_union;
    lat_union.f = ufo->latitude;
    buffer[index++] = (lat_union.i >> 16) & 0xFFFF;
    buffer[index++] = lat_union.i & 0xFFFF;

    // longitude - 2 регистра
    union { float f; uint32_t i; } lon_union;
    lon_union.f = ufo->longitude;
    buffer[index++] = (lon_union.i >> 16) & 0xFFFF;
    buffer[index++] = lon_union.i & 0xFFFF;

    // old_latitude - 2 регистра
    union { float f; uint32_t i; } old_lat_union;
    old_lat_union.f = ufo->old_latitude;
    buffer[index++] = (old_lat_union.i >> 16) & 0xFFFF;
    buffer[index++] = old_lat_union.i & 0xFFFF;

    // old_longitude - 2 регистра
    union { float f; uint32_t i; } old_lon_union;
    old_lon_union.f = ufo->old_longitude;
    buffer[index++] = (old_lon_union.i >> 16) & 0xFFFF;
    buffer[index++] = old_lon_union.i & 0xFFFF;

    // altitude - 2 регистра
    union { float f; uint32_t i; } alt_union;
    alt_union.f = ufo->altitude;
    buffer[index++] = (alt_union.i >> 16) & 0xFFFF;
    buffer[index++] = alt_union.i & 0xFFFF;

    // pressure_altitude - 2 регистра
    union { float f; uint32_t i; } palt_union;
    palt_union.f = ufo->pressure_altitude;
    buffer[index++] = (palt_union.i >> 16) & 0xFFFF;
    buffer[index++] = palt_union.i & 0xFFFF;

    // course - 2 регистра
    union { float f; uint32_t i; } course_union;
    course_union.f = ufo->course;
    buffer[index++] = (course_union.i >> 16) & 0xFFFF;
    buffer[index++] = course_union.i & 0xFFFF;

    // speed - 2 регистра
    union { float f; uint32_t i; } speed_union;
    speed_union.f = ufo->speed;
    buffer[index++] = (speed_union.i >> 16) & 0xFFFF;
    buffer[index++] = speed_union.i & 0xFFFF;

    // aircraft_type - 1 регистр
    buffer[index++] = ufo->aircraft_type;

    // flight[16] - 8 регистров
    for (int i = 0; i < 16; i += 2) {
        buffer[index++] = (ufo->flight[i] << 8) | ufo->flight[i + 1];
    }

    // vert_rate - 2 регистра
    buffer[index++] = (ufo->vert_rate >> 16) & 0xFFFF;
    buffer[index++] = ufo->vert_rate & 0xFFFF;

    // Squawk - 2 регистра
    buffer[index++] = (ufo->Squawk >> 16) & 0xFFFF;
    buffer[index++] = ufo->Squawk & 0xFFFF;

    // timemsg - 2 регистра
    buffer[index++] = ((uint32_t)ufo->timemsg >> 16) & 0xFFFF;
    buffer[index++] = (uint32_t)ufo->timemsg & 0xFFFF;

    // vs - 2 регистра
    union { float f; uint32_t i; } vs_union;
    vs_union.f = ufo->vs;
    buffer[index++] = (vs_union.i >> 16) & 0xFFFF;
    buffer[index++] = vs_union.i & 0xFFFF;

    // Остальные поля можно добавить по аналогии
    // Заполняем оставшиеся регистры нулями
    while (index < 50) {
        buffer[index++] = 0;
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

void initTestData() {
    // Инициализация дополнительных данных
    AdditionalData.new_flag = true;
    AdditionalData.new_button = 1;
    AdditionalData.setMessageRead = false;
    AdditionalData.MessageRead = true;
    AdditionalData.SOS_Sprite_on_off = false;
    AdditionalData.SOS_View_on_off = true;
    AdditionalData.new_SOS_flag = false;
    AdditionalData.confirm_message = true;
    strcpy(AdditionalData.msg_resp, "Test message from source");
    AdditionalData.isValidGNSS = true;
    AdditionalData.FLYRF_MODE_TEST = 5;

    // Инициализация ThisAircraft
    ThisAircraft.timestamp = millis() / 1000;
    ThisAircraft.addr = 0xABCDEF;
    ThisAircraft.addr_type = 1;
    ThisAircraft.latitude = 55.8200;
    ThisAircraft.longitude = 37.6200;
    ThisAircraft.old_latitude = 55.7550;
    ThisAircraft.old_longitude = 37.6170;
    ThisAircraft.altitude = 1500;
    ThisAircraft.pressure_altitude = 1450;
    ThisAircraft.course = 1;
    ThisAircraft.speed = 196;
    ThisAircraft.aircraft_type = 2;
    strcpy(ThisAircraft.flight, "SU1234");
    ThisAircraft.vert_rate = 500;
    ThisAircraft.Squawk = 7000;
    ThisAircraft.timemsg = millis() / 1000;
    ThisAircraft.vs = 250.5;

    // Инициализация Container
    for (int i = 0; i < MAX_TRACKING_OBJECTS; i++) 
    {
        Container[i].timestamp = millis() / 1000;
        Container[i].addr = 0x123456 + i;
        Container[i].addr_type = i % 3;
        Container[i].latitude = 55.7558 + i * 0.010;
        Container[i].longitude = 37.6176 + i * 0.010;
        Container[i].old_latitude = 55.7550 + i * 0.010;
        Container[i].old_longitude = 37.6170 + i * 0.010;
        Container[i].altitude = 1000 + i * 100;
        Container[i].pressure_altitude = 950 + i * 100;
        Container[i].course = 90 + i * 10;
        Container[i].speed = 250 + i * 10;
        Container[i].aircraft_type = i % 4;
        sprintf(Container[i].flight, "FL%03d", 100 + i);
        Container[i].vert_rate = 300 + i * 50;
        Container[i].Squawk = 7000 + i;
        Container[i].timemsg = millis() / 1000;
        Container[i].vs = 100.0 + i * 25;
    }
}
/*

Карта регистров:

0-49: Container[0]
50-99: Container[1]
100-149: Container[2]
150-199: Container[3]
200-249: Container[4]
250-299: Container[5]
300-349: Container[6]
350-399: Container[7]
900-949: ThisAircraft
999: Аналоговое значение

Библиотеки для установки:

Для источника:

ModbusMaster by Doc Walker


Для приемника:

Modbus-ESP8266 by Andre Sarmento Barbosa


Данные передаются небольшими блоками по 5 регистров для обеспечения стабильности связи.
*/