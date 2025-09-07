#include <Arduino.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "adsb_cpr.h"
#include "adsb_gillham.h"

// ----------------------- ВНУТРЕННИЕ НАСТРОЙКИ -----------------------
static double g_ref_lat = 55.0;
static double g_ref_lon = 37.0;
static uint32_t g_pair_window_ms = 10000; // 10 сек

// Кэш CPR для глобального декода (even/odd пара)
struct CprSlot {
  uint32_t icao;
  bool has_even;
  bool has_odd;
  uint32_t t_even_ms;
  uint32_t t_odd_ms;
  uint32_t lat_even, lon_even;
  uint32_t lat_odd,  lon_odd;
  double   last_lat, last_lon; // удобство локального улучшения
};

#define CPR_CACHE_SLOTS 64
static CprSlot g_cpr_cache[CPR_CACHE_SLOTS];

// ----------------------- ВСПОМОГАТЕЛЬНЫЕ ----------------------------
static inline uint32_t rotl24(uint32_t x, unsigned s) { return ((x<<s) | (x>>(24-s))) & 0xFFFFFF; }

// Достаёт БИТЫ из сообщения (MSB=бит 0)
// bit 0 — самый старший бит байта msg[0]
static uint32_t getBits(const uint8_t *msg, int startBit, int bitLen) {
  uint32_t acc = 0;
  for (int i=0; i<bitLen; ++i) {
    int b = startBit + i;
    int byte = b >> 3;
    int bit  = 7 - (b & 7);
    uint8_t v = (msg[byte] >> bit) & 1;
    acc = (acc << 1) | v;
  }
  return acc;
}

// Таблица 6-битных символов для позывного (TC=1..4)
static const char *charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZ_____"
                             "______________________________"
                             " 0123456789______";

// Безопасный заполнитель для позывного
static void decodeCallsign(const uint8_t *me, char out[16]) {
  // В ME (56 бит) для TC=1..4: 8 символов по 6 бит, начиная с бита 8 ME (после типкода и т.п.)
  // Но точные позиции: в ES identification message после TypeCode(5), EC(3) идут 48 бит (=8*6)
  // Возьмём из общего сообщения: ME начинается с бита 32.
  // Возьмём 48 бит начиная с (32+8) = 40
  memset(out, 0, 16);
  int base = 32 + 8;
  for (int i=0;i<8;i++) {
    uint32_t v = getBits(me, base + i*6, 6);
    char c = (v < 64) ? charset[v] : ' ';
    out[i] = (c=='_' ? ' ' : c);
  }
  out[8] = 0;
  // trim
  for (int i=7;i>=0;i--) { if (out[i]==' ') out[i]=0; else break; }
}

// NL(lat): количество зон долгот для данной широты (DO-260B)
static int cprNL(double lat) {
  if (lat < 0) lat = -lat;
  if (lat < 10.47047130) return 59;
  if (lat < 14.82817437) return 58;
  if (lat < 18.18626357) return 57;
  if (lat < 21.02939493) return 56;
  if (lat < 23.54504487) return 55;
  if (lat < 25.82924707) return 54;
  if (lat < 27.93898710) return 53;
  if (lat < 29.91135686) return 52;
  if (lat < 31.77209708) return 51;
  if (lat < 33.53993436) return 50;
  if (lat < 35.22899598) return 49;
  if (lat < 36.85025108) return 48;
  if (lat < 38.41241892) return 47;
  if (lat < 39.92256684) return 46;
  if (lat < 41.38651832) return 45;
  if (lat < 42.80914012) return 44;
  if (lat < 44.19454951) return 43;
  if (lat < 45.54626723) return 42;
  if (lat < 46.86733252) return 41;
  if (lat < 48.16039128) return 40;
  if (lat < 49.42776439) return 39;
  if (lat < 50.67150166) return 38;
  if (lat < 51.89342469) return 37;
  if (lat < 53.09516153) return 36;
  if (lat < 54.27817472) return 35;
  if (lat < 55.44378444) return 34;
  if (lat < 56.59318756) return 33;
  if (lat < 57.72747354) return 32;
  if (lat < 58.84763776) return 31;
  if (lat < 59.95459277) return 30;
  if (lat < 61.04917774) return 29;
  if (lat < 62.13216659) return 28;
  if (lat < 63.20427479) return 27;
  if (lat < 64.26616523) return 26;
  if (lat < 65.31845310) return 25;
  if (lat < 66.36171008) return 24;
  if (lat < 67.39646774) return 23;
  if (lat < 68.42322022) return 22;
  if (lat < 69.44242631) return 21;
  if (lat < 70.45451075) return 20;
  if (lat < 71.45986473) return 19;
  if (lat < 72.45884545) return 18;
  if (lat < 73.45177442) return 17;
  if (lat < 74.43893416) return 16;
  if (lat < 75.42056257) return 15;
  if (lat < 76.39684391) return 14;
  if (lat < 77.36789461) return 13;
  if (lat < 78.33374083) return 12;
  if (lat < 79.29428225) return 11;
  if (lat < 80.24923213) return 10;
  if (lat < 81.19801349) return 9;
  if (lat < 82.13956981) return 8;
  if (lat < 83.07199445) return 7;
  if (lat < 83.99173563) return 6;
  if (lat < 84.89166191) return 5;
  if (lat < 85.75541621) return 4;
  if (lat < 86.53536998) return 3;
  if (lat < 87.00000000) return 2;
  return 1;
}

