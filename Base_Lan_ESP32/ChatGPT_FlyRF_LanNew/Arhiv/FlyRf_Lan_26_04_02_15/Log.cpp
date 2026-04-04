#include "Log.h"
#include <stdarg.h>
#include <stdio.h>

static bool g_logReady = false;

void Log_setup()
{
    g_logReady = true;
}

void Log_write(const char* level, const char* tag, const char* fmt, ...)
{
    if (!g_logReady) return;
    char msg[192];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);
    Serial.printf("[%s][%s] %s\r\n", level ? level : "?", tag ? tag : "APP", msg);
}
