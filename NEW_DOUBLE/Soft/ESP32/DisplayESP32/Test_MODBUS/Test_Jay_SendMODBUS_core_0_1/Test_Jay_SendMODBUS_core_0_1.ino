
// ===== Источник: ESP32S3, RS485 Modbus RTU Master (ModbusMaster) =====
#include <Arduino.h>
#include <ModbusMaster.h>

static const int RS485_TX_PIN = 18;   // Serial2 TX
static const int RS485_RX_PIN = 17;   // Serial2 RX
static const int RS485_DE_RE  = 21;   // RS485 DE/RE
static const uint32_t RS485_BAUD = 921600;
static const uint8_t SLAVE_ID = 1;
static const uint16_t MAX_WRITE_BLOCK = 32; // безопасный размер для ModbusRTU

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

// Внешние данные для отправки
ufo_t Container[MAX_TRACKING_OBJECTS] = {};
ufo_t ThisAircraft = {};


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



// -------- Карта регистров ----------
static const uint16_t BASE_CONTAINER = 0x0000;
static const uint16_t OBJ_STRIDE     = 64;
static const uint16_t BASE_THIS      = 0x1000;
static const uint16_t BASE_EXTRA     = 0x2000;

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
  OFF_flight = 22,       // 8 regs
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
  OFF_callsign = 52      // 4 regs
};

// EXTRA регистры
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

// ---------- Утилиты упаковки ----------
static inline void splitU32(uint32_t u, uint16_t& lo, uint16_t& hi) { lo = (uint16_t)(u & 0xFFFF); hi = (uint16_t)(u >> 16); }
static inline uint32_t fToU32(float f) { union { float f; uint32_t u; } v; v.f = f; return v.u; }
static inline uint16_t mkWord(uint8_t low, uint8_t high) { return (uint16_t)low | ((uint16_t)high << 8); }
static inline uint16_t i8ToWord(int8_t v) { return (uint16_t)((uint8_t)v); }
//====================================================================================
// ===== TEST GENERATOR (SOURCE) =====
#define TEST_GENERATOR 1
#if TEST_GENERATOR

static inline void setFlight(ufo_t& u, const char* s) {
    memset(u.flight, ' ', sizeof(u.flight));
    size_t n = strnlen(s, sizeof(u.flight));
    memcpy(u.flight, s, n);
}
static inline void setCallsign(ufo_t& u, const char* s8) {
    uint8_t buf[8] = { 0 };
    size_t n = strnlen(s8, 8);
    memcpy(buf, s8, n);
    memcpy(u.callsign, buf, 8);
}
static void seedUFO(ufo_t& u, double lat, double lon, uint32_t addr, const char* cs, const char* fl) {
    memset(&u, 0, sizeof(u));
    u.timestamp = (uint32_t)(millis() / 1000);
    u.addr = addr;
    u.addr_type = 1;
    u.latitude = (float)lat;
    u.longitude = (float)lon;
    u.old_latitude = u.latitude;
    u.old_longitude = u.longitude;
    u.altitude = 1000.0f;
    u.pressure_altitude = 990.0f;
    u.course = 90.0f;
    u.speed = 120.0f;
    u.aircraft_type = 1;
    setFlight(u, fl);
    u.vert_rate = 0;
    u.Squawk = 1200;
    u.timemsg = u.timestamp;
    u.vs = 0.0f;
    u.geoid_separation = 30.0f;
    u.hdop = 120;
    u.rssi = -60;
    u.distance = 10.0f;
    u.bearing = 180.0f;
    u.signal_source = 1;
    u.seen = u.timestamp;
    u.hour_msg = 12;
    u.min_msg = 34;
    u.delay_time_msg = 0;
    setCallsign(u, cs);
}

