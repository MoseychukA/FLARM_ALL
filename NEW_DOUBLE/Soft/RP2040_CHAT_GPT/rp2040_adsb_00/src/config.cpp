#include "config.h"
#include <LittleFS.h>

FilterConfig g_filter;
CorrConfig   g_corr;
Profile      g_profile = PROFILE_NORMAL;

volatile float g_ref_lat = 55.751244f;
volatile float g_ref_lon = 37.618423f;
volatile bool  g_rawDump = true;

static const char* CFG = "/config.txt";

void applyProfile(Profile p){
  g_profile = p;
  if (p==PROFILE_NORMAL){
    g_corr.base_thr=8.0f; g_corr.k_ema=2.5f; g_corr.posHitsMin=2;
  } else if (p==PROFILE_HIGH_EMI){
    g_corr.base_thr=10.0f; g_corr.k_ema=3.5f; g_corr.posHitsMin=3;
  } else if (p==PROFILE_URBAN){
    g_corr.base_thr=11.0f; g_corr.k_ema=4.0f; g_corr.posHitsMin=3;
  } else if (p==PROFILE_REMOTE){
    g_corr.base_thr=7.0f; g_corr.k_ema=2.0f; g_corr.posHitsMin=2;
  }
}
void setProfile(Profile p){ applyProfile(p); }

void loadConfig(){
  LittleFS.begin();
  File f = LittleFS.open(CFG, "r");
  if (!f) { applyProfile(PROFILE_NORMAL); return; }
  int prof = f.parseInt(); g_ref_lat = f.parseFloat(); g_ref_lon = f.parseFloat();
  g_rawDump = f.parseInt();
  g_filter.enabled = f.parseInt();
  g_filter.min_us = f.parseFloat();
  g_filter.norm_us= f.parseFloat();
  g_filter.max_us = f.parseFloat();
  applyProfile((Profile)prof);
  f.close();
}
void saveConfig(){
  LittleFS.begin();
  File f = LittleFS.open(CFG, "w");
  if (!f) return;
  f.printf("%d %.6f %.6f %d %d %.3f %.3f %.3f\n",
    (int)g_profile, g_ref_lat, g_ref_lon, g_rawDump?1:0,
    g_filter.enabled?1:0, g_filter.min_us, g_filter.norm_us, g_filter.max_us);
  f.close();
}
void showConfig(){
  Serial.printf("Profile=%d ref=(%.6f,%.6f) rawDump=%d filter=[%d %.3f/%.3f/%.3f]\n",
    (int)g_profile, g_ref_lat, g_ref_lon, (int)g_rawDump,
    (int)g_filter.enabled, g_filter.min_us, g_filter.norm_us, g_filter.max_us);
}