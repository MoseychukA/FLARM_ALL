// ===== Источник: ESP32S3, RS-485 Modbus RTU Master (ModbusMaster) =====
#include <Arduino.h>
#include <ModbusMaster.h>

// ---------------- Пины и параметры RS485 ----------------
static const int RS485_TX_PIN = 18;   // Serial2 TX
static const int RS485_RX_PIN = 17;   // Serial2 RX
static const int RS485_DE_RE = 21;  // Управление прием/передача
static const uint32_t RS485_BAUD = 115200;// 921600; // Можно уменьшить при необходимости

// ---------------- Идентификаторы Modbus -----------------
static const uint8_t SLAVE_ID = 1; // Приемник

// ---------------- Данные ----------------
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
    float     course;        /* CoG */
    float     speed;         /* knots */
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

// Экспортируемые массивы/структуры (как в вашем проекте)
ufo_t Container[MAX_TRACKING_OBJECTS];
ufo_t ThisAircraft;

// Дополнительные данные источника
uint16_t analog_code_M = 0;
bool     new_flag_M = false;
uint8_t  new_buttton_M = 0;
bool     setMessageRead_M = false;
bool     MessageRead_M = false;
bool     SOS_Sprite_on_off_M = false;
bool     SOS_View_on_off_M = false;
bool     new_SOS_flag_M = false;
bool     confirm_message_M = false;
char     msg_resp_M[60] = { 0 };
bool     isValidGNSS_M = false;
uint8_t  FLYRF_MODE_TEST_M = 0;

// ----------- Карта регистров -----------
static const uint16_t BASE_CONTAINER = 0x0000;
static const uint16_t OBJ_STRIDE = 64;    // регистров на объект
static const uint16_t BASE_THIS = 0x1000;
static const uint16_t BASE_EXTRA = 0x2000;

// Смещения внутри ufo_t (в регистрах)
enum {
    OFF_timestamp = 0,
    OFF_addr = 2,
    OFF_addr_type = 4,
    OFF_lat = 5,
    OFF_lon = 7,
    OFF_old_lat = 9,
    OFF_old_lon = 11,
    OFF_alt = 13,
    OFF_press_alt = 15,
    OFF_course = 17,
    OFF_speed = 19,
    OFF_aircraft_type = 21,
    OFF_flight = 22, // 8 regs
    OFF_vert_rate = 30,
    OFF_Squawk = 32,
    OFF_timemsg = 34,
    OFF_vs = 36,
    OFF_geoid = 38,
    OFF_hdop = 40,
    OFF_rssi = 41,
    OFF_distance = 42,
    OFF_bearing = 44,
    OFF_signal_src = 46,
    OFF_seen = 47,
    OFF_hour = 49,
    OFF_min = 50,
    OFF_delay = 51,
    OFF_callsign = 52 // 4 regs
};

// Доп. блок
static const uint16_t REG_ANALOG_CODE = BASE_EXTRA + 0x0000;
static const uint16_t REG_new_flag_M = BASE_EXTRA + 0x0001;
static const uint16_t REG_new_buttton_M = BASE_EXTRA + 0x0002;
static const uint16_t REG_setMessageRead_M = BASE_EXTRA + 0x0003;
static const uint16_t REG_MessageRead_M = BASE_EXTRA + 0x0004;
static const uint16_t REG_SOS_Sprite_on_off_M = BASE_EXTRA + 0x0005;
static const uint16_t REG_SOS_View_on_off_M = BASE_EXTRA + 0x0006;
static const uint16_t REG_new_SOS_flag_M = BASE_EXTRA + 0x0007;
static const uint16_t REG_confirm_message_M = BASE_EXTRA + 0x0008;
static const uint16_t REG_isValidGNSS_M = BASE_EXTRA + 0x0009;
static const uint16_t REG_FLYRF_MODE_TEST_M = BASE_EXTRA + 0x000A;
static const uint16_t REG_msg_resp_M_base = BASE_EXTRA + 0x0010; // 30 regs (60 bytes)

// ----------- Modbus master -----------
ModbusMaster node;

