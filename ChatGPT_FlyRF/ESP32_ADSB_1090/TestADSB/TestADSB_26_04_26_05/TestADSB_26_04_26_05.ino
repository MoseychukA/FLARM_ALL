/*
  Project: TestADSB_26_04_26_05
  Board:   ESP32S3 Dev Module, Arduino IDE
  Output:  GPIO4

  Назначение:
    Формирование тестовых Mode-S / ADS-B пакетов DF4, DF5, DF11, DF17, DF18, DF19.
    Вывод HEX пакета и контроль CRC в Serial.
    Выдача baseband ADS-B/Mode-S PPM импульсов высокого уровня на GPIO4.

  Данные самолета:
    ICAO      15D5C6
    Squawk    1234
    Callsign  TEST001
    Altitude  1005 ft, кодируется с Q-bit с округлением к шагу 25 ft
    Speed     1070 km/h (578 kt)
    Course    95 deg
    Base Latitude  55.95501
    Base Longitude 37.23166
    Movement: координаты изменяются вдоль курса 95 deg от -25 км до +25 км; скорость увеличена до 1070 км/ч; при обратном движении Course=275 deg.

  Период:
    Один полный набор пакетов DF4/DF5/DF11/DF17/DF17/DF18/DF19 каждую 1 секунду.

  ВАЖНО:
    Это не ВЧ 1090 MHz сигнал, а baseband PPM-посылка.
    Длительность каждого полубита = 0.5 мкс.
    Один бит = 1.0 мкс.
    Преамбула = 8.0 мкс.

  CRC:
    Расчет CRC сделан байтовым методом, совместимым с проектом приемника ADSB_RP2040_25_11_15_01.
    Полином 0xFFF409, initial value 0.

  Проверка синдрома:
    DF17/DF11: syndrome должен быть 000000.
    DF4/DF5: syndrome должен быть равен ICAO 15D5C6.

  Важно для приемника ADSB_RP2040_25_11_15_01:
    AircraftDictionary::IngestADSBPacket() принимает в словарь самолета только DF17.
    Поэтому координаты и скорость передаются как DF17 TC11 EVEN, DF17 TC11 ODD и DF17 TC19.
    DF18/DF19 не используются для основных данных, так как этот приемник их не заносит в aircraft dictionary.
*/

struct ModeSMessage {
  bool bits[112];
  int len;
  char name[28];
};


#include <Arduino.h>
#include <math.h>
#include <string.h>

#include "driver/rmt.h"
#include "driver/gpio.h"

#define ADSB_OUT_PIN       4
#define RMT_TX_CHANNEL     RMT_CHANNEL_0

// 80 MHz / 8 = 10 MHz, один tick = 0.1 мкс.
#define RMT_CLK_DIV        8
#define ADSB_HALF_US_TICKS 5     // 0.5 мкс

#define ICAO_ADDR          0x15D5C6UL

static const char CALLSIGN[] = "TEST001";

static const int    SQUAWK      = 1234;
static const int    ALTITUDE_FT = 1005;
static const int    SPEED_KMH   = 1070;
static const int    SPEED_KT    = 578;   // 1070 км/ч = 577.8 kt, округлено до 578 kt
static const float  COURSE_DEG  = 95.0f;
static const double BASE_LATITUDE    = 55.95501;
static const double BASE_LONGITUDE   = 37.23166;

// Движение самолета: вдоль COURSE_DEG, туда/обратно относительно базовой точки.
// Диапазон: -25 км ... +25 км от базовой точки.
static const double MOVE_RANGE_KM    = 25.0;
static const double EARTH_RADIUS_KM  = 6371.0;

static double gCurrentLatitude       = BASE_LATITUDE;
static double gCurrentLongitude      = BASE_LONGITUDE;
static double gTrackOffsetKm         = 0.0;
static int    gTrackDirection        = 1;
static uint32_t gLastMoveMs          = 0;

static float normalizeCourseDeg(float course) {
  while (course >= 360.0f) course -= 360.0f;
  while (course < 0.0f) course += 360.0f;
  return course;
}

static float currentCourseDeg() {
  // При движении вперед курс базовый.
  // При обратном движении курс разворачивается на 180 градусов.
  if (gTrackDirection >= 0) {
    return normalizeCourseDeg(COURSE_DEG);
  }
  return normalizeCourseDeg(COURSE_DEG + 180.0f);
}




// ------------------------------------------------------------
// Aircraft movement simulation
// ------------------------------------------------------------

