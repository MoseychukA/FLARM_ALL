
// ===== Приемник: ESP32S3, RS-485 Modbus RTU Slave (ModbusRTU) =====

#include <Arduino.h>
#include <ModbusRTU.h>
#include <string.h>
#include <math.h>

static const int RS485_TX_PIN = 39;   // Serial1 TX
static const int RS485_RX_PIN = 38;   // Serial1 RX
static const int RS485_DE_RE  = 40;   // Управление прием/передача
static const uint32_t RS485_BAUD = 921600;
static const uint8_t SLAVE_ID = 1;

struct ExtraInfoM;
typedef struct ExtraInfoM extra_m_t;


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

// Экспортируемые массивы/структуры
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

// ----------- Карта регистров -----------
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

static const uint16_t REG_ANALOG_CODE          = BASE_EXTRA + 0x0000;
static const uint16_t REG_new_flag_M           = BASE_EXTRA + 0x0001;
static const uint16_t REG_new_buttton_M        = BASE_EXTRA + 0x0002;
static const uint16_t REG_setMessageRead_M     = BASE_EXTRA + 0x0003;
static const uint16_t REG_MessageRead_M        = BASE_EXTRA + 0x0004;
static const uint16_t REG_SOS_Sprite_on_off_M  = BASE_EXTRA + 0x0005;
static const uint16_t REG_SOS_View_on_off_M    = BASE_EXTRA + 0x0006;
static const uint16_t REG_new_SOS_flag_M       = BASE_EXTRA + 0x0007;
static const uint16_t REG_confirm_message_M    = BASE_EXTRA + 0x0008;
static const uint16_t REG_isValidGNSS_M        = BASE_EXTRA + 0x0009;
static const uint16_t REG_FLYRF_MODE_TEST_M    = BASE_EXTRA + 0x000A;
static const uint16_t REG_msg_resp_M_base      = BASE_EXTRA + 0x0010; // 30 regs

static const uint16_t CONTAINER_REG_COUNT = MAX_TRACKING_OBJECTS * OBJ_STRIDE;
static const uint16_t THIS_REG_COUNT = 64;
static const uint16_t EXTRA_REG_COUNT = 64;

ModbusRTU mb;

// ---------- Утилиты распаковки ----------
static inline uint32_t joinU32(uint16_t lo, uint16_t hi) { return ((uint32_t)hi << 16) | lo; }
static inline float u32ToF(uint32_t u) { union { uint32_t u; float f; } v; v.u = u; return v.f; }
static inline void regsToBytes(uint16_t regVal, uint8_t& b0, uint8_t& b1) {
  b0 = (uint8_t)(regVal & 0xFF);
  b1 = (uint8_t)((regVal >> 8) & 0xFF);
}

// ======== ЛОГГЕР И ФЛАГИ ИЗМЕНЕНИЙ (исправленный) ========

// Пороги для сравнения float
#ifndef EPS_LATLON
#define EPS_LATLON 1e-6f
#endif
#ifndef EPS_FLOAT
#define EPS_FLOAT  1e-3f
#endif

// ---- Флаги "грязных" областей ----
static volatile bool dirtyContainer[MAX_TRACKING_OBJECTS] = { false };
static volatile bool dirtyThis = false;
static volatile bool dirtyExtra = false;

// ---- Снапшоты предыдущих значений ----
static ufo_t prevContainer[MAX_TRACKING_OBJECTS] = {};
static bool  prevInitContainer[MAX_TRACKING_OBJECTS] = { false };
static ufo_t prevThisAircraft = {};
static bool  prevInitThisAircraft = false;

// Доп.информация источника на приёмнике
struct ExtraInfoM {
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
    uint16_t analog_code_M;
};

