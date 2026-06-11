
#ifndef OLEDHELPER_H
#define OLEDHELPER_H

#include <U8x8lib.h>

#define SSD1306_OLED_I2C_ADDR   0x3C
#define SH1106_OLED_I2C_ADDR    0x3C /* 0x3D */

#define isTimeToOLED()          (millis() - OLEDTimeMarker > 500)

byte OLED_setup(void);
void OLED_loop(void);
void OLED_fini(int);
void OLED_info1(void);
void OLED_info2(void);
void OLED_info3(int, char *, char *, char *);
void OLED_Next_Page(void);
void OLED_Up(void);

extern U8X8    *u8x8;
extern uint8_t OLED_flip;

//extern const char *ISO3166_CC[];
extern const char SoftRF_text1[];
extern const char SoftRF_text2[];
extern const char SoftRF_text3[];
extern const char ID_text[];
extern const char PROTOCOL_text[];
//extern const char RX_text[];
//extern const char TX_text[];

#endif /* OLEDHELPER_H */