static double degToRad(double deg) {
  return deg * M_PI / 180.0;
}

static double radToDeg(double rad) {
  return rad * 180.0 / M_PI;
}

static void calcPointByDistanceAndBearing(double latDeg, double lonDeg,
                                          double bearingDeg, double distanceKm,
                                          double &outLatDeg, double &outLonDeg) {
  // Прямая геодезическая задача на сфере.
  // distanceKm может быть отрицательной: это движение в обратную сторону от курса.
  double lat1 = degToRad(latDeg);
  double lon1 = degToRad(lonDeg);
  double brng = degToRad(bearingDeg);
  double dr = distanceKm / EARTH_RADIUS_KM;

  double sinLat1 = sin(lat1);
  double cosLat1 = cos(lat1);
  double sinDr = sin(dr);
  double cosDr = cos(dr);

  double lat2 = asin(sinLat1 * cosDr + cosLat1 * sinDr * cos(brng));
  double lon2 = lon1 + atan2(sin(brng) * sinDr * cosLat1,
                             cosDr - sinLat1 * sin(lat2));

  outLatDeg = radToDeg(lat2);
  outLonDeg = radToDeg(lon2);

  // Нормализация долготы в диапазон -180 ... +180.
  while (outLonDeg > 180.0) outLonDeg -= 360.0;
  while (outLonDeg < -180.0) outLonDeg += 360.0;
}

static void updateMovingAircraftPosition() {
  uint32_t now = millis();
  if (gLastMoveMs == 0) {
    gLastMoveMs = now;
    calcPointByDistanceAndBearing(BASE_LATITUDE, BASE_LONGITUDE, COURSE_DEG,
                                  gTrackOffsetKm, gCurrentLatitude, gCurrentLongitude);
    return;
  }

  uint32_t dtMs = now - gLastMoveMs;
  if (dtMs == 0) return;
  gLastMoveMs = now;

  // Скорость движения задана в км/ч.
  double speedKmPerSec = (double)SPEED_KMH / 3600.0;
  double deltaKm = speedKmPerSec * ((double)dtMs / 1000.0);

  gTrackOffsetKm += deltaKm * (double)gTrackDirection;

  if (gTrackOffsetKm >= MOVE_RANGE_KM) {
    gTrackOffsetKm = MOVE_RANGE_KM;
    gTrackDirection = -1;
  } else if (gTrackOffsetKm <= -MOVE_RANGE_KM) {
    gTrackOffsetKm = -MOVE_RANGE_KM;
    gTrackDirection = 1;
  }

  calcPointByDistanceAndBearing(BASE_LATITUDE, BASE_LONGITUDE, COURSE_DEG,
                                gTrackOffsetKm, gCurrentLatitude, gCurrentLongitude);
}

static void printCurrentPosition() {
  Serial.print("  POS lat=");
  Serial.print(gCurrentLatitude, 6);
  Serial.print(" lon=");
  Serial.print(gCurrentLongitude, 6);
  Serial.print(" offset_km=");
  Serial.print(gTrackOffsetKm, 3);
  Serial.print(" dir=");
  Serial.print(gTrackDirection > 0 ? "+" : "-");
  Serial.print(" course=");
  Serial.println(currentCourseDeg(), 1);
}

// ------------------------------------------------------------
// Basic bit helpers
// ------------------------------------------------------------

static void msgClear(ModeSMessage &m, const char *name) {
  memset(&m, 0, sizeof(m));
  strncpy(m.name, name, sizeof(m.name) - 1);
}

static void appendBit(ModeSMessage &m, bool bit) {
  if (m.len < 112) {
    m.bits[m.len++] = bit;
  }
}

static void appendBits(ModeSMessage &m, uint32_t value, int count) {
  for (int i = count - 1; i >= 0; i--) {
    appendBit(m, ((value >> i) & 1U) != 0);
  }
}

static int msgToBytes(const ModeSMessage &m, uint8_t *out, int outMax) {
  int bytes = (m.len + 7) / 8;
  if (bytes > outMax) bytes = outMax;

  for (int b = 0; b < bytes; b++) {
    uint8_t v = 0;

    for (int i = 0; i < 8; i++) {
      int bitIndex = b * 8 + i;
      v <<= 1;
      if (bitIndex < m.len && m.bits[bitIndex]) {
        v |= 1;
      }
    }

    out[b] = v;
  }

  return bytes;
}

