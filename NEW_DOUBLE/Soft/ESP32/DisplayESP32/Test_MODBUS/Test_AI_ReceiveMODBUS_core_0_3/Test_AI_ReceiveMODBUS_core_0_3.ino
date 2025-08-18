//Скетч ПРИЕМНИК(ESP32S3 + ModbusRTU + Serial1 GPIO39 / 38, RE / DE = 40)
//Приемник — slave ID 1.
//Принимает записи в HREG по нашей карте адресов.
//Внутренний массив holding регистров живет в библиотеке; мы периодически переносим данные в структуры Container[] и ThisAircraft в отдельной задаче, закрепленной на ядре 0.
//Загрузите этот скетч в модуль‑приемник.

// ===== Приемник: ESP32S3, RS-485 Modbus RTU Slave (ModbusRTU) =====
#include <Arduino.h>
#include <ModbusRTU.h>

// ---------------- Пины и параметры RS485 ----------------
static const int RS485_TX_PIN = 39;   // Serial1 TX
static const int RS485_RX_PIN = 38;   // Serial1 RX
static const int RS485_DE_RE = 40;  // Управление прием/передача
static const uint32_t RS485_BAUD = 921600;

static const uint8_t SLAVE_ID = 1;

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

// Экспортируемые массивы/структуры (целевые)
ufo_t Container[MAX_TRACKING_OBJECTS];
ufo_t ThisAircraft;

// Доп. блок на приемнике (зеркало)
uint16_t analog_code_R = 0;
bool     new_flag_R = false;
uint8_t  new_buttton_R = 0;
bool     setMessageRead_R = false;
bool     MessageRead_R = false;
bool     SOS_Sprite_on_off_R = false;
bool     SOS_View_on_off_R = false;
bool     new_SOS_flag_R = false;
bool     confirm_message_R = false;
char     msg_resp_R[60] = { 0 };
bool     isValidGNSS_R = false;
uint8_t  FLYRF_MODE_TEST_R = 0;

// ----------- Карта регистров (должна совпадать) -----------
static const uint16_t BASE_CONTAINER = 0x0000;
static const uint16_t OBJ_STRIDE = 64;
static const uint16_t BASE_THIS = 0x1000;
static const uint16_t BASE_EXTRA = 0x2000;

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
    OFF_flight = 22,
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
    OFF_callsign = 52
};

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
static const uint16_t REG_msg_resp_M_base = BASE_EXTRA + 0x0010; // 0x2010..0x202D (30 регистров)

// Подсчет количества выделяемых регистров
static const uint16_t CONTAINER_REG_COUNT = MAX_TRACKING_OBJECTS * OBJ_STRIDE; // 8*64 = 512
static const uint16_t THIS_REG_COUNT = 64;
static const uint16_t EXTRA_REG_COUNT = 64;


// ======== ЛОГГЕР И ФЛАГИ ИЗМЕНЕНИЙ ========
#include <math.h>
extern "C" {
#include <string.h>
}

// Пороги для сравнения float
#ifndef EPS_LATLON
#define EPS_LATLON 1e-6f
#endif
#ifndef EPS_FLOAT
#define EPS_FLOAT  1e-3f
#endif

// ---- Флаги "грязных" областей, помечаются в cbSetHreg ----
static volatile bool dirtyContainer[MAX_TRACKING_OBJECTS] = { false };
static volatile bool dirtyThis = false;
static volatile bool dirtyExtra = false;

// ---- Снапшоты предыдущих значений для печати только изменений ----
static ufo_t prevContainer[MAX_TRACKING_OBJECTS] = {};
static bool  prevInitContainer[MAX_TRACKING_OBJECTS] = { false };

static ufo_t prevThisAircraft = {};
static bool  prevInitThisAircraft = false;

typedef struct ExtraInfoM {
    bool     new_flag_M;
    uint8_t  new_buttton_M;
    bool     setMessageRead_M;
    bool     MessageRead_M;
    bool     SOS_Sprite_on_off_M;
    bool     SOS_View_on_off_M;
    bool     new_SOS_flag_M;
    bool     confirm_message_M;
    char     msg_resp_M[60];
    bool     isValidGNSS_M;
    uint8_t  FLYRF_MODE_TEST_M;
    uint16_t analog_code_M;
} extra_m_t;

static extra_m_t prevExtra = {};
static bool      prevInitExtra = false;

// Построить extra_m_t из ваших _R переменных (зеркало)
static extra_m_t makeExtraFromR() 
{
    extra_m_t e;
    e.new_flag_M = new_flag_R;
    e.new_buttton_M = new_buttton_R;
    e.setMessageRead_M = setMessageRead_R;
    e.MessageRead_M = MessageRead_R;
    e.SOS_Sprite_on_off_M = SOS_Sprite_on_off_R;
    e.SOS_View_on_off_M = SOS_View_on_off_R;
    e.new_SOS_flag_M = new_SOS_flag_R;
    e.confirm_message_M = confirm_message_R;
    memcpy(e.msg_resp_M, msg_resp_R, sizeof(e.msg_resp_M));
    e.isValidGNSS_M = isValidGNSS_R;
    e.FLYRF_MODE_TEST_M = FLYRF_MODE_TEST_R;
    e.analog_code_M = analog_code_R;
    return e;
}

// ---- Утилиты сравнения ----
static inline bool diffF(float a, float b, float eps) { return fabsf(a - b) > eps; }
static inline bool diffI64(time_t a, time_t b) { return a != b; }
static inline bool diffU32(uint32_t a, uint32_t b) { return a != b; }
static inline bool diffU16(uint16_t a, uint16_t b) { return a != b; }
static inline bool diffU8(uint8_t a, uint8_t b) { return a != b; }
static inline bool diffI(int a, int b) { return a != b; }
static inline bool diffI8(int8_t a, int8_t b) { return a != b; }
static inline bool diffBool(bool a, bool b) { return a != b; }
static inline bool diffBytes(const void* a, const void* b, size_t n) { return memcmp(a, b, n) != 0; }

// ---- Печать UFO (полная/изменения) ----
static void printUFOFull(const ufo_t& o, int idx, const char* tag) 
{
    Serial.printf("[%s] UFO idx=%d ", tag, idx);
    Serial.printf("  timestamp: %llu ", (unsigned long long)o.timestamp);
    Serial.printf("  addr: %u, addr_type: %u ", (unsigned)o.addr, (unsigned)o.addr_type);
    Serial.printf("  lat/lon: %.6f / %.6f ", o.latitude, o.longitude);
    Serial.printf("  old_lat/lon: %.6f / %.6f ", o.old_latitude, o.old_longitude);
    Serial.printf("  alt: %.1f, p_alt: %.1f, course: %.1f, speed: %.1f ", o.altitude, o.pressure_altitude, o.course, o.speed);
    Serial.printf("  aircraft_type: %u ", (unsigned)o.aircraft_type);
    Serial.print("  flight: "); Serial.write(o.flight, sizeof(o.flight)); Serial.println();
    Serial.printf("  vert_rate: %d, Squawk: %d ", o.vert_rate, o.Squawk);
    Serial.printf("  timemsg: %llu ", (unsigned long long)o.timemsg);
    Serial.printf("  vs: %.1f, geoid_sep: %.2f, hdop: %u, rssi: %d ", o.vs, o.geoid_separation, (unsigned)o.hdop, (int)o.rssi);
    Serial.printf("  dist: %.2f, bearing: %.1f, signal_source: %u ", o.distance, o.bearing, (unsigned)o.signal_source);
    Serial.printf("  seen: %llu, hour_msg: %u, min_msg: %u, delay_msg: %u ",(unsigned long long)o.seen, (unsigned)o.hour_msg, (unsigned)o.min_msg, (unsigned)o.delay_time_msg);
    Serial.print("  callsign: "); Serial.write((char*)o.callsign, 8); Serial.println();
}

