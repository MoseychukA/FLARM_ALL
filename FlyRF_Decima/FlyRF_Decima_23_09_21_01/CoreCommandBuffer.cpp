#include "CoreCommandBuffer.h"
#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>
#include "Settings.h"
#include "Configuration_ESP32.h"  // Основные настройки программы
#include <Stream.h>
#include "TinyVector.h"




//#define EEPROM_SIZE 500
// EEPROM object


//--------------------------------------------------------------------------------------------------------------------------------------
// список поддерживаемых команд
//--------------------------------------------------------------------------------------------------------------------------------------
const char SSID_COMMAND[] PROGMEM = "SSID"; // получить/установить имя точки доступа WiFi
const char PASS_COMMAND[] PROGMEM = "PASS"; // получить/установить пароль точки доступа WiFi
const char IP_COMMAND[] PROGMEM = "IP";   // получить IP адрес, полученный точкой доступа WiFi

const char DATETIME_COMMAND[] PROGMEM = "DATETIME"; // получить/установить дату/время на контроллер
const char FREERAM_COMMAND[] PROGMEM = "FREERAM"; // получить информацию о свободной памяти
const char PIN_COMMAND[] PROGMEM = "PIN"; // установить уровень на пине
const char LS_COMMAND[] PROGMEM = "LS"; // отдать список файлов
const char FILE_COMMAND[] PROGMEM = "FILE"; // отдать содержимое файла
const char FILESIZE_COMMAND[] PROGMEM = "FILESIZE"; // отдать размер файла
const char DELFILE_COMMAND[] PROGMEM = "DELFILE"; // удалить файл
const char UPLOADFILE_COMMAND[] PROGMEM = "UPL"; // загрузить файл
const char VOLTAGE_AKK_COMMAND[] PROGMEM = "AKK"; // получить вольтаж на аккумуляторе
const char UUID_COMMAND[] PROGMEM = "UUID"; // получить уникальный идентификатор контроллера
const char VERSION_COMMAND[] PROGMEM = "VER"; // отдать информацию о версии
const char SDTEST_COMMAND[] PROGMEM = "SDTEST"; // запустить тест SD

