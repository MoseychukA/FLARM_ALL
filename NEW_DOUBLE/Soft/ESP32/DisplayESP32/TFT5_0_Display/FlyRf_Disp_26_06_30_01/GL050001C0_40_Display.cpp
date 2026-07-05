#include "GL050001C0_40_Display.h"

#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"

static const int LCD_WIDTH = 800;
static const int LCD_HEIGHT = 480;

#ifndef GL050001C0_40_PINMAP_CONFIRMED
#define GL050001C0_40_PINMAP_CONFIRMED 1
#endif

#if GL050001C0_40_PINMAP_CONFIRMED
static const int LCD_PIN_DE = 41;
static const int LCD_PIN_VSYNC = 42;
static const int LCD_PIN_HSYNC = 47;
static const int LCD_PIN_PCLK = 48;
static const int LCD_PIN_BL = 21;
static const int LCD_PIN_DISP_EN = -1;

static const int LCD_PIN_DATA[16] = {
  1, 2, 3, 4, 5,        // B0..B4
  6, 7, 10, 11, 12, 13, // G0..G5
  14, 15, 16, 18, 19    // R0..R4
};
#endif

static esp_lcd_panel_handle_t panel = NULL;
static uint16_t lineBuffer[LCD_WIDTH];
static bool ready = false;
static uint32_t lastStatusMs = 0;

static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

static const uint16_t COLOR_BG = rgb565(0, 10, 18);
static const uint16_t COLOR_BAR = rgb565(8, 34, 48);
static const uint16_t COLOR_TEXT = rgb565(235, 245, 248);
static const uint16_t COLOR_DIM = rgb565(120, 146, 156);
static const uint16_t COLOR_ACCENT = rgb565(0, 180, 150);
static const uint16_t COLOR_WARN = rgb565(255, 190, 40);

static void fillRect(int x, int y, int w, int h, uint16_t color)
{
  if (!ready || w <= 0 || h <= 0) {
    return;
  }
  if (x < 0) {
    w += x;
    x = 0;
  }
  if (y < 0) {
    h += y;
    y = 0;
  }
  if (x + w > LCD_WIDTH) {
    w = LCD_WIDTH - x;
  }
  if (y + h > LCD_HEIGHT) {
    h = LCD_HEIGHT - y;
  }
  if (w <= 0 || h <= 0) {
    return;
  }

  for (int i = 0; i < w; ++i) {
    lineBuffer[i] = color;
  }
  for (int row = 0; row < h; ++row) {
    esp_lcd_panel_draw_bitmap(panel, x, y + row, x + w, y + row + 1, lineBuffer);
  }
}

static const uint8_t *glyphFor(char c)
{
  static const uint8_t space[7] = {0, 0, 0, 0, 0, 0, 0};
  static const uint8_t dash[7] = {0, 0, 0, 0x1F, 0, 0, 0};
  static const uint8_t colon[7] = {0, 0x04, 0x04, 0, 0x04, 0x04, 0};
  static const uint8_t dot[7] = {0, 0, 0, 0, 0, 0x0C, 0x0C};
  static const uint8_t zero[7] = {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E};
  static const uint8_t one[7] = {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E};
  static const uint8_t two[7] = {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F};
  static const uint8_t three[7] = {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E};
  static const uint8_t four[7] = {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02};
  static const uint8_t five[7] = {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E};
  static const uint8_t six[7] = {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E};
  static const uint8_t seven[7] = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08};
  static const uint8_t eight[7] = {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E};
  static const uint8_t nine[7] = {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E};
  static const uint8_t letters[26][7] = {
    {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},
    {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E},
    {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E},
    {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E},
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F},
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10},
    {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F},
    {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},
    {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E},
    {0x01, 0x01, 0x01, 0x01, 0x11, 0x11, 0x0E},
    {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11},
    {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F},
    {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11},
    {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11},
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
    {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10},
    {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D},
    {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11},
    {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E},
    {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04},
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
    {0x11, 0x11, 0x11, 0x11, 0x0A, 0x0A, 0x04},
    {0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11},
    {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11},
    {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04},
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}
  };

  if (c >= 'a' && c <= 'z') {
    c -= 32;
  }
  if (c >= 'A' && c <= 'Z') {
    return letters[c - 'A'];
  }
  switch (c) {
    case '0': return zero;
    case '1': return one;
    case '2': return two;
    case '3': return three;
    case '4': return four;
    case '5': return five;
    case '6': return six;
    case '7': return seven;
    case '8': return eight;
    case '9': return nine;
    case '-': return dash;
    case ':': return colon;
    case '.': return dot;
    default: return space;
  }
}

