#include "adsb_decoder.h"
#include "adsb_crc.h"
#include "adsb_cpr.h"
#include "adsb_gillham.h"
#include "config.h"

#include <string.h>
#include <math.h>

// ---- Globals ----
volatile bool g_rawDump = true;

struct ICAOPair {
  cpr_pair_t pair;
};
static ICAOPair g_pairs[256]; // small cache by icao % 256

static inline uint32_t be24(const uint8_t* p){ return (p[0]<<16)|(p[1]<<8)|p[2]; }
static inline uint32_t be32(const uint8_t* p){ return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3]; }

static uint32_t get_bits(const uint8_t* b, int start, int len) {
  // start: bit index from MSB=0 across entire frame
  uint32_t v=0;
  for (int i=0;i<len;i++) {
    int bit = start+i;
    int byte = bit>>3;
    int off = 7-(bit&7);
    v = (v<<1) | ((b[byte]>>off)&1);
  }
  return v;
}

static float kts_to_kmh(float k){ return k*1.852f; }

// altitude decode (DF17, TC=9/10/11 field)
static int32_t decode_altitude_m(uint32_t altfield) {
  // 13 bits, bit Q at position 4 (from MSB): b(8..0) but in standard: use bitmaps.
  // Quick path: if Q=1, N= (AC13 >> 1) * 25 - 1000 ft
  bool q = (altfield & 0x10) != 0;
  if (q) {
    int N = (( (altfield & 0x0F) | ((altfield & 0x1FE0)>>1) ) ); // collapse bit hole
    int ft = N*25 - 1000;
    return (int32_t)lrintf(ft * 0.3048f);
  } else {
    int ft = gillham_ac13_to_alt_ft((uint16_t)altfield, false);
    if (ft == INT32_MIN) return INT32_MIN;
    return (int32_t)lrintf(ft * 0.3048f);
  }
}

// callsign (TC 1-4)
static void decode_callsign(const uint8_t* b, char out[16]) {
  static const char tbl[] = "#ABCDEFGHIJKLMNOPQRSTUVWXYZ#####_###############0123456789######";
  // ME[1..6] bytes; 8 6-bit chars
  uint64_t v = ((uint64_t)b[4]<<40)|((uint64_t)b[5]<<32)|((uint64_t)b[6]<<24)|((uint64_t)b[7]<<16)|((uint64_t)b[8]<<8)|b[9];
  for (int i=0;i<8;i++){
    int c = (v >> (42 - i*6)) & 0x3F;
    out[i] = (c< (int)sizeof(tbl)-1) ? tbl[c] : ' ';
  }
  out[8]=0;
  // copy into 16-char buffer padded with spaces
  memset(out+8,' ',7); out[15]=0;
}

// TC=19 velocity
static void decode_tc19(const uint8_t* b, uint16_t* speed_kmh, uint16_t* track_deg, int16_t* vr_mpm) {
  uint8_t subtype = (get_bits(b, 37, 3)); // ME[1] bits 5..7
  *vr_mpm = 0;
  if (subtype==1 || subtype==2){
    // Ground speed (vector), east/west, north/south
    int ew_dir = get_bits(b, 46,1);
    int ew_spd = get_bits(b, 47,10);
    int ns_dir = get_bits(b, 57,1);
    int ns_spd = get_bits(b, 58,10);
    if (ew_spd==0 || ns_spd==0) { *speed_kmh=0; *track_deg=0; return; }
    float ve = (ew_spd-1) * (ew_dir? -1.0f : 1.0f);
    float vn = (ns_spd-1) * (ns_dir? -1.0f : 1.0f);
    float v = sqrtf(ve*ve + vn*vn);
    float trk = atan2f(ve, vn) * 180.0f/M_PI; if (trk<0) trk+=360.0f;
    *speed_kmh = (uint16_t)lrintf(kts_to_kmh(v));
    *track_deg = (uint16_t)lrintf(trk);
    int vr_sgn = get_bits(b, 68,1) ? -1 : 1;
    int vr = get_bits(b, 69,9) - 1;
    *vr_mpm = (int16_t)(vr_sgn * vr * 64 * 0.3048f * 3.28084f); // (ft/min)≈64fpm steps => placeholder
  } else if (subtype==3 || subtype==4){
    // Heading + ground/true speed
    int hdg_stat = get_bits(b, 46,1);
    int hdg = get_bits(b, 47,10);
    int spd_typ = get_bits(b, 57,1); // 0=GS,1=TAS
    int spd = get_bits(b, 58,10);
    float track = (hdg/1024.0f)*360.0f;
    *track_deg = (uint16_t)lrintf(track);
    *speed_kmh = (uint16_t)lrintf(kts_to_kmh(spd-1));
    int vr_sgn = get_bits(b, 68,1) ? -1 : 1;
    int vr = get_bits(b, 69,9) - 1;
    *vr_mpm = (int16_t)(vr_sgn * vr); // leave in raw steps, adjust upstream if needed
  } else {
    *speed_kmh=0; *track_deg=0; *vr_mpm=0;
  }
}