//--------------------------------------------------------------------------------------------------------------------------------------
CoreCommandBuffer Commands(&SerialDEBUG);
//--------------------------------------------------------------------------------------------------------------------------------------
CoreCommandBuffer::CoreCommandBuffer(Stream* s) : pStream(s) // конструктор
{
    strBuff = new String();
    strBuff->reserve(BUFFER_SIZE+40);
}
//--------------------------------------------------------------------------------------------------------------------------------------
bool CoreCommandBuffer::hasCommand()                // проверяет на наличие входящей команды
{
  
  if(!(pStream && pStream->available()))
  {
    return false;
  }

    char ch; 
    if (pStream->available())                       // Определить наличие символа в порту трекера
    {
        SerialDEBUG.print("pStream->available ");

        while (pStream->available()>0)              // читаем данные во внутренний буфер
        {
            ch = (char)pStream->read();

            if (ch == '\r')                         // Пропустить, не записывать в буфер
            {
                continue;
            }
 
            if (ch == '\n')                         // Пропустить, не записывать в буфер
            {
                continue;
            } // if
            *strBuff += ch;

            delay(5);
            // не даём вычитать больше символов, чем надо - иначе нас можно заспамить
            if (strBuff->length() >= BUFFER_SIZE)   // Если иформации больше чем BUFFER_SIZE - принимать не будем и очистим буфер
            {
                clearCommand();
                return false;
            } // if
        } // while
        //SerialDEBUG.print("strBuff->length() ");
        //SerialDEBUG.println(strBuff->length());
       return true;   // Завершили чтение сообщения. Информация находится в strBuff
    }
   return false;     // Новой информации не поступало.
}
//--------------------------------------------------------------------------------------------------------------------------------------
CommandParser::CommandParser() // констуктор
{
  
}
//--------------------------------------------------------------------------------------------------------------------------------------
CommandParser::~CommandParser() // деструктор
{
  clear();
}
//--------------------------------------------------------------------------------------------------------------------------------------
void CommandParser::clear() // очищает внутренние данные
{
  for(size_t i=0;i<arguments.size();i++)
  {
    delete [] arguments[i];  
  }

  arguments.clear();
 
}
//--------------------------------------------------------------------------------------------------------------------------------------
const char* CommandParser::getArg(size_t idx) const // возвращает аргумент команды по индексу
{
    if (arguments.size() && idx < arguments.size())
        return arguments[idx];

    return NULL;
}
//--------------------------------------------------------------------------------------------------------------------------------------
bool CommandParser::parse(const String& command, bool isSetCommand) // разбирает входящую строку на параметры
{
    clear();
    // разбиваем на аргументы
    const char* startPtr = command.c_str() + strlen_P(isSetCommand ? (const char*)CORE_COMMAND_SET : (const char*)CORE_COMMAND_GET);
    size_t len = 0;

    while (*startPtr)
    {
        const char* delimPtr = strchr(startPtr, CORE_COMMAND_PARAM_DELIMITER);

        if (!delimPtr)
        {
            len = strlen(startPtr);
            char* newArg = new char[len + 1];
            memset(newArg, 0, len + 1);
            strncpy(newArg, startPtr, len);
            arguments.push_back(newArg);

            return arguments.size();
        } // if(!delimPtr)

        size_t len = delimPtr - startPtr;


        char* newArg = new char[len + 1];
        memset(newArg, 0, len + 1);
        strncpy(newArg, startPtr, len);
        arguments.push_back(newArg);

        startPtr = delimPtr + 1;

    } // while      

    return arguments.size();

}
//--------------------------------------------------------------------------------------------------------------------------------------
// CommandHandlerClass
//--------------------------------------------------------------------------------------------------------------------------------------
CommandHandlerClass CommandHandler;
//--------------------------------------------------------------------------------------------------------------------------------------
CommandHandlerClass::CommandHandlerClass() // конструктор
{
  
}
//--------------------------------------------------------------------------------------------------------------------------------------
void CommandHandlerClass::handleCommands() // обработчик входящих сообщений в loop
{
  if(Commands.hasCommand())   // Пришло новое сообщение 
  {    

    String command = Commands.getCommand();  // Скопировать буфер с сообщением 

	//DBGLN("handleCommands...");
	//DBGLN(command.startsWith(CORE_COMMAND_SET));
	//DBGLN(command.startsWith(CORE_COMMAND_GET));

    if (command.startsWith(CORE_COMMAND_GET) || command.startsWith(CORE_COMMAND_SET))
    {
        Stream* pStream = Commands.getStream();
        processCommand(command, pStream);
    
    }
    Commands.clearCommand(); // очищаем буфер команд
  
  } // if(Commands.hasCommand())  
}


