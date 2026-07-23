#pragma once
//--------------------------------------------------------------------------------------------------------------------------------
#include <WString.h>
//--------------------------------------------------------------------------------------------------------------------------------
class ModuleController; // forward declaration
//--------------------------------------------------------------------------------------------------------------------------------
#include "Configuration_ESP32.h"
#include "TinyVector.h"

//--------------------------------------------------------------------------------------------------------------------------------
// структура для публикации
//--------------------------------------------------------------------------------------------------------------------------------
#pragma pack(push,1)
typedef struct
{
  bool Status : 1;                // Статус ответа на запрос: false - ошибка, true - нет ошибки
  bool AddModuleIDToAnswer : 1;   // добавлять ли имя модуля в ответ?
  bool Busy : 1;                  // флаг, что структура занята для записи
  byte pad : 5;
  
} PublishStructFlags;
#pragma pack(pop)
//--------------------------------------------------------------------------------------------------------------------------------
struct PublishStruct
{
  PublishStructFlags Flags;

  String Text; // текстовое сообщение о публикации, общий для всех буфер
  void* Data; // любая информация, в зависимости от типа модуля

  void Reset()
  {
    Flags.Status = false;
    Flags.AddModuleIDToAnswer = true;
    Flags.Busy = false;

    Text = "";
    Data = NULL;
  }

  PublishStruct& operator=(const String& src);
  PublishStruct& operator=(const char* src);
  PublishStruct& operator=(char src);
  PublishStruct& operator=(const __FlashStringHelper *src);
  PublishStruct& operator=(unsigned long src);
  PublishStruct& operator=(int src);
  PublishStruct& operator=(long src);

  PublishStruct& operator<<(const String& src);
  PublishStruct& operator<<(const char* src);
  PublishStruct& operator<<(char src);
  PublishStruct& operator<<(const __FlashStringHelper *src);
  PublishStruct& operator<<(unsigned long src);
  PublishStruct& operator<<(unsigned int src);
  PublishStruct& operator<<(int src);
  PublishStruct& operator<<(long src);
  
};
//--------------------------------------------------------------------------------------------------------------------------------
class AbstractModule
{
  private:
    const char* moduleID;    
    
protected:

public:

  AbstractModule(const char* id) : moduleID(id)
  { 

  }

 
  const char* GetID() {return moduleID;}
  void SetID(const char* id) {moduleID = id;}

  // функции, перегружаемые наследниками

  virtual void Setup() = 0; // вызывается для настроек модуля
  virtual void Update() = 0; // обновляет состояние модуля (для поддержки состояния периферии, например, включение диода)
  
};
//--------------------------------------------------------------------------------------------------------------------------------