static extra_m_t prevExtra = {};
static bool      prevInitExtra = false;

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
    Serial.printf("[%s] UFO idx=%d \r\n", tag, idx);
    Serial.printf("  timestamp: %llu \r\n", (unsigned long long)o.timestamp);
    Serial.printf("  addr: %u, addr_type: %u \r\n", (unsigned)o.addr, (unsigned)o.addr_type);
    Serial.printf("  lat/lon: %.6f / %.6f \r\n", o.latitude, o.longitude);
    Serial.printf("  old_lat/lon: %.6f / %.6f \r\n", o.old_latitude, o.old_longitude);
    Serial.printf("  alt: %.1f, p_alt: %.1f, course: %.1f, speed: %.1f\r\n ",  o.altitude, o.pressure_altitude, o.course, o.speed);
    Serial.printf("  aircraft_type: %u\r\n ", (unsigned)o.aircraft_type);
    Serial.print("  flight: "); Serial.write(o.flight, sizeof(o.flight)); Serial.println();
    Serial.printf("  vert_rate: %d, Squawk: %d \r\n", o.vert_rate, o.Squawk);
    Serial.printf("  timemsg: %llu\r\n", (unsigned long long)o.timemsg);
    Serial.printf("  vs: %.1f, geoid_sep: %.2f, hdop: %u, rssi: %d \r\n",    o.vs, o.geoid_separation, (unsigned)o.hdop, (int)o.rssi);
    Serial.printf("  dist: %.2f, bearing: %.1f, signal_source: %u \r\n",    o.distance, o.bearing, (unsigned)o.signal_source);
    Serial.printf("  seen: %llu, hour_msg: %u, min_msg: %u, delay_msg: %u \r\n",    (unsigned long long)o.seen, (unsigned)o.hour_msg, (unsigned)o.min_msg, (unsigned)o.delay_time_msg);
    Serial.print("  callsign: \r\n"); Serial.write((char*)o.callsign, 8); Serial.println();
}