void CommandHandlerClass::processCommand(const String& command, Stream* pStream) // выполнение входящей команды
{
    bool commandHandled = false;

	//DBGLN("processCommand()");
 //   DBGLN(command.startsWith(CORE_COMMAND_SET));
 //   DBGLN(command.startsWith(CORE_COMMAND_GET));


    if (command.startsWith(CORE_COMMAND_SET)) // команда установки свойств
    {
        CommandParser cParser;
        if (cParser.parse(command, true)) // если разобрали команду
        {
            const char* commandName = cParser.getArg(0);

            if (!strcmp_P(commandName, DATETIME_COMMAND)) // DATETIME, установка даты/времени
            {
                if (cParser.argsCount() > 1)
                {
                    // запросили установку даты/времени, приходит строка вида 25.12.2017 12:23:49
                    const char* paramPtr = cParser.getArg(1);
                    commandHandled = printBackSETResult(setDATETIME(paramPtr), commandName, pStream);
                }
                else
                {
                    // недостаточно параметров
                    commandHandled = printBackSETResult(false, commandName, pStream);
                }

            } // DATETIME

            else
            if(!strcmp_P(commandName, SSID_COMMAND)) // записать настройки WiFi. Пример SET=SSID#ASUS#panasonic
            {
                if(cParser.argsCount() > 1)
                {
                      commandHandled = setSSID(commandName, cParser, pStream);  // записать настройки WiFi
					 // ESP.restart();
                }
                else
                {
                    // недостаточно параметров
                    commandHandled = printBackSETResult(false,commandName,pStream);
                }
            } // SSID_COMMAND

            else
            if (!strcmp_P(commandName, PASS_COMMAND)) // записать настройки WiFi
            {
                if (cParser.argsCount() > 1)
                {
                    commandHandled = setSSID(commandName, cParser, pStream);  // записать настройки WiFi
                     ESP.restart();
                }
                else
                {
                    // недостаточно параметров
                    commandHandled = printBackSETResult(false, commandName, pStream);
                }
            } // PASS_COMMAND







            //else
            //if(!strcmp_P(commandName,UPLOADFILE_COMMAND)) // загрузка файла
            //{
            //    if(cParser.argsCount() > 2)
            //    {
                  //  ////SwitchRS485MainHandler(false); // выключаем обработчик RS-485 по умолчанию
                  //  commandHandled = setUPLOADFILE(cParser, pStream);
                  //  ////SwitchRS485MainHandler(true); // включаем обработчик RS-485 по умолчанию
            //    }
            //    else
            //    {
            //        // недостаточно параметров
            //        commandHandled = printBackSETResult(false,commandName,pStream);
            //    }
            //} // UPLOADFILE_COMMAND                  

            ////    else
            ////    if(!strcmp_P(commandName, MOTORESOURCE_CURRENT_COMMAND)) // установка текущего моторесурса
            ////    {
            ////        // запросили установить текущий моторесурс SET=RES_CUR|0
            ////        if(cParser.argsCount() > 1)
            ////        {
            ////          commandHandled = setMOTORESOURCE_CURRENT(cParser, pStream);
            ////        }
            ////        else
            ////        {
            ////          // недостаточно параметров
            ////          commandHandled = printBackSETResult(false,commandName,pStream);
            ////        }
            ////    } // MOTORESOURCE_CURRENT_COMMAND               
            ////    else
            ////    if(!strcmp_P(commandName, MOTORESOURCE_MAX_COMMAND)) // установка максимального моторесурса
            ////    {
            ////        // запросили установить текущий моторесурс SET=RES_MAX|1000
            ////        if(cParser.argsCount() > 1)
            ////        {
            ////          commandHandled = setMOTORESOURCE_MAX(cParser, pStream);
            ////        }
            ////        else
            ////        {
            ////          // недостаточно параметров
            ////          commandHandled = printBackSETResult(false,commandName,pStream);
            ////        }
            ////    } // MOTORESOURCE_MAX_COMMAND               
            ////  
            ////  //TODO: тут разбор команды !!!
            ////  
        } // if(cParser.parse(command,true))

    } // SET COMMAND


    else if (command.startsWith(CORE_COMMAND_GET)) // команда на получение свойств
    {
        CommandParser cParser;

        if (cParser.parse(command, false)) // если команда разобрана, то
        {
            const char* commandName = cParser.getArg(0);

            if (!strcmp_P(commandName, DATETIME_COMMAND)) // получение даты/времени
            {
                commandHandled = getDATETIME(commandName, pStream);

            } // DATETIME_COMMAND

            else if (!strcmp_P(commandName, VERSION_COMMAND)) // получение версии ПО
            {
                commandHandled = getVER(pStream);
            }
            else if (!strcmp_P(commandName, VOLTAGE_AKK_COMMAND)) // получение напряжения на аккумуляторе
            {
                commandHandled = getVOLTAGEAKK(commandName, cParser, pStream);

            } // VOLTAGE_COMMAND  

             //   else
             //   if(!strcmp_P(commandName, UUID_COMMAND)) // получение уникального ID контроллера
             //   {
             //       commandHandled = getUUID(commandName,cParser,pStream);                    
             // 
             //   } // UUID_COMMAND  

             //   else
             //   if(!strcmp_P(commandName, SDTEST_COMMAND)) // тестирование SD
             //   {
             //       commandHandled = getSDTEST(commandName,cParser,pStream);                    
             //     
             //   } // SDTEST_COMMAND       

                else      
                if(!strcmp_P(commandName, FREERAM_COMMAND)) // получение кол-ва свободной оперативной памяти
                {
                  commandHandled = getFREERAM(commandName,pStream);
                } // FREERAM_COMMAND

             //   else
             //   if(!strcmp_P(commandName, LS_COMMAND)) // LS, получение списка файлов и папок
             //   {
             //       // запросили получить список файлов в папке, GET=LS|FolderName
                    ////SwitchRS485MainHandler(false); // выключаем обработчик RS-485 по умолчанию
             //       commandHandled = getLS(commandName,cParser,pStream);                    
                    ////SwitchRS485MainHandler(true); // включаем обработчик RS-485 по умолчанию
             //   } // LS        

             //   else
             //   if(!strcmp_P(commandName, FILE_COMMAND)) // FILE, получение файла
             //   {
             //       // запросили получить файл, GET=FILE|FilePath
                      //       //SwitchRS485MainHandler(false); // выключаем обработчик RS-485 по умолчанию
             //         commandHandled = getFILE(commandName,cParser,pStream);                    
                      //       //SwitchRS485MainHandler(true); // выключаем обработчик RS-485 по умолчанию
             //   } // LS      

             //   else
             //   if(!strcmp_P(commandName, FILESIZE_COMMAND)) // FILESIZE, получение размера файла
             //   {
             //       // запросили размер файла, GET=FILESIZE|FilePath
                      // //SwitchRS485MainHandler(false); // выключаем обработчик RS-485 по умолчанию
             //       commandHandled = getFILESIZE(commandName,cParser,pStream); 
                      // //SwitchRS485MainHandler(true); // выключаем обработчик RS-485 по умолчанию
             //   } // LS        

           //TODO: тут разбор команды !!!

        } // if(cParser.parse(command,false))

    } // GET COMMAND


    if (!commandHandled)
    {
        onUnknownCommand(command, pStream);
    }
}
//--------------------------------------------------------------------------------------------------------------------------------------

