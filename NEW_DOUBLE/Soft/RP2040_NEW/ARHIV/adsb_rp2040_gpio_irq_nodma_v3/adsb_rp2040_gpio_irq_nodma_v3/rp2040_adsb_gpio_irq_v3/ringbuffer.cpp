#include "ringbuffer.h"
#include <math.h>
#include <string.h>

EdgeBuf g_edge[3];

static inline bool edge_pop(int ch, Edge &out){ EdgeBuf &B=g_edge[ch]; if (B.ridx==B.widx) return false; out = B.e[B.ridx]; B.ridx = (B.ridx+1) & (EDGE_BUF_SIZE-1); return true; }

struct NoiseStat{ float ema=0; uint32_t last_t0=0; };
static NoiseStat ns[3];

static float eval_preamble_score(uint32_t t0, const struct Config &cfg, Edge *win, int wcount){
  const int centers[6] = {0,1,2,5,6,8};
  int posHits=0, negHits=0;
  for (int k=0;k<6;k++){
    int c = centers[k]; int wnd_hits=0;
    for (int i=0;i<wcount;i++){
      int dt = (int)win[i].t - (int)t0; if (dt < c - cfg.tol_us) continue; if (dt > c + cfg.tol_us) break; wnd_hits++;
    }
    if (wnd_hits < cfg.min_win_hits) return -1e9f;
    posHits += wnd_hits;
    if (k<5){ int mid=(centers[k]+centers[k+1])/2; int off=0; for(int i=0;i<wcount;i++){ int dt=(int)win[i].t-(int)t0; if (dt < mid - cfg.tol_us) continue; if (dt > mid + cfg.tol_us) break; off++; } negHits += off; }
  }
  return (float)posHits - 1.0f*(float)negHits;
}

int run_edge_correlator_and_slice(int ch, const struct Config &cfg, LockFreePacketQueue &outq){
  static Edge buf[3][256]; static int cnt[3]={0,0,0};
  Edge e; while (edge_pop(ch, e)){
    if (cnt[ch] < 256) buf[ch][cnt[ch]++] = e; else { memmove(buf[ch], buf[ch]+1, sizeof(Edge)*255); buf[ch][255]=e; }
  }
  if (cnt[ch] < 16) return 0;

  float best_score=-1e9f; uint32_t best_t0=0; int best_i=-1;
  for (int i=0;i<cnt[ch]; ++i){ if (buf[ch][i].level!=1) continue; uint32_t t0 = buf[ch][i].t; if ((t0 - ns[ch].last_t0) < (uint32_t)cfg.dead_time_us) continue;
    Edge win[64]; int wcount=0; for (int j=i;j<cnt[ch] && wcount<64;j++){ int dt=(int)buf[ch][j].t - (int)t0; if (dt < -1) continue; if (dt>10) break; win[wcount++]=buf[ch][j]; }
    if (wcount < 6) continue; float sc = eval_preamble_score(t0, cfg, win, wcount); if (sc > best_score){ best_score=sc; best_t0=t0; best_i=i; }
  }
  float dyn_thr = cfg.base_thr + cfg.dyn_k * ns[ch].ema;
  if (best_score < dyn_thr) { ns[ch].ema = (1.0f - cfg.ema_alpha)*ns[ch].ema + cfg.ema_alpha * (best_score>0?best_score:0); return 0; }

  RawMessage rm; memset(&rm,0,sizeof(rm)); rm.channel=ch; rm.bits=112; rm.bytes=14; rm.t0_us=best_t0;
  for (int b=0;b<112;b++){
    uint32_t a0 = best_t0 + 8 + b*1; int c1=0,c2=0; for (int k=best_i;k<cnt[ch];k++){ uint32_t tt=buf[ch][k].t; if (tt>=a0 && tt<a0+1) c1++; else if (tt>=a0+1 && tt<a0+2) c2++; if (tt>a0+2) break; }
    int v = (c1>c2)?1:0; int byte=b>>3, bit=7-(b&7); if (v) rm.payload[byte] |= (1<<bit);
  }
  ns[ch].last_t0 = best_t0;
  outq.push(rm);
  return 1;
}
