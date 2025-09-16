/*
  ADSB_Receiver_RP2040_Final.ino
  Полный ADS-B parser + state cache + CPR global + local fallback + caching of last TC19/pos
  Serial (USB) 115200 - отладка
  Serial2 (TX=4,RX=5) 921600 - на ESP32S3
*/

#include <Arduino.h>
#include <math.h>
#include <map>
#include <cstring>
#include "data_structures.h" // содержит PFBQueue template и ваши типы

// Serial config
#define USB_BAUD 115200
#define UART2_BAUD 921600
#define UART2_TX_PIN 4
#define UART2_RX_PIN 5

/* ToDUMP1090 structure (packed) */
struct ToDUMP1090 {
  uint32_t addr;
  uint16_t squawk;
  char     flight[16];
  int32_t  altitude;
  int32_t  speed;
  int32_t  track;
  int32_t  vert_rate;
  float    lat;
  float    lon;
  int32_t  seen_time;
  uint8_t  endOfPacket[3];
} __attribute__((packed));

extern PFBQueue<Decoded1090Packet> decoded_1090_packet_out_queue;

/* --------------------------
   Aircraft state (cache)
   -------------------------- */
struct AircraftState {
  uint32_t icao = 0;
  char callsign[16] = {0};
  uint16_t squawk = 0;
  int32_t altitude = -1; // ft

  // last known authoritative position
  double lat = NAN;
  double lon = NAN;
  bool pos_valid = false;
  uint64_t pos_time = 0;

  // CPR parts (even / odd) with timestamps
  int32_t cpr_even_lat = -1;
  int32_t cpr_even_lon = -1;
  uint64_t cpr_even_time = 0;
  int32_t cpr_odd_lat = -1;
  int32_t cpr_odd_lon = -1;
  uint64_t cpr_odd_time = 0;

  // last velocity (TC19)
  int32_t last_speed = 0;       // knots
  int32_t last_track = 0;       // degrees
  int32_t last_vert_rate = 0;   // ft/min
  uint64_t vel_time = 0;

  // simple smoothing state
  float filt_speed = 0.0f;
  float filt_track = 0.0f;
  bool has_filtered = false;

  uint64_t seen = 0;
};

#define MAX_AIRCRAFT 256
static AircraftState acList[MAX_AIRCRAFT];

static AircraftState* getAircraft(uint32_t icao) {
  for (int i=0;i<MAX_AIRCRAFT;i++) if (acList[i].icao == icao) return &acList[i];
  for (int i=0;i<MAX_AIRCRAFT;i++) if (acList[i].icao == 0) {
    memset(&acList[i], 0, sizeof(AircraftState));
    acList[i].icao = icao;
    acList[i].callsign[0] = '\0';
    acList[i].pos_valid = false;
    return &acList[i];
  }
  // fallback to index 0 if full
  return &acList[0];
}

/* --------------------------
   Bit operations (getbits)
   input msg[] - bytes (MSB first)
   firstbit: 1..N
   -------------------------- */
static inline uint32_t getbits(const uint8_t *msg, int firstbit, int nbits) {
  int bitIndex = firstbit - 1;
  int byteIndex = bitIndex / 8;
  int bitInByte = 7 - (bitIndex % 8);
  uint32_t val = 0;
  for (int i=0;i<nbits;i++) {
    uint8_t b = (msg[byteIndex] >> bitInByte) & 1;
    val = (val << 1) | b;
    bitInByte--;
    if (bitInByte < 0) { bitInByte = 7; byteIndex++; }
  }
  return val;
}

/* --------------------------
   Callsign decode (6-bit) per ADS-B
   ME bits 41..88 -> 8 * 6 bits
   -------------------------- */
static const char charset[64] = {
  '@','A','B','C','D','E','F','G','H','I',
  'J','K','L','M','N','O','P','Q','R','S',
  'T','U','V','W','X','Y','Z','[','\\',']',
  '^','_',' ','0','1','2','3','4','5','6',
  '7','8','9',':',';','<','=','>','?',' ',
  ' ',' ',' ',' ',' ',' ',' ',' '
};

static inline void decodeCallsignFromMsg(const uint8_t *msg, char *dst, size_t dstlen) {
  for (int i=0;i<8 && i+1 < (int)dstlen;i++) {
    uint8_t c = getbits(msg, 41 + i*6, 6) & 0x3F;
    char ch = charset[c];
    if (ch == '@') ch = ' ';
    if (ch >= '[' && ch <= '?') ch = ' ';
    dst[i] = ch;
  }
  dst[8] = '\0';
}

