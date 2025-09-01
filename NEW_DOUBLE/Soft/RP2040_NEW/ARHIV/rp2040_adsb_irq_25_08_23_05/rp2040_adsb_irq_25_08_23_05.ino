// rp2040_adsb_gpio_irq_nodma_v5_premain.ino
#include <Arduino.h>
#include <LittleFS.h>
#include <hardware/gpio.h>
#include <hardware/adc.h>
#include <pico/multicore.h>
#include "ringbuffer.h"
#include "adsb_decoder.h"
#include "utils.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_packet.h"

// Inputs: CH1=GPIO18, CH2=GPIO19, CH3=GPIO22
static constexpr uint8_t PIN_CH1 = 18, PIN_CH2 = 19, PIN_CH3 = 22;
// Preamble indicators: CH1->GPIO17, CH2->GPIO20, CH3->GPIO23
static constexpr uint8_t PIN_PRE1 = 17, PIN_PRE2 = 20, PIN_PRE3 = 23;
static constexpr uint8_t UART2_TX = 4, UART2_RX = 5;
static constexpr uint8_t PIN_BLINK_CORE0 = 15, PIN_BLINK_CORE1 = 25;
static constexpr uint8_t PIN_RSSI_ADC = 26; // ADC0

static LockFreePacketQueue pktq[3];
static Config g_cfg; static String cmd_line;


struct Stats
{
  uint32_t edges[3];
  uint32_t preambles[3];
  uint32_t msgs;
  uint32_t crc_ok;
  uint32_t crc_fix;
  uint32_t crc_fail;
} g_stat;

static float rssi_ema = 0.0f;
static uint32_t pre_sig_until[3] = {0, 0, 0};

static inline uint8_t pin_to_ch(uint gpio)
{
  return (gpio == PIN_CH1) ? 0 : (gpio == PIN_CH2) ? 1 : (gpio == PIN_CH3) ? 2 : 255;
}

static inline uint8_t ch_to_pre_pin(int ch)
{
  return (ch == 0) ? PIN_PRE1 : (ch == 1) ? PIN_PRE2 : PIN_PRE3;
}

static void gpio_irq_handler(uint gpio, uint32_t events)
{
  uint8_t ch = pin_to_ch(gpio);
  if (ch > 2) return;
  if (!(g_cfg.chmask & (1 << ch))) return;
  uint32_t t = time_us_32();
  uint8_t lvl = (events & GPIO_IRQ_EDGE_RISE) ? 1 : 0;
  if (g_cfg.invert[ch]) lvl ^= 1;
  uint16_t wi = g_edge[ch].widx; g_edge[ch].e[wi].t = t;
  g_edge[ch].e[wi].level = lvl;
  g_edge[ch].widx = (wi + 1) & (EDGE_BUF_SIZE - 1); g_stat.edges[ch]++;
}

void core1_entry()
{
  uint32_t last = time_us_32();
  adc_init();
  adc_gpio_init(PIN_RSSI_ADC);
  adc_select_input(0);
  while (true)
  {
    uint32_t t = time_us_32();
    if (t - last > 500000)
    {
      last = t;
      sio_hw->gpio_togl = (1u << PIN_BLINK_CORE1);
    }
    uint16_t raw = adc_read();
    rssi_ema = 0.9f * rssi_ema + 0.1f * (float)raw; tight_loop_contents();
  }
}


static uint32_t crc32_(const uint8_t* data, size_t len) 
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) 
    {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) 
        {
            crc = (crc >> 1) ^ (0xEDB88320u & -(int)(crc & 1));
        }
    }
    return ~crc;
}

static void send_to_esp32(const FlightOutput &fo)
{
  struct __attribute__((packed)) Frame {
    uint8_t sof[2];
    uint16_t len;
    FlightOutput fo;
    uint32_t crc;
  } f;
  f.sof[0] = 0x55;
  f.sof[1] = 0xAA;
  f.len = sizeof(FlightOutput);
  memcpy(&f.fo, &fo, sizeof(FlightOutput));
  f.crc = crc32_((uint8_t*)&f.fo, sizeof(FlightOutput));
  Serial2.write((uint8_t*)&f, sizeof(f));
}

