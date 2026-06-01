#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "AdsbReceiver.h"

extern TFT_eSPI tft;

class ScreenView {
public:
  void begin(const String &version);
  void update(const AdsbStats &stats, const AdsbMessage &last, bool hasMessage);
  void showMessage(const AdsbStats &stats, const AdsbMessage &msg);

private:
  uint32_t lastDrawMs = 0;
  uint32_t lastTouchMs = 0;
  String versionText;

  void drawHeader();
  void drawHex24(uint32_t value);
};
