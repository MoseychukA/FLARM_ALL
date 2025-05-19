#include <SPI.h>
#include <LoraSx1262.h>
#include <TFT_eSPI.h> // Hardware-specific library

TFT_eSPI tft = TFT_eSPI();       // Invoke custom library

byte* payload = (byte*)"Hello world!";
//byte* payload = (byte*)"$PSRFO, 20, e09de1035f859f3ea159a7d5238cc629f9d16ffa";
//byte* payload = (byte*)"e09de1035f859f3ea159a7d5238cc629f9d16ffa";

LoraSx1262 radio;


int counter = 0;
const int ledPin =  4;// the number of the LED pin

int ledState = LOW;             // ledState used to set the LED 

unsigned long previousMillis = 0;        // will store last time LED was updated

// constants won't change:
const long interval = 3000;           // interval at which to blink (milliseconds)


# define LORA_SS_PIN 46
# define LORA_RESET_PIN 7
# define LORA_DIO0_PIN 3
#define LORA_BYP_PIN 14
#define LORA_EN_PIN 17
#define LORA_BUSY_PIN 18
#define LORA_DIO1_PIN 1
#define LORA_DIO2_PIN 2

void setup() {

    // Begin serial communication
    Serial.begin(115200);

    delay(500);
    Serial.println("Starting setup!");

    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, HIGH);

    pinMode(LORA_BYP_PIN, OUTPUT);
    digitalWrite(LORA_BYP_PIN, HIGH);

    pinMode(LORA_EN_PIN, OUTPUT);
    digitalWrite(LORA_EN_PIN, HIGH);

 // Use this initializer if you're using a 1.8" TFT
 tft.init();   // initialize
 tft.setRotation(3); // Set the display image orientation to 0, 1, 2 or 3

 tft.fillScreen(TFT_BLACK);

 tft.setTextWrap(false);
 tft.fillScreen(TFT_BLACK);
 tft.setCursor(0, 10);
 tft.setTextColor(TFT_WHITE);
 tft.setTextSize(2);
 tft.println("LoRa Sender");


 
  Serial.println("LoRa Sender start");
 
  if (psramInit() == false)
      Serial.println("PSRAM failed to initialize");
  else
      Serial.println("PSRAM initialized");

  Serial.printf("PSRAM Size available (bytes): %d\r\n", ESP.getFreePsram());

  heap_caps_malloc_extmem_enable(8000); //Use PSRAM for memory requests larger than 1,000 bytes

  Serial.printf("PSRAM Size available (bytes): %d\r\n", ESP.getFreePsram());
  Serial.println(ESP.getPsramSize());


  if (!radio.begin()) 
  { 
      Serial.println("***Failed to initialize radio.");
      while (1);
  }
  else
  {
      Serial.println("***LoRa to initialize radio Ok!.");

  }


  
 /******************************
* ДОПОЛНИТЕЛЬНАЯ КОНФИГУРАЦИЯ
*************************
* Предустановки дают некоторую гибкость, чтобы радио работало так, как вам нужно, без необходимости
* понимать базовые концепции. См. пример AdvancedRadioConfig для более продвинутой конфигурации.
* Вообще говоря, большая дальность означает более медленную скорость, а меньшая дальность позволяет более высокую скорость
*
* ВСЕ ПЕРЕДАТЧИКИ/ПРИЕМНИКИ ДОЛЖНЫ ИМЕТЬ СООТВЕТСТВУЮЩИЕ КОНФИГУРАЦИИ, иначе
* они не смогут общаться друг с другом
*/

  // radio.configSetFrequency(902000000);  //Freq in Hz. Must comply with your local radio regulations

   radio.configSetFrequency(868800000);  //Freq in Hz. Must comply with your local radio regulations

  //Предустановки конфигурации. Комментируйте/раскомментируйте, чтобы увидеть, сколько времени занимает передача каждого пакета
  radio.configSetPreset(PRESET_DEFAULT);      //По умолчанию - Средний диапазон, средняя скорость
 // radio.configSetPreset(PRESET_FAST);       //Быстрый - Быстрее, но менее надежный на больших расстояниях. Используйте, когда вам нужна быстрая скорость, а радиостанции ближе.
 // radio.configSetPreset(PRESET_LONGRANGE);  //LongRange - Медленнее и надежнее. Подходит для больших расстояний или когда надежность важнее скорости


Serial.println("Setup END!");

}

void loop() 
{
 
 
  tft.setTextWrap(true, true);

  // Font and background colour, background colour is used for anti-alias blending
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

 
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval)
  {
      // save the last time you blinked the LED
      previousMillis = currentMillis;

      Serial.print("Sending packet: ");
      Serial.println(counter);

      // send packet "Hello world!"
       uint32_t startTime = millis();
     // radio.transmit(payload, strlen(payload));
      radio.transmit(payload, 40);
      uint32_t elapsed = millis() - startTime;
      Serial.print("Transmission time (ms): ");
      Serial.println(elapsed);
      delay(100);


      tft.setCursor(10, 70);
     // tft.println("Transmitting: OK!");
      tft.drawString("Sending: hello", 10, 120, 1);
      tft.drawNumber(counter, 195, 120, 1);


      if (ledState == LOW) 
      {
          ledState = HIGH;
      }
      else 
      {
          ledState = LOW;
      }

   
      digitalWrite(ledPin, ledState);

      counter++;

  }

}