void CommandHandlerClass::onUnknownCommand(const String& command, Stream* outStream) // обработчик неизвестной команды
{
    outStream->print(CORE_COMMAND_ANSWER_ERROR);
    outStream->println(F("UNKNOWN COMMAND"));  
}


//--------------------------------------------------------------------------------------------------------------------------------------
bool CommandHandlerClass::getVOLTAGEAKK(const char* commandPassed, const CommandParser& parser, Stream* pStream) // получение напряжения на аккумуляторе
{
    if (parser.argsCount() < 1)
        return false;

    pStream->print(CORE_COMMAND_ANSWER_OK);
  
    pStream->print(commandPassed);
    pStream->print(" = ");
    int PowerAkk = 0;
    //for (int i = 0; i < 20; i++)
    //{
    //    PowerAkk += analogRead(POWER_BATTERY);
    //}
    //float Akk = PowerAkk / 20;
    //Akk = ((Akk * 3.3) / 4096) * 2.46;  // Напряжение питания  
    //pStream->print(Akk);
    //pStream->println("V");

	return true;
}

//--------------------------------------------------------------------------------------------------------------------------------------
bool CommandHandlerClass::getTIMEAKK(const char* commandPassed, const CommandParser& parser, Stream* pStream) // получение время работы аккумулятора
{
    if (parser.argsCount() < 1)
        return false;

    pStream->print(CORE_COMMAND_ANSWER_OK);
    pStream->print(CORE_COMMAND_PARAM_DELIMITER);
    pStream->print(commandPassed);
    pStream->print(CORE_COMMAND_PARAM_DELIMITER);

    int TimeAkk = 99;// Settings.GetTimeAkk();                                            // Получить время работы аккумулятора
  
    pStream->println(TimeAkk);

    return true;
}



