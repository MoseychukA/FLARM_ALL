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
static const uint32_t RS485_BAUD = 115200;//921600;

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
bool cbSetHreg(TRegister* reg, uint16_t val) {
    reg->value = val;
    return true;
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
void updateTask(void* arg) {
    const TickType_t period = pdMS_TO_TICKS(50);
    for (;;) {
        // Цикл Modbus
        mb.task();

        // Обновляем контейнеры
        for (int i = 0; i < MAX_TRACKING_OBJECTS; ++i) {
            uint16_t baseAddr = BASE_CONTAINER + i * OBJ_STRIDE;
            pullUFOFromRegs(baseAddr, Container[i]);
        }
        pullUFOFromRegs(BASE_THIS, ThisAircraft);
        pullExtraFromRegs();

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

float latitude_old;
uint16_t analog_code_tmp = 0;

void loop() 
{
 
    if (analog_code_R != analog_code_tmp)
    {
        analog_code_tmp = analog_code_R;
        Serial.println(analog_code_tmp);

    }



    if (ThisAircraft.latitude != latitude_old)
    {

        latitude_old = ThisAircraft.latitude;
        Serial.println(latitude_old);

    }


}
