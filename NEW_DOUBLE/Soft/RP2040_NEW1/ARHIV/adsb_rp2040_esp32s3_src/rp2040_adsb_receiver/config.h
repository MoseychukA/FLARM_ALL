#pragma once
#include <Arduino.h>
#include <LittleFS.h>

enum class Profile { Normal, HighEMI, Urban, Remote };

struct Config {
  Profile profile = Profile::Normal;
  bool filter_enabled = true;
  float min_us = 0.4f;
  float max_us = 0.7f;
  float norm_us = 0.5f;
  float base_thr = 0.25f;
  float k_dyn = 1.5f;
  float ema_alpha = 0.02f;
  double ref_lat = 55.7558;
  double ref_lon = 37.6176;

  String toString() const {
    String s; s += F("PROFILE=");
    switch(profile){case Profile::Normal:s+="Normal";break;case Profile::HighEMI:s+="High-EMI";break;case Profile::Urban:s+="Urban";break;case Profile::Remote:s+="Remote";break;}
    s += F(" FILTER="); s += filter_enabled?"ON":"OFF";
    s += F(" US=["); s += String(min_us,2); s+=","; s+=String(max_us,2); s+=","; s+=String(norm_us,2); s+="]";
    s += F(" THR="); s += String(base_thr,2); s += "+"; s += String(k_dyn,2); s += F("*EMA");
    s += F(" REF=("); s += String(ref_lat,6); s += ","; s+= String(ref_lon,6); s += ")";
    return s;
  }
};

void set_profile(Profile p);
Config get_config();
void save_config();
void load_config();
