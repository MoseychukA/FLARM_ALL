#include <mutex>
#include "Arduino.h"
#include "pico/multicore.h"
#include "unit_conversions.h"
#include "pico/binary_info.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "comms.h"
#include "transponder_packet.h"
#include "packet_decoder.h"
#include "hal.h"
#include "unit_conversions.h"
#include "bsp.h"
#include "awb_utils.h"
#include "decode_utils.h"
#include "object_dictionary.h"
#include "aircraft_dictionary.h"
#include "nasa_cpr.h"
#include "adsbee.h"
#include "beast_tables.h"
#include "data_structures.h"
#include <stdio.h>

const uint16_t kStatusLEDBootupBlinkPeriodMs = 200; 
const uint32_t kESP32BootupTimeoutMs         = 10000;
const uint32_t kESP32BootupCommsRetryMs      = 500;

 
BSP bsp = BSP({});
ADSBee adsbee = ADSBee({});
SettingsManager settings_manager;
ObjectDictionary object_dictionary;
PacketDecoder decoder = PacketDecoder({ .enable_1090_error_correction = true });

/*Настройки только для теста*/
const int ledPin = 15;                       // the number of the LED pin
int ledState = LOW;                          // ledState used to set the LED
unsigned long previousMillis = 0;            // will store last time LED was updated
const long interval = 1000;                  // interval at which to blink (milliseconds)

const int ledPin1 = 25;
int ledState1 = LOW;
unsigned long previousMillis1 = 0;
const long interval1 = 300;

//Raw1090Packet rx_packet_[BSP::kMaxNumDemodStateMachines];
Raw1090Packet raw_1090_packet_queue_buffer_[SettingsManager::Settings::kMaxNumTransponderPackets];
PFBQueue<Raw1090Packet> raw_1090_packet_queue = PFBQueue<Raw1090Packet>({ .buf_len_num_elements = SettingsManager::Settings::kMaxNumTransponderPackets, .buffer = raw_1090_packet_queue_buffer_ });

//===================================================================================================
/* Конфигурация */
#define RX_QUEUE_SIZE 128
#define MAX_AIRCRAFT 128
#define ICAO_TTL_MS 60000UL
#define SERIALIZE_PERIOD_MS 2000UL

Raw1090Packet raw_packet;

//========================================================================

/* CRC ориентация */
static bool g_use_crc_b = false;
static volatile uint32_t g_stat_crcA_ok = 0, g_stat_crcB_ok = 0;

/* Очередь пакетов */
struct PacketBuf { uint16_t words_len; uint32_t words[4]; uint16_t bits_len; uint64_t ts_ms; };
volatile uint32_t rx_head = 0, rx_tail = 0; PacketBuf rx_queue[RX_QUEUE_SIZE];

/* CPR и состояние ВС */
struct CprFrame { bool valid = false; bool odd = false; uint32_t lat_cpr = 0; uint32_t lon_cpr = 0; uint64_t t_ms = 0; };
struct AircraftState {
    uint32_t icao = 0; char flight[9] = { 0 }; int squawk = -1;
    int pressure_alt_ft = INT32_MIN; int geo_alt_ft = INT32_MIN;
    float gs_kt = 0; float track_deg = 0; int vert_rate_fpm = 0;
    CprFrame last_even; CprFrame last_odd; bool has_pos = false; double lat = 0; double lon = 0; uint64_t last_pos_ms = 0;
    bool has_ref = false; double ref_lat = 0; double ref_lon = 0; uint64_t ref_ms = 0;
    uint64_t last_seen_ms = 0;
};
static AircraftState g_aircraft[MAX_AIRCRAFT];

/* Статистика */
static volatile uint32_t g_stat_enq = 0, g_stat_proc = 0, g_stat_ok = 0, g_stat_fix1 = 0, g_stat_fail = 0, g_stat_badlen = 0, g_stat_altok = 0;

/* Утилиты */
static uint16_t crc16_ccitt(const uint8_t* data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; ++i)
    {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; b++)
        {
            crc = (crc & 0x8000) ? ((crc << 1) ^ 0x1021) : (crc << 1);
        }
    }
    return crc;
}

