// rp2040_adsb_irq_nodma.ino
#include <Arduino.h>
#include <LittleFS.h>
#include <hardware/pio.h>
#include <hardware/irq.h>
#include <pico/multicore.h>
#include "adsb_sampler.pio.h"
#include "ringbuffer.h"
#include "adsb_decoder.h"
#include "config.h"
#include "esp_packet.h"
#include "utils.h"

// Inputs by request: 17, 19, 22 (three channels)
static constexpr uint8_t PIN_CH1 = 17;
static constexpr uint8_t PIN_CH2 = 19;
static constexpr uint8_t PIN_CH3 = 22;

static constexpr uint8_t PIN_BLINK_CORE0 = 15; // 1s
static constexpr uint8_t PIN_BLINK_CORE1 = 25; // 0.5s

static constexpr uint8_t UART2_TX = 4; static constexpr uint8_t UART2_RX = 5;
static constexpr uint32_t SAMPLE_FREQ_HZ = 20000000; // 20 MHz

struct Channel { PIO pio; uint sm; uint pin; };
static Channel ch[3];
static LockFreePacketQueue pktq[3];

static Config g_cfg; static String cmd_line;

// Sample FIFOs
extern SampleFifo g_samp[3];

static void pio0_irq_handler();
static void pio1_irq_handler();
static void setup_pio_irq_channel(Channel &c, PIO pio, uint sm, uint pin, int prog_off);
static void process_packets_loop();

static uint32_t crc32_(const uint8_t* data, size_t len){ uint32_t crc=0xFFFFFFFF; for(size_t i=0;i<len;i++){ crc^=data[i]; for(int j=0;j<8;j++) crc=(crc>>1) ^ (0xEDB88320 & (-(int)(crc&1))); } return ~crc; }

void core1_entry(){ uint32_t last=time_us_32(); while(true){ uint32_t t=time_us_32(); if(t-last>500000){ last=t; sio_hw->gpio_togl=(1u<<PIN_BLINK_CORE1);} // nothing else, IRQs feed FIFOs
    tight_loop_contents(); }
}

static void handle_cmd(const String &cmd){ String u=cmd; u.trim(); u.toUpperCase(); if(u=="HELP"){ Serial.println("Commands: HELP, SHOW, SAVE, LOAD, RAW ON|OFF"); return;} if(u=="SHOW"){ print_config(g_cfg); return;} if(u=="SAVE"){ save_config(g_cfg); Serial.println("Saved"); return;} if(u=="LOAD"){ load_config(g_cfg); Serial.println("Loaded"); return;} if(u=="RAW ON"){ g_cfg.output_raw=true; Serial.println("RAW=ON"); return;} if(u=="RAW OFF"){ g_cfg.output_raw=false; Serial.println("RAW=OFF"); return;} Serial.println("Unknown"); }

void setup(){ Serial.begin(115200); unsigned long t0=millis(); while(!Serial && !Serial.dtr() && (millis()-t0)<8000) delay(10); Serial.println("IRQ no-DMA variant start");
  LittleFS.begin(); load_config(g_cfg);
  gpio_init(PIN_BLINK_CORE0); gpio_set_dir(PIN_BLINK_CORE0, GPIO_OUT); gpio_put(PIN_BLINK_CORE0,0);
  gpio_init(PIN_BLINK_CORE1); gpio_set_dir(PIN_BLINK_CORE1, GPIO_OUT); gpio_put(PIN_BLINK_CORE1,0);
  Serial2.setTX(UART2_TX); Serial2.setRX(UART2_RX); Serial2.begin(921600);
  for(int i=0;i<3;i++) pktq[i].begin(64);

  // Load PIO program
  PIO p0=pio0, p1=pio1; int off0 = pio_add_program(p0,&adsb_sampler_program); int off1 = pio_add_program(p1,&adsb_sampler_program);
  setup_pio_irq_channel(ch[0], p0, 0, PIN_CH1, off0);
  setup_pio_irq_channel(ch[1], p0, 1, PIN_CH2, off0);
  setup_pio_irq_channel(ch[2], p1, 0, PIN_CH3, off1);

  // IRQ handlers for PIO RX not empty
  irq_set_exclusive_handler(PIO0_IRQ_0, pio0_irq_handler); irq_set_enabled(PIO0_IRQ_0, true);
  irq_set_exclusive_handler(PIO1_IRQ_0, pio1_irq_handler); irq_set_enabled(PIO1_IRQ_0, true);

  multicore_launch_core1(core1_entry);
  Serial.println("Setup done");
}

