// rp2040_adsb_receiver.ino
// ADS-B receiver for RP2040 (Arduino core)
// Requirements: 3-channel PIO sampling -> DMA ring -> core1; decoding on core0
// Serial USB at 115200; Serial2 UART TX=4 RX=5 at 921600 to ESP32S3
// Preamble indicators: ch1=17, ch2=20, ch3=23; Receiver inputs: ch1=18, ch2=19, ch3=22
// RSSI on ADC pin 26. Blink pin15 on core0 (1000ms), pin25 on core1 (500ms)
// LittleFS config with profiles Normal/High-EMI/Urban/Remote

#include <Arduino.h>
#include <LittleFS.h>
#include <hardware/pio.h>
#include <hardware/dma.h>
#include <hardware/regs/dreq.h>
#include <pico/multicore.h>
#include <hardware/sync.h>
#include "adsb_sampler.pio.h"
#include "ringbuffer.h"
#include "adsb_decoder.h"
#include "config.h"
#include "esp_packet.h"
#include "utils.h"

// Pin map
static constexpr uint8_t PIN_RX1 = 18; // ch1
static constexpr uint8_t PIN_RX2 = 19; // ch2
static constexpr uint8_t PIN_RX3 = 22; // ch3
static constexpr uint8_t PIN_PRE1 = 17; // preamble indicator ch1
static constexpr uint8_t PIN_PRE2 = 20; // preamble indicator ch2
static constexpr uint8_t PIN_PRE3 = 23; // preamble indicator ch3
static constexpr uint8_t PIN_RSSI_ADC = 26; // ADC0
static constexpr uint8_t PIN_BLINK_CORE0 = 15; // 1s toggle
static constexpr uint8_t PIN_BLINK_CORE1 = 25; // 0.5s toggle

// Serial2 pins
static constexpr uint8_t UART2_TX = 4;
static constexpr uint8_t UART2_RX = 5;

// Sampling params
// 8us preamble, 80 samples -> 10MHz. We'll sample at 20MHz to improve timing and filtering.
static constexpr uint32_t SAMPLE_FREQ_HZ = 20000000; // 20 MHz
static constexpr uint32_t WORD_SAMPLES = 32; // one PIO push holds 32 samples

// DMA ring size (32-bit words per channel)
static constexpr uint32_t DMA_RING_WORDS = 1024; // as required

// Queues for detected packets (raw)
static constexpr size_t PACKET_QUEUE_CAP = 64;

// Global state
struct PioDmaChannel {
  PIO pio;
  uint sm;
  int dma_chan;
  volatile uint32_t *ring;
  uint32_t ring_words; // elements in ring
  volatile uint32_t widx; // DMA write index (advance by DMA); we will track read index separately
  volatile uint32_t ridx; // CPU read index
  uint dreq;
  uint pin;
  uint pre_pin; // indicator
};

static PioDmaChannel ch[3];

// Packet queue per channel
static LockFreePacketQueue pktq[3];

// RSSI
static volatile uint16_t rssi_adc_last = 0;

// Profile/config
static Config g_cfg;

// Command line
static String cmd_line;

// Forward decl
void core1_entry();
static void setup_pio_dma_channel(PioDmaChannel &c, PIO pio, uint sm, uint pin_in, uint pre_pin, int8_t pio_prog_offset);
static void rearm_dma(PioDmaChannel &c);
static bool try_detect_and_slice(PioDmaChannel &c, int ch_index, uint32_t now_ticks);
static void handle_usb_command(const String &cmd);
static void send_to_esp32(const FlightOutput &fo);
static void process_packets_loop();

