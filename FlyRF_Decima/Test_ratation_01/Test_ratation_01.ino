 
/***************************************************

Тест функций вращения

Эта библиотека предназначена для работы с TFT-дисплеями (с SPI-интерфейсом) от Adafruit. Библиотека поддерживает следующие устройства:

* 1,8-дюймовую TFT-плату с SD-картой:
  https://www.adafruit.com/products/358
* 1,8-дюймовый TFT-модуль:
  https://www.adafruit.com/product/802
* 1,44-дюймовую TFT-плату:
  https://www.adafruit.com/product/2088
* «Голый» 1,8-дюймовый TFT-дисплей (без плат и модулей):
  https://www.adafruit.com/products/618  

Руководства и схемы подключения ищите по ссылкам выше. Этим дисплеям 
для коммуникации требуется SPI-интерфейс с 4 или 5 контактами (контакт 
RST опционален).

Adafruit инвестировала время и ресурсы, создавая эту библиотеку с 
открытым кодом. Пожалуйста, поддержите Adafruit и оборудование с 
открытым кодом, покупая продукты Adafruit!

Библиотека написана Лимор Фрид (Limor Fried, Ladyada) для Adafruit 
Industries. Весь текст выше должен быть включен при любом повторном 
распространении.

****************************************************/

#include <Adafruit_GFX.h>    // подключаем графическую библиотеку
#include <Adafruit_ST7735.h> // подключаем библиотеку для управления дисплеем
#include <SPI.h>

#define TFT_CS    21
#define TFT_RST    4  // этот контакт можно подключить к RESET-
#define TFT_DC     2
#define TFT_SCLK  18   // здесь можно задать любой контакт
#define TFT_MOSI  23   // здесь можно задать любой контакт
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

#define TFT_LCD   5   // здесь можно задать любой контакт


void setup(void) { 
  Serial.begin(9600);
  Serial.print("Hello! Adafruit ST7735 rotation test");  //  "Привет! Это тест вращающих функций для TFT-дисплея ST7735"
 pinMode(TFT_LCD, OUTPUT);
 digitalWrite(TFT_LCD, HIGH);
  // используйте этот инициализатор, если работаете 
  // с 1,8-дюймовым TFT-дисплеем:
  tft.initR(INITR_BLACKTAB);   // инициализируем чипa ST7735S, черный ярлычок

  // используйте этот инициализатор, если работаете 
  // с 1,44-дюймовым TFT-дисплеем (нужно раскомментировать):
  //tft.initR(INITR_144GREENTAB);  // инициализируем чипa ST7735S, зеленый ярлычок

  Serial.println("init");

  tft.setTextWrap(false); // позволяем тексту уйти за правый край дисплея
  tft.fillScreen(ST7735_BLACK);

  Serial.println("This is a test of the rotation capabilities of the TFT library!");  //  "Это скетч, проверяющий функции вращения на TFT-дисплее ST7735"
  Serial.println("Press <SEND> (or type a character) to advance");   //  "Чтобы продолжить, нажмите <SEND> (или впишите символ)"
}

void loop(void) {
  rotateLine();
  rotateText();
  rotatePixel();
  rotateFastline();
  rotateDrawrect();
  rotateFillrect();
  rotateDrawcircle();
  rotateFillcircle();
  rotateTriangle();
  rotateFillTriangle();
  rotateRoundRect();
  rotateFillRoundRect();
  rotateChar();
  rotateString();
}

void rotateText() {
  for (uint8_t i=0; i<4; i++) {
    tft.fillScreen(ST7735_BLACK);
    Serial.println(tft.getRotation(), DEC);

    tft.setCursor(0, 30);
    tft.setTextColor(ST7735_RED);
    tft.setTextSize(1);
    tft.println("Hello World!");   //  "Привет, мир!"
    tft.setTextColor(ST7735_YELLOW);
    tft.setTextSize(2);
    tft.println("Hello World!");  //  "Привет, мир!"
    tft.setTextColor(ST7735_GREEN);
    tft.setTextSize(3);
    tft.println("Hello World!");  //  "Привет, мир!"
    tft.setTextColor(ST7735_BLUE);
    tft.setTextSize(4);
    tft.print(1234.567);
    while (!Serial.available());
    Serial.read();  Serial.read();  Serial.read();
  
    tft.setRotation(tft.getRotation()+1);
  }
}

void rotateFillcircle(void) {
  for (uint8_t i=0; i<4; i++) {
    tft.fillScreen(ST7735_BLACK);
    Serial.println(tft.getRotation(), DEC);

    tft.fillCircle(10, 30, 10, ST7735_YELLOW);

    while (!Serial.available());
    Serial.read();  Serial.read();  Serial.read();

    tft.setRotation(tft.getRotation()+1);
  }
}

void rotateDrawcircle(void) {
  for (uint8_t i=0; i<4; i++) {
    tft.fillScreen(ST7735_BLACK);
    Serial.println(tft.getRotation(), DEC);

    tft.drawCircle(10, 30, 10, ST7735_YELLOW);
 
    while (!Serial.available());
    Serial.read();  Serial.read();  Serial.read();
  
    tft.setRotation(tft.getRotation()+1);
  }
}