static void msgToHex(const ModeSMessage &m, char *out, size_t outSize) {
  uint8_t buf[14];
  int bytes = msgToBytes(m, buf, sizeof(buf));
  int pos = 0;

  for (int i = 0; i < bytes; i++) {
    if (pos + 2 < (int)outSize) {
      sprintf(out + pos, "%02X", buf[i]);
      pos += 2;
    }
  }

  out[pos] = 0;
}

static void printHex24(uint32_t v) {
  v &= 0xFFFFFFUL;
  if (v < 0x100000UL) Serial.print('0');
  if (v < 0x10000UL)  Serial.print('0');
  if (v < 0x1000UL)   Serial.print('0');
  if (v < 0x100UL)    Serial.print('0');
  if (v < 0x10UL)     Serial.print('0');
  Serial.print(v, HEX);
}

// ------------------------------------------------------------
// CRC24 Mode-S, точно как в ADSB_RP2040_25_11_15_01
// ------------------------------------------------------------

// Таблица CRC24 взята из ADSB_RP2040_25_11_15_01 / crc_tables.h
static const uint32_t crc24_table[256] = {
    0x0, 0xFFF409, 0x1C1B, 0xFFE812, 0x3836, 0xFFCC3F, 0x242D, 0xFFD024, 0x706C, 0xFF8465, 0x6C77, 0xFF987E, 0x485A,
    0xFFBC53, 0x5441, 0xFFA048, 0xE0D8, 0xFF14D1, 0xFCC3, 0xFF08CA, 0xD8EE, 0xFF2CE7, 0xC4F5, 0xFF30FC, 0x90B4, 0xFF64BD,
    0x8CAF, 0xFF78A6, 0xA882, 0xFF5C8B, 0xB499, 0xFF4090, 0x1C1B0, 0xFE35B9, 0x1DDAB, 0xFE29A2, 0x1F986, 0xFE0D8F,
    0x1E59D, 0xFE1194, 0x1B1DC, 0xFE45D5, 0x1ADC7, 0xFE59CE, 0x189EA, 0xFE7DE3, 0x195F1, 0xFE61F8, 0x12168, 0xFED561,
    0x13D73, 0xFEC97A, 0x1195E, 0xFEED57, 0x10545, 0xFEF14C, 0x15104, 0xFEA50D, 0x14D1F, 0xFEB916, 0x16932, 0xFE9D3B,
    0x17529, 0xFE8120, 0x38360, 0xFC7769, 0x39F7B, 0xFC6B72, 0x3BB56, 0xFC4F5F, 0x3A74D, 0xFC5344, 0x3F30C, 0xFC0705,
    0x3EF17, 0xFC1B1E, 0x3CB3A, 0xFC3F33, 0x3D721, 0xFC2328, 0x363B8, 0xFC97B1, 0x37FA3, 0xFC8BAA, 0x35B8E, 0xFCAF87,
    0x34795, 0xFCB39C, 0x313D4, 0xFCE7DD, 0x30FCF, 0xFCFBC6, 0x32BE2, 0xFCDFEB, 0x337F9, 0xFCC3F0, 0x242D0, 0xFDB6D9,
    0x25ECB, 0xFDAAC2, 0x27AE6, 0xFD8EEF, 0x266FD, 0xFD92F4, 0x232BC, 0xFDC6B5, 0x22EA7, 0xFDDAAE, 0x20A8A, 0xFDFE83,
    0x21691, 0xFDE298, 0x2A208, 0xFD5601, 0x2BE13, 0xFD4A1A, 0x29A3E, 0xFD6E37, 0x28625, 0xFD722C, 0x2D264, 0xFD266D,
    0x2CE7F, 0xFD3A76, 0x2EA52, 0xFD1E5B, 0x2F649, 0xFD0240, 0x706C0, 0xF8F2C9, 0x71ADB, 0xF8EED2, 0x73EF6, 0xF8CAFF,
    0x722ED, 0xF8D6E4, 0x776AC, 0xF882A5, 0x76AB7, 0xF89EBE, 0x74E9A, 0xF8BA93, 0x75281, 0xF8A688, 0x7E618, 0xF81211,
    0x7FA03, 0xF80E0A, 0x7DE2E, 0xF82A27, 0x7C235, 0xF8363C, 0x79674, 0xF8627D, 0x78A6F, 0xF87E66, 0x7AE42, 0xF85A4B,
    0x7B259, 0xF84650, 0x6C770, 0xF93379, 0x6DB6B, 0xF92F62, 0x6FF46, 0xF90B4F, 0x6E35D, 0xF91754, 0x6B71C, 0xF94315,
    0x6AB07, 0xF95F0E, 0x68F2A, 0xF97B23, 0x69331, 0xF96738, 0x627A8, 0xF9D3A1, 0x63BB3, 0xF9CFBA, 0x61F9E, 0xF9EB97,
    0x60385, 0xF9F78C, 0x657C4, 0xF9A3CD, 0x64BDF, 0xF9BFD6, 0x66FF2, 0xF99BFB, 0x673E9, 0xF987E0, 0x485A0, 0xFB71A9,
    0x499BB, 0xFB6DB2, 0x4BD96, 0xFB499F, 0x4A18D, 0xFB5584, 0x4F5CC, 0xFB01C5, 0x4E9D7, 0xFB1DDE, 0x4CDFA, 0xFB39F3,
    0x4D1E1, 0xFB25E8, 0x46578, 0xFB9171, 0x47963, 0xFB8D6A, 0x45D4E, 0xFBA947, 0x44155, 0xFBB55C, 0x41514, 0xFBE11D,
    0x4090F, 0xFBFD06, 0x42D22, 0xFBD92B, 0x43139, 0xFBC530, 0x54410, 0xFAB019, 0x5580B, 0xFAAC02, 0x57C26, 0xFA882F,
    0x5603D, 0xFA9434, 0x5347C, 0xFAC075, 0x52867, 0xFADC6E, 0x50C4A, 0xFAF843, 0x51051, 0xFAE458, 0x5A4C8, 0xFA50C1,
    0x5B8D3, 0xFA4CDA, 0x59CFE, 0xFA68F7, 0x580E5, 0xFA74EC, 0x5D4A4, 0xFA20AD, 0x5C8BF, 0xFA3CB6, 0x5EC92, 0xFA189B,
    0x5F089, 0xFA0480
};

