#include "adsb_decoder.h"
#include "adsb_crc.h"
#include "adsb_cpr.h"
#include "adsb_gillham.h"
#include "config.h"

#include <Arduino.h>
#include <string.h>
#include <math.h>

#include "hardware/pio.h"
#include "hardware/dma.h"
#include "adsb_sampler.pio.h"

Profile g_profile = PROFILE_NORMAL;

// ====== RING BUFFERS PER CHANNEL ======
static volatile uint32_t *ring1 = nullptr;
static volatile uint32_t *ring2 = nullptr;
static volatile uint32_t *ring3 = nullptr;
static volatile uint32_t w1=0, r1=0, w2=0, r2=0, w3=0, r3=0;

static int dma1=-1, dma2=-1, dma3=-1;
static PIO pio = pio0;
static uint sm1=0, sm2=1, sm3=2;
static uint prog_offs = 0;

// Коррелятор/фильтр/EMA шума
static uint32_t lastDetSampleIdx[3] = {0,0,0};
static uint32_t sampleCounter = 0;
static bool digiFilterEnabled = DIGIFLT_ENABLE_DEFAULT;
static int emaNoise = 0;

// Очередь собранных пакетов
static AdsbPacket pktQ[MAX_PACKETS_QUEUE];
static volatile uint32_t qW=0, qR=0;

// Таймер меток
static inline uint32_t micros32() { return (uint32_t) (micros() & 0xFFFFFFFFu); }

// ====== Вспомогательные: добавление пакета в очередь ======
static inline void enqueuePkt(const AdsbPacket &p) {
  uint32_t n = (qW + 1) & (MAX_PACKETS_QUEUE-1);
  if (n == qR) return; // переполнение — отброс
  pktQ[qW] = p;
  qW = n;
}

bool adsb_dma_fetch(AdsbPacket *pkt) {
  if (qR == qW) return false;
  *pkt = pktQ[qR];
  qR = (qR + 1) & (MAX_PACKETS_QUEUE-1);
  return true;
}

// ====== Инициализация системы ======
void adsb_system_init() {
  pinMode(CH1_PRE_LED, OUTPUT);
  pinMode(CH2_PRE_LED, OUTPUT);
  pinMode(CH3_PRE_LED, OUTPUT);
  digitalWrite(CH1_PRE_LED, LOW);
  digitalWrite(CH2_PRE_LED, LOW);
  digitalWrite(CH3_PRE_LED, LOW);
}

// ====== Настройка PIO + DMA на 3 канала ======
void adsb_dma_init() {
  adsb_system_init();

  // Загружаем программу
  uint prog = pio_add_program(pio, &adsb_sampler_program);
  prog_offs = prog;

  // init SMs
  adsb_sampler_program_init(pio, sm1, prog_offs, CH1_PIN_IN);
  adsb_sampler_program_init(pio, sm2, prog_offs, CH2_PIN_IN);
  adsb_sampler_program_init(pio, sm3, prog_offs, CH3_PIN_IN);

  // clkdiv для 2 МГц (125 МГц / 62.5 = 2 МГц)
  float div = 125000000.0f / (float)SAMPLE_RATE_HZ;
  pio_sm_set_clkdiv(pio, sm1, div);
  pio_sm_set_clkdiv(pio, sm2, div);
  pio_sm_set_clkdiv(pio, sm3, div);

  // Пуск
  pio_sm_set_enabled(pio, sm1, true);
  pio_sm_set_enabled(pio, sm2, true);
  pio_sm_set_enabled(pio, sm3, true);

  // DMA кольца
  ring1 = (volatile uint32_t*)malloc(DMA_RING_WORDS * sizeof(uint32_t));
  ring2 = (volatile uint32_t*)malloc(DMA_RING_WORDS * sizeof(uint32_t));
  ring3 = (volatile uint32_t*)malloc(DMA_RING_WORDS * sizeof(uint32_t));
  memset((void*)ring1, 0, DMA_RING_WORDS*4);
  memset((void*)ring2, 0, DMA_RING_WORDS*4);
  memset((void*)ring3, 0, DMA_RING_WORDS*4);

  // Настройка 3 DMA каналов
  dma1 = dma_claim_unused_channel(true);
  dma2 = dma_claim_unused_channel(true);
  dma3 = dma_claim_unused_channel(true);

  dma_channel_config c1 = dma_channel_get_default_config(dma1);
  channel_config_set_transfer_data_size(&c1, DMA_SIZE_32);
  channel_config_set_read_increment(&c1, false);
  channel_config_set_write_increment(&c1, true);
  channel_config_set_dreq(&c1, pio_get_dreq(pio, sm1, false));
  dma_channel_configure(dma1, &c1,
                        (void*)ring1,         // dst
                        (const void*)&pio->rxf[sm1], // src
                        DMA_RING_WORDS, true);

  dma_channel_config c2 = dma_channel_get_default_config(dma2);
  channel_config_set_transfer_data_size(&c2, DMA_SIZE_32);
  channel_config_set_read_increment(&c2, false);
  channel_config_set_write_increment(&c2, true);
  channel_config_set_dreq(&c2, pio_get_dreq(pio, sm2, false));
  dma_channel_configure(dma2, &c2,
                        (void*)ring2,
                        (const void*)&pio->rxf[sm2],
                        DMA_RING_WORDS, true);

  dma_channel_config c3 = dma_channel_get_default_config(dma3);
  channel_config_set_transfer_data_size(&c3, DMA_SIZE_32);
  channel_config_set_read_increment(&c3, false);
  channel_config_set_write_increment(&c3, true);
  channel_config_set_dreq(&c3, pio_get_dreq(pio, sm3, false));
  dma_channel_configure(dma3, &c3,
                        (void*)ring3,
                        (const void*)&pio->rxf[sm3],
                        DMA_RING_WORDS, true);
}

