#include "GL050001C0_40_Display.h"

#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_idf_version.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "SmoothStartupFonts.h"
#include "SmoothRuntimeFonts.h"
#include "WebRF.h"
#include <math.h>
#include <string.h>

static const int LCD_WIDTH = 800;
static const int LCD_HEIGHT = 480;

#ifndef GL050001C0_40_PINMAP_CONFIRMED
#define GL050001C0_40_PINMAP_CONFIRMED 1
#endif

#if GL050001C0_40_PINMAP_CONFIRMED
static const int LCD_PIN_DE = 45;
static const int LCD_PIN_VSYNC = 19;
static const int LCD_PIN_HSYNC = 47;
static const int LCD_PIN_PCLK = 48;
static const int LCD_PIN_DISP_EN = -1;

static const int LCD_PIN_DATA[16] = {
  1, 2, 3, 4, 5,        // B0..B4
  6, 7, 10, 11, 12, 13, // G0..G5
  14, 15, 16, 17, 18    // R0..R4
};
#endif

static esp_lcd_panel_handle_t panel = NULL;
static uint16_t *frameBuffer = NULL;
static SemaphoreHandle_t vsyncSemaphore = NULL;
static bool ready = false;
static uint32_t startupShownMs = 0;
static bool operationalFrameShown = false;
static bool shellDrawn = false;
static bool stateAvailable = false;
static GL050001C0_40_State displayState = {};
static GL050001C0_40_State renderedState = {};
static bool renderedStateValid = false;
static uint32_t lastDisplayUpdateMs = 0;
static String startupVersion;

// Поле полёта может занимать весь экран при повороте. Ограничиваем его
// обновление одним кадром в секунду, уменьшая нагрузку RGB DMA и PSRAM.
static const uint32_t DISPLAY_UPDATE_INTERVAL_MS = 1000UL;

static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

static const uint16_t COLOR_TEXT = rgb565(235, 245, 248);
static const uint16_t COLOR_STATUS_TEXT = rgb565(150, 165, 170);
static const uint16_t COLOR_WARN = rgb565(255, 190, 40);
static const uint16_t COLOR_SCREEN_BG = rgb565(0, 8, 14);
static const uint16_t COLOR_GRID = rgb565(145, 180, 188);
static const uint16_t COLOR_GREEN = rgb565(80, 235, 80);
static const uint16_t COLOR_RED = rgb565(255, 55, 45);
static const uint16_t COLOR_ORANGE = rgb565(255, 135, 25);
static const uint16_t COLOR_OWN_AIRCRAFT = rgb565(255, 255, 255);

