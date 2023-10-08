#pragma once
#ifndef _MEMORY_H
#define _MEMORY_H
//--------------------------------------------------------------------------------------------------------------------------------
#include <Arduino.h>
//--------------------------------------------------------------------------------------------------------------------------------
//#define MEM_CONTROL_BYTE 0xBC
//--------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------
// настройки EEPROM (МИНИМАЛЬНЫЙ ОБЪЁМ EEPROM - 16 Кб !!!)
//--------------------------------------------------------------------------------------------------------------------------------
/*
ПОДДЕРЖИВАЕМЫЕ ТИПЫ ПАМЯТИ:

  EEPROM_BUILTIN // встроенный EEPROM
  EEPROM_AT24C32 // I2C-память AT24C32
  EEPROM_AT24C64 // I2C-память AT24C64
  EEPROM_AT24C128 // I2C-память AT24C128 - МИНИМАЛЬНЫЙ НЕОБХОДИМЫЙ ОБЪЁМ !!!
  EEPROM_AT24C256 // I2C-память AT24C256
  EEPROM_AT24C512 // I2C-память AT24C512
*/
#define EEPROM_USED_MEMORY EEPROM_AT24C128 // используем внешнюю память I2C AT24C128
// если используется внешнняя память AT24C* - то ниже определяется индекс микросхемы. 
//базовый адрес памяти на шине - 0x50.
#define EEPROM_MEMORY_INDEX 8
//--------------------------------------------------------------------------------------------------------------------------------
void MemInit();

//-----------------------------------------------------------------------------------------------------------------------
void MemWrite(unsigned int address, byte data);
void MemWrite(unsigned int address, byte* data, int n);
void MemWriteInt(unsigned int address, unsigned int data);
void MemWriteLong(unsigned int address, unsigned long data);
void MemWriteFloat(unsigned int address, float data);
void MemWriteDouble(unsigned int address, double data);
void MemWriteChars(unsigned int address, char* data, int length);
byte MemRead(unsigned int address);
void MemRead(unsigned int address, byte* data, int n);
unsigned int MemReadInt(unsigned int address);
unsigned long MemReadLong(unsigned int address);
float MemReadFloat(unsigned int address);
double MemReadDouble(unsigned int address);
void MemReadChars(unsigned int address, char* data, int n);


//--------------------------------------------------------------------------------------------------------------------------------

#endif