static uint32_t crc24Bytes(const uint8_t *buffer, int len) {
  // Полная копия алгоритма приемника ADSB_RP2040_25_11_15_01 / crc.cpp:
  // crc = ((crc << 8) ^ crc24_table[((crc >> 16) ^ byte) & 0xFF]) & 0xFFFFFF;
  uint32_t crc = 0;

  for (int i = 0; i < len; i++) {
    uint8_t byte = buffer[i];
    crc = ((crc << 8) ^ crc24_table[((crc >> 16) ^ byte) & 0xFF]) & 0xFFFFFFUL;
  }

  return crc & 0xFFFFFFUL;
}

static uint32_t crc24MessagePayload(const ModeSMessage &m) {
  uint8_t payload[14];
  int payloadBytes = msgToBytes(m, payload, sizeof(payload));
  return crc24Bytes(payload, payloadBytes);
}

static void appendCRC(ModeSMessage &m, bool xorIcao) {
  uint32_t crc = crc24MessagePayload(m);

  if (xorIcao) {
    crc ^= ICAO_ADDR;
  }

  appendBits(m, crc, 24);
}

static void printCrcCheck(const ModeSMessage &m) {
  uint8_t buf[14];
  int bytes = msgToBytes(m, buf, sizeof(buf));

  if (bytes < 7) {
    return;
  }

  uint32_t calc = crc24Bytes(buf, bytes - 3);
  uint32_t parity =
      ((uint32_t)buf[bytes - 3] << 16) |
      ((uint32_t)buf[bytes - 2] << 8) |
      ((uint32_t)buf[bytes - 1]);

  uint32_t syndrome = calc ^ parity;

  Serial.print("  CRC calc=");
  printHex24(calc);
  Serial.print(" parity=");
  printHex24(parity);
  Serial.print(" syndrome=");
  printHex24(syndrome);

  if ((m.name[0] == 'D') && (m.name[1] == 'F') &&
      ((m.name[2] == '4') || (m.name[2] == '5'))) {
    Serial.print(" expected ICAO=");
    printHex24(ICAO_ADDR);
  } else {
    Serial.print(" expected=000000");
  }

  Serial.println();
}

// ------------------------------------------------------------
// Encoders
// ------------------------------------------------------------

static uint16_t encodeAltitudeQ12(int altitudeFt) {
  // ADS-B 12-bit altitude field with Q bit.
  // N = (altitude + 1000) / 25.
  // Q-bit packet кодирует высоту с шагом 25 ft.
  int n = (int)lround((altitudeFt + 1000) / 25.0);

  if (n < 0) n = 0;
  if (n > 0x7FF) n = 0x7FF;

  // 12-bit AC field, Q bit = bit 4.
  uint16_t ac = ((n & 0x0FF0) << 1) | 0x0010 | (n & 0x000F);
  return ac & 0x0FFF;
}