//--------------------------------------------------------------------------------------------------------------------------------------
bool CommandHandlerClass::getVER(Stream* pStream) // получение версии ПО
{  

    pStream->print(CORE_COMMAND_ANSWER_OK);
    pStream->print(" ");

    pStream->print(F("GasAnalizer "));
    pStream->println(SOFTWARE_VERSION);

    return true;
}

//--------------------------------------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------------------------------------
bool CommandHandlerClass::getFREERAM(const char* commandPassed, Stream* pStream) // получение свободной оперативной памяти
{
  if(commandPassed)
  {
      pStream->print(CORE_COMMAND_ANSWER_OK);
      pStream->print(commandPassed);
      pStream->println(" ");    
  }

    pStream->print("Free heap size:  (bytes) ");
    pStream->println(ESP.getFreeHeap());
    pStream->print("Total heap size: (bytes) ");
    pStream->println(ESP.getHeapSize());
    pStream->print("Flash chip size: (bytes) ");
    pStream->println(ESP.getFlashChipSize());
    pStream->print("Free_heap_size:  (bytes) ");
    pStream->println(esp_get_free_heap_size());
    pStream->print("Minimum_free_heap_size: (bytes) ");
    pStream->println(esp_get_minimum_free_heap_size());
 
  return true;
    
}
//--------------------------------------------------------------------------------------------------------------------------------------
bool CommandHandlerClass::setDATETIME(const char* param) // установка даты/времени
{
    // разбираем параметр на составные части
    int8_t day = 0;
    int8_t month = 0;
    int16_t year = 0;
    int8_t hour = 0;
    int8_t minute = 0;
    int8_t second = 0;

    // буфер под промежуточные данные
    char workBuff[5] = {0};
    char* writePtr = workBuff;

    // извлекаем день
    const char* delim = strchr(param,'.');
    if(!delim || (delim - param > 4))
      return false;

    while(param < delim)
      *writePtr++ = *param++;
    *writePtr = 0;
    writePtr = workBuff;

    day = atoi(workBuff);

    param = delim+1; // перемещаемся на следующий компонент - месяц
    delim = strchr(param,'.');
    if(!delim || (delim - param > 4))
      return false;

    while(param < delim)
      *writePtr++ = *param++;
    *writePtr = 0;
    writePtr = workBuff;

    month = atoi(workBuff);

    param = delim+1; // перемещаемся на следующий компонент - год
    delim = strchr(param,' ');
    if(!delim || (delim - param > 4))
      return false;

    while(param < delim)
      *writePtr++ = *param++;
    *writePtr = 0;
    writePtr = workBuff;

    year = atoi(workBuff);

    param = delim+1; // перемещаемся на следующий компонент - час
    delim = strchr(param,':');
    if(!delim || (delim - param > 4))
      return false;

    while(param < delim)
      *writePtr++ = *param++;
    *writePtr = 0;
    writePtr = workBuff;

    hour = atoi(workBuff);

    param = delim+1; // перемещаемся на следующий компонент - минута
    delim = strchr(param,':');
    if(!delim || (delim - param > 4))
      return false;

    while(param < delim)
      *writePtr++ = *param++;
    *writePtr = 0;
    writePtr = workBuff;

    minute = atoi(workBuff);

    param = delim+1; // перемещаемся на следующий компонент - секунда

    while(*param && writePtr < &(workBuff[4]))
      *writePtr++ = *param++;
    *writePtr = 0;

    second = atoi(workBuff);

    setCurrentDateTime(day, month, year,hour,minute,second);
    
  return true;
 
}
//--------------------------------------------------------------------------------------------------------------------------------------
void CommandHandlerClass::setCurrentDateTime(uint8_t day, uint8_t month, uint16_t year, uint8_t hour, uint8_t minute, uint8_t second) // установка даты/времени
{
   // вычисляем день недели
    int16_t dow;
    uint8_t mArr[12] = {6,2,2,5,0,3,5,1,4,6,2,4};
    dow = (year % 100);
    dow = dow*1.25;
    dow += day;
    dow += mArr[month-1];
    
    if (((year % 4)==0) && (month<3))
     dow -= 1;
     
    while (dow>7)
     dow -= 7;  
#ifdef USE_DS3231_REALTIME_CLOCK
    RTC_DS3231  rtc = Settings.GetClock();
    rtc.adjust(DateTime(year, month, day, hour, minute, second));
#endif
  // RealtimeClock.setTime(second, minute, hour, dow, day, month, year);
    
}
//--------------------------------------------------------------------------------------------------------------------------------------
bool CommandHandlerClass::getDATETIME(const char* commandPassed, Stream* pStream) // получение даты/времени
{
    // запросили получение времени/даты
#ifdef USE_DS3231_REALTIME_CLOCK
    RTC_DS3231  rtc = Settings.GetClock();
    RTCTime  now = rtc.getTime();
    
    // получаем компоненты даты в виде строк
    String dateStr = rtc.getDateStr(now);
    String timeStr = rtc.getTimeStr(now);
 
    pStream->print(CORE_COMMAND_ANSWER_OK);
    //pStream->print(commandPassed);
    pStream->print("DATE ");
    pStream->print(dateStr);
    pStream->print(" TIME ");
    pStream->println(timeStr);

    return true;
#endif

}
//--------------------------------------------------------------------------------------------------------------------------------------
bool CommandHandlerClass::printBackSETResult(bool isOK, const char* command, Stream* pStream) // печать ответа на команду
{
  if(isOK)
    pStream->print(CORE_COMMAND_ANSWER_OK);
  else
    pStream->print(CORE_COMMAND_ANSWER_ERROR);

  pStream->print(command);
  pStream->print(" ");

  if (isOK)
  {

      //RTC_DS3231  rtc = Settings.GetClock();
      //RTCTime  now = rtc.getTime();

      //// получаем компоненты даты в виде строк
      //String dateStr = rtc.getDateStr(now);
      //String timeStr = rtc.getTimeStr(now);

      //pStream->print(CORE_COMMAND_ANSWER_OK);
      ////pStream->print(commandPassed);
      //pStream->print("DATE ");
      //pStream->print(dateStr);
      //pStream->print(" TIME ");
      //pStream->println(timeStr);
       pStream->println(F("OK"));
  }
 
  else
    pStream->println(F("BAD_PARAMS"));

  return true;
}

