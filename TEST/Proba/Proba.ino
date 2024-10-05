#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

void setup() {
    tft.init(); // инициализация дисплея
    tft.setRotation(2); // вращение на 180 градусов
    tft.fillScreen(TFT_BLACK); // заливка фона чёрным цветом


    tft.setCursor(60, 150); // x,y координаты текста
    tft.setTextColor(TFT_WHITE); // цвет текста - белый
    tft.setTextSize(2); // размер текста - №2 (7*2 = 14 точек)
    tft.print("Привет!"); // вывод текста
}

void loop() {
}
