#pragma once
#include <Arduino.h>
struct Settings { String mode; String profile; bool log_on; double ref_lat; double ref_lon; };
bool storage_init();
bool storage_save(const Settings& s);
bool storage_load(Settings& s);