static void printUFOChanged(const ufo_t& a, const ufo_t& b, int idx) 
{
    Serial.printf("[CHG] UFO idx=%d ", idx);
    if (diffI64(a.timestamp, b.timestamp)) Serial.printf("  timestamp: %llu -> %llu ",(unsigned long long)a.timestamp,(unsigned long long)b.timestamp);
    if (diffU32(a.addr, b.addr)) Serial.printf("  addr: %u -> %u ",(unsigned)a.addr,(unsigned)b.addr);
    if (diffU8(a.addr_type, b.addr_type)) Serial.printf("  addr_type: %u -> %u ",(unsigned)a.addr_type,(unsigned)b.addr_type);
    if (diffF(a.latitude, b.latitude, EPS_LATLON) || diffF(a.longitude, b.longitude, EPS_LATLON))
    Serial.printf("  lat/lon: %.6f/%.6f -> %.6f/%.6f ", a.latitude,a.longitude,b.latitude,b.longitude);
    if (diffF(a.old_latitude, b.old_latitude, EPS_LATLON) || diffF(a.old_longitude, b.old_longitude, EPS_LATLON))
    Serial.printf("  old_lat/lon: %.6f/%.6f -> %.6f/%.6f", a.old_latitude,a.old_longitude,b.old_latitude,b.old_longitude);
    if (diffF(a.altitude, b.altitude, EPS_FLOAT)) Serial.printf("  altitude: %.1f -> %.1f ", a.altitude, b.altitude);
    if (diffF(a.pressure_altitude, b.pressure_altitude, EPS_FLOAT)) Serial.printf("  pressure_altitude: %.1f -> %.1f ", a.pressure_altitude, b.pressure_altitude);
    if (diffF(a.course, b.course, EPS_FLOAT)) Serial.printf("  course: %.1f -> %.1f ", a.course, b.course);
    if (diffF(a.speed, b.speed, EPS_FLOAT)) Serial.printf("  speed: %.1f -> %.1f", a.speed, b.speed);
    if (diffU8(a.aircraft_type, b.aircraft_type)) Serial.printf("  aircraft_type: %u -> %u",(unsigned)a.aircraft_type,(unsigned)b.aircraft_type);
    if (diffBytes(a.flight, b.flight, sizeof(a.flight))) {
    Serial.print("  flight: "); Serial.write(a.flight, sizeof(a.flight)); Serial.print(" -> ");
    Serial.write(b.flight, sizeof(b.flight)); Serial.println();
    }
    if (diffI(a.vert_rate, b.vert_rate)) Serial.printf("  vert_rate: %d -> %d ", a.vert_rate, b.vert_rate);
    if (diffI(a.Squawk, b.Squawk)) Serial.printf("  Squawk: %d -> %d ", a.Squawk, b.Squawk);
    if (diffI64(a.timemsg, b.timemsg)) Serial.printf("  timemsg: %llu -> %llu ",(unsigned long long)a.timemsg,(unsigned long long)b.timemsg);
    if (diffF(a.vs, b.vs, EPS_FLOAT)) Serial.printf("  vs: %.1f -> %.1f ", a.vs, b.vs);
    if (diffF(a.geoid_separation, b.geoid_separation, EPS_FLOAT)) Serial.printf("  geoid_separation: %.2f -> %.2f ", a.geoid_separation, b.geoid_separation);
    if (diffU16(a.hdop, b.hdop)) Serial.printf("  hdop: %u -> %u ",(unsigned)a.hdop,(unsigned)b.hdop);
    if (diffI8(a.rssi, b.rssi)) Serial.printf("  rssi: %d -> %d ",(int)a.rssi,(int)b.rssi);
    if (diffF(a.distance, b.distance, EPS_FLOAT)) Serial.printf("  distance: %.2f -> %.2f ", a.distance, b.distance);
    if (diffF(a.bearing, b.bearing, EPS_FLOAT)) Serial.printf("  bearing: %.1f -> %.1f ", a.bearing, b.bearing);
    if (diffU8(a.signal_source, b.signal_source)) Serial.printf("  signal_source: %u -> %u ",(unsigned)a.signal_source,(unsigned)b.signal_source);
    if (diffI64(a.seen, b.seen)) Serial.printf("  seen: %llu -> %llu ",(unsigned long long)a.seen,(unsigned long long)b.seen);
    if (diffU8(a.hour_msg, b.hour_msg)) Serial.printf("  hour_msg: %u -> %u ",(unsigned)a.hour_msg,(unsigned)b.hour_msg);
    if (diffU8(a.min_msg, b.min_msg)) Serial.printf("  min_msg: %u -> %u  ",(unsigned)a.min_msg,(unsigned)b.min_msg);
    if (diffU16(a.delay_time_msg, b.delay_time_msg)) Serial.printf("  delay_time_msg: %u -> %u ",(unsigned)a.delay_time_msg,(unsigned)b.delay_time_msg);
    if (diffBytes(a.callsign, b.callsign, sizeof(a.callsign))) {
    Serial.print("  callsign: "); Serial.write((char*)a.callsign, 8); Serial.print(" -> ");
    Serial.write((char*)b.callsign, 8); Serial.println();
    }
}

// ThisAircraft
static void printThisAircraftFull(const ufo_t& o) { printUFOFull(o, -1, "THIS"); }
static void printThisAircraftChanged(const ufo_t& a, const ufo_t& b) {
    Serial.println("[CHG] THIS");
    printUFOChanged(a, b, -1);
}