static void printUFOChanged(const ufo_t& a, const ufo_t& b, int idx) 
{
    Serial.printf("[CHG] UFO idx=%d ", idx);
    if (diffI64(a.timestamp, b.timestamp)) Serial.printf("  timestamp: %llu -> %llu \r\n",(unsigned long long)a.timestamp,(unsigned long long)b.timestamp);
    if (diffU32(a.addr, b.addr)) Serial.printf("  addr: %u -> %u \r\n",(unsigned)a.addr,(unsigned)b.addr);
    if (diffU8(a.addr_type, b.addr_type)) Serial.printf("  addr_type: %u -> %u \r\n",(unsigned)a.addr_type,(unsigned)b.addr_type);
    if (diffF(a.latitude, b.latitude, EPS_LATLON) || diffF(a.longitude, b.longitude, EPS_LATLON))
    Serial.printf("  lat/lon: %.6f/%.6f -> %.6f/%.6f \r\n", a.latitude,a.longitude,b.latitude,b.longitude);
    if (diffF(a.old_latitude, b.old_latitude, EPS_LATLON) || diffF(a.old_longitude, b.old_longitude, EPS_LATLON))
    Serial.printf("  old_lat/lon: %.6f/%.6f -> %.6f/%.6f \r\n", a.old_latitude,a.old_longitude,b.old_latitude,b.old_longitude);
    if (diffF(a.altitude, b.altitude, EPS_FLOAT)) Serial.printf("  altitude: %.1f -> %.1f \r\n", a.altitude, b.altitude);
    if (diffF(a.pressure_altitude, b.pressure_altitude, EPS_FLOAT)) Serial.printf("  pressure_altitude: %.1f -> %.1f\r\n", a.pressure_altitude, b.pressure_altitude);
    if (diffF(a.course, b.course, EPS_FLOAT)) Serial.printf("  course: %.1f -> %.1f \r\n", a.course, b.course);
    if (diffF(a.speed, b.speed, EPS_FLOAT)) Serial.printf("  speed: %.1f -> %.1f \r\n", a.speed, b.speed);
    if (diffU8(a.aircraft_type, b.aircraft_type)) Serial.printf("  aircraft_type: %u -> %u \r\n",(unsigned)a.aircraft_type,(unsigned)b.aircraft_type);
    if (diffBytes(a.flight, b.flight, sizeof(a.flight))) {
    Serial.print("  flight: \r\n"); Serial.write(a.flight, sizeof(a.flight)); Serial.print(" -> ");
    Serial.write(b.flight, sizeof(b.flight)); Serial.println();
    }
    if (diffI(a.vert_rate, b.vert_rate)) Serial.printf("  vert_rate: %d -> %d \r\n", a.vert_rate, b.vert_rate);
    if (diffI(a.Squawk, b.Squawk)) Serial.printf("  Squawk: %d -> %d \r\n", a.Squawk, b.Squawk);
    if (diffI64(a.timemsg, b.timemsg)) Serial.printf("  timemsg: %llu -> %llu \r\n",(unsigned long long)a.timemsg,(unsigned long long)b.timemsg);
    if (diffF(a.vs, b.vs, EPS_FLOAT)) Serial.printf("  vs: %.1f -> %.1f \r\n", a.vs, b.vs);
    if (diffF(a.geoid_separation, b.geoid_separation, EPS_FLOAT)) Serial.printf("  geoid_separation: %.2f -> %.2f \r\n", a.geoid_separation, b.geoid_separation);
    if (diffU16(a.hdop, b.hdop)) Serial.printf("  hdop: %u -> %u \r\n",(unsigned)a.hdop,(unsigned)b.hdop);
    if (diffI8(a.rssi, b.rssi)) Serial.printf("  rssi: %d -> %d \r\n",(int)a.rssi,(int)b.rssi);
    if (diffF(a.distance, b.distance, EPS_FLOAT)) Serial.printf("  distance: %.2f -> %.2f \r\n", a.distance, b.distance);
    if (diffF(a.bearing, b.bearing, EPS_FLOAT)) Serial.printf("  bearing: %.1f -> %.1f \r\n", a.bearing, b.bearing);
    if (diffU8(a.signal_source, b.signal_source)) Serial.printf("  signal_source: %u -> %u \r\n",(unsigned)a.signal_source,(unsigned)b.signal_source);
    if (diffI64(a.seen, b.seen)) Serial.printf("  seen: %llu -> %llu \r\n",(unsigned long long)a.seen,(unsigned long long)b.seen);
    if (diffU8(a.hour_msg, b.hour_msg)) Serial.printf("  hour_msg: %u -> %u \r\n",(unsigned)a.hour_msg,(unsigned)b.hour_msg);
    if (diffU8(a.min_msg, b.min_msg)) Serial.printf("  min_msg: %u -> %u \r\n",(unsigned)a.min_msg,(unsigned)b.min_msg);
    if (diffU16(a.delay_time_msg, b.delay_time_msg)) Serial.printf("  delay_time_msg: %u -> %u \r\n",(unsigned)a.delay_time_msg,(unsigned)b.delay_time_msg);
    if (diffBytes(a.callsign, b.callsign, sizeof(a.callsign))) {
    Serial.print("  callsign: "); Serial.write((char*)a.callsign, 8); Serial.print(" -> ");
    Serial.write((char*)b.callsign, 8); Serial.println();
    }
}

static void printThisAircraftFull(const ufo_t& o) { printUFOFull(o, -1, "THIS"); }
static void printThisAircraftChanged(const ufo_t& a, const ufo_t& b) {
    Serial.println("[CHG] THIS"); printUFOChanged(a, b, -1);
}

