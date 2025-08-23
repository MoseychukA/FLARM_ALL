#pragma once
#include <Arduino.h>
enum class Profile{ Normal, HighEMI, Urban, Remote };
struct Config{ Profile profile=Profile::Normal; float base_thr=10.0f; float k_ema=1.5f; int dead_time_us=7; int min_pos_hits=12; float penalty_w=1.0f; bool output_raw=true; float ref_lat=55.93574f; float ref_lon=37.34873f; float ema_alpha=0.1f; float dyn_k=1.5f; int min_pos_hits_per_win=2; int pre_strict_tol=1; int short_min_samp=8; int long_max_samp=14; int out_format=0; uint32_t stat_qdrop=0; uint32_t stat_msgs=0; uint32_t stat_crc_fix=0; uint32_t stat_crc_fail=0; };
const char* profile_name(Profile p);
void load_config(struct Config &cfg);
void save_config(const struct Config &cfg);
void print_config(const struct Config &cfg);