/* CRC‑24 варианты */
static uint32_t modes_crc24_a(const uint8_t* bits, int nbits) {
    uint32_t crc = 0; const uint32_t poly = 0x864CFB;
    for (int i = 0; i < nbits; i++) {
        int by = i >> 3; int bi = 7 - (i & 7);
        uint8_t bit = (bits[by] >> bi) & 1u;
        uint8_t top = (crc >> 23) & 1u;
        crc = ((crc << 1) & 0xFFFFFF) | bit;
        if (top) crc ^= poly;
    }
    return crc & 0xFFFFFF;
}

static uint32_t modes_crc24_b(const uint8_t* bits, int nbits) {
    uint32_t crc = 0; const uint32_t poly = 0xFFF409;
    for (int i = 0; i < nbits; i++) {
        int by = i >> 3; int bi = (i & 7);
        uint8_t bit = (bits[by] >> bi) & 1u;
        uint8_t top = (crc & 1u);
        crc >>= 1;
        if (top ^ bit) crc ^= poly;
    }
    return crc & 0xFFFFFF;
}


static inline uint32_t bitrev32(uint32_t v) {
    v = ((v & 0x55555555u) << 1) | ((v >> 1) & 0x55555555u);
    v = ((v & 0x33333333u) << 2) | ((v >> 2) & 0x33333333u);
    v = ((v & 0x0F0F0F0Fu) << 4) | ((v >> 4) & 0x0F0F0F0Fu);
    v = (v << 24) | ((v & 0xFF00u) << 8) | ((v >> 8) & 0xFF00u) | (v >> 24);
    return v;
}

static void words32_to_bytes_variant(const uint32_t* w, uint16_t words32, uint8_t* out_bytes, uint16_t& out_len_bytes, bool reverse_bits) {
    uint32_t tmp[4] = { 0,0,0,0 };
    for (int i = 0; i < words32 && i < 4; i++) tmp[i] = reverse_bits ? bitrev32(w[i]) : w[i];
    int total_bits = words32 * 32;
    int total_bytes = (total_bits + 7) / 8;
    out_len_bytes = total_bytes;
    memset(out_bytes, 0, total_bytes);
    for (int i = 0; i < total_bits; ++i) {
        int widx = i / 32;
        int bpos = 31 - (i % 32);
        uint8_t bit = (tmp[widx] >> bpos) & 1u;
        int by = i >> 3;
        int bi = 7 - (i & 7);
        out_bytes[by] |= (bit << bi);
    }
}

static uint32_t get_bits_u32(const uint8_t* bits, int start, int len)
{
    uint32_t v = 0;
    for (int i = 0; i < len; ++i)
    {
        int idx = start + i;
        int by = idx >> 3;
        int bi = 7 - (idx & 7); v = (v << 1) | ((bits[by] >> bi) & 1u);
    } return v;
}

static bool correct_one_bit(uint8_t* msg, int nbits)
{
    for (int i = 0; i < nbits; ++i) {
        int by = i >> 3; int bi = 7 - (i & 7);
        msg[by] ^= (1u << bi);
        int df = get_bits_u32(msg, 0, 5);
        bool ok = false;
        if (nbits == 112 && (df == 17 || df == 18 || df == 5 || df == 21 || df == 4 || df == 20)) {
            uint32_t aa = get_bits_u32(msg, 8, 24);
            uint32_t parity = get_bits_u32(msg, 88, 24);
            uint32_t crc = modes_crc24_a(msg, 88);
            if ((crc ^ aa) == parity) ok = true;
            if (!ok) { crc = modes_crc24_b(msg, 88); if ((crc ^ aa) == parity) ok = true; }
        }
        else {
            uint32_t parity = get_bits_u32(msg, nbits - 24, 24);
            uint32_t crc = modes_crc24_a(msg, nbits - 24);
            if (crc == parity) ok = true;
            if (!ok) { crc = modes_crc24_b(msg, nbits - 24); if (crc == parity) ok = true; }
        }
        if (ok) { g_stat_fix1++; return true; }
        msg[by] ^= (1u << bi);
    }
    return false;
}