// Extra helpers
static void printExtraFull(const extra_m_t& e) 
{
    Serial.println("[NEW] EXTRA");
    Serial.printf("  new_flag_M: %d  \r\n", e.new_flag_M);
    Serial.printf("  new_buttton_M: %u  \r\n", (unsigned)e.new_buttton_M);
    Serial.printf("  setMessageRead_M: %d, MessageRead_M: %d \r\n ", e.setMessageRead_M, e.MessageRead_M);
    Serial.printf("  SOS_Sprite_on_off_M: %d, SOS_View_on_off_M: %d  \r\n", e.SOS_Sprite_on_off_M, e.SOS_View_on_off_M);
    Serial.printf("  new_SOS_flag_M: %d, confirm_message_M: %d  \r\n", e.new_SOS_flag_M, e.confirm_message_M);
    Serial.printf("  msg_resp_M: %.*s \r\n ", (int)sizeof(e.msg_resp_M), e.msg_resp_M);
    Serial.printf("  isValidGNSS_M: %d, FLYRF_MODE_TEST_M: %u  \r\n", e.isValidGNSS_M, (unsigned)e.FLYRF_MODE_TEST_M);
    Serial.printf("  analog_code_M: %u  \r\n", (unsigned)e.analog_code_M);
}
static void printExtraChanged(const extra_m_t& a, const extra_m_t& b) 
{
Serial.println("[CHG] EXTRA");
if (diffBool(a.new_flag_M, b.new_flag_M)) Serial.printf("  new_flag_M: %d -> %d\r\n", a.new_flag_M, b.new_flag_M);
if (diffU8(a.new_buttton_M, b.new_buttton_M)) Serial.printf("  new_buttton_M: %u -> %u\r\n",(unsigned)a.new_buttton_M,(unsigned)b.new_buttton_M);
if (diffBool(a.setMessageRead_M, b.setMessageRead_M)) Serial.printf("  setMessageRead_M: %d -> %d\r\n", a.setMessageRead_M, b.setMessageRead_M);
if (diffBool(a.MessageRead_M, b.MessageRead_M)) Serial.printf("  MessageRead_M: %d -> %d\r\n", a.MessageRead_M, b.MessageRead_M);
if (diffBool(a.SOS_Sprite_on_off_M, b.SOS_Sprite_on_off_M)) Serial.printf("  SOS_Sprite_on_off_M: %d -> %d\r\n", a.SOS_Sprite_on_off_M, b.SOS_Sprite_on_off_M);
if (diffBool(a.SOS_View_on_off_M, b.SOS_View_on_off_M)) Serial.printf("  SOS_View_on_off_M: %d -> %d\r\n", a.SOS_View_on_off_M, b.SOS_View_on_off_M);
if (diffBool(a.new_SOS_flag_M, b.new_SOS_flag_M)) Serial.printf("  new_SOS_flag_M: %d -> %d\r\n", a.new_SOS_flag_M, b.new_SOS_flag_M);
if (diffBool(a.confirm_message_M, b.confirm_message_M)) Serial.printf("  confirm_message_M: %d -> %d\r\n", a.confirm_message_M, b.confirm_message_M);
if (diffBytes(a.msg_resp_M, b.msg_resp_M, sizeof(a.msg_resp_M)))
Serial.printf("  msg_resp_M: '%.*s' -> '%.*s'\r\n", (int)sizeof(a.msg_resp_M), a.msg_resp_M, (int)sizeof(b.msg_resp_M), b.msg_resp_M);
if (diffBool(a.isValidGNSS_M, b.isValidGNSS_M)) Serial.printf("  isValidGNSS_M: %d -> %d\r\n", a.isValidGNSS_M, b.isValidGNSS_M);
if (diffU8(a.FLYRF_MODE_TEST_M, b.FLYRF_MODE_TEST_M)) Serial.printf("  FLYRF_MODE_TEST_M: %u -> %u\r\n",(unsigned)a.FLYRF_MODE_TEST_M,(unsigned)b.FLYRF_MODE_TEST_M);
if (diffU16(a.analog_code_M, b.analog_code_M)) Serial.printf("  analog_code_M: %u -> %u\r\n",(unsigned)a.analog_code_M,(unsigned)b.analog_code_M);
}

