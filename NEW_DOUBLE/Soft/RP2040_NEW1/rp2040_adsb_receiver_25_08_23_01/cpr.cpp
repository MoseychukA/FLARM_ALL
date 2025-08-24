#include "cpr.h"
#include <math.h>

static inline double cprNL(double lat){
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

bool cpr_decode_global(CPRContext &ctx, bool even, uint32_t rl, uint32_t rlo, uint32_t t_ms, double &oLat, double &oLon){
  if (even){ ctx.last_even_ms=t_ms; ctx.last_even_raw= (rl<<17) | (rlo & 0x1FFFF); }
  else { ctx.last_odd_ms=t_ms; ctx.last_odd_raw= (rl<<17) | (rlo & 0x1FFFF); }
  if (abs((int32_t)(ctx.last_even_ms - ctx.last_odd_ms)) > 10000) return false; // need within 10s window

  // Extract raw
  uint32_t evenLat = (ctx.last_even_raw>>17)&0x1FFFF;
  uint32_t evenLon = ctx.last_even_raw & 0x1FFFF;
  uint32_t oddLat  = (ctx.last_odd_raw>>17)&0x1FFFF;
  uint32_t oddLon  = ctx.last_odd_raw & 0x1FFFF;

  // CPR decode per standard
  const double NZ = 15.0;
  int j = (int)floor((59*oddLat - 60*evenLat)/131072.0 + 0.5);
  double rlat_e = 6.0 * (double)((int)floor((60*evenLat/131072.0) + j));
  double rlat_o = 6.0 * (double)((int)floor((59*oddLat/131072.0) + j));
  double lat = fmod(even ? rlat_e : rlat_o, 360.0);
  if (lat >= 270.0) lat -= 360.0;
  double nl = cprNL(lat);
  double ni = max(1.0, nl - (even ? 0 : 1));
  int m = (int)floor(((even ? evenLon : oddLon) * (nl- (even?0:1)))/131072.0 + 0.5);
  double lon = (360.0/ni) * (fmod((double)m, ni));
  if (lon > 180.0) lon -= 360.0;
  oLat = lat; oLon = lon; return true;
}

bool cpr_decode_local(double refLat, double refLon, bool even, uint32_t rl, uint32_t rlo, double &oLat, double &oLon){
  double dLat = even ? (360.0/60.0) : (360.0/59.0);
  int j = (int)floor(refLat/dLat) + (int)floor(0.5 + fmod(refLat, dLat)/dLat - (double)rl/131072.0);
  double lat = dLat*(j + (double)rl/131072.0);
  double nl = cprNL(lat);
  double dLon = (nl>0) ? 360.0 / (even ? nl : (nl-1)) : 360.0;
  int m = (int)floor(refLon/dLon) + (int)floor(0.5 + fmod(refLon, dLon)/dLon - (double)rlo/131072.0);
  double lon = dLon*(m + (double)rlo/131072.0);
  if (lon > 180.0) lon -= 360.0;
  if (lat > 90.0) lat -= 180.0;
  oLat = lat; oLon = lon; return true;
}
