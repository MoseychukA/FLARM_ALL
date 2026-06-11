#pragma once
#ifndef _MEMORY_H
#define _MEMORY_H
//--------------------------------------------------------------------------------------------------------------------------------
#include <Arduino.h>

//--------------------------------------------------------------------------------------------------------------------------------
void MemInit();

void MemClear();

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
String MemReadString(unsigned int address);
void MemCommit();


//--------------------------------------------------------------------------------------------------------------------------------

#endif