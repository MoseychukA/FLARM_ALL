/**
 *  Made by AudunKodehode
 *
 *  See: https://github.com/AudunKodehode/JC3248W535EN-Touch-LCD
 */

#ifndef JC3248W535EN_TOUCH_LCD_H
#define JC3248W535EN_TOUCH_LCD_H

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <Wire.h>

class JC3248W535EN {
public:
    JC3248W535EN();
    bool begin();
    
    // Color functions
    uint16_t rgb(uint8_t r, uint8_t g, uint8_t b);
    void setColor(uint8_t r, uint8_t g, uint8_t b);
    void clear(uint8_t r = 0, uint8_t g = 0, uint8_t b = 0);
    void flush();
    
    // Font functions
    void setFont(const GFXfont* font);
    void prt(const String &text, int x, int y, uint8_t size = 1);
    
    // Drawing functions
    void drawFillRect(int16_t x, int16_t y, int16_t w, int16_t h);
    void drawFillRect2(int16_t x, int16_t y, int16_t w, int16_t h);
    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1);
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h);
    void drawFillCircle(int16_t x, int16_t y, int16_t radius);
    void drawCircleOutline(int16_t x, int16_t y, int16_t radius);
    void drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2);
    void drawFillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2);
    void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius);
    void drawFillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius);
    void drawEllipse(int16_t x, int16_t y, int16_t rx, int16_t ry);
    void drawFillEllipse(int16_t x, int16_t y, int16_t rx, int16_t ry);
    
    // Image functions
    void image(const uint16_t* bitmap, int16_t x, int16_t y, int16_t w, int16_t h);
    void fetchJpeg(const char* url, int16_t x, int16_t y);
    uint16_t getPixel(int16_t x, int16_t y); // New function to read a pixel color
    bool loadImageFromUrl(const char* url, int16_t x, int16_t y); // Integrated image loading method
    
    // QR code function
    void drawQRCode(const char* data, uint16_t x, uint16_t y, uint8_t moduleSize = 3,
                   uint8_t bgColorR = 255, uint8_t bgColorG = 255, uint8_t bgColorB = 255,
                   uint8_t fgColorR = 0, uint8_t fgColorG = 0, uint8_t fgColorB = 0);
    
    // Mapping functions
    uint16_t mapX(uint16_t x, uint16_t y);
    uint16_t mapY(uint16_t x, uint16_t y);
    
    // Touch function
    bool getTouchPoint(uint16_t &x, uint16_t &y);
    
    // Make gfx accessible for direct pixel manipulation
    Arduino_Canvas* gfx;
    
private:
    Arduino_ESP32QSPI* bus;
    Arduino_AXS15231B* g;
    
    uint8_t currentR, currentG, currentB;
    uint16_t currentColor;
};

#endif // JC3248W535EN_TOUCH_LCD_H