/* Gillham/Mode A/C/S */
static int decodeID13Field(int ID13Field) {
    int hexGillham = 0;
    if (ID13Field & 0x1000) { hexGillham |= 0x0010; } // C1
    if (ID13Field & 0x0800) { hexGillham |= 0x1000; } // A1
    if (ID13Field & 0x0400) { hexGillham |= 0x0020; } // C2
    if (ID13Field & 0x0200) { hexGillham |= 0x2000; } // A2
    if (ID13Field & 0x0100) { hexGillham |= 0x0040; } // C4
    if (ID13Field & 0x0080) { hexGillham |= 0x4000; } // A4
    if (ID13Field & 0x0020) { hexGillham |= 0x0100; } // B1
    if (ID13Field & 0x0010) { hexGillham |= 0x0001; } // D1/Q
    if (ID13Field & 0x0008) { hexGillham |= 0x0200; } // B2
    if (ID13Field & 0x0004) { hexGillham |= 0x0002; } // D2
    if (ID13Field & 0x0002) { hexGillham |= 0x0400; } // B4
    if (ID13Field & 0x0001) { hexGillham |= 0x0004; } // D4
    return hexGillham;
}
static int ModeAToModeC(int g) {
    int A = (g >> 12) & 7, B = (g >> 8) & 7, C = (g >> 4) & 7, D = g & 7;
    if ((A & 3) == 3 || (B & 3) == 3 || (C & 3) == 3 || (D & 3) == 3) return -999;
    int f500 = (C & 1);
    if ((!f500 && (D & 1)) || (f500 && !(D & 1))) return -999;
    int thousands = (A << 3) | B;
    int hundreds = D * 100;
    int altitude = thousands * 1000 + hundreds + (f500 ? 500 : 0);
    if (!f500 && (C == 0) && (D == 7)) return -999;
    if (f500 && (C == 7) && (D == 0)) return -999;
    if (altitude < -1200 || altitude > 50100) return -999;
    return altitude / 100;
}
static int decodeAC13Field_feet(int AC13) {
    int m_bit = AC13 & 0x0040, q_bit = AC13 & 0x0010;
    if (!m_bit) {
        if (q_bit) { int n = ((AC13 & 0x1F80) >> 2) | ((AC13 & 0x0020) >> 1) | (AC13 & 0x000F); return n * 25 - 1000; }
        int n = ModeAToModeC(decodeID13Field(AC13)); if (n < -12) return INT32_MIN; return 100 * n;
    }
    else return INT32_MIN;
}
static int id13_to_squawk(int id) {
    int g = decodeID13Field(id);
    int A = ((g >> 12) & 7), B = ((g >> 8) & 7), C = ((g >> 4) & 7), D = (g & 7);
    return (A << 9) | (B << 6) | (C << 3) | D;
}

/* CPR */
static int NL(double lat) {
    double a = fabs(lat) * M_PI / 180.0;
    if (fabs(lat) >= 87.0) return 1;
    double x = 1 - (1 - cos(M_PI / 30.0)) / (cos(a) * cos(a));
    if (x <= 0) return 1;
    int n = (int)floor(2 * M_PI / acos(x));
    if (n < 1) n = 1; if (n > 59) n = 59; return n;
}
static bool cpr_global_decode(const CprFrame& even, const CprFrame& odd, double& out_lat, double& out_lon) {
    if (!even.valid || !odd.valid) return false;
    double ye = even.lat_cpr / 131072.0, yo = odd.lat_cpr / 131072.0;
    double xe = even.lon_cpr / 131072.0, xo = odd.lon_cpr / 131072.0;
    const int NZ = 15;
    double dlat0 = 360.0 / (4 * NZ), dlat1 = 360.0 / (4 * NZ - 1);
    int j = (int)floor(59.0 * ye - 60.0 * yo + 0.5);
    double rlat0 = dlat0 * (fmod(j, 60) + ye) - 90.0; if (rlat0 >= 90.0) rlat0 -= 180.0;
    double rlat1 = dlat1 * (fmod(j, 59) + yo) - 90.0; if (rlat1 >= 90.0) rlat1 -= 180.0;
    bool use_odd = (odd.t_ms > even.t_ms);
    double lat = use_odd ? rlat1 : rlat0;
    int nl = NL(lat); if (nl <= 0) return false;
    double lon;
    if (!use_odd) {
        int ni = nl; if (ni < 1) return false; double dlon = 360.0 / ni;
        int m = (int)floor(xo * nl - xe * (nl - 1) + 0.5);
        lon = dlon * (fmod(m, ni) + xe);
    }
    else {
        int ni = nl - 1; if (ni < 1) return false; double dlon = 360.0 / ni;
        int m = (int)floor(xe * (nl - 1) - xo * nl + 0.5);
        lon = dlon * (fmod(m, ni) + xo);
    }
    if (lon > 180.0) lon -= 360.0;
    out_lat = lat; out_lon = lon; return true;
}
static bool cpr_local_decode(const CprFrame& msg, double ref_lat, double ref_lon, double& out_lat, double& out_lon) {
    int f = msg.odd ? 1 : 0; const int NZ = 15; double dlat = 360.0 / (4 * NZ - f);
    double y = msg.lat_cpr / 131072.0; double x = msg.lon_cpr / 131072.0;
    int j = (int)floor(ref_lat / dlat); double lat = dlat * (j + y); if (lat >= 270.0) lat -= 360.0;
    int nl = NL(lat); if (nl <= 0) return false; int ni = nl - f; if (ni < 1) return false; double dlon = 360.0 / ni;
    int m = (int)floor(ref_lon / dlon); double lon = dlon * (m + x); if (lon > 180.0) lon -= 360.0;
    out_lat = lat; out_lon = lon; return true;
}

