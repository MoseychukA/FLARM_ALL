#pragma once

#include <Arduino.h>
#include "TinyVector.h"
//#include "InterruptHandler.h"
//--------------------------------------------------------------------------------------------------------------------------------------
// класс для накопления команды из потока
//--------------------------------------------------------------------------------------------------------------------------------------
class CoreCommandBuffer
{
private:
  Stream* pStream;
  String* strBuff;
public:
  CoreCommandBuffer(Stream* s);

  bool hasCommand();
  const String& getCommand() {return *strBuff;}
  void clearCommand() {delete strBuff; strBuff = new String(); }
  Stream* getStream() {return pStream;}

};
//--------------------------------------------------------------------------------------------------------------------------------------
extern CoreCommandBuffer Commands;
//--------------------------------------------------------------------------------------------------------------------------------------
typedef Vector<char*> CommandArgsVec;
//--------------------------------------------------------------------------------------------------------------------------------------
class CommandParser // класс-парсер команды из потока
{
  private:
    CommandArgsVec arguments;
  public:
    CommandParser();
    ~CommandParser();

    void clear();
    bool parse(const String& command, bool isSetCommand);
    bool parseTXT(const String& command, bool isSetCommand);
    const char* getArg(size_t idx) const;
    size_t argsCount() const {return arguments.size();}
};
//--------------------------------------------------------------------------------------------------------------------------------------
class CommandHandlerClass // класс-обработчик команд из потока
{
  public:
  
    CommandHandlerClass();
    
    void handleCommands();
    void processCommand(const String& command,Stream* outStream);

   // int  getFreeMemory();
    bool getVER(Stream* pStream);
    void ESP32_SoftReset(void);                                          // Системный сброс 

 private:
  void onUnknownCommand(const String& command, Stream* outStream);
  
 
  
  bool getFREERAM(const char* commandPassed, Stream* pStream); // получение свободной памяти

  bool setDATETIME(const char* param); // установка даты/времени
  bool getDATETIME(const char* commandPassed, Stream* pStream); // получение даты/времени
  void setCurrentDateTime(uint8_t day, uint8_t month, uint16_t year, uint8_t hour, uint8_t minute, uint8_t second); // установка даты/времени
  bool getVOLTAGEAKK(const char* commandPassed, const CommandParser& parser, Stream* pStream); // получение напряжения на аккумуляторе
  bool getTIMEAKK(const char* commandPassed, const CommandParser& parser, Stream* pStream);    // получить время работы аккумулятора в часах.   Пример #1#GET#TIMEAKK
  bool printBackSETResult(bool isOK, const char* command, Stream* pStream);
  bool setTXT(const char* commandPassed, CommandParser& parser, Stream* pStream, String textString); // 
  bool clearMemory(const char* commandPassed, CommandParser& parser);  // Стереть всю память
  bool getSSID(const char* commandPassed, const CommandParser& parser, Stream* pStream); // получение настроек WiFi
  bool setSSID(const char* commandPassed, CommandParser& parser, Stream* pStream); // записать настройки WiFi
 
};
//--------------------------------------------------------------------------------------------------------------------------------------
extern CommandHandlerClass CommandHandler;