void rotateFillrect(void) {
  for (uint8_t i=0; i<4; i++) {
    tft.fillScreen(ST7735_BLACK);
    Serial.println(tft.getRotation(), DEC);

    tft.fillRect(10, 20, 10, 20, ST7735_GREEN);
 
    while (!Serial.available());
    Serial.read();  Serial.read();  Serial.read();

    tft.setRotation(tft.getRotation()+1);
  }
}

void rotateDrawrect(void) {
  for (uint8_t i=0; i<4; i++) {
    tft.fillScreen(ST7735_BLACK);
    Serial.println(tft.getRotation(), DEC);

    tft.drawRect(10, 20, 10, 20, ST7735_GREEN);
 
    while (!Serial.available());
    Serial.read();  Serial.read();  Serial.read();

    tft.setRotation(tft.getRotation()+1);
  }
}

void rotateFastline(void) {
  for (uint8_t i=0; i<4; i++) {
    tft.fillScreen(ST7735_BLACK);
    Serial.println(tft.getRotation(), DEC);

    tft.drawFastHLine(0, 20, tft.width(), ST7735_RED);
    tft.drawFastVLine(20, 0, tft.height(), ST7735_BLUE);

    while (!Serial.available());
    Serial.read();  Serial.read();  Serial.read();

    tft.setRotation(tft.getRotation()+1);
  }
}

void rotateLine(void) {
  for (uint8_t i=0; i<4; i++) {
    tft.fillScreen(ST7735_BLACK);
    Serial.println(tft.getRotation(), DEC);

    tft.drawLine(tft.width()/2, tft.height()/2, 0, 0, ST7735_RED);
    while (!Serial.available());
    Serial.read();  Serial.read();  Serial.read();

    tft.setRotation(tft.getRotation()+1);
  }
}

void rotatePixel(void) {
  for (uint8_t i=0; i<4; i++) {
    tft.fillScreen(ST7735_BLACK);
    Serial.println(tft.getRotation(), DEC);

    tft.drawPixel(10,20, ST7735_WHITE);
    while (!Serial.available());
    Serial.read();  Serial.read();  Serial.read();

    tft.setRotation(tft.getRotation()+1);
  }
}

void rotateTriangle(void) {
  for (uint8_t i=0; i<4; i++) {
    tft.fillScreen(ST7735_BLACK);
    Serial.println(tft.getRotation(), DEC);

    tft.drawTriangle(20, 10, 10, 30, 30, 30, ST7735_GREEN);
    while (!Serial.available());
    Serial.read();  Serial.read();  Serial.read();

    tft.setRotation(tft.getRotation()+1);
  }
}

void rotateFillTriangle(void) {
  for (uint8_t i=0; i<4; i++) {
    tft.fillScreen(ST7735_BLACK);
    Serial.println(tft.getRotation(), DEC);

    tft.fillTriangle(20, 10, 10, 30, 30, 30, ST7735_RED);
    while (!Serial.available());
    Serial.read();  Serial.read();  Serial.read();

    tft.setRotation(tft.getRotation()+1);
  }
}

void rotateRoundRect(void) {
  for (uint8_t i=0; i<4; i++) {
    tft.fillScreen(ST7735_BLACK);
    Serial.println(tft.getRotation(), DEC);

    tft.drawRoundRect(20, 10, 25, 15, 5, ST7735_BLUE);
    while (!Serial.available());
    Serial.read();  Serial.read();  Serial.read();

    tft.setRotation(tft.getRotation()+1);
  }
}

void rotateFillRoundRect(void) {
  for (uint8_t i=0; i<4; i++) {
    tft.fillScreen(ST7735_BLACK);
    Serial.println(tft.getRotation(), DEC);

    tft.fillRoundRect(20, 10, 25, 15, 5, ST7735_CYAN);
    while (!Serial.available());
    Serial.read();  Serial.read();  Serial.read();

    tft.setRotation(tft.getRotation()+1);
  }
}

void rotateChar(void) {
  for (uint8_t i=0; i<4; i++) {
    tft.fillScreen(ST7735_BLACK);
    Serial.println(tft.getRotation(), DEC);

    tft.drawChar(25, 15, 'A', ST7735_WHITE, ST7735_WHITE, 1);
    while (!Serial.available());
    Serial.read();  Serial.read();  Serial.read();

    tft.setRotation(tft.getRotation()+1);
  }
}

void rotateString(void) {
  for (uint8_t i=0; i<4; i++) {
    tft.fillScreen(ST7735_BLACK);
    Serial.println(tft.getRotation(), DEC);

    tft.setCursor(8, 25);
    tft.setTextSize(1);
    tft.setTextColor(ST7735_WHITE);
    tft.print("Adafruit Industries");
    while (!Serial.available());
    Serial.read();  Serial.read();  Serial.read();

    tft.setRotation(tft.getRotation()+1);
  }
}
