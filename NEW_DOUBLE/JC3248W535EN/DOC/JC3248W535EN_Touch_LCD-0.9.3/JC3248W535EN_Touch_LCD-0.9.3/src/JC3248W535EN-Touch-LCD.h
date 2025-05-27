/**
 *  Made by AudunKodehode
 *
 *  See: https://github.com/AudunKodehode/JC3248W535EN-Touch-LCD
 */

#ifndef JC3248W535EN_TOUCH_LCD_H
#define JC3248W535EN_TOUCH_LCD_H



#include <Arduino.h>
#include <Arduino_GFX_Library/Arduino_GFX_Library.h>
#include <Wire.h>
#include <Preferences.h>

class JC3248W535EN {
private:
    Arduino_DataBus* bus;
    Arduino_GFX* g;
    Arduino_Canvas* gfx;
    uint16_t currentColor;
    uint8_t currentR, currentG, currentB;
    
    // Helper functions
    uint16_t mapX(uint16_t x, uint16_t y);
    uint16_t mapY(uint16_t x, uint16_t y);
    uint16_t rgb(uint8_t r, uint8_t g, uint8_t b);

public:
    JC3248W535EN();
    bool begin();
    void setColor(uint8_t r, uint8_t g, uint8_t b);
    
    // Drawing functions
    void clear(uint8_t r = 0, uint8_t g = 0, uint8_t b = 0);
    void drawFillRect(int16_t x, int16_t y, int16_t w, int16_t h);
    void drawFillRect2(int16_t x, int16_t y, int16_t w, int16_t h);
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h);
    void drawFillCircle(int16_t x, int16_t y, int16_t radius);
    void drawCircleOutline(int16_t x, int16_t y, int16_t radius);
    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1);
    void drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2);
    void drawFillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2);
    void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius);
    void drawFillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius);
    void drawEllipse(int16_t x, int16_t y, int16_t rx, int16_t ry);
    void drawFillEllipse(int16_t x, int16_t y, int16_t rx, int16_t ry);
    void prt(const String &text, int x, int y, uint8_t size);
    void flush();
    
    // Touch functions
    bool getTouchPoint(uint16_t &x, uint16_t &y);

    /**
     * Draw a QR code directly on the screen.
     * 
     * @param data The data to encode in the QR code (URL or text)
     * @param x The x-coordinate of the top-left corner to start drawing
     * @param y The y-coordinate of the top-left corner to start drawing
     * @param moduleSize Size of each QR module in pixels
     * @param bgColorR Background color - Red component (0-255)
     * @param bgColorG Background color - Green component (0-255)
     * @param bgColorB Background color - Blue component (0-255)
     * @param fgColorR QR code color - Red component (0-255)
     * @param fgColorG QR code color - Green component (0-255)
     * @param fgColorB QR code color - Blue component (0-255)
     */
    void drawQRCode(const char* data, uint16_t x, uint16_t y, uint8_t moduleSize = 4,
                    uint8_t bgColorR = 255, uint8_t bgColorG = 255, uint8_t bgColorB = 255,
                    uint8_t fgColorR = 0, uint8_t fgColorG = 0, uint8_t fgColorB = 0);
};

#endif
