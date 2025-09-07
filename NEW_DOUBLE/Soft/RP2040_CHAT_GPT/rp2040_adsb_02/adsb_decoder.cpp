#include "adsb_decoder.h"
#include "config.h"
#include "ringbuffer.h"
#include "preamble_corr.h"
#include "adsb_crc.h"
#include "adsb_cpr.h"
#include "adsb_gillham.h"

#ifdef ARDUINO_ARCH_RP2040
#include <hardware/pio.h>
#include <hardware/dma.h>
#include <hardware/irq.h>
#include <pico/time.h>
#include "adsb_sampler.pio.h"
#endif

static QueueHandle_t packetQueue;

// ====== PIO/DMA context ======
#ifdef ARDUINO_ARCH_RP2040
static PIO pio = pio0;
static int sm1=-1, sm2=-1, sm3=-1;
static uint offset_prog=0;
static int dma1=-1, dma2=-1, dma3=-1;
#endif

// Кольца DMA по каналам
static DMARing ring1, ring2, ring3;

// Корреляторы по каналам
static CorrState corr1, corr2, corr3;
static RuntimeConfig g_cfg;

// Локальный CPR по ICAO (простая карта на 8 сессий)
struct CPRMap {
  CPRState s[8];
} cprmap;

static CPRState* cpr_slot(uint32_t icao) {
  int idx = icao & 7;
  CPRState &slot = cprmap.s[idx];
  if (slot.icao != icao) cpr_reset(slot), slot.icao = icao;
  return &cprmap.s[idx];
}

// ====== Вспомогательные ======
static inline uint32_t micros32() { return (uint32_t)micros(); }

static void setup_profile(Profile p) {
  switch (p) {
    case PROFILE_HIGH_EMI: g_cfg.base_thr=7.5f; g_cfg.k_neg=0.35f; break;
    case PROFILE_URBAN:    g_cfg.base_thr=7.0f; g_cfg.k_neg=0.30f; break;
    case PROFILE_REMOTE:   g_cfg.base_thr=6.0f; g_cfg.k_neg=0.20f; break;
    default:               g_cfg.base_thr=6.5f; g_cfg.k_neg=0.25f; break;
  }
  g_cfg.profile = p;
}

// Преобразование 32-бит слова в 32 отсчёта 0/1
static inline void expand_word_to_bits(uint32_t w, uint8_t *dst) {
  for (int i=31;i>=0;--i) *dst++ = (w >> i) & 1;
}

// Сборка бита Manchester ADS-B из двух интервалов (упрощение для быстрого прототипа)
// Здесь используем 0.5 мкс шаг — для бита (1 мкс) берём 2 отсчёта как "символ" и средим.
static inline uint8_t bit_from_samples(const uint8_t *s, int pos) {
  // pos — начало бита в отсчётах (2 отсчёта на бит)
  int a = s[pos];
  int b = s[pos+1];
  // Mode-S: '1' — импульс в первой половине бита, '0' — во второй.
  // Упрощённо: если a>b => 1, иначе 0
  return (a > b) ? 1 : 0;
}

// Из окна после преамбулы собрать 56/112 бит
static bool assemble_packet_bits(const uint8_t *s, uint32_t s_len, uint32_t start, AdsbPacket &out) {
  // Преамбула длится 8 мкс (16 отсчётов). Полезные биты начинаются после ~8 мкс.
  uint32_t bit_start = start + 16; // в отсчётах (0.5мкс)
  // Попробуем 112, если не хватает — 56.
  uint16_t try_bits[2] = {112, 56};
  for (int k=0;k<2;k++) {
    uint16_t bits = try_bits[k];
    uint32_t need = bit_start + bits*2; // по 2 отсчёта на бит
    if (need > s_len) continue;

    memset(out.raw, 0, sizeof(out.raw));
    for (int i=0;i<bits;i++) {
      uint8_t b = bit_from_samples(s, bit_start + i*2);
      if (b) out.raw[i>>3] |= (1 << (7 - (i&7)));
    }
    out.bits = bits;
    return true;
  }
  return false;
}

