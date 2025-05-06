#include <SPI.h>
#include <LoraSx1262.h>
#include <TFT_eSPI.h> // Hardware-specific library

TFT_eSPI tft = TFT_eSPI();       // Invoke custom library

int counter = 0;
const int ledPin =  4;//LED_BUILTIN;// the number of the LED pin

int ledState = LOW;             // ledState used to set the LED 

unsigned long previousMillis = 0;        // will store last time LED was updated

// constants won't change:
const long interval = 3000;           // interval at which to blink (milliseconds)


# define LORA_SS_PIN 46
# define LORA_RESET_PIN 7
# define LORA_DIO0_PIN 3

const char* payload = "Hello world.  This a pretty long payload.";
LoraSx1262 radio;

void setup() 
{
  // put your setup code here, to run once:
    Serial.begin(115200);

    delay(500);
    Serial.println("Starting setup!");

    pinMode(ledPin, OUTPUT); 
    digitalWrite(ledPin, HIGH);

  if (!radio.begin()) 
  { 
    Serial.println("Failed to initialize radio.");
  }
  else
  {
      Serial.println("Initialize LoraSx1262 Ok!");
  }

  //FREQUENCY - Set frequency to 902Mhz (default 915Mhz)
  radio.configSetFrequency(868800000);  //Freq in Hz. Must comply with your local radio regulations

  ////BANDWIDTH - Set bandwidth to 250khz (default 500khz)
  //radio.configSetBandwidth(5);  //0=7.81khz, 5=200khz, 6=500khz. See documentation for more

  ////CODING RATE - Set the coding rate to CR_4_6
  //radio.configSetCodingRate(2); //1-4 = coding rate CR_4_5, CR_4_6, CR_4_7, and CR_4_8 respectively

  ////SPREADING FACTOR - Set the spreading factor to SF12.  (default is SF7)
  //radio.configSetSpreadingFactor(12); //5-12 are valid ranges.  5 is fast and short range, 12 is slow and long range
    //ѕредустановки конфигурации.  омментируйте/раскомментируйте, чтобы увидеть, сколько времени занимает передача каждого пакета
 // radio.configSetPreset(PRESET_DEFAULT);      //ѕо умолчанию - —редний диапазон, средн¤¤ скорость
 // radio.configSetPreset(PRESET_FAST);       //Ѕыстрый - Ѕыстрее, но менее надежный на больших рассто¤ни¤х. »спользуйте, когда вам нужна быстра¤ скорость, а радиостанции ближе.
  radio.configSetPreset(PRESET_LONGRANGE);  //LongRange - ћедленнее и надежнее. ѕодходит дл¤ больших рассто¤ний или когда надежность важнее скорости


  tft.init();   // initialize
  //tft.setRotation(3); // Set the display image orientation to 0, 1, 2 or 3

  //tft.fillScreen(TFT_BLACK);

  //tft.setTextWrap(false);
  //tft.fillScreen(TFT_BLACK);
  //tft.setCursor(0, 10);
  //tft.setTextColor(TFT_WHITE);
  //tft.setTextSize(2);
  //tft.println("LoRa Sender");

  Serial.println("Setup END!");


}

void loop() {
  Serial.print("Transmitting... ");
  radio.transmit((byte*)payload, strlen(payload));
  Serial.println("Done!");

  delay(1000);
}