void testGenInit() {
    randomSeed(esp_timer_get_time());
    seedUFO(ThisAircraft, 55.7500, 37.6167, 0xABCDEF00, "THISACF", "TST000000000000");
    seedUFO(Container[0], 55.7510, 37.6177, 0x01020304, "UFO0001", "FLT000000000000");
    seedUFO(Container[1], 55.7520, 37.6187, 0x05060708, "UFO0002", "FLT000100000000");
    seedUFO(Container[2], 55.7530, 37.6197, 0x0A0B0C0D, "UFO0003", "FLT000200000000");

    analog_code_M = 1000;
    new_flag_M = true;
    new_buttton_M = 1;
    setMessageRead_M = false;
    MessageRead_M = false;
    SOS_Sprite_on_off_M = false;
    SOS_View_on_off_M = false;
    new_SOS_flag_M = false;
    confirm_message_M = false;
    snprintf(msg_resp_M, sizeof(msg_resp_M), "INIT OK");
    isValidGNSS_M = true;
    FLYRF_MODE_TEST_M = 1;
}

void testGenStep(uint32_t step) {
    // ThisAircraft: плавное движение
    float t = step * 0.2f; // сек
    ThisAircraft.timestamp = (uint32_t)(millis() / 1000);
    ThisAircraft.old_latitude = ThisAircraft.latitude;
    ThisAircraft.old_longitude = ThisAircraft.longitude;
    ThisAircraft.latitude = 55.7500f + 0.001f * sinf(t * 0.1f);
    ThisAircraft.longitude = 37.6167f + 0.001f * cosf(t * 0.1f);
    ThisAircraft.altitude = 1000.0f + 50.0f * sinf(t * 0.2f);
    ThisAircraft.course = fmodf(90.0f + t * 5.0f, 360.0f);
    ThisAircraft.speed = 120.0f + 2.0f * sinf(t * 0.15f);
    ThisAircraft.vert_rate = (int)(100.0f * sinf(t * 0.2f));
    ThisAircraft.timemsg = ThisAircraft.timestamp;
    ThisAircraft.vs = (float)ThisAircraft.vert_rate;
    ThisAircraft.hdop = 100 + (uint16_t)(10.0f * (sinf(t * 0.3f) + 1.0f));
    ThisAircraft.rssi = -70 + (int8_t)(10.0f * sinf(t * 0.25f));
    ThisAircraft.distance = 10.0f + 0.1f * step;
    ThisAircraft.bearing = fmodf(180.0f + t, 360.0f);
    ThisAircraft.signal_source = 1;
    ThisAircraft.seen = ThisAircraft.timestamp;
    ThisAircraft.hour_msg = (uint8_t)((12 + (step / 20)) % 24);
    ThisAircraft.min_msg = (uint8_t)((34 + (step / 2)) % 60);
    ThisAircraft.delay_time_msg = (uint16_t)(step % 500);

    char flbuf[17]; snprintf(flbuf, sizeof(flbuf), "TST%012lu", (unsigned long)step);
    setFlight(ThisAircraft, flbuf);

    // Три объекта Container: небольшие сдвиги
    for (int i = 0; i < 3; i++) {
        ufo_t& U = Container[i];
        U.timestamp = ThisAircraft.timestamp;
        U.old_latitude = U.latitude;
        U.old_longitude = U.longitude;
        U.latitude += 0.0001f * sinf(t * 0.05f + i);
        U.longitude += 0.0001f * cosf(t * 0.05f + i);
        U.altitude += 5.0f * sinf(t * 0.1f + i);
        U.course = fmodf(U.course + 0.5f + i * 0.1f, 360.0f);
        U.speed = 110.0f + 1.0f * sinf(t * 0.1f + i);
        U.vert_rate = (int)(50.0f * sinf(t * 0.1f + i));
        U.timemsg = U.timestamp;
        U.vs = (float)U.vert_rate;
        U.distance += 0.05f;
        U.bearing = fmodf(U.bearing + 0.3f + i * 0.1f, 360.0f);
        U.hdop = 120 + (step + i) % 5;
        U.rssi = -65 + (int8_t)((step + i) % 6);

        char fl[17]; snprintf(fl, sizeof(fl), "FLT%012lu", (unsigned long)(step + i));
        setFlight(U, fl);
    }

    // EXTRA
    analog_code_M = (analog_code_M + 7) % 4096;
    new_flag_M = (step % 10) < 5;
    new_buttton_M = (uint8_t)((step / 5) % 4);
    setMessageRead_M = (step % 20) == 0;
    MessageRead_M = (step % 30) == 0;
    SOS_Sprite_on_off_M = (step % 40) < 20;
    SOS_View_on_off_M = (step % 60) < 30;
    new_SOS_flag_M = (step % 25) == 0;
    confirm_message_M = (step % 50) == 0;
    isValidGNSS_M = ((step / 15) % 2) == 0;
    FLYRF_MODE_TEST_M = (uint8_t)((step / 10) % 8);
    snprintf(msg_resp_M, sizeof(msg_resp_M), "MSG %lu OK", (unsigned long)step);
}