/* Очередь */
static bool rx_enqueue(const Raw1090Packet& p) {
    uint32_t next = (rx_head + 1) % RX_QUEUE_SIZE; if (next == rx_tail) return false;
    rx_queue[rx_head].words_len = (p.buffer_len_bits + 31) / 32;
    rx_queue[rx_head].bits_len = p.buffer_len_bits;
    for (int i = 0; i < rx_queue[rx_head].words_len && i < 4; i++) rx_queue[rx_head].words[i] = p.buffer[i];
    rx_queue[rx_head].ts_ms = (uint64_t)(p.mlat_48mhz_64bit_counts / 48000);
    rx_head = next; return true;
}
static bool rx_dequeue(PacketBuf& out) { if (rx_tail == rx_head) return false; out = rx_queue[rx_tail]; rx_tail = (rx_tail + 1) % RX_QUEUE_SIZE; return true; }

static void print_hex24(uint32_t v, char* out) { static const char* hex = "0123456789ABCDEF"; for (int i = 0; i < 6; ++i) { out[5 - i] = hex[v & 0xF]; v >>= 4; } out[6] = 0; }

/* CRC‑проверка */
static bool try_crc_variant(uint8_t* b, int nbits, uint32_t(*fn)(const uint8_t*, int))
{
    int df = get_bits_u32(b, 0, 5);
    if (nbits == 112 && (df == 17 || df == 18 || df == 5 || df == 21 || df == 4 || df == 20)) {
        uint32_t aa = get_bits_u32(b, 8, 24);
        uint32_t parity = get_bits_u32(b, 88, 24);
        uint32_t crc = fn(b, 88);
        if ((crc ^ aa) == parity) return true;
        if (correct_one_bit(b, 112)) return true;
    }
    else {
        uint32_t parity = get_bits_u32(b, nbits - 24, 24);
        uint32_t crc = fn(b, nbits - 24);
        if (crc == parity) return true;
        if (correct_one_bit(b, nbits)) return true;
    }
    return false;
}