/* --------------------------
   Altitude decode (barometric), using Q bit method.
   Returns altitude in feet or -1 if unknown.
   -------------------------- */
static inline int decodeBaroAltitudeFromMsg(const uint8_t *msg) {
  // Bits per spec: bits 41-52 cover altitude & Q formatting.
  // Implementation: take 7 bits (41..47) and 4 bits (49..52), skip Q at 48.
  uint32_t upper7 = getbits(msg, 41, 7); // bits 41..47
  uint32_t q = getbits(msg, 48, 1);      // bit 48
  uint32_t lower4 = getbits(msg, 49, 4); // bits 49..52
  uint32_t n = (upper7 << 4) | lower4;   // 11-bit-ish code for Q=1
  if (q == 1) {
    // per spec: altitude = n*25 - 1000
    int alt = (int)n * 25 - 1000;
    return alt;
  } else {
    // Q==0 => Gillham-coded alt (complex) -> not implemented fully here
    return -1;
  }
}

/* --------------------------
   Decode Mode A (DF=5) 13-bit -> octal squawk
   -------------------------- */
static inline uint16_t decodeModeA(uint32_t code13) {
  // map 13-bit to 4 octal digits A B C D
  int A = ((code13 >> 11) & 1)*4 + ((code13 >> 10) & 1)*2 + ((code13 >> 9) & 1);
  int B = ((code13 >> 8) & 1)*4 + ((code13 >> 7) & 1)*2 + ((code13 >> 6) & 1);
  int C = ((code13 >> 5) & 1)*4 + ((code13 >> 4) & 1)*2 + ((code13 >> 3) & 1);
  int D = ((code13 >> 2) & 1)*4 + ((code13 >> 1) & 1)*2 + ((code13 >> 0) & 1);
  return (uint16_t)(A*1000 + B*100 + C*10 + D);
}

/* --------------------------
   CPR helpers (dump1090-compatible)
   -------------------------- */
static inline double cprMod(double a, double b) {
  double r = fmod(a,b);
  if (r < 0) r += b;
  return r;
}

