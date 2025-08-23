#include "ringbuffer.h"
#include <math.h>
#include <string.h>

EdgeBuf g_edge[3];

static inline bool edge_pop(int ch, Edge &out){ EdgeBuf &B=g_edge[ch]; if (B.ridx==B.widx) return false; out = B.e[B.ridx]; B.ridx = (B.ridx+1) & (EDGE_BUF_SIZE-1); return true; }

struct NoiseStat{ float ema=0; uint32_t last_t0=0; };
static NoiseStat ns[3];

struct Pulse{ uint32_t t_rise; uint32_t t_fall; float width; };

static int collect_window(int ch, uint32_t t0, int pre_us, int post_us, Pulse *out, int maxp){
  // Build pulses from edges around [t0-pre_us .. t0+post_us]
  // Simple pairing: keep last rise, on fall emit a pulse
  int pc=0; uint32_t last_rise=0; int have_rise=0; // copy current edge buffer snapshot
  // We cannot rewind edge buffer easily; instead scan last 256 edges like in previous version
  static Edge buf[3][256]; static int cnt[3]={0,0,0}; Edge e; while (edge_pop(ch,e)) { if (cnt[ch]<256) buf[ch][cnt[ch]++]=e; else { memmove(buf[ch], buf[ch]+1, sizeof(Edge)*255); buf[ch][255]=e; } }
  for (int i=0;i<cnt[ch];++i){ int dt = (int)buf[ch][i].t - (int)t0; if (dt < -pre_us) continue; if (dt > post_us) break; if (buf[ch][i].level){ last_rise=buf[ch][i].t; have_rise=1; } else if (have_rise){ uint32_t fall=buf[ch][i].t; float w = (float)((int)fall - (int)last_rise); if (pc<maxp){ out[pc++]={last_rise, fall, w}; } have_rise=0; }
  }
  return pc;
}

static float eval_preamble_score(uint32_t t0, const struct Config &cfg, Pulse *p, int pc){
  // Expected pulse center offsets (us) approximated: 0,1,2,5,6,8
  const int centers[6]={0,1,2,5,6,8}; int pos=0, neg=0, good=0;
  // width filtering
  for (int i=0;i<pc;i++){ if (p[i].width < cfg.w_short_us || p[i].width > cfg.w_long_us) { /*reject*/ } }
  for (int k=0;k<6;k++){
    int c=centers[k]; int hits=0; for (int i=0;i<pc;i++){ int ct = (int)((p[i].t_rise + p[i].t_fall)/2 - t0); if (ct < c - cfg.tol_us) continue; if (ct > c + cfg.tol_us) break; if (p[i].width >= cfg.w_short_us && p[i].width <= cfg.w_long_us){ hits++; } }
    if (hits < cfg.min_win_hits) return -1e9f; pos += hits; good++;
    if (k<5){ int mid=(centers[k]+centers[k+1])/2; int off=0; for(int i=0;i<pc;i++){ int ct = (int)((p[i].t_rise + p[i].t_fall)/2 - t0); if (ct < mid - cfg.tol_us) continue; if (ct > mid + cfg.tol_us) break; if (p[i].width >= cfg.w_short_us && p[i].width <= cfg.w_long_us) off++; } neg += off; }
  }
  return (float)pos - (float)neg;
}

int run_edge_correlator_and_slice(int ch, const struct Config &cfg, LockFreePacketQueue &outq){
  // Use recent edges to pick candidates (rising edges as t0)
  static Edge recent[3][256]; static int rcnt[3]={0,0,0}; Edge e; int pulled=0; while (edge_pop(ch,e)){ if (rcnt[ch]<256) recent[ch][rcnt[ch]++]=e; else { memmove(recent[ch], recent[ch]+1, sizeof(Edge)*255); recent[ch][255]=e; } pulled++; }
  if (rcnt[ch] < 16) return 0;

  float best=-1e9f; uint32_t best_t0=0; int preamble_found=0;
  for (int i=0;i<rcnt[ch]; ++i){ if (recent[ch][i].level!=1) continue; uint32_t t0 = recent[ch][i].t; if ((t0 - ns[ch].last_t0) < (uint32_t)cfg.dead_time_us) continue;
    Pulse pulses[64]; int pc = collect_window(ch, t0, 2, 12, pulses, 64); if (pc<3) continue;
    float sc = eval_preamble_score(t0, cfg, pulses, pc); if (sc > best){ best=sc; best_t0=t0; }
  }

  float dyn = cfg.base_thr + cfg.dyn_k * ns[ch].ema;
  if (best > -1e8f) preamble_found=1; // detected something even below dyn threshold
  if (best < dyn) { ns[ch].ema = (1.0f-cfg.ema_alpha)*ns[ch].ema + cfg.ema_alpha*(best>0?best:0); return preamble_found?2:0; }

  // Slice bits using edge density halves; decide 56/112 by energy after 56
  RawMessage rm; memset(&rm,0,sizeof(rm)); rm.channel=ch; rm.t0_us=best_t0; int ones_after_56=0;
  for (int b=0;b<112;b++){
    uint32_t a0 = best_t0 + 8 + b*1; int c1=0,c2=0; for (int k=0;k<rcnt[ch];k++){ uint32_t tt=recent[ch][k].t; if (tt>=a0 && tt<a0+1) c1++; else if (tt>=a0+1 && tt<a0+2) c2++; if (tt>a0+2) break; }
    int v = (c1>c2)?1:0; if (b>=56) ones_after_56 += (c1+c2); int byte=b>>3, bit=7-(b&7); if (v) rm.payload[byte] |= (1<<bit);
  }
  if (ones_after_56 < 3){ rm.bits=56; rm.bytes=7; } else { rm.bits=112; rm.bytes=14; }
  ns[ch].last_t0 = best_t0; outq.push(rm); return 1; }