void setup() {
  // USB Serial
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}
  Serial.println("RP2040 ADS-B RX starting...");

  // LittleFS
  LittleFS.begin();
  load_config(g_cfg);
  Serial.print("Profile: "); Serial.println(profile_name(g_cfg.profile));

  // GPIO init
  gpio_init(PIN_PRE1); gpio_set_dir(PIN_PRE1, GPIO_OUT); gpio_put(PIN_PRE1, 0);
  gpio_init(PIN_PRE2); gpio_set_dir(PIN_PRE2, GPIO_OUT); gpio_put(PIN_PRE2, 0);
  gpio_init(PIN_PRE3); gpio_set_dir(PIN_PRE3, GPIO_OUT); gpio_put(PIN_PRE3, 0);
  gpio_init(PIN_BLINK_CORE0); gpio_set_dir(PIN_BLINK_CORE0, GPIO_OUT); gpio_put(PIN_BLINK_CORE0, 0);
  gpio_init(PIN_BLINK_CORE1); gpio_set_dir(PIN_BLINK_CORE1, GPIO_OUT); gpio_put(PIN_BLINK_CORE1, 0);

  // ADC for RSSI
  analogReadResolution(12);
  analogRead(PIN_RSSI_ADC); // prime

  // UART2 to ESP32S3
  Serial2.setTX(UART2_TX);
  Serial2.setRX(UART2_RX);
  Serial2.begin(921600);

  // Prepare packet queues
  for (int i=0;i<3;i++) pktq[i].begin(PACKET_QUEUE_CAP);

  // Setup PIO programs (3 state machines)
  PIO pio0 = pio0_hw; PIO pio1 = pio1_hw;
  int offset0 = -1; int offset1 = -1;
  // Load program into pio0 and pio1 for flexibility
  offset0 = pio_add_program(pio0, &adsb_sampler_program);
  offset1 = pio_add_program(pio1, &adsb_sampler_program);

  // Configure three channels, try to distribute SM across PIOs
  setup_pio_dma_channel(ch[0], pio0, 0, PIN_RX1, PIN_PRE1, offset0);
  setup_pio_dma_channel(ch[1], pio0, 1, PIN_RX2, PIN_PRE2, offset0);
  setup_pio_dma_channel(ch[2], pio1, 0, PIN_RX3, PIN_PRE3, offset1);

  // Launch core1 for reception/aggregation
  multicore_launch_core1(core1_entry);

  Serial.println("Setup complete. Type HELP for commands.");
}

static uint32_t last_blink0 = 0;

void loop() {
  // Blink on core0 each 1000ms
  uint32_t now = millis();
  if (now - last_blink0 >= 1000) {
    last_blink0 = now;
    sio_hw->gpio_togl = (1u << PIN_BLINK_CORE0);
  }

  // Handle serial commands
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (cmd_line.length()) {
        handle_usb_command(cmd_line);
        cmd_line = "";
      }
    } else {
      cmd_line += c;
      if (cmd_line.length() > 128) cmd_line = cmd_line.substring(0, 128);
    }
  }

  // Process packets decoded by core1 -> queued for core0 decode
  process_packets_loop();
}

static void setup_pio_dma_channel(PioDmaChannel &c, PIO pio, uint sm, uint pin_in, uint pre_pin, int8_t prog_offset) {
  c.pio = pio; c.sm = sm; c.pin = pin_in; c.pre_pin = pre_pin; c.ring_words = DMA_RING_WORDS; c.ridx = 0; c.widx = 0;
  // Allocate ring buffer
  c.ring = (volatile uint32_t*)malloc(sizeof(uint32_t)*DMA_RING_WORDS);
  memset((void*)c.ring, 0, sizeof(uint32_t)*DMA_RING_WORDS);

  // Init PIO state machine
  adsb_sampler_program_init(pio, sm, prog_offset, pin_in, SAMPLE_FREQ_HZ);

  // Determine DREQ
  uint dreq;
  if (pio == pio0) {
    if (sm == 0) dreq = DREQ_PIO0_RX0; else if (sm == 1) dreq = DREQ_PIO0_RX1; else if (sm==2) dreq = DREQ_PIO0_RX2; else dreq = DREQ_PIO0_RX3;
  } else {
    if (sm == 0) dreq = DREQ_PIO1_RX0; else if (sm == 1) dreq = DREQ_PIO1_RX1; else if (sm==2) dreq = DREQ_PIO1_RX2; else dreq = DREQ_PIO1_RX3;
  }
  c.dreq = dreq;

  // DMA setup
  int dma_chan = dma_claim_unused_channel(true);
  c.dma_chan = dma_chan;
  dma_channel_config cfg = dma_channel_get_default_config(dma_chan);
  channel_config_set_read_increment(&cfg, false);
  channel_config_set_write_increment(&cfg, true);
  channel_config_set_dreq(&cfg, dreq);
  channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
  // Configure ring write address wrapping
  channel_config_set_ring(&cfg, true, __builtin_ctz(DMA_RING_WORDS)); // wrap by ring size (must be power of 2)

  dma_channel_configure(
    dma_chan,
    &cfg,
    (void*)c.ring,                // dst
    (const void*)&pio->rxf[sm],   // src
    0xFFFFFFFF,                   // transfer count: continuous
    false
  );

  // Set initial write address and start
  dma_hw->ch[dma_chan].al3_write_addr = (uintptr_t)c.ring;
  dma_hw->ch[dma_chan].al3_trans_count_trig = 0xFFFFFFFF; // continuous
}

