#pragma once
#include <Arduino.h>
#include <vector>
#include <string.h>

// Raw message container
struct RawMessage {
  uint8_t channel;         // 0..2
  uint8_t payload[14];     // up to 112 bits
  uint8_t bytes;           // 7 or 14
  uint16_t bits;           // 56 or 112
  uint32_t t0_us;          // detected preamble start
};

// Lock-free packet queue (single-producer single-consumer per channel)
class LockFreePacketQueue {
public:
  void begin(size_t cap) {
    capacity = 1; while (capacity < cap) capacity <<= 1; // power of 2
    buf.resize(capacity);
    head = tail = 0;
  }
  bool push(const RawMessage &m) {
    uint32_t h = head.load(std::memory_order_relaxed);
    uint32_t t = tail.load(std::memory_order_acquire);
    if (((h+1) & (capacity-1)) == t) return false; // full
    buf[h] = m;
    head.store((h+1) & (capacity-1), std::memory_order_release);
    return true;
  }
  bool pop(RawMessage &m) {
    uint32_t t = tail.load(std::memory_order_relaxed);
    uint32_t h = head.load(std::memory_order_acquire);
    if (t == h) return false;
    m = buf[t];
    tail.store((t+1) & (capacity-1), std::memory_order_release);
    return true;
  }
private:
  std::vector<RawMessage> buf;
  std::atomic<uint32_t> head{0}, tail{0};
  uint32_t capacity{0};
};

// Sample FIFO per channel: hold recent samples for correlator
static const int SAMPLE_FIFO_SIZE = 4096; // bits per channel
struct SampleFifo { volatile uint32_t widx=0, ridx=0; uint8_t s[SAMPLE_FIFO_SIZE]; };

extern SampleFifo g_samp[3];

inline void push_samples(int ch, uint32_t word) {
  // Push 32 LSB-first samples (bit0 is first)
  uint32_t w = word;
  for (int i=0;i<32;i++) {
    uint8_t b = (w & 1u) ? 1 : 0; w >>= 1;
    uint32_t wi = g_samp[ch].widx & (SAMPLE_FIFO_SIZE-1);
    g_samp[ch].s[wi] = b;
    g_samp[ch].widx = (g_samp[ch].widx + 1) & (SAMPLE_FIFO_SIZE-1);
  }
}

struct CorrState {
  // dynamic thresholding with EMA of noise
  float base_thr;
  float k;
  float ema_noise;
  uint32_t dead_until; // in sample ticks (20MHz -> 50ns per tick)
};

struct CorrCfg {
  int preamble_len_samp; // 80 for 10MHz, here 160 for 20MHz
  int bit_samp;          // samples per 1.0us bit (20)
  int halfbit_samp;      // samples per 0.5us (10)
  int short_min;         // 0.4us min length (8)
  int long_max;          // 0.7us max length (14)
};

struct DecodedFrame; // fwd
class LockFreePacketQueue;

void init_corr_state(CorrState &st, const struct Config &cfg);
int run_correlator_and_slice(int ch, const struct Config &cfg, LockFreePacketQueue &outq);

extern LockFreePacketQueue pktq[3];
