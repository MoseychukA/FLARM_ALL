#include "config.h"

extern volatile bool filter_enabled;
extern volatile float filt_min_us, filt_max_us, normalize_to_us;
extern volatile float g_base_thr, ema_alpha, k_dyn;
extern volatile Profile current_profile;
//extern CPRContext cpr_ctx; // from main

static Config g_cfg;

void apply_cfg(){
  current_profile = g_cfg.profile;
  filter_enabled = g_cfg.filter_enabled;
  filt_min_us = g_cfg.min_us;
  filt_max_us = g_cfg.max_us;
  normalize_to_us = g_cfg.norm_us;
  g_base_thr = g_cfg.base_thr;
  k_dyn = g_cfg.k_dyn;
  ema_alpha = g_cfg.ema_alpha;
}

void set_profile(Profile p){ g_cfg.profile=p; apply_cfg(); }
Config get_config(){ return g_cfg; }

static const char *CFG_PATH = "/config.txt";

void save_config(){
  File f = LittleFS.open(CFG_PATH, "w");
  if (!f) return;
  f.printf("profile=%d\n", (int)g_cfg.profile);
  f.printf("filter=%d\n", g_cfg.filter_enabled?1:0);
  f.printf("min=%.3f max=%.3f norm=%.3f\n", g_cfg.min_us, g_cfg.max_us, g_cfg.norm_us);
  f.printf("thr=%.3f kd=%.3f ema=%.3f\n", g_cfg.base_thr, g_cfg.k_dyn, g_cfg.ema_alpha);
  f.printf("ref=%.6f,%.6f\n", g_cfg.ref_lat, g_cfg.ref_lon);
  f.close();
}

void load_config()
{
  if (!LittleFS.exists(CFG_PATH)) { apply_cfg(); return; }
  File f = LittleFS.open(CFG_PATH, "r"); if (!f) { apply_cfg(); return; }
  while (f.available()){
    String line = f.readStringUntil('\n'); line.trim();
    if (line.startsWith("profile=")) g_cfg.profile = (Profile) line.substring(8).toInt();
    else if (line.startsWith("filter=")) g_cfg.filter_enabled = (line.substring(7).toInt()!=0);
    else if (line.startsWith("min=")){
      int sp1=line.indexOf(' '); int sp2=line.indexOf(" norm=");
      g_cfg.min_us = line.substring(4, sp1).toFloat();
      g_cfg.max_us = line.substring(sp1+5, sp2).toFloat();
      g_cfg.norm_us= line.substring(sp2+6).toFloat();
    } else if (line.startsWith("thr=")){
      int sp1=line.indexOf(' '); int sp2=line.indexOf(" ema=");
      g_cfg.base_thr = line.substring(4, sp1).toFloat();
      g_cfg.k_dyn = line.substring(sp1+4, sp2).toFloat();
      g_cfg.ema_alpha = line.substring(sp2+4).toFloat();
    } else if (line.startsWith("ref=")){
      int comma=line.indexOf(',');
      g_cfg.ref_lat = line.substring(4, comma).toFloat();
      g_cfg.ref_lon = line.substring(comma+1).toFloat();
    }
  }
  f.close();
  apply_cfg();
}
