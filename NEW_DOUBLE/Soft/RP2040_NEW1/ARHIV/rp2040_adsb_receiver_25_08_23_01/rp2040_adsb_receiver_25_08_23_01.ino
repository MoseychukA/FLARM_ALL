// rp2040_adsb_receiver.ino
// Board: Raspberry Pi Pico / RP2040 (Earle Philhower core recommended)
// Features: 3-channel ADS-B sampler via PIO+DMA into ring buffers, dual-core
//           preamble weighted correlator, digital filter, CPR decode (local/global),
//           DF17/DF18/DF20/DF21 parsing incl. TC=19 subtypes and BDS 6.0/6.1,
//           Gillham AC13 (Q=0) altitude, single-bit CRC correction,
//           LittleFS config (profiles), USB Serial CLI, Serial2 to ESP32S3.

#include <Arduino.h>
#include <LittleFS.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/regs/dreq.h"
#include "hardware/sync.h"
#include "hardware/irq.h"

#include "adsb_sampler.pio.h"  // pre-generated PIO header
#include "protocol.h"
#include "crc.h"
#include "cpr.h"
#include "adsb_decoder.h"
#include "bds.h"
#include "config.h"

// ---------------- Pins and Serials ----------------
// USB Serial debug/control
static const uint32_t USB_BAUD = 115200;
// Serial2 to ESP32S3
static const int UART2_TX = 4;  // GPIO4
static const int UART2_RX = 5;  // GPIO5
static const uint32_t UART2_BAUD = 921600;

// Input pins for ADS-B channels
static const int CH_PIN[3] = {19, 22, 18};
// Preamble indicator pins
static const int PRE_IND_PIN[3] = {17, 20, 23};
// RSSI analog pin (GPIO26 -> ADC0)
static const int RSSI_PIN = 26;

// Periodic toggle pins
static const int CORE0_TOGGLE_PIN = 15; // 1000 ms
static const int CORE1_TOGGLE_PIN = 25; // 500 ms

// ---------------- PIO + DMA sampler params ----------------
// Target sample rate: 10 MHz (0.1 us) to capture 0.5 us pulses reliably
static const uint32_t SAMPLE_RATE_HZ = 10000000UL;
// Ring buffer sizes (power of two for fast masking)
static const uint32_t RING_WORDS = 1 << 12; // 4096 32-bit words per channel
static const uint32_t RING_MASK = RING_WORDS - 1;

// DMA ring buffers per channel
static volatile uint32_t *ringbuf[3];
static volatile uint32_t write_idx[3];
static volatile uint32_t read_idx[3];

// PIO state machines per channel
static PIO pio = pio0;
static int sm_idx[3];
static int dma_chan[3];

// Correlator / filter settings (can be updated via CLI or config)
volatile bool filter_enabled = true;
volatile float filt_min_us = 0.4f; // lower pass threshold
volatile float filt_max_us = 0.7f; // upper pass threshold
volatile float normalize_to_us = 0.5f;

// Dynamic threshold parameters
volatile float g_base_thr = 0.25f; // base energy threshold
volatile float ema_alpha = 0.02f;  // EMA smoothing for noise
volatile float k_dyn = 1.5f;       // multiplier for dynamic threshold

// Dead time after a detection (us)
volatile uint32_t dead_time_us = 7;

// Profiles
volatile Profile current_profile = Profile::Normal;

// CPR state
CPRContext cpr_ctx;

// Output queue to ESP32S3
static const size_t OUT_Q_CAP = 256;
FOFrame out_queue[OUT_Q_CAP];
volatile uint16_t out_q_head = 0, out_q_tail = 0;

// Received packets raw queue per channel
static const size_t RAW_Q_CAP = 512;
RawPacket raw_q[3][RAW_Q_CAP];
volatile uint16_t raw_head[3] = {0}, raw_tail[3] = {0};

// RSSI EMA
volatile float rssi_ema = 0.0f;
volatile float rssi_alpha = 0.05f;

// Core toggles
volatile uint32_t core0_last_toggle = 0;
volatile uint32_t core1_last_toggle = 0;

// --------------- Helpers ---------------
static inline void pin_fast_out(int gpio) {
  gpio_init(gpio);
  gpio_set_dir(gpio, GPIO_OUT);
}

static inline void pin_fast_in(int gpio) {
  gpio_init(gpio);
  gpio_set_dir(gpio, GPIO_IN);
}

static inline void gpio_write(int gpio, bool v) {
  gpio_put(gpio, v);
}

static inline uint32_t micros32() {
  return (uint32_t)time_us_32();
}