void testGenTask(void* arg) {
    testGenInit();
    uint32_t step = 0;
    const TickType_t period = pdMS_TO_TICKS(200);
    for (;;) {
        testGenStep(step++);
        vTaskDelay(period);
    }
}
#endif // TEST_GENERATOR

//===================================================================================
// Упаковать UFO в массив из 64 регистров (младшее слово сначала)
static void packUFOtoRegs(const ufo_t& u, uint16_t outRegs[OBJ_STRIDE]) {
  memset(outRegs, 0, OBJ_STRIDE * sizeof(uint16_t));
  uint16_t lo, hi;

  splitU32((uint32_t)u.timestamp, lo, hi); outRegs[OFF_timestamp] = lo; outRegs[OFF_timestamp+1] = hi;
  splitU32(u.addr, lo, hi);                outRegs[OFF_addr] = lo; outRegs[OFF_addr+1] = hi;
  outRegs[OFF_addr_type] = (uint16_t)u.addr_type;

  splitU32(fToU32(u.latitude), lo, hi);          outRegs[OFF_lat] = lo; outRegs[OFF_lat+1] = hi;
  splitU32(fToU32(u.longitude), lo, hi);         outRegs[OFF_lon] = lo; outRegs[OFF_lon+1] = hi;
  splitU32(fToU32(u.old_latitude), lo, hi);      outRegs[OFF_old_lat] = lo; outRegs[OFF_old_lat+1] = hi;
  splitU32(fToU32(u.old_longitude), lo, hi);     outRegs[OFF_old_lon] = lo; outRegs[OFF_old_lon+1] = hi;

  splitU32(fToU32(u.altitude), lo, hi);          outRegs[OFF_alt] = lo; outRegs[OFF_alt+1] = hi;
  splitU32(fToU32(u.pressure_altitude), lo, hi); outRegs[OFF_press_alt] = lo; outRegs[OFF_press_alt+1] = hi;
  splitU32(fToU32(u.course), lo, hi);            outRegs[OFF_course] = lo; outRegs[OFF_course+1] = hi;
  splitU32(fToU32(u.speed), lo, hi);             outRegs[OFF_speed] = lo; outRegs[OFF_speed+1] = hi;

  outRegs[OFF_aircraft_type] = (uint16_t)u.aircraft_type;

  for (int i=0;i<8;i++) {
    outRegs[OFF_flight + i] = mkWord((uint8_t)u.flight[2*i], (uint8_t)u.flight[2*i+1]);
  }

  splitU32((uint32_t)(int32_t)u.vert_rate, lo, hi); outRegs[OFF_vert_rate] = lo; outRegs[OFF_vert_rate+1] = hi;
  splitU32((uint32_t)(int32_t)u.Squawk, lo, hi);    outRegs[OFF_Squawk] = lo; outRegs[OFF_Squawk+1] = hi;
  splitU32((uint32_t)u.timemsg, lo, hi);            outRegs[OFF_timemsg] = lo; outRegs[OFF_timemsg+1] = hi;

  splitU32(fToU32(u.vs), lo, hi);                   outRegs[OFF_vs] = lo; outRegs[OFF_vs+1] = hi;
  splitU32(fToU32(u.geoid_separation), lo, hi);     outRegs[OFF_geoid] = lo; outRegs[OFF_geoid+1] = hi;

  outRegs[OFF_hdop] = u.hdop;
  outRegs[OFF_rssi] = i8ToWord(u.rssi);

  splitU32(fToU32(u.distance), lo, hi);             outRegs[OFF_distance] = lo; outRegs[OFF_distance+1] = hi;
  splitU32(fToU32(u.bearing), lo, hi);              outRegs[OFF_bearing]  = lo; outRegs[OFF_bearing+1]  = hi;

  outRegs[OFF_signal_src] = (uint16_t)u.signal_source;

  splitU32((uint32_t)u.seen, lo, hi);               outRegs[OFF_seen] = lo; outRegs[OFF_seen+1] = hi;

  outRegs[OFF_hour]  = (uint16_t)u.hour_msg;
  outRegs[OFF_min]   = (uint16_t)u.min_msg;
  outRegs[OFF_delay] = (uint16_t)u.delay_time_msg;

  for (int i=0;i<4;i++) {
    outRegs[OFF_callsign + i] = mkWord(u.callsign[2*i], u.callsign[2*i+1]);
  }
}