static void print_line(const DecodedFrame &df, const FlightOutput &fo)
{
  switch (g_cfg.out_format)
  {
    case 1:
      Serial.print(df.icao, HEX);
      Serial.print(',');
      Serial.print(df.callsign);
      Serial.print(',');
      Serial.print(df.lat, 6);
      Serial.print(',');
      Serial.print(df.lon, 6);
      Serial.print(',');
      Serial.print(fo.altitude);
      Serial.print(',');
      Serial.print(fo.speed);
      Serial.print(',');
      Serial.print(fo.vert_rate);
      Serial.print(',');
      Serial.print(fo.course);
      Serial.print(',');
      Serial.println((int)rssi_ema);
      break;
    case 2:
      Serial.printf("{\"icao\":\"%06X\",\"cs\":\"%s\",\"lat\":%.6f,\"lon\":%.6f,\"alt\":%d,\"spd\":%d,\"vs\":%d,\"trk\":%d,\"rssi\":%d}\n", df.icao, df.callsign, df.lat, df.lon, fo.altitude, fo.speed, fo.vert_rate, fo.course, (int)rssi_ema);
      break;
    default:
      Serial.print("ICAO=");
      Serial_printHex(df.icao, 6);
      Serial.print(" cs=");
      Serial.print(df.callsign);
      Serial.print(" lat=");
      Serial.print(df.lat, 6);
      Serial.print(" lon=");
      Serial.print(df.lon, 6);
      Serial.print(" spd=");
      Serial.print(fo.speed);
      Serial.print(" alt=");
      Serial.print(fo.altitude);
      Serial.print(" vs=");
      Serial.print(fo.vert_rate);
      Serial.print(" trk=");
      Serial.print(fo.course);
      Serial.print(" rssi=");
      Serial.println((int)rssi_ema);
  }
}

static void emit_debug_raw(const RawMessage &rm)
{
  if (!g_cfg.output_raw) return;
  Serial.print("CH");
  Serial.print(rm.channel + 1);
  Serial.print(" ");
  Serial.print(rm.bits);
  Serial.print("b ");
  for (size_t i = 0; i < rm.bytes; i++)
  {
    if (i) Serial.print(' ');
    char buf[4]; sprintf(buf, "%02X", rm.payload[i]);
    Serial.print(buf);
  } Serial.println();
}

static void process_packets()
{
  RawMessage rm; DecodedFrame df;
  for (int i = 0; i < 3; i++)
  {
    while (pktq[i].pop(rm))
    {
      emit_debug_raw(rm);
      CrcStatus cs = check_and_fix_crc(rm);
      if (cs == CRC_OK) g_stat.crc_ok++;
      else if (cs == CRC_FIXED1) g_stat.crc_fix++;
      else g_stat.crc_fail++;
      if (decode_adsb_frame(rm, df, g_cfg))
      {
        g_stat.msgs++;
        FlightOutput fo = {0};
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
        fo.signal_source = 1;
        fo.aircraft_type = df.aircraft_type;
        print_line(df, fo);
        send_to_esp32(fo);
      }
    }
  }
}

static const char* profile_name(int p)
{
  switch (p)
  {
    case 1: return "High-EMI";
    case 2: return "Urban";
    case 3: return "Remote";
    default: return "Normal";
  }
}

static void apply_profile(Config &c)
{
  switch (c.profile)
  {
    case 1: c.dead_time_us = 8; c.tol_us = 1; c.min_pulses = 5; c.min_win_hits = 1; c.base_thr = 7.0f; c.ema_alpha = 0.1f; c.dyn_k = 1.0f; c.w_short_us = 0.4f; c.w_long_us = 0.7f;
      break;
    case 2: c.dead_time_us = 7; c.tol_us = 2; c.min_pulses = 5; c.min_win_hits = 1; c.base_thr = 6.5f; c.ema_alpha = 0.15f; c.dyn_k = 1.2f; c.w_short_us = 0.4f; c.w_long_us = 0.7f;
      break;
    case 3: c.dead_time_us = 6; c.tol_us = 3; c.min_pulses = 4; c.min_win_hits = 1; c.base_thr = 5.0f; c.ema_alpha = 0.08f; c.dyn_k = 0.8f; c.w_short_us = 0.4f; c.w_long_us = 0.7f;
      break;
    default: c.dead_time_us = 7; c.tol_us = 2; c.min_pulses = 5; c.min_win_hits = 1; c.base_thr = 6.0f; c.ema_alpha = 0.1f; c.dyn_k = 1.0f; c.w_short_us = 0.4f; c.w_long_us = 0.7f;
      break;
  }
}