static uint16_t encodeAltitudeModeS13(int altitudeFt) {
  // Упрощенное 13-битное поле AC для DF4.
  // Для тестового приемника важно получить корректную структуру и AP parity.
  return encodeAltitudeQ12(altitudeFt) & 0x1FFF;
}

static uint16_t encodeSquawkModeA(int squawk) {
  // Mode-A identity field, порядок битов:
  // C1 A1 C2 A2 C4 A4 X B1 D1 B2 D2 B4 D4
  int a = (squawk / 1000) % 10;
  int b = (squawk / 100) % 10;
  int c = (squawk / 10) % 10;
  int d = squawk % 10;

  uint16_t code = 0;

  auto setBit = [&](int posFromLeft, bool value) {
    if (value) {
      code |= (1U << (12 - posFromLeft));
    }
  };

  setBit(0,  (c & 1) != 0); // C1
  setBit(1,  (a & 1) != 0); // A1
  setBit(2,  (c & 2) != 0); // C2
  setBit(3,  (a & 2) != 0); // A2
  setBit(4,  (c & 4) != 0); // C4
  setBit(5,  (a & 4) != 0); // A4
  setBit(6,  false);         // X
  setBit(7,  (b & 1) != 0); // B1
  setBit(8,  (d & 1) != 0); // D1
  setBit(9,  (b & 2) != 0); // B2
  setBit(10, (d & 2) != 0); // D2
  setBit(11, (b & 4) != 0); // B4
  setBit(12, (d & 4) != 0); // D4

  return code & 0x1FFF;
}

static uint8_t adsbCharCode(char ch) {
  if (ch >= 'a' && ch <= 'z') ch -= 32;

  if (ch >= 'A' && ch <= 'Z') {
    return ch - 'A' + 1;
  }

  if (ch >= '0' && ch <= '9') {
    return ch - '0' + 48;
  }

  return 32; // space
}

// ------------------------------------------------------------
// CPR encoding
// ------------------------------------------------------------

static int cprNL(double lat) {
  double a = fabs(lat);

  if (a < 10.47047130) return 59;
  if (a < 14.82817437) return 58;
  if (a < 18.18626357) return 57;
  if (a < 21.02939493) return 56;
  if (a < 23.54504487) return 55;
  if (a < 25.82924707) return 54;
  if (a < 27.93898710) return 53;
  if (a < 29.91135686) return 52;
  if (a < 31.77209708) return 51;
  if (a < 33.53993436) return 50;
  if (a < 35.22899598) return 49;
  if (a < 36.85025108) return 48;
  if (a < 38.41241892) return 47;
  if (a < 39.92256684) return 46;
  if (a < 41.38651832) return 45;
  if (a < 42.80914012) return 44;
  if (a < 44.19454951) return 43;
  if (a < 45.54626723) return 42;
  if (a < 46.86733252) return 41;
  if (a < 48.16039128) return 40;
  if (a < 49.42776439) return 39;
  if (a < 50.67150166) return 38;
  if (a < 51.89342469) return 37;
  if (a < 53.09516153) return 36;
  if (a < 54.27817472) return 35;
  if (a < 55.44378444) return 34;
  if (a < 56.59318756) return 33;
  if (a < 57.72747354) return 32;
  if (a < 58.84763776) return 31;
  if (a < 59.95459277) return 30;
  if (a < 61.04917774) return 29;
  if (a < 62.13216659) return 28;
  if (a < 63.20427479) return 27;
  if (a < 64.26616523) return 26;
  if (a < 65.31845310) return 25;
  if (a < 66.36171008) return 24;
  if (a < 67.39646774) return 23;
  if (a < 68.42322022) return 22;
  if (a < 69.44242631) return 21;
  if (a < 70.45451075) return 20;
  if (a < 71.45986473) return 19;
  if (a < 72.45884545) return 18;
  if (a < 73.45177442) return 17;
  if (a < 74.43893416) return 16;
  if (a < 75.42056257) return 15;
  if (a < 76.39684391) return 14;
  if (a < 77.36789461) return 13;
  if (a < 78.33374083) return 12;
  if (a < 79.29428225) return 11;
  if (a < 80.24923213) return 10;
  if (a < 81.19801349) return 9;
  if (a < 82.13956981) return 8;
  if (a < 83.07199445) return 7;
  if (a < 83.99173563) return 6;
  if (a < 84.89166191) return 5;
  if (a < 85.75541621) return 4;
  if (a < 86.53536998) return 3;
  if (a < 87.00000000) return 2;

  return 1;
}

