/*
  There are four different methods of plotting anti-aliased fonts to the screen.

  This sketch uses method 1, using tft.print() and tft.println() calls.

  In some cases the sketch shows what can go wrong too, so read the comments!
  
  The font is rendered WITHOUT a background, but a background colour needs to be
  set so the anti-aliasing of the character is performed correctly. This is because
  characters are drawn one by one.
  
  This method is good for static text that does not change often because changing
  values may flicker. The text appears at the tft cursor coordinates.

  It is also possible to "print" text directly into a created sprite, for example using
  spr.println("Hello"); and then push the sprite to the screen. That method is not
  demonstrated in this sketch.
  
*/
//  The fonts used are in the sketch data folder, press Ctrl+K to view.

//  Upload the fonts and icons to SPIFFS (must set at least 1M for SPIFFS) using the
//  "Tools"  "ESP8266 (or ESP32) Sketch Data Upload" menu option in the IDE.
//  To add this option follow instructions here for the ESP8266:
//  https://github.com/esp8266/arduino-esp8266fs-plugin
//  or for the ESP32:
//  https://github.com/me-no-dev/arduino-esp32fs-plugin

//  Close the IDE and open again to see the new menu option.

//  A processing sketch to create new fonts can be found in the Tools folder of TFT_eSPI
//  https://github.com/Bodmer/TFT_eSPI/tree/master/Tools/Create_Smooth_Font/Create_font

//  This sketch uses font files created from the Noto family of fonts:
//  https://www.google.com/get/noto/

#define FONT_10 "Arial10"
#define FONT_11 "Arial11"
#define FONT_12 "Arial12"
#define FONT_14 "Arial14"
#define FONT_16 "Arial16"
#define FONT_18 "Arial18"
#define FONT_20 "Arial20"
#define FONT_22 "Arial22"
#define FONT_25 "Arial25"
#define FONT_30 "Arial30"

#include <FS.h>// Файлы шрифтов хранятся в SPIFFS, поэтому загрузите библиотеку
#include <SPI.h>
#include <TFT_eSPI.h>       // Библиотека для конкретного оборудования

TFT_eSPI tft = TFT_eSPI();


void setup(void) {

  Serial.begin(250000);
  tft.begin();
  tft.setRotation(1);

//******  проверка загрузки шрифтов
  if (!SPIFFS.begin()) {
    Serial.println("Ошибка инициализации SPIFFS!");
    while (1) yield(); // Оставайся здесь, бездельничая, ожидая
  }  
}


void loop() {

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK); // Set the font colour AND the background colour
                                          // so the anti-aliasing works

  tft.setCursor(0, 0); // Set cursor at top left of screen


  tft.loadFont(FONT_10); // загружаем шрифт
  tft.print(FONT_10); // Выводим название шрифта
  tft.println(" - Потрогай Еще Эти Французские Булочки"); // Натпись про БУЛКИ
  tft.unloadFont(); // выгрузить шрифт 
  tft.println(); // New line
  
 // tft.setCursor(0, 22); // Set cursor at top left of screen
  tft.loadFont(FONT_11); // Загружаем шрифт 
  tft.print(FONT_11); // Выводим название шрифта
  tft.println(" - Потрогай Еще Эти Французские Булочки"); // Натпись про БУЛКИ
  tft.unloadFont(); // выгрузить шрифт
  tft.println(); // New line


  tft.loadFont(FONT_12); // загружаем шрифт
  tft.print(FONT_12); // Выводим название шрифта
  tft.println(" - Потрогай Еще Эти Французские Булочки"); // Натпись про БУЛКИ
  tft.unloadFont(); // выгрузить шрифт 
  tft.println(); // New line
  

  tft.loadFont(FONT_14); // Загружаем шрифт 
  tft.print(FONT_14); // Выводим название шрифта
  tft.println(" - Потрогай Еще Эти Французские Булочки"); // Натпись про БУЛКИ
  tft.unloadFont(); // выгрузить шрифт
  tft.println(); // New line


  tft.loadFont(FONT_16); // Загружаем шрифт 
  tft.print(FONT_16); // Выводим название шрифта
 // tft.println(" - Потрогай Еще Эти Французские Бу"); // Натпись про БУЛКИ
  tft.println(" - Потрогай Еще Эти Французские Бу"); // Натпись про БУЛКИ
  tft.unloadFont(); // выгрузить шрифт
  tft.println(); // New line


  tft.loadFont(FONT_18); // Загружаем шрифт 
  tft.print(FONT_18); // Выводим название шрифта
  tft.println(" - Потрогай Еще Эти Французск"); // Натпись про БУЛКИ
  tft.unloadFont(); // выгрузить шрифт
  tft.println(); // New line


  tft.loadFont(FONT_20); // Загружаем шрифт 
  tft.print(FONT_20); // Выводим название шрифта
  tft.println(" - Потрогай Еще Эти Францу"); // Натпись про БУЛКИ
  tft.unloadFont(); // выгрузить шрифт
  tft.println(); // New line


  tft.loadFont(FONT_22); // Загружаем шрифт 
  tft.print(FONT_22); // Выводим название шрифта
  tft.println(" - Потрогай Еще Эти Фра"); // Натпись про БУЛКИ
  tft.unloadFont(); // выгрузить шрифт
  tft.println(); // New line


  tft.loadFont(FONT_25); // Загружаем шрифт 
  tft.print(FONT_25); // Выводим название шрифта
  tft.println(" - Потрогай Еще Эти"); // Натпись про БУЛКИ
  tft.unloadFont(); // выгрузить шрифт
  tft.println(); // New line
  
  
  tft.loadFont(FONT_30); // Загружаем шрифт 
  tft.print(FONT_30); // Выводим название шрифта
  tft.println(" - Потрогай Еще Эти"); // Натпись про БУЛКИ
  tft.unloadFont(); // выгрузить шрифт
  tft.println(); // New line
  delay(200000);
}