static bool check_crc_auto(uint8_t* b, int nbits) {
    static uint32_t(*fn_main)(const uint8_t*, int) = modes_crc24_a;
    static uint32_t(*fn_alt)(const uint8_t*, int) = modes_crc24_b;

    if (try_crc_variant(b, nbits, fn_main))
    {
        if (g_use_crc_b) g_stat_crcB_ok++; else g_stat_crcA_ok++;
        return true;
    }
    if (try_crc_variant(b, nbits, fn_alt))
    {
        if (g_use_crc_b) { g_stat_crcA_ok++; g_use_crc_b = false; }
        else { g_stat_crcB_ok++; g_use_crc_b = true; }
        return true;
    }
    return false;
}
/* Состояние бортов */
static AircraftState* get_aircraft(uint32_t icao)
{
    for (int i = 0; i < MAX_AIRCRAFT; ++i) if (g_aircraft[i].icao == icao) return &g_aircraft[i];
    for (int i = 0; i < MAX_AIRCRAFT; ++i) if (g_aircraft[i].icao == 0)
    {
        g_aircraft[i].icao = icao; return &g_aircraft[i];
    }
    int idx = 0; uint64_t min_seen = UINT64_MAX;
    for (int i = 0; i < MAX_AIRCRAFT; ++i) if (g_aircraft[i].last_seen_ms < min_seen)
    {
        min_seen = g_aircraft[i].last_seen_ms; idx = i;
    }
    g_aircraft[idx] = AircraftState();
    g_aircraft[idx].icao = icao;
    return &g_aircraft[idx];
}

/* Вывод */
static void send_status_json(const struct AircraftState* ac)
{
    char icao_hex[7]; print_hex24(ac->icao, icao_hex);
    float spd_kmh = (ac->gs_kt) * 1.852f;
    float alt_m = (ac->pressure_alt_ft == INT32_MIN ? NAN : ac->pressure_alt_ft * 0.3048f);
    float vr_mpm = ac->vert_rate_fpm * 0.3048f;
    char json[512];
    //snprintf(json, sizeof(json), "{addr%s,Squawk:%d,flight:%s,altitude:%.0f,pressure_altitude:%.0f,speed:%.1f,course:%.1f,vert_rate:%.1f,latitude:%.6f,longitude:%.6f,seen:%llu,timestamp:%llu,signal_source:1,aircraft_type:9}", icao_hex, ac->squawk < 0 ? 0 : ac->squawk, ac->flight, alt_m, alt_m, spd_kmh, ac->track_deg, vr_mpm,
    //    ac->lat, ac->lon, (unsigned long long)(ac->last_seen_ms), (unsigned long long)millis());
    snprintf(json, sizeof(json), "%s, %d, %s, %.0f, %.0f, %.1f, %.1f, %.1f, %.6f, %.6f, %llu, %llu", icao_hex, ac->squawk < 0 ? 0 : ac->squawk, ac->flight, alt_m, alt_m, spd_kmh, ac->track_deg, vr_mpm,
        ac->lat, ac->lon, (unsigned long long)(ac->last_seen_ms), (unsigned long long)millis());

    char frame[600];
    uint16_t crc = crc16_ccitt((const uint8_t*)json, strlen(json));
    snprintf(frame, sizeof(frame), "%s\r\n", json);
    //snprintf(frame, sizeof(frame), "!ADSB:%s|CRC16=%04X\r\n", json, crc);
    Serial2.print(frame);
    Serial.print(frame);
}

