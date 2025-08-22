
#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <semphr.h>

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/adc.h"
#include "hardware/irq.h"
#include "hardware/gpio.h"

#include "adsb_sampler.pio.h"
#include "adsb_crc.h"
#include "adsb_decoder.h"

static const uint8_t PIN_UART2_TX = 4;
static const uint8_t PIN_UART2_RX = 5;

struct RxChanCfg { uint8_t pin_in; uint8_t pin_led; };
static const RxChanCfg CH[3] = { {18,17}, {19,20}, {22,23} };

static const uint8_t PIN_BLINK_CORE0 = 15;
static const uint8_t PIN_BLINK_CORE1 = 25;
static const uint8_t PIN_RSSI = 26;        // ADC0

static const uint32_t SAMPLE_RATE_HZ = 10000000u; // 10 MHz
static const uint8_t  T03 = (uint8_t)(SAMPLE_RATE_HZ * 0.3e-6 + 0.5);
static const uint8_t  T04 = (uint8_t)(SAMPLE_RATE_HZ * 0.4e-6 + 0.5);
static const uint8_t  T05 = (uint8_t)(SAMPLE_RATE_HZ * 0.5e-6 + 0.5);
static const uint8_t  T07 = (uint8_t)(SAMPLE_RATE_HZ * 0.7e-6 + 0.5);
static const uint8_t  TBIT = (uint8_t)(SAMPLE_RATE_HZ * 1.0e-6 + 0.5);
static const uint16_t TPRE = (uint16_t)(SAMPLE_RATE_HZ * 8.0e-6 + 0.5);

typedef struct {
  uint8_t ch;
  uint8_t nbits;
  uint8_t data[14];
  uint32_t rssi_u;
} AdsbPacket;

static QueueHandle_t q_packets;
static QueueHandle_t q_fixed_packets;

typedef struct {
  PIO pio; uint sm;
  bool last; uint8_t runlen; uint32_t sample_pos;
  uint32_t pulse_times[4096]; uint16_t phead, ptail;
  bool in_frame;
} RxState;

static RxState RX[3];

static inline void fast_gpio_set(uint8_t pin, bool v) {
  if (v) sio_hw->gpio_set = 1u << pin; else sio_hw->gpio_clr = 1u << pin;
}
static inline void fast_gpio_toggle(uint8_t pin) { sio_hw->gpio_togl = 1u << pin; }

static inline void push_pulse(RxState& rs, uint32_t tcenter) {
  uint16_t next = (rs.phead + 1) & 0x0FFF;
  if (next == rs.ptail) { rs.ptail = (rs.ptail + 1) & 0x0FFF; }
  rs.pulse_times[rs.phead] = tcenter; rs.phead = next;
}
static inline bool peek_pulse(RxState& rs, uint16_t idx_from_tail, uint32_t& t) {
  uint16_t cnt = (rs.phead - rs.ptail) & 0x0FFF;
  if (idx_from_tail >= cnt) return false;
  uint16_t i = (rs.ptail + idx_from_tail) & 0x0FFF;
  t = rs.pulse_times[i]; return true;
}

static bool check_preamble_10MHz(RxState& rs, uint32_t& t0) {
  uint16_t cnt = (rs.phead - rs.ptail) & 0x0FFF; if (cnt < 4) return false;
  uint32_t tA, tB, tC, tD; peek_pulse(rs, cnt-4, tA); peek_pulse(rs, cnt-3, tB); peek_pulse(rs, cnt-2, tC); peek_pulse(rs, cnt-1, tD);
  int dAB = (int)(tB - tA); int dBC = (int)(tC - tB); int dCD = (int)(tD - tC);
  auto inrng = [](int v, int c){ return v >= c-2 && v <= c+2; };
  if (inrng(dAB, 10) && inrng(dBC, 25) && inrng(dCD, 10)) { t0 = tA - (T05/2); return true; }
  return false;
}

