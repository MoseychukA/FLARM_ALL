#include <ModbusMaster.h>
#include <HardwareSerial.h>

// Конфигурация RS485
#define RS485_TX_PIN 18
#define RS485_RX_PIN 17
#define RS485_DE_RE_PIN 21

// Modbus настройки
#define SLAVE_ID 1
#define BAUD_RATE 921600

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

// Глобальные переменные
ufo_t Container[MAX_TRACKING_OBJECTS];
ufo_t ThisAircraft;
additional_data_t AdditionalInfo;

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

// Функция конвертации float в два uint16_t
void floatToUint16(float value, uint16_t* high, uint16_t* low) {
    union {
        float f;
        uint32_t i;
    } converter;
    converter.f = value;
    *high = (converter.i >> 16) & 0xFFFF;
    *low = converter.i & 0xFFFF;
}

// Функция конвертации time_t в два uint16_t
void timeToUint16(time_t value, uint16_t* high, uint16_t* low) {
    *high = (value >> 16) & 0xFFFF;
    *low = value & 0xFFFF;
}

// Функция записи блока данных
bool writeRegisterBlock(uint16_t startAddress, uint16_t* data, uint16_t count) {
    for (uint16_t i = 0; i < count; i++) {
        node.setTransmitBuffer(i, data[i]);
    }

    uint8_t result = node.writeMultipleRegisters(startAddress, count);
  
    if (result == node.ku8MBSuccess) {
        //Serial.print("✓ Written ");
        //Serial.print(count);
        //Serial.print(" registers starting at ");
        //Serial.println(startAddress);
        return true;
    }
    else {
  /*      Serial.print("✗ Error writing registers at ");
        Serial.print(startAddress);
        Serial.print(", error: ");
        Serial.println(result);*/
        return false;
    }
}

// Функция передачи одного UFO объекта
bool sendUFOObject(uint8_t objectIndex, const ufo_t* ufo) {
    const uint16_t baseRegister = (objectIndex < 8) ? (1000 + objectIndex * 100) : 1800;
    uint16_t data[60];  // Достаточно для UFO структуры
    uint16_t registerIndex = 0;

    //Serial.print("Sending UFO object ");
    //Serial.print(objectIndex);
    //Serial.print(" to register ");
    //Serial.println(baseRegister);

    // Упаковка данных UFO в массив uint16_t
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

    //Serial.print("Total registers to send: ");
    //Serial.println(registerIndex);

    // Передача блоками с исправленной функцией min
    const uint16_t maxBlockSize = 20;
    for (uint16_t i = 0; i < registerIndex; i += maxBlockSize) {
        // ИСПРАВЛЕНО: явное приведение типов для функции min
        uint16_t remaining = registerIndex - i;
        uint16_t blockSize = (maxBlockSize < remaining) ? maxBlockSize : remaining;

        if (!writeRegisterBlock(baseRegister + i, &data[i], blockSize)) {
            return false;
        }
        delay(10); // Небольшая задержка между блоками
    }

    return true;
}

// Функция передачи дополнительных данных
bool sendAdditionalData() {
    const uint16_t baseRegister = 2000;
    uint16_t data[40];
    uint16_t registerIndex = 0;

    //Serial.println("Sending Additional Data to register 2000");

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

    //Serial.print("Additional data registers to send: ");
    //Serial.println(registerIndex);

    return writeRegisterBlock(baseRegister, data, registerIndex);
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    // Настройка RS485
    pinMode(RS485_DE_RE_PIN, OUTPUT);
    digitalWrite(RS485_DE_RE_PIN, LOW);

    // Инициализация Serial2 для RS485
    RS485Serial.begin(BAUD_RATE, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);

    // Настройка Modbus Master
    node.begin(SLAVE_ID, RS485Serial);
    node.preTransmission(preTransmission);
    node.postTransmission(postTransmission);

    Serial.println("=================================");
    Serial.println("Modbus Master FIXED VERSION");
    Serial.println("=================================");

    // Инициализация тестовых данных
    memset(&Container, 0, sizeof(Container));
    memset(&ThisAircraft, 0, sizeof(ThisAircraft));
    memset(&AdditionalInfo, 0, sizeof(AdditionalInfo));

    // Заполнение тестовыми данными Container
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

    // Заполнение тестовыми данными ThisAircraft
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

    // Заполнение дополнительных данных
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
    strcpy(AdditionalInfo.msg_resp_M, "Test message from master device ver 1.0");

    Serial.println("Test data initialized");
    Serial.println("Starting transmission in 3 seconds...");
    delay(1000);
}

void loop() {
    unsigned long startTime = millis();
    bool allSuccess = true;

    Serial.println();
    Serial.println("=== STARTING DATA TRANSMISSION ===");

    // Передача Container[0-7]
    Serial.println("Sending Container objects...");
    for (int i = 0; i < MAX_TRACKING_OBJECTS; i++) {
        if (!sendUFOObject(i, &Container[i])) {
            allSuccess = false;
            Serial.print("Failed to send Container[");
            Serial.print(i);
            Serial.println("]");
        }
        else {
 /*           Serial.print("✓ Container[");
            Serial.print(i);
            Serial.println("] sent successfully");*/
        }
    }

    // Передача ThisAircraft (объект 8, адрес 1800)
    Serial.println("Sending ThisAircraft...");
    if (!sendUFOObject(8, &ThisAircraft)) {
        allSuccess = false;
        Serial.println("Failed to send ThisAircraft");
    }
    else {
        //Serial.println("✓ ThisAircraft sent successfully");
    }

    // Передача дополнительных данных
    Serial.println("Sending Additional Data...");
    if (!sendAdditionalData()) {
        allSuccess = false;
        Serial.println("Failed to send Additional Data");
    }
    else {
        //Serial.println("✓ Additional Data sent successfully");
    }

    unsigned long endTime = millis();
    unsigned long transmissionTime = endTime - startTime;

    //Serial.println();
    //Serial.println("=== TRANSMISSION SUMMARY ===");
    //Serial.print("Time: ");
    //Serial.print(transmissionTime);
    //Serial.print(" ms, Status: ");
    //Serial.println(allSuccess ? "SUCCESS" : "ERROR");

    //if (transmissionTime > 500) {
    //    //Serial.println("WARNING: Transmission time exceeded 500ms!");
    //}
    //else {
    //    //Serial.println("✓ Transmission time within 500ms limit");
    //}

    Serial.println("=====================================");

    delay(150); // Передача каждые 2 секунды
}