// Enqueue raw packet
static inline void raw_enqueue(int ch, const RawPacket &rp) {
  uint16_t nxt = (raw_head[ch] + 1) % RAW_Q_CAP;
  if (nxt == raw_tail[ch]) return; // drop on overflow
  raw_q[ch][raw_head[ch]] = rp;
  raw_head[ch] = nxt;
}

static inline bool raw_dequeue_any(int &ch, RawPacket &rp) {
  for (int i=0;i<3;i++) {
    if (raw_tail[i] != raw_head[i]) {
      ch = i;
      rp = raw_q[i][raw_tail[i]];
      raw_tail[i] = (raw_tail[i]+1)%RAW_Q_CAP;
      return true;
    }
  }
  return false;
}

static inline void out_enqueue(const FOFrame &fo) {
  uint16_t nxt = (out_q_head + 1) % OUT_Q_CAP;
  if (nxt == out_q_tail) return; // overflow drop
  out_queue[out_q_head] = fo;
  out_q_head = nxt;
}

static inline bool out_dequeue(FOFrame &fo) {
  if (out_q_tail == out_q_head) return false;
  fo = out_queue[out_q_tail];
  out_q_tail = (out_q_tail + 1) % OUT_Q_CAP;
  return true;
}

// --------------- Sampler setup ---------------

static void setup_pio_sm(int ch, int pin) 
{
  // Load program once
  static int prog_offset = -1;
  if (prog_offset < 0) 
  {
    prog_offset = pio_add_program(pio, &adsb_sampler_program);
  }
  sm_idx[ch] = pio_claim_unused_sm(pio, true);

  // Configure SM
  adsb_sampler_program_init(pio, sm_idx[ch], prog_offset, pin, SAMPLE_RATE_HZ);
}

static void setup_dma_for_sm(int ch) {
  dma_chan[ch] = dma_claim_unused_channel(true);

  dma_channel_config c = dma_channel_get_default_config(dma_chan[ch]);
  channel_config_set_read_increment(&c, false);
  channel_config_set_write_increment(&c, true);
  channel_config_set_dreq(&c, pio_get_dreq(pio, sm_idx[ch], false));
  channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
  channel_config_set_ring(&c, true, 12); // 2^12 words ring = 4096 words

  dma_channel_configure(
    dma_chan[ch],
    &c,
    (void*)ringbuf[ch],          // dst
    &pio->rxf[sm_idx[ch]],       // src
    RING_WORDS,                  // transfer count (words) - triggers wrap via ring
    true                         // start
  );
}

// --------------- Correlator and detector ---------------

// Extract bits from ring buffer. Each word holds 32 samples (bit per sample).
// We assume LSB-first from PIO.
static inline uint8_t get_sample_bit(uint32_t word, int idx) {
  return (word >> idx) & 1u;
}

// Detector state per channel
struct DetectState {
  uint32_t last_detect_us = 0;
  float noise_ema = 0.0f;
};
static DetectState det[3];

// Weighted correlation against Mode S preamble over 80 samples (~8 us at 10 MHz)
// Windows: A,B,C,D pulses positions with tolerance checks on dAB~10, dBC~25, dCD~10 samples
// Returns t0 offset (start of preamble window) or -1 if not detected.
static int corr_preamble_and_get_t0(const uint8_t *s, int len, int ch) 
{
  if (len < 80) return -1;

  // Expected pulse centers in samples from t0
  const int A = 0;
  const int B = 10;
  const int C = 35;
  const int D = 45;

  // Correlate using weighted windows and penalties
  int posHitsA=0,posHitsB=0,posHitsC=0,posHitsD=0;
  int negHits=0, penalty=0;

  auto window_score = [&](int center)
  {
    int hits=0;
    for(int i=-3;i<=3;i++){
      int idx = center+i;
      if (idx>=0 && idx<len) hits += s[idx];
    }
    return hits; // 0..7
  };

  posHitsA = window_score(A);
  posHitsB = window_score(B);
  posHitsC = window_score(C);
  posHitsD = window_score(D);

  // Count unexpected ones in the gaps (negHits)
  for(int i=0;i<80;i++)
  {
    bool inPos = (abs(i-A)<=3)||(abs(i-B)<=3)||(abs(i-C)<=3)||(abs(i-D)<=3);
    if (!inPos && s[i]) negHits++;
  }

  // Long pulse rejection: detect runs > 7 samples (~0.7 us)
  int run=0; bool longPulse=false; for(int i=0;i<80;i++){ run = s[i] ? run+1 : 0; if (run>7){ longPulse=true; break; } }
  if (longPulse) return -1;

  // Score
  int posScore = posHitsA + posHitsB + posHitsC + posHitsD; // 0..28
  penalty = negHits;

  // Dynamic thresholding
  float dyn_thr = g_base_thr + k_dyn * det[ch].noise_ema;
  // Map score to 0..1 approx: max 28, subtract penalty
  float score = (posScore - penalty) / 28.0f;

  // Update noise when fail
  if (score < dyn_thr) {
    det[ch].noise_ema = (1.0f - ema_alpha)*det[ch].noise_ema + ema_alpha*(negHits/60.0f);
    return -1;
  }

  // Additional spacing checks (narrow tolerance in high EMI profiles)
  int tol = (current_profile==Profile::HighEMI) ? 1 : 2;
  // Try to refine A position by local maximum within first 8 samples
  int bestA = 0, bestAhits = posHitsA;
  for (int i=-2;i<=2;i++){
    int c = A+i;
    if (c>=0){
      int h = window_score(c);
      if (h>bestAhits){ bestAhits=h; bestA=c; }
    }
  }
  int dAB = B - bestA;
  int dBC = C - B;
  int dCD = D - C;
  if (abs(dAB-10)>tol || abs(dBC-25)>tol || abs(dCD-10)>tol){
    det[ch].noise_ema = (1.0f - ema_alpha)*det[ch].noise_ema + ema_alpha*(negHits/60.0f);
    return -1;
  }

  return 0; // t0 at start of s[0]
}