// Extra
static void printExtraFull(const extra_m_t& e) 
{
    Serial.println("[NEW] EXTRA");
    Serial.printf("  new_flag_M: %d ", e.new_flag_M);
    Serial.printf("  new_buttton_M: %u ", (unsigned)e.new_buttton_M);
    Serial.printf("  setMessageRead_M: %d, MessageRead_M: %d", e.setMessageRead_M, e.MessageRead_M);
    Serial.printf("  SOS_Sprite_on_off_M: %d, SOS_View_on_off_M: %d ", e.SOS_Sprite_on_off_M, e.SOS_View_on_off_M);
    Serial.printf("  new_SOS_flag_M: %d, confirm_message_M: %d ", e.new_SOS_flag_M, e.confirm_message_M);
    Serial.printf("  msg_resp_M: %.*s ", (int)sizeof(e.msg_resp_M), e.msg_resp_M);
    Serial.printf("  isValidGNSS_M: %d, FLYRF_MODE_TEST_M: %u ", e.isValidGNSS_M, (unsigned)e.FLYRF_MODE_TEST_M);
    Serial.printf("  analog_code_M: %u ", (unsigned)e.analog_code_M);
}
static void printExtraChanged(const extra_m_t& a, const extra_m_t& b) 
{
    Serial.println("[CHG] EXTRA");
    if (diffBool(a.new_flag_M, b.new_flag_M)) Serial.printf("  new_flag_M: %d -> %d", a.new_flag_M, b.new_flag_M);
    if (diffU8(a.new_buttton_M, b.new_buttton_M)) Serial.printf("  new_buttton_M: %u -> %u",(unsigned)a.new_buttton_M,(unsigned)b.new_buttton_M);
    if (diffBool(a.setMessageRead_M, b.setMessageRead_M)) Serial.printf("  setMessageRead_M: %d -> %d", a.setMessageRead_M, b.setMessageRead_M);
    if (diffBool(a.MessageRead_M, b.MessageRead_M)) Serial.printf("  MessageRead_M: %d -> %d", a.MessageRead_M, b.MessageRead_M);
    if (diffBool(a.SOS_Sprite_on_off_M, b.SOS_Sprite_on_off_M)) Serial.printf("  SOS_Sprite_on_off_M: %d -> %d", a.SOS_Sprite_on_off_M, b.SOS_Sprite_on_off_M);
    if (diffBool(a.SOS_View_on_off_M, b.SOS_View_on_off_M)) Serial.printf("  SOS_View_on_off_M: %d -> %d", a.SOS_View_on_off_M, b.SOS_View_on_off_M);
    if (diffBool(a.new_SOS_flag_M, b.new_SOS_flag_M)) Serial.printf("  new_SOS_flag_M: %d -> %d ", a.new_SOS_flag_M, b.new_SOS_flag_M);
    if (diffBool(a.confirm_message_M, b.confirm_message_M)) Serial.printf("  confirm_message_M: %d -> %d ", a.confirm_message_M, b.confirm_message_M);
    if (diffBytes(a.msg_resp_M, b.msg_resp_M, sizeof(a.msg_resp_M)))
    Serial.printf("  msg_resp_M: '%.*s' -> '%.*s' ", (int)sizeof(a.msg_resp_M), a.msg_resp_M, (int)sizeof(b.msg_resp_M), b.msg_resp_M);
    if (diffBool(a.isValidGNSS_M, b.isValidGNSS_M)) Serial.printf("  isValidGNSS_M: %d -> %d ", a.isValidGNSS_M, b.isValidGNSS_M);
    if (diffU8(a.FLYRF_MODE_TEST_M, b.FLYRF_MODE_TEST_M)) Serial.printf("  FLYRF_MODE_TEST_M: %u -> %u ",(unsigned)a.FLYRF_MODE_TEST_M,(unsigned)b.FLYRF_MODE_TEST_M);
    if (diffU16(a.analog_code_M, b.analog_code_M)) Serial.printf("  analog_code_M: %u -> %u ",(unsigned)a.analog_code_M,(unsigned)b.analog_code_M);
}

// ---- Публичные функции логирования (вызывать после pull...) ----
static void logContainerIfNewOrChanged(int i) {
    if (i < 0 || i >= MAX_TRACKING_OBJECTS) return;
    const ufo_t& cur = Container[i];
    if (!prevInitContainer[i]) {
        printUFOFull(cur, i, "NEW");
        prevContainer[i] = cur;
        prevInitContainer[i] = true;
        return;
    }
    bool isNew = diffI64(prevContainer[i].timestamp, cur.timestamp) ||
        diffU32(prevContainer[i].addr, cur.addr);
    if (isNew) {
        printUFOFull(cur, i, "NEW");
    }
    else {
        if (!diffBytes(&prevContainer[i], &cur, sizeof(ufo_t))) return;
        printUFOChanged(prevContainer[i], cur, i);
    }
    prevContainer[i] = cur;
}
static void logThisAircraftIfNewOrChanged() {
    const ufo_t& cur = ThisAircraft;
    if (!prevInitThisAircraft) {
        printThisAircraftFull(cur);
        prevThisAircraft = cur;
        prevInitThisAircraft = true;
        return;
    }
    bool isNew = diffI64(prevThisAircraft.timestamp, cur.timestamp) ||
        diffU32(prevThisAircraft.addr, cur.addr);
    if (isNew) {
        printThisAircraftFull(cur);
    }
    else {
        if (!diffBytes(&prevThisAircraft, &cur, sizeof(ufo_t))) return;
        printThisAircraftChanged(prevThisAircraft, cur);
    }
    prevThisAircraft = cur;
}
static void logExtraIfNewOrChanged(const extra_m_t& cur) {
    if (!prevInitExtra) {
        printExtraFull(cur);
        prevExtra = cur;
        prevInitExtra = true;
        return;
    }
    if (!diffBytes(&prevExtra, &cur, sizeof(cur))) return;
    printExtraChanged(prevExtra, cur);
    prevExtra = cur;
}






// -------------- Modbus RTU slave --------------
ModbusRTU mb;

// -------------- Утилиты распаковки --------------
static inline uint32_t joinU32(uint16_t lo, uint16_t hi) {
    return ((uint32_t)hi << 16) | lo;
}
static inline float u32ToF(uint32_t u) {
    union { uint32_t u; float f; } v;
    v.u = u;
    return v.f;
}
static inline void regsToBytes(uint16_t regVal, uint8_t& b0, uint8_t& b1) {
    b0 = (uint8_t)(regVal & 0xFF);
    b1 = (uint8_t)((regVal >> 8) & 0xFF);
}

// -------------- Хэндлер записи регистров --------------
// Здесь можно логировать/фильтровать. Мы просто разрешаем запись.
//bool cbSetHreg(TRegister* reg, uint16_t val) {
//    reg->value = val;
//    return true;
//}


//bool cbSetHreg(TRegister* reg, uint16_t val) {
//    reg->value = val;
//
//    uint16_t a = reg->address; // адрес изменённого регистра
//    if (a >= BASE_CONTAINER && a < BASE_CONTAINER + CONTAINER_REG_COUNT) {
//        int idx = (a - BASE_CONTAINER) / OBJ_STRIDE;
//        if (idx >= 0 && idx < MAX_TRACKING_OBJECTS) dirtyContainer[idx] = true;
//    }
//    else if (a >= BASE_THIS && a < BASE_THIS + THIS_REG_COUNT) {
//        dirtyThis = true;
//    }
//    else if (a >= BASE_EXTRA && a < BASE_EXTRA + EXTRA_REG_COUNT) {
//        dirtyExtra = true;
//    }
//    return true;
//}

uint16_t cbSetHreg(TRegister* reg, uint16_t val)
{
    reg->value = val;

    // Убедимся, что это запись в Holding Register
    if (!reg->address.isHreg()) {
        return Modbus::EX_SUCCESS;
    }

    uint16_t a = reg->address.address; // номер изменённого HREG

    // Помечаем «грязные» области по адресу
    if (a >= BASE_CONTAINER && a < BASE_CONTAINER + CONTAINER_REG_COUNT) {
        int idx = (a - BASE_CONTAINER) / OBJ_STRIDE;
        if (idx >= 0 && idx < MAX_TRACKING_OBJECTS) {
            dirtyContainer[idx] = true;
        }
    }
    else if (a >= BASE_THIS && a < BASE_THIS + THIS_REG_COUNT) {
        dirtyThis = true;
    }
    else if (a >= BASE_EXTRA && a < BASE_EXTRA + EXTRA_REG_COUNT) {
        dirtyExtra = true;
    }

    return Modbus::EX_SUCCESS;
}