// Обработать один канал: снять из DMA-ринга слова, развернуть в биты,
// прогнать фильтр, коррелятор, собрать пакеты -> положить в очередь.
static void process_channel(DMARing &ring, CorrState &st, uint8_t channel) {
  static uint8_t sample_buf[DMA_RING_WORDS * 32]; // максимум временно
  uint32_t nwords = ring_avail(ring);
  if (nwords == 0) return;

  // Развернём в 0/1
  uint32_t total_samples = 0;
  for (uint32_t i=0;i<nwords;i++) {
    uint32_t w;
    if (!ring_pop(ring, w)) break;
    expand_word_to_bits(w, &sample_buf[total_samples]);
    total_samples += 32;
  }

  digital_filter_03us(sample_buf, total_samples, g_cfg.digital_filter_on);

  CorrProfile prof; corr_profile_from_runtime(g_cfg, prof);

  uint32_t i = 0;
  while (i + PRE_WIN_SAMPLES + 2 < total_samples) {
    PreambleHit h = corr_preamble_and_get_t0(sample_buf, total_samples, i, st, prof);
    if (h.ok) {
      AdsbPacket pkt{};
      if (assemble_packet_bits(sample_buf, total_samples, h.t0_index, pkt)) {
        pkt.channel = channel;
        pkt.t0_us = micros32();
        // RSSI: грубо посчитаем среднюю плотность "1" в преамбуле (условно)
        int ones=0;
        for (int k=0;k<16;k++) ones += sample_buf[h.t0_index + k];
        pkt.rssi = (int16_t)(ones * 256); // условная шкала

#if DEBUG_DUMP_RAW
        // Отладочный вывод в USB (можно закомментировать при высоких нагрузках)
        Serial.printf("RAW ch%u len=%u: ", channel, pkt.bits);
        for (int b=0;b<(pkt.bits+7)/8; ++b) Serial.printf("%02X", pkt.raw[b]);
        Serial.println();
#endif
        // Проверка CRC и попытка одно-битной коррекции
        if (!adsb_crc_check(pkt.raw, pkt.bits)) {
          uint8_t tmp[14]; memcpy(tmp, pkt.raw, sizeof(tmp));
          if (adsb_crc_fix_1bit(tmp, pkt.bits)) memcpy(pkt.raw, tmp, sizeof(tmp));
          else { i += DEAD_TIME_SAMPLES; continue; }
        }

        // Пакет валиден — в очередь
        xQueueSend(packetQueue, &pkt, 0);
        i += DEAD_TIME_SAMPLES; // мёртвое время
        continue;
      }
    }
    i += 2; // скользящее окно
  }
}

#ifdef ARDUINO_ARCH_RP2040
// DMA IRQ handlers — просто двигаем write index кольца.
static void __isr __time_critical_func(dma1_irq_handler)() {
  dma_hw->ints0 = 1u << dma1;
  ring_commit(ring1);
}
static void __isr __time_critical_func(dma2_irq_handler)() {
  dma_hw->ints0 = 1u << dma2;
  ring_commit(ring2);
}
static void __isr __time_critical_func(dma3_irq_handler)() {
  dma_hw->ints0 = 1u << dma3;
  ring_commit(ring3);
}
#endif