// Цифровой фильтр: отбрасывает импульсы короче минимума или длиннее максимума, нормализует ширину до ~0,5 мкс
static void digital_filter_samples(uint8_t *s, int n) 
{
  if (!filter_enabled) return;
  int minS = (int)roundf(filt_min_us * SAMPLE_RATE_HZ / 1e6);
  int maxS = (int)roundf(filt_max_us * SAMPLE_RATE_HZ / 1e6);
  int normS = (int)roundf(normalize_to_us * SAMPLE_RATE_HZ / 1e6);
  int i=0;
  while (i<n) 
  {
    if (s[i]==0) { i++; continue; }
    int j=i; while (j<n && s[j]) j++;
    int w = j-i;
    if (w<minS || w>maxS) {
      // wipe as noise
      for (int k=i;k<j;k++) s[k]=0;
    } else {
      // normalize width to normS
      for (int k=i;k<j;k++) s[k]= (k < i+normS) ? 1 : 0;
    }
    i=j;
  }
}

// Extract packet bits following a detected preamble starting at t0
// Return bit length (56 or 112) and fill bits[] as bytes (LSB first per byte)
static int extract_packet_bits(const uint8_t *s, int len, uint8_t *outBytes, int maxBytes) {
  // Mode S long: 112 bits, short: 56 bits. Each bit cell is 1 us = 10 samples.
  // After 8 us preamble, first bit starts at +8us.
  const int bitSamp = 10;
  const int preSamp = 80; // 8us

  // We will attempt 112 first if we have enough samples
  auto try_extract = [&](int nBits)->bool{
    int nBytes = (nBits+7)/8;
    if (nBytes>maxBytes) return false;
    memset(outBytes, 0, nBytes);
    for (int b=0;b<nBits;b++) {
      int center = preSamp + b*bitSamp + bitSamp/2;
      if (center+2 >= len) return false;
      // Manchester-like: ones are a pulse in first half, zeros in second (for ADS-B it's pulse position modulation)
      // Simplified energy: sum window early vs late
      int early=0, late=0;
      for (int k=-2;k<=2;k++){
        int idx = center + k;
        if (idx>=preSamp && idx<len) {
          if (k<=0) early += s[idx]; else late += s[idx];
        }
      }
      int bit = (early>late) ? 1 : 0;
      outBytes[b>>3] |= (bit << (7-(b&7))); // MSB-first per byte standard for Mode S payload
    }
    return true;
  };

  if (try_extract(112)) return 112;
  if (try_extract(56)) return 56;
  return 0;
}

// Attempt single-bit error correction by flipping each bit until CRC validates
static bool try_single_bit_fix(uint8_t *frame, int nBits) {
  int nBytes = (nBits+7)/8;
  for (int i=0;i<nBits;i++){
    int byte = i>>3; int bit = 7-(i&7);
    frame[byte] ^= (1<<bit);
    if (modes_crc_ok(frame, nBits)) return true;
    frame[byte] ^= (1<<bit);
  }
  return false;
}

