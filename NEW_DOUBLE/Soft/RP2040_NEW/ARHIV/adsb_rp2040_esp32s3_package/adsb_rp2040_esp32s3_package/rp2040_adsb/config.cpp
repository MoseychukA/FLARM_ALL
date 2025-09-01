#include "config.h"
#include <LittleFS.h>

const char* profile_name(Profile p) {
  switch(p){case Profile::Normal: return "Normal"; case Profile::HighEMI: return "High-EMI"; case Profile::Urban: return "Urban"; default: return "Remote"; }
}

void load_config(struct Config &cfg) {
  if (LittleFS.exists("/config.txt")) {
    File f = LittleFS.open("/config.txt", "r");
    if (f) {
      while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim(); if (!line.length()||line[0]=='#') continue;
        int eq = line.indexOf('='); if (eq<0) continue; String k=line.substring(0,eq); String v=line.substring(eq+1);
        k.trim(); v.trim();
        if (k=="profile") { if (v=="Normal") cfg.profile=Profile::Normal; else if(v=="High-EMI") cfg.profile=Profile::HighEMI; else if(v=="Urban") cfg.profile=Profile::Urban; else cfg.profile=Profile::Remote; }
        else if (k=="base_thr") cfg.base_thr=v.toFloat();
        else if (k=="k_ema") cfg.k_ema=v.toFloat();
        else if (k=="dead_time_us") cfg.dead_time_us=v.toInt();
        else if (k=="min_pos_hits") cfg.min_pos_hits=v.toInt();
        else if (k=="penalty_w") cfg.penalty_w=v.toFloat();
        else if (k=="output_raw") cfg.output_raw = (v=="1"||v=="true"||v=="on");
        else if (k=="ref_lat") cfg.ref_lat=v.toFloat();
        else if (k=="ref_lon") cfg.ref_lon=v.toFloat();
      }
      f.close();
    }
  }
}

void save_config(const struct Config &cfg) {
  File f = LittleFS.open("/config.txt", "w");
  if (!f) return;
  f.printf("profile=%s\n", profile_name(cfg.profile));
  f.printf("base_thr=%f\n", cfg.base_thr);
  f.printf("k_ema=%f\n", cfg.k_ema);
  f.printf("dead_time_us=%d\n", cfg.dead_time_us);
  f.printf("min_pos_hits=%d\n", cfg.min_pos_hits);
  f.printf("penalty_w=%f\n", cfg.penalty_w);
  f.printf("output_raw=%d\n", cfg.output_raw?1:0);
  f.printf("ref_lat=%f\n", cfg.ref_lat);
  f.printf("ref_lon=%f\n", cfg.ref_lon);
  f.close();
}

void print_config(const struct Config &cfg) {
  Serial.printf("Profile=%s base_thr=%.1f k_ema=%.2f dead=%dus minHits=%d penalty=%.2f RAW=%d ref=(%.4f,%.4f)\n",
    profile_name(cfg.profile), cfg.base_thr, cfg.k_ema, cfg.dead_time_us, cfg.min_pos_hits, cfg.penalty_w, cfg.output_raw?1:0, cfg.ref_lat, cfg.ref_lon);
}