static double cprDlat(bool odd) { return odd ? (360.0/59.0) : (360.0/60.0); }

static int cprMod(int a, int b) {
  int m = a % b;
  if (m < 0) m += b;
  return m;
}

// Глобальная CPR (pair) — возвращает true, если успешно
static bool cprGlobal(uint32_t lat_even, uint32_t lon_even,
                      uint32_t lat_odd,  uint32_t lon_odd,
                      double &o_lat, double &o_lon) {
  // по DO-260B
  const int NZ = 15;
  int j = floor( (59*lat_even - 60*lat_odd)/131072.0 + 0.5 );
  double rlat_even = cprDlat(false) * (cprMod(j,60) + lat_even/131072.0);
  double rlat_odd  = cprDlat(true)  * (cprMod(j,59) + lat_odd /131072.0);

  // Выбираем ту, у которой NL>0
  double lat = (fabs(rlat_even) <= 90 && cprNL(rlat_even) > 0) ? rlat_even : rlat_odd;
  int nl = cprNL(lat);
  if (nl <= 0) return false;

  int ni = nl - ( (j % 2)==0 ? 0 : 1 );
  double dlon = 360.0 / ni;

  int m = floor( (lon_even*(nl-1) - lon_odd*nl)/131072.0 + 0.5 );
  double lon = dlon * (cprMod(m, ni) + (( (j%2)==0 ? lon_even : lon_odd)/131072.0));

  if (lon > 180.0) lon -= 360.0;
  o_lat = lat; o_lon = lon;
  return true;
}

// Локальная CPR по опорной точке g_ref_lat/lon
static bool cprLocal(uint32_t lat_cpr, uint32_t lon_cpr, bool odd,
                     double refLat, double refLon, double &o_lat, double &o_lon) {
  double dLat = cprDlat(odd);
  int    j = floor(refLat / dLat) + floor( 0.5 + ((refLat - floor(refLat/dLat)*dLat) / dLat - lat_cpr/131072.0) );
  double lat = dLat * (j + lat_cpr/131072.0);
  if (lat >= 270) lat -= 360;
  int nl = cprNL(lat);
  if (nl == 0) { // на полюсах — lon = 0
    o_lat = lat; o_lon = 0;
    return true;
  }
  int ni = nl - (odd ? 1 : 0);
  double dLon = 360.0 / ni;
  int m = floor(refLon/dLon) +
          floor( 0.5 + ((refLon - floor(refLon/dLon)*dLon)/dLon - lon_cpr/131072.0) );
  double lon = dLon * (m + lon_cpr/131072.0);
  if (lon > 180.0) lon -= 360.0;
  o_lat = lat; o_lon = lon;
  return true;
}

// Получить/создать слот CPR по ICAO
static CprSlot* getCprSlot(uint32_t icao) {
  int freeIdx = -1;
  for (int i=0;i<CPR_CACHE_SLOTS;i++) {
    if (g_cpr_cache[i].icao == icao) return &g_cpr_cache[i];
    if (freeIdx<0 && g_cpr_cache[i].icao==0) freeIdx=i;
  }
  if (freeIdx<0) freeIdx = 0; // простая замена
  memset(&g_cpr_cache[freeIdx], 0, sizeof(CprSlot));
  g_cpr_cache[freeIdx].icao = icao;
  return &g_cpr_cache[freeIdx];
}