// Channel worker: consume ring buffer and detect packets, enqueue raw
static void channel_worker(int ch) 
{
  // Sliding window over samples. Convert ring words into bitstream window.
  const int windowSamp = 80 + 112*10 + 20; // enough for long packet
  static uint8_t win[3][1600]; // 1600 samples max
  int wp = 0;

  uint32_t last_dead_until = 0;

  for(;;)
  {

      uint32_t now = millis();
      if (now - core1_last_toggle >= 500)
      {
          core1_last_toggle = now;
          static bool lv = false; lv = !lv; gpio_write(CORE1_TOGGLE_PIN, lv);
      }
    // Move data from DMA ring into window
    while (read_idx[ch] != write_idx[ch]) 
    {
      uint32_t w = ringbuf[ch][read_idx[ch] & RING_MASK];
      read_idx[ch]++;
      // unpack 32 samples
      for(int i=0;i<32;i++)
      {
        win[ch][wp] = (w>>i)&1u; // LSB-first
        wp++; if (wp>=windowSamp){ // process on full window
          // optional digital filter
         //!! digital_filter_samples(win[ch], wp);
          int t0 = corr_preamble_and_get_t0(win[ch], wp, ch);
          if (t0>=0) {
            uint32_t nowus = micros32();
            if (nowus >= det[ch].last_detect_us + dead_time_us) 
            {
              // indicate preamble
              gpio_write(PRE_IND_PIN[ch], 1);
              // extract bits
              uint8_t frame[32] = {0};
              int nBits = extract_packet_bits(win[ch]+t0, wp-t0, frame, sizeof(frame));
              gpio_write(PRE_IND_PIN[ch], 0);
              if (nBits==56 || nBits==112) {
                // Debug RAW before CRC
                Serial.print(F("RAW CH")); Serial.print(ch+1);
                Serial.print(F(" "));
                for (int i=0;i<(nBits+7)/8;i++){ if (i) Serial.print(' '); Serial.print(frame[i], HEX);} Serial.println();

                bool ok = modes_crc_ok(frame, nBits);
                if (!ok) {
                  // try one-bit fix
                  uint8_t tmp[32]; memcpy(tmp, frame, sizeof(tmp));
                  if (try_single_bit_fix(tmp, nBits)) {
                    RawPacket rp; rp.channel=ch; rp.bitlen=nBits; rp.ok=true; memcpy(rp.bytes, tmp, (nBits+7)/8);
                    raw_enqueue(ch, rp);
                  }
                } else {
                  RawPacket rp; rp.channel=ch; rp.bitlen=nBits; rp.ok=true; memcpy(rp.bytes, frame, (nBits+7)/8);
                  raw_enqueue(ch, rp);
                }
                det[ch].last_detect_us = nowus;
              }
            }
          }
          // shift window left half to keep overlap
          int keep = 200; // keep some tail
          memmove(win[ch], win[ch]+wp-keep, keep);
          wp = keep;
        }
      }
    }
    tight_loop_contents();
  }
}



static void send_to_esp32(const FOFrame &fo){
  PackedFO pkt; packFO(fo, pkt);
  uint32_t crc = crc32_ieee((const uint8_t*)&pkt, sizeof(pkt));
  Serial2.write((const uint8_t*)&pkt, sizeof(pkt));
  Serial2.write((const uint8_t*)&crc, sizeof(crc));
}

static void process_packets()
{
  int ch; RawPacket rp;
  while (raw_dequeue_any(ch, rp)){
    // Decode
    DecodedADSB d; memset(&d, 0, sizeof(d));
    if (!adsb_decode(rp.bytes, rp.bitlen, micros32(), cpr_ctx, d)) continue;

    // Report to USB
    Serial.print(F("DECODE CH")); Serial.print(ch+1);
    Serial.print(F(" icao=")); Serial.print(d.addr, HEX);
    Serial.print(F(" flight=\"")); Serial.print(d.flight);
    Serial.print(F("\" lat=")); Serial.print(d.lat, 6);
    Serial.print(F(" lon=")); Serial.print(d.lon, 6);
    Serial.print(F(" spd=")); Serial.print(d.speed_kmh);
    Serial.print(F(" alt=")); Serial.println(d.altitude_m);

    // Build FOFrame
    FOFrame fo{};
    fo.addr = d.addr;
    fo.Squawk = d.squawk;
    memcpy(fo.flight, d.flight, sizeof(fo.flight));
    fo.altitude = d.geo_alt_m;
    fo.pressure_altitude = d.altitude_m;
    fo.speed = d.speed_kmh;
    fo.course = d.track_deg;
    fo.vert_rate = d.vert_rate_fpm; // can be converted if needed
    fo.latitude = d.lat;
    fo.longitude = d.lon;
    fo.seen = d.seen_time_ms;
    fo.timestamp = d.timestamp;
    fo.signal_source = 1;
    fo.aircraft_type = 9;

    out_enqueue(fo);
  }
}