static bool try_demod_bits(RxState& rs, uint32_t t0, uint8_t* out, uint8_t& nbits) {
  uint32_t data_start = t0 + TPRE; memset(out, 0, 14); nbits = 112; uint16_t cnt = (rs.phead - rs.ptail) & 0x0FFF;
  for (uint8_t bit=0; bit<112; bit++) {
    uint32_t w0 = data_start + bit*TBIT; uint32_t w1 = w0 + 5; bool first=false, second=false;
    for (uint16_t k=0; k<cnt; k++) {
      uint32_t tp; if (!peek_pulse(rs, k, tp)) break; if (tp + 2 < w0) continue; if (tp > w0 + TBIT + 2) break;
      if (tp >= (int)w0 - 2 && tp <= (int)w1 + 2) first = true; else if (tp > (int)w1 - 2 && tp <= (int)(w0 + TBIT) + 2) second = true;
    }
    uint8_t val = first ? 1 : (second ? 0 : 0); if (val) out[bit>>3] |= (1 << (7-(bit&7)));
    if (bit == 55) {
      bool any_after = false; for (uint8_t b2=56; b2<112 && !any_after; b2++) {
        uint32_t w0b = data_start + b2*TBIT; uint32_t wE = w0b + TBIT; for (uint16_t k=0; k<cnt; k++) {
          uint32_t tp; if (!peek_pulse(rs, k, tp)) break; if (tp < w0b-2) continue; if (tp > wE+2) break; any_after = true; break; }
      }
      if (!any_after) { nbits = 56; break; }
    }
  }
  return true;
}

static inline void feed_sample_and_filter(RxState& rs, bool s) {
  rs.sample_pos++;
  if (s == rs.last) { if (rs.runlen < 255) rs.runlen++; return; }
  if (rs.last) {
    if (rs.runlen >= T04 && rs.runlen <= T07) {
      uint32_t start = rs.sample_pos - rs.runlen; uint32_t center = start + rs.runlen/2; push_pulse(rs, center);
    }
  }
  rs.last = s; rs.runlen = 1;
}

typedef struct { uint32_t icao; bool has_even, has_odd; uint8_t even_me[7], odd_me[7]; uint32_t even_time_ms, odd_time_ms; } IcaoCprState;
#define ICAO_MAP_SIZE 256
static IcaoCprState ICAO_CPR[ICAO_MAP_SIZE];
static inline uint8_t hash_icao(uint32_t icao) { icao ^= icao >> 12; icao ^= icao >> 6; icao ^= icao >> 3; return (uint8_t)icao; }

