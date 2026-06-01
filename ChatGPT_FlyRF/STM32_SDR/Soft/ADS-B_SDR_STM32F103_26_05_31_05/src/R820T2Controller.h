#pragma once

#include <Arduino.h>

class R820T2Controller {
public:
  bool begin();
  bool setFrequency(uint32_t hz);
  void setManualGain(uint8_t lna, uint8_t mixer, uint8_t vga);

private:
  static const uint8_t address = 0x1A;
  uint8_t cache[32] = {};
  bool present = false;

  bool writeReg(uint8_t reg, uint8_t value);
  bool writeMasked(uint8_t reg, uint8_t value, uint8_t mask);
  void setTrackingFilter(uint32_t hz);
  bool setPll(uint32_t loHz);
};