// ----------- Снапшоты для диффа -----------
ufo_t prevContainer[MAX_TRACKING_OBJECTS];
ufo_t prevThisAircraft;
uint16_t prev_analog_code_M;
bool     prev_new_flag_M;
uint8_t  prev_new_buttton_M;
bool     prev_setMessageRead_M;
bool     prev_MessageRead_M;
bool     prev_SOS_Sprite_on_off_M;
bool     prev_SOS_View_on_off_M;
bool     prev_new_SOS_flag_M;
bool     prev_confirm_message_M;
char     prev_msg_resp_M[60];
bool     prev_isValidGNSS_M;
uint8_t  prev_FLYRF_MODE_TEST_M;

// -------------- Утилиты упаковки --------------
static inline void splitU32(uint32_t v, uint16_t& lo, uint16_t& hi) {
    lo = (uint16_t)(v & 0xFFFF);
    hi = (uint16_t)((v >> 16) & 0xFFFF);
}
static inline uint32_t packF32(float f) {
    union { float f; uint32_t u; } u;
    u.f = f;
    return u.u;
}

// ---------------- Управление DE/RE ---------------
void preTransmission() {
    digitalWrite(RS485_DE_RE, HIGH); // TX enable
    delayMicroseconds(1);
}
void postTransmission() {
    delayMicroseconds(1);
    digitalWrite(RS485_DE_RE, LOW); // RX enable
}

// -------------- Функции отправки -----------------
bool writeReg16(uint16_t addr, uint16_t val) {
    node.clearResponseBuffer();
    uint8_t rc = node.writeSingleRegister(addr, val); // 0x06
    return rc == node.ku8MBSuccess;
}
bool writeReg32(uint16_t addr, uint32_t val) {
    uint16_t lo, hi;
    splitU32(val, lo, hi);
    node.clearResponseBuffer();
    node.setTransmitBuffer(0, lo);
    node.setTransmitBuffer(1, hi);
    uint8_t rc = node.writeMultipleRegisters(addr, 2); // 0x10
    return rc == node.ku8MBSuccess;
}
bool writeFloat(uint16_t addr, float f) {
    return writeReg32(addr, packF32(f));
}
bool writeBytesToRegs(uint16_t addr, const uint8_t* bytes, size_t nBytes) {
    // Упаковка по 2 байта в регистр (младший байт первым)
    size_t nRegs = (nBytes + 1) / 2;
    node.clearTransmitBuffer();
    for (size_t i = 0; i < nRegs; ++i) {
        uint16_t v = bytes[2 * i];
        if (2 * i + 1 < nBytes) v |= ((uint16_t)bytes[2 * i + 1]) << 8;
        node.setTransmitBuffer(i, v);
    }
    uint8_t rc = node.writeMultipleRegisters(addr, nRegs);
    return rc == node.ku8MBSuccess;
}