static uint32_t last_blink0=0;
void loop(){ uint32_t now=millis(); if(now-last_blink0>1000){ last_blink0=now; sio_hw->gpio_togl=(1u<<PIN_BLINK_CORE0);} while(Serial.available()){ char c=Serial.read(); if(c=='
'||c==''){ if(cmd_line.length()) { handle_cmd(cmd_line); cmd_line=""; } } else cmd_line+=c; }
  process_packets_loop(); }

static inline void drain_sm(PIO pio, uint sm, int ch_index){ while(!pio_sm_is_rx_fifo_empty(pio, sm)) { uint32_t w = pio->rxf[sm]; push_samples(ch_index, w); } }

static void pio0_irq_handler(){ uint32_t status = pio0->irq; // any source
  // Drain both SM on pio0 (sm0 for pin17, sm1 for pin19)
  drain_sm(pio0, 0, 0);
  drain_sm(pio0, 1, 1);
  // Clear IRQ (write 1s)
  pio0->irq = status;
}

static void pio1_irq_handler(){ uint32_t status = pio1->irq; drain_sm(pio1, 0, 2); pio1->irq = status; }

static void setup_pio_irq_channel(Channel &c, PIO pio, uint sm, uint pin, int prog_off){ c.pio=pio; c.sm=sm; c.pin=pin; adsb_sampler_program_init(pio, sm, prog_off, pin, SAMPLE_FREQ_HZ);
  // Enable IRQ when RX FIFO not empty (IRQ0 level)
  // Use RX FIFO not empty as source per SM
  if (pio==pio0) {
    pio_set_irq0_source_enabled(pio, (pio_interrupt_source) (pis_sm0_rx_fifo_not_empty + sm), true);
  } else {
    pio_set_irq0_source_enabled(pio, (pio_interrupt_source) (pis_sm0_rx_fifo_not_empty + sm), true);
  }
}

static void emit_debug_raw(const RawMessage &rm){ Serial.print("CH"); Serial.print(rm.channel+1); Serial.print(" "); Serial.print(rm.bits); Serial.print("b "); for(size_t i=0;i<rm.bytes;i++){ if(i) Serial.print(' '); char buf[4]; sprintf(buf, "%02X", rm.payload[i]); Serial.print(buf);} Serial.println(); }

static void send_to_esp32(const FlightOutput &fo){ struct __attribute__((packed)) Frame{ uint8_t sof[2]; uint16_t len; FlightOutput fo; uint32_t crc; } f; f.sof[0]=0x55; f.sof[1]=0xAA; f.len=sizeof(FlightOutput); memcpy(&f.fo,&fo,sizeof(FlightOutput)); f.crc=crc32_((uint8_t*)&f.fo,sizeof(FlightOutput)); Serial2.write((uint8_t*)&f,sizeof(f)); }

static void process_packets_loop(){ RawMessage rm; DecodedFrame df; if (pktq[0].pop(rm) || pktq[1].pop(rm) || pktq[2].pop(rm)){
    if (g_cfg.output_raw) emit_debug_raw(rm);
    CrcStatus cs = check_and_fix_crc(rm);
    if (decode_adsb_frame(rm, df, g_cfg)){
      FlightOutput fo={0}; fo.addr=df.icao; fo.Squawk=df.squawk; memcpy(fo.flight, df.callsign, sizeof(fo.flight)); fo.altitude=df.altitude_m_geo; fo.pressure_altitude=df.altitude_m_baro; fo.speed=df.speed_kmh; fo.course=df.track_deg; fo.vert_rate=df.vert_rate_mpm; fo.latitude=df.lat; fo.longitude=df.lon; fo.seen=df.seen_time_ms; fo.timestamp=millis(); fo.signal_source=1; fo.aircraft_type=df.aircraft_type; send_to_esp32(fo);
    }
  }
}
