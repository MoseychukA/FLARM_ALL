#include "adsb_cpr.h"
#include <math.h>
#include "config.h"

#define NZ 15
#define Dlat_even (360.0f / (4*NZ))
#define Dlat_odd  (360.0f / (4*NZ-1))
static inline float cpr_mod(float a, float b){ float r=fmodf(a,b); return r<0?r+b:r; }

int cpr_NL(float lat) {
  float a = fabsf(lat);
  if (a < 10.47047130f) return 59;
  if (a < 14.82817437f) return 58;
  if (a < 18.18626357f) return 57;
  if (a < 21.02939493f) return 56;
  if (a < 23.54504487f) return 55;
  if (a < 25.82924707f) return 54;
  if (a < 27.93898710f) return 53;
  if (a < 29.91135686f) return 52;
  if (a < 31.77209708f) return 51;
  if (a < 33.53993436f) return 50;
  if (a < 35.22899598f) return 49;
  if (a < 36.85025108f) return 48;
  if (a < 38.41241892f) return 47;
  if (a < 39.92256684f) return 46;
  if (a < 41.38651832f) return 45;
  if (a < 42.80914012f) return 44;
  if (a < 44.19454951f) return 43;
  if (a < 45.54626723f) return 42;
  if (a < 46.86733252f) return 41;
  if (a < 48.16039128f) return 40;
  if (a < 49.42776439f) return 39;
  if (a < 50.67150166f) return 38;
  if (a < 51.89342469f) return 37;
  if (a < 53.09516153f) return 36;
  if (a < 54.27817472f) return 35;
  if (a < 55.44378444f) return 34;
  if (a < 56.59318756f) return 33;
  if (a < 57.72747354f) return 32;
  if (a < 58.84763776f) return 31;
  if (a < 59.95459277f) return 30;
  if (a < 61.04917774f) return 29;
  if (a < 62.13216659f) return 28;
  if (a < 63.20427479f) return 27;
  if (a < 64.26616523f) return 26;
  if (a < 65.31845310f) return 25;
  if (a < 66.36171008f) return 24;
  if (a < 67.39646774f) return 23;
  if (a < 68.42322022f) return 22;
  if (a < 69.44242631f) return 21;
  if (a < 70.45451075f) return 20;
  if (a < 71.45986473f) return 19;
  if (a < 72.45884545f) return 18;
  if (a < 73.45177442f) return 17;
  if (a < 74.43893416f) return 16;
  if (a < 75.42056257f) return 15;
  if (a < 76.39684391f) return 14;
  if (a < 77.36789461f) return 13;
  if (a < 78.33374083f) return 12;
  if (a < 79.29428225f) return 11;
  if (a < 80.24923213f) return 10;
  if (a < 81.19801349f) return 9;
  if (a < 82.13956981f) return 8;
  if (a < 83.07199445f) return 7;
  if (a < 83.99173563f) return 6;
  if (a < 84.89166191f) return 5;
  if (a < 85.75541621f) return 4;
  if (a < 86.53536998f) return 3;
  if (a < 87.00000000f) return 2;
  return 1;
}

void cpr_reset(CPRState &s){ s = CPRState{}; }

void cpr_push_even(CPRState &s, uint32_t icao, uint32_t lat, uint32_t lon, uint32_t t_ms){
  if (s.icao!=icao) cpr_reset(s), s.icao=icao;
  s.lat_even=lat; s.lon_even=lon; s.t_even_ms=t_ms; s.has_even=true;
}
void cpr_push_odd (CPRState &s, uint32_t icao, uint32_t lat, uint32_t lon, uint32_t t_ms){
  if (s.icao!=icao) cpr_reset(s), s.icao=icao;
  s.lat_odd=lat; s.lon_odd=lon; s.t_odd_ms=t_ms; s.has_odd=true;
}

bool cpr_global(const CPRState &s, CPRResult &r) {
  if (!s.has_even || !s.has_odd) return false;
  uint32_t dt = (s.t_even_ms > s.t_odd_ms) ? (s.t_even_ms - s.t_odd_ms) : (s.t_odd_ms - s.t_even_ms);
  if (dt > CPR_PAIR_WINDOW_MS) return false;

  float j = floorf(((59.f * s.lat_even - 60.f * s.lat_odd) / 131072.f) + 0.5f);
  float rlat_even = Dlat_even * (cpr_mod(j, 60.f) + (float)s.lat_even / 131072.f);
  float rlat_odd  = Dlat_odd  * (cpr_mod(j, 59.f) + (float)s.lat_odd  / 131072.f);

  float lat = (s.t_even_ms > s.t_odd_ms) ? rlat_even : rlat_odd;
  int nl = cpr_NL(lat);
  float dlon = 360.f / (nl > 0 ? nl : 1);

  float m = floorf(((float)s.lon_even * (nl-1) - (float)s.lon_odd * nl) / 131072.f + 0.5f);
  float lon = dlon * (cpr_mod(m, (float)nl) + ((s.t_even_ms > s.t_odd_ms) ? s.lon_even : s.lon_odd) / 131072.f);

  if (lon > 180.f) lon -= 360.f;

  r.ok = true; r.lat = lat; r.lon = lon;
  return true;
}

bool cpr_local(uint32_t lat_cpr, uint32_t lon_cpr, bool fflag,
               float ref_lat, float ref_lon, CPRResult &r) {
  // fflag=0 even, =1 odd
  float dlat = fflag ? Dlat_odd : Dlat_even;
  int  j = floorf(ref_lat / dlat) + floorf(0.5f + (ref_lat - dlat * floorf(ref_lat / dlat)) / dlat);
  float lat = dlat * (j + (float)lat_cpr / 131072.f);
  if (lat >= 270.f) lat -= 360.f;
  int nl = cpr_NL(lat) - (fflag ? 1 : 0);
  float dlon = (nl > 1) ? 360.f / nl : 360.f;
  int m = floorf(ref_lon / dlon) + floorf(0.5f + (ref_lon - dlon * floorf(ref_lon / dlon)) / dlon);
  float lon = dlon * (m + (float)lon_cpr / 131072.f);
  if (lon > 180.f) lon -= 360.f;

  r.ok=true; r.lat=lat; r.lon=lon;
  return true;
}