void adsb_correlator_init() {
  emaNoise = 0;
}

// ====== Битовое чтение из кольца ======
static inline bool ring_has_data(volatile uint32_t w, volatile uint32_t r) {
  return w != r;
}
static inline uint32_t ring_next(uint32_t idx) { return (idx + 1) & (DMA_RING_WORDS-1); }

// «полусемплы»: в каждом 32-битном слове 32 последовательных семпла (битов LSB-first).
static inline int get_sample_bit(uint32_t word, int bitIdxLSB) {
  return (word >> bitIdxLSB) & 1;
}

// ====== Цифровой фильтр импульсов ======
static bool pulse_valid_len(int runLen) {
  if (!digiFilterEnabled) return true;
  return (runLen >= MIN_PULSE_SAMPLES) && (runLen <= MAX_PULSE_SAMPLES);
}

// ====== Коррелятор преамбулы c масками и EMA ======
struct CorrRes {
  bool hit;
  uint32_t t0Sample;  // позиция начала преамбулы в отсчётах
  int score;
};

static CorrRes correlate_preamble_on_wordstream(uint8_t ch,
                                                const uint32_t *buf, uint32_t rIdx, uint32_t wIdx) {
  CorrRes res{false,0,0};
  // Пробегаем по доступным словам; формируем последовательность семплов
  // и выполним корреляцию по окнам (центры ожидаемых импульсов).
  // Для простоты: проверяем последовательные биты, считаем posHits и penalty.
  int posCenters[6] = {0, 2, 6, 8, 12, 14}; // относительно t0 в полусемплах
  // Динамический порог
  int dynThr = BASE_THR + (K_THR * emaNoise) / 256;

  // Перебор доступных семплов — скользящее окно ~16 полуотсчётов
  // Для ускорения анализируем слово за словом.
  uint32_t curIdx = rIdx;
  uint32_t samplePos = sampleCounter;
  while (curIdx != wIdx) {
    uint32_t word = buf[curIdx];
    for (int b=0; b<32; ++b) {
      // dead-time
      if (samplePos - lastDetSampleIdx[ch-1] < PREAMBLE_DEADTIME_SAMPLES) {
        samplePos++;
        continue;
      }
      // Собираем 16 полуотсчётов — проверим окрестность
      // (здесь грубо: смотрим на биты в центрах, оцениваем score)
      int posHits = 0, negHits = 0, penalty = 0;

      for (int k=0; k<PRE_SAMPLES; ++k) {
        // получим семпл (текущий + k). Здесь упрощённо — только центр «окон»
        // для маски: если k не в posCenters±1 — считаем unexpected 1 → penalty.
        // Получим бит:
        // бит k лежит в последующих словах — для простоты здесь смотрим только ближайшие 16 внутри одного слова.
        int bit = 0;
        if (b + k < 32) bit = (word >> (b+k)) & 1;
        // взвешивание: если k близко к одному из posCenters → добавляем к posHits
        bool nearCenter = false;
        for (int c=0;c<6;c++) {
          if (abs(k - posCenters[c]) <= 1) { nearCenter = true; break; }
        }
        if (nearCenter) {
          if (bit) posHits++;
        } else {
          if (bit) { penalty += MASK_UNEXPECTED_PENALTY; }
          else { negHits++; }
        }
      }
      int score = posHits - penalty;
      // Порог + минимальные posHits
      if (score >= dynThr && posHits >= 4) {
        // Успех — t0 = samplePos
        res.hit = true;
        res.t0Sample = samplePos;
        res.score = score;
        lastDetSampleIdx[ch-1] = samplePos;
        digitalWrite(ch==1?CH1_PRE_LED:(ch==2?CH2_PRE_LED:CH3_PRE_LED), HIGH);
        return res;
      } else {
        // обновим EMA шума
        // emaNoise = emaNoise + alpha*(negHits - emaNoise)
        int e = negHits;
        emaNoise = emaNoise + ((e - emaNoise) * EMA_ALPHA_NUM) / EMA_ALPHA_DEN;
      }
      samplePos++;
    }
    curIdx = ring_next(curIdx);
  }
  return res;
}