void core1_entry() {
  // Start DMA channels
  for (int i=0;i<3;i++) {
    dma_channel_start(ch[i].dma_chan);
  }
  uint32_t last_blink = time_us_32();

  // EMA noise per channel
  CorrState corr[3];
  for (int i=0;i<3;i++) init_corr_state(corr[i], g_cfg);

  while (true) {
    // Blink pin 25 at 500ms
    uint32_t t = time_us_32();
    if ((t - last_blink) >= 500000) {
      last_blink = t; sio_hw->gpio_togl = (1u << PIN_BLINK_CORE1);
    }

    // Periodically sample RSSI
    rssi_adc_last = analogRead(PIN_RSSI_ADC);

    // For each channel, attempt detection on new data
    for (int i=0;i<3;i++) {
      // Use DMA write address to infer producer index
      uint32_t waddr = dma_hw->ch[ch[i].dma_chan].write_addr;
      uint32_t baseaddr = (uint32_t)(uintptr_t)ch[i].ring;
      uint32_t widx = ((waddr - baseaddr) >> 2) & (DMA_RING_WORDS - 1);
      ch[i].widx = widx;

      // Process available words
      while (ch[i].ridx != ch[i].widx) {
        // Fetch one word of 32 samples into a staging buffer the correlator uses internally
        uint32_t word = ch[i].ring[ch[i].ridx];
        ch[i].ridx = (ch[i].ridx + 1) & (DMA_RING_WORDS - 1);
        push_samples(i, word); // push 32 samples into channel i sample FIFO
      }

      // Try detect preamble and slice packets
      bool got = try_detect_and_slice(ch[i], i, t);
      if (got) {
        // blink indicator quickly
        sio_hw->gpio_set = (1u << ch[i].pre_pin);
        // leave it on for a very short time; will be turned off by decay
      }
      // decay indicator
      static uint32_t pre_decay[3] = {0,0,0};
      if (sio_hw->gpio_out & (1u << ch[i].pre_pin)) {
        if (t - pre_decay[i] > 100) { sio_hw->gpio_clr = (1u << ch[i].pre_pin); pre_decay[i] = t; }
      } else {
        pre_decay[i] = t;
      }
    }
  }
}

// Detection and slicing wrapper
static bool try_detect_and_slice(PioDmaChannel &c, int ch_index, uint32_t now_ticks) {
  // The correlator works on per-channel sample FIFO fed by push_samples()
  // It returns candidate messages (raw bits and metadata) via pktq[ch_index]
  int new_msgs = run_correlator_and_slice(ch_index, g_cfg, pktq[ch_index]);
  return new_msgs > 0;
}

