#pragma once
#include <Arduino.h>

struct BDS60 {
  int16_t vert_rate_fpm = 0;
  bool valid = false;
};

struct BDS61 {
  int16_t baro_geo_diff_ft = 0;
  bool valid = false;
};

BDS60 parse_bds60(const uint8_t *me);
BDS61 parse_bds61(const uint8_t *me);