// Упаковать EXTRA регион (64 регистра)
static void packExtratoRegs(uint16_t outRegs[64]) {
  memset(outRegs, 0, 64 * sizeof(uint16_t));
  outRegs[0x0000] = analog_code_M;
  outRegs[0x0001] = (uint16_t)(new_flag_M ? 1 : 0);
  outRegs[0x0002] = (uint16_t)new_buttton_M;
  outRegs[0x0003] = (uint16_t)(setMessageRead_M ? 1 : 0);
  outRegs[0x0004] = (uint16_t)(MessageRead_M ? 1 : 0);
  outRegs[0x0005] = (uint16_t)(SOS_Sprite_on_off_M ? 1 : 0);
  outRegs[0x0006] = (uint16_t)(SOS_View_on_off_M ? 1 : 0);
  outRegs[0x0007] = (uint16_t)(new_SOS_flag_M ? 1 : 0);
  outRegs[0x0008] = (uint16_t)(confirm_message_M ? 1 : 0);
  outRegs[0x0009] = (uint16_t)(isValidGNSS_M ? 1 : 0);
  outRegs[0x000A] = (uint16_t)FLYRF_MODE_TEST_M;

  for (int i=0;i<30;i++) {
    uint8_t b0 = (uint8_t)msg_resp_M[2*i];
    uint8_t b1 = (uint8_t)msg_resp_M[2*i + 1];
    outRegs[0x0010 + i] = mkWord(b0, b1);
  }
}

// ----------- Modbus Master -----------
ModbusMaster node;

// RS485 управление
void preTransmission() {
  digitalWrite(RS485_DE_RE, HIGH);
}
void postTransmission() {
  // небольшая пауза на опустошение сдвигового регистра UART
  delayMicroseconds(20);
  digitalWrite(RS485_DE_RE, LOW);
}

// Буферы предыдущих состояний
static uint16_t prevObjRegs[MAX_TRACKING_OBJECTS][OBJ_STRIDE] = {0};
static bool     prevObjInit[MAX_TRACKING_OBJECTS] = {false};

static uint16_t prevThisRegs[OBJ_STRIDE] = {0};
static bool     prevThisInit = false;

static uint16_t prevExtraRegs[64] = {0};
static bool     prevExtraInit = false;

uint8_t writeBlockChunked(uint16_t startAddr, const uint16_t* data, uint16_t count) 
{
    uint16_t offset = 0;
    while (offset < count) {
        uint16_t n = (count - offset > MAX_WRITE_BLOCK) ? MAX_WRITE_BLOCK : (count - offset);
        for (uint16_t i = 0; i < n; i++) node.setTransmitBuffer(i, data[offset + i]);
        uint8_t rc = node.writeMultipleRegisters(startAddr + offset, n);
        Serial.printf("[MB] write 0x%04X len=%u rc=%u \r\n", startAddr + offset, n, rc);
            vTaskDelay(1);
        if (rc != 0) return rc; // прерываем при ошибке
        offset += n;
    }
    return 0;
}






