#pragma once
#include <Arduino.h>
#include <vector>
#include <string.h>
#include <atomic>

struct RawMessage{ uint8_t channel; uint8_t payload[14]; uint8_t bytes; uint16_t bits; uint32_t t0_us; };
class LockFreePacketQueue{ public: void begin(size_t cap){ capacity=1; while(capacity<cap) capacity<<=1; buf.resize(capacity); head=tail=0;} bool push(const RawMessage&m){ uint32_t h=head.load(std::memory_order_relaxed); uint32_t t=tail.load(std::memory_order_acquire); if(((h+1)&(capacity-1))==t) return false; buf[h]=m; head.store((h+1)&(capacity-1), std::memory_order_release); return true;} bool pop(RawMessage&m){ uint32_t t=tail.load(std::memory_order_relaxed); uint32_t h=head.load(std::memory_order_acquire); if(t==h) return false; m=buf[t]; tail.store((t+1)&(capacity-1), std::memory_order_release); return true;} private: std::vector<RawMessage> buf; std::atomic<uint32_t> head{0}, tail{0}; uint32_t capacity{0}; };
static const int SAMPLE_FIFO_SIZE = 8192; struct SampleFifo{ volatile uint32_t widx=0, ridx=0; uint8_t s[SAMPLE_FIFO_SIZE]; };
extern SampleFifo g_samp[3];
inline void push_samples(int ch, uint32_t word){ uint32_t w=word; for(int i=0;i<32;i++){ uint8_t b=(w&1u)?1:0; w>>=1; uint32_t wi=g_samp[ch].widx & (SAMPLE_FIFO_SIZE-1); g_samp[ch].s[wi]=b; g_samp[ch].widx=(g_samp[ch].widx+1)&(SAMPLE_FIFO_SIZE-1);} }
struct CorrState{ float base_thr; float k; float ema_noise; uint32_t dead_until; };
struct CorrCfg{ int preamble_len_samp; int bit_samp; int halfbit_samp; int short_min; int long_max; };
struct Config; class LockFreePacketQueue; struct DecodedFrame;
void init_corr_state(CorrState &st, const struct Config &cfg);
int run_correlator_and_slice(int ch, const struct Config &cfg, LockFreePacketQueue &outq);
