#include <ModbusRTU.h>
#include <HardwareSerial.h>

// Конфигурация RS485
#define RS485_TX_PIN 39
#define RS485_RX_PIN 38
#define RS485_DE_RE_PIN 40

// Modbus настройки
#define SLAVE_ID 1
#define BAUD_RATE 921600

// Создание объекта Modbus Slave
ModbusRTU mb;
HardwareSerial RS485Serial(1);

// Структуры данных (ИДЕНТИЧНЫЕ источнику)
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

// Флаги обновления данных
bool dataUpdated = false;
unsigned long lastDataUpdate = 0;
uint16_t registerWriteCount = 0;

// Функции конвертации (ИДЕНТИЧНЫЕ источнику)
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

// Функция извлечения UFO объекта из регистров (ТОЧНАЯ КОПИЯ логики источника)
void extractUFOObject(uint8_t objectIndex, ufo_t* ufo) {
    const uint16_t baseRegister = (objectIndex < 8) ? (1000 + objectIndex * 100) : 1800;
    uint16_t registerIndex = 0;

    //Serial.print("Extracting UFO object ");
    //Serial.print(objectIndex);
    //Serial.print(" from register ");
    //Serial.println(baseRegister);

    // Очистка структуры
    memset(ufo, 0, sizeof(ufo_t));

    // ТОЧНАЯ КОПИЯ упаковки из источника
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

    // Flight number (ТОЧНАЯ КОПИЯ упаковки)
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

    // Callsign (ТОЧНАЯ КОПИЯ упаковки)
    for (int i = 0; i < 4; i++) {
        uint16_t reg = mb.Hreg(baseRegister + registerIndex++);
        if (i * 2 < 8) ufo->callsign[i * 2] = reg & 0xFF;
        if (i * 2 + 1 < 8) ufo->callsign[i * 2 + 1] = (reg >> 8) & 0xFF;
    }
    ufo->callsign[7] = '\0';

    //Serial.print("Extracted registers: ");
    //Serial.println(registerIndex);
}

// Функция извлечения дополнительных данных (ТОЧНАЯ КОПИЯ логики источника)
void extractAdditionalData() {
    const uint16_t baseRegister = 2000;
    uint16_t registerIndex = 0;

    //Serial.println("Extracting Additional Data from register 2000");

    // Очистка структуры
    memset(&AdditionalInfo, 0, sizeof(additional_data_t));

    // ТОЧНАЯ КОПИЯ упаковки из источника
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
    AdditionalInfo.analog_signal = mb.Hreg(baseRegister + registerIndex++);

    // msg_resp_M (ТОЧНАЯ КОПИЯ упаковки)
    for (int i = 0; i < 30; i++) {
        uint16_t reg = mb.Hreg(baseRegister + registerIndex++);
        if (i * 2 < 60) AdditionalInfo.msg_resp_M[i * 2] = reg & 0xFF;
        if (i * 2 + 1 < 60) AdditionalInfo.msg_resp_M[i * 2 + 1] = (reg >> 8) & 0xFF;
    }
    AdditionalInfo.msg_resp_M[59] = '\0';

    //Serial.print("Additional data registers: ");
    //Serial.println(registerIndex);
}

// Callback функция для записи регистров
uint16_t cbWrite(TRegister* reg, uint16_t val) 
{
    registerWriteCount++;
    dataUpdated = true;
    lastDataUpdate = millis();

    //if (registerWriteCount % 100 == 0) {
    //    Serial.print("Registers written: ");
    //    Serial.println(registerWriteCount);
    //}

    return val;
}

// Callback функция для чтения регистров
uint16_t cbRead(TRegister* reg, uint16_t val) {
    return val;
}

void setup() {
    Serial.begin(115200);
    delay(1000);

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

    Serial.println("=================================");
    Serial.println("Modbus Slave CORRECTED VERSION");
    Serial.println("=================================");

    // Инициализация данных
    memset(&Container, 0, sizeof(Container));
    memset(&ThisAircraft, 0, sizeof(ThisAircraft));
    memset(&AdditionalInfo, 0, sizeof(AdditionalInfo));

    dataUpdated = false;
    lastDataUpdate = 0;
    registerWriteCount = 0;

    Serial.println("Ready to receive data...");
}