static void format_squawk(uint16_t code, char out[8]) {
  // 13-bit squawk Gillham-like -> 4 octal
  out[0] = ((code>>9)&7)+'0';
  out[1] = ((code>>6)&7)+'0';
  out[2] = ((code>>3)&7)+'0';
  out[3] = ((code>>0)&7)+'0';
  out[4]=0;
}

void adsb_init(){}

// Global state for local CPR reference
volatile float g_ref_lat = 55.751244f;
volatile float g_ref_lon = 37.618423f;

// Profile/dynamic threshold stubs
void set_corr_profile(bool high_emi) { (void)high_emi; }

// RAW dump
void dump_raw(const adsb_packet_t* pkt) {
  if (!g_rawDump) return;
  Serial.printf("[RAW][CH%d][%u bits] ", pkt->chan, pkt->nbits);
  for (int i=0;i<(pkt->nbits+7)/8;i++) Serial.printf("%02X", pkt->data[i]);
  Serial.println();
}

// packet queue interface (implemented in main.cpp)
extern bool enqueue_packet_from_core1(const adsb_packet_t& pkt);

// High-level decoder (DF17/DF18 and partial DF20/21 BDS)
bool adsb_decode(const adsb_packet_t* pkt, adsb_decoded_t* out) {
  if (!pkt || !out) return false;
  // Optionally dump raw before CRC
  dump_raw(pkt);

  // CRC and single-bit correction
  uint8_t data[MAX_BYTES]; memcpy(data, pkt->data, sizeof(data));
  bool ok = adsb_parity_ok(data, pkt->nbits);
  bool corrected = false;
  if (!ok) {
    // Try single-bit correction
    corrected = adsb_fix_single_bit(data, pkt->nbits);
    ok = corrected;
  }
  if (!ok) return false;

  // Basic header
  uint8_t df = (data[0] >> 3) & 0x1F;
  uint8_t ca = data[0] & 0x07;
  (void)ca;

  uint32_t icao = be24(&data[1]); // for DF17/18 after CRC removal this may differ; proceed as common SDR method

  memset(out, 0, sizeof(*out));
  out->addr = icao;
  out->seen_time = millis();
  out->type = 0;

  if (df==17 || df==18) {
    uint8_t tc = (data[4] >> 3) & 0x1F;

    if (tc>=1 && tc<=4) { // callsign
      decode_callsign(data, out->flight);
    }
    else if (tc==19) { // velocity
      uint16_t spd, hdg; int16_t vr;
      decode_tc19(data, &spd, &hdg, &vr);
      out->speed = spd;
      out->track = hdg;
      out->vert_rate = vr;
    }
    else if (tc==9 || tc==10 || tc==11) { // airborne position
      bool odd = (data[6] & 0x04) != 0; // time flag
      uint32_t altfield = ((data[5]&0x07)<<8) | data[6]; // rough, will re-extract precise bits
      // Proper altitude field (13 bits in ME[3..5]): bits 19..31 overall ME
      uint32_t re_alt = ((get_bits(data, 40, 7) << 6) | get_bits(data, 48,6)); // AC13 mixed; Q @ bit 51
      int32_t alt_m = decode_altitude_m(re_alt);
      if (alt_m != INT32_MIN) out->altitude = alt_m;

      uint32_t cprLat = (get_bits(data, 54, 17));
      uint32_t cprLon = (get_bits(data, 71, 17));

      // pair store
      ICAOPair &pair = g_pairs[out->addr & 0xFF];
      if (odd) { pair.pair.have_odd=true;  pair.pair.t_odd_ms=millis(); pair.pair.lat_odd=cprLat; pair.pair.lon_odd=cprLon; }
      else     { pair.pair.have_even=true; pair.pair.t_even_ms=millis(); pair.pair.lat_even=cprLat; pair.pair.lon_even=cprLon; }
      pair.pair.icao = out->addr;

      // Try global (if both within 10s)
      if (pair.pair.have_even && pair.pair.have_odd &&
          (abs((int)pair.pair.t_even_ms - (int)pair.pair.t_odd_ms) <= 10000)) {
        float lat, lon;
        if (cpr_global_decode(&pair.pair, &lat, &lon)) { out->lat=lat; out->lon=lon; }
      } else {
        // Fallback local decode
        cpr_local_decode(g_ref_lat, g_ref_lon, cprLat, cprLon, odd, &out->lat, &out->lon);
      }
    }
    else if (tc==5 || tc==6 || tc==7 || tc==8) { // surface pos: can decode similarly if needed
      // Not fully implemented here
    }

    // Squawk (DF17 extended?) Typically DF5/DF21 hold identity; DF17 has ID only in BDS 1,0 not included here
    // We'll leave squawk empty unless DF 5/21 appear (handled below).
  }
  else if (df==5 || df==21) {
    // Identity reply: squawk code in 13 bits
    uint16_t id = (uint16_t)get_bits(data, 19, 13);
    char sq[8]={0}; format_squawk(id, sq);
    strncpy(out->flight, sq, sizeof(out->flight));
  }
  else if (df==20 || df==21) {
    // BDS 6,0/6,1 skeleton (baro-geo diff, VS) – requires IC bit fields per transponder implementation
    // TODO: parse specific fields when ME field indicates 6,0/6,1
  }

  // default types
  if (out->type==0) out->type = 9;

  // Coherence: when only velocity seen, we still output partial
  return true;
}