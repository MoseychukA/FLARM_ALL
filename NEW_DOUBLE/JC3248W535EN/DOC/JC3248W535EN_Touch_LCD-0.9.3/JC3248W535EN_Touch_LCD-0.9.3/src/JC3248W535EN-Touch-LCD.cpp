
/**
 *  Made by AudunKodehode
 *
 *  See: https://github.com/AudunKodehode/JC3248W535EN-Touch-LCD
 */

#include "JC3248W535EN-Touch-LCD.h"
#include "qrcode_helper.h" // https://github.com/ricmoo/QRCode

// Pin definitions
#define GFX_BL 1
#define TOUCH_ADDR 0x3B
#define TOUCH_SDA 4
#define TOUCH_SCL 8
#define TOUCH_I2C_CLOCK 400000
#define TOUCH_RST_PIN 12
#define TOUCH_INT_PIN 11
#define AXS_MAX_TOUCH_NUMBER 1

JC3248W535EN::JC3248W535EN() {
    bus = new Arduino_ESP32QSPI(45, 47, 21, 48, 40, 39);
    g = new Arduino_AXS15231B(bus, GFX_NOT_DEFINED, 0, false, 320, 480);
    gfx = new Arduino_Canvas(320, 480, g, 0, 0, 0);
    currentR = currentG = currentB = 255;
    currentColor = 0xFFFF;
}

bool JC3248W535EN::begin() {
    // Initialize display
    if (!gfx->begin()) return false;
    
    // Initialize backlight
    if (GFX_BL != GFX_NOT_DEFINED) {
        pinMode(GFX_BL, OUTPUT);
        digitalWrite(GFX_BL, HIGH);
    }
    
    // Initialize touch
    Wire.begin(TOUCH_SDA, TOUCH_SCL);
    Wire.setClock(TOUCH_I2C_CLOCK);
    
    // Configure touch pins
    pinMode(TOUCH_INT_PIN, INPUT_PULLUP);
    pinMode(TOUCH_RST_PIN, OUTPUT);
    digitalWrite(TOUCH_RST_PIN, LOW);
    delay(200);
    digitalWrite(TOUCH_RST_PIN, HIGH);
    delay(200);
    
    gfx->fillScreen(0); // Clear screen
    return true;
}

uint16_t JC3248W535EN::mapX(uint16_t x, uint16_t y) {
    return 320 - y;
}

uint16_t JC3248W535EN::mapY(uint16_t x, uint16_t y) {
    return x;
}

uint16_t JC3248W535EN::rgb(uint8_t r, uint8_t g, uint8_t b) {
    return gfx->color565(r, g, b);
}

void JC3248W535EN::setColor(uint8_t r, uint8_t g, uint8_t b) {
    currentR = r;
    currentG = g;
    currentB = b;
    currentColor = rgb(r, g, b);
}

void JC3248W535EN::clear(uint8_t r, uint8_t g, uint8_t b) {
    gfx->fillScreen(rgb(r, g, b));
}
void JC3248W535EN::flush() {
    gfx->flush();
}

// Drawing functions implementation
void JC3248W535EN::drawFillRect(int16_t x, int16_t y, int16_t w, int16_t h) {
    int16_t px = 320 - (y + h);
    int16_t py = x;
    int16_t pw = h;
    int16_t ph = w;
    if (px < 0) px = 0;
    if (py < 0) py = 0;
    if (px + pw > 320) pw = 320 - px;
    if (py + ph > 480) ph = 480 - py;
    gfx->fillRect(px, py, pw, ph, currentColor);
}
// Drawing functions implementation
void JC3248W535EN::drawFillRect2(int16_t x, int16_t y, int16_t w, int16_t h) {
    int16_t px = 320 - (y + h);
    int16_t py = x;
    int16_t pw = h;
    int16_t ph = w;
    if (px < 0) px = 0;
    if (py < 0) py = 0;
    if (px + pw > 320) pw = 320 - px;
    if (py + ph > 480) ph = 480 - py;
    gfx->fillRect(px, py, pw, ph, currentColor);
}