void loop() {
    static unsigned long lastPrint = 0;
    static unsigned long lastStructUpdate = 0;
    static bool hasValidData = false;

    // Обработка Modbus запросов
    mb.task();

    // Обновляем структуры данных каждые 500ms после получения данных
    if (dataUpdated && (millis() - lastDataUpdate > 100) && (millis() - lastStructUpdate > 100)) 
    {
        lastStructUpdate = millis();

        Serial.println();
        Serial.println("=== EXTRACTING RECEIVED DATA ===");

        // Container objects (объекты 0-7)
        for (int i = 0; i < MAX_TRACKING_OBJECTS; i++) {
            extractUFOObject(i, &Container[i]);
        }

        // ThisAircraft (объект 8, адрес 1800)
        extractUFOObject(8, &ThisAircraft);

        // Additional data (адрес 2000)
        extractAdditionalData();

        // Проверяем валидность данных
        if (ThisAircraft.addr != 0 || Container[0].addr != 0 || AdditionalInfo.analog_signal != 0) {
            hasValidData = true;
        }

        dataUpdated = false;
        Serial.println("=== EXTRACTION COMPLETE ===");
    }

    // Вывод данных каждые 4 секунды
    //if (millis() - lastPrint > 2000) {
    //    lastPrint = millis();

    //    //Serial.println();
    //    //Serial.println("=== RECEIVED DATA STATUS ===");
    //    //Serial.print("Total registers written: ");
    //    //Serial.println(registerWriteCount);

    //    if (hasValidData) {
    //        Serial.println();
    //        Serial.println("=== VALID DATA FOUND ===");

    //        // ThisAircraft
    //        if (ThisAircraft.addr != 0) {
    //            Serial.println(">>> ThisAircraft <<<");
    //            Serial.print("  Addr: 0x");
    //            Serial.println(ThisAircraft.addr, HEX);
    //            Serial.print("  Addr_type: ");
    //            Serial.println(ThisAircraft.addr_type);
    //            Serial.print("  Timestamp: ");
    //            Serial.println((unsigned long)ThisAircraft.timestamp);
    //            Serial.print("  Lat: ");
    //            Serial.println(ThisAircraft.latitude, 6);
    //            Serial.print("  Lon: ");
    //            Serial.println(ThisAircraft.longitude, 6);
    //            Serial.print("  Alt: ");
    //            Serial.println(ThisAircraft.altitude, 1);
    //            Serial.print("  Course: ");
    //            Serial.println(ThisAircraft.course, 1);
    //            Serial.print("  Speed: ");
    //            Serial.println(ThisAircraft.speed, 1);
    //            Serial.print("  Aircraft_type: ");
    //            Serial.println(ThisAircraft.aircraft_type);
    //            Serial.print("  Flight: '");
    //            Serial.print(ThisAircraft.flight);
    //            Serial.println("'");
    //            Serial.print("  Callsign: '");
    //            Serial.print((char*)ThisAircraft.callsign);
    //            Serial.println("'");
    //            Serial.print("  Vert_rate: ");
    //            Serial.println(ThisAircraft.vert_rate);
    //            Serial.print("  Squawk: ");
    //            Serial.println(ThisAircraft.Squawk);
    //            Serial.print("  HDOP: ");
    //            Serial.println(ThisAircraft.hdop);
    //            Serial.print("  RSSI: ");
    //            Serial.println(ThisAircraft.rssi);
    //        }

    //        // Container[0]
    //        if (Container[0].addr != 0) {
    //            Serial.println(">>> Container[0] <<<");
    //            Serial.print("  Addr: 0x");
    //            Serial.println(Container[0].addr, HEX);
    //            Serial.print("  Lat: ");
    //            Serial.println(Container[0].latitude, 6);
    //            Serial.print("  Lon: ");
    //            Serial.println(Container[0].longitude, 6);
    //            Serial.print("  Alt: ");
    //            Serial.println(Container[0].altitude, 1);
    //            Serial.print("  Flight: '");
    //            Serial.print(Container[0].flight);
    //            Serial.println("'");
    //            Serial.print("  Callsign: '");
    //            Serial.print((char*)Container[0].callsign);
    //            Serial.println("'");
    //        }

    //        // Additional Data
    //        if (AdditionalInfo.analog_signal != 0 || strlen(AdditionalInfo.msg_resp_M) > 0) {
    //            Serial.println(">>> Additional Data <<<");
    //            Serial.print("  new_flag_M: ");
    //            Serial.println(AdditionalInfo.new_flag_M ? "true" : "false");
    //            Serial.print("  new_buttton_M: ");
    //            Serial.println(AdditionalInfo.new_buttton_M);
    //            Serial.print("  setMessageRead_M: ");
    //            Serial.println(AdditionalInfo.setMessageRead_M ? "true" : "false");
    //            Serial.print("  MessageRead_M: ");
    //            Serial.println(AdditionalInfo.MessageRead_M ? "true" : "false");
    //            Serial.print("  SOS_Sprite_on_off_M: ");
    //            Serial.println(AdditionalInfo.SOS_Sprite_on_off_M ? "true" : "false");
    //            Serial.print("  SOS_View_on_off_M: ");
    //            Serial.println(AdditionalInfo.SOS_View_on_off_M ? "true" : "false");
    //            Serial.print("  new_SOS_flag_M: ");
    //            Serial.println(AdditionalInfo.new_SOS_flag_M ? "true" : "false");
    //            Serial.print("  confirm_message_M: ");
    //            Serial.println(AdditionalInfo.confirm_message_M ? "true" : "false");
    //            Serial.print("  isValidGNSS_M: ");
    //            Serial.println(AdditionalInfo.isValidGNSS_M ? "true" : "false");
    //            Serial.print("  FLYRF_MODE_TEST_M: ");
    //            Serial.println(AdditionalInfo.FLYRF_MODE_TEST_M);
    //            Serial.print("  analog_signal: ");
    //            Serial.println(AdditionalInfo.analog_signal);
    //            if (strlen(AdditionalInfo.msg_resp_M) > 0) {
    //                Serial.print("  msg_resp_M: '");
    //                Serial.print(AdditionalInfo.msg_resp_M);
    //                Serial.println("'");
    //            }
    //        }
    //    }
    //    else 
    //    {
    //       /* Serial.println(">>> NO VALID DATA YET <<<");
    //        if (registerWriteCount > 0) 
    //        {
    //            Serial.println("Registers are being written - check data structure");
    //        }
    //        else 
    //        {
    //            Serial.println("No registers written - check Master connection");
    //        }*/
    //    }

    //  //  Serial.println("============================");
    //}

   // delay(10);
}