#pragma once

#include <Arduino.h>
#include "TinyVector.h"
#include "SettingsMail.h"
//#include "EEPROMRF.h"

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
    
    void setup();
    void handleCommands();
    void processCommand(const String& command,Stream* outStream);
    bool getVER(Stream* pStream);
    //void Stm32_SoftReset(void);                                          // Системный сброс 

 private:

    void onUnknownCommand(const String& command, Stream* outStream);
    bool getVOLTAGEAKK(const char* commandPassed, const CommandParser& parser, Stream* pStream); // получение напряжения на аккумуляторе
    bool getTIMEAKK(const char* commandPassed, const CommandParser& parser, Stream* pStream);    // получить время работы аккумулятора в часах.   Пример #1#GET#TIMEAKK
    bool setTIMEAKK(const char* commandPassed, CommandParser& parser, Stream* pStream);          // установить время работы аккумулятора в часах. Пример #1#GET#TIMEAKK#8
    bool getTIMELCD(const char* commandPassed, const CommandParser& parser, Stream* pStream);    // получить время работы дисплея в секундах.     Пример #1#GET#TIMELCD
    bool setTIMELCD(const char* commandPassed, CommandParser& parser, Stream* pStream);          // установить время работы дисплея в секундах.   Пример #1#GET#TIMELCD#10
    bool printBackSETResult(bool isOK, const char* command, Stream* pStream);
    bool setTXT(const char* commandPassed, CommandParser& parser, Stream* pStream, String textString); // 
 
};
//--------------------------------------------------------------------------------------------------------------------------------------
extern CommandHandlerClass CommandHandler;