/* Обработка пакета */
static void process_packet(const PacketBuf& pb) 
{
    g_stat_proc++;
    uint8_t bytes[16] = { 0 }, bytes_alt[16] = { 0 }; uint16_t blen = 0, blen_alt = 0;
    words32_to_bytes_variant(pb.words, pb.words_len, bytes, blen, false);
    words32_to_bytes_variant(pb.words, pb.words_len, bytes_alt, blen_alt, true);

    int nbits = pb.bits_len;

    //Serial.print("nbits: ");
    //Serial.println(nbits);
    if (nbits != 112 && nbits != 56) 
    {
        g_stat_badlen++; 
        return; 
    }

 /*   bool ok = check_crc_auto(bytes, nbits);
    bool used_alt = false;
    if (!ok) 
    { 
        used_alt = true;
        ok = check_crc_auto(bytes_alt, nbits); 
       if (ok) memcpy(bytes, bytes_alt, sizeof(bytes)); 
    }
    if (!ok) 
    {
        g_stat_fail++; 
        return; 
    }

    if (used_alt) g_stat_altok++; 
    else g_stat_ok++;*/

    uint64_t now_ms = pb.ts_ms;
    uint32_t aa = get_bits_u32(bytes, 8, 24);
    AircraftState* ac = get_aircraft(aa);
    ac->last_seen_ms = now_ms;

    int df = get_bits_u32(bytes, 0, 5);

    if (df == 17 || df == 18) {
        uint32_t type_code = get_bits_u32(bytes, 32, 5);
        uint64_t me = 0; for (int i = 0; i < 56; ++i) me = (me << 1) | get_bits_u32(bytes, 32 + i, 1);

        if (type_code >= 1 && type_code <= 4) {
            char fl[9]; for (int i = 0; i < 8; ++i) { uint8_t code = (me >> (6 * (7 - i))) & 0x3F; char c = ' '; if (code >= 1 && code <= 26) c = 'A' + code - 1; else if (code >= 48 && code <= 57) c = '0' + (code - 48); fl[i] = c; } fl[8] = 0; strncpy(ac->flight, fl, 9);
        }
        else if ((type_code >= 9 && type_code <= 18) || (type_code >= 20 && type_code <= 22)) {
            bool odd = get_bits_u32(bytes, 53, 1); uint32_t ac13 = get_bits_u32(bytes, 40, 13);
            int alt_ft = decodeAC13Field_feet(ac13); if (alt_ft != INT32_MIN) { ac->pressure_alt_ft = alt_ft; ac->geo_alt_ft = alt_ft; }
            uint32_t latc = get_bits_u32(bytes, 54, 17); uint32_t lonc = get_bits_u32(bytes, 71, 17);
            CprFrame cf; cf.valid = true; cf.odd = odd; cf.lat_cpr = latc; cf.lon_cpr = lonc; cf.t_ms = now_ms;
            if (odd) ac->last_odd = cf; else ac->last_even = cf;

            bool pos_ok = false; double lat, lon;
            if (ac->last_even.valid && ac->last_odd.valid && (llabs((long long)ac->last_even.t_ms - (long long)ac->last_odd.t_ms) <= 10000)) pos_ok = cpr_global_decode(ac->last_even, ac->last_odd, lat, lon);
            if (!pos_ok && ac->has_ref) pos_ok = cpr_local_decode(cf, ac->ref_lat, ac->ref_lon, lat, lon);

            if (pos_ok) {
                double max_range_km = 5.0;
                if (ac->gs_kt > 0) { double gs_kmh = ac->gs_kt * 1.852; max_range_km = gs_kmh * 0.5; if (max_range_km < 5.0) max_range_km = 5.0; if (max_range_km > 50.0) max_range_km = 50.0; }
                auto dist_km = [](double lat1, double lon1, double lat2, double lon2) {
                    double R = 6371.0; double a = sin((lat2 - lat1) * M_PI / 360.0); a *= a; double b = sin((lon2 - lon1) * M_PI / 360.0); b *= b * cos(lat1 * M_PI / 180.0) * cos(lat2 * M_PI / 180.0); double c = 2 * asin(sqrt(a + b)); return R * c; };

                bool within_range = true;
                if (ac->has_ref) { double dk = dist_km(ac->ref_lat, ac->ref_lon, lat, lon); if (dk > max_range_km) within_range = false; }

                if (within_range) {
                    ac->lat = lat; ac->lon = lon; ac->has_pos = true; ac->last_pos_ms = now_ms;

                    const uint64_t REF_TTL_MS = 30000; const double MAX_REF_JUMP_KM = 10.0;
                    bool reset_ref = false;
                    if (ac->has_ref && (now_ms - ac->ref_ms > REF_TTL_MS)) reset_ref = true;
                    if (ac->has_ref && !reset_ref) { double dk2 = dist_km(ac->ref_lat, ac->ref_lon, lat, lon); if (dk2 > MAX_REF_JUMP_KM) reset_ref = true; }
                    if (reset_ref) ac->has_ref = false;

                    ac->ref_lat = lat; ac->ref_lon = lon; ac->ref_ms = now_ms; ac->has_ref = true;
                }
            }
        }
        else if (type_code == 19) {
            int subtype = get_bits_u32(bytes, 37, 3);
            if (subtype == 1 || subtype == 2) {
                int s_ew = get_bits_u32(bytes, 45, 1) ? -1 : 1; int v_ew = get_bits_u32(bytes, 46, 10) - 1;
                int s_ns = get_bits_u32(bytes, 56, 1) ? -1 : 1; int v_ns = get_bits_u32(bytes, 57, 10) - 1;
                if (v_ew >= 0 && v_ns >= 0) { float vew = (float)(s_ew * v_ew); float vns = (float)(s_ns * v_ns); float gs = sqrtf(vew * vew + vns * vns); float trk = fmodf(atan2f(vew, vns) * 180.0f / M_PI + 360.0f, 360.0f); ac->gs_kt = gs; ac->track_deg = trk; }
                int vr_sign = get_bits_u32(bytes, 67, 1) ? -1 : 1; int vr = get_bits_u32(bytes, 68, 9) - 1; if (vr >= 0) ac->vert_rate_fpm = vr_sign * vr;
            }
            else if (subtype == 3) {
                int hdg_stat = get_bits_u32(bytes, 46, 1); int hdg = get_bits_u32(bytes, 47, 10); if (hdg_stat) ac->track_deg = (hdg / 1024.0f) * 360.0f;
                int as_stat = get_bits_u32(bytes, 57, 1); int as = get_bits_u32(bytes, 58, 10) - 1; if (as_stat && as >= 0) ac->gs_kt = (float)as;
                int vr_sign = get_bits_u32(bytes, 68, 1) ? -1 : 1; int vr = get_bits_u32(bytes, 69, 9) - 1; if (vr >= 0) ac->vert_rate_fpm = vr_sign * vr;
            }
        }
    }
    else if (df == 5 || df == 21) {
        int id13 = get_bits_u32(bytes, 32, 13); ac->squawk = id13_to_squawk(id13);
    }
    else if (df == 4 || df == 20) {
        int ac13 = get_bits_u32(bytes, 32, 13); int alt_ft = decodeAC13Field_feet(ac13); if (alt_ft != INT32_MIN) { ac->pressure_alt_ft = alt_ft; ac->geo_alt_ft = alt_ft; }
    }

    if (ac->has_pos || ac->gs_kt > 0 || strlen(ac->flight) > 0 || ac->squawk >= 0) send_status_json(ac);
}

