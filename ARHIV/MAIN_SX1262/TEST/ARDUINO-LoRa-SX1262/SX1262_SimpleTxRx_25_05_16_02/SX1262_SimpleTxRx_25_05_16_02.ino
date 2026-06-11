
#include <LoraSx1262.h>

LoraSx1262 radio;
char* payload = "Hello world.  This a pretty long payload.";

byte receiveBuff[255];

int counter = 0;
const int ledPin = 4;// the number of the LED pin

int ledState = LOW;             // ledState used to set the LED 

unsigned long previousMillis = 0;        // will store last time LED was updated

// constants won't change:
const long interval = 3000;           // interval at which to blink (milliseconds)

#define LMIC_UNUSED_PIN 255
#define LORA_SS_PIN 46
#define LORA_RESET_PIN 7
#define LORA_DIO0_PIN 42
#define LORA_BYP_PIN 14
#define LORA_EN_PIN 17
#define LORA_BUSY_PIN 18
#define LORA_DIO1_PIN 1
#define LORA_DIO2_PIN 2


void setup() {
    Serial.begin(115200);

    delay(500);
    Serial.println("Starting setup!");

    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, HIGH);

    pinMode(LORA_BYP_PIN, OUTPUT);
    digitalWrite(LORA_BYP_PIN, HIGH);

    pinMode(LORA_EN_PIN, OUTPUT);
    digitalWrite(LORA_EN_PIN, HIGH);


    if (!radio.begin())
    { //Initialize radio
        Serial.println("Failed to initialize radio.");
    }
    else
    {
        Serial.println("initialize radio Ok!.");
    }


    //FREQUENCY - Set frequency to 902Mhz (default 915Mhz)
    radio.configSetFrequency(868800000);  //Freq in Hz. Must comply with your local radio regulations

    //BANDWIDTH - Set bandwidth to 250khz (default 500khz)
    radio.configSetBandwidth(4);  //0=7.81khz, 5=200khz, 6=500khz. See documentation for more

    //CODING RATE - Set the coding rate to CR_4_6
    radio.configSetCodingRate(1); //1-4 = coding rate CR_4_5, CR_4_6, CR_4_7, and CR_4_8 respectively

    //SPREADING FACTOR - Set the spreading factor to SF12.  (default is SF7)
    radio.configSetSpreadingFactor(7); //5-12 are valid ranges.  5 is fast and short range, 12 is slow and long range


    //Configuration presets. Comment/uncomment to observe how long each packet takes to transmit
   // radio.configSetPreset(PRESET_DEFAULT);      //Default   - Medium range, medium speed
    //radio.configSetPreset(PRESET_FAST);       //Fast      - Faster, but less reliable at longer distances.  Use when you need fast speed and radios are closer.
   // radio.configSetPreset(PRESET_LONGRANGE);  //LongRange - Slower and more reliable.  Good for long distance or when reliability is more important than speed


}

void loop() {
  //Receive a packet over radio
  int bytesRead = radio.lora_receive_async(receiveBuff, sizeof(receiveBuff));

  if (bytesRead > -1) 
  {
    digitalWrite(ledPin, LOW);
    //Print the payload out over serial
    Serial.print("Received: ");
    Serial.write(receiveBuff,bytesRead);
    Serial.println(); //Add a newline after printing

    int Rssi = radio.lora_receive_rssi();
    Serial.print("Rssi: ");
    Serial.println(Rssi); //Add a newline after printing


    int snr = radio.lora_receive_snr();
    Serial.print("snr: ");
    Serial.println(snr); //SNR (соотношение сигнал/шум)

    int signalRssi = radio.lora_receive_signalRssi();
    Serial.print("signalRssi: ");
    Serial.println(signalRssi); //Add a newline after printing

    digitalWrite(ledPin, HIGH);
  }
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval)
  {
      // save the last time you blinked the LED
      previousMillis = currentMillis;
      digitalWrite(ledPin, LOW);
     // Serial.print("Transmitting... ");
      radio.transmit((byte*)payload, strlen(payload));
     // Serial.println("Done!");
      digitalWrite(ledPin, HIGH);

      //if (ledState == LOW)
      //{
      //    ledState = HIGH;
      //}
      //else
      //{
      //    ledState = LOW;
      //}
      //digitalWrite(ledPin, ledState);

      counter++;
  }
}