// ====== Сборка бит из сэмплов в AdsbPacket.raw ======
static bool assemble_bits_and_form_packet(uint8_t ch,
                                          const uint32_t *buf, uint32_t rIdx, uint32_t wIdx,
                                          uint32_t t0Sample, AdsbPacket *out) {
  // После t0 идёт payload 56 или 112 бит, каждый по 1 мкс, PPM:
  // bit '1': импульс в [0..0.5) мкс окна бита; bit '0': импульс в [0.5..1.0) мкс.
  // Сэмплируем два полуотсчёта на бит (0 и 1). Решение: если первый полуотсчёт=1 → бит=1, если второй=1 → бит=0.
  // Считаем сначала 112 бит, если CRC не проходит и длина=112 → пробуем 56.
  uint8_t tmp[14] = {0};
  auto read_sample = [&](uint32_t sampleIndex)->int {
    // sampleIndex в глобальном счётчике (sampleCounter). Мы знаем rIdx..wIdx, но для краткости считаем, что окно ещё в буфере — проект на высоких скоростях должен копировать
    // Здесь реализуем чтение от текущего rIdx: упрощаем, берём текущее слово rIdx, а смещение — по локальной позиции внутри
    // Для надёжности в реале надо иметь большой буфер и адресацию по абсолютному sampleIndex.
    // В этой версии примем, что t0Sample совпадает с текущим словом в correlate(), и используем только последующие несколько слов — достаточны для 112*2=224 полуотсчётов.
    // Возьмём локальный указатель на первое слово после rIdx и сдвиг:
    (void)wIdx;
    uint32_t local = rIdx + ((sampleIndex - t0Sample) >> 5);
    uint32_t off   = (sampleIndex - t0Sample) & 31;
    local &= (DMA_RING_WORDS-1);
    uint32_t word = buf[local];
    return (word >> off) & 1;
  };

  auto try_len = [&](int nBits)->bool {
    memset(tmp, 0, sizeof(tmp));
    for (int i=0;i<nBits;i++) {
      uint32_t bitStart = t0Sample + PRE_SAMPLES + i*BIT_SAMPLES; // начало окна бита
      int s0 = read_sample(bitStart + 0);
      int s1 = read_sample(bitStart + 1);
      int bit = (s0 && !s1) ? 1 : (!s0 && s1) ? 0 :
                (s0 && s1) ? 1 : 0; // при коллизии считаем '1'
      // MSB-first упаковка
      int byte = i >> 3;
      int bitInByte = 7 - (i & 7);
      tmp[byte] |= (bit << bitInByte);
    }
    // Перед отдачей — проверка CRC (включая 24-бит паритет в конце)
    if (modes_crc_ok(tmp, nBits)) {
      memcpy(out->raw, tmp, (nBits+7)/8);
      out->bits = nBits;
      return true;
    }
    // Попытка коррекции 1 бита:
    uint8_t copy[14]; memcpy(copy, tmp, sizeof(copy));
    if (modes_crc_try_fix_1bit(copy, nBits)) {
      memcpy(out->raw, copy, (nBits+7)/8);
      out->bits = nBits;
      return true;
    }
    return false;
  };

  if (try_len(112)) return true;
  if (try_len(56))  return true;
  return false;
}

