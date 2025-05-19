#include <SPI.h>
#include <LoraSx1262.h>
#include <TFT_eSPI.h> // Hardware-specific library

TFT_eSPI tft = TFT_eSPI();       // Invoke custom library

int counter = 0;
const int ledPin =  4;

int ledState = LOW;             // ledState used to set the LED 

unsigned long previousMillis = 0;        // will store last time LED was updated

// constants won't change:
const long interval = 3000;           // interval at which to blink (milliseconds)


# define LORA_SS_PIN 46
# define LORA_RESET_PIN 7
# define LORA_DIO0_PIN 3
# define LORA_BYP_PIN 14
# define LORA_EN_PIN 17
# define LORA_BUSY_PIN 18
# define LORA_DIO1_PIN 1
# define LORA_DIO2_PIN 2


const char* payload = "SX1262 ";
LoraSx1262 radio;

void setup() 
{
  // put your setup code here, to run once:
    Serial.begin(115200);

    delay(500);
    Serial.println("Starting setup!");

    pinMode(ledPin, OUTPUT); 
    digitalWrite(ledPin, HIGH);

    pinMode(LORA_BYP_PIN, OUTPUT);
    digitalWrite(LORA_BYP_PIN, LOW);

    pinMode(LORA_EN_PIN, OUTPUT);
    digitalWrite(LORA_EN_PIN, LOW);

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
  radio.configSetPreset(PRESET_DEFAULT);      //ѕо умолчанию - —редний диапазон, средн¤¤ скорость
 // radio.configSetPreset(PRESET_FAST);       //Ѕыстрый - Ѕыстрее, но менее надежный на больших рассто¤ни¤х. »спользуйте, когда вам нужна быстра¤ скорость, а радиостанции ближе.
 // radio.configSetPreset(PRESET_LONGRANGE);  //LongRange - ћедленнее и надежнее. ѕодходит дл¤ больших рассто¤ний или когда надежность важнее скорости

  Serial.println("Setup END!");


}

void loop() 
{
  Serial.print("Transmitting... ");
  radio.transmit((byte*)payload, strlen(payload));
  Serial.println("Done!");

  counter++;

  if (ledState == LOW)
  {
      ledState = HIGH;
  }
  else {
      ledState = LOW;
  }

  // set the LED with the ledState of the variable:
  digitalWrite(ledPin, ledState);

  delay(1500);
}