static double modPositive(double x, double y) {
  double r = fmod(x, y);
  if (r < 0) r += y;
  return r;
}

static void encodeCPR(double lat, double lon, bool odd, uint32_t &latCpr, uint32_t &lonCpr) {
  const int NZ = 15;

  double dLat = odd ? 360.0 / (4 * NZ - 1) : 360.0 / (4 * NZ);
  double yz = floor(131072.0 * modPositive(lat, dLat) / dLat + 0.5);

  if (yz >= 131072.0) yz = 0;

  int nl = cprNL(lat) - (odd ? 1 : 0);
  if (nl < 1) nl = 1;

  double dLon = 360.0 / nl;
  double xz = floor(131072.0 * modPositive(lon, dLon) / dLon + 0.5);

  if (xz >= 131072.0) xz = 0;

  latCpr = ((uint32_t)yz) & 0x1FFFFUL;
  lonCpr = ((uint32_t)xz) & 0x1FFFFUL;
}

// ------------------------------------------------------------
// ADS-B ME builders
// ------------------------------------------------------------

static void appendME_Callsign(ModeSMessage &m) {
  // TC=4, aircraft identification.
  appendBits(m, 4, 5);
  appendBits(m, 0, 3);

  char cs[9];
  memset(cs, ' ', 8);
  cs[8] = 0;

  for (int i = 0; i < 8 && CALLSIGN[i]; i++) {
    cs[i] = CALLSIGN[i];
  }

  for (int i = 0; i < 8; i++) {
    appendBits(m, adsbCharCode(cs[i]), 6);
  }
}

static void appendME_Position(ModeSMessage &m, bool odd) {
  uint32_t latCpr = 0;
  uint32_t lonCpr = 0;
  encodeCPR(gCurrentLatitude, gCurrentLongitude, odd, latCpr, lonCpr);

  // TC=11 airborne position, barometric altitude.
  appendBits(m, 11, 5);
  appendBits(m, 0, 2);                              // surveillance status
  appendBits(m, 0, 1);                              // single antenna flag
  appendBits(m, encodeAltitudeQ12(ALTITUDE_FT), 12);
  appendBits(m, 0, 1);                              // time flag
  appendBits(m, odd ? 1 : 0, 1);                    // CPR F flag
  appendBits(m, latCpr, 17);
  appendBits(m, lonCpr, 17);
}

static void appendME_Velocity(ModeSMessage &m) {
  // TC=19 subtype=1, ground speed.
  double activeCourse = currentCourseDeg();
  double rad = activeCourse * M_PI / 180.0;

  double vx = SPEED_KT * sin(rad); // east/west
  double vy = SPEED_KT * cos(rad); // north/south

  int ewDir = vx >= 0 ? 0 : 1; // 0 east, 1 west
  int nsDir = vy >= 0 ? 0 : 1; // 0 north, 1 south

  int ewVel = (int)lround(fabs(vx)) + 1;
  int nsVel = (int)lround(fabs(vy)) + 1;

  if (ewVel > 1023) ewVel = 1023;
  if (nsVel > 1023) nsVel = 1023;

  appendBits(m, 19, 5);      // TC
  appendBits(m, 1, 3);       // subtype
  appendBits(m, 0, 1);       // intent change
  appendBits(m, 0, 1);       // reserved
  appendBits(m, 0, 3);       // NACv

  appendBits(m, ewDir, 1);
  appendBits(m, ewVel, 10);

  appendBits(m, nsDir, 1);
  appendBits(m, nsVel, 10);

  appendBits(m, 0, 1);       // vertical rate source
  appendBits(m, 0, 1);       // vertical rate sign
  appendBits(m, 1, 9);       // vertical rate value: 1 = 0 fpm, чтобы приемник считал поле доступным

  appendBits(m, 0, 2);       // reserved
  appendBits(m, 0, 1);       // GNSS/baro diff sign
  appendBits(m, 0, 7);       // GNSS/baro diff value
}

// ------------------------------------------------------------
// Full frame builders
// ------------------------------------------------------------

static void buildDF4(ModeSMessage &m) {
  msgClear(m, "DF4 ALT");

  appendBits(m, 4, 5);                              // DF
  appendBits(m, 0, 3);                              // FS
  appendBits(m, 0, 5);                              // DR
  appendBits(m, 0, 6);                              // UM
  appendBits(m, encodeAltitudeModeS13(ALTITUDE_FT), 13);

  appendCRC(m, true);                               // AP = CRC XOR ICAO
}