static void handle_usb_command(const String &cmd) {
  String u = cmd; u.trim(); u.toUpperCase();
  if (u == "HELP") {
    Serial.println("Commands: HELP, SHOW, SAVE, LOAD, PROFILE <NORMAL|HIGH-EMI|URBAN|REMOTE>, RAW ON|OFF");
    return;
  }
  if (u == "SHOW") { print_config(g_cfg); return; }
  if (u == "SAVE") { save_config(g_cfg); Serial.println("Saved to /config.txt"); return; }
  if (u == "LOAD") { load_config(g_cfg); Serial.println("Loaded from /config.txt"); return; }
  if (u.startsWith("PROFILE ")) {
    String p = u.substring(8); p.trim();
    if (p == "NORMAL") g_cfg.profile = Profile::Normal; else if (p=="HIGH-EMI") g_cfg.profile = Profile::HighEMI; else if (p=="URBAN") g_cfg.profile=Profile::Urban; else if (p=="REMOTE") g_cfg.profile=Profile::Remote; else { Serial.println("Bad profile"); return; }
    Serial.print("Profile set: "); Serial.println(profile_name(g_cfg.profile));
    return;
  }
  if (u == "RAW ON") { g_cfg.output_raw = true; Serial.println("RAW=ON"); return; }
  if (u == "RAW OFF") { g_cfg.output_raw = false; Serial.println("RAW=OFF"); return; }
  Serial.println("Unknown command");
}

static void send_to_esp32(const FlightOutput &fo) {
  // Build framed packet with CRC32
  struct __attribute__((packed)) Frame {
    uint8_t sof[2];
    uint16_t len;
    FlightOutput fo;
    uint32_t crc;
  } f;
  f.sof[0] = 0x55; f.sof[1] = 0xAA;
  f.len = sizeof(FlightOutput);
  memcpy(&f.fo, &fo, sizeof(FlightOutput));
  f.crc = crc32((uint8_t*)&f.fo, sizeof(FlightOutput));
  Serial2.write((uint8_t*)&f, sizeof(f));
}

static void emit_debug_raw(const RawMessage &rm) {
  Serial.print("CH"); Serial.print(rm.channel+1);
  Serial.print(" ");
  Serial.print(rm.bits); Serial.print("b ");
  for (size_t i=0;i<rm.bytes;i++) {
    if (i) Serial.print(' ');
    char buf[4]; sprintf(buf, "%02X", rm.payload[i]); Serial.print(buf);
  }
  Serial.println();
}

static void process_packets_loop() {
  RawMessage rm;
  DecodedFrame df;
  while (pktq[0].pop(rm) || pktq[1].pop(rm) || pktq[2].pop(rm)) {
    // Output RAW before CRC as requested
    if (g_cfg.output_raw) emit_debug_raw(rm);

    // CRC check and single-bit fix
    CrcStatus cs = check_and_fix_crc(rm);

    // Decode ADS-B/Mode-S
    if (decode_adsb_frame(rm, df, g_cfg)) {
      // Build FlightOutput for ESP32 and also print to USB
      FlightOutput fo = {};
      fo.addr = df.icao;
      fo.Squawk = df.squawk;
      memcpy(fo.flight, df.callsign, sizeof(fo.flight));
      fo.altitude = df.altitude_m_geo;
      fo.pressure_altitude = df.altitude_m_baro;
      fo.speed = df.speed_kmh;
      fo.course = df.track_deg;
      fo.vert_rate = df.vert_rate_mpm;
      fo.latitude = df.lat;
      fo.longitude = df.lon;
      fo.seen = df.seen_time_ms;
      fo.timestamp = millis();
      fo.signal_source = 1; // DUMP1090-like source id
      fo.aircraft_type = df.aircraft_type;

      // Print to USB succinctly
      Serial.print("ICAO="); Serial.printHex(df.icao, 6);
      Serial.print(" flight="); Serial.print(df.callsign);
      Serial.print(" lat="); Serial.print(df.lat, 6);
      Serial.print(" lon="); Serial.print(df.lon, 6);
      Serial.print(" spd="); Serial.print(fo.speed);
      Serial.print(" alt="); Serial.print(fo.altitude);
      Serial.print(" vs="); Serial.print(fo.vert_rate);
      Serial.print(" trk="); Serial.print(fo.course);
      Serial.println();

      // Send to ESP32
      send_to_esp32(fo);
    }
  }
}
