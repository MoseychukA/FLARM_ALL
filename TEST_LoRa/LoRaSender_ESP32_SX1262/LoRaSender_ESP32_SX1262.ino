
#include <Ra01S.h>
#include <SPI.h>


#define RF_FREQUENCY                                  868800000 // Hz  center frequency
#define TX_OUTPUT_POWER                             22        // dBm tx output power
#define LORA_BANDWIDTH                              4         // bandwidth
                                                              // 2: 31.25Khz
                                                              // 3: 62.5Khz
                                                              // 4: 125Khz
                                                              // 5: 250KHZ
                                                              // 6: 500Khz                                                               
#define LORA_SPREADING_FACTOR                       7         // spreading factor [SF5..SF12]
#define LORA_CODINGRATE                             1         // [1: 4/5,
                                                              //  2: 4/6,
                                                              //  3: 4/7,
                                                              //  4: 4/8]

#define LORA_PREAMBLE_LENGTH                        8         // Same for Tx and Rx
#define LORA_PAYLOADLENGTH                          0         // 0: Variable length packet (explicit header)
                                                              // 1..255  Fixed length packet (implicit header)



int counter = 0;
int ledPin = 4;
int ledState = LOW;             // ledState used to set the LED



////#define USE_EBYTE
//
//#if 1
///*
// * for ATmega328/2560
// * VCC    3V3/3V3
// * GND    GND/GND
// * SCK    13/52
// * MISO   12/50
// * MOSI   11/51
// * NSS     5/5
// * RST     6/6
// * BUSY    7/7
// * TXEN    8/8 for EBYTE
// * RXEN    9/9 for EBYTE
// */
//
//#ifdef USE_EBYTE
//SX126x  lora(46,               //Port-Pin Output: SPI select
//             7,               //Port-Pin Output: Reset 
//             14,               //Port-Pin Input:  Busy
//             1,               //Port-Pin Output: TXEN
//             2                //Port-Pin Output: RXEN
//             );
//
//#else
SX126x  lora(46,               //Port-Pin Output: SPI select
             7,               //Port-Pin Output: Reset 
             18,              //Port-Pin Input:  Busy
              -1,
              -1
             );
//#endif // USE_EBYTE
//
//#endif // ATmega328/2560


//#include <SoftwareSerial.h>

// Software Serial object to interface with my SIM900
//SoftwareSerial TestSerial(1, 2);
//SoftwareSerial swSer(1, 2, false, 256);

#define pin_GPIO14 14 
#define pin_GPIO17 17 
#define pin_GPIO1   1 
#define pin_GPIO2   2 

void setup() 
{
  delay(1000);
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);


  pinMode(pin_GPIO14, OUTPUT);
  pinMode(pin_GPIO17, OUTPUT);
 /* pinMode(pin_GPIO1, OUTPUT);
  pinMode(pin_GPIO2, OUTPUT);*/

  digitalWrite(pin_GPIO14, HIGH);
  digitalWrite(pin_GPIO17, HIGH);
  /*digitalWrite(pin_GPIO1, HIGH);
  digitalWrite(pin_GPIO2, HIGH);*/

 // lora.DebugPrint(true);

//#ifdef USE_EBYTE
//  Serial.println("Enable TCXO");
//  int16_t ret = lora.begin(RF_FREQUENCY,              //frequency in Hz
//                           TX_OUTPUT_POWER,           //tx power in dBm
//                           3.3,                       //use TCXO
//                           true);                     //use TCXO
//  if (ret != ERR_NONE) while(1) {delay(1);}
//#else
  Serial.println("Disable TCXO");
  int16_t ret = lora.begin(RF_FREQUENCY,              //frequency in Hz
                           TX_OUTPUT_POWER);          //tx power in dBm
  if (ret != ERR_NONE) while(1) {delay(1);}
//#endif // USE_EBYTE

  lora.LoRaConfig(LORA_SPREADING_FACTOR, 
                  LORA_BANDWIDTH, 
                  LORA_CODINGRATE, 
                  LORA_PREAMBLE_LENGTH, 
                  LORA_PAYLOADLENGTH, 
                  true,               //crcOn  
                  false);             //invertIrq
  Serial.println("Setup END!!");
}

void loop() 
{
  uint8_t txData[255];
  sprintf((char *)txData, "SX1262 %d", counter);
  uint8_t len = strlen((char *)txData);

  // Wait for transmission to complete
  if (lora.Send(txData, len, SX126x_TXMODE_SYNC)) 
  {
    Serial.println("Send SX1262 ");
  }
  else 
  {
    Serial.println("Send fail");
  }

  // Do not wait for the transmission to be completed
  //lora.Send(txData, len, SX126x_TXMODE_ASYNC );

  counter++;
  if (counter > 999) counter = 0;

  if (ledState == LOW)
  {
      ledState = HIGH;
  }
  else {
      ledState = LOW;
  }

  // set the LED with the ledState of the variable:
  digitalWrite(ledPin, ledState);

  delay(3000);
}