static void handle_cmd(String u)
{
  u.trim();
  u.toUpperCase();
  if (u == "HELP")
  {
    Serial.println("Commands: HELP, SHOW, SAVE, LOAD, RAW ON|OFF, PROFILE <NORMAL|HIGH-EMI|URBAN|REMOTE>, INV <CH1|CH2|CH3> <ON|OFF>, REF <lat> <lon>, THR <val>, TOL <us>, WND <short_us> <long_us>, CHMASK <mask>, FORMAT <LOG|CSV|JSON>");
    return;
  }
  if (u == "SHOW")
  {
    Serial.print("Profile=");
    Serial.print(profile_name(g_cfg.profile));
    Serial.print(" RAW=");
    Serial.print(g_cfg.output_raw ? 1 : 0);
    Serial.print(" INV=");
    for (int i = 0; i < 3; i++)
    {
      Serial.print(g_cfg.invert[i] ? "1" : "0");
      if (i < 2) Serial.print(',');
    }
    Serial.print(" REF=");
    Serial.print(g_cfg.ref_lat, 6);
    Serial.print(",");
    Serial.print(g_cfg.ref_lon, 6);
    Serial.print(" THR=");
    Serial.print(g_cfg.base_thr, 2);
    Serial.print(" TOL=");
    Serial.print(g_cfg.tol_us);
    Serial.print(" W=[");
    Serial.print(g_cfg.w_short_us, 2);
    Serial.print(",");
    Serial.print(g_cfg.w_long_us, 2);
    Serial.print("] CHMASK=");
    Serial.print(g_cfg.chmask, HEX);
    Serial.print(" FORMAT=");
    Serial.println(g_cfg.out_format);
    Serial.printf("Edges: ch1=%lu ch2=%lu ch3=%lu preambles: ch1=%lu ch2=%lu ch3=%lu msgs=%lu crc_ok=%lu fix=%lu fail=%lu\n", g_stat.edges[0], g_stat.edges[1], g_stat.edges[2], g_stat.preambles[0], g_stat.preambles[1], g_stat.preambles[2], g_stat.msgs, g_stat.crc_ok, g_stat.crc_fix, g_stat.crc_fail);
    return;
  }
  if (u == "SAVE")
  {
    File f = LittleFS.open("/config.txt", "w");
    if (f)
    {
      f.printf("profile=%d\nraw=%d\ninv=%d,%d,%d\nref_lat=%f\nref_lon=%f\nthr=%f\ntol=%d\nwnd=%f,%f\nchmask=%u\nfmt=%d\n", g_cfg.profile, g_cfg.output_raw ? 1 : 0, g_cfg.invert[0] ? 1 : 0, g_cfg.invert[1] ? 1 : 0, g_cfg.invert[2] ? 1 : 0, g_cfg.ref_lat, g_cfg.ref_lon, g_cfg.base_thr, g_cfg.tol_us, g_cfg.w_short_us, g_cfg.w_long_us, g_cfg.chmask, g_cfg.out_format); f.close(); Serial.println("Saved");
    }
    return;
  }
  if (u == "LOAD")
  {
    if (LittleFS.exists("/config.txt"))
    {
      File f = LittleFS.open("/config.txt", "r");
      if (f)
      { while (f.available())
        {
          String line = f.readStringUntil('\n');
          line.trim(); if (!line.length() || line[0] == '#') continue;
          int eq = line.indexOf('=');
          if (eq < 0) continue;
          String k = line.substring(0, eq), v = line.substring(eq + 1);
          k.trim(); v.trim(); if (k == "profile") g_cfg.profile = v.toInt();
          else if (k == "raw") g_cfg.output_raw = (v == "1");
          else if (k == "inv") {
            int a, b, c;
            if (sscanf(v.c_str(), "%d,%d,%d", &a, &b, &c) == 3)
            {
              g_cfg.invert[0] = a;
              g_cfg.invert[1] = b;
              g_cfg.invert[2] = c;
            }
          }
          else if (k == "ref_lat") g_cfg.ref_lat = v.toFloat();
          else if (k == "ref_lon") g_cfg.ref_lon = v.toFloat();
          else if (k == "thr") g_cfg.base_thr = v.toFloat();
          else if (k == "tol") g_cfg.tol_us = v.toInt();
          else if (k == "wnd") {
            float a, b;
            if (sscanf(v.c_str(), "%f,%f", &a, &b) == 2)
            {
              g_cfg.w_short_us = a; g_cfg.w_long_us = b;
            }
          }
          else if (k == "chmask") g_cfg.chmask = (uint8_t)v.toInt();
          else if (k == "fmt") g_cfg.out_format = v.toInt();
        }
        f.close();
      }
      apply_profile(g_cfg);
    } 
   /* apply_profile(g_cfg);*/
    Serial.println("Loaded");
    return;
  }
  if (u == "RAW ON")
  {
    g_cfg.output_raw = true;
    Serial.println("RAW=ON");
    return;
  }
  if (u == "RAW OFF")
  {
    g_cfg.output_raw = false;
    Serial.println("RAW=OFF");
    return;
  }
  if (u.startsWith("PROFILE "))
  {
    String p = u.substring(8);
    p.trim();
    if (p == "NORMAL") g_cfg.profile = 0;
    else if (p == "HIGH-EMI") g_cfg.profile = 1;
    else if (p == "URBAN") g_cfg.profile = 2;
    else if (p == "REMOTE") g_cfg.profile = 3;
    else
    {
      Serial.println("Bad profile");
      return;
    }
    apply_profile(g_cfg);
    Serial.print("Profile set: ");
    Serial.println(profile_name(g_cfg.profile));
    return;
  }
  if (u.startsWith("INV "))
  {
    String rest = u.substring(4);
    rest.trim();
    int ch = -1;
    if (rest.startsWith("CH1")) ch = 0;
    else if (rest.startsWith("CH2")) ch = 1;
    else if (rest.startsWith("CH3")) ch = 2; int sp = rest.indexOf(' ');
    if (ch < 0 || sp < 0)
    {
      Serial.println("INV usage: INV CH1|CH2|CH3 ON|OFF");
      return;
    }
    String st = rest.substring(sp + 1); st.trim();
    bool on = (st == "ON");
    g_cfg.invert[ch] = on;
    Serial.print("Invert CH");
    Serial.print(ch + 1);
    Serial.print("=");
    Serial.println(on ? "ON" : "OFF");
    return;
  }
  if (u.startsWith("REF "))
  {
    float a, b;
    if (sscanf(u.c_str() + 4, "%f %f", &a, &b) == 2)
    {
      g_cfg.ref_lat = a;
      g_cfg.ref_lon = b;
      Serial.print("REF set: ");
      Serial.print(a, 6);
      Serial.print(",");
      Serial.println(b, 6);
    }
    else Serial.println("REF usage: REF <lat> <lon>");
    return;
  }
  if (u.startsWith("THR "))
  {
    float v;
    if (sscanf(u.c_str() + 4, "%f", &v) == 1)
    {
      g_cfg.base_thr = v;
      Serial.print("THR set: ");
      Serial.println(v, 2);
    }
    else Serial.println("THR usage: THR <value>");
    return;
  }
  if (u.startsWith("TOL "))
  {
    int v;
    if (sscanf(u.c_str() + 4, "%d", &v) == 1)
    {
      g_cfg.tol_us = v;
      Serial.print("TOL set: ");
      Serial.println(v);
    }
    else Serial.println("TOL usage: TOL <us>");
    return;
  }
  if (u.startsWith("WND "))
  {
    float a, b;
    if (sscanf(u.c_str() + 4, "%f %f", &a, &b) == 2)
    {
      g_cfg.w_short_us = a;
      g_cfg.w_long_us = b;
      Serial.print("WND set: ");
      Serial.print(a, 2);
      Serial.print(",");
      Serial.println(b, 2);
    }
    else Serial.println("WND usage: WND <short_us> <long_us>");
    return;
  }
  if (u.startsWith("CHMASK "))
  {
    int v;
    if (sscanf(u.c_str() + 7, "%d", &v) == 1)
    {
      g_cfg.chmask = (uint8_t)v;
      Serial.print("CHMASK=");
      Serial.println(g_cfg.chmask, HEX);
    }
    else Serial.println("CHMASK usage: CHMASK <mask> (1=CH1,2=CH2,4=CH3)");
    return;
  }
  if (u.startsWith("FORMAT "))
  {
    String f = u.substring(7);
    f.trim();
    if (f == "LOG") g_cfg.out_format = 0;
    else if (f == "CSV") g_cfg.out_format = 1;
    else if (f == "JSON") g_cfg.out_format = 2;
    else {
      Serial.println("Bad format");
      return;
    }
    Serial.print("FORMAT set: ");
    Serial.println(g_cfg.out_format);
    return;
  } Serial.println("Unknown");
}