static inline int cprNL(double lat) {
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

/* Global CPR decode (even + odd), raw values are 17-bit ints (0..131071) */
static bool cprDecodeGlobal(int even_lat_raw, int even_lon_raw,
                            int odd_lat_raw,  int odd_lon_raw,
                            double &outLat, double &outLon)
{
  double lat_even = (double)even_lat_raw / 131072.0;
  double lat_odd  = (double)odd_lat_raw  / 131072.0;
  double lon_even = (double)even_lon_raw / 131072.0;
  double lon_odd  = (double)odd_lon_raw  / 131072.0;

  int j = floor((59 * lat_even - 60 * lat_odd) + 0.5);

  double rlat_even = (360.0 / 60.0) * (cprMod(j, 60) + lat_even);
  double rlat_odd  = (360.0 / 59.0) * (cprMod(j, 59) + lat_odd);

  if (rlat_even >= 270.0) rlat_even -= 360.0;
  if (rlat_odd >= 270.0) rlat_odd -= 360.0;

  // choose odd for final latitude (dump1090 convention)
  outLat = rlat_odd;
  if (outLat > 90.0 || outLat < -90.0) return false;

  int nl = cprNL(outLat);
  if (nl <= 0) return false;
  int ni = nl - 1;
  if (ni <= 0) ni = 1;

  double dlon = 360.0 / ni;
  int m = floor((lon_even * (nl - 1) - lon_odd * nl) / 131072.0 + 0.5);
  outLon = dlon * (cprMod(m, ni) + lon_even);

  if (outLon >= 180.0) outLon -= 360.0;
  return true;
}

/* Local CPR decode fallback: use last known ref position */
static bool cprDecodeLocal(int raw_lat, int raw_lon, bool isOdd, double refLat, double refLon, double &outLat, double &outLon) {
  double latf = (double)raw_lat / 131072.0;
  double lonf = (double)raw_lon / 131072.0;

  double dLat = isOdd ? (360.0/59.0) : (360.0/60.0);
  int j = floor(refLat / dLat) - floor((refLat / dLat - latf) + 0.5);
  outLat = dLat * (j + latf);

  int nl = cprNL(refLat);
  if (nl < 1) nl = 1;
  int ni = isOdd ? (nl - 1) : nl;
  if (ni <= 0) ni = 1;
  double dLon = 360.0 / ni;
  int m = floor(refLon / dLon) - floor((refLon / dLon - lonf) + 0.5);
  outLon = dLon * (m + lonf);

  if (outLon >= 180.0) outLon -= 360.0;
  if (outLat > 90.0 || outLat < -90.0) return false;
  return true;
}

/* --------------------------
   Parse DF=17 (112-bit) messages from raw bytes
   We'll extract:
    - ICAO: bits 9..32
    - Type Code: bits 33..37
    - callsign for TC 1..4 (bits 41..88 8x6)
    - For position TC: odd flag bit 54, lat 55..71 (17 bits), lon 72..88 (17 bits)
    - altitude via Q-bit at bit 48 and surrounding bits
    - TC=19 velocity fields (subtype etc.)
   This parse works with msg[14] array (msb-first).
   -------------------------- */

struct RawFields {
  uint32_t icao;
  uint8_t tc;
  bool valid;
  // callsign
  char callsign[16];
  // position
  bool hasPosition;
  bool posOdd;
  int cprLat;
  int cprLon;
  int altitude;
  // velocities
  bool hasVelocity;
  int v_speed;
  int v_track;
  int v_vrate;
  // squawk from TC28
  uint16_t squawk;
};

static void initRawFields(RawFields &rf) { memset(&rf,0,sizeof(rf)); rf.valid=false; rf.hasPosition=false; rf.hasVelocity=false; rf.altitude=-1; rf.callsign[0]=0; }

static bool parseDF17Bytes(const uint8_t *msg, int len, RawFields &out) {
  if (len < 14) return false;
  initRawFields(out);
  out.valid = true;
  out.icao = (msg[1]<<16) | (msg[2]<<8) | msg[3];
  out.tc = (uint8_t)getbits(msg,33,5);

  // callsign TC 1..4
  if (out.tc >=1 && out.tc <=4) {
    decodeCallsignFromMsg(msg, out.callsign, sizeof(out.callsign));
  }

  // position TC
  if ((out.tc >=5 && out.tc <=8) || (out.tc >=9 && out.tc <=18) || (out.tc >=20 && out.tc <=22)) {
    out.hasPosition = true;
    out.posOdd = getbits(msg,54,1);
    out.cprLat = (int)getbits(msg,55,17);
    out.cprLon = (int)getbits(msg,72,17);
    if (out.tc >= 9 && out.tc <= 18) out.altitude = decodeBaroAltitudeFromMsg(msg);
    else if (out.tc >= 20 && out.tc <= 22) out.altitude = decodeBaroAltitudeFromMsg(msg); // approximate
  }

  // TC19 velocities
  if (out.tc == 19) {
    out.hasVelocity = true;
    int subtype = (int)getbits(msg,38,3);
    if (subtype == 1 || subtype == 2) {
      // ground speed fields
      int ewSign = getbits(msg,46,1);
      int ewVel = getbits(msg,47,10);
      int nsSign = getbits(msg,57,1);
      int nsVel = getbits(msg,58,10);
      int ve = ewVel - 1; if (ve < 0) ve = 0;
      int vn = nsVel - 1; if (vn < 0) vn = 0;
      double vx = (ewSign ? -ve : ve);
      double vy = (nsSign ? -vn : vn);
      double spd = sqrt(vx*vx + vy*vy);
      double heading = atan2(vx, vy) * 180.0 / M_PI;
      if (heading < 0) heading += 360.0;
      out.v_speed = (int)round(spd);
      out.v_track = (int)round(heading);
    } else if (subtype == 3 || subtype == 4) {
      int airspeed = (int)getbits(msg,46,10) - 1; if (airspeed < 0) airspeed = 0;
      out.v_speed = airspeed;
      out.v_track = (int)getbits(msg,57,10);
    }
    int vr_sign = (int)getbits(msg,68,1);
    int vr_mag = (int)getbits(msg,69,9) - 1; if (vr_mag < 0) vr_mag = 0;
    out.v_vrate = (vr_sign ? -1 : 1) * (vr_mag * 64);
  }

  // TC28 status => emergency -> map to squawk codes approximately
  if (out.tc == 28) {
    int emerg = (int)getbits(msg,38,3);
    if (emerg == 1) out.squawk = 7700;
    else if (emerg == 2) out.squawk = 7600;
    else if (emerg == 3) out.squawk = 7500;
  }

  return true;
}

/* --------------------------
   parse DF=5 (Mode A) 7-byte messages => squawk
   -------------------------- */
static bool parseDF5Bytes(const uint8_t *msg, int len, uint32_t &icao_out, uint16_t &squawk_out) {
  if (len < 7) return false;
  icao_out = (msg[1]<<16) | (msg[2]<<8) | msg[3];
  uint32_t code13 = getbits(msg,20,13);
  squawk_out = decodeModeA(code13);
  return true;
}

/* --------------------------
   Update AircraftState with RawFields (from DF=17)
   Also maintains CPR even/odd and velocity caches.
   -------------------------- */
static const float SPEED_ALPHA = 0.4f; // smoothing factor for speed/track (0..1) higher = faster response

static void updateStateFromRaw(const RawFields &rf) {
  if (!rf.valid) return;
  AircraftState *ac = getAircraft(rf.icao);
  ac->seen = millis();

  // callsign
  if (rf.tc >=1 && rf.tc <=4 && rf.callsign[0] != 0) {
    strncpy(ac->callsign, rf.callsign, sizeof(ac->callsign)-1);
    ac->callsign[sizeof(ac->callsign)-1] = 0;
  }

  // position
  if (rf.hasPosition) {
    // store even/odd
    if (rf.posOdd) {
      ac->cpr_odd_lat = rf.cprLat;
      ac->cpr_odd_lon = rf.cprLon;
      ac->cpr_odd_time = millis();
    } else {
      ac->cpr_even_lat = rf.cprLat;
      ac->cpr_even_lon = rf.cprLon;
      ac->cpr_even_time = millis();
    }
    // update altitude if present
    if (rf.altitude != -1) ac->altitude = rf.altitude;

    // try global decode if both exist and timely
    if (ac->cpr_even_lat >= 0 && ac->cpr_odd_lat >= 0) {
      if (abs((long)(ac->cpr_even_time - ac->cpr_odd_time)) < 10000) {
        double lat, lon;
        if (cprDecodeGlobal(ac->cpr_even_lat, ac->cpr_even_lon,
                            ac->cpr_odd_lat, ac->cpr_odd_lon,
                            lat, lon)) {
          ac->lat = lat; ac->lon = lon; ac->pos_valid = true;
          ac->pos_time = millis();
        }
      }
    } else {
      // local fallback using last known pos as reference
      double refLat = ac->pos_valid ? ac->lat : 0.0;
      double refLon = ac->pos_valid ? ac->lon : 0.0;
      double latf, lonf;
      bool ok = cprDecodeLocal(rf.cprLat, rf.cprLon, rf.posOdd, refLat, refLon, latf, lonf);
      if (ok) {
        ac->lat = latf; ac->lon = lonf; ac->pos_valid = true; ac->pos_time = millis();
      }
    }
  }

  // velocity
  if (rf.hasVelocity) {
    ac->last_speed = rf.v_speed;
    ac->last_track = rf.v_track;
    ac->last_vert_rate = rf.v_vrate;
    ac->vel_time = millis();

    // smoothing:
    if (!ac->has_filtered) {
      ac->filt_speed = (float)ac->last_speed;
      ac->filt_track = (float)ac->last_track;
      ac->has_filtered = true;
    } else {
      ac->filt_speed = (1.0f - SPEED_ALPHA) * ac->filt_speed + SPEED_ALPHA * (float)ac->last_speed;
      // track smoothing is tricky around 0/360 boundary -> handle via vector interpolation
      float prev_heading = ac->filt_track * (M_PI/180.0f);
      float new_heading = ac->last_track * (M_PI/180.0f);
      float x = cos(prev_heading)*(1.0f - SPEED_ALPHA) + cos(new_heading)*SPEED_ALPHA;
      float y = sin(prev_heading)*(1.0f - SPEED_ALPHA) + sin(new_heading)*SPEED_ALPHA;
      float ang = atan2(y,x) * 180.0f / M_PI;
      if (ang < 0) ang += 360.0f;
      ac->filt_track = ang;
    }
  }

  // TC28 squawk
  if (rf.squawk) {
    ac->squawk = rf.squawk;
  }
}

/* --------------------------
   Update AircraftState from DF=5 squawk message
   -------------------------- */
static void updateStateFromDF5(uint32_t icao, uint16_t squawk) {
  AircraftState *ac = getAircraft(icao);
  ac->squawk = squawk;
  ac->seen = millis();
}

/* --------------------------
   Emit state (print + send Serial2)
   Uses the last cached values (pos from CPR pair or fallback; smoothed speed/track)
   -------------------------- */
static void emitAircraft(const AircraftState &ac) {
  Serial.printf("ICAO=%06X FLT=%-8s SQ=%04o ALT=%d SPD=%.0f TRK=%.0f VR=%d LAT=%f LON=%f AGE=%lums\n",
                ac.icao,
                ac.callsign[0] ? ac.callsign : "----",
                ac.squawk,
                (int)ac.altitude,
                ac.has_filtered ? ac.filt_speed : (float)ac.last_speed,
                ac.has_filtered ? ac.filt_track : (float)ac.last_track,
                ac.last_vert_rate,
                ac.pos_valid ? ac.lat : NAN,
                ac.pos_valid ? ac.lon : NAN,
                (unsigned long)(millis() - ac.seen)
  );

  ToDUMP1090 t;
  memset(&t,0,sizeof(t));
  t.addr = ac.icao;
  t.squawk = ac.squawk;
  strncpy(t.flight, ac.callsign, sizeof(t.flight)-1);
  t.altitude = ac.altitude;
  t.speed = (int32_t)(ac.has_filtered ? roundf(ac.filt_speed) : ac.last_speed);
  t.track = (int32_t)(ac.has_filtered ? roundf(ac.filt_track) : ac.last_track);
  t.vert_rate = ac.last_vert_rate;
  t.lat = ac.pos_valid ? (float)ac.lat : 0.0f;
  t.lon = ac.pos_valid ? (float)ac.lon : 0.0f;
  t.seen_time = (int32_t)(millis()/1000);
  t.endOfPacket[0] = 0xFF; t.endOfPacket[1] = 0xFF; t.endOfPacket[2] = 0xFF;

  Serial2.write((const uint8_t*)&t, sizeof(t));
}

/* --------------------------
   Hook: pop raw bytes (14 or 7) from decoded_1090_packet_out_queue
   You must adapt this function to how Decoded1090Packet is stored in your queue.
   Example commented inside shows using DumpPacketBuffer to extract raw words.
   For now this is a stub returning false unless you adapt it.
   -------------------------- */
static bool popDecodedPacket_uint8(uint8_t msg_out[14], int &out_len_bytes) {
  // ---------- REPLACE this stub with your actual queue pop ----------
  // Example (pseudocode using your Decoded1090Packet class):
  /*
  Decoded1090Packet dp;
  if (!decoded_1090_packet_out_queue.Pop(dp)) return false;
  uint32_t buf[4] = {0};
  dp.DumpPacketBuffer(buf); // fills 32-bit words big-endian
  int bits = dp.GetPacketBufferLenBits(); // 56 or 112
  int bytes = (bits + 7) / 8;
  for (int i=0;i<bytes;i++) {
    int wordIndex = i / 4;
    int byteIndex = i % 4;
    uint32_t w = buf[wordIndex];
    msg_out[i] = (uint8_t)((w >> (24 - byteIndex*8)) & 0xFF);
  }
  out_len_bytes = bytes;
  return true;
  */
  (void)msg_out; (void)out_len_bytes;
  return false;
}

/* --------------------------
   Main loop: read from queue, parse and update state
   Periodically emit all recent aircraft states
   -------------------------- */
void setup() {
  Serial.begin(USB_BAUD);
  while (!Serial) delay(1);
  // Serial2 init (RP2040 cores vary) - many cores support begin(baud,config,rx,tx)
  Serial2.begin(UART2_BAUD, SERIAL_8N1, UART2_RX_PIN, UART2_TX_PIN);
  Serial.println("ADS-B RP2040 full parser with caching starting...");
}

static uint32_t lastEmit = 0;
static const uint32_t EMIT_INTERVAL_MS = 2000;

void loop() {
  uint8_t msg[14];
  int len = 0;

  if (popDecodedPacket_uint8(msg, len)) {
    // Parse message type
    int df = (int)getbits(msg,1,5);
    if (df == 17) {
      RawFields rf;
      if (parseDF17Bytes(msg, len, rf)) {
        updateStateFromRaw(rf);
      }
    } else if (df == 5) {
      uint32_t icao; uint16_t sq;
      if (parseDF5Bytes(msg, len, icao, sq)) updateStateFromDF5(icao, sq);
    } else {
      // other DF types ignored for now
    }
  }

  uint32_t now = millis();
  if (now - lastEmit >= EMIT_INTERVAL_MS) {
    lastEmit = now;
    // emit only recently seen aircraft
    for (int i=0;i<MAX_AIRCRAFT;i++) {
      if (acList[i].icao != 0 && (now - acList[i].seen) < 60000) {
        emitAircraft(acList[i]);
      }
    }
  }

  delay(1);
}
