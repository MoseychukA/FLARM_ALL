// See SetupX_Template.h for all options available
#define USER_SETUP_ID 214

#define SSD1283a_DRIVER


#define TFT_WIDTH  130
#define TFT_HEIGHT 130


#define SSD1351_1DOT5_INCH_130 // For 130 x 130 display

// Wiring:
//   +-------------+------------+-------------------------------------------------------------------+
//   | Display PCB |  TFT_eSPI  |                               Info                                |
//   +-------------+------------+-------------------------------------------------------------------+
//   | GND         | GND (0V)   | Common                                                            |
//   | VCC         | 5V or 3.3V | Better to power with 5V if display PCB supports it                |
//   | DIN         | TFT_MOSI   | SPI data                                                          |
//   | SCK         | TFT_SCLK   | SPI clock                                                         |
//   | DC          | TFT_DC     | Distinguish between a command or its data                         |
//   | RST         | TFT_RST    | Hardware reset, can connect to MCU RST pin as well                |
//   | CS          | TFT_CS     | Chip select, Set to -1 if for manually use with multiple displays |
//   +-------------+------------+-------------------------------------------------------------------+
/*

*/

#if defined(ESP32)
  #define TFT_MOSI 27
  #define TFT_SCLK 5
  #define TFT_DC   15
  #define TFT_RST  4
  #define TFT_CS   2
#elif defined (ARDUINO_ARCH_ESP8266)
//#define TFT_MOSI PIN_D5 // Can't change
//#define TFT_SCLK PIN_D7 // Can't change
  #define TFT_DC   PIN_D3
  #define TFT_RST  PIN_D4
  #define TFT_CS   PIN_D8
#endif


#define LOAD_GLCD   // Original Adafruit 8 pixel font needs ~1820 bytes in FLASH
#define LOAD_FONT2  // Small 16 pixel high font, needs ~3534 bytes in FLASH, 96 characters
#define LOAD_FONT4  // Medium 26 pixel high font, needs ~5848 bytes in FLASH, 96 characters
#define LOAD_FONT6  // Large 48 pixel font, needs ~2666 bytes in FLASH, only characters 1234567890:-.apm
#define LOAD_FONT7  // 7 segment 48 pixel font, needs ~2438 bytes in FLASH, only characters 1234567890:.
#define LOAD_FONT8  // Large 75 pixel font needs ~3256 bytes in FLASH, only characters 1234567890:-.
//#define LOAD_FONT8N // Alternative to Font 8 above, slightly narrower, so 3 digits fit a 160 pixel TFT
#define LOAD_GFXFF  // FreeFonts- 48 Adafruit_GFX free fonts FF1 to FF48 and custom fonts
#define SMOOTH_FONT


#define SPI_FREQUENCY  20000000
//#define SPI_FREQUENCY  40000000 // Works after shielding the wires!