void JC3248W535EN::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    // Convert coordinates to match the screen orientation
    int16_t px0 = 320 - y0;
    int16_t py0 = x0;
    int16_t px1 = 320 - y1;
    int16_t py1 = x1;
    
    // Draw the line with the current color
    gfx->drawLine(px0, py0, px1, py1, currentColor);
}

void JC3248W535EN::drawRect(int16_t x, int16_t y, int16_t w, int16_t h) {
    int16_t px = 320 - (y + h);
    int16_t py = x;
    int16_t pw = h;
    int16_t ph = w;
    if (px < 0) px = 0;
    if (py < 0) py = 0;
    if (px + pw > 320) pw = 320 - px;
    if (py + ph > 480) ph = 480 - py;
    gfx->drawRect(px, py, pw, ph, currentColor);
}

void JC3248W535EN::drawFillCircle(int16_t x, int16_t y, int16_t radius) {
    int16_t px = 320 - y;
    int16_t py = x;
    gfx->fillCircle(px, py, radius, currentColor);
}

void JC3248W535EN::drawCircleOutline(int16_t x, int16_t y, int16_t radius) {
    int16_t px = 320 - y;
    int16_t py = x;
    gfx->drawCircle(px, py, radius, currentColor);
}

void JC3248W535EN::drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
    // Convert coordinates to match screen orientation
    int16_t px0 = 320 - y0;
    int16_t py0 = x0;
    int16_t px1 = 320 - y1;
    int16_t py1 = x1;
    int16_t px2 = 320 - y2;
    int16_t py2 = x2;
    
    gfx->drawTriangle(px0, py0, px1, py1, px2, py2, currentColor);
}

void JC3248W535EN::drawFillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
    // Convert coordinates to match screen orientation
    int16_t px0 = 320 - y0;
    int16_t py0 = x0;
    int16_t px1 = 320 - y1;
    int16_t py1 = x1;
    int16_t px2 = 320 - y2;
    int16_t py2 = x2;
    
    gfx->fillTriangle(px0, py0, px1, py1, px2, py2, currentColor);
}

void JC3248W535EN::drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius) {
    int16_t px = 320 - (y + h);
    int16_t py = x;
    int16_t pw = h;
    int16_t ph = w;
    if (px < 0) px = 0;
    if (py < 0) py = 0;
    if (px + pw > 320) pw = 320 - px;
    if (py + ph > 480) ph = 480 - py;
    
    gfx->drawRoundRect(px, py, pw, ph, radius, currentColor);
}

void JC3248W535EN::drawFillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius) {
    int16_t px = 320 - (y + h);
    int16_t py = x;
    int16_t pw = h;
    int16_t ph = w;
    if (px < 0) px = 0;
    if (py < 0) py = 0;
    if (px + pw > 320) pw = 320 - px;
    if (py + ph > 480) ph = 480 - py;
    
    gfx->fillRoundRect(px, py, pw, ph, radius, currentColor);
}

void JC3248W535EN::drawEllipse(int16_t x, int16_t y, int16_t rx, int16_t ry) {
    int16_t px = 320 - y;
    int16_t py = x;
    
    // Note: ellipses might need swapping rx and ry due to orientation change
    gfx->drawEllipse(px, py, ry, rx, currentColor);
}

void JC3248W535EN::drawFillEllipse(int16_t x, int16_t y, int16_t rx, int16_t ry) {
    int16_t px = 320 - y;
    int16_t py = x;
    
    // Note: ellipses might need swapping rx and ry due to orientation change
    gfx->fillEllipse(px, py, ry, rx, currentColor);
}

void JC3248W535EN::prt(const String &text, int x, int y, uint8_t size) {
    uint8_t originalRotation = gfx->getRotation();
    gfx->setRotation(1);
    gfx->setTextColor(currentColor);
    gfx->setTextSize(size);
    gfx->setCursor(x, y);
    gfx->println(text);
    gfx->setRotation(originalRotation);
}

