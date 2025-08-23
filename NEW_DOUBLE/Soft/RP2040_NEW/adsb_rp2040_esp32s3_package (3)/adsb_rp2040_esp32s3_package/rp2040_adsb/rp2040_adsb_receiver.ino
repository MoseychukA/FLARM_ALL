// rp2040_adsb_receiver.ino
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

static constexpr uint8_t PIN_RX1 = 18; static constexpr uint8_t PIN_RX2 = 19; static constexpr uint8_t PIN_RX3 = 22;
static constexpr uint8_t PIN_PRE1 = 17; static constexpr uint8_t PIN_PRE2 = 20; static constexpr uint8_t PIN_PRE3 = 23;
static constexpr uint8_t PIN_RSSI_ADC = 26; static constexpr uint8_t PIN_BLINK_CORE0 = 15; static constexpr uint8_t PIN_BLINK_CORE1 = 25;
static constexpr uint8_t UART2_TX = 4; static constexpr uint8_t UART2_RX = 5;
static constexpr uint32_t SAMPLE_FREQ_HZ = 20000000; // 20 MHz
static constexpr uint32_t DMA_RING_WORDS = 1024;
static constexpr size_t PACKET_QUEUE_CAP = 64;

struct PioDmaChannel { PIO pio; uint sm; int dma_chan; volatile uint32_t *ring; uint32_t ring_words; volatile uint32_t widx; volatile uint32_t ridx; uint dreq; uint pin; uint pre_pin; };
static PioDmaChannel chn[3];
static LockFreePacketQueue pktq[3];
static volatile uint16_t rssi_adc_last = 0;
static Config g_cfg; static String cmd_line;
void core1_entry();
static void setup_pio_dma_channel(PioDmaChannel &c, PIO pio, uint sm, uint pin_in, uint pre_pin, int8_t pio_prog_offset);
static bool try_detect_and_slice(PioDmaChannel &c, int ch_index, uint32_t now_ticks);
static void handle_usb_command(const String &cmd);
static void send_to_esp32(const FlightOutput &fo);
static void process_packets_loop();

void setup(){
  Serial.begin(115200); while(!Serial && millis()<2000){}
  Serial.println("RP2040 ADS-B RX starting...");
  LittleFS.begin(); load_config(g_cfg); Serial.print("Profile: "); Serial.println(profile_name(g_cfg.profile));
  gpio_init(PIN_PRE1); gpio_set_dir(PIN_PRE1, GPIO_OUT); gpio_put(PIN_PRE1, 0);
  gpio_init(PIN_PRE2); gpio_set_dir(PIN_PRE2, GPIO_OUT); gpio_put(PIN_PRE2, 0);
  gpio_init(PIN_PRE3); gpio_set_dir(PIN_PRE3, GPIO_OUT); gpio_put(PIN_PRE3, 0);
  gpio_init(PIN_BLINK_CORE0); gpio_set_dir(PIN_BLINK_CORE0, GPIO_OUT); gpio_put(PIN_BLINK_CORE0, 0);
  gpio_init(PIN_BLINK_CORE1); gpio_set_dir(PIN_BLINK_CORE1, GPIO_OUT); gpio_put(PIN_BLINK_CORE1, 0);
  analogReadResolution(12); analogRead(PIN_RSSI_ADC);
  Serial2.setTX(UART2_TX); Serial2.setRX(UART2_RX); Serial2.begin(921600);
  for (int i=0;i<3;i++) pktq[i].begin(PACKET_QUEUE_CAP);
  PIO p0 = pio0; PIO p1 = pio1; int off0 = pio_add_program(p0, &adsb_sampler_program); int off1 = pio_add_program(p1, &adsb_sampler_program);
  setup_pio_dma_channel(chn[0], p0, 0, PIN_RX1, PIN_PRE1, off0);
  setup_pio_dma_channel(chn[1], p0, 1, PIN_RX2, PIN_PRE2, off0);
  setup_pio_dma_channel(chn[2], p1, 0, PIN_RX3, PIN_PRE3, off1);
  multicore_launch_core1(core1_entry);
  Serial.println("Setup complete. Type HELP for commands.");
}

static uint32_t last_blink0 = 0;
void loop(){
  uint32_t now = millis(); if (now - last_blink0 >= 1000) { last_blink0 = now; sio_hw->gpio_togl = (1u<<PIN_BLINK_CORE0); }
  while (Serial.available()) { char c = Serial.read(); if (c=='
'||c==''){ if(cmd_line.length()){ handle_usb_command(cmd_line); cmd_line=""; } } else { cmd_line += c; if(cmd_line.length()>128) cmd_line=cmd_line.substring(0,128);} }
  process_packets_loop();
}

static void setup_pio_dma_channel(PioDmaChannel &c, PIO pio, uint sm, uint pin_in, uint pre_pin, int8_t prog_offset){
  c.pio=pio; c.sm=sm; c.pin=pin_in; c.pre_pin=pre_pin; c.ring_words=DMA_RING_WORDS; c.ridx=0; c.widx=0; c.ring=(volatile uint32_t*)malloc(sizeof(uint32_t)*DMA_RING_WORDS); memset((void*)c.ring,0,sizeof(uint32_t)*DMA_RING_WORDS);
  adsb_sampler_program_init(pio, sm, prog_offset, pin_in, SAMPLE_FREQ_HZ);
  uint dreq = (pio==pio0)? (sm==0?DREQ_PIO0_RX0: sm==1?DREQ_PIO0_RX1: sm==2?DREQ_PIO0_RX2:DREQ_PIO0_RX3) : (sm==0?DREQ_PIO1_RX0: sm==1?DREQ_PIO1_RX1: sm==2?DREQ_PIO1_RX2:DREQ_PIO1_RX3);
  c.dreq=dreq; int chan = dma_claim_unused_channel(true); c.dma_chan=chan;
  dma_channel_config cfg = dma_channel_get_default_config(chan);
  channel_config_set_read_increment(&cfg, false); channel_config_set_write_increment(&cfg, true);
  channel_config_set_dreq(&cfg, dreq); channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
  channel_config_set_ring(&cfg, true, __builtin_ctz(DMA_RING_WORDS));
  dma_channel_configure(chan, &cfg, (void*)c.ring, (const void*)&pio->rxf[sm], 0xFFFFFFFF, false);
  dma_hw->ch[chan].al3_write_addr = (uintptr_t)c.ring; dma_hw->ch[chan].al3_trans_count_trig = 0xFFFFFFFF;
}

void core1_entry(){
  for (int i=0;i<3;i++) dma_channel_start(chn[i].dma_chan);
  uint32_t last_blink = time_us_32(); CorrState corr[3]; for (int i=0;i<3;i++) init_corr_state(corr[i], g_cfg);
  while(true){
    uint32_t t = time_us_32(); if ((t-last_blink)>=500000){ last_blink=t; sio_hw->gpio_togl = (1u<<PIN_BLINK_CORE1);} rssi_adc_last = analogRead(PIN_RSSI_ADC);
    for(int i=0;i<3;i++){
      uint32_t waddr = dma_hw->ch[chn[i].dma_chan].write_addr; uint32_t baseaddr = (uint32_t)(uintptr_t)chn[i].ring; uint32_t widx = ((waddr - baseaddr)>>2) & (DMA_RING_WORDS-1); chn[i].widx = widx;
      while (chn[i].ridx != chn[i].widx){ uint32_t word = chn[i].ring[chn[i].ridx]; chn[i].ridx = (chn[i].ridx+1)&(DMA_RING_WORDS-1); push_samples(i, word); }
      bool got = try_detect_and_slice(chn[i], i, t); if (got){ sio_hw->gpio_set = (1u<<chn[i].pre_pin);} static uint32_t pre_decay[3]={0,0,0}; if (sio_hw->gpio_out & (1u<<chn[i].pre_pin)){ if (t - pre_decay[i] > 100){ sio_hw->gpio_clr = (1u<<chn[i].pre_pin); pre_decay[i]=t; } } else pre_decay[i]=t;
    }
  }
}

static bool try_detect_and_slice(PioDmaChannel &c, int ch_index, uint32_t now_ticks){ int new_msgs = run_correlator_and_slice(ch_index, g_cfg, pktq[ch_index]); return new_msgs>0; }

static void handle_usb_command(const String &cmd){ String u=cmd; u.trim(); u.toUpperCase(); if(u=="HELP"){ Serial.println("Commands: HELP, SHOW, SAVE, LOAD, PROFILE <NORMAL|HIGH-EMI|URBAN|REMOTE>, RAW ON|OFF"); return;} if(u=="SHOW"){ print_config(g_cfg); return;} if(u=="SAVE"){ save_config(g_cfg); Serial.println("Saved to /config.txt"); return;} if(u=="LOAD"){ load_config(g_cfg); Serial.println("Loaded from /config.txt"); return;} if(u.startsWith("PROFILE ")){ String p=u.substring(8); p.trim(); if(p=="NORMAL") g_cfg.profile=Profile::Normal; else if(p=="HIGH-EMI") g_cfg.profile=Profile::HighEMI; else if(p=="URBAN") g_cfg.profile=Profile::Urban; else if(p=="REMOTE") g_cfg.profile=Profile::Remote; else { Serial.println("Bad profile"); return;} Serial.print("Profile set: "); Serial.println(profile_name(g_cfg.profile)); return;} if(u=="RAW ON"){ g_cfg.output_raw=true; Serial.println("RAW=ON"); return;} if(u=="RAW OFF"){ g_cfg.output_raw=false; Serial.println("RAW=OFF"); return;} Serial.println("Unknown command"); }

static uint32_t crc32(const uint8_t* data, size_t len){ uint32_t crc=0xFFFFFFFF; for(size_t i=0;i<len;i++){ crc^=data[i]; for(int j=0;j<8;j++) crc=(crc>>1) ^ (0xEDB88320 & (-(int)(crc&1))); } return ~crc; }

static void send_to_esp32(const FlightOutput &fo){ struct __attribute__((packed)) Frame{ uint8_t sof[2]; uint16_t len; FlightOutput fo; uint32_t crc; } f; f.sof[0]=0x55; f.sof[1]=0xAA; f.len=sizeof(FlightOutput); memcpy(&f.fo,&fo,sizeof(FlightOutput)); f.crc=crc32((uint8_t*)&f.fo,sizeof(FlightOutput)); Serial2.write((uint8_t*)&f,sizeof(f)); }

static void emit_debug_raw(const RawMessage &rm){ Serial.print("CH"); Serial.print(rm.channel+1); Serial.print(" "); Serial.print(rm.bits); Serial.print("b "); for(size_t i=0;i<rm.bytes;i++){ if(i) Serial.print(' '); char buf[4]; sprintf(buf, "%02X", rm.payload[i]); Serial.print(buf);} Serial.println(); }

static void process_packets_loop(){ RawMessage rm; DecodedFrame df; while (pktq[0].pop(rm) || pktq[1].pop(rm) || pktq[2].pop(rm)) { if (g_cfg.output_raw) emit_debug_raw(rm); CrcStatus cs = check_and_fix_crc(rm); if (decode_adsb_frame(rm, df, g_cfg)) { FlightOutput fo={0}; fo.addr=df.icao; fo.Squawk=df.squawk; memcpy(fo.flight, df.callsign, sizeof(fo.flight)); fo.altitude=df.altitude_m_geo; fo.pressure_altitude=df.altitude_m_baro; fo.speed=df.speed_kmh; fo.course=df.track_deg; fo.vert_rate=df.vert_rate_mpm; fo.latitude=df.lat; fo.longitude=df.lon; fo.seen=df.seen_time_ms; fo.timestamp=millis(); fo.signal_source=1; fo.aircraft_type=df.aircraft_type; Serial.print("ICAO="); Serial_printHex(df.icao,6); Serial.print(" flight="); Serial.print(df.callsign); Serial.print(" lat="); Serial.print(df.lat,6); Serial.print(" lon="); Serial.print(df.lon,6); Serial.print(" spd="); Serial.print(fo.speed); Serial.print(" alt="); Serial.print(fo.altitude); Serial.print(" vs="); Serial.print(fo.vert_rate); Serial.print(" trk="); Serial.print(fo.course); Serial.println(); send_to_esp32(fo);} } }