// -------------- Копирование из HREG в структуры --------------
void pullUFOFromRegs(uint16_t baseAddr, ufo_t& u) {
    auto H = [&](uint16_t a) { return mb.Hreg(a); };

    u.timestamp = (time_t)joinU32(H(baseAddr + OFF_timestamp), H(baseAddr + OFF_timestamp + 1));
    u.addr = joinU32(H(baseAddr + OFF_addr), H(baseAddr + OFF_addr + 1));
    u.addr_type = (uint8_t)H(baseAddr + OFF_addr_type);

    u.latitude = u32ToF(joinU32(H(baseAddr + OFF_lat), H(baseAddr + OFF_lat + 1)));
    u.longitude = u32ToF(joinU32(H(baseAddr + OFF_lon), H(baseAddr + OFF_lon + 1)));
    u.old_latitude = u32ToF(joinU32(H(baseAddr + OFF_old_lat), H(baseAddr + OFF_old_lat + 1)));
    u.old_longitude = u32ToF(joinU32(H(baseAddr + OFF_old_lon), H(baseAddr + OFF_old_lon + 1)));

    u.altitude = u32ToF(joinU32(H(baseAddr + OFF_alt), H(baseAddr + OFF_alt + 1)));
    u.pressure_altitude = u32ToF(joinU32(H(baseAddr + OFF_press_alt), H(baseAddr + OFF_press_alt + 1)));
    u.course = u32ToF(joinU32(H(baseAddr + OFF_course), H(baseAddr + OFF_course + 1)));
    u.speed = u32ToF(joinU32(H(baseAddr + OFF_speed), H(baseAddr + OFF_speed + 1)));

    u.aircraft_type = (uint8_t)H(baseAddr + OFF_aircraft_type);

    // flight[16] -> 8 регистров
    for (int i = 0; i < 8; ++i) {
        uint16_t r = H(baseAddr + OFF_flight + i);
        regsToBytes(r, (uint8_t&)u.flight[2 * i], (uint8_t&)u.flight[2 * i + 1]);
    }

    u.vert_rate = (int)(int32_t)joinU32(H(baseAddr + OFF_vert_rate), H(baseAddr + OFF_vert_rate + 1));
    u.Squawk = (int)(int32_t)joinU32(H(baseAddr + OFF_Squawk), H(baseAddr + OFF_Squawk + 1));
    u.timemsg = (time_t)joinU32(H(baseAddr + OFF_timemsg), H(baseAddr + OFF_timemsg + 1));

    u.vs = u32ToF(joinU32(H(baseAddr + OFF_vs), H(baseAddr + OFF_vs + 1)));
    u.geoid_separation = u32ToF(joinU32(H(baseAddr + OFF_geoid), H(baseAddr + OFF_geoid + 1)));

    u.hdop = H(baseAddr + OFF_hdop);
    u.rssi = (int8_t)(H(baseAddr + OFF_rssi) & 0xFF);

    u.distance = u32ToF(joinU32(H(baseAddr + OFF_distance), H(baseAddr + OFF_distance + 1)));
    u.bearing = u32ToF(joinU32(H(baseAddr + OFF_bearing), H(baseAddr + OFF_bearing + 1)));

    u.signal_source = (uint8_t)H(baseAddr + OFF_signal_src);
    u.seen = (time_t)joinU32(H(baseAddr + OFF_seen), H(baseAddr + OFF_seen + 1));

    u.hour_msg = (uint8_t)H(baseAddr + OFF_hour);
    u.min_msg = (uint8_t)H(baseAddr + OFF_min);
    u.delay_time_msg = H(baseAddr + OFF_delay);

    for (int i = 0; i < 4; ++i) {
        uint16_t r = H(baseAddr + OFF_callsign + i);
        regsToBytes(r, u.callsign[2 * i], u.callsign[2 * i + 1]);
    }
}

void pullExtraFromRegs() {
    analog_code_R = mb.Hreg(REG_ANALOG_CODE);
    new_flag_R = mb.Hreg(REG_new_flag_M) != 0;
    new_buttton_R = (uint8_t)mb.Hreg(REG_new_buttton_M);
    setMessageRead_R = mb.Hreg(REG_setMessageRead_M) != 0;
    MessageRead_R = mb.Hreg(REG_MessageRead_M) != 0;
    SOS_Sprite_on_off_R = mb.Hreg(REG_SOS_Sprite_on_off_M) != 0;
    SOS_View_on_off_R = mb.Hreg(REG_SOS_View_on_off_M) != 0;
    new_SOS_flag_R = mb.Hreg(REG_new_SOS_flag_M) != 0;
    confirm_message_R = mb.Hreg(REG_confirm_message_M) != 0;
    isValidGNSS_R = mb.Hreg(REG_isValidGNSS_M) != 0;
    FLYRF_MODE_TEST_R = (uint8_t)mb.Hreg(REG_FLYRF_MODE_TEST_M);

    for (int i = 0; i < 30; ++i) {
        uint16_t r = mb.Hreg(REG_msg_resp_M_base + i);
        regsToBytes(r, (uint8_t&)msg_resp_R[2 * i], (uint8_t&)msg_resp_R[2 * i + 1]);
    }
}

// -------------- Задача обновления структур --------------
//void updateTask(void* arg) {
//    const TickType_t period = pdMS_TO_TICKS(50);
//    for (;;) {
//        // Цикл Modbus
//        mb.task();
//
//        // Обновляем контейнеры
//        for (int i = 0; i < MAX_TRACKING_OBJECTS; ++i) {
//            uint16_t baseAddr = BASE_CONTAINER + i * OBJ_STRIDE;
//            pullUFOFromRegs(baseAddr, Container[i]);
//        }
//        pullUFOFromRegs(BASE_THIS, ThisAircraft);
//        pullExtraFromRegs();
//
//        vTaskDelay(period);
//    }
//}

void updateTask(void* arg) {
    const TickType_t period = pdMS_TO_TICKS(50);
    for (;;) {
        mb.task();

        // Обновляем контейнеры из HREG
        for (int i = 0; i < MAX_TRACKING_OBJECTS; ++i) {
            uint16_t baseAddr = BASE_CONTAINER + i * OBJ_STRIDE;
            pullUFOFromRegs(baseAddr, Container[i]);

            // Если были записи в регистры этого объекта — логируем
            if (dirtyContainer[i]) {
                logContainerIfNewOrChanged(i);
                dirtyContainer[i] = false;
            }
        }

        // ThisAircraft
        pullUFOFromRegs(BASE_THIS, ThisAircraft);
        if (dirtyThis) {
            logThisAircraftIfNewOrChanged();
            dirtyThis = false;
        }

        // Extra
        pullExtraFromRegs();
        if (dirtyExtra) {
            extra_m_t curExtra = makeExtraFromR();
            logExtraIfNewOrChanged(curExtra);
            dirtyExtra = false;
        }

        vTaskDelay(period);
    }
}