/* TTL и сериализация */
static void periodic_housekeeping() 
{
    static uint64_t last_ser = 0; uint64_t now = millis();
    for (int i = 0; i < MAX_AIRCRAFT; ++i)
        if (g_aircraft[i].icao != 0)
        if (now - g_aircraft[i].last_seen_ms > ICAO_TTL_MS) g_aircraft[i] = AircraftState();

    if (now - last_ser >= SERIALIZE_PERIOD_MS) 
    {
        for (int i = 0; i < MAX_AIRCRAFT; ++i)  if (g_aircraft[i].icao != 0) send_status_json(&g_aircraft[i]);
        last_ser = now;
    }
}

/* Колбэк приема из adsbee.cpp */
extern "C" void ADSB_OnRawPacket(Raw1090Packet * pkt)
{
    //// Вывести содержимое пакета в HEX
    //Serial.printf("Пришли пакет (HEX): ");
    //for (int i = 0; i < Raw1090Packet::kMaxPacketLenWords32; ++i) 
    //{
    //    Serial.printf("%08X ", pkt->buffer[i]);
    //}
    //Serial.println();
    g_stat_enq++;
    rx_enqueue(*pkt);
}

/* Хекс-инъекция тестовой пары DF17 (уберите в эфире) */
static bool hex2bytes(const char* hex, uint8_t* out, int out_len)
{
    int n = strlen(hex);
    if (n != 28) return false;
    auto cv = [](char c)->int
    {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
        if (c >= 'a' && c <= 'f') return 10 + (c - 'a'); return -1;
    };
    for (int i = 0; i < 14; ++i)
    {
        int hi = cv(hex[2 * i]), lo = cv(hex[2 * i + 1]);
        if (hi < 0 || lo < 0) return false; out[i] = (uint8_t)((hi << 4) | lo);
    }
    return true;
}
static void inject_hex_df17(const char* hex)
{
    uint8_t msg[14];
    if (!hex2bytes(hex, msg, 14))
    {
        Serial.println("hex len invalid");
        return;
    }
    uint32_t w[4] = { 0,0,0,0 };
    for (int i = 0; i < 112; i++)
    {
        int by = i >> 3, bi = 7 - (i & 7); int bit = (msg[by] >> bi) & 1; int wi = i >> 5, wb = 31 - (i & 31);
        w[wi] |= (bit << wb);
    }
    Raw1090Packet p;
    memcpy(p.buffer, w, sizeof(w));
    p.buffer_len_bits = 112;
    p.mlat_48mhz_64bit_counts = (uint64_t)time_us_64() * 48ULL;
    ADSB_OnRawPacket(&p);
}
static void inject_test_pair()
{
    Serial.println("inject_test_pair START");
    inject_hex_df17("8D40621D58C382D690C8AC2863A7"); // even
    delay(100);
    inject_hex_df17("8D40621D58C386435CC412692AD6"); // odd
    Serial.println("inject_test_pair END");
}