void adsb_decoder_init() {
  packetQueue = xQueueCreate(PACKET_QUEUE_LEN, sizeof(AdsbPacket));
  setup_profile(PROFILE_NORMAL);

#ifdef ARDUINO_ARCH_RP2040
  // ====== PIO program ======
  offset_prog = pio_add_program(pio, &adsb_sampler_program);

  // Общая конфигурация SM
  pio_sm_config c = adsb_sampler_program_get_default_config(offset_prog);
  float div = (float)clock_get_hz(clk_sys) / (float)g_cfg.pio_clk_hz; // 2MHz
  sm_config_set_clkdiv(&c, div);

  // SM1 — PIN_CH1
  sm1 = pio_claim_unused_sm(pio, true);
  pio_sm_set_consecutive_pindirs(pio, sm1, PIN_CH1, 1, false);
  pio_sm_config c1 = c;
  sm_config_set_in_pins(&c1, PIN_CH1);
  pio_sm_init(pio, sm1, offset_prog, &c1);
  pio_sm_set_enabled(pio, sm1, true);

  // SM2 — PIN_CH2
  sm2 = pio_claim_unused_sm(pio, true);
  pio_sm_set_consecutive_pindirs(pio, sm2, PIN_CH2, 1, false);
  pio_sm_config c2 = c;
  sm_config_set_in_pins(&c2, PIN_CH2);
  pio_sm_init(pio, sm2, offset_prog, &c2);
  pio_sm_set_enabled(pio, sm2, true);

  // SM3 — PIN_CH3
  sm3 = pio_claim_unused_sm(pio, true);
  pio_sm_set_consecutive_pindirs(pio, sm3, PIN_CH3, 1, false);
  pio_sm_config c3 = c;
  sm_config_set_in_pins(&c3, PIN_CH3);
  pio_sm_init(pio, sm3, offset_prog, &c3);
  pio_sm_set_enabled(pio, sm3, true);

  // ====== DMA -> кольца ======
  ring_init(ring1, DMA_RING_WORDS);
  ring_init(ring2, DMA_RING_WORDS);
  ring_init(ring3, DMA_RING_WORDS);

  // DMA1
  dma1 = dma_claim_unused_channel(true);
  dma_channel_config d1 = dma_channel_get_default_config(dma1);
  channel_config_set_read_increment(&d1, false);
  channel_config_set_write_increment(&d1, false); // пишем всегда в текущую ячейку + IRQ => commit
  channel_config_set_dreq(&d1, DREQ_PIO0_RX0 + sm1);
  channel_config_set_ring(&d1, true, 0); // без авто-инкремента
  dma_channel_configure(dma1, &d1,
      (void*)ring_write_ptr(ring1),  // dst
      (void*)&pio->rxf[sm1],         // src
      1, true);
  // IRQ
  dma_channel_set_irq0_enabled(dma1, true);

  // DMA2
  dma2 = dma_claim_unused_channel(true);
  dma_channel_config d2 = dma_channel_get_default_config(dma2);
  channel_config_set_read_increment(&d2, false);
  channel_config_set_write_increment(&d2, false);
  channel_config_set_dreq(&d2, DREQ_PIO0_RX0 + sm2);
  dma_channel_configure(dma2, &d2,
      (void*)ring_write_ptr(ring2),
      (void*)&pio->rxf[sm2],
      1, true);
  dma_channel_set_irq0_enabled(dma2, true);

  // DMA3
  dma3 = dma_claim_unused_channel(true);
  dma_channel_config d3 = dma_channel_get_default_config(dma3);
  channel_config_set_read_increment(&d3, false);
  channel_config_set_write_increment(&d3, false);
  channel_config_set_dreq(&d3, DREQ_PIO0_RX0 + sm3);
  dma_channel_configure(dma3, &d3,
      (void*)ring_write_ptr(ring3),
      (void*)&pio->rxf[sm3],
      1, true);
  dma_channel_set_irq0_enabled(dma3, true);

  // Включаем общие IRQ
  irq_set_exclusive_handler(DMA_IRQ_0, [](){
    if (dma_hw->ints0 & (1u<<dma1)) dma1_irq_handler();
    if (dma_hw->ints0 & (1u<<dma2)) dma2_irq_handler();
    if (dma_hw->ints0 & (1u<<dma3)) dma3_irq_handler();
  });
  irq_set_enabled(DMA_IRQ_0, true);
#endif
}

bool adsb_decoder_fetch(AdsbPacket *pkt) {
  // Подсосать данные с DMA в пакеты, пока есть — положить в очередь.
  process_channel(ring1, corr1, 1);
  process_channel(ring2, corr2, 2);
  process_channel(ring3, corr3, 3);

  return xQueueReceive(packetQueue, pkt, 0) == pdTRUE;
}

// ====== Полный парсинг сообщений (DF17/DF18/20/21 частично) ======

static inline uint32_t get_bits(const uint8_t *b, int pos, int len) {
  // pos: 0 — наиболее значимый бит первого байта
  uint32_t v=0;
  for (int i=0;i<len;i++) {
    int bit = pos + i;
    int byte = bit >> 3;
    int off  = 7 - (bit & 7);
    v = (v<<1) | ((b[byte] >> off) & 1);
  }
  return v;
}

static void decode_identity_squawk(const uint8_t *me, DecodedADSB &fo) {
  // DF17, TC=20/21 (identity), или DF4/5 (SSR). Здесь для DF17/ME.
  // Squawk — 4 цифры октального кода.
  uint32_t id13 = get_bits(me, 0, 13); // AC13 поле
  // Для простоты преобразуем в числовой код (0..4095):
  fo.Squawk = (int)id13; // при необходимости — раскодировать в 4 цифры
}

static float kt_to_knots(float mps){
  return mps * 1.94384449f;
}