static void flush_out_queue(){
  FOFrame fo;
  while (out_dequeue(fo)){
    send_to_esp32(fo);
  }
}

static void read_rssi(){
  // Use ADC on GPIO26
  uint16_t v = analogRead(RSSI_PIN);
  float f = (float)v / 4095.0f; // 12-bit ADC
  rssi_ema = (1.0f - rssi_alpha)*rssi_ema + rssi_alpha * f;
}

static void cli_handle_line(const String &line){
  if (line.equalsIgnoreCase("SHOW")){
    Config cfg = get_config();
    Serial.println(cfg.toString());
  } else if (line.startsWith("PROFILE")){
    String p = line.substring(7); p.trim();
    if (p.equalsIgnoreCase("Normal")) set_profile(Profile::Normal);
    else if (p.equalsIgnoreCase("High-EMI")) set_profile(Profile::HighEMI);
    else if (p.equalsIgnoreCase("Urban")) set_profile(Profile::Urban);
    else if (p.equalsIgnoreCase("Remote")) set_profile(Profile::Remote);
    Serial.println(F("OK"));
  } else if (line.equalsIgnoreCase("FILTER ON")){
    filter_enabled = true; Serial.println(F("OK"));
  } else if (line.equalsIgnoreCase("FILTER OFF"))
  {
    filter_enabled = false; Serial.println(F("OK"));
  } else if (line.equalsIgnoreCase("SAVE")){
    save_config(); Serial.println(F("SAVED"));
  } else if (line.equalsIgnoreCase("LOAD")){
    load_config(); Serial.println(F("LOADED"));
  } else {
    Serial.println(F("Unknown command"));
  }
}




void setup()
{
  // USB Serial
  Serial.begin(USB_BAUD);
  unsigned long t0 = millis();
  while (!Serial && !Serial.dtr() && (millis() - t0) < 8000) delay(10);
  delay(2000);

  // Serial2 to ESP32S3
  Serial2.setTX(UART2_TX);
  Serial2.setRX(UART2_RX);
  Serial2.begin(UART2_BAUD);

  // Pins
  pin_fast_out(CORE0_TOGGLE_PIN);
  pin_fast_in(RSSI_PIN);

  // ADC init for RSSI
  analogReadResolution(12);

  // LittleFS
  LittleFS.begin();
  load_config(); // autoload on start

  // CPR context init (set receiver reference for local CPR)
  cpr_ctx.init(55.7558, 37.6176); // default Moscow center; can be updated via config

  Serial.println(F("RP2040 ADS-B RX start"));
}


// --------------- Core1 entry: sampling/indicators ---------------
void setup1() 
{
    // Toggle pin setup
    pin_fast_out(CORE1_TOGGLE_PIN);

    // Preamble indicator pins
    for (int i = 0; i < 3; i++) pin_fast_out(PRE_IND_PIN[i]);

    // Allocate ring buffers
    for (int i = 0; i < 3; i++) 
    {
        void* buf = malloc(RING_WORDS * sizeof(uint32_t));
        ringbuf[i] = (volatile uint32_t*)buf;
        write_idx[i] = 0; read_idx[i] = 0;
    }

    // Setup PIO SMs and DMA
    for (int i = 0; i < 3; i++)
    {
        setup_pio_sm(i, CH_PIN[i]);
        setup_dma_for_sm(i);
    }
}


void loop()
{
  // Toggle pin every 1000 ms
  if (millis() - core0_last_toggle >= 1000) 
  {
    core0_last_toggle = millis();
    static bool lv=false; lv=!lv; gpio_write(CORE0_TOGGLE_PIN, lv);
  }

  // CLI
  static String l;
  while (Serial.available())
  {
    char c = (char)Serial.read();
    if (c=='\n' || c=='\r')
    {
      if (l.length()>0) { cli_handle_line(l); l=""; }
    } else l += c;
  }

  // RSSI
  read_rssi();

  // Process and forward
  process_packets();
  flush_out_queue();
}


void loop1()
{
    // Toggle every 500 ms
 /*   uint32_t now = millis();
    if (now - core1_last_toggle >= 500) 
    {
        core1_last_toggle = now;
        static bool lv = false; lv = !lv; gpio_write(CORE1_TOGGLE_PIN, lv);
    }*/

    // Run channel workers
    channel_worker(0); // each is a while(true) loop; arrange time-slicing by allowing tight_loop_contents
    //channel_worker(1);
    //channel_worker(2);
}

// --------------- Core0: processing, CLI, forwarding ---------------