static void fillRect(int x, int y, int w, int h, uint16_t color)
{
  if (frameBuffer == NULL || w <= 0 || h <= 0) {
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

  for (int row = 0; row < h; ++row) {
    uint16_t *dst = frameBuffer + ((y + row) * LCD_WIDTH) + x;
    for (int col = 0; col < w; ++col) {
      dst[col] = color;
    }
  }
}

static void drawStartupBackground()
{
  if (frameBuffer == NULL) {
    return;
  }

  for (int y = 0; y < LCD_HEIGHT; ++y) {
    for (int x = 0; x < LCD_WIDTH; ++x) {
      const int leftLight = ((LCD_WIDTH - 1 - x) * 22) / (LCD_WIDTH - 1);
      const int centerGlow = (y < 250) ? ((250 - y) * 8) / 250 : 0;
      const uint8_t r = 2 + leftLight / 7;
      const uint8_t g = 18 + leftLight / 2 + centerGlow;
      const uint8_t b = 42 + leftLight + centerGlow * 2;
      frameBuffer[y * LCD_WIDTH + x] = rgb565(r, g, b);
    }
  }
}

static uint16_t blendRgb565(uint16_t background, uint16_t foreground, uint8_t alpha)
{
  if (alpha == 255) {
    return foreground;
  }
  if (alpha == 0) {
    return background;
  }

  const uint32_t inverse = 255 - alpha;
  const uint32_t red = (((background >> 11) & 0x1F) * inverse +
                        ((foreground >> 11) & 0x1F) * alpha + 127) / 255;
  const uint32_t green = (((background >> 5) & 0x3F) * inverse +
                          ((foreground >> 5) & 0x3F) * alpha + 127) / 255;
  const uint32_t blue = ((background & 0x1F) * inverse +
                         (foreground & 0x1F) * alpha + 127) / 255;
  return (red << 11) | (green << 5) | blue;
}

static void drawSmoothTextMask(const SmoothTextMask &mask)
{
  uint32_t pixelIndex = 0;
  uint32_t rleIndex = 0;
  const uint32_t pixelCount = (uint32_t)mask.width * mask.height;

  while (rleIndex + 1 < mask.rleSize && pixelIndex < pixelCount) {
    const uint8_t count = mask.rle[rleIndex++];
    const uint8_t alpha = mask.rle[rleIndex++];
    if (alpha == 0) {
      pixelIndex += count;
      continue;
    }

    for (uint8_t index = 0; index < count && pixelIndex < pixelCount; ++index, ++pixelIndex) {
      const int x = mask.x + pixelIndex % mask.width;
      const int y = mask.y + pixelIndex / mask.width;
      if (x >= 0 && x < LCD_WIDTH && y >= 0 && y < LCD_HEIGHT) {
        uint16_t &destination = frameBuffer[y * LCD_WIDTH + x];
        destination = blendRgb565(destination, mask.color, alpha);
      }
    }
  }
}

static void drawSmoothStartupText()
{
  // Последняя маска содержит номер старого тестового проекта. Номер текущей
  // сборки выводится динамически, поэтому старую маску не рисуем.
  const uint8_t fixedMaskCount = SMOOTH_STARTUP_TEXT_COUNT > 0 ?
                                 SMOOTH_STARTUP_TEXT_COUNT - 1 : 0;
  for (uint8_t index = 0; index < fixedMaskCount; ++index) {
    drawSmoothTextMask(SMOOTH_STARTUP_TEXTS[index]);
  }
}

static void setPixel(int x, int y, uint16_t color)
{
  if (frameBuffer != NULL && x >= 0 && x < LCD_WIDTH && y >= 0 && y < LCD_HEIGHT) {
    frameBuffer[y * LCD_WIDTH + x] = color;
  }
}

static void drawLine(int x0, int y0, int x1, int y1, uint16_t color)
{
  const int dx = abs(x1 - x0);
  const int sx = x0 < x1 ? 1 : -1;
  const int dy = -abs(y1 - y0);
  const int sy = y0 < y1 ? 1 : -1;
  int error = dx + dy;
  for (;;) {
    setPixel(x0, y0, color);
    if (x0 == x1 && y0 == y1) {
      break;
    }
    const int twiceError = error * 2;
    if (twiceError >= dy) {
      error += dy;
      x0 += sx;
    }
    if (twiceError <= dx) {
      error += dx;
      y0 += sy;
    }
  }
}

static void drawLine3px(int x0, int y0, int x1, int y1, uint16_t color)
{
  const int dx = x1 - x0;
  const int dy = y1 - y0;
  const float length = sqrtf((float)(dx * dx + dy * dy));
  if (length < 1.0f) {
    setPixel(x0, y0, color);
    return;
  }
  const int offsetX = (int)lroundf(-(float)dy / length);
  const int offsetY = (int)lroundf( (float)dx / length);
  drawLine(x0, y0, x1, y1, color);
  drawLine(x0 + offsetX, y0 + offsetY,
           x1 + offsetX, y1 + offsetY, color);
  drawLine(x0 - offsetX, y0 - offsetY,
           x1 - offsetX, y1 - offsetY, color);
}

static void drawCircle(int centerX, int centerY, int radius, uint16_t color)
{
  int x = radius;
  int y = 0;
  int error = 1 - radius;
  while (x >= y) {
    setPixel(centerX + x, centerY + y, color);
    setPixel(centerX + y, centerY + x, color);
    setPixel(centerX - y, centerY + x, color);
    setPixel(centerX - x, centerY + y, color);
    setPixel(centerX - x, centerY - y, color);
    setPixel(centerX - y, centerY - x, color);
    setPixel(centerX + y, centerY - x, color);
    setPixel(centerX + x, centerY - y, color);
    ++y;
    if (error < 0) {
      error += 2 * y + 1;
    } else {
      --x;
      error += 2 * (y - x) + 1;
    }
  }
}

static void fillCircle(int centerX, int centerY, int radius, uint16_t color)
{
  for (int y = -radius; y <= radius; ++y) {
    const int halfWidth = (int)sqrtf((float)(radius * radius - y * y));
    fillRect(centerX - halfWidth, centerY + y, halfWidth * 2 + 1, 1, color);
  }
}

static void drawRectOutline(int x, int y, int width, int height, uint16_t color)
{
  drawLine(x, y, x + width - 1, y, color);
  drawLine(x, y + height - 1, x + width - 1, y + height - 1, color);
  drawLine(x, y, x, y + height - 1, color);
  drawLine(x + width - 1, y, x + width - 1, y + height - 1, color);
}

static const SmoothGlyph *findSmoothGlyph(const SmoothFont &font, uint16_t codepoint)
{
  for (uint16_t index = 0; index < font.glyphCount; ++index) {
    if (font.glyphs[index].codepoint == codepoint) {
      return &font.glyphs[index];
    }
  }
  return NULL;
}

static uint16_t readUtf8Codepoint(const char *text, size_t length, size_t &index)
{
  const uint8_t first = (uint8_t)text[index++];
  if (first < 0x80 || index >= length) {
    return first;
  }
  if ((first & 0xE0) == 0xC0 && index < length) {
    return ((uint16_t)(first & 0x1F) << 6) | ((uint8_t)text[index++] & 0x3F);
  }
  if ((first & 0xF0) == 0xE0 && index + 1 < length) {
    const uint16_t codepoint = ((uint16_t)(first & 0x0F) << 12) |
                               ((uint16_t)((uint8_t)text[index] & 0x3F) << 6) |
                               ((uint8_t)text[index + 1] & 0x3F);
    index += 2;
    return codepoint;
  }
  return '?';
}

static void drawRuntimeGlyph(int originX, int baselineY, const SmoothGlyph &glyph, uint16_t color)
{
  uint32_t pixelIndex = 0;
  uint16_t rleIndex = 0;
  const uint32_t pixelCount = (uint32_t)glyph.width * glyph.height;
  while (rleIndex + 1 < glyph.rleSize && pixelIndex < pixelCount) {
    const uint8_t count = glyph.rle[rleIndex++];
    const uint8_t alpha = glyph.rle[rleIndex++];
    if (alpha == 0) {
      pixelIndex += count;
      continue;
    }
    for (uint8_t index = 0; index < count && pixelIndex < pixelCount; ++index, ++pixelIndex) {
      const int x = originX + glyph.xOffset + pixelIndex % glyph.width;
      const int y = baselineY + glyph.yOffset + pixelIndex / glyph.width;
      if (x >= 0 && x < LCD_WIDTH && y >= 0 && y < LCD_HEIGHT) {
        uint16_t &destination = frameBuffer[y * LCD_WIDTH + x];
        destination = blendRgb565(destination, color, alpha);
      }
    }
  }
}

static int smoothTextWidth(const char *text, const SmoothFont &font)
{
  if (text == NULL) {
    return 0;
  }
  int width = 0;
  const size_t length = strlen(text);
  size_t index = 0;
  while (index < length) {
    const SmoothGlyph *glyph = findSmoothGlyph(font, readUtf8Codepoint(text, length, index));
    width += glyph != NULL ? glyph->xAdvance : font.lineHeight / 2;
  }
  return width;
}

static void drawSmoothString(int x, int baselineY, const char *text,
                             uint16_t color, const SmoothFont &font)
{
  if (text == NULL) {
    return;
  }
  const size_t length = strlen(text);
  size_t index = 0;
  int cursorX = x;
  while (index < length) {
    const SmoothGlyph *glyph = findSmoothGlyph(font, readUtf8Codepoint(text, length, index));
    if (glyph != NULL) {
      drawRuntimeGlyph(cursorX, baselineY, *glyph, color);
      cursorX += glyph->xAdvance;
    } else {
      cursorX += font.lineHeight / 2;
    }
  }
}

static void drawSmoothStringCentered(int centerX, int baselineY, const char *text,
                                     uint16_t color, const SmoothFont &font)
{
  drawSmoothString(centerX - smoothTextWidth(text, font) / 2,
                   baselineY, text, color, font);
}

static uint16_t targetColor(uint8_t alarmLevel)
{
  if (alarmLevel >= 3) {
    return COLOR_RED;
  }
  if (alarmLevel == 2) {
    return COLOR_ORANGE;
  }
  if (alarmLevel == 1) {
    return COLOR_WARN;
  }
  return COLOR_TEXT;
}

static void drawBatteryIcon(int x, int y, uint8_t percent, bool valid)
{
  const uint16_t border = valid ? COLOR_TEXT : COLOR_GRID;
  const uint16_t levelColor = !valid ? COLOR_GRID :
                              (percent > 50 ? COLOR_GREEN :
                               (percent > 20 ? COLOR_WARN : COLOR_RED));
  drawRectOutline(x, y, 45, 20, border);
  fillRect(x + 45, y + 6, 4, 8, border);
  // Пять сегментов соответствуют шагу 20%. При 0% все сегменты пусты,
  // при 100% заполнены все пять. Процент рассчитан по диапазону 7...12 В.
  const int segments = valid && percent > 0 ? ((percent + 19) / 20) : 0;
  for (int segment = 0; segment < 5; ++segment) {
    fillRect(x + 4 + segment * 8, y + 4, 6, 12,
             segment < segments ? levelColor : COLOR_SCREEN_BG);
  }
}

static void drawTargetTable()
{
  if (!displayState.tableVisible || displayState.trackerMessageVisible) {
    return;
  }
  static const int columnX[9] = {40, 125, 210, 295, 380, 465, 565, 690, 766};
  static const char *headers[9] = {
    "ICAO", "SQUAWK", "FLIGHT", "ALT", "SPEED",
    "COURSE", "LATITUDE", "LONGITUDE", "SIG"
  };
  for (uint8_t column = 0; column < 9; ++column) {
    drawSmoothStringCentered(columnX[column], 54, headers[column], COLOR_OWN_AIRCRAFT, FONT_TINY);
  }
  drawLine(8, 59, 792, 59, COLOR_OWN_AIRCRAFT);

  // Заголовки являются постоянной частью интерфейса. Без подтверждённого
  // пакета скрываем только строки с принятыми данными самолётов.
  if (!displayState.baseConnected) {
    return;
  }

  const uint8_t rows = displayState.tableTargetCount < 3 ?
                       displayState.tableTargetCount : 3;
  for (uint8_t row = 0; row < rows; ++row) {
    const GL050001C0_40_Target &target = displayState.tableTargets[row];
    const int baseline = 76 + row * 20;
    const uint16_t color = COLOR_OWN_AIRCRAFT;
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%06lX", (unsigned long)(target.address & 0xFFFFFFUL));
    drawSmoothStringCentered(columnX[0], baseline, buffer, color, FONT_TINY);
    if (target.squawk != 0) {
      snprintf(buffer, sizeof(buffer), "%u", (unsigned)target.squawk);
      drawSmoothStringCentered(columnX[1], baseline, buffer, color, FONT_TINY);
    }
    drawSmoothStringCentered(columnX[2], baseline,
                             target.callsign[0] ? target.callsign : "--------",
                             color, FONT_TINY);
    snprintf(buffer, sizeof(buffer), "%d", (int)target.altitudeM);
    drawSmoothStringCentered(columnX[3], baseline, buffer, color, FONT_TINY);
    snprintf(buffer, sizeof(buffer), "%u", (unsigned)target.speedKmh);
    drawSmoothStringCentered(columnX[4], baseline, buffer, color, FONT_TINY);
    snprintf(buffer, sizeof(buffer), "%03u", (unsigned)target.courseDeg);
    drawSmoothStringCentered(columnX[5], baseline, buffer, color, FONT_TINY);
    if (target.latitude != 0.0f)
      snprintf(buffer, sizeof(buffer), "%.5f", target.latitude);
    else
      snprintf(buffer, sizeof(buffer), "---");
    drawSmoothStringCentered(columnX[6], baseline, buffer, color, FONT_TINY);
    if (target.longitude != 0.0f)
      snprintf(buffer, sizeof(buffer), "%.5f", target.longitude);
    else
      snprintf(buffer, sizeof(buffer), "---");
    drawSmoothStringCentered(columnX[7], baseline, buffer, color, FONT_TINY);
    if (target.signalRssi != 0) {
      snprintf(buffer, sizeof(buffer), "%d", (int)target.signalRssi);
    } else {
      snprintf(buffer, sizeof(buffer), "--");
    }
    drawSmoothStringCentered(columnX[8], baseline, buffer, color, FONT_TINY);
  }
}

static uint16_t chooseRadarRangeMeters()
{
  if (displayState.radarRangeM != 0) {
    return displayState.radarRangeM;
  }
  uint32_t nearest = 64000;
  for (uint8_t index = 0; index < displayState.targetCount; ++index) {
    const uint32_t distance = displayState.targets[index].distanceM;
    if (distance > 0 && distance < nearest) {
      nearest = distance;
    }
  }
  if (nearest > 16000) return 32000;
  if (nearest >  8000) return 16000;
  if (nearest >  4000) return  8000;
  if (nearest >  2000) return  4000;
  if (nearest >  1000) return  2000;
  return 1000;
}

static void drawRadar()
{
  static const float DEG_TO_RAD_LOCAL = 0.01745329252f;
  const int centerX = 400;
  const int centerY = 360;
  const int outerRadius = 350;
  const int targetRadius = 336;
  // Без базы сохраняем статическое координатное поле в положении "север
  // вверх". Значения курса и целей из последнего пакета не используем.
  const uint16_t effectiveCourse = displayState.baseConnected ?
                                   displayState.courseDeg : 0U;
  const uint16_t rangeMeters = displayState.baseConnected ?
                               chooseRadarRangeMeters() : 32000U;

  drawCircle(centerX, centerY, 180, COLOR_GRID);
  drawLine(centerX - 8, centerY, centerX + 8, centerY, COLOR_GRID);
  drawLine(centerX, centerY - 8, centerX, centerY + 8, COLOR_GRID);

  // Полная замена pushRotated(): шкала компаса математически вращается на
  // отрицательный курс собственного самолёта. Сам самолёт остаётся по центру.
  for (int bearing = 0; bearing < 360; bearing += 10) {
    const float radians = (bearing - (int)effectiveCourse) * DEG_TO_RAD_LOCAL;
    const int outerX = centerX + (int)lroundf(sinf(radians) * outerRadius);
    const int outerY = centerY - (int)lroundf(cosf(radians) * outerRadius);
    const int tickLength = (bearing % 30 == 0) ? 14 : 6;
    const int innerX = centerX + (int)lroundf(sinf(radians) * (outerRadius - tickLength));
    const int innerY = centerY - (int)lroundf(cosf(radians) * (outerRadius - tickLength));
    drawLine(innerX, innerY, outerX, outerY, COLOR_GRID);
  }

  static const char *cardinalText[4] = {"N", "E", "S", "W"};
  static const int cardinalBearing[4] = {0, 90, 180, 270};
  for (int cardinal = 0; cardinal < 4; ++cardinal) {
    const float radians = (cardinalBearing[cardinal] - (int)effectiveCourse) *
                          DEG_TO_RAD_LOCAL;
    const int labelRadius = outerRadius - 25;
    const int labelX = centerX + (int)lroundf(sinf(radians) * labelRadius);
    const int labelY = centerY - (int)lroundf(cosf(radians) * labelRadius) + 8;
    drawSmoothStringCentered(labelX, labelY, cardinalText[cardinal],
                             COLOR_GRID, FONT_MEDIUM);
  }

  drawLine(centerX - 1, centerY - 15, centerX - 1, centerY + 12, COLOR_OWN_AIRCRAFT);
  drawLine(centerX, centerY - 15, centerX, centerY + 12, COLOR_OWN_AIRCRAFT);
  drawLine(centerX + 1, centerY - 15, centerX + 1, centerY + 12, COLOR_OWN_AIRCRAFT);
  drawLine(centerX - 14, centerY - 3, centerX + 12, centerY - 3, COLOR_OWN_AIRCRAFT);
  drawLine(centerX - 18, centerY - 1, centerX + 17, centerY - 1, COLOR_OWN_AIRCRAFT);
  drawLine(centerX - 8, centerY + 12, centerX + 8, centerY + 12, COLOR_OWN_AIRCRAFT);

  char rangeText[20];
  snprintf(rangeText, sizeof(rangeText), "%u km", (unsigned)(rangeMeters / 1000));
  // Исходная рамка (216,302,56,17), масштаб 1,5 и горизонтальное поле 40 px.
  drawRectOutline(364, 453, 84, 26, COLOR_RED);
  drawSmoothStringCentered(406, 473, rangeText, COLOR_TEXT, FONT_SMALL);

  // Круги, шкала, стороны света, собственный самолёт и надпись дальности
  // остаются. Ни одна цель из устаревшего RS485-пакета не выводится.
  if (!displayState.baseConnected) {
    return;
  }

  for (uint8_t index = 0; index < displayState.targetCount; ++index) {
    const GL050001C0_40_Target &target = displayState.targets[index];
    const uint32_t maxVisibleMeters = (uint32_t)rangeMeters * 2U;
    if (target.distanceM == 0 || target.distanceM > maxVisibleMeters) {
      continue;
    }
    const float relativeAngle = ((int)target.bearingDeg - (int)effectiveCourse) * DEG_TO_RAD_LOCAL;
    float radius;
    if (target.distanceM <= rangeMeters) {
      radius = ((float)target.distanceM * 180.0f) / rangeMeters;
    } else {
      radius = 180.0f + ((float)(target.distanceM - rangeMeters) *
                         (targetRadius - 180.0f)) / rangeMeters;
    }
    const int x = centerX + (int)lroundf(sinf(relativeAngle) * radius);
    const int y = centerY - (int)lroundf(cosf(relativeAngle) * radius);
    // Таблица занимает верхнюю часть экрана. Цели, попавшие за её границу,
    // не рисуем поверх заголовков и не включаем в частичную радарную область.
    if (y < 130) {
      continue;
    }
    const uint16_t color = targetColor(target.alarmLevel);
    // Диаметр символа цели уменьшен примерно на 25%: радиус 10 -> 8 px.
    fillCircle(x, y, 8, color);
    const float courseAngle = ((int)target.courseDeg - (int)effectiveCourse) * DEG_TO_RAD_LOCAL;
    const int courseX = x + (int)lroundf(sinf(courseAngle) * 30);
    const int courseY = y - (int)lroundf(cosf(courseAngle) * 30);
    drawLine3px(x, y, courseX, courseY, color);
    char label[20];
    snprintf(label, sizeof(label), "%+d", (int)target.relativeAltitudeM);
    drawSmoothString(x + 16, y - 7, label, color, FONT_SMALL);
    if (target.verticalRate > 0) {
      drawLine(x + 14, y + 22, x + 14, y + 2, color);
      drawLine(x + 14, y + 2, x + 7, y + 9, color);
      drawLine(x + 14, y + 2, x + 21, y + 9, color);
    } else if (target.verticalRate < 0) {
      drawLine(x + 14, y + 2, x + 14, y + 22, color);
      drawLine(x + 14, y + 22, x + 7, y + 15, color);
      drawLine(x + 14, y + 22, x + 21, y + 15, color);
    }
  }
}

static void drawStatusBlocks()
{
  char buffer[48];
  if (displayState.showLoraStatus) {
    if (displayState.baseConnected)
      snprintf(buffer, sizeof(buffer), "LoRa Tx %lu", (unsigned long)displayState.loraTxPackets);
    else
      snprintf(buffer, sizeof(buffer), "LoRa Tx ---");
    drawSmoothString(8, 412, buffer, COLOR_STATUS_TEXT, FONT_TINY);
    if (displayState.baseConnected)
      snprintf(buffer, sizeof(buffer), "LoRa Rx %lu", (unsigned long)displayState.loraRxPackets);
    else
      snprintf(buffer, sizeof(buffer), "LoRa Rx ---");
    drawSmoothString(8, 433, buffer, COLOR_STATUS_TEXT, FONT_TINY);
    if (displayState.baseConnected && displayState.loraRfHz != 0) {
      snprintf(buffer, sizeof(buffer), "LoRa RF %.3f", displayState.loraRfHz / 1000000.0f);
    } else {
      snprintf(buffer, sizeof(buffer), "LoRa RF ---");
    }
    drawSmoothString(8, 454, buffer, COLOR_STATUS_TEXT, FONT_TINY);
    if (displayState.baseConnected && displayState.loraRssiDb < 0) {
      snprintf(buffer, sizeof(buffer), "LoRa RSSI %d dB", (int)displayState.loraRssiDb);
    } else {
      snprintf(buffer, sizeof(buffer), "LoRa RSSI ---");
    }
    drawSmoothString(8, 475, buffer, COLOR_STATUS_TEXT, FONT_TINY);
  }

  if (displayState.showLanStatus) {
    if (displayState.baseConnected)
      snprintf(buffer, sizeof(buffer), "LAN %s  UDP %s",
               displayState.lanLinkUp ? "On" : "Off",
               displayState.lanUdpWorking ? "On" : "Off");
    else
      snprintf(buffer, sizeof(buffer), "LAN ---  UDP ---");
    drawSmoothString(600, 412, buffer, COLOR_STATUS_TEXT, FONT_TINY);
    if (displayState.baseConnected)
      snprintf(buffer, sizeof(buffer), "IP %u.%u.%u.%u",
               (unsigned)displayState.lanIp[0], (unsigned)displayState.lanIp[1],
               (unsigned)displayState.lanIp[2], (unsigned)displayState.lanIp[3]);
    else
      snprintf(buffer, sizeof(buffer), "IP ---");
    drawSmoothString(600, 433, buffer, COLOR_STATUS_TEXT, FONT_TINY);
    if (displayState.baseConnected)
      snprintf(buffer, sizeof(buffer), "UDP %u", (unsigned)displayState.lanUdpPort);
    else
      snprintf(buffer, sizeof(buffer), "UDP ---");
    drawSmoothString(600, 454, buffer, COLOR_STATUS_TEXT, FONT_TINY);
    if (displayState.baseConnected)
      snprintf(buffer, sizeof(buffer), "LAN %lu/%lu",
               (unsigned long)displayState.lanTxPackets,
               (unsigned long)displayState.lanRxPackets);
    else
      snprintf(buffer, sizeof(buffer), "LAN ---/---");
    drawSmoothString(600, 475, buffer, COLOR_STATUS_TEXT, FONT_TINY);
  } else if (displayState.showGpsStatus) {
    if (displayState.baseConnected)
      snprintf(buffer, sizeof(buffer), "GPS Sat %u", (unsigned)displayState.satellites);
    else
      snprintf(buffer, sizeof(buffer), "GPS Sat ---");
    drawSmoothString(615, 433, buffer, COLOR_STATUS_TEXT, FONT_TINY);
    if (displayState.baseConnected && displayState.gnssValid &&
        displayState.latitude != 0.0f) {
      snprintf(buffer, sizeof(buffer), "GPS Lat %.5f", displayState.latitude);
      drawSmoothString(615, 454, buffer, COLOR_STATUS_TEXT, FONT_TINY);
    } else {
      drawSmoothString(615, 454, "GPS Lat ---", COLOR_STATUS_TEXT, FONT_TINY);
    }
    if (displayState.baseConnected && displayState.gnssValid &&
        displayState.longitude != 0.0f) {
      snprintf(buffer, sizeof(buffer), "GPS Lon %.5f", displayState.longitude);
      drawSmoothString(615, 475, buffer, COLOR_STATUS_TEXT, FONT_TINY);
    } else {
      drawSmoothString(615, 475, "GPS Lon ---", COLOR_STATUS_TEXT, FONT_TINY);
    }
  }
}

static void drawTopBar()
{
  char buffer[48];
  if (displayState.baseConnected && displayState.timeValid) {
    snprintf(buffer, sizeof(buffer), "%02u:%02u",
             (unsigned)displayState.hour, (unsigned)displayState.minute);
  } else {
    snprintf(buffer, sizeof(buffer), "--:--");
  }
  drawSmoothString(8, 32, buffer, COLOR_GREEN, FONT_LARGE);

  if (displayState.showPowerVoltage || displayState.showPowerCurrent) {
    if (displayState.powerValid) {
      if (displayState.showPowerVoltage && displayState.showPowerCurrent) {
        snprintf(buffer, sizeof(buffer), "%.1f V  %.0f mA",
                 displayState.voltageV, displayState.currentMa);
      } else if (displayState.showPowerVoltage) {
        snprintf(buffer, sizeof(buffer), "%.1f V", displayState.voltageV);
      } else {
        snprintf(buffer, sizeof(buffer), "%.0f mA", displayState.currentMa);
      }
    } else {
      snprintf(buffer, sizeof(buffer), "POWER ---");
    }
    drawSmoothString(540, 27, buffer, COLOR_TEXT, FONT_SMALL);
  }
  if (displayState.showPowerBattery) {
    drawBatteryIcon(690, 8, displayState.batteryPercent, displayState.powerValid);
    if (displayState.powerValid) {
      snprintf(buffer, sizeof(buffer), "%u%%", (unsigned)displayState.batteryPercent);
    } else {
      snprintf(buffer, sizeof(buffer), "---");
    }
    drawSmoothString(747, 27, buffer, COLOR_TEXT, FONT_SMALL);
  }
}

static void drawOperationalOverlays();

static void drawOperationalFrame()
{
  fillRect(0, 0, LCD_WIDTH, LCD_HEIGHT, COLOR_SCREEN_BG);
  drawRadar();
  drawStatusBlocks();
  drawTopBar();
  drawTargetTable();
  drawOperationalOverlays();
}

static void drawOperationalOverlays()
{

  if (displayState.trackerMessageVisible && displayState.trackerMessage[0] != '\0') {
    fillRect(20, 62, 760, 116, COLOR_SCREEN_BG);
    drawRectOutline(20, 62, 760, 116, COLOR_WARN);
    const SmoothFont &messageFont =
      smoothTextWidth(displayState.trackerMessage, FONT_SMALL) <= 730 ?
      FONT_SMALL : FONT_TINY;
    drawSmoothStringCentered(400, 126, displayState.trackerMessage,
                             COLOR_WARN, messageFont);
  }

  if (displayState.gnssStatusVisible && displayState.baseConnected &&
      !displayState.trackerMessageVisible) {
    fillRect(165, 190, 470, 95, COLOR_SCREEN_BG);
    drawRectOutline(165, 190, 470, 95, COLOR_WARN);
    drawSmoothStringCentered(400, 228, "ОПРЕДЕЛЕНИЕ МЕСТОПОЛОЖЕНИЯ",
                             COLOR_WARN, FONT_SMALL);
    drawSmoothStringCentered(400, 260, "ОЖИДАЙТЕ", COLOR_WARN, FONT_SMALL);
  }

  if (!displayState.baseConnected) {
    drawSmoothStringCentered(400, 245, "НЕТ СВЯЗИ С БАЗОЙ", COLOR_RED, FONT_LARGE);
  }

  if (displayState.sosActive) {
    fillRect(300, 275, 200, 78, COLOR_SCREEN_BG);
    drawRectOutline(300, 275, 200, 78, COLOR_WARN);
    drawSmoothStringCentered(400, 329, "SOS", COLOR_WARN, FONT_MEDIUM);
  }
}

static bool IRAM_ATTR onRgbVsync(esp_lcd_panel_handle_t panelHandle,
                                const esp_lcd_rgb_panel_event_data_t *eventData,
                                void *userContext)
{
  (void)panelHandle;
  (void)eventData;
  (void)userContext;
  BaseType_t taskWoken = pdFALSE;
  if (vsyncSemaphore != NULL) {
    xSemaphoreGiveFromISR(vsyncSemaphore, &taskWoken);
  }
  return taskWoken == pdTRUE;
}

static void waitForVsync()
{
  if (vsyncSemaphore == NULL) {
    return;
  }
  xSemaphoreTake(vsyncSemaphore, 0);
  xSemaphoreTake(vsyncSemaphore, pdMS_TO_TICKS(100));
}

static void flushBand(int y, int height)
{
  if (!ready || panel == NULL || frameBuffer == NULL || height <= 0) {
    return;
  }
  if (y < 0) {
    height += y;
    y = 0;
  }
  if (y + height > LCD_HEIGHT) {
    height = LCD_HEIGHT - y;
  }
  if (height <= 0) {
    return;
  }

  waitForVsync();
  esp_lcd_panel_draw_bitmap(panel, 0, y, LCD_WIDTH, y + height,
                            frameBuffer + y * LCD_WIDTH);
}

static void flushFrame()
{
  flushBand(0, LCD_HEIGHT);
}

static const uint8_t *glyphFor(char c)
{
  static const uint8_t space[7] = {0, 0, 0, 0, 0, 0, 0};
  static const uint8_t dash[7] = {0, 0, 0, 0x1F, 0, 0, 0};
  static const uint8_t colon[7] = {0, 0x04, 0x04, 0, 0x04, 0x04, 0};
  static const uint8_t dot[7] = {0, 0, 0, 0, 0, 0x0C, 0x0C};
  static const uint8_t underscore[7] = {0, 0, 0, 0, 0, 0, 0x1F};
  static const uint8_t leftParen[7] = {0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02};
  static const uint8_t rightParen[7] = {0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08};
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
    case '_': return underscore;
    case '(': return leftParen;
    case ')': return rightParen;
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

static int textWidth(const String &text, int scale)
{
  if (text.length() == 0) {
    return 0;
  }
  return (int)text.length() * 6 * scale - scale;
}

static void drawTextCentered(int y, const String &text, uint16_t color, int scale)
{
  drawText((LCD_WIDTH - textWidth(text, scale)) / 2, y, text, color, scale);
}

enum CyrillicGlyph {
  CYR_A,
  CYR_V,
  CYR_D,
  CYR_E,
  CYR_ZH,
  CYR_I,
  CYR_SHORT_I,
  CYR_K,
  CYR_L,
  CYR_O,
  CYR_S,
  CYR_T,
  CYR_CH,
  CYR_YU,
  CYR_YA
};

static const uint8_t CYRILLIC_GLYPHS[][7] = {
  {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, // A
  {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}, // V
  {0x0E, 0x0A, 0x0A, 0x12, 0x12, 0x1F, 0x11}, // D
  {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}, // E
  {0x15, 0x15, 0x0E, 0x04, 0x0E, 0x15, 0x15}, // ZH
  {0x11, 0x11, 0x13, 0x15, 0x19, 0x11, 0x11}, // I
  {0x0A, 0x04, 0x11, 0x13, 0x15, 0x19, 0x11}, // SHORT I
  {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}, // K
  {0x03, 0x05, 0x09, 0x11, 0x11, 0x11, 0x11}, // L
  {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, // O
  {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}, // S
  {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}, // T
  {0x11, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x01}, // CH
  {0x12, 0x15, 0x15, 0x1D, 0x15, 0x15, 0x12}, // YU
  {0x0F, 0x11, 0x11, 0x0F, 0x05, 0x09, 0x11}  // YA
};

static void drawCyrillicChar(int x, int y, CyrillicGlyph glyphIndex,
                             uint16_t color, int scale, bool italic)
{
  const uint8_t *glyph = CYRILLIC_GLYPHS[glyphIndex];
  for (int row = 0; row < 7; ++row) {
    const int italicOffset = italic ? (6 - row) * scale / 3 : 0;
    for (int col = 0; col < 5; ++col) {
      if (glyph[row] & (1 << (4 - col))) {
        fillRect(x + italicOffset + col * scale, y + row * scale,
                 scale, scale, color);
      }
    }
  }
}

static void drawCyrillicWordCentered(int y, const CyrillicGlyph *word,
                                     size_t length, uint16_t color, int scale)
{
  const int glyphStep = 7 * scale;
  const int width = (int)length * glyphStep - 2 * scale;
  int x = (LCD_WIDTH - width) / 2;
  for (size_t i = 0; i < length; ++i) {
    drawCyrillicChar(x, y, word[i], color, scale, true);
    x += glyphStep;
  }
}

static void drawShell()
{
  drawStartupBackground();
  drawSmoothStartupText();
  if (startupVersion.length() > 0) {
    const int width = smoothTextWidth(startupVersion.c_str(), FONT_TINY);
    drawSmoothString(778 - width, 468, startupVersion.c_str(),
                     COLOR_TEXT, FONT_TINY);
  }
  shellDrawn = true;
}

static void ensureShell()
{
  if (!shellDrawn) {
    drawShell();
  }
}

static bool topBarChanged(const GL050001C0_40_State &a,
                          const GL050001C0_40_State &b)
{
  return a.timeValid != b.timeValid || a.hour != b.hour || a.minute != b.minute ||
         a.powerValid != b.powerValid || a.batteryPercent != b.batteryPercent ||
         a.voltageV != b.voltageV || a.currentMa != b.currentMa ||
         a.showPowerVoltage != b.showPowerVoltage ||
         a.showPowerCurrent != b.showPowerCurrent ||
         a.showPowerBattery != b.showPowerBattery;
}

static bool targetTableChanged(const GL050001C0_40_State &a,
                               const GL050001C0_40_State &b)
{
  if (a.tableVisible != b.tableVisible ||
      a.trackerMessageVisible != b.trackerMessageVisible ||
      strcmp(a.trackerMessage, b.trackerMessage) != 0 ||
      a.tableTargetCount != b.tableTargetCount) {
    return true;
  }
  for (uint8_t index = 0; index < a.tableTargetCount; ++index) {
    const GL050001C0_40_Target &x = a.tableTargets[index];
    const GL050001C0_40_Target &y = b.tableTargets[index];
    if (x.address != y.address || strcmp(x.callsign, y.callsign) != 0 ||
        x.squawk != y.squawk || x.altitudeM != y.altitudeM ||
        x.speedKmh != y.speedKmh || x.courseDeg != y.courseDeg ||
        x.latitude != y.latitude || x.longitude != y.longitude ||
        x.signalRssi != y.signalRssi) {
      return true;
    }
  }
  return false;
}

static bool radarChanged(const GL050001C0_40_State &a,
                         const GL050001C0_40_State &b)
{
  if (a.baseConnected != b.baseConnected || a.sosActive != b.sosActive ||
      a.trackerMessageVisible != b.trackerMessageVisible ||
      a.gnssStatusVisible != b.gnssStatusVisible ||
      strcmp(a.trackerMessage, b.trackerMessage) != 0 ||
      a.courseDeg != b.courseDeg || a.radarRangeM != b.radarRangeM ||
      a.targetCount != b.targetCount) {
    return true;
  }
  for (uint8_t index = 0; index < a.targetCount; ++index) {
    const GL050001C0_40_Target &x = a.targets[index];
    const GL050001C0_40_Target &y = b.targets[index];
    if (x.distanceM != y.distanceM || x.bearingDeg != y.bearingDeg ||
        x.courseDeg != y.courseDeg ||
        x.relativeAltitudeM != y.relativeAltitudeM ||
        x.verticalRate != y.verticalRate ||
        x.alarmLevel != y.alarmLevel) {
      return true;
    }
  }
  return false;
}

static bool statusBlocksChanged(const GL050001C0_40_State &a,
                                const GL050001C0_40_State &b)
{
  return a.gnssValid != b.gnssValid || a.satellites != b.satellites ||
         a.latitude != b.latitude || a.longitude != b.longitude ||
         a.showLoraStatus != b.showLoraStatus ||
         a.showGpsStatus != b.showGpsStatus ||
         a.showLanStatus != b.showLanStatus ||
         a.lanReady != b.lanReady || a.lanLinkUp != b.lanLinkUp ||
         a.lanUdpWorking != b.lanUdpWorking ||
         memcmp(a.lanIp, b.lanIp, sizeof(a.lanIp)) != 0 ||
         a.lanUdpPort != b.lanUdpPort ||
         a.lanTxPackets != b.lanTxPackets || a.lanRxPackets != b.lanRxPackets ||
         a.loraTxPackets != b.loraTxPackets ||
         a.loraRxPackets != b.loraRxPackets ||
         a.loraRfHz != b.loraRfHz || a.loraRssiDb != b.loraRssiDb;
}

bool GL050001C0_40_setup()
{
#if !GL050001C0_40_PINMAP_CONFIRMED
  Serial.println(F("[GL050001C0-40] RGB panel is disabled: not enough free GPIOs in the current FlyRF pinout."));
  Serial.println(F("[GL050001C0-40] Move peripherals first, then set GL050001C0_40_PINMAP_CONFIRMED=1 and fill the pin map."));
  return false;
#else
  esp_lcd_rgb_panel_config_t config = {};
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  config.clk_src = LCD_CLK_SRC_PLL160M;
#endif
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
  // Снижение PCLK уменьшает нагрузку RGB DMA на PSRAM при работе Wi-Fi.
  config.timings.pclk_hz = 24000000;
  config.timings.h_res = LCD_WIDTH;
  config.timings.v_res = LCD_HEIGHT;
  config.timings.hsync_pulse_width = 64;
  config.timings.hsync_back_porch = 80;
  config.timings.hsync_front_porch = 42;
  config.timings.vsync_pulse_width = 2;
  config.timings.vsync_back_porch = 74;
  config.timings.vsync_front_porch = 1;
  config.timings.flags.pclk_active_neg = false;
  config.flags.fb_in_psram = true;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  config.num_fbs = 1;
  // Начинаем со стабильного режима теста №7 (60 строк). Если конкретная
  // сборка ESP32 Arduino оставила меньше непрерывной DMA-SRAM, драйвер
  // автоматически пробует 40 и 20 строк вместо полного отказа дисплея.
  static const uint16_t bounceLineCandidates[] = {60, 40, 20};
#endif

  esp_err_t err = ESP_OK;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  for (size_t attempt = 0;
       attempt < sizeof(bounceLineCandidates) / sizeof(bounceLineCandidates[0]);
       ++attempt) {
    const uint16_t bounceLines = bounceLineCandidates[attempt];
    config.bounce_buffer_size_px = LCD_WIDTH * bounceLines;
    Serial.print(F("[GL050001C0-40] DMA free/largest before panel="));
    Serial.print(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA));
    Serial.print('/');
    Serial.print(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA));
    Serial.print(F(", bounce lines="));
    Serial.println(bounceLines);
    panel = NULL;
    err = esp_lcd_new_rgb_panel(&config, &panel);
    if (err == ESP_OK) {
      Serial.print(F("[GL050001C0-40] panel allocated, bounce lines="));
      Serial.println(bounceLines);
      break;
    }
    Serial.print(F("[GL050001C0-40] allocation retry after error: "));
    Serial.println(err);
  }
#else
  err = esp_lcd_new_rgb_panel(&config, &panel);
#endif
  if (err != ESP_OK) {
    Serial.print(F("[GL050001C0-40] panel allocation failed: "));
    Serial.println(err);
    panel = NULL;
    return false;
  }

  vsyncSemaphore = xSemaphoreCreateBinary();
  if (vsyncSemaphore == NULL) {
    Serial.println(F("[GL050001C0-40] VSYNC semaphore allocation failed"));
    panel = NULL;
    return false;
  }
  esp_lcd_rgb_panel_event_callbacks_t callbacks = {};
  callbacks.on_vsync = onRgbVsync;
  err = esp_lcd_rgb_panel_register_event_callbacks(panel, &callbacks, NULL);
  if (err != ESP_OK) {
    Serial.print(F("[GL050001C0-40] VSYNC callback registration failed: "));
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

  frameBuffer = (uint16_t *)heap_caps_malloc(LCD_WIDTH * LCD_HEIGHT * sizeof(uint16_t),
                                            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (frameBuffer == NULL) {
    Serial.println(F("[GL050001C0-40] PSRAM framebuffer allocation failed"));
    panel = NULL;
    return false;
  }

  ready = true;
  drawShell();
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  Serial.println(F("[GL050001C0-40] RGB panel initialized: PCLK 24 MHz, VSYNC 2/74/1, bounce buffer 60 lines"));
#else
  Serial.println(F("[GL050001C0-40] RGB panel initialized: PCLK 24 MHz"));
#endif
  return true;
#endif
}

void GL050001C0_40_showStartup(const String &version)
{
  if (!ready) {
    return;
  }

  startupVersion = version;
  shellDrawn = false;
  drawShell();
  flushFrame();
  startupShownMs = millis();
  operationalFrameShown = false;
  renderedStateValid = false;
}

void GL050001C0_40_showStatus(uint32_t rxPackets, uint32_t txPackets)
{
  displayState.rs485RxPackets = rxPackets;
  displayState.rs485TxPackets = txPackets;
}

void GL050001C0_40_updateState(const GL050001C0_40_State &state)
{
  displayState = state;
  stateAvailable = true;
}

bool GL050001C0_40_needsFrameUpdate()
{
  return ready && stateAvailable;
}
void GL050001C0_40_showPowerOff()
{
  if (!ready) {
    return;
  }

  drawShell();
  flushFrame();
}

void GL050001C0_40_loop()
{
  const uint32_t nowMs = millis();
  if (!ready || !stateAvailable ||
      (uint32_t)(nowMs - startupShownMs) < 3000UL) {
    return;
  }

  if (!operationalFrameShown) {
    if (!Web_displayUpdateAllowed()) {
      return;
    }
    drawOperationalFrame();
    flushFrame();
    renderedState = displayState;
    renderedStateValid = true;
    operationalFrameShown = true;
    lastDisplayUpdateMs = nowMs;
    delay(1);
    return;
  }

  if (!renderedStateValid ||
      (uint32_t)(nowMs - lastDisplayUpdateMs) < DISPLAY_UPDATE_INTERVAL_MS ||
      !Web_displayUpdateAllowed()) {
    return;
  }

  const bool updateTop = topBarChanged(displayState, renderedState);
  const bool updateTable = targetTableChanged(displayState, renderedState);
  const bool updateRadar = radarChanged(displayState, renderedState);
  const bool updateStatus = statusBlocksChanged(displayState, renderedState);
  if (!updateTop && !updateTable && !updateRadar && !updateStatus) {
    return;
  }

  // При повороте поле полёта и шкала компаса затрагивают весь экран. Такой кадр
  // собирается целиком, иначе на верхней панели остаются риски старого курса.
  // Все изменения без поворота по-прежнему передаются отдельными полосами.
  if (updateRadar) {
    drawOperationalFrame();
    flushFrame();
    renderedState = displayState;
    renderedStateValid = true;
    lastDisplayUpdateMs = nowMs;
    delay(1);
    return;
  }

  if (updateTop) {
    // Центральная часть верхней строки содержит верхнюю дугу радара.
    // Очищаем только блок времени слева и блок питания справа.
    fillRect(0, 0, 190, 38, COLOR_SCREEN_BG);
    fillRect(520, 0, LCD_WIDTH - 520, 38, COLOR_SCREEN_BG);
    drawTopBar();
  }
  if (updateTable) {
    fillRect(0, 38, LCD_WIDTH, 88, COLOR_SCREEN_BG);
    drawTargetTable();
  }
  if (updateStatus) {
    // Нижние блоки расположены поверх шкалы радара: сначала восстанавливаем
    // фон радара, затем заново выводим только телеметрию.
    fillRect(0, 390, LCD_WIDTH, LCD_HEIGHT - 390, COLOR_SCREEN_BG);
    drawRadar();
    drawStatusBlocks();
  }

  if (updateTop) {
    flushBand(0, 38);
  }
  if (updateTable) {
    flushBand(38, 88);
  }
  if (updateStatus) {
    flushBand(390, LCD_HEIGHT - 390);
  }

  renderedState = displayState;
  renderedStateValid = true;
  lastDisplayUpdateMs = nowMs;
  delay(1);
}