static void decode_tc19(const uint8_t *me, DecodedADSB &fo) {
  // TC=19: скорости/курс
  uint8_t subtype = get_bits(me, 5, 3);
  if (subtype==1 || subtype==2) {
    // Векторная скорость: EW, NS, Вертикальная
    int ew_dir = get_bits(me, 13, 1);
    int ew_spd = get_bits(me, 14, 10);
    int ns_dir = get_bits(me, 24, 1);
    int ns_spd = get_bits(me, 25, 10);
    int vr_src = get_bits(me, 35, 1); // баро/гео
    int vr_sign= get_bits(me, 36, 1);
    int vr     = get_bits(me, 37, 9); // ft/min

    float vx = (ew_dir ? -1.f : 1.f) * ew_spd; // knots
    float vy = (ns_dir ? -1.f : 1.f) * ns_spd; // knots
    fo.speed = sqrtf(vx*vx + vy*vy);
    fo.course = fmodf(atan2f(vx, vy) * 180.f / PI + 360.f, 360.f); // 0..360
    fo.vert_rate = (vr_sign ? -vr : vr);
  } else if (subtype==3 || subtype==4) {
    // Ground speed & track
    int spd = get_bits(me, 21, 10);
    int trk_status = get_bits(me, 31, 1);
    int trk = get_bits(me, 32, 10);
    fo.speed = spd; // knots
    fo.course = trk * 360.f / 1024.f;
  }
}

static void decode_position_cpr(uint32_t icao, const uint8_t *me, bool fflag, uint32_t now_ms, DecodedADSB &fo) {
  uint32_t lat = get_bits(me, 21, 17);
  uint32_t lon = get_bits(me, 38, 17);

  CPRState *slot = cpr_slot(icao);
  if (fflag) cpr_push_odd (*slot, icao, lat, lon, now_ms);
  else       cpr_push_even(*slot, icao, lat, lon, now_ms);

  CPRResult res;
  if (slot->has_even && slot->has_odd) {
    if (cpr_global(*slot, res)) {
      fo.latitude = res.lat;
      fo.longitude= res.lon;
      return;
    }
  }
  // Локальная (fallback), если есть опорная точка (здесь можно задать координаты приёмника)
  CPRResult loc;
  if (cpr_local(lat, lon, fflag, /*ref*/ 55.75f, 37.61f, loc)) {
    fo.latitude = loc.lat;
    fo.longitude= loc.lon;
  }
}

static void decode_alt(const uint8_t *me, DecodedADSB &fo) {
  // 12 бит AC + Q
  uint16_t ac = (get_bits(me, 8, 5) << 8) | get_bits(me, 13, 8);
  bool q = (ac & 0x10) != 0; // Q-bit внутри AC, согласно билдеру выше
  int feet = adsb_gillham_decode(ac & 0x0FFF, q);
  fo.pressure_altitude = (float)feet;
  fo.altitude = fo.pressure_altitude; // если нет гео
}

DecodedADSB adsb_decode_packet(const AdsbPacket &pkt) {
  DecodedADSB fo{}; memset(&fo, 0, sizeof(fo));
  uint32_t now_s = millis()/1000;
  fo.timestamp = now_s;
  fo.signal_source = 1;

  const uint8_t *b = pkt.raw;
  uint8_t df = get_bits(b, 0, 5);
  fo.addr = get_bits(b, 8, 24);

  if (df==17 || df==18) {
    uint8_t tc = get_bits(b, 32, 5);
    const uint8_t *me = &b[4]; // 56 бит ME для DF17 (pos 32..88)
    if (tc >= 1 && tc <= 4) {
      // идентификатор (позывной)
      for (int i=0;i<8;i++){
        uint8_t c6 = get_bits(me, 8 + i*6, 6);
        char c = ' ';
        if (c6 >= 1 && c6 <= 26) c = 'A' + c6 - 1;
        else if (c6 >= 48 && c6 <= 57) c = '0' + (c6 - 48);
        else if (c6==32) c = ' ';
        fo.flight[i] = c;
      }
      fo.flight[8] = 0;
    } else if (tc >= 9 && tc <= 18) {
      // Позиция (баро/гео)
      bool fflag = get_bits(me, 21-21, 1); // bit 53 от начала DF? — для простоты:
      // точнее:
      bool f = get_bits(me, 0, 1); // F (odd/even) — первый бит ME
      decode_alt(me, fo);
      decode_position_cpr(fo.addr, me, f, millis(), fo);
    } else if (tc == 19) {
      decode_tc19(me, fo);
    } else if (tc == 20 || tc==21) {
      decode_identity_squawk(me, fo);
    }
  }
  return fo;
}
