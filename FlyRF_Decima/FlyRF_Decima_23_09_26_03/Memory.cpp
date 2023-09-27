#include "AT24CX.h"               // Поддержка энергонезависимой памяти
#include "Memory.h"
#include "Configuration_ESP32.h"  // Основные настройки программы

AT24CX* memAT24CX;




//--------------------------------------------------------------------------------------------------------------------------------
void MemInit()
{

	memAT24CX = new AT24C128(EEPROM_MEMORY_INDEX);


//
//#if EEPROM_USED_MEMORY == EEPROM_AT24C32
//    memAT24CX = new AT24C32(EEPROM_MEMORY_INDEX);
//#elif EEPROM_USED_MEMORY == EEPROM_AT24C64
//    memAT24CX = new AT24C64(EEPROM_MEMORY_INDEX);
//#elif EEPROM_USED_MEMORY == EEPROM_AT24C128
//    memAT24CX = new AT24C128(EEPROM_MEMORY_INDEX);
//#elif EEPROM_USED_MEMORY == EEPROM_AT24C256
//    memAT24CX = new AT24C256(EEPROM_MEMORY_INDEX);
//#elif EEPROM_USED_MEMORY == EEPROM_AT24C512
//    memAT24CX = new AT24C512(EEPROM_MEMORY_INDEX);
//#endif 
}

//--------------------------------------------------------------------------------------------------------------------------------
void MemWriteChars(unsigned int address, char* data, int length)
{
    memAT24CX->writeChars(address,data, length);
}
//--------------------------------------------------------------------------------------------------------------------------------
void MemReadChars(unsigned int address, char* data, int n)
{
    memAT24CX->readChars(address, data, n);
}
//--------------------------------------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------------------------------------
void MemWrite(unsigned int address, byte data)
{
    memAT24CX->write(address, data);
}
//--------------------------------------------------------------------------------------------------------------------------------
void MemWrite(unsigned int address, byte* data, int n)
{
    memAT24CX->write(address, data, n);
}
//--------------------------------------------------------------------------------------------------------------------------------
void MemWriteInt(unsigned int address, unsigned int data)
{
    memAT24CX->writeInt(address, data);
}
//--------------------------------------------------------------------------------------------------------------------------------
void MemWriteLong(unsigned int address, unsigned long data)
{
    memAT24CX->writeLong(address, data);
}
//--------------------------------------------------------------------------------------------------------------------------------
void MemWriteFloat(unsigned int address, float data)
{
    memAT24CX->writeFloat(address, data);
}
//--------------------------------------------------------------------------------------------------------------------------------
void MemWriteDouble(unsigned int address, double data)
{
    memAT24CX->writeDouble(address, data);
}
//--------------------------------------------------------------------------------------------------------------------------------



//--------------------------------------------------------------------------------------------------------------------------------
void MemRead(unsigned int address, byte* data, int n)
{
    memAT24CX->read(address, data, n);
}
//--------------------------------------------------------------------------------------------------------------------------------
unsigned int MemReadInt(unsigned int address)
{
    memAT24CX->readInt(address);
}
//--------------------------------------------------------------------------------------------------------------------------------
unsigned long MemReadLong(unsigned int address)
{
    memAT24CX->readLong(address);
}
//--------------------------------------------------------------------------------------------------------------------------------
float MemReadFloat(unsigned int address)
{
    memAT24CX->readFloat(address);
}
//--------------------------------------------------------------------------------------------------------------------------------
double MemReadDouble(unsigned int address)
{
    memAT24CX->readDouble(address);
}
//--------------------------------------------------------------------------------------------------------------------------------
uint8_t MemRead(unsigned int address)
{
    return memAT24CX->read(address);
}
//--------------------------------------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------------------------------------


/*    Примеры применения  */
/*
  // read and write byte
  Serial.println("Write 42 to address 12");
  mem.write(12, 42);
  Serial.println("Read byte from address 12 ...");
  byte b = mem.read(12);
  Serial.print("... read: ");
  Serial.println(b, DEC);
  Serial.println();
  
  // read and write integer
  Serial.println("Write 65000 to address 15");
  mem.writeInt(15, 65000);
  Serial.println("Read integer from address 15 ...");
  unsigned int i = mem.readInt(15);
  Serial.print("... read: ");
  Serial.println(i, DEC);
  Serial.println();

  // read and write long
  Serial.println("Write 3293732729 to address 20");
  mem.writeLong(20, 3293732729UL);
  Serial.println("Read long from address 20 ...");
  unsigned long l = mem.readLong(20);
  Serial.print("... read: ");
  Serial.println(l, DEC);
  Serial.println();

  // read and write long
  Serial.println("Write 1111111111 to address 31");
  mem.writeLong(31, 1111111111);
  Serial.println("Read long from address 31 ...");
  unsigned long l2 = mem.readLong(31);
  Serial.print("... read: ");
  Serial.println(l2, DEC);
  Serial.println();
  
  // read and write float
  Serial.println("Write 3.14 to address 40");
  mem.writeFloat(40, 3.14);
  Serial.println("Read float from address 40 ...");
  float f = mem.readFloat(40);
  Serial.print("... read: ");
  Serial.println(f, DEC);
  Serial.println();  

  // read and write double
  Serial.println("Write 3.14159265359 to address 50");
  mem.writeDouble(50, 3.14159265359);
  Serial.println("Read double from address 50 ...");
  double d = mem.readDouble(50);
  Serial.print("... read: ");
  Serial.println(d, DEC);
  Serial.println();
  
  // read and write char
  Serial.print("Write chars: '");
  char msg[] = "This is a message";
  Serial.print(msg);
  Serial.println("' to address 200");
  mem.writeChars(200, msg, sizeof(msg));
  Serial.println("Read chars from address 200 ...");
  char msg2[30];
  mem.readChars(200, msg2, sizeof(msg2));
  Serial.print("... read: '");
  Serial.print(msg2);
  Serial.println("'");
  Serial.println();

  // write array of bytes 
  Serial.println("Write array of 80 bytes at address 1000");
  byte xy[] = {0,0,0,1,1,1,2,2,2,3,3,3,4,4,4,5,5,5,6,6,6,7,7,7,8,8,8,9,9,9,    // 10 x 3 = 30
              10,11,12,13,14,15,16,17,18,19,                                   //          10
              120,121,122,123,124,125,126,127,128,129,                         //          10
              130,131,132,133,134,135,136,137,138,139,                         //          10
              200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,217,218,219};                   //          20
  mem.write(1000, (byte*)xy, sizeof(xy));

  // read bytes with multiple steps
  Serial.println("Read 80 single bytes starting at address 1000");
  for (int i=0; i<sizeof(xy); i++) {
    byte sb = mem.read(1000+i);
    Serial.print("[");
    Serial.print(1000+i);
    Serial.print("] = ");
    Serial.println(sb);
  } 
  Serial.println();

  // read bytes with one step
  Serial.println("Read 80 bytes with one operation at address 1000");
  byte z[80];
  memset(&z[0], 32, sizeof(z));
  mem.read(1000, z, sizeof(z));
  for (int i=0; i<sizeof(z); i++) {
    Serial.print("[");
    Serial.print(1000+i);
    Serial.print("] = ");
    Serial.println(z[i]);
  } 
  
*/