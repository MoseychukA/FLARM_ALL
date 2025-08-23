#pragma once
#include <Arduino.h>
enum class Profile{ Normal, HighEMI, Urban, Remote };
struct Config{ Profile profile=Profile::Normal; float base_thr=10.0f; float k_ema=1.5f; int dead_time_us=7; int min_pos_hits=12; float penalty_w=1.0f; bool output_raw=true; float ref_lat=0.0f; float ref_lon=0.0f; };
const char* profile_name(Profile p);
void load_config(struct Config &cfg);
void save_config(const struct Config &cfg);
void print_config(const struct Config &cfg);
