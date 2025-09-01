#pragma once
#include <Arduino.h>
inline void Serial_printHex(uint32_t v, int n){ for(int i=n-1;i>=0;i--){ uint8_t b=(v>>(i*4))&0xF; Serial.print((char)(b<10?'0'+b:'A'+b-10)); } }