// Отправка одного объекта ufo_t по диффу
void sendUFOdiff(uint16_t baseAddr, const ufo_t& cur, const ufo_t& prev) {
    if (cur.timestamp != prev.timestamp) writeReg32(baseAddr + OFF_timestamp, (uint32_t)cur.timestamp);
    if (cur.addr != prev.addr)      writeReg32(baseAddr + OFF_addr, cur.addr);
    if (cur.addr_type != prev.addr_type) writeReg16(baseAddr + OFF_addr_type, cur.addr_type);

    if (cur.latitude != prev.latitude)  writeFloat(baseAddr + OFF_lat, cur.latitude);
    if (cur.longitude != prev.longitude) writeFloat(baseAddr + OFF_lon, cur.longitude);
    if (cur.old_latitude != prev.old_latitude)   writeFloat(baseAddr + OFF_old_lat, cur.old_latitude);
    if (cur.old_longitude != prev.old_longitude)  writeFloat(baseAddr + OFF_old_lon, cur.old_longitude);

    if (cur.altitude != prev.altitude)  writeFloat(baseAddr + OFF_alt, cur.altitude);
    if (cur.pressure_altitude != prev.pressure_altitude) writeFloat(baseAddr + OFF_press_alt, cur.pressure_altitude);
    if (cur.course != prev.course)    writeFloat(baseAddr + OFF_course, cur.course);
    if (cur.speed != prev.speed)     writeFloat(baseAddr + OFF_speed, cur.speed);

    if (cur.aircraft_type != prev.aircraft_type) writeReg16(baseAddr + OFF_aircraft_type, cur.aircraft_type);

    if (memcmp(cur.flight, prev.flight, sizeof(cur.flight)) != 0) {
        writeBytesToRegs(baseAddr + OFF_flight, (const uint8_t*)cur.flight, sizeof(cur.flight));
    }

    if (cur.vert_rate != prev.vert_rate) writeReg32(baseAddr + OFF_vert_rate, (uint32_t)cur.vert_rate);
    if (cur.Squawk != prev.Squawk)    writeReg32(baseAddr + OFF_Squawk, (uint32_t)cur.Squawk);
    if (cur.timemsg != prev.timemsg)   writeReg32(baseAddr + OFF_timemsg, (uint32_t)cur.timemsg);

    if (cur.vs != prev.vs)                         writeFloat(baseAddr + OFF_vs, cur.vs);
    if (cur.geoid_separation != prev.geoid_separation) writeFloat(baseAddr + OFF_geoid, cur.geoid_separation);

    if (cur.hdop != prev.hdop)   writeReg16(baseAddr + OFF_hdop, cur.hdop);
    if (cur.rssi != prev.rssi)   writeReg16(baseAddr + OFF_rssi, (int16_t)cur.rssi);

    if (cur.distance != prev.distance) writeFloat(baseAddr + OFF_distance, cur.distance);
    if (cur.bearing != prev.bearing)  writeFloat(baseAddr + OFF_bearing, cur.bearing);

    if (cur.signal_source != prev.signal_source) writeReg16(baseAddr + OFF_signal_src, cur.signal_source);

    if (cur.seen != prev.seen) writeReg32(baseAddr + OFF_seen, (uint32_t)cur.seen);

    if (cur.hour_msg != prev.hour_msg) writeReg16(baseAddr + OFF_hour, cur.hour_msg);
    if (cur.min_msg != prev.min_msg)  writeReg16(baseAddr + OFF_min, cur.min_msg);
    if (cur.delay_time_msg != prev.delay_time_msg) writeReg16(baseAddr + OFF_delay, cur.delay_time_msg);

    if (memcmp(cur.callsign, prev.callsign, sizeof(cur.callsign)) != 0) {
        writeBytesToRegs(baseAddr + OFF_callsign, (const uint8_t*)cur.callsign, sizeof(cur.callsign));
    }
}

// Отправка доп. блока по диффу
void sendExtraDiff() 
{
    if (analog_code_M != prev_analog_code_M) { writeReg16(REG_ANALOG_CODE, analog_code_M); prev_analog_code_M = analog_code_M; }
    if (new_flag_M != prev_new_flag_M) { writeReg16(REG_new_flag_M, new_flag_M ? 1 : 0); prev_new_flag_M = new_flag_M; }
    if (new_buttton_M != prev_new_buttton_M) { writeReg16(REG_new_buttton_M, new_buttton_M); prev_new_buttton_M = new_buttton_M; }
    if (setMessageRead_M != prev_setMessageRead_M) { writeReg16(REG_setMessageRead_M, setMessageRead_M ? 1 : 0); prev_setMessageRead_M = setMessageRead_M; }
    if (MessageRead_M != prev_MessageRead_M) { writeReg16(REG_MessageRead_M, MessageRead_M ? 1 : 0); prev_MessageRead_M = MessageRead_M; }
    if (SOS_Sprite_on_off_M != prev_SOS_Sprite_on_off_M) { writeReg16(REG_SOS_Sprite_on_off_M, SOS_Sprite_on_off_M ? 1 : 0); prev_SOS_Sprite_on_off_M = SOS_Sprite_on_off_M; }
    if (SOS_View_on_off_M != prev_SOS_View_on_off_M) { writeReg16(REG_SOS_View_on_off_M, SOS_View_on_off_M ? 1 : 0); prev_SOS_View_on_off_M = SOS_View_on_off_M; }
    if (new_SOS_flag_M != prev_new_SOS_flag_M) { writeReg16(REG_new_SOS_flag_M, new_SOS_flag_M ? 1 : 0); prev_new_SOS_flag_M = new_SOS_flag_M; }
    if (confirm_message_M != prev_confirm_message_M) { writeReg16(REG_confirm_message_M, confirm_message_M ? 1 : 0); prev_confirm_message_M = confirm_message_M; }
    if (isValidGNSS_M != prev_isValidGNSS_M) { writeReg16(REG_isValidGNSS_M, isValidGNSS_M ? 1 : 0); prev_isValidGNSS_M = isValidGNSS_M; }
    if (FLYRF_MODE_TEST_M != prev_FLYRF_MODE_TEST_M) { writeReg16(REG_FLYRF_MODE_TEST_M, FLYRF_MODE_TEST_M); prev_FLYRF_MODE_TEST_M = FLYRF_MODE_TEST_M; }

    if (memcmp(msg_resp_M, prev_msg_resp_M, sizeof(msg_resp_M)) != 0) {
        writeBytesToRegs(REG_msg_resp_M_base, (const uint8_t*)msg_resp_M, sizeof(msg_resp_M));
        memcpy(prev_msg_resp_M, msg_resp_M, sizeof(msg_resp_M));
    }
}

