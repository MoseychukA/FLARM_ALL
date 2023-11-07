#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>



//#define Serial SERIAL_PORT_USBVIRTUAL   // Подключаем USB порт в качестве COM порта
const int ledPin =  28;//LED_BUILTIN;// the number of the LED pin
//const int ledPin10 = 10;// the number of the LED pin
// Variables will change :
int ledState = LOW;             // ledState used to set the LED

// set pin for SAMD21E18A
//setPins(int ss = LORA_DEFAULT_SS_PIN, int reset = LORA_DEFAULT_RESET_PIN, int dio0 = LORA_DEFAULT_DIO0_PIN);
# define LORA_SS_PIN 23
# define LORA_RESET_PIN 4
# define LORA_DIO0_PIN 5

# define RFID_A0_PIN 2
# define RFID_A1_PIN 3
//+++++++++++++++++++++++++++++ Внешняя память +++++++++++++++++++++++++++++++++++++++
int deviceaddress = 0x57 ;                       // Адрес микросхемы памяти
//unsigned int eeaddress = 0x00;                  // Адрес ячейки памяти
byte hi;                                     // Старший байт для преобразования числа
byte low;                                    // Младший байт для преобразования числа



// чтение
unsigned long i2c_eeprom_ulong_read(int addr)
{
  byte raw[4];
  for (byte i = 0; i < 4; i++) raw[i] = i2c_eeprom_read_byte(deviceaddress, addr + i);
  unsigned long &num = (unsigned long&)raw;
  return num;
}

// запись
void i2c_eeprom_ulong_write(int addr, unsigned long num)
{
  byte raw[4];
  (unsigned long&)raw = num;
  for (byte i = 0; i < 4; i++) i2c_eeprom_write_byte(deviceaddress, addr + i, raw[i]);
}

//+++++++++++++++++++++++  Настройки +++++++++++++++++++++++++++++++++++++
 
void i2c_eeprom_write_byte(int deviceaddress, unsigned int eeaddress, byte data)
{
  int rdata = data;
  Wire.beginTransmission(deviceaddress);
  Wire.write((int)(eeaddress >> 8)); // MSB
  Wire.write((int)(eeaddress & 0xFF)); // LSB
  Wire.write(rdata);
  Wire.endTransmission();
  delay(10);
}
byte i2c_eeprom_read_byte(int deviceaddress, unsigned int eeaddress) {
  byte rdata = 0xFF;
  Wire.beginTransmission(deviceaddress);
  Wire.write((int)(eeaddress >> 8)); // MSB
  Wire.write((int)(eeaddress & 0xFF)); // LSB
  Wire.endTransmission();
  Wire.requestFrom(deviceaddress, 1);
  if (Wire.available()) rdata = Wire.read();
  return rdata;
}
void i2c_eeprom_read_buffer( int deviceaddress, unsigned int eeaddress, byte *buffer, int length )
{

Wire.beginTransmission(deviceaddress);
Wire.write((int)(eeaddress >> 8)); // MSB
Wire.write((int)(eeaddress & 0xFF)); // LSB
Wire.endTransmission();
Wire.requestFrom(deviceaddress,length);
int c = 0;
for ( c = 0; c < length; c++ )
if (Wire.available()) buffer[c] = Wire.read();

}
void i2c_eeprom_write_page( int deviceaddress, unsigned int eeaddresspage, byte* data, byte length )
{
Wire.beginTransmission(deviceaddress);
Wire.write((int)(eeaddresspage >> 8)); // MSB
Wire.write((int)(eeaddresspage & 0xFF)); // LSB
byte c;
for ( c = 0; c < length; c++)
Wire.write(data[c]);
Wire.endTransmission();

}
void i2c_test()
{

  Serial.println("--------  EEPROM Test  ---------");

 M24LR16E_write(100);
  
//for(int i=0;i<100;i++)
//{
//   i2c_eeprom_write_byte(deviceaddress, i,i); // write to EEPROM
//}



for(int i=0;i<4+2332;i++)
{
 byte b = i2c_eeprom_read_byte(deviceaddress, i); // access the first address from the memory
  Serial.print(i); //print content to serial port
  Serial.print(" - "); 
   Serial.println(b,HEX); //print content to serial port
}
  /*
  char somedata[] = "this data from the eeprom i2c"; // data to write
                             //i2c_eeprom_write_page(0x50, 0, (byte *)somedata, sizeof(somedata)); // write to EEPROM
  i2c_eeprom_write_page(deviceaddress, 0, (byte *)somedata, sizeof(somedata)); // write to EEPROM
  delay(100); //add a small delay
  Serial.println("Written Done");
  delay(10);
  Serial.print("Read EERPOM:");
  byte b = i2c_eeprom_read_byte(deviceaddress, 0); // access the first address from the memory
  char addr = 0; //first address

  while (b != 0)
  {
    Serial.print((char)b); //print content to serial port
    if (b != somedata[addr])
    {
      break;
    }
    addr++; //increase address
    b = i2c_eeprom_read_byte(deviceaddress, addr); //access an address from the memory
  }

*/


 // i2c_eeprom_ulong_write(255,4096);
unsigned long xx = i2c_eeprom_ulong_read(255);
  Serial.println();

     Serial.println(xx); //print content to serial port
  Serial.println();

}

void  M24LR16E_write(int adress)
{
int ad=2304;
    Wire.beginTransmission(deviceaddress);
    Wire.write((int)(ad >> 8));   // MSB
  Wire.write((int)(ad & 0xFF));


  Wire.write((byte)0x00);
  Wire.write((byte)0x00);
  Wire.write((byte)0x00);
  Wire.write((byte)0x00);

  Wire.write((byte)0x09);

  Wire.write((byte)0x00);
  Wire.write((byte)0x00);
  Wire.write((byte)0x00);
  Wire.write((byte)0x66);
  Wire.endTransmission(); 
  
    Wire.beginTransmission(deviceaddress);
    Wire.write((int)(adress >> 8));   // MSB
  Wire.write((int)(adress & 0xFF));
   Wire.write((byte)0x55);
  Wire.endTransmission();
  }



void setup() {
  Serial.begin(57600);
  Wire.begin();
  while (!Serial);                     // Ожидаем включение СОМ порта, иначе информация будет выводиться сразу 
 pinMode (ledPin, OUTPUT);
 //  pinMode (ledPin10, OUTPUT);
 LoRa.setPins(LORA_SS_PIN, LORA_RESET_PIN, LORA_DIO0_PIN);

 pinMode (RFID_A0_PIN, OUTPUT);
 pinMode (RFID_A1_PIN, OUTPUT);
 digitalWrite(RFID_A0_PIN, HIGH);
 digitalWrite(RFID_A1_PIN, HIGH);
//digitalWrite(RFID_A0_PIN, LOW);
// digitalWrite(RFID_A1_PIN, LOW);

 
  Serial.println("LoRa Receiver");
//i2c_test();



  if (!LoRa.begin(868E6)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
}

void loop() {
  // try to parse packet
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    // received a packet
    Serial.print("Received packet '");

    // read packet
    while (LoRa.available()) {
      Serial.print((char)LoRa.read());
    }

    // print RSSI of packet
    Serial.print("' with RSSI ");
    Serial.println(LoRa.packetRssi());
      if (ledState == LOW) {
      ledState = HIGH;
    } else {
      ledState = LOW;
    }

    // set the LED with the ledState of the variable:
    digitalWrite(ledPin, ledState);
  }
}
