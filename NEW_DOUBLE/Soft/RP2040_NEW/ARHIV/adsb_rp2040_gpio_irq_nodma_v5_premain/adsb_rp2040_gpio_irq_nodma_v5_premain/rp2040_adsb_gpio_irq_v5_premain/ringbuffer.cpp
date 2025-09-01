#include "ringbuffer.h"
#include <math.h>
#include <string.h>

EdgeBuf g_edge[3];

static inline bool edge_pop(int ch, Edge &out){ EdgeBuf &B=g_edge[ch]; if (B.ridx==B.widx) return false; out = B.e[B.ridx]; B.ridx = (B.ridx+1) & (EDGE_BUF_SIZE-1); return true; }

struct NoiseStat{ float ema=0; uint32_t last_t0=0; };
static NoiseStat ns[3];

struct Pulse{ uint32_t t_rise; uint32_t t_fall; float width; };

static int collect_recent_edges(int ch, Edge *buf, int maxn){ static Edge recent[3][256]; static int rcnt[3]={0,0,0}; Edge e; while (edge_pop(ch,e)){ if (rcnt[ch]<256) recent[ch][rcnt[ch]++]=e; else { memmove(recent[ch], recent[ch]+1, sizeof(Edge)*255); recent[ch][255]=e; rcnt[ch]=256; } } int n=rcnt[ch]; if (n>maxn) n=maxn; if (n>0) memcpy(buf, recent[ch]+(rcnt[ch]-n), sizeof(Edge)*n); return n; }

static int pulses_from_edges(Edge *edges, int n, Pulse *out, int maxp){ int pc=0; uint32_t last_rise=0; bool have_rise=false; for(int i=0;i<n;i++){ if (edges[i].level){ last_rise=edges[i].t; have_rise=true; } else if (have_rise){ uint32_t fall=edges[i].t; float w=(float)((int)fall-(int)last_rise); if (pc<maxp) out[pc++]={last_rise,fall,w}; have_rise=false; } } return pc; }

static float eval_preamble_score(uint32_t t0, const struct Config &cfg, Pulse *p, int pc){ const int centers[6]={0,1,2,5,6,8}; int pos=0, neg=0; for (int k=0;k<6;k++){ int c=centers[k]; int hits=0; for(int i=0;i<pc;i++){ float w=p[i].width; if (w<cfg.w_short_us || w>cfg.w_long_us) continue; int ct=(int)((p[i].t_rise+p[i].t_fall)/2 - t0); if (ct < c - cfg.tol_us) continue; if (ct > c + cfg.tol_us) break; hits++; } if (hits < cfg.min_win_hits) return -1e9f; pos += hits; if (k<5){ int mid=(centers[k]+centers[k+1])/2; int off=0; for(int i=0;i<pc;i++){ float w=p[i].width; if (w<cfg.w_short_us || w>cfg.w_long_us) continue; int ct=(int)((p[i].t_rise+p[i].t_fall)/2 - t0); if (ct < mid - cfg.tol_us) continue; if (ct > mid + cfg.tol_us) break; off++; } neg += off; } } return (float)pos - (float)neg; }

int run_edge_correlator_and_slice(int ch, const struct Config &cfg, LockFreePacketQueue &outq){ Edge edges[256]; int n=collect_recent_edges(ch, edges, 256); if (n<8) return 0; float best=-1e9f; uint32_t best_t0=0; int preamble_found=0; for (int i=0;i<n;i++){ if (!edges[i].level) continue; uint32_t t0=edges[i].t; if ((t0 - ns[ch].last_t0) < (uint32_t)cfg.dead_time_us) continue; Pulse pulses[64]; int pc=pulses_from_edges(edges+i, n-i, pulses, 64); if (pc<3) continue; float sc=eval_preamble_score(t0, cfg, pulses, pc); if (sc>-1e8f) preamble_found=1; if (sc>best){ best=sc; best_t0=t0; } }
  float dyn=cfg.base_thr + cfg.dyn_k * ns[ch].ema; if (best < dyn){ ns[ch].ema=(1.0f-cfg.ema_alpha)*ns[ch].ema + cfg.ema_alpha*(best>0?best:0); return preamble_found?2:0; }
  RawMessage rm; memset(&rm,0,sizeof(rm)); rm.channel=ch; rm.t0_us=best_t0; Pulse pulses[128]; int pc2=pulses_from_edges(edges, n, pulses, 128); int energy_after_56=0; int bits=112; for(int b=0;b<112;b++){ uint32_t half1_c=best_t0+8+b*1+0; uint32_t half2_c=best_t0+8+b*1+1; int d1=9999,d2=9999; for(int i=0;i<pc2;i++){ int ct=(int)((pulses[i].t_rise+pulses[i].t_fall)/2); int d=abs(ct-(int)half1_c); if (d<d1) d1=d; d=abs(ct-(int)half2_c); if (d<d2) d2=d; } int v=(d1<d2)?1:0; int byte=b>>3, bit=7-(b&7); if (v) rm.payload[byte]|=(1<<bit); if (b>=56) energy_after_56 += (d1<3 || d2<3); }
  if (energy_after_56<2){ bits=56; } rm.bits=bits; rm.bytes=bits/8; ns[ch].last_t0=best_t0; outq.push(rm); return 1; }