static void buildDF5(ModeSMessage &m) {
  msgClear(m, "DF5 SQUAWK");

  appendBits(m, 5, 5);                              // DF
  appendBits(m, 0, 3);                              // FS
  appendBits(m, 0, 5);                              // DR
  appendBits(m, 0, 6);                              // UM
  appendBits(m, encodeSquawkModeA(SQUAWK), 13);

  appendCRC(m, true);                               // AP = CRC XOR ICAO
}

static void buildDF11(ModeSMessage &m) {
  msgClear(m, "DF11 ALLCALL");

  appendBits(m, 11, 5);                             // DF
  appendBits(m, 5, 3);                              // CA
  appendBits(m, ICAO_ADDR, 24);                     // AA

  appendCRC(m, false);                              // PI
}

static void buildDF17Callsign(ModeSMessage &m) {
  msgClear(m, "DF17 CALLSIGN");

  appendBits(m, 17, 5);                             // DF
  appendBits(m, 5, 3);                              // CA
  appendBits(m, ICAO_ADDR, 24);                     // AA
  appendME_Callsign(m);
  appendCRC(m, false);
}

static void buildDF17PositionEven(ModeSMessage &m) {
  msgClear(m, "DF17 POS EVEN");

  appendBits(m, 17, 5);                             // DF
  appendBits(m, 5, 3);                              // CA
  appendBits(m, ICAO_ADDR, 24);                     // AA
  appendME_Position(m, false);
  appendCRC(m, false);
}

static void buildDF17PositionOdd(ModeSMessage &m) {
  msgClear(m, "DF17 POS ODD");

  appendBits(m, 17, 5);                             // DF
  appendBits(m, 5, 3);                              // CA
  appendBits(m, ICAO_ADDR, 24);                     // AA
  appendME_Position(m, true);
  appendCRC(m, false);
}

static void buildDF17Velocity(ModeSMessage &m) {
  msgClear(m, "DF17 VELOCITY");

  appendBits(m, 17, 5);                             // DF
  appendBits(m, 5, 3);                              // CA
  appendBits(m, ICAO_ADDR, 24);                     // AA
  appendME_Velocity(m);                             // ME TC=19
  appendCRC(m, false);
}

static void buildDF18PositionOdd(ModeSMessage &m) {
  msgClear(m, "DF18 POS ODD");

  appendBits(m, 18, 5);                             // DF
  appendBits(m, 0, 3);                              // CF
  appendBits(m, ICAO_ADDR, 24);                     // AA/address
  appendME_Position(m, true);
  appendCRC(m, false);
}

static void buildDF19Velocity(ModeSMessage &m) {
  msgClear(m, "DF19 VELOCITY");

  appendBits(m, 19, 5);                             // DF
  appendBits(m, 0, 3);                              // AF/reserved test field
  appendBits(m, ICAO_ADDR, 24);                     // address
  appendME_Velocity(m);
  appendCRC(m, false);
}

// ------------------------------------------------------------
// RMT pulse output
// ------------------------------------------------------------

static bool setupRmt() {
  rmt_config_t config;
  memset(&config, 0, sizeof(config));

  config.rmt_mode = RMT_MODE_TX;
  config.channel = RMT_TX_CHANNEL;
  config.gpio_num = (gpio_num_t)ADSB_OUT_PIN;
  config.mem_block_num = 3;
  config.clk_div = RMT_CLK_DIV;

  config.tx_config.loop_en = false;
  config.tx_config.carrier_en = false;
  config.tx_config.idle_output_en = true;
  config.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;

  esp_err_t err = rmt_config(&config);
  if (err != ESP_OK) {
    Serial.print("RMT config error: ");
    Serial.println((int)err);
    return false;
  }

  err = rmt_driver_install(RMT_TX_CHANNEL, 0, 0);
  if (err != ESP_OK) {
    Serial.print("RMT driver install error: ");
    Serial.println((int)err);
    return false;
  }

  return true;
}

