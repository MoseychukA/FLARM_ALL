#include "AbstractModule.h"
//--------------------------------------------------------------------------------------------------------------------------------
PublishStruct& PublishStruct::operator=(const String& src)
{
  this->Text = src;
  return *this;
}
//--------------------------------------------------------------------------------------------------------------------------------
PublishStruct& PublishStruct::operator=(const char* src)
{
  this->Text = src;
  return *this;
}
//--------------------------------------------------------------------------------------------------------------------------------
PublishStruct& PublishStruct::operator=(char src)
{
  this->Text = src;
  return *this;  
}
//--------------------------------------------------------------------------------------------------------------------------------
PublishStruct& PublishStruct::operator=(const __FlashStringHelper *src)
{
  this->Text = src;
  return *this;    
}
//--------------------------------------------------------------------------------------------------------------------------------
PublishStruct& PublishStruct::operator=(unsigned long src)
{
  this->Text = src;
  return *this;    
  
}
//--------------------------------------------------------------------------------------------------------------------------------
PublishStruct& PublishStruct::operator=(int src)
{
  this->Text = src;
  return *this;      
}
//--------------------------------------------------------------------------------------------------------------------------------
PublishStruct& PublishStruct::operator=(long src)
{
  this->Text = src;
  return *this;      
  
}
//--------------------------------------------------------------------------------------------------------------------------------
PublishStruct& PublishStruct::operator<<(const String& src)
{
  this->Text += src;
  return *this;
}
//--------------------------------------------------------------------------------------------------------------------------------
PublishStruct& PublishStruct::operator<<(const char* src)
{
  this->Text += src;
  return *this;
}
//--------------------------------------------------------------------------------------------------------------------------------
PublishStruct& PublishStruct::operator<<(char src)
{
  this->Text += src;
  return *this;  
}
//--------------------------------------------------------------------------------------------------------------------------------
PublishStruct& PublishStruct::operator<<(const __FlashStringHelper *src)
{
  this->Text += src;
  return *this;    
}
//--------------------------------------------------------------------------------------------------------------------------------
PublishStruct& PublishStruct::operator<<(unsigned long src)
{
  this->Text += src;
  return *this;    
  
}
//--------------------------------------------------------------------------------------------------------------------------------
PublishStruct& PublishStruct::operator<<(int src)
{
  this->Text += src;
  return *this;      
}
//--------------------------------------------------------------------------------------------------------------------------------
PublishStruct& PublishStruct::operator<<(unsigned int src)
{
  this->Text += src;
  return *this;        
}
//--------------------------------------------------------------------------------------------------------------------------------
PublishStruct& PublishStruct::operator<<(long src)
{
  this->Text += src;
  return *this;      
}
//--------------------------------------------------------------------------------------------------------------------------------