// ----------------------- SQUAWK (DF5/DF21) --------------------------
// Gillham Mode A (Identity) : 4 октальных цифры кодируются битовыми группами
// Используем стандартные соответствия (как в dump1090).
static int decodeModeAfromID13(uint32_t id13) {
  // id13 — 13 бит (A,C,B,D interleaved). Но в ответе DF5 приходит 12 бит + X?
  // Практически в Mode S identity поле — 12 бит, старшие D1/D2/D4/B? — используем таблицу:
  // Здесь применим приём dump1090: преобразуем 13-бит на основе расстановки:
  //   C1 A1 C2 A2 C4 A4  B1 D1 B2 D2 B4 D4  (LSB отсутствует X)
  // Нам приходит 12 бит — соберём «квази-13» со старшим 0 в X позиции.
  // Далее маппинг в 4 октальные цифры.
  uint16_t a = 0;
  // В этом примере id13 уже упакован как 12 бит в порядке: C1,A1,C2,A2,C4,A4,B1,D1,B2,D2,B4,D4
  // Возьмём каждую четвёрку для октальной цифры:
  int A = ((id13 >> 10)&1) | (((id13 >> 8)&1)<<1) | (((id13 >> 6)&1)<<2); // A1,A2,A4
  int B = ((id13 >> 5)&1)  | (((id13 >> 3)&1)<<1) | (((id13 >> 1)&1)<<2); // B1,B2,B4
  int C = ((id13 >> 11)&1) | (((id13 >> 9)&1)<<1) | (((id13 >> 7)&1)<<2); // C1,C2,C4
  int D = ((id13 >> 4)&1)  | (((id13 >> 2)&1)<<1) | (((id13 >> 0)&1)<<2); // D1,D2,D4
  // Каждая тройка — октальная цифра 0..7
  return ( (D & 7) | ((C & 7)<<3) | ((B & 7)<<6) | ((A & 7)<<9) ); // упакуем 4 цифры по 3 бита
}

static int unpackSquawkDigits(int packed) {
  // packed: 12 бит в 4 октальных цифры (3 бита на цифру), порядок DCBA в этом пакете
  int d1 = (packed     ) & 7;
  int d2 = (packed >> 3) & 7;
  int d3 = (packed >> 6) & 7;
  int d4 = (packed >> 9) & 7;
  return d4*1000 + d3*100 + d2*10 + d1;
}

// ----------------------- VELOCITY (TC=19) ---------------------------
static void decodeVelocityTC19(const uint8_t *msg, DecodedADSB &out) {
  // msg — весь кадр (112 бит)
  // ME поле: биты [32..88)
  uint32_t meTC = getBits(msg, 32, 5);
  uint32_t subtype = getBits(msg, 37, 3);
  if (meTC != 19) return;

  if (subtype==1 || subtype==2) {
    // Ground speed (V_ew, V_ns), vertical rate
    int s_ew = getBits(msg, 45, 1);
    int v_ew = getBits(msg, 46, 10) - 1; // 1..1023 → 0..1022 kt
    int s_ns = getBits(msg, 56, 1);
    int v_ns = getBits(msg, 57, 10) - 1;
    if (v_ew < 0 || v_ns < 0) return;

    double ve = (s_ew ? -v_ew : v_ew);
    double vn = (s_ns ? -v_ns : v_ns);
    out.speed  = sqrt(ve*ve + vn*vn);
    double trk = atan2(ve, vn) * 180.0 / M_PI;
    if (trk < 0) trk += 360.0;
    out.course = trk;

    int vr_src = getBits(msg, 67, 1); (void)vr_src;
    int vr_sgn = getBits(msg, 68, 1);
    int vr     = getBits(msg, 69, 9) - 1; // 0..511 → -1..510
    if (vr >= 0) out.vert_rate = (vr_sgn ? -vr : vr) * 64; // fpm
  } else if (subtype==3 || subtype==4) {
    // Airspeed & Heading
    int hdg_stat = getBits(msg, 45, 1);
    int hdg_raw  = getBits(msg, 46, 10); // 0..1023, scale 360/1024
    if (hdg_stat) {
      out.course = (hdg_raw * 360.0) / 1024.0;
    }
    int as_tas  = getBits(msg, 56, 1); // 0=IAS,1=TAS — необязательно
    int as_val  = getBits(msg, 57, 10) - 1;
    if (as_val >= 0) out.speed = as_val;

    int vr_src = getBits(msg, 67, 1); (void)vr_src;
    int vr_sgn = getBits(msg, 68, 1);
    int vr     = getBits(msg, 69, 9) - 1;
    if (vr >= 0) out.vert_rate = (vr_sgn ? -v