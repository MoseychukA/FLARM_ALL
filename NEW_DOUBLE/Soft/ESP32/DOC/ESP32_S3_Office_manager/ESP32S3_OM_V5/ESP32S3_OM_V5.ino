/* 
пример работы со шрифтами от тети Ады
положить файлик glcdfont.c в папку 
...\Documents\Arduino\libraries\TFT_eSPI\Fonts
в файле TFT_eSPI.cpp закоментировать строку if (c > 255) return;
*/

#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

void setup() {

  tft.init();                 // инициализация дисплея
  tft.setRotation(1);         // вращение на 180 градусов
  tft.fillScreen(TFT_BLACK);  // заливка фона чёрным цветом
  tft.setCursor(0, 0);        // x,y координаты текста

  for (int i = 1; i <= 5; i++) {
    tft.setTextColor(TFT_WHITE);  // цвет текста - белый
    tft.setTextSize(i);           // размер увеличиваем с каждой итерацией  
    tft.println("Съешь ещё этих мягких французских булок, да выпей же чаю.");
    tft.println("");
  }
}

void loop() {
}