// Вычислить и отправить только изменённые диапазоны
void sendChangedRanges(uint16_t baseAddr, uint16_t cur[], uint16_t prev[], uint16_t total) {
  uint16_t i = 0;
  while (i < total) {
    // пропустить совпадающие
    while (i < total && cur[i] == prev[i]) i++;
    if (i >= total) break;
    // найти непрерывный изменённый отрезок
    uint16_t start = i;
    while (i < total && cur[i] != prev[i] && (i - start) < MAX_WRITE_BLOCK) i++; // не больше 32 за раз
    uint16_t len = i - start;
    uint8_t rc = writeBlockChunked(baseAddr + start, &cur[start], len);
    if (rc != 0) {
      // при ошибке можно ретраить/логировать
      // Serial.printf("MB write err %u at 0x%04X len=%u", rc, baseAddr + start, len);
    } else {
      // обновить prev на отправленном участке
      memcpy(&prev[start], &cur[start], len * sizeof(uint16_t));
    }
    // продолжить поиск
  }
}

// Отправить один UFO (Container[i])
void sendUFOObject(int idx) {
  if (idx < 0 || idx >= MAX_TRACKING_OBJECTS) return;
  ufo_t snap = Container[idx];   // локальная копия
  uint16_t regs[OBJ_STRIDE];
  packUFOtoRegs(snap, regs);

  uint16_t base = BASE_CONTAINER + idx * OBJ_STRIDE;

  if (!prevObjInit[idx]) {
      uint8_t rc = writeBlockChunked(base, regs, OBJ_STRIDE);
      if (rc == 0) { memcpy(prevObjRegs[idx], regs, sizeof(regs)); prevObjInit[idx] = true; }
      return;
  }

  sendChangedRanges(base, regs, prevObjRegs[idx], OBJ_STRIDE);
}

// Отправить ThisAircraft
void sendThisAircraft() {
  ufo_t snap = ThisAircraft;
  uint16_t regs[OBJ_STRIDE];
  packUFOtoRegs(snap, regs);

  if (!prevThisInit) {
      uint8_t rc = writeBlockChunked(BASE_THIS, regs, OBJ_STRIDE);
      if (rc == 0) { memcpy(prevThisRegs, regs, sizeof(regs)); prevThisInit = true; }
      return;
  }
  sendChangedRanges(BASE_THIS, regs, prevThisRegs, OBJ_STRIDE);
}

// Отправить EXTRA
void sendExtra() {
  uint16_t regs[64];
  packExtratoRegs(regs);

  if (!prevExtraInit) {
      uint8_t rc = writeBlockChunked(BASE_EXTRA, regs, 64);
      if (rc == 0) { memcpy(prevExtraRegs, regs, sizeof(regs)); prevExtraInit = true; }
      return;
  }

  sendChangedRanges(BASE_EXTRA, regs, prevExtraRegs, 64);
}

static bool slaveOnlineOnce = false;
static bool checkSlaveOnline() {
    // можно читать 1 регистр, например первый из EXTRA
    uint8_t rc = node.readHoldingRegisters(BASE_EXTRA, 1);
    Serial.printf("[MB] ping read 0x%04X rc=%u\r\n", BASE_EXTRA, rc);//!!
    return (rc == node.ku8MBSuccess);
}



// Задача передачи (ядро 0)
void txTask(void* arg) {
  const TickType_t period = pdMS_TO_TICKS(50); // цикл ~20 Гц
  for (;;) 
  {
     // Порядок: сначала ThisAircraft, затем объекты, затем EXTRA
    sendThisAircraft();
    for (int i=0;i<MAX_TRACKING_OBJECTS;i++) sendUFOObject(i);
    sendExtra();

    vTaskDelay(period);
  }
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(RS485_BAUD, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
  delay(500);
  Serial.println("Start setup");
  pinMode(RS485_DE_RE, OUTPUT);
  digitalWrite(RS485_DE_RE, LOW);

  node.begin(SLAVE_ID, Serial2);
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);
  node.setResponseTimeout(1000); // миллисекунды

#if TEST_GENERATOR
  testGenInit(); // сначала заполнить структуры
  delay(1000);
#endif
  xTaskCreatePinnedToCore(txTask, "txTask", 8192, nullptr, 2, nullptr, 0);
#if TEST_GENERATOR
  xTaskCreatePinnedToCore(testGenTask, "testGenTask", 8192, nullptr, 1, nullptr, 0);
#endif

  Serial.println("End setup");
}

void loop() 
{
  // Пусто, всё в txTask




}