void setup()
{
  Serial.begin(115200);
  unsigned long t0 = millis();
  while (!Serial && !Serial.dtr() && (millis() - t0) < 8000) delay(10);
  delay(2000);
  Serial.println("GPIO IRQ no-DMA v5 preamble-ind start");
  LittleFS.begin();
  g_cfg.profile = 0;
  g_cfg.output_raw = true;
  g_cfg.out_format = 0;
  g_cfg.invert[0] = g_cfg.invert[1] = g_cfg.invert[2] = false;
  g_cfg.ref_lat = 55.93574f;
  g_cfg.ref_lon = 37.34873f;
  g_cfg.chmask = 0x07;
  g_cfg.rssi_alpha = 0.1f;
  g_cfg.rssi_k = 0.002f;
  apply_profile(g_cfg);

  gpio_init(PIN_BLINK_CORE0);
  gpio_set_dir(PIN_BLINK_CORE0, GPIO_OUT);
  gpio_put(PIN_BLINK_CORE0, 0);
  gpio_init(PIN_BLINK_CORE1);
  gpio_set_dir(PIN_BLINK_CORE1, GPIO_OUT);
  gpio_put(PIN_BLINK_CORE1, 0);

  // Preamble indicator pins
  gpio_init(PIN_PRE1);
  gpio_set_dir(PIN_PRE1, GPIO_OUT);
  gpio_put(PIN_PRE1, 0);
  gpio_init(PIN_PRE2);
  gpio_set_dir(PIN_PRE2, GPIO_OUT);
  gpio_put(PIN_PRE2, 0);
  gpio_init(PIN_PRE3);
  gpio_set_dir(PIN_PRE3, GPIO_OUT);
  gpio_put(PIN_PRE3, 0);

  Serial2.setTX(UART2_TX);
  Serial2.setRX(UART2_RX);
  Serial2.begin(921600);

  for (int i = 0; i < 3; i++) pktq[i].begin(128);
  // Inputs with pull-down
  gpio_init(PIN_CH1);
  gpio_set_dir(PIN_CH1, GPIO_IN);
  gpio_pull_down(PIN_CH1);
  gpio_init(PIN_CH2);
  gpio_set_dir(PIN_CH2, GPIO_IN);
  gpio_pull_down(PIN_CH2);
  gpio_init(PIN_CH3);
  gpio_set_dir(PIN_CH3, GPIO_IN);
  gpio_pull_down(PIN_CH3);
  // IRQs
  gpio_set_irq_enabled_with_callback(PIN_CH1, GPIO_IRQ_EDGE_RISE /*| GPIO_IRQ_EDGE_FALL*/, true, &gpio_irq_handler);
  gpio_set_irq_enabled(PIN_CH2, GPIO_IRQ_EDGE_RISE /*| GPIO_IRQ_EDGE_FALL*/, true);
  gpio_set_irq_enabled(PIN_CH3, GPIO_IRQ_EDGE_RISE /*| GPIO_IRQ_EDGE_FALL*/, true);
  multicore_launch_core1(core1_entry);
  Serial.println("Setup done");
}

