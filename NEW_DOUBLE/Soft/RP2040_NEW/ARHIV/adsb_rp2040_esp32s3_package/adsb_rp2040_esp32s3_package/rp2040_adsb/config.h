#pragma once
#include <Arduino.h>

enum class Profile { Normal, HighEMI, Urban, Remote };

struct Config {
  Profile profile = Profile::Normal;
  float base_thr = 10.0f;  // correlator base threshold
  float k_ema = 1.5f;      // EMA coefficient
  int dead_time_us = 7;    // dead time after detection (us)
  int min_pos_hits = 12;   // minimal hits across preamble windows
  float penalty_w = 1.0f;  // penalty weight
  bool output_raw = true;
  float ref_lat = 0.0f;    // local CPR reference
  float ref_lon = 0.0f;
};

const char* profile_name(Profile p);
void load_config(struct Config &cfg);
void save_config(const struct Config &cfg);
void print_config(const struct Config &cfg);