//========================================================================



void setup() 
{
    bi_decl(bi_program_description("ADSBee 1090 ADSB Receiver"));

    comms_manager.Init();  //Сначала настроим вывод в КОМ порт
    sleep_ms(500);
    adsbee.Init();
    comms_manager.console_printf("ADSBee 1090 Version %d.%d.%d\r\n",
        object_dictionary.kFirmwareVersionMajor, object_dictionary.kFirmwareVersionMinor,
        object_dictionary.kFirmwareVersionPatch);

    Serial.begin(115200);
    unsigned long t0 = millis(); while (!Serial && !Serial.dtr() && (millis() - t0) < 8000) delay(10);
    delay(2000);


    comms_manager.console_printf("Software ");
    Serial.print("Software ");
    String ver_soft = __FILE__;
    int val_srt = ver_soft.lastIndexOf('\\');
    ver_soft.remove(0, val_srt + 1);
    val_srt = ver_soft.lastIndexOf('.');
    ver_soft.remove(val_srt);
    Serial.println(ver_soft);
    comms_manager.console_printf(ver_soft.c_str());
 
    settings_manager.Load();    // Загрузить настройки по умолчанию. Нужно еще поработать с этой функцией.

    for (uint16_t i = 0; i < 5; i++)
    {
        adsbee.SetStatusLED(true);
        sleep_ms(kStatusLEDBootupBlinkPeriodMs / 2);
        adsbee.SetStatusLED(false);
        sleep_ms(kStatusLEDBootupBlinkPeriodMs / 2);
    }

   // inject_test_pair();
    Serial.println("Setup End\r\n");
    comms_manager.console_printf("\r\nSetup End\r\n");
}


void setup1()
{
    pinMode(ledPin1, OUTPUT);
}



void loop() 
{

   // decoder.UpdateLogLoop();   // Вывод сырых пакетов ().
   // comms_manager.Update();    // Вывод расшифрованных пакетов.
    adsbee.Update();

    //PacketBuf pb;
    //if (rx_dequeue(pb)) process_packet(pb);
    //else 
    //{ 
    //    periodic_housekeeping();
    //    delay(5);
    //}

 /*   static unsigned long t_stat = 0;
    if (millis() - t_stat > 2000) 
    {
        t_stat = millis();
        Serial.printf("STAT enq=%lu proc=%lu ok=%lu fix1=%lu altok=%lu fail=%lu badlen=%lu q(h=%u,t=%u)\r\n",
            (unsigned long)g_stat_enq, (unsigned long)g_stat_proc, (unsigned long)g_stat_ok,
            (unsigned long)g_stat_fix1, (unsigned long)g_stat_altok, (unsigned long)g_stat_fail,
            (unsigned long)g_stat_badlen,(unsigned)rx_head, (unsigned)rx_tail);
    }*/
 
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) 
    {
        previousMillis = currentMillis;
        if (ledState == LOW) 
        {
            ledState = HIGH;
        }
        else 
        {
            ledState = LOW;
        }
         digitalWrite(ledPin, ledState);
    }
}


void loop1()
{
   decoder.UpdateDecoderLoop();   //PacketDecoder 

    unsigned long currentMillis1 = millis();
    if (currentMillis1 - previousMillis1 >= interval1)
    { 
        previousMillis1 = currentMillis1;
        if (ledState1 == LOW)
        {
            ledState1 = HIGH;
        }
        else
        {
            ledState1 = LOW;
        }
       digitalWrite(ledPin1, ledState1);
    }


}