// Сборка extra из ваших _R переменных
static extra_m_t makeExtraFromR() {
    extra_m_t e{};
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

// Публичные функции логирования
static void logContainerIfNewOrChanged(int i) {
    const ufo_t& cur = Container[i];
    if (!prevInitContainer[i]) {
        printUFOFull(cur, i, "NEW");
        prevContainer[i] = cur;
        prevInitContainer[i] = true;
        return;
    }
    bool isNew = diffI64(prevContainer[i].timestamp, cur.timestamp) || diffU32(prevContainer[i].addr, cur.addr);
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
    bool isNew = diffI64(prevThisAircraft.timestamp, cur.timestamp) || diffU32(prevThisAircraft.addr, cur.addr);
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
//=========================================================================================

// ---------- Хэндлер записи регистров ----------
uint16_t cbSetHreg(TRegister* reg, uint16_t val) 
{
  reg->value = val;
  if (!reg->address.isHreg()) return Modbus::EX_SUCCESS;

  //if (reg->address.isHreg()) {
  //    Serial.printf("[RX] HREG write addr=0x%04X val=0x%04X\r\n ", reg->address.address, val);
  //}
  // // пометки dirty как у вас
  //    return Modbus::EX_SUCCESS;

  uint16_t a = reg->address.address; // адрес HREG
  if (a >= BASE_CONTAINER && a < BASE_CONTAINER + CONTAINER_REG_COUNT) 
  {
    int idx = (a - BASE_CONTAINER) / OBJ_STRIDE;
    if (idx >= 0 && idx < MAX_TRACKING_OBJECTS) dirtyContainer[idx] = true;
  }
  else if (a >= BASE_THIS && a < BASE_THIS + THIS_REG_COUNT) 
  {
    dirtyThis = true;
  } 
  else if (a >= BASE_EXTRA && a < BASE_EXTRA + EXTRA_REG_COUNT) 
  {
    dirtyExtra = true;
  }
  return Modbus::EX_SUCCESS;
}

// ---------- Копирование из HREG в структуры ----------
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
  u.min_msg  = (uint8_t)H(baseAddr + OFF_min);
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

// ---------- Задача обновления структур ----------
void updateTask(void* arg) {
  const TickType_t period = pdMS_TO_TICKS(50);
  for (;;) {
    mb.task();

    for (int i = 0; i < MAX_TRACKING_OBJECTS; ++i) {
      uint16_t baseAddr = BASE_CONTAINER + i * OBJ_STRIDE;
      pullUFOFromRegs(baseAddr, Container[i]);
      if (dirtyContainer[i]) {
        logContainerIfNewOrChanged(i);
        dirtyContainer[i] = false;
      }
    }

    pullUFOFromRegs(BASE_THIS, ThisAircraft);
    if (dirtyThis) {
      logThisAircraftIfNewOrChanged();
      dirtyThis = false;
    }

    pullExtraFromRegs();
    if (dirtyExtra) {
      auto cur = makeExtraFromR();
      logExtraIfNewOrChanged(cur);
      dirtyExtra = false;
    }

    vTaskDelay(period);
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("Start setup");
  Serial1.begin(RS485_BAUD, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
  mb.begin(&Serial1, RS485_DE_RE); // библиотека сама управляет RE/DE пином
 // mb.begin(&Serial1, RS485_DE_RE, true); // прямой уровень: HIGH = передача
  mb.slave(SLAVE_ID);

  mb.addHreg(BASE_CONTAINER, 0, CONTAINER_REG_COUNT);
  mb.addHreg(BASE_THIS, 0, THIS_REG_COUNT);
  mb.addHreg(BASE_EXTRA, 0, EXTRA_REG_COUNT);

  mb.onSetHreg(BASE_CONTAINER, cbSetHreg, CONTAINER_REG_COUNT);
  mb.onSetHreg(BASE_THIS, cbSetHreg, THIS_REG_COUNT);
  mb.onSetHreg(BASE_EXTRA, cbSetHreg, EXTRA_REG_COUNT);

  xTaskCreatePinnedToCore(updateTask, "updateTask", 8192, nullptr, 2, nullptr, 0);
  Serial.println("End setup");
}

void loop() {
  // Основная работа в updateTask()
}
