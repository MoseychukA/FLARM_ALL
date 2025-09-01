#include "adsb_decoder.h"
#include <string.h>
#include <math.h>

static const uint32_t crc24_poly = 0xFFF409; // Mode-S polynomial representation trick

uint32_t crc32(const uint8_t* data, size_t len) {
  uint32_t crc = 0xFFFFFFFF;
  for (size_t i=0;i<len;i++) {
    crc ^= data[i];
    for (int j=0;j<8;j++) crc = (crc>>1) ^ (0xEDB88320 & (-(int)(crc & 1)));
  }
  return ~crc;
}

static uint32_t bitrev(uint32_t x, int bits) {
  uint32_t r=0; for (int i=0;i<bits;i++){ r=(r<<1)|((x>>i)&1);} return r;
}

uint32_t crc24_modes(const uint8_t *msg, int bits) {
  // Compute 24-bit CRC over first (bits-24) bits; returns remainder
  // Implementation adapted to Mode S polynomial
  int bytes = (bits+7)/8;
  uint64_t data = 0; // process bitwise, but for speed we do simple method
  uint32_t rem = 0; // remainder
  for (int i=0;i<bits-24;i++) {
    int byte = i>>3; int bit = 7-(i&7);
    int b = (msg[byte]>>bit)&1;
    int top = ((rem>>23)&1) ^ b;
    rem = ((rem<<1)&0xFFFFFF) ^ (top?0xFFF409:0);
  }
  // Return remainder XOR last 24 bits to get parity check; but we only use for parity calculation
  return rem;
}

static uint32_t modes_checksum(const uint8_t *msg, int bits) {
  // Compute CRC parity over bits-24 and xor with last 24 to get syndrome
  uint32_t rem = crc24_modes(msg, bits);
  uint32_t last = ((uint32_t)msg[(bits/8)-3]<<16)|((uint32_t)msg[(bits/8)-2]<<8)|msg[(bits/8)-1];
  return rem ^ last;
}

CrcStatus check_and_fix_crc(RawMessage &rm) {
  uint32_t syn = modes_checksum(rm.payload, rm.bits);
  if (syn == 0) return CRC_OK;
  // single-bit fix: try flipping
  int B = rm.bits;
  for (int i=0;i<B;i++) {
    int byte = i>>3; int bit = 7-(i&7);
    rm.payload[byte] ^= (1<<bit);
    if (modes_checksum(rm.payload, B) == 0) return CRC_FIXED1;
    rm.payload[byte] ^= (1<<bit);
  }
  return CRC_FAIL;
}

static int decode_df(const RawMessage &rm) { return rm.payload[0] >> 3; }
static int get_tc(const RawMessage &rm) { return (rm.payload[4] >> 3); }

static void hex_icao(const RawMessage &rm, uint32_t &icao) {
  // For DF17/18, ICAO is in bytes 1..3 (after DF and CA), parity xored
  icao = ((uint32_t)rm.payload[1] << 16) | ((uint32_t)rm.payload[2] << 8) | rm.payload[3];
}

static int decode_callsign(const uint8_t *me, char *out) {
  // 8 6-bit chars from ME[0..5]
  static const char set[] = "#ABCDEFGHIJKLMNOPQRSTUVWXYZ#####_###############0123456789######";
  uint64_t v = 0; for (int i=0;i<6;i++) v = (v<<8) | me[i];
  for (int i=7;i>=0;i--) { int c = v & 0x3F; out[i] = set[c]; v >>= 6; }
  out[8] = 0; for (int i=0;i<8;i++) if (out[i]=='#'||out[i]=='_') out[i]=' ';
  return 1;
}

static int alt_from_ac13(uint16_t ac13, bool q) {
  if (q) {
    int n = ((ac13 & 0x0FE0) >> 1) | (ac13 & 0x000F); // remove M&Q bits
    int alt_ft = n * 25 - 1000;
    return (int)round(alt_ft * 0.3048);
  } else {
    // Gillham decode
    return gillham2alt(ac13, false);
  }
}

// CPR helpers
static float cprNL(float lat) {
  if (lat == 0) return 59;
  float a = fabs(lat);
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
  else return 1;
}

bool cpr_decode_global(const uint32_t even_lat_cpr, const uint32_t even_lon_cpr,
                       const uint32_t odd_lat_cpr,  const uint32_t odd_lon_cpr,
                       bool surface, float &out_lat, float &out_lon) {
  const int NZ = surface ? 15 : 60;
  const float Dlat0 = 360.0f / (4*NZ);
  const float Dlat1 = 360.0f / (4*NZ - 1);
  float lat0 = Dlat0 * (even_lat_cpr / 131072.0f);
  float lat1 = Dlat1 * (odd_lat_cpr  / 131072.0f);
  int j = floor(59*lat0 - 60*lat1 + 0.5f);
  float rlat0 = Dlat0 * (j % 60) + lat0;
  float rlat1 = Dlat1 * (j % 59) + lat1;
  float rlat = (rlat0 + rlat1)/2.0f; // choose based on time closeness; simplified
  float nl = cprNL(rlat);
  if (nl < 1) return false;
  float Dlon = 360.0f / nl;
  int m = floor((even_lon_cpr * (nl-1) - odd_lon_cpr * nl) / 131072.0f + 0.5f);
  float rlon = Dlon * (m % (int)nl) + (even_lon_cpr / 131072.0f) * Dlon;
  out_lat = rlat; out_lon = rlon; return true;
}