static void send_raw_debug(const AdsbPacket& p) {
  Serial2.print("CH"); Serial2.print(p.ch+1);
  Serial2.print(" LEN="); Serial2.print(p.nbits);
  Serial2.print(" RAW=");
  for (uint8_t i=0;i<(p.nbits+7)/8;i++) { if (p.data[i]<16) Serial2.print('0'); Serial2.print(p.data[i], HEX); }
  Serial2.print("
");
}

static void decode_and_send(AdsbPacket& p) {
  send_raw_debug(p);
  const uint8_t nbytes = (p.nbits+7)/8; uint8_t msg[14]; memcpy(msg, p.data, nbytes);
  uint8_t df = (msg[0] >> 3) & 0x1F; uint32_t parity = (msg[nbytes-3]<<16)|(msg[nbytes-2]<<8)|msg[nbytes-1];
  uint32_t crc_wo = modes_crc24(msg, nbytes-3); uint32_t syndrome = (crc_wo ^ parity) & 0xFFFFFF;
  bool fixed=false; int fixed_bit=-1;
  if (syndrome != 0) { fixed_bit = modes_try_single_bit_fix(msg, p.nbits); if (fixed_bit >= 0) { fixed=true; AdsbPacket fp = p; memcpy(fp.data, msg, nbytes); xQueueSend(q_fixed_packets, &fp, 0); parity = (msg[nbytes-3]<<16)|(msg[nbytes-2]<<8)|msg[nbytes-1]; crc_wo = modes_crc24(msg, nbytes-3); syndrome = (crc_wo ^ parity) & 0xFFFFFF; } }
  uint32_t icao = (df==17) ? (parity ^ modes_crc24(msg, nbytes-3)) : ((msg[1]<<16)|(msg[2]<<8)|msg[3]);
  const uint8_t* me = &msg[4]; char callsign[9] = {0}; double lat=NAN, lon=NAN, gs=NAN, trk=NAN; int alt=-1;
  if (df == 17 && p.nbits == 112) {
    uint8_t tc = (me[0] >> 3) & 0x1F;
    if (tc >= 1 && tc <= 4) decode_callsign(me, callsign);
    else if (tc >= 9 && tc <= 18) {
      bool odd = (me[0] & 0x04) != 0; uint8_t h = hash_icao(icao);
      if (ICAO_CPR[h].icao != icao) { memset(&ICAO_CPR[h], 0, sizeof(IcaoCprState)); ICAO_CPR[h].icao = icao; }
      if (odd) { memcpy(ICAO_CPR[h].odd_me, me, 7); ICAO_CPR[h].odd_time_ms = millis(); ICAO_CPR[h].has_odd = true; }
      else     { memcpy(ICAO_CPR[h].even_me, me, 7); ICAO_CPR[h].even_time_ms = millis(); ICAO_CPR[h].has_even = true; }
      if (ICAO_CPR[h].has_even && ICAO_CPR[h].has_odd && (uint32_t)abs((int)(ICAO_CPR[h].even_time_ms - ICAO_CPR[h].odd_time_ms)) <= 10000) { cpr_global_decode(lat, lon, ICAO_CPR[h].even_me, ICAO_CPR[h].odd_me); }
      alt = decode_altitude(me);
    } else if (tc == 19) { decode_velocity(me, gs, trk); }
  }
  Serial2.print("DF="); Serial2.print(df);
  Serial2.print(" ICAO="); for (int i=0;i<3;i++){ uint8_t b=(icao>>(16-8*i))&0xFF; if (b<16) Serial2.print('0'); Serial2.print(b,HEX); }
  if (callsign[0]) { Serial2.print(" FLT="); Serial2.print(callsign); }
  if (!isnan(lat) && !isnan(lon)) { Serial2.print(" LAT="); Serial2.print(lat,6); Serial2.print(" LON="); Serial2.print(lon,6); }
  if (!isnan(gs)) { Serial2.print(" GS="); Serial2.print(gs,1); Serial2.print("kt TRK="); Serial2.print(trk,1); }
  if (alt>=0) { Serial2.print(" ALT="); Serial2.print(alt); Serial2.print("ft"); }
  Serial2.print(" RSSI="); Serial2.print(p.rssi_u);
  Serial2.print(" CRC_OK="); Serial2.print(syndrome==0 ? "Y":"N"); if (fixed) { Serial2.print(" FIXED bit="); Serial2.print(fixed_bit); }
  Serial2.print("
");
}

static TaskHandle_t hRxTask = nullptr;
static void gpio_irq_cb(uint gpio, uint32_t events) { BaseType_t hpw = pdFALSE; if (hRxTask) vTaskNotifyGiveFromISR(hRxTask, &hpw); portYIELD_FROM_ISR(hpw); }

static void rx_task(void* arg) {
  PIO pio = pio0; uint offset = pio_add_program(pio, &adsb_sampler_program);
  for (int c=0;c<3;c++) {
    RX[c] = {}; RX[c].pio = pio; RX[c].sm = c; gpio_init(CH[c].pin_led); gpio_set_dir(CH[c].pin_led, GPIO_OUT); fast_gpio_set(CH[c].pin_led, 0);
    pio_sm_config cfg = adsb_sampler_program_get_default_config(offset);
    sm_config_set_in_pins(&cfg, CH[c].pin_in);
    sm_config_set_in_shift(&cfg, true, true, 32);
    float div = (float)clock_get_hz(clk_sys) / (float)SAMPLE_RATE_HZ; sm_config_set_clkdiv(&cfg, div);
    pio_gpio_init(pio, CH[c].pin_in);
    pio_sm_set_consecutive_pindirs(pio, RX[c].sm, CH[c].pin_in, 1, false);
    pio_sm_init(pio, RX[c].sm, offset, &cfg); pio_sm_set_enabled(pio, RX[c].sm, true);
  }
  gpio_init(PIN_BLINK_CORE1); gpio_set_dir(PIN_BLINK_CORE1, GPIO_OUT); fast_gpio_set(PIN_BLINK_CORE1, 0);
  TickType_t lastBlink = xTaskGetTickCount();
  adc_init(); adc_gpio_init(PIN_RSSI); adc_select_input(0);
  gpio_set_irq_enabled_with_callback(CH[0].pin_in, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpio_irq_cb);
  gpio_set_irq_enabled(CH[1].pin_in, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
  gpio_set_irq_enabled(CH[2].pin_in, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
  while (1) {
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(2));
    for (int c=0;c<3;c++) {
      RxState& rs = RX[c];
      while (!pio_sm_is_rx_fifo_empty(rs.pio, rs.sm)) {
        uint32_t w = pio_sm_get_blocking(rs.pio, rs.sm);
        for (int i=31;i>=0;i--) {
          bool bit = (w>>i) & 1; feed_sample_and_filter(rs, bit);
          uint32_t t0; if (!rs.in_frame && check_preamble_10MHz(rs, t0)) {
            fast_gpio_set(CH[c].pin_led, 1);
            uint8_t out[14]; uint8_t nbits=0;
            if (try_demod_bits(rs, t0, out, nbits)) {
              AdsbPacket pkt{}; pkt.ch = c; pkt.nbits = nbits; memcpy(pkt.data, out, (nbits+7)/8);
              uint32_t acc = 0; const int N=16; for (int k=0;k<N;k++) acc += adc_read(); pkt.rssi_u = acc/N;
              xQueueSend(q_packets, &pkt, 0);
            }
            fast_gpio_set(CH[c].pin_led, 0);
          }
        }
      }
    }
    if (xTaskGetTickCount() - lastBlink >= pdMS_TO_TICKS(500)) { fast_gpio_toggle(PIN_BLINK_CORE1); lastBlink = xTaskGetTickCount(); }
    taskYIELD();
  }
}

static void proc_task(void* arg) {
  Serial2.setTX(PIN_UART2_TX); Serial2.setRX(PIN_UART2_RX); Serial2.begin(230400);
  gpio_init(PIN_BLINK_CORE0); gpio_set_dir(PIN_BLINK_CORE0, GPIO_OUT); fast_gpio_set(PIN_BLINK_CORE0, 0);
  TickType_t lastBlink = xTaskGetTickCount();
  AdsbPacket pkt; while (1) {
    if (xQueueReceive(q_packets, &pkt, pdMS_TO_TICKS(10)) == pdPASS) { decode_and_send(pkt); }
    if (xQueueReceive(q_fixed_packets, &pkt, 0) == pdPASS) { Serial2.print("FIXED_RAW="); for (uint8_t i=0;i<(pkt.nbits+7)/8;i++) { if (pkt.data[i]<16) Serial2.print('0'); Serial2.print(pkt.data[i], HEX); } Serial2.print("
"); }
    if (xTaskGetTickCount() - lastBlink >= pdMS_TO_TICKS(1000)) { fast_gpio_toggle(PIN_BLINK_CORE0); lastBlink = xTaskGetTickCount(); }
  }
}

extern "C" void vTaskCoreAffinitySet(TaskHandle_t xTask, UBaseType_t uxCoreAffinityMask);
__attribute__((constructor)) static void start_rtos() {
  q_packets = xQueueCreate(64, sizeof(AdsbPacket)); q_fixed_packets = xQueueCreate(32, sizeof(AdsbPacket));
  TaskHandle_t hRx=nullptr, hProc=nullptr; xTaskCreate(rx_task, "rx", 8192, NULL, configMAX_PRIORITIES-1, &hRx); xTaskCreate(proc_task, "proc", 8192, NULL, configMAX_PRIORITIES-2, &hProc); hRxTask = hRx;
  if (hRx)   vTaskCoreAffinitySet(hRx,   (1<<1));
  if (hProc) vTaskCoreAffinitySet(hProc, (1<<0));
}

void setup() {}
void loop() {}