// ====== Обработка входящих данных по каждому каналу ======
static void process_channel(uint8_t ch,
                            volatile uint32_t *ring,
                            volatile uint32_t &rIdx,
                            volatile uint32_t &wIdx) {
  // Обновим wIdx по DMA: в простой схеме — читаем «обратный указатель»,
  // но в Arduino без IRQ: считаем, что DMA пишет по кругу, а мы «догоняем» — упрощённо примем,
  // что новые данные всегда есть (или используем heuristics).
  // В этой версии просто используем «видимые» слова: обработаем до wIdx==DMA_RING_WORDS/2 и сбросим (демо).
  // Для практики: настройте IRQ DMA и обновляйте wIdx в колбэке.
  // Здесь — прототип без IRQ: сканируем весь буфер один раз за вызов.
  uint32_t localW = DMA_RING_WORDS; // предположим, что запись прошла круг

  CorrRes cr = correlate_preamble_on_wordstream(ch, (const uint32_t*)ring, rIdx, localW);
  if (!cr.hit) return;

  AdsbPacket pkt{};
  pkt.ch = ch;
  pkt.timestamp = micros32();
  pkt.rssi = analogRead(RSSI_PIN);

  if (assemble_bits_and_form_packet(ch, (const uint32_t*)ring, rIdx, localW, cr.t0Sample, &pkt)) {
    enqueuePkt(pkt);
  }

  // сдвинем rIdx «примерно» за пакет (PRE_SAMPLES + nBits*2 полуотсчётов => слово)
  // приблизительно 16 + 224 = 240 полуотсчётов ~ 8 слов => +8
  rIdx = (rIdx + 8) & (DMA_RING_WORDS-1);

  // погасим LED через небольшой интервал (здесь сразу)
  digitalWrite(ch==1?CH1_PRE_LED:(ch==2?CH2_PRE_LED:CH3_PRE_LED), LOW);
}