static void drawChar(int x, int y, char c, uint16_t color, int scale)
{
  const uint8_t *glyph = glyphFor(c);
  for (int row = 0; row < 7; ++row) {
    for (int col = 0; col < 5; ++col) {
      if (glyph[row] & (1 << (4 - col))) {
        fillRect(x + col * scale, y + row * scale, scale, scale, color);
      }
    }
  }
}

static void drawText(int x, int y, const String &text, uint16_t color, int scale)
{
  int cursor = x;
  for (size_t i = 0; i < text.length(); ++i) {
    drawChar(cursor, y, text[i], color, scale);
    cursor += 6 * scale;
  }
}

static void drawShell()
{
  fillRect(0, 0, LCD_WIDTH, LCD_HEIGHT, COLOR_BG);
  fillRect(0, 0, LCD_WIDTH, 76, COLOR_BAR);
  fillRect(0, 74, LCD_WIDTH, 2, COLOR_ACCENT);
  drawText(28, 22, "FLYRF DISPLAY", COLOR_TEXT, 4);
  drawText(28, 430, "GL050001C0-40 RGB 800X480", COLOR_DIM, 2);
}

bool GL050001C0_40_setup()
{
#if !GL050001C0_40_PINMAP_CONFIRMED
  Serial.println(F("[GL050001C0-40] RGB panel is disabled: not enough free GPIOs in the current FlyRF pinout."));
  Serial.println(F("[GL050001C0-40] Move peripherals first, then set GL050001C0_40_PINMAP_CONFIRMED=1 and fill the pin map."));
  return false;
#else
  pinMode(LCD_PIN_BL, OUTPUT);
  digitalWrite(LCD_PIN_BL, LOW);

  esp_lcd_rgb_panel_config_t config = {};
  config.data_width = 16;
  config.psram_trans_align = 64;
  config.sram_trans_align = 4;
  config.disp_gpio_num = LCD_PIN_DISP_EN;
  config.pclk_gpio_num = LCD_PIN_PCLK;
  config.vsync_gpio_num = LCD_PIN_VSYNC;
  config.hsync_gpio_num = LCD_PIN_HSYNC;
  config.de_gpio_num = LCD_PIN_DE;
  for (int i = 0; i < 16; ++i) {
    config.data_gpio_nums[i] = LCD_PIN_DATA[i];
  }
  config.timings.pclk_hz = 16000000;
  config.timings.h_res = LCD_WIDTH;
  config.timings.v_res = LCD_HEIGHT;
  config.timings.hsync_pulse_width = 4;
  config.timings.hsync_back_porch = 8;
  config.timings.hsync_front_porch = 8;
  config.timings.vsync_pulse_width = 4;
  config.timings.vsync_back_porch = 8;
  config.timings.vsync_front_porch = 8;
  config.flags.fb_in_psram = true;

  esp_err_t err = esp_lcd_new_rgb_panel(&config, &panel);
  if (err != ESP_OK) {
    Serial.print(F("[GL050001C0-40] panel allocation failed: "));
    Serial.println(err);
    panel = NULL;
    return false;
  }

  err = esp_lcd_panel_reset(panel);
  if (err == ESP_OK) {
    err = esp_lcd_panel_init(panel);
  }
  if (err != ESP_OK) {
    Serial.print(F("[GL050001C0-40] panel init failed: "));
    Serial.println(err);
    panel = NULL;
    return false;
  }

  ready = true;
  digitalWrite(LCD_PIN_BL, HIGH);
  drawShell();
  Serial.println(F("[GL050001C0-40] RGB panel initialized"));
  return true;
#endif
}

void GL050001C0_40_showStartup(const String &version)
{
  if (!ready) {
    return;
  }

  drawShell();
  drawText(72, 145, "DECIMA", COLOR_ACCENT, 6);
  drawText(72, 235, "STARTING", COLOR_WARN, 4);
  drawText(72, 305, version, COLOR_TEXT, 3);
}

void GL050001C0_40_showStatus(uint32_t rxPackets, uint32_t txPackets)
{
  if (!ready) {
    return;
  }

  drawShell();
  drawText(72, 130, "AIR TRAFFIC DISPLAY", COLOR_TEXT, 4);
  drawText(72, 220, String("RX ") + String(rxPackets), COLOR_ACCENT, 4);
  drawText(72, 290, String("TX ") + String(txPackets), COLOR_ACCENT, 4);
  drawText(72, 370, "TFT ESPI RADAR UI NOT PORTED", COLOR_WARN, 2);
}

void GL050001C0_40_showPowerOff()
{
  if (!ready) {
    return;
  }

  drawShell();
  drawText(72, 165, "POWER OFF", COLOR_WARN, 5);
  drawText(72, 260, "PLEASE WAIT", COLOR_TEXT, 4);
}

void GL050001C0_40_loop()
{
  if (!ready || millis() - lastStatusMs < 1000UL) {
    return;
  }
  lastStatusMs = millis();
}
