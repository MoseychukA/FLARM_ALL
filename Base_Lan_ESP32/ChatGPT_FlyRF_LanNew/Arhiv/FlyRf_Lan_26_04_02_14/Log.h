#pragma once
#include <Arduino.h>

void Log_setup();
void Log_write(const char* level, const char* tag, const char* fmt, ...);

#define LOGE(tag, fmt, ...) Log_write("E", tag, fmt, ##__VA_ARGS__)
#define LOGW(tag, fmt, ...) Log_write("W", tag, fmt, ##__VA_ARGS__)
#define LOGI(tag, fmt, ...) Log_write("I", tag, fmt, ##__VA_ARGS__)
#define LOGD(tag, fmt, ...) Log_write("D", tag, fmt, ##__VA_ARGS__)