bool cpr_decode_local(uint32_t lat_cpr, uint32_t lon_cpr, bool surface,
                      float ref_lat, float ref_lon, float &out_lat, float &out_lon) {
  int NZ = surface ? 15 : 60;
  float Dlat = 360.0f / (4*NZ);
  int j = floor(ref_lat / Dlat) + floor(0.5f + fmod(ref_lat, Dlat)/Dlat - lat_cpr/131072.0f);
  out_lat = Dlat * (j + lat_cpr/131072.0f);
  float nl = cprNL(out_lat); if (nl < 1) nl = 1;
  float Dlon = 360.0f / nl;
  int m = floor(ref_lon / Dlon) + floor(0.5f + fmod(ref_lon, Dlon)/Dlon - lon_cpr/131072.0f);
  out_lon = Dlon * (m + lon_cpr/131072.0f);
  return true;
}

int gillham2alt(uint16_t ac13, bool qbit) {
  // Full Gillham (AC13) decoder based on standard mapping
  // ac13: 13-bit code ABCD#EFGHIJKL (with M/Q bits), but here we assume raw 13-bit AC field
  // We'll map Gray-like Gillham to altitude in feet then meters
  // Decode gray-coded 100-ft and 500-ft digits. This is a compact implementation; for full coverage use table.

  // Bits positions: D11 A, D10 B, D9 C, D8 D, D7 , D6 E, D5 F, D4 G, D3 H, D2 I, D1 J, D0 K (approx)
  // Practical approach: use lookup table from ac13 -> feet for Q=0 per documented pairs
  static const int16_t table[8192] = {0}; // placeholder; full table would be large
  // For demo purposes return NAN mapping as invalid -> -99999 if not supported
  return -99999;
}

static void decode_velocity_tc19(const uint8_t *me, DecodedFrame &df) {
  int subtype = (me[0] >> 0) & 0x7;
  if (subtype == 1 || subtype == 2) {
    // Ground speed north/east
    int vew = ((me[1] & 0x7F) << 3) | (me[2] >> 5);
    int vns = ((me[2] & 0x1F) << 6) | (me[3] >> 2);
    bool sgn_ew = me[1] & 0x80; bool sgn_ns = me[3] & 0x02;
    int ew = vew - 1; int ns = vns - 1;
    float vx = (sgn_ew ? -ew : ew);
    float vy = (sgn_ns ? -ns : ns);
    float spd = sqrtf(vx*vx + vy*vy);
    df.speed_kmh = (int)round(spd * 1.852f);
    float trk = atan2f(vx, vy) * 180.0f / PI; if (trk<0) trk+=360.0f;
    df.track_deg = (int)round(trk);
  } else if (subtype == 3 || subtype == 4) {
    // Airspeed and heading
    int hdg_enc = ((me[1] & 0x7F) << 3) | (me[2] >> 5);
    bool hdg_valid = me[1] & 0x80;
    float hdg = hdg_valid ? (hdg_enc * 360.0f / 1024.0f) : NAN;
    int as = ((me[2] & 0x1F) << 6) | (me[3] >> 2);
    int airspeed = as - 1;
    df.speed_kmh = (int)round(airspeed * 1.852f);
    df.track_deg = (int)round(hdg);
  }
}

static void decode_bds60_61(const uint8_t *me, DecodedFrame &df) {
  // Minimal parser: vertical rate and baro-geo difference if present
  // Many DF20/21 comm-B; here we handle ADS-B ME=19 subtypes won't include BDS6.x usually; kept for completeness
}

bool decode_adsb_frame(const RawMessage &rm, DecodedFrame &df, const struct Config &cfg) {
  memset(&df, 0, sizeof(df)); df.lat=NAN; df.lon=NAN; strcpy(df.callsign, "");
  df.seen_time_ms = millis();

  int dfnum = decode_df(rm);
  if (dfnum != 17 && dfnum != 18 && dfnum != 20 && dfnum != 21) return false;
  hex_icao(rm, df.icao);

  // Identify type code
  int tc = get_tc(rm);
  const uint8_t *me = &rm.payload[5]; // ME field starts at byte 5

  if (tc >= 1 && tc <= 4) {
    // Callsign
    decode_callsign(me, df.callsign);
  } else if (tc >= 9 && tc <= 18) {
    // Airborne position (baro)
    bool q = me[1] & 0x10; // Q-bit in AC13
    uint16_t ac13 = ((me[0] & 0x0F)<<9) | (me[1]<<1) | ((me[2]>>7)&1);
    int alt_m = alt_from_ac13(ac13, q);
    df.altitude_m_baro = alt_m;
    // CPR fields
    uint32_t latc = ((me[2] & 0x7F) << 10) | (me[3] << 2) | (me[4] >> 6);
    uint32_t lonc = ((me[4] & 0x3F) << 12) | (me[5] << 4) | (me[6] >> 4);
    bool odd = me[6] & 0x04;
    // Save/restore with global CPR across messages is outside this function scope
  } else if (tc == 19) {
    decode_velocity_tc19(me, df);
  } else if (tc >= 5 && tc <= 8) {
    // Surface position
  }

  // Squawk from DF5/DF21 not fully handled here
  return true;
}
