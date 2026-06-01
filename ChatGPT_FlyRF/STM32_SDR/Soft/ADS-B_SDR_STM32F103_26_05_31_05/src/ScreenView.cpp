#include "ScreenView.h"
#include "ProjectPins.h"

TFT_eSPI tft = TFT_eSPI();

void ScreenView::begin(const String &version)
{
  versionText = version;
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  drawHeader();
}

void ScreenView::drawHeader()
{
  tft.fillRect(0, 0, 320, 40, TFT_BLACK);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(4, 4);
  tft.print("ADS-B 1090 STM32F103");
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(4, 25);
  tft.print(versionText);
  tft.drawFastHLine(0, 39, 320, TFT_DARKGREY);
}

void ScreenView::drawHex24(uint32_t value)
{
  if (value < 0x100000UL) tft.print('0');
  if (value < 0x010000UL) tft.print('0');
  if (value < 0x001000UL) tft.print('0');
  if (value < 0x000100UL) tft.print('0');
  if (value < 0x000010UL) tft.print('0');
  tft.print(value, HEX);
}

void ScreenView::showMessage(const AdsbStats &stats, const AdsbMessage &msg)
{
  update(stats, msg, true);
}

void ScreenView::update(const AdsbStats &stats, const AdsbMessage &last, bool hasMessage)
{
  if (millis() - lastDrawMs < 250) return;
  lastDrawMs = millis();

  tft.setTextSize(2);
  tft.fillRect(0, 42, 320, 198, TFT_BLACK);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(4, 46);
  tft.print("R820T2 1090.000 MHz");

  tft.setCursor(4, 70);
  tft.print("ADC ");
  tft.print(stats.adcNow);
  tft.print(" TH ");
  tft.print(stats.threshold);

  tft.setCursor(4, 94);
  tft.print("PRE ");
  tft.print(stats.preambles);
  tft.print(" MSG ");
  tft.print(stats.frames);

  tft.setCursor(4, 118);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.print("OK ");
  tft.print(stats.validFrames);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.print(" CRC ");
  tft.print(stats.crcErrors);

  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setCursor(4, 148);
  if (hasMessage) {
    tft.print("DF ");
    tft.print(last.df);
    tft.print(" ICAO ");
    drawHex24(last.icao);

    tft.setCursor(4, 172);
    tft.print("TC ");
    tft.print(last.typeCode);
    tft.print(" RSSI ");
    tft.print(last.signal);

    tft.setCursor(4, 198);
    uint8_t n = last.bitCount / 8;
    if (n > 8) n = 8;
    for (uint8_t i = 0; i < n; i++) {
      if (last.raw[i] < 16) tft.print('0');
      tft.print(last.raw[i], HEX);
    }
  } else {
    tft.print("Waiting ADS-B frame");
  }

#ifdef TOUCH_CS
  uint16_t x, y;
  if (tft.getTouch(&x, &y) && millis() - lastTouchMs > 300) {
    lastTouchMs = millis();
    tft.setTextSize(1);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.fillRect(190, 222, 130, 14, TFT_BLACK);
    tft.setCursor(192, 224);
    tft.print("XPT ");
    tft.print(x);
    tft.print(',');
    tft.print(y);
  }
#endif
}