bool JC3248W535EN::getTouchPoint(uint16_t &x, uint16_t &y) {
    uint8_t data[AXS_MAX_TOUCH_NUMBER * 6 + 2] = {0};
    
    // Define the read command array properly
    const uint8_t read_cmd[11] = {
        0xb5, 0xab, 0xa5, 0x5a, 0x00, 0x00,
        (uint8_t)((AXS_MAX_TOUCH_NUMBER * 6 + 2) >> 8),
        (uint8_t)((AXS_MAX_TOUCH_NUMBER * 6 + 2) & 0xff),
        0x00, 0x00, 0x00
    };
    
    Wire.beginTransmission(TOUCH_ADDR);
    Wire.write(read_cmd, 11);
    if (Wire.endTransmission() != 0) return false;
    
    if (Wire.requestFrom(TOUCH_ADDR, sizeof(data)) != sizeof(data)) return false;
    
    for (int i = 0; i < sizeof(data); i++) {
        data[i] = Wire.read();
    }
    
    if (data[1] > 0 && data[1] <= AXS_MAX_TOUCH_NUMBER) {
        uint16_t rawX = ((data[2] & 0x0F) << 8) | data[3];
        uint16_t rawY = ((data[4] & 0x0F) << 8) | data[5];
        
        if (rawX == 273 && rawY == 273) return false;
        if (rawX > 4000 || rawY > 4000) return false;
        
        y = map(rawX, 0, 320, 320, 0);
        x = rawY;
        
        return true;
    }
    
    return false;
}

void JC3248W535EN::drawQRCode(const char* data, uint16_t x, uint16_t y, uint8_t moduleSize,
                             uint8_t bgColorR, uint8_t bgColorG, uint8_t bgColorB,
                             uint8_t fgColorR, uint8_t fgColorG, uint8_t fgColorB) {
  // Create QR code
  QRCode qrcode;
  
  // Adjust version based on data length to ensure it fits
  // A higher version can store more data but creates a larger QR code
  uint8_t version = 3;  // Default for ~65 alphanumeric characters
  
  // Increase version for longer data
  size_t dataLen = strlen(data);
  if (dataLen > 65 && dataLen <= 114) {
    version = 4;  // Up to ~114 alphanumeric characters
  } else if (dataLen > 114 && dataLen <= 154) {
    version = 5;  // Up to ~154 alphanumeric characters
  } else if (dataLen > 154 && dataLen <= 195) {
    version = 6;  // Up to ~195 alphanumeric characters
  } else if (dataLen > 195) {
    version = 8;  // Up to ~310 alphanumeric characters
    // If your data is longer than 310 characters, you may need to increase version further
  }
  
  // Calculate buffer size and allocate memory
  uint16_t bufferSize = qrcode_getBufferSize(version);
  uint8_t* qrcodeData = new uint8_t[bufferSize];
  
  if (qrcodeData == NULL) {
    // Memory allocation failed
    Serial.println("Failed to allocate memory for QR code");
    return;
  }
  
  // Initialize the QR code
  qrcode_initText(&qrcode, qrcodeData, version, 0, data);
  
  Serial.print("QR code version ");
  Serial.print(version);
  Serial.print(", size: ");
  Serial.print(qrcode.size);
  Serial.println("x" + String(qrcode.size));
  
  // Set background color and draw background
  setColor(bgColorR, bgColorG, bgColorB);
  drawFillRect(x, y, qrcode.size * moduleSize, qrcode.size * moduleSize);
  
  // Set QR code color
  setColor(fgColorR, fgColorG, fgColorB);
  
  // Draw each dark module (skip drawing light modules since they're the same as background)
  for (uint8_t qrY = 0; qrY < qrcode.size; qrY++) {
    for (uint8_t qrX = 0; qrX < qrcode.size; qrX++) {
      // Only draw the dark modules (more efficient)
      if (qrcode_getModule(&qrcode, qrX, qrY)) {
        drawFillRect2(
          x + qrX * moduleSize,
          y + qrY * moduleSize,
          moduleSize,
          moduleSize
        );
      }
    }
  }
  
  // Add a flush at the end for efficiency
  flush();
  
  // Free allocated memory
  delete[] qrcodeData;
}