bool CommandHandlerClass::getSSID(const char* commandPassed, const CommandParser& parser, Stream* pStream) // получение настроек WiFi
{
    if (parser.argsCount() < 1)
        return false;



    pStream->print(CORE_COMMAND_ANSWER_OK);

    pStream->print(commandPassed);
    pStream->print(" = ");
    //int PowerAkk = 0;
    //for (int i = 0; i < 20; i++)
    //{
    //    PowerAkk += analogRead(POWER_BATTERY);
    //}
    //float Akk = PowerAkk / 20;
    //Akk = ((Akk * 3.3) / 4096) * 2.46;  // Напряжение питания  
    //pStream->print(Akk);
    //pStream->println("V");

    return true;
}

bool CommandHandlerClass::setSSID(const char* commandPassed, CommandParser& parser, Stream* pStream) // записать настройки WiFi
{
    if (parser.argsCount() < 1)
        return false;

    if (parser.argsCount() > 2)
    {
 
       // int shouldConnectToRouter = atoi(parser.getArg(1));
        String routerID = parser.getArg(1);
        String routerPassword = parser.getArg(2);
 /*       String stationID = parser.getArg(4);
        String stationPassword = parser.getArg(5);*/


	
		char ssid_array[20] = "";
		ssid_array[routerID.length() + 1] = '\0';
		strncpy(ssid_array, routerID.c_str(), routerID.length() + 1);       // Преобразование принятую строку String в массив char для последующей обработки

       // Settings.SetRouterSSID(ssid_array);
        //preferences.putString(ssid, ssid_array);

	/*	memCom.write(ROUTER_ID_EEPROM_ADDR, CORRECT_DATA);
		memCom.writeChars(ROUTER_ID_EEPROM_ADDR + 2, ssid_array, strlen(ssid_array)+1);*/

		ssid_array[routerPassword.length() + 1] = '\0';
		strncpy(ssid_array, routerPassword.c_str(), routerPassword.length() + 1);       // Преобразование принятую строку String в массив char для последующей обработки

        Settings.SetRouterPassword(ssid_array);
       // preferences.putString(password, ssid_array);

		//memCom.write(ROUTER_PASSWORD_EEPROM_ADDR, CORRECT_DATA);
		//memCom.writeChars(ROUTER_PASSWORD_EEPROM_ADDR + 2, ssid_array, strlen(ssid_array) + 1);

	
        pStream->print(CORE_COMMAND_ANSWER_OK);
        pStream->println(commandPassed);

        ////shouldConnectToRouter = Settings.GetWiFiState();
    /*    String routerID1 = Settings.GetRouterID();
        String routerPassword1 = Settings.GetRouterPassword();*/
        ////String stationID1 = Settings.GetStationID();
        ////String stationPassword1 = Settings.GetStationPassword();

        pStream->print("SSID = ");
        pStream->println(routerID);

        pStream->print("Password = ");
        pStream->println(routerPassword);
      
  
        //pStream->print("SetStationID = ");
        //pStream->println(stationID);

        //pStream->print("SetStationPassword = ");
        //pStream->println(stationPassword);

        pStream->println("");

	    return true;
    }
}