static void pre_sig_pulse(int ch)
{
  uint8_t pin = ch_to_pre_pin(ch);
  sio_hw->gpio_set = (1u << pin);
  pre_sig_until[ch] = time_us_32() + 200;
}

static void handle_pre_sig_off()
{
  uint32_t now = time_us_32();
  for (int i = 0; i < 3; i++)
  {
    if (pre_sig_until[i] && (int32_t)(now - pre_sig_until[i]) >= 0)
    {
      uint8_t pin = ch_to_pre_pin(i);
      sio_hw->gpio_clr = (1u << pin);
      pre_sig_until[i] = 0;
    }
  }
}

static uint32_t last_blink0 = 0;

void loop()
{
  uint32_t now = millis();
  if (now - last_blink0 > 1000)
  {
    last_blink0 = now;
    sio_hw->gpio_togl = (1u << PIN_BLINK_CORE0);
  }
  for (int i = 0; i < 3; i++)
  {
    if (!(g_cfg.chmask & (1 << i))) continue;
    int r = run_edge_correlator_and_slice(i, g_cfg, pktq[i]);

    if (r) 
    {
        g_stat.preambles[i]++; 
        pre_sig_pulse(i); 
    }

    //if (r & 0x2)
    //{
    //  g_stat.preambles[i]++; pre_sig_pulse(i);
    //}
  }
  handle_pre_sig_off();
  process_packets();

  while (Serial.available())
  {
    char c = Serial.read();
    if (c == '\n' || c == '\r')
    {
      if (cmd_line.length())
      {
        handle_cmd(cmd_line);
        cmd_line = "";
      }
    }
    else cmd_line += c;
  }
}