// -------------- Задача отправки (ядро 0) --------------
void modbusSendTask(void* arg) {
    // Изначальная синхронизация снапшотов
    memcpy(prevContainer, Container, sizeof(Container));
    prevThisAircraft = ThisAircraft;
    prev_analog_code_M = analog_code_M;
    prev_new_flag_M = new_flag_M;
    prev_new_buttton_M = new_buttton_M;
    prev_setMessageRead_M = setMessageRead_M;
    prev_MessageRead_M = MessageRead_M;
    prev_SOS_Sprite_on_off_M = SOS_Sprite_on_off_M;
    prev_SOS_View_on_off_M = SOS_View_on_off_M;
    prev_new_SOS_flag_M = new_SOS_flag_M;
    prev_confirm_message_M = confirm_message_M;
    prev_isValidGNSS_M = isValidGNSS_M;
    prev_FLYRF_MODE_TEST_M = FLYRF_MODE_TEST_M;
    memcpy(prev_msg_resp_M, msg_resp_M, sizeof(msg_resp_M));

    const TickType_t period = pdMS_TO_TICKS(50); // период сканирования/отправки
    TickType_t last = xTaskGetTickCount();

    for (;;) {
        // Отправляем дифф по контейнеру
        for (int i = 0; i < MAX_TRACKING_OBJECTS; ++i) 
        {
            uint16_t baseAddr = BASE_CONTAINER + i * OBJ_STRIDE;
            sendUFOdiff(baseAddr, Container[i], prevContainer[i]);
            // Обновляем локальный снапшот после отправки
            prevContainer[i] = Container[i];
        }
        // ThisAircraft
        sendUFOdiff(BASE_THIS, ThisAircraft, prevThisAircraft);
        prevThisAircraft = ThisAircraft;

        // Доп. блок
        sendExtraDiff();

        vTaskDelayUntil(&last, period);
    }
}

// -------------- Setup/Loop ----------------
void setup() {
    pinMode(RS485_DE_RE, OUTPUT);
    digitalWrite(RS485_DE_RE, LOW); // RX по умолчанию

    Serial.begin(115200);
    Serial.println("Setup start");

    Serial2.begin(RS485_BAUD, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
    node.begin(SLAVE_ID, Serial2);
    node.preTransmission(preTransmission);
    node.postTransmission(postTransmission);
    node.setResponseTimeout(250); // миллисекунды
    // Пример: инициализация тестовых данных (удалите/замените в реальном проекте)
    for (int i = 0; i < MAX_TRACKING_OBJECTS; ++i) 
    {
        Container[i].addr = i + 100;
        snprintf(Container[i].flight, sizeof(Container[i].flight), "FLT%03d", i);
    }
    strcpy(ThisAircraft.flight, "OWNPLANE");

    // Задача на ядре 0
    xTaskCreatePinnedToCore(modbusSendTask, "modbusSendTask", 8192, nullptr, 2, nullptr, 0);

    Serial.println("Setup END");
}

void loop() 
{

    analog_code_M = random(512, 2300);
    ThisAircraft.latitude = random(55.00000, 56.00000);

    delay(1000);

    // Остальная логика источника, обновляющая Container/ThisAircraft и доп. переменные
    // ...
}