//--------------------------------------------------------------------------------------------------------------------------------------
bool CommandHandlerClass::setTXT(const char* commandPassed, CommandParser& parser, Stream* pStream, String textString) // Программа приема текстового сообщения
{
  if(commandPassed)
  {
     /* pStream->print(CORE_COMMAND_ANSWER_OK);
      pStream->print(commandPassed);
      pStream->print(CORE_COMMAND_PARAM_DELIMITER); */   

      /* 0) Преобразовать строку
      *  1) получить текущий адрес сообщения.
      *  2) записать признак нового сообщения ("1")
      * 2a) получить номер текущего сообщения 
      *  3) Записать номер сообщения (адрес сообщения + 1)
      * 4a) увеличить номер
      *  4) сохранить сообщение не более 160 символов (адрес сообщения + 10)
      *  5) получить количество сообщений
      *  6) увеличить количество сообщений на "1"
      *  7) Сохранить количество сообщений
  

        Параметры блока записи сообщения в энергонезависимую память
        Под сообщение отведено 120 байт плюс 20 байт для различных флагов
        1 байт - флаг наличия сообщения. "1" - есть новое сообщение, иначе нет
        2 байт - флаг операции прочтения сообщения. "1" новое сообщение прочтено, иначе нет
        3 байт - флаг передачи подтверждения "ОК". "1" подтверждение прочтения передано, иначе нет
        4 байт - порядковый номер сообщения.
        9 - 18 байт - резерв
        19 - 159 отведено под сообщение(4 строки по 20 символов).Максимальное количество сообщений - 99.
          */

	  const char* NumberMessage1 = parser.getArg(0);
      String timeString = parser.getArg(2);    // Получить текстовую строку из сообщения

      int16_t NumberMessage = atoi(parser.getArg(0));

      DBG("NumberMessage ");
      DBGLN(NumberMessage);                                            // получить номер сообщения из принятой строки

      DBG("TimeMessage ");
      DBGLN(timeString);

     // char msg[Number_of_bytes_block] = "";                            // Массив для приема текстовых сообщений
     // char time_msg[Number_of_bytes_time] = "";                        // Массив для приема времени текстовых сообщений

     // strncpy(msg, textString.c_str(), textString.length() + 1);       // Преобразование принятую строку String в массив char для последующей обработки
     // strncpy(time_msg, timeString.c_str(), timeString.length() + 1);  // Преобразование принятую строку String в массив char для последующей обработки

     //// NumberMessage
     // char msgOK_Trecker[8] = "#";                                     // Формирование строки для ответного сообщения 
     // strcat(msgOK_Trecker, NumberMessage1);                           // Добавили в ответ номер ответного сообщения
     // SERIAL_TRACKER.println(msgOK_Trecker);                           // Передать подтерждение о получении сообщения в треккер

      /*Процедура записи сообщения в память*/

      //
      ////***************** Пока не нужно считать общее количество сообщений **********************************
      //uint8_t mess_count = Settings.getAllCoutMessage();               // Получить общее количество сообщений
      //mess_count++;   
      //if (mess_count > 250)
      //{
      //    mess_count = 0;                                              // При превышении общего количества сообщений 250, сбросить в "0"
      //}
      //// 
      //Settings.setAllCoutMessage(mess_count);                          // Сохранить новое значение количества сообщений
      ////****************************************************************************************************

   //   uint8_t not_read = Settings.getCoutNotReadMessage();             // получить показания счетчика не прочитанного количества сообщений
   //   not_read++;
   //   Settings.setCoutNotReadMessage(not_read);                        // сохранить новое состояние счетчика не прочитанного количества сообщений 

	  //uint8_t  count_message = Settings.getCurrentCountMessage();      // получить номер текущего сообщения 
   //       count_message++;
   //       if (count_message > Max_Count_Block_Message)
   //       {
   //           count_message = 1;
   //       }
   //
   //   Settings.setCurrentCountMessage(count_message);                  // записать новый номер текущего сообщения 

   //   unsigned int cur_adr = (count_message * Number_of_bytes_block) + Start_Block_Text_ADDRESS - Number_of_bytes_block; // получить  адрес текущего сообщения.
   //   DBGLN(count_message);                                            //
   //   delay(50);

   //   //Сохранить сообщение во внешнюю память
   //   MemWrite(cur_adr,1);                                                         // 1 байт - флаг наличия сообщения. "1" - есть новое сообщение, иначе нет
   //   MemWrite(cur_adr + addr_read_NOT_TRANSMITTED, MESSAGE_NOT_CONFIRMED);        // 1 байт - флаг передачи подтверждения "ОК". "MESSAGE_NOT_CONFIRMED" подтверждение  прочтения НЕ ПЕРЕДАНО      MemWrite(cur_adr + addr_number_this_message, NumberMessage);                             // Записать номер данного сообщения
   //   MemWrite(cur_adr + addr_number_this_message, NumberMessage);                 // Сохранить номер сообщения из центра
   //   MemWriteChars(cur_adr + addr_time_this_message, time_msg, sizeof(time_msg)); // Записать время соббщения в память по текущему адресу 
   //   MemWriteChars(cur_adr + addr_current_message, msg, sizeof(msg));             // Записать соббщение в память по текущему адресу 

//#ifdef _DEBUG_TXT
//      uint8_t  conf_OK = MemRead(cur_adr + addr_read_NOT_TRANSMITTED);             // 1 байт - флаг передачи подтверждения "ОК". "1" подтверждение прочтения НЕ ПЕРЕДАНО
//      DBG("!!conf_OK ");
//      DBGLN(conf_OK);
//      MemReadChars(cur_adr + addr_current_message, msg, sizeof(msg));              // Проверить записанное сообщение.
//#endif
      //Settings.setNewMessageFlag(true);                                            // Сохранить флаг нового сообщения
      //Settings.setMessageDiodeBlink(true);                                         // Разрешить мигание светодиода"Сообщение" (красный)
      //Settings.displayBacklight(true);                                             // Включить подсветку дисплея
   }

  //pStream->println("Test TXT");

  return true;
    
}
//--------------------------------------------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------------------------------------------

bool  CommandHandlerClass::clearMemory(const char* commandPassed, CommandParser& parser)      // Стереть всю память
{

    if (parser.argsCount() < 1)
        return false;

 	DBGLN(F("Start EEPROM clearance..."));

    //MemClear();                                                 // Стереть всю память
    //ClearMessage();                                               // Стереть все сообщения

    DBGLN(F("EEPROM clearance END"));

    ESP32_SoftReset();
}


//--------------------------------------------------------------------------------------------------------------------------------------
/*
 * Функциональная функция: функция мягкого сброса STM32
 */
void  CommandHandlerClass::ESP32_SoftReset(void)
{
    ESP.restart();
}
// --------------------------------------------------------------------------------------------------------------------------------------