static void transmitModeS(const ModeSMessage &m) {
  rmt_item32_t items[128];
  int n = 0;

  // ADS-B / Mode-S preamble, 8 мкс.
  // HIGH pulses at 0.0, 1.0, 3.5, 4.5 мкс, each 0.5 мкс.
  const uint8_t preambleSlots[16] = {
    1, 0,
    1, 0,
    0, 0,
    0, 1,
    0, 1,
    0, 0,
    0, 0,
    0, 0
  };

  for (int i = 0; i < 16; i += 2) {
    items[n].level0 = preambleSlots[i];
    items[n].duration0 = ADSB_HALF_US_TICKS;
    items[n].level1 = preambleSlots[i + 1];
    items[n].duration1 = ADSB_HALF_US_TICKS;
    n++;
  }

  // Data bits.
  // bit 1 = HIGH first 0.5 мкс, LOW second 0.5 мкс.
  // bit 0 = LOW first 0.5 мкс, HIGH second 0.5 мкс.
  for (int i = 0; i < m.len; i++) {
    if (m.bits[i]) {
      items[n].level0 = 1;
      items[n].duration0 = ADSB_HALF_US_TICKS;
      items[n].level1 = 0;
      items[n].duration1 = ADSB_HALF_US_TICKS;
    } else {
      items[n].level0 = 0;
      items[n].duration0 = ADSB_HALF_US_TICKS;
      items[n].level1 = 1;
      items[n].duration1 = ADSB_HALF_US_TICKS;
    }
    n++;
  }

  rmt_write_items(RMT_TX_CHANNEL, items, n, true);
  rmt_wait_tx_done(RMT_TX_CHANNEL, pdMS_TO_TICKS(20));
}

// ------------------------------------------------------------
// Main helpers
// ------------------------------------------------------------

static void printAndSend(const ModeSMessage &m) {
  char hex[40];
  msgToHex(m, hex, sizeof(hex));

  Serial.print(m.name);
  Serial.print("  LEN=");
  Serial.print(m.len);
  Serial.print("  HEX=");
  Serial.println(hex);

  printCrcCheck(m);
  transmitModeS(m);

  // Небольшая пауза между пакетами внутри одного секундного цикла.
  delay(20);
}

static void sendAllPacketsOnce() {
  ModeSMessage m;

  buildDF4(m);
  printAndSend(m);

  buildDF5(m);
  printAndSend(m);

  buildDF11(m);
  printAndSend(m);

  buildDF17Callsign(m);
  printAndSend(m);

  buildDF17PositionEven(m);
  printAndSend(m);

  // Для приемника ADSB_RP2040_25_11_15_01 координаты должны быть именно DF17.
  // Он не заносит DF18 в aircraft dictionary.
  buildDF17PositionOdd(m);
  printAndSend(m);

  // Скорость и курс передаются как DF17 + ME TypeCode 19.
  // DF19 как downlink format приемник не применяет к aircraft dictionary.
  buildDF17Velocity(m);
  printAndSend(m);

  printCurrentPosition();
  Serial.println("---");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(ADSB_OUT_PIN, OUTPUT);
  digitalWrite(ADSB_OUT_PIN, LOW);

  Serial.println();
  Serial.println("Start TestADSB_26_04_26_05");
  Serial.print("Speed km/h: "); Serial.println(SPEED_KMH);
  Serial.print("Speed kt: "); Serial.println(SPEED_KT);
  Serial.println("ESP32S3 ADS-B / Mode-S baseband test generator");
  Serial.print("Output GPIO: ");
  Serial.println(ADSB_OUT_PIN);
  Serial.println("Half-bit pulse: 0.5 us");
  Serial.println("Bit time: 1.0 us");
  Serial.println("Preamble: 8.0 us");
  Serial.println("CRC: receiver table algorithm, polynomial 0xFFF409, initial 0");
  Serial.println("Aircraft:");
  Serial.println("  ICAO      15D5C6");
  Serial.println("  Squawk    1234");
  Serial.println("  Callsign  TEST001");
  Serial.println("  Altitude  1005 ft");
  Serial.println("  Speed     1070 km/h (578 kt)");
  Serial.println("  Course    95 deg");
  Serial.println("  Base latitude   55.95501");
  Serial.println("  Base longitude  37.23166");
  Serial.println("  Movement        -25 km ... +25 km from base point along course");
  Serial.println("DF17 position EVEN + DF17 position ODD + DF17 velocity TC19 enabled");

  if (!setupRmt()) {
    Serial.println("RMT init failed. Output disabled.");
  } else {
    Serial.println("RMT init OK.");
  }

  Serial.println();
}

void loop() {
  uint32_t startMs = millis();

  updateMovingAircraftPosition();
  sendAllPacketsOnce();

  while ((uint32_t)(millis() - startMs) < 1000UL) {
    delay(1);
  }
}
