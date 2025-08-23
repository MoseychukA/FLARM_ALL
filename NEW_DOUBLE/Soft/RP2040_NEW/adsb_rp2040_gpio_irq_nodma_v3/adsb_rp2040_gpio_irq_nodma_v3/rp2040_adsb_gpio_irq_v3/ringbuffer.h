#pragma once
#include <Arduino.h>
#include <vector>
#include <string.h>
#include <atomic>

struct RawMessage{ uint8_t channel; uint8_t payload[14]; uint8_t bytes; uint16_t bits; uint32_t t0_us; };
class LockFreePacketQueue{ public: void begin(size_t cap){ capacity=1; while(capacity<cap) capacity<<=1; buf.resize(capacity); head=tail=0;} bool push(const RawMessage&m){ uint32_t h=head.load(std::memory_order_relaxed); uint32_t t=tail.load(std::memory_order_acquire); if(((h+1)&(capacity-1))==t) return false; buf[h]=m; head.store((h+1)&(capacity-1), std::memory_order_release); return true;} bool pop(RawMessage&m){ uint32_t t=tail.load(std::memory_order_relaxed); uint32_t h=head.load(std::memory_order_acquire); if(t==h) return false; m=buf[t]; tail.store((t+1)&(capacity-1), std::memory_order_release); return true;} private: std::vector<RawMessage> buf; std::atomic<uint32_t> head{0}, tail{0}; uint32_t capacity{0}; };

struct Edge { uint32_t t; uint8_t level; };
static const int EDGE_BUF_SIZE = 8192;
struct EdgeBuf { volatile uint16_t widx=0, ridx=0; Edge e[EDGE_BUF_SIZE]; };
extern EdgeBuf g_edge[3];

struct Config{
  int profile; // 0 Normal, 1 High-EMI, 2 Urban, 3 Remote
  bool output_raw; int out_format; // 0 LOG, 1 CSV, 2 JSON
  int dead_time_us; int tol_us; int min_pulses; int min_win_hits; float base_thr; float ema_alpha; float dyn_k;
  bool invert[3];
  float ref_lat; float ref_lon; // for local CPR
};

int run_edge_correlator_and_slice(int ch, const struct Config &cfg, LockFreePacketQueue &outq);