bool adsb_parse(const AdsbPacket &pkt, DecodedADSB *out) {
  // Разбор DF, CA, ICAO, ME, CRC уже проверен
  const uint8_t *m = pkt.raw;
  int bits = pkt.bits;
  int nBytes = (bits+7)/8;

  auto getBits = [&](int from, int len)->uint32_t {
    // from — индекс бита MSB-first от 0
    uint32_t v=0;
    for (int i=0;i<len;i++) {
      int idx = from + i;
      int by = idx >> 3;
      int bi = 7 - (idx & 7);
      int bit = (m[by] >> bi) & 1;
      v = (v<<1) | bit;
    }
    return v;
  };

  int DF = getBits(0,5);
  int CA = getBits(5,3);
  uint32_t ICAO = getBits(8,24);
  uint32_t ME   = getBits(32,56);
  uint32_t TC   = (ME >> 51) & 0x1F;

  memset(out, 0, sizeof(*out));
  out->timestamp = time(NULL);
  out->addr = ICAO;
  out->addr_type = 0;
  out->signal_source = 1;
  out->seen = out->timestamp;

  // DF17/18: TC 1..4 (Flight ID), 5..18 (Position), 19 (Velocity), 20..22 (Surface), 28 (Status)
  if (DF==17 || DF==18) {
    if (TC >=1 && TC <=4) {
      // Flight ID: 8 6-битных символов (IA-5)
      static const char charset[64] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ#####_###############0123456789######";
      uint64_t data = ((uint64_t)ME) & 0x000FFFFFFFFFFFFFull;
      char call[9]={0};
      for (int i=0;i<8;i++) {
        int c = (data >> (42 - i*6)) & 0x3F;
        call[i] = (c>=0 && c<64) ? charset[c] : ' ';
      }
      strncpy(out->flight, call, 8);
    } else if ((TC >=9 && TC <=18) || (TC >=5 && TC <=8)) {
      // Position Airborne/Surface
      bool odd = ((ME >> 50) & 1) ? true:false;
      uint32_t yz = (ME >> 17) & 0x1FFFF;
      uint32_t xz = (ME >> 0)  & 0x1FFFF;
      bool globalOk=false;
      float lat, lon;
      if (TC>=5 && TC<=8) {
        cpr_decode_surface(out->addr, odd, yz, xz, &lat, &lon, &globalOk);
      } else {
        cpr_decode_airborne(out->addr, odd, yz, xz, &lat, &lon, &globalOk);
      }
      out->latitude = lat; out->longitude = lon;

      // Высота (если airborne TC 9..18)
      if (TC>=9 && TC<=18) {
        int q = (ME >> 20) & 1;
        uint16_t ac13 = ((ME >> 21) & 0x1FFF);
        int32_t feet = gillham_decode_ac13(ac13, q);
        out->altitude = feet * 0.3048f;
        out->pressure_altitude = out->altitude;
      }
    } else if (TC==19) {
      // Velocity
      int subtype = (ME >> 48) & 0x7;
      if (subtype==1 || subtype==2) {
        // Airspeed + heading (vector)
        int ew_dir = (ME >> 46) & 1;
        int ew_spd = (ME >> 36) & 0x3FF;
        int ns_dir = (ME >> 35) & 1;
        int ns_spd = (ME >> 25) & 0x3FF;
        int vert_src = (ME >> 24) & 1;
        int vert_dir = (ME >> 23) & 1;
        int vert_rate= (ME >> 14) & 0x1FF;
        float v_ew = (ew_dir? -1:1) * (float)(ew_spd - 1);
        float v_ns = (ns_dir? -1:1) * (float)(ns_spd - 1);
        float spd = sqrtf(v_ew*v_ew + v_ns*v_ns);
        float trk = atan2f(v_ew, v_ns) * 180.0f / (float)M_PI; if (trk<0) trk+=360;
        out->speed = spd; // в узлах (по стандарту)
        out->course = trk;
        out->vert_rate = (vert_dir? -1:1) * (vert_rate-1) * 64; // фт/мин
      } else if (subtype==3 || subtype==4) {
        // Ground speed / track (кн/истинная)
        int sup = (ME >> 47) & 1;
        int gs  = (ME >> 37) & 0x3FF;
        int trk_status = (ME >> 36) & 1;
        int trk = (ME >> 27) & 0x1FF;
        int vert_src = (ME >> 26) & 1;
        int vert_dir = (ME >> 25) & 1;
        int vr  = (ME >> 16) & 0x1FF;
        out->speed = (gs - 1); // узлы
        out->course = (trk * 360.0f) / 512.0f;
        out->vert_rate = (vert_dir? -1:1) * (vr-1) * 64;
      }
    } else if (TC==28) {
      // Aircraft status (emergency) — можно извлечь код приоритет/сквок
      // Для лаконичности — выставим типовой Squawk 7700/7600/7500 по полю
      int emerg = (ME >> 43) & 0x7;
      int squawk = 0;
      if (emerg==1) squawk=7500;
      else if (emerg==2) squawk=7600;
      else if (emerg==3) squawk=7700;
      if (squawk) out->Squawk = squawk;
    }
    return true;
  }

  // DF20/21 (Comm-B) могли бы нести Squawk (ID), но тут ограничимся базовым:
  if (DF==20 || DF==21) {
    // ME содержит BDS — разбор по таблицам BDS 1,0/6,0/6,1 можно добавить.
    // Здесь оставим как заготовку: Squawk не всегда доступен.
    // out->Squawk = ...
    return true;
  }

  return true;
}

// ====== Главный цикл «ядра 0»: обработка трёх каналов ======
static void service_all_channels() {
  process_channel(1, ring1, r1, w1);
  process_channel(2, ring2, r2, w2);
  process_channel(3, ring3, r3, w3);
}