void setup() {
    Serial.begin(115200);
    Serial.println("Setup start");
    Serial1.begin(RS485_BAUD, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
    mb.begin(&Serial1, RS485_DE_RE); // Библиотека сама управляет RE/DE пином
    mb.slave(SLAVE_ID);

    // Выделяем Holding Registers для всех областей
    mb.addHreg(BASE_CONTAINER, 0, CONTAINER_REG_COUNT); // 0x0000..0x01FF
    mb.addHreg(BASE_THIS, 0, THIS_REG_COUNT);      // 0x1000..0x103F
    mb.addHreg(BASE_EXTRA, 0, EXTRA_REG_COUNT);     // 0x2000..0x203F

    // Разрешаем запись в HREG (можно оставить один общий колбэк)
    mb.onSetHreg(BASE_CONTAINER, cbSetHreg, CONTAINER_REG_COUNT);
    mb.onSetHreg(BASE_THIS, cbSetHreg, THIS_REG_COUNT);
    mb.onSetHreg(BASE_EXTRA, cbSetHreg, EXTRA_REG_COUNT);

    // Задача на ядре 0
    xTaskCreatePinnedToCore(updateTask, "updateTask", 8192, nullptr, 2, nullptr, 0);
    Serial.println("Setup END");
}

float /*ThisAircraft.*/latitude_old;



void loop() {
    // Можно добавить валидацию, таймстемпы «видели/не видели», и т.п.
    // Основная работа идет в updateTask() + mb.task()

    if (ThisAircraft.latitude != latitude_old)
    {

        latitude_old = ThisAircraft.latitude;
        Serial.println(latitude_old);

    }


}




//Диаграмма взаимодействия
//Примечания и рекомендации :
//
//Контроль времени 0.3 с : при высокой скорости и «дифф‑стратегии» скетч передает только измененные регистры; даже если изменилось много, при 921600 бод укладывается с запасом.При необходимости увеличьте частоту опроса(например, 20 мс) или агрегируйте несколько соседних изменений в один 0x10 пакет.
//Надежность RS‑485 : используйте терминаторы 120Ω на концах шины, подтяжки «fail‑safe»(A к Vcc, B к GND через резисторы 680Ω–1kΩ), экранированный кабель, общую землю.
//Если 921600 оказывается нестабильно на ваших преобразователях RS‑485 — попробуйте 460800 или 230400. Оба варианта с дифф‑передачей также должны укладываться в 0.3 с.
//Порядок слов / байт для 32‑бит совпадает на обеих сторонах(младшее слово сначала).Если вы захотите поменять порядок, измените функции writeReg32 / writeFloat и joinU32 на обеих сторонах согласованно.
//В примерах оставлены заделы / резерв в блоках регистров(stride = 64).Это позволит безболезненно расширять структуру ufo_t.
//Готов помочь адаптировать под ваши реальные источники данных(GNSS / датчики), добавить фильтры «изменения с порогом» для float, логи, подтверждения / ретраи и др.
//
//Ниже — готовое решение под Apduino IDE для двух модулей ESP32‑S3 с связью по RS‑485 через MODBUS RTU.Источник работает мастером(ModbusMaster.h), приемник — слейвом(ModbusRTU).Реализованы:
//
//FreeRTOS - задачи, закрепленные на ядре 0
//Передача только измененных данных(CRC32 по блоку)
//Карта регистров MODBUS(ThisAircraft + Container[8] + служебные / флаговые поля + аналоговый код)
//Скорость обмена высокая(по умолчанию 460800 бод), полный пакет укладывается намного быстрее 0.3 сек
//Управление DE / RE RS485 : Источник GPIO21, Приемник GPIO40
//Пины UART : Источник Serial2 TX18 RX17, Приемник Serial1 TX39 RX38
//Установите библиотеки :
//
//ModbusMaster(для мастера)
//ModbusRTU(emelianov / ModbusRTU для слейва)
//Схема задач и потоков данных
//
//Карта регистров MODBUS(Holding Registers, 16 бит)
//
//SLAVE_ID = 1
//Блок служебных / флагов: HREG_BASE_CTRL = 0x0000, HREG_CTRL_COUNT = 34
//[0] analog_code(u16)
//[1] flags_bitmask(биты : 0 new_flag_M, 1 setMessageRead_M, 2 MessageRead_M, 3 SOS_Sprite_on_off_M, 4 SOS_View_on_off_M, 5 new_SOS_flag_M, 6 confirm_message_M, 7 isValidGNSS_M)
//[2] new_buttton_M(u8 в младшем байте)
//[3] FLYRF_MODE_TEST_M(u8 в младшем байте)
//[4] msg_len(0..60, байты)
//[5..34] msg_resp_M как 30 регистров(по 2 байта / регистр, Big - endian внутри регистра : [hi = byte0, lo = byte1] )
//Блок ThisAircraft : HREG_BASE_THIS = 0x0100, UFO_REGS = 64 (IEEE754 float как 2 регистра, u32 как 2 регистра, u64 как 4 регистра, строки как массив байт по 2 в регистр)
//Массив Container[i] : HREG_BASE_CONT = 0x0200, блок i : base_i = 0x0200 + i * UFO_REGS, i = 0..7, размер блока UFO_REGS = 64
//Примечание по упорядочиванию слов : для полей 32 / 64 бит используется порядок “старшее слово — младшее слово”(Modbus Big - Endian по регистрам).Для строк — 2 байта в регистре : старший байт первый.
//
//Скорость / тайминги
//
//UART : 460800 бод, 8N1.Полный пакет(ThisAircraft + 8 контейнеров + флаги / сообщение) переносится за < 0.05 сек при 460800 бод.Можно поднять до 921600, если железо и линии позволяют.
//    Источник(ESP32‑S3, мастер, ModbusMaster, Serial2 TX18 / RX17, RS485 DE GPIO21)
//    Файл : esp32s3_source_master.ino
//#include <Arduino.h>
//#include <ModbusMaster.h>
//
//    // -------------------- Пользовательские данные --------------------
//#define MAX_TRACKING_OBJECTS 8
//
//    typedef struct UFO {
//    time_t    timestamp;
//    uint32_t  addr;
//    uint8_t   addr_type;
//    float     latitude;
//    float     longitude;
//    float     old_latitude;
//    float     old_longitude;
//    float     altitude;
//    float     pressure_altitude;
//    float     course;        /* CoG */
//    float     speed;         /* knots */
//    uint8_t   aircraft_type;
//    char      flight[16];    // Flight number
//    int       vert_rate;     // Vertical rate
//    int       Squawk;        // Squawk
//    time_t    timemsg;       // time of coord message
//    float     vs;            // feet per minute
//    float     geoid_separation; // metres
//    uint16_t  hdop;          // cm
//    int8_t    rssi;          // SX1276 only
//    float     distance;
//    float     bearing;
//    uint8_t   signal_source;
//    time_t    seen;
//    uint8_t   hour_msg;
//    uint8_t   min_msg;
//    uint16_t  delay_time_msg;
//    uint8_t   callsign[8];
//} ufo_t;
//
//// Эти объекты должны быть определены в вашем проекте.
//// Здесь объявляем только extern.
//extern ufo_t Container[MAX_TRACKING_OBJECTS];
//extern ufo_t ThisAircraft;
//
//// Доп. передаваемые данные (объявлены у вас в проекте)
//extern bool     new_flag_M;
//extern uint8_t  new_buttton_M;
//extern bool     setMessageRead_M;
//extern bool     MessageRead_M;
//extern bool     SOS_Sprite_on_off_M;
//extern bool     SOS_View_on_off_M;
//extern bool     new_SOS_flag_M;
//extern bool     confirm_message_M;
//extern char     msg_resp_M[60];
//extern bool     isValidGNSS_M;
//extern uint8_t  FLYRF_MODE_TEST_M;
//extern uint16_t analog_code_M;
//
//// -------------------- RS485 / MODBUS --------------------
//#define RS485_DE_PIN     21
//#define UART_TX_PIN      18
//#define UART_RX_PIN      17
//#define UART_BAUD        460800
//
//#define SLAVE_ID         1
//
//// Карта регистров
//#define HREG_BASE_CTRL   0x0000
//#define HREG_CTRL_COUNT  34
//
//#define HREG_BASE_THIS   0x0100
//#define UFO_REGS         64
//
//#define HREG_BASE_CONT   0x0200
//
//ModbusMaster node;
//
//// -------------------- CRC32 --------------------
//static uint32_t crc32_update(uint32_t crc, uint8_t data) {
//    crc ^= data;
//    for (int i = 0; i < 8; i++) {
//        if (crc & 1) crc = (crc >> 1) ^ 0xEDB88320UL;
//        else crc >>= 1;
//    }
//    return crc;
//}
//static uint32_t crc32_buf(const uint8_t* buf, size_t len) {
//    uint32_t crc = 0xFFFFFFFFUL;
//    for (size_t i = 0; i < len; ++i) crc = crc32_update(crc, buf[i]);
//    return ~crc;
//}
//
//// -------------------- Пакетизация UFO -> регистры --------------------
//static inline void put_u16(uint16_t* regs, uint16_t& idx, uint16_t v) {
//    regs[idx++] = v;
//}
//static inline void put_u32(uint16_t* regs, uint16_t& idx, uint32_t v) {
//    regs[idx++] = (uint16_t)((v >> 16) & 0xFFFF);
//    regs[idx++] = (uint16_t)(v & 0xFFFF);
//}
//static inline void put_u64(uint16_t* regs, uint16_t& idx, uint64_t v) {
//    regs[idx++] = (uint16_t)((v >> 48) & 0xFFFF);
//    regs[idx++] = (uint16_t)((v >> 32) & 0xFFFF);
//    regs[idx++] = (uint16_t)((v >> 16) & 0xFFFF);
//    regs[idx++] = (uint16_t)(v & 0xFFFF);
//}
//static inline void put_f32(uint16_t* regs, uint16_t& idx, float f) {
//    union { float f; uint32_t u; } u; u.f = f;
//    put_u32(regs, idx, u.u);
//}
//static inline void put_i32(uint16_t* regs, uint16_t& idx, int32_t v) {
//    put_u32(regs, idx, (uint32_t)v);
//}
//static inline void put_i8_u8(uint16_t* regs, uint16_t& idx, int8_t v) {
//    // пишем как знаковый в младший байт, старший = 0
//    regs[idx++] = (uint16_t)((uint8_t)v);
//}
//static inline void put_u8(uint16_t* regs, uint16_t& idx, uint8_t v) {
//    regs[idx++] = (uint16_t)v;
//}
//static inline void put_bytes_as_regs(uint16_t* regs, uint16_t& idx, const uint8_t* bytes, size_t n) {
//    for (size_t i = 0; i < n; i += 2) {
//        uint8_t b0 = bytes[i];
//        uint8_t b1 = (i + 1 < n) ? bytes[i + 1] : 0;
//        uint16_t word = ((uint16_t)b0 << 8) | b1; // Big-endian внутри регистра
//        regs[idx++] = word;
//    }
//}
//
//static uint16_t pack_ufo_to_regs(const ufo_t& u, uint16_t* regs, uint16_t maxRegs) {
//    uint16_t i = 0;
//    // Важно: количество пушей = UFO_REGS (64)
//    // time_t считаем 64-битным
//    put_u64(regs, i, (uint64_t)u.timestamp);
//    put_u32(regs, i, u.addr);
//    put_u8(regs, i, u.addr_type);
//    put_f32(regs, i, u.latitude);
//    put_f32(regs, i, u.longitude);
//    put_f32(regs, i, u.old_latitude);
//    put_f32(regs, i, u.old_longitude);
//    put_f32(regs, i, u.altitude);
//    put_f32(regs, i, u.pressure_altitude);
//    put_f32(regs, i, u.course);
//    put_f32(regs, i, u.speed);
//    put_u8(regs, i, u.aircraft_type);
//    put_bytes_as_regs(regs, i, (const uint8_t*)u.flight, 16);
//    put_i32(regs, i, u.vert_rate);
//    put_i32(regs, i, u.Squawk);
//    put_u64(regs, i, (uint64_t)u.timemsg);
//    put_f32(regs, i, u.vs);
//    put_f32(regs, i, u.geoid_separation);
//    put_u16(regs, i, u.hdop);
//    put_i8_u8(regs, i, u.rssi);
//    put_f32(regs, i, u.distance);
//    put_f32(regs, i, u.bearing);
//    put_u8(regs, i, u.signal_source);
//    put_u64(regs, i, (uint64_t)u.seen);
//    put_u8(regs, i, u.hour_msg);
//    put_u8(regs, i, u.min_msg);
//    put_u16(regs, i, u.delay_time_msg);
//    put_bytes_as_regs(regs, i, (const uint8_t*)u.callsign, 8);
//
//    // Если где-то меньше/больше — можно дополнить нулями или проверить i == UFO_REGS
//    while (i < UFO_REGS) regs[i++] = 0;
//    return UFO_REGS;
//}
//
//// Блок флагов/сообщения -> регистры
//static uint16_t pack_ctrl_to_regs(uint16_t* regs, uint16_t maxRegs) {
//    uint16_t i = 0;
//    regs[i++] = analog_code_M;
//
//    uint16_t flags = 0;
//    flags |= (new_flag_M ? 1 : 0) << 0;
//    flags |= (setMessageRead_M ? 1 : 0) << 1;
//    flags |= (MessageRead_M ? 1 : 0) << 2;
//    flags |= (SOS_Sprite_on_off_M ? 1 : 0) << 3;
//    flags |= (SOS_View_on_off_M ? 1 : 0) << 4;
//    flags |= (new_SOS_flag_M ? 1 : 0) << 5;
//    flags |= (confirm_message_M ? 1 : 0) << 6;
//    flags |= (isValidGNSS_M ? 1 : 0) << 7;
//    regs[i++] = flags;
//
//    regs[i++] = (uint16_t)new_buttton_M;
//    regs[i++] = (uint16_t)FLYRF_MODE_TEST_M;
//
//    // msg_resp_M: до 60 байт
//    size_t msg_len = strnlen(msg_resp_M, 60);
//    regs[i++] = (uint16_t)msg_len;
//    // Упаковать как 30 регистров максимум
//    for (size_t k = 0; k < 30; ++k) {
//        uint8_t b0 = (2 * k < msg_len) ? (uint8_t)msg_resp_M[2 * k] : 0;
//        uint8_t b1 = (2 * k + 1 < msg_len) ? (uint8_t)msg_resp_M[2 * k + 1] : 0;
//        regs[i++] = ((uint16_t)b0 << 8) | b1;
//    }
//
//    // Заполнить до HREG_CTRL_COUNT
//    while (i < HREG_CTRL_COUNT) regs[i++] = 0;
//    return HREG_CTRL_COUNT;
//}
//
//// -------------------- Вспомогательные --------------------
//static void preTransmission() { digitalWrite(RS485_DE_PIN, HIGH); }
//static void postTransmission() { digitalWrite(RS485_DE_PIN, LOW); }
//
//static uint8_t write_regs_block(uint16_t startAddr, const uint16_t* regs, uint16_t qty) {
//    // Разбивать не нужно: qty <= 64
//    node.clearTransmitBuffer();
//    for (uint16_t i = 0; i < qty; ++i) node.setTransmitBuffer(i, regs[i]);
//    return node.writeMultipleRegisters(startAddr, qty);
//}
//
//// -------------------- Отслеживание изменений --------------------
//static uint32_t last_crc_this = 0;
//static uint32_t last_crc_cont[MAX_TRACKING_OBJECTS] = { 0 };
//static uint32_t last_crc_ctrl = 0;
//
//static void txTask(void* arg) {
//    static uint16_t regs[UFO_REGS];
//    static uint16_t ctrl[HREG_CTRL_COUNT];
//
//    const TickType_t period = pdMS_TO_TICKS(20);
//
//    for (;;) {
//        // ThisAircraft
//        uint16_t n = pack_ufo_to_regs(ThisAircraft, regs, UFO_REGS);
//        uint32_t crc = crc32_buf((uint8_t*)regs, n * 2);
//        if (crc != last_crc_this) {
//            uint8_t res = write_regs_block(HREG_BASE_THIS, regs, n);
//            if (res == node.ku8MBSuccess) last_crc_this = crc;
//        }
//
//        // Container[i]
//        for (int i = 0; i < MAX_TRACKING_OBJECTS; ++i) {
//            n = pack_ufo_to_regs(Container[i], regs, UFO_REGS);
//            crc = crc32_buf((uint8_t*)regs, n * 2);
//            if (crc != last_crc_cont[i]) {
//                uint16_t base = HREG_BASE_CONT + i * UFO_REGS;
//                uint8_t res = write_regs_block(base, regs, n);
//                if (res == node.ku8MBSuccess) last_crc_cont[i] = crc;
//                // небольшая пауза между блоками
//                vTaskDelay(pdMS_TO_TICKS(2));
//            }
//        }
//
//        // Флаги/сообщение/аналог
//        uint16_t m = pack_ctrl_to_regs(ctrl, HREG_CTRL_COUNT);
//        uint32_t c2 = crc32_buf((uint8_t*)ctrl, m * 2);
//        if (c2 != last_crc_ctrl) {
//            uint8_t res = write_regs_block(HREG_BASE_CTRL, ctrl, m);
//            if (res == node.ku8MBSuccess) last_crc_ctrl = c2;
//        }
//
//        vTaskDelay(period);
//    }
//}
//
//void setup() {
//    // RS485 DE
//    pinMode(RS485_DE_PIN, OUTPUT);
//    digitalWrite(RS485_DE_PIN, LOW);
//
//    // UART
//    Serial2.begin(UART_BAUD, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
//
//    // Modbus Master
//    node.begin(SLAVE_ID, Serial2);
//    node.preTransmission(preTransmission);
//    node.postTransmission(postTransmission);
//    node.setTimeout(50); // мс
//
//    // Создаем задачу на ядре 0
//    xTaskCreatePinnedToCore(txTask, "txTask", 8192, nullptr, 2, nullptr, 0);
//}
//
//void loop() {
//    // пусто: все делает FreeRTOS на ядре 0
//}



//
////==========================================================================
//
////Приемник(ESP32‑S3, слейв, ModbusRTU, Serial1 TX39 / RX38, RS485 DE GPIO40)
////Файл: esp32s3_receiver_slave.ino
//#include <Arduino.h>
//#include <ModbusRTU.h>
//
//// -------------------- Пользовательские данные --------------------
//#define MAX_TRACKING_OBJECTS 8
//
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
//    float     course;        /* CoG */
//    float     speed;         /* knots */
//    uint8_t   aircraft_type;
//    char      flight[16];    // Flight number
//    int       vert_rate;     // Vertical rate
//    int       Squawk;        // Squawk
//    time_t    timemsg;       // time of coord message
//    float     vs;            // feet per minute
//    float     geoid_separation; // metres
//    uint16_t  hdop;          // cm
//    int8_t    rssi;          // SX1276 only
//    float     distance;
//    float     bearing;
//    uint8_t   signal_source;
//    time_t    seen;
//    uint8_t   hour_msg;
//    uint8_t   min_msg;
//    uint16_t  delay_time_msg;
//    uint8_t   callsign[8];
//} ufo_t;
//
//// Эти объекты здесь определяем (приемник пишет сюда)
//ufo_t Container[MAX_TRACKING_OBJECTS];
//ufo_t ThisAircraft;
//
//// Принятые доп. поля
//volatile uint16_t analog_code_R = 0;
//volatile bool     new_flag_R = false;
//volatile uint8_t  new_buttton_R = 0;
//volatile bool     setMessageRead_R = false;
//volatile bool     MessageRead_R = false;
//volatile bool     SOS_Sprite_on_off_R = false;
//volatile bool     SOS_View_on_off_R = false;
//volatile bool     new_SOS_flag_R = false;
//volatile bool     confirm_message_R = false;
//volatile bool     isValidGNSS_R = false;
//volatile uint8_t  FLYRF_MODE_TEST_R = 0;
//char              msg_resp_R[61] = { 0 }; // 60 + '\0'
//
//// -------------------- RS485 / MODBUS --------------------
//#define RS485_DE_PIN     40
//#define UART_TX_PIN      39
//#define UART_RX_PIN      38
//#define UART_BAUD        921600// 460800
//
//#define SLAVE_ID         1
//
//#define HREG_BASE_CTRL   0x0000
//#define HREG_CTRL_COUNT  34
//
//#define HREG_BASE_THIS   0x0100
//#define UFO_REGS         64
//
//#define HREG_BASE_CONT   0x0200
//
//ModbusRTU mb;
//
//// Локальные зеркала регистров (быстро считать пакетом)
//static uint16_t regs_ctrl[HREG_CTRL_COUNT];
//static uint16_t regs_this[UFO_REGS];
//static uint16_t regs_cont[MAX_TRACKING_OBJECTS][UFO_REGS];
//
//// -------------------- Функции распаковки --------------------
//static inline uint16_t get_u16(const uint16_t* regs, uint16_t& idx) {
//    return regs[idx++];
//}
//static inline uint32_t get_u32(const uint16_t* regs, uint16_t& idx) {
//    uint32_t hi = regs[idx++];
//    uint32_t lo = regs[idx++];
//    return (hi << 16) | lo;
//}
//static inline uint64_t get_u64(const uint16_t* regs, uint16_t& idx) {
//    uint64_t w3 = regs[idx++];
//    uint64_t w2 = regs[idx++];
//    uint64_t w1 = regs[idx++];
//    uint64_t w0 = regs[idx++];
//    return (w3 << 48) | (w2 << 32) | (w1 << 16) | w0;
//}
//static inline float get_f32(const uint16_t* regs, uint16_t& idx) {
//    union { uint32_t u; float f; } u;
//    u.u = get_u32(regs, idx);
//    return u.f;
//}
//static inline int32_t get_i32(const uint16_t* regs, uint16_t& idx) {
//    return (int32_t)get_u32(regs, idx);
//}
//static inline int8_t get_i8_u8(const uint16_t* regs, uint16_t& idx) {
//    uint16_t w = regs[idx++];
//    return (int8_t)(w & 0xFF);
//}
//static inline uint8_t get_u8(const uint16_t* regs, uint16_t& idx) {
//    uint16_t w = regs[idx++];
//    return (uint8_t)(w & 0xFF);
//}
//static inline void get_bytes_from_regs(const uint16_t* regs, uint16_t& idx, uint8_t* out, size_t n) {
//    for (size_t i = 0; i < n; i += 2) {
//        uint16_t w = regs[idx++];
//        uint8_t b0 = (uint8_t)((w >> 8) & 0xFF);
//        uint8_t b1 = (uint8_t)(w & 0xFF);
//        out[i] = b0;
//        if (i + 1 < n) out[i + 1] = b1;
//    }
//}
//
//static void unpack_ufo_from_regs(ufo_t& u, const uint16_t* regs) {
//    uint16_t i = 0;
//    u.timestamp = (time_t)get_u64(regs, i);
//    u.addr = get_u32(regs, i);
//    u.addr_type = get_u8(regs, i);
//    u.latitude = get_f32(regs, i);
//    u.longitude = get_f32(regs, i);
//    u.old_latitude = get_f32(regs, i);
//    u.old_longitude = get_f32(regs, i);
//    u.altitude = get_f32(regs, i);
//    u.pressure_altitude = get_f32(regs, i);
//    u.course = get_f32(regs, i);
//    u.speed = get_f32(regs, i);
//    u.aircraft_type = get_u8(regs, i);
//    get_bytes_from_regs(regs, i, (uint8_t*)u.flight, 16);
//    u.vert_rate = get_i32(regs, i);
//    u.Squawk = get_i32(regs, i);
//    u.timemsg = (time_t)get_u64(regs, i);
//    u.vs = get_f32(regs, i);
//    u.geoid_separation = get_f32(regs, i);
//    u.hdop = get_u16(regs, i);
//    u.rssi = get_i8_u8(regs, i);
//    u.distance = get_f32(regs, i);
//    u.bearing = get_f32(regs, i);
//    u.signal_source = get_u8(regs, i);
//    u.seen = (time_t)get_u64(regs, i);
//    u.hour_msg = get_u8(regs, i);
//    u.min_msg = get_u8(regs, i);
//    u.delay_time_msg = get_u16(regs, i);
//    get_bytes_from_regs(regs, i, (uint8_t*)u.callsign, 8);
//}
//
//static void unpack_ctrl_from_regs() {
//    uint16_t i = 0;
//    analog_code_R = regs_ctrl[i++];
//
//    uint16_t flags = regs_ctrl[i++];
//    new_flag_R = flags & (1 << 0);
//    setMessageRead_R = flags & (1 << 1);
//    MessageRead_R = flags & (1 << 2);
//    SOS_Sprite_on_off_R = flags & (1 << 3);
//    SOS_View_on_off_R = flags & (1 << 4);
//    new_SOS_flag_R = flags & (1 << 5);
//    confirm_message_R = flags & (1 << 6);
//    isValidGNSS_R = flags & (1 << 7);
//
//    new_buttton_R = (uint8_t)(regs_ctrl[i++] & 0xFF);
//    FLYRF_MODE_TEST_R = (uint8_t)(regs_ctrl[i++] & 0xFF);
//
//    uint16_t msg_len = regs_ctrl[i++];
//    if (msg_len > 60) msg_len = 60;
//    for (int k = 0; k < 30; ++k) {
//        uint16_t w = regs_ctrl[i++];
//        uint8_t b0 = (uint8_t)((w >> 8) & 0xFF);
//        uint8_t b1 = (uint8_t)(w & 0xFF);
//        int idx = 2 * k;
//        if (idx < 60) msg_resp_R[idx] = (idx < msg_len) ? b0 : 0;
//        if (idx + 1 < 60) msg_resp_R[idx + 1] = (idx + 1 < msg_len) ? b1 : 0;
//    }
//    msg_resp_R[msg_len] = '\0';
//}
//
//// -------------------- FreeRTOS задачи --------------------
//static void mbTask(void* arg) {
//    for (;;) {
//        mb.task();
//        vTaskDelay(pdMS_TO_TICKS(2));
//    }
//}
//
//static void decodeTask(void* arg) {
//    const TickType_t period = pdMS_TO_TICKS(20);
//    for (;;) {
//        // Считываем из внутренних HREG-ов библиотеки в локальные буферы
//        for (uint16_t k = 0; k < HREG_CTRL_COUNT; ++k) regs_ctrl[k] = mb.Hreg(HREG_BASE_CTRL + k);
//        for (uint16_t k = 0; k < UFO_REGS; ++k) regs_this[k] = mb.Hreg(HREG_BASE_THIS + k);
//        for (int i = 0; i < MAX_TRACKING_OBJECTS; ++i) {
//            uint16_t base = HREG_BASE_CONT + i * UFO_REGS;
//            for (uint16_t k = 0; k < UFO_REGS; ++k) regs_cont[i][k] = mb.Hreg(base + k);
//        }
//
//        // Обновляем структуры
//        unpack_ctrl_from_regs();
//        unpack_ufo_from_regs(ThisAircraft, regs_this);
//        for (int i = 0; i < MAX_TRACKING_OBJECTS; ++i) {
//            unpack_ufo_from_regs(Container[i], regs_cont[i]);
//        }
//
//        vTaskDelay(period);
//    }
//}
//
//void setup() {
//    // UART
//    Serial1.begin(UART_BAUD, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
//
//    // Modbus RTU Slave
//    mb.begin(&Serial1, RS485_DE_PIN); // библиотека сама управляет DE/RE
//    mb.slave(SLAVE_ID);
//
//    // Выделяем регистры
//    mb.addHreg(HREG_BASE_CTRL, 0, HREG_CTRL_COUNT);
//    mb.addHreg(HREG_BASE_THIS, 0, UFO_REGS);
//    for (int i = 0; i < MAX_TRACKING_OBJECTS; ++i) {
//        mb.addHreg(HREG_BASE_CONT + i * UFO_REGS, 0, UFO_REGS);
//    }
//
//    // Задачи на ядре 0
//    xTaskCreatePinnedToCore(mbTask, "mbTask", 4096, nullptr, 3, nullptr, 0);
//    xTaskCreatePinnedToCore(decodeTask, "decodeTask", 8192, nullptr, 2, nullptr, 0);
//}
//
//void loop() {
//    // пусто
//}
// 
// 
//============================================================================= 
// 
// 
// 
//Примечания по интеграции и надежности
//
//Структура time_t : код выше пакует как 64 бита.Если в вашей сборке time_t 32 - битный, можно заменить put_u64 / get_u64 на put_u32 / get_u32 и скорректировать UFO_REGS(но текущее решение будет корректно для 64 - битного времени).
//Endianness : выбран “старшее слово первым” для 32 / 64 - битных полей и порядок байтов внутри регистра : [hi, lo] .Оба конца используют одинаковую схему.
//Передачу “только изменений” обеспечивает CRC32 по упакованному буферу блока на источнике.
//Скорость : по умолчанию 460800 бод.При необходимости увеличьте UART_BAUD до 921600 на обоих узлах.
//Ограничение длины записи : библиотека ModbusMaster по умолчанию поддерживает запись до 64 регистров за один вызов writeMultipleRegisters, поэтому блок UFO_REGS = 64 подобран под этот предел.
//Потоки / ядра : все задачи запущены на ядре 0. Основной loop() пустой.
//RS485 : обязательно общий GND и корректные терминаторы / биас на линии для устойчивости на высокой скорости.
//Если потребуется, могу адаптировать карту регистров, порядок / разрядность полей или довести упаковку под 32 - битный time_t.