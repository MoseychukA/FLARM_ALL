// rp2040_adsb_gpio_irq_nodma_v3.ino
#include <Arduino.h>
#include <LittleFS.h>
#include <hardware/gpio.h>
#include <pico/multicore.h>
#include "ringbuffer.h"
#include "adsb_decoder.h"

static constexpr uint8_t PIN_CH1 = 17;
static constexpr uint8_t PIN_CH2 = 19;
static constexpr uint8_t PIN_CH3 = 22;
static constexpr uint8_t UART2_TX = 4; static constexpr uint8_t UART2_RX = 5;
static constexpr uint8_t PIN_BLINK_CORE0 = 15; static constexpr uint8_t PIN_BLINK_CORE1 = 25;

static LockFreePacketQueue pktq[3];
static Config g_cfg; static String cmd_line;
EdgeBuf g_edge[3];

static uint32_t crc32_(const uint8_t* data, size_t len){ uint32_t crc=0xFFFFFFFF; for(size_t i=0;i<len;i++){ crc^=data[i]; for(int j=0;j<8;j++) crc=(crc>>1) ^ (0xEDB88320 & (-(int)(crc&1))); } return ~crc; }

static inline uint8_t pin_to_ch(uint gpio){ return (gpio==PIN_CH1)?0 : (gpio==PIN_CH2)?1 : (gpio==PIN_CH3)?2 : 255; }
static void gpio_irq_handler(uint gpio, uint32_t events){ uint8_t ch = pin_to_ch(gpio); if (ch>2) return; uint32_t t=time_us_32(); uint8_t lvl = (events & GPIO_IRQ_EDGE_RISE)?1:0; if (g_cfg.invert[ch]) lvl ^= 1; uint16_t wi = g_edge[ch].widx; g_edge[ch].e[wi].t=t; g_edge[ch].e[wi].level=lvl; g_edge[ch].widx = (wi+1) & (EDGE_BUF_SIZE-1); }

void core1_entry(){ uint32_t last=time_us_32(); while(true){ uint32_t t=time_us_32(); if(t-last>500000){ last=t; sio_hw->gpio_togl=(1u<<PIN_BLINK_CORE1);} tight_loop_contents(); } }

struct __attribute__((packed)) FlightOutput{ uint32_t addr; int32_t Squawk; char flight[8]; int32_t altitude; int32_t pressure_altitude; int32_t speed; int32_t course; int32_t vert_rate; float latitude; float longitude; uint32_t seen; uint32_t timestamp; uint8_t signal_source; uint8_t aircraft_type; };

static void send_to_esp32(const FlightOutput &fo){ struct __attribute__((packed)) Frame{ uint8_t sof[2]; uint16_t len; FlightOutput fo; uint32_t crc; } f; f.sof[0]=0x55; f.sof[1]=0xAA; f.len=sizeof(FlightOutput); memcpy(&f.fo,&fo,sizeof(FlightOutput)); f.crc=crc32_((uint8_t*)&f.fo,sizeof(FlightOutput)); Serial2.write((uint8_t*)&f,sizeof(f)); }

static void emit_debug_raw(const RawMessage &rm){ if(!g_cfg.output_raw) return; Serial.print("CH"); Serial.print(rm.channel+1); Serial.print(" "); Serial.print(rm.bits); Serial.print("b "); for(size_t i=0;i<rm.bytes;i++){ if(i) Serial.print(' '); char buf[4]; sprintf(buf, "%02X", rm.payload[i]); Serial.print(buf);} Serial.println(); }

static void process_packets(){ RawMessage rm; DecodedFrame df; for (int i=0;i<3;i++){ while (pktq[i].pop(rm)){
    emit_debug_raw(rm);
    CrcStatus cs = check_and_fix_crc(rm);
    if (decode_adsb_frame(rm, df, g_cfg)){
      FlightOutput fo={0}; fo.addr=df.icao; fo.Squawk=df.squawk; memcpy(fo.flight, df.callsign, sizeof(fo.flight)); fo.altitude=df.altitude_m_geo; fo.pressure_altitude=df.altitude_m_baro; fo.speed=df.speed_kmh; fo.course=df.track_deg; fo.vert_rate=df.vert_rate_mpm; fo.latitude=df.lat; fo.longitude=df.lon; fo.seen=df.seen_time_ms; fo.timestamp=millis(); fo.signal_source=1; fo.aircraft_type=df.aircraft_type; send_to_esp32(fo);
    }
  }}
}

static const char* profile_name(int p){ switch(p){ case 1: return "High-EMI"; case 2: return "Urban"; case 3: return "Remote"; default: return "Normal"; } }
static void apply_profile(Config &c){ switch(c.profile){ case 1: c.dead_time_us=8; c.tol_us=2; c.min_pulses=5; c.min_win_hits=1; c.base_thr=6.0f; c.ema_alpha=0.1f; c.dyn_k=1.0f; break; case 2: c.dead_time_us=7; c.tol_us=2; c.min_pulses=5; c.min_win_hits=1; c.base_thr=6.0f; c.ema_alpha=0.15f; c.dyn_k=1.2f; break; case 3: c.dead_time_us=6; c.tol_us=3; c.min_pulses=4; c.min_win_hits=1; c.base_thr=4.0f; c.ema_alpha=0.08f; c.dyn_k=0.8f; break; default: c.dead_time_us=7; c.tol_us=2; c.min_pulses=5; c.min_win_hits=1; c.base_thr=5.0f; c.ema_alpha=0.1f; c.dyn_k=1.0f; break; } }

static void handle_cmd(String u){ u.trim(); u.toUpperCase(); if(u=="HELP"){ Serial.println("Commands: HELP, SHOW, SAVE, LOAD, RAW ON|OFF, PROFILE <NORMAL|HIGH-EMI|URBAN|REMOTE>, INV <CH1|CH2|CH3> <ON|OFF>, REF <lat> <lon>"); return; } if(u=="SHOW"){ Serial.print("Profile="); Serial.print(profile_name(g_cfg.profile)); Serial.print(" RAW="); Serial.print(g_cfg.output_raw?1:0); Serial.print(" INV="); for(int i=0;i<3;i++){ Serial.print(g_cfg.invert[i]?"1":"0"); if(i<2) Serial.print(','); } Serial.print(" REF="); Serial.print(g_cfg.ref_lat,6); Serial.print(","); Serial.println(g_cfg.ref_lon,6); return; } if(u=="SAVE"){ File f=LittleFS.open("/config.txt","w"); if(f){ f.printf("profile=%d
raw=%d
inv=%d,%d,%d
ref_lat=%f
ref_lon=%f
", g_cfg.profile, g_cfg.output_raw?1:0, g_cfg.invert[0]?1:0, g_cfg.invert[1]?1:0, g_cfg.invert[2]?1:0, g_cfg.ref_lat, g_cfg.ref_lon); f.close(); Serial.println("Saved"); } return; } if(u=="LOAD"){ if(LittleFS.exists("/config.txt")){ File f=LittleFS.open("/config.txt","r"); if(f){ while(f.available()){ String line=f.readStringUntil('
'); line.trim(); if(!line.length()||line[0]=='#') continue; int eq=line.indexOf('='); if(eq<0) continue; String k=line.substring(0,eq), v=line.substring(eq+1); k.trim(); v.trim(); if(k=="profile") g_cfg.profile=v.toInt(); else if(k=="raw") g_cfg.output_raw=(v=="1"); else if(k=="inv"){ int a,b,c; if(sscanf(v.c_str(), "%d,%d,%d", &a,&b,&c)==3){ g_cfg.invert[0]=a; g_cfg.invert[1]=b; g_cfg.invert[2]=c; } } else if(k=="ref_lat") g_cfg.ref_lat=v.toFloat(); else if(k=="ref_lon") g_cfg.ref_lon=v.toFloat(); } f.close(); } } apply_profile(g_cfg); Serial.println("Loaded"); return; } if(u=="RAW ON"){ g_cfg.output_raw=true; Serial.println("RAW=ON"); return; } if(u=="RAW OFF"){ g_cfg.output_raw=false; Serial.println("RAW=OFF"); return; } if(u.startsWith("PROFILE ")){ String p=u.substring(8); p.trim(); if(p=="NORMAL") g_cfg.profile=0; else if(p=="HIGH-EMI") g_cfg.profile=1; else if(p=="URBAN") g_cfg.profile=2; else if(p=="REMOTE") g_cfg.profile=3; else { Serial.println("Bad profile"); return; } apply_profile(g_cfg); Serial.print("Profile set: "); Serial.println(profile_name(g_cfg.profile)); return; } if(u.startsWith("INV ")){ String rest=u.substring(4); rest.trim(); int ch=-1; if(rest.startsWith("CH1")) ch=0; else if(rest.startsWith("CH2")) ch=1; else if(rest.startsWith("CH3")) ch=2; int sp=rest.indexOf(' '); if(ch<0||sp<0){ Serial.println("INV usage: INV CH1|CH2|CH3 ON|OFF"); return;} String st=rest.substring(sp+1); st.trim(); bool on = (st=="ON"); g_cfg.invert[ch]=on; Serial.print("Invert CH"); Serial.print(ch+1); Serial.print("="); Serial.println(on?"ON":"OFF"); return; } if(u.startsWith("REF ")){ float a,b; if(sscanf(u.c_str()+4, "%f %f", &a, &b)==2){ g_cfg.ref_lat=a; g_cfg.ref_lon=b; Serial.print("REF set: "); Serial.print(a,6); Serial.print(","); Serial.println(b,6); } else Serial.println("REF usage: REF <lat> <lon>"); return; } Serial.println("Unknown"); }

void setup(){ Serial.begin(115200); unsigned long t0=millis(); while(!Serial && !Serial.dtr() && (millis()-t0)<8000) delay(10); Serial.println("GPIO IRQ no-DMA v3 start"); LittleFS.begin(); g_cfg.profile=0; g_cfg.output_raw=true; g_cfg.out_format=0; g_cfg.invert[0]=g_cfg.invert[1]=g_cfg.invert[2]=false; g_cfg.ref_lat=55.93574f; g_cfg.ref_lon=37.34873f; apply_profile(g_cfg);
  gpio_init(PIN_BLINK_CORE0); gpio_set_dir(PIN_BLINK_CORE0, GPIO_OUT); gpio_put(PIN_BLINK_CORE0,0);
  gpio_init(PIN_BLINK_CORE1); gpio_set_dir(PIN_BLINK_CORE1, GPIO_OUT); gpio_put(PIN_BLINK_CORE1,0);
  Serial2.setTX(UART2_TX); Serial2.setRX(UART2_RX); Serial2.begin(921600);
  for(int i=0;i<3;i++) pktq[i].begin(64);
  gpio_init(PIN_CH1); gpio_set_dir(PIN_CH1, GPIO_IN); gpio_pull_down(PIN_CH1);
  gpio_init(PIN_CH2); gpio_set_dir(PIN_CH2, GPIO_IN); gpio_pull_down(PIN_CH2);
  gpio_init(PIN_CH3); gpio_set_dir(PIN_CH3, GPIO_IN); gpio_pull_down(PIN_CH3);
  gpio_set_irq_enabled_with_callback(PIN_CH1, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
  gpio_set_irq_enabled(PIN_CH2, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
  gpio_set_irq_enabled(PIN_CH3, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
  multicore_launch_core1(core1_entry);
  Serial.println("Setup done"); }

static uint32_t last_blink0=0; void loop(){ uint32_t now=millis(); if(now-last_blink0>1000){ last_blink0=now; sio_hw->gpio_togl=(1u<<PIN_BLINK_CORE0);} for (int i=0;i<3;i++) run_edge_correlator_and_slice(i, g_cfg, pktq[i]); process_packets(); while(Serial.available()){ char c=Serial.read(); if(c=='
'||c==''){ if(cmd_line.length()){ handle_cmd(cmd_line); cmd_line=""; } } else cmd_line+=c; } }
