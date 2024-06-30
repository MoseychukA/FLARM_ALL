#include "CoreCommandBuffer.h"
#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>
#include "SettingsMail.h"
#include "Configuration_ESP32.h"
#include <Stream.h>
#include "TinyVector.h"
#include "TFTModule.h" 
#include "Memory.h"               // Работа с энергонезависимой памятью

#include "NMEA.h"
#include "GNSS.h"



//--------------------------------------------------------------------------------------------------------------------------------------
// отправить команду на контроллер дисплея. список поддерживаемых команд
//--------------------------------------------------------------------------------------------------------------------------------------

const char TIME_AKK_COMMAND[]     PROGMEM = "TIMEAKK";  // Установить/получить время работы аккумулятора в часах. Пример #1#SET#TIMEAKK#10 или #1#GET#TIMEAKK
const char VOLTAGE_AKK_COMMAND[]  PROGMEM = "AKK";      // получить напряжение аккумулятора в вольтах. Пример #1#GET#AKK
const char VERSION_COMMAND[]      PROGMEM = "VER";      // отдать информацию о версии.                 Пример #1#GET#VER
const char TEXT_COMMAND[]         PROGMEM = "TXT";      // отправить текст на треккер.                 Пример #1#SET#TXT#далее следует строка текста
const char CLEAR_COMMAND[]        PROGMEM = "CLEAR";    // Очистить внешнюю память и записать начальные настройки. Пример #1#SET#CLEAR
const char CLEAR_MAIL[]           PROGMEM = "CLEARMAIL";// Очистить почту. Пример #1#SET#CLEARMAIL
const char SET_FLY[]              PROGMEM = "FLY";      // Установить параметры стороннего самолета
const char TIME_TFT_COMMAND[]     PROGMEM = "TIMETFT";  // Установить/получить время подсветки дисплея. Пример #1#SET#TIMETFT#10 или #1#GET#TIMELCD
const char PIN_COMMAND[]          PROGMEM = "PIN";      // установить уровень на пине.                 Пример #1#SET#PIN#29#0 - включить светодиод "Сообщение" или получить состояние #1#GET#PIN#29


//--------------------------------------------------------------------------------------------------------------------------------------
//
//typedef struct _MailTime {
//    unsigned int year; /**< Years                    - [1900, 2089]                 */
//    unsigned int mon;  /**< Months                   - [   1,   12]                 */
//    unsigned int day;  /**< Day of the month         - [   1,   31]                 */
//    unsigned int hour; /**< Hours since midnight     - [   0,   23]                 */
//    unsigned int min;  /**< Minutes after the hour   - [   0,   59]                 */
//    unsigned int sec;  /**< Seconds after the minute - [   0,   60] (1 leap second) */
//    unsigned int hsec; /**< Hundredth part of second - [   0,   99]                 */
//    bool isValid;
//} MailTime;
//
//
//typedef struct _TimeInfo {
//
//    MailTime       utc;        /**< UTC of the position data                                        */
//                          
//} TimeInfo;


//--------------------------------------------------------------------------------------------------------------------------------------
CoreCommandBuffer Commands(&SERIAL_TRACKER)/*, Commands_DEBUG(&DEBUG_Serial)*/;
//--------------------------------------------------------------------------------------------------------------------------------------

CoreCommandBuffer::CoreCommandBuffer(Stream* s) : pStream(s) // конструктор
{
    strBuff = new String();
    strBuff->reserve(BUFFER_SIZE+20);
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
                //clearCommand();
                return true;
            } // if
        } // while

        SettingsMail.setNumber_from_Message(strBuff->length());
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
  if(arguments.size() && idx < arguments.size())
    return arguments[idx];

  return NULL;
}
//--------------------------------------------------------------------------------------------------------------------------------------
bool CommandParser::parse(const String& command, bool isSetCommand) // разбирает входящую строку на параметры
{
  clear();
    // разбиваем на аргументы
  
    const char* startPtr = command.c_str() + strlen_P(isSetCommand ? (const char* )CORE_COMMAND_SET : (const char*) CORE_COMMAND_GET);
    size_t len = 0;

    while(*startPtr)
    {
      const char* delimPtr = strchr(startPtr,CORE_COMMAND_PARAM_DELIMITER);  // Ищет символ CORE_COMMAND_PARAM_DELIMITER в строке delimPtr и возвращает указатель на первое совпадение.
            
      if(!delimPtr)
      {
        len = strlen(startPtr);
        char* newArg = new char[len + 1];
        memset(newArg,0,len+1);
        strncpy(newArg,startPtr,len);
        arguments.push_back(newArg);        

        return arguments.size();
      } // if(!delimPtr)

      size_t len = delimPtr - startPtr;

     
      char* newArg = new char[len + 1];
      memset(newArg,0,len+1);
      strncpy(newArg,startPtr,len);
      arguments.push_back(newArg);

      startPtr = delimPtr + 1;
      
    } // while      

  return arguments.size();
    
}

bool CommandParser::parseTXT(const String& command, bool isSetCommand) // разбирает входящую строку на параметры. Команда ввода текста после знака "#"
{
    clear();
    // разбиваем на аргументы
    const char* startPtr = command.c_str() + strlen_P((const char*) CORE_TEXT_TFT);
    size_t len = 0;
	//DBG("startPtr - ");
	//DBGLN(startPtr);
    while (*startPtr)
    {
        const char* delimPtr = strchr(startPtr, CORE_COMMAND_PARAM_DELIMITERTXT);  // Ищет символ CORE_COMMAND_PARAM_DELIMITER в строке delimPtr и возвращает указатель на первое совпадение.

        if (!delimPtr)
        {
            len = strlen(startPtr);
            char* newArg = new char[len + 1];
            memset(newArg, 0, len + 1);
            strncpy(newArg, startPtr, len);
            arguments.push_back(newArg);
			//DBG("arguments.size()1 ");
			//DBGLN(arguments.size());
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

void CommandHandlerClass::setup()
{
    //if (!MAIL_DATA.begin(0xA00)) //2560
    //{
    //    Serial.println("Failed to initialise MAIL_DATA");
    //    Serial.println("Restarting...");
    //    delay(1000);
    //    // ESP.restart();
    //}
    //else
    //{
    //   // Serial.println("Initialise MAIL_DATA");
    //}
}

//--------------------------------------------------------------------------------------------------------------------------------------
void CommandHandlerClass::handleCommands() // обработчик входящих сообщений в loop
{
  if(Commands.hasCommand())   // Пришло новое сообщение 
  {    

    String command = Commands.getCommand();  // Скопировать буфер с сообщением 

	if (command.startsWith(CORE_TEXT_TFT)) // Определение типа принятой команды. Остальные игнорируются. Проверяет, начинается ли command строкой CORE_TEXT_TFT. В случае совпадения возвращает true
	{
		Stream* pStream = Commands.getStream();   // Копировать в pStream принятую строку. В строке находится принятый текст из USART
		processCommand(command, pStream);         // Вызываем обработку принятого сообщения
	}
    else if (command.startsWith(BUFFER_REQUEST_ER)) // буфер трекера не пустой
    {
        bool set_empty = false;
        SettingsMail.set_empty_buffer_request(set_empty); //Buffer Full полный
        //DBGLN("Buffer Full полный");
    }
    else if (command.startsWith(BUFFER_REQUEST_OK)) // буфер трекера пустой
    {
        bool set_empty = true;
        SettingsMail.set_empty_buffer_request(set_empty);
        //DBGLN("Buffer Ok");
    }

    Commands.clearCommand(); // Строка без команды в начале текста, очищаем буфер команд
  
  } // if(Commands.hasCommand())  
}


void CommandHandlerClass::processCommand(const String& command, Stream* pStream) // выполнение входящей команды
{
    bool commandHandled = false;
 
	if (command.startsWith(CORE_TEXT_TFT))            // Если команда CORE_TEXT_TFT
    {
        CommandParser cParser;                        // Разбираем строку на составляющие   

	    if (cParser.parseTXT(command, false))         // если команда разобрана, то
        {
		    const char* commandName = cParser.getArg(0);

            String textString = cParser.getArg(1);    // Получить текстовую строку из сообщения
	
			if (textString.startsWith(CORE_COMMAND_GET) || textString.startsWith(CORE_COMMAND_SET)) // Определение типа принятой команды. Остальные игнорируются
			{
				if (textString.startsWith(CORE_COMMAND_SET))          // Если команда "SET=" продолжить разбор
				{
  
					const char* commandName = cParser.getArg(2);      // в первом аргументе тип команды.

                    if (!strcmp_P(commandName, CLEAR_MAIL))           // Стереть всю память
					{
    					commandHandled = clearMail(commandName, cParser, pStream);
					}  

                    else if (!strcmp_P(commandName, SET_FLY))              // Установить параметры стороннего самолета
                    {
                        if (cParser.argsCount() > 1)
                        {

                            const char* paramPtr = cParser.getArg(3);
                            commandHandled = printBackSETResult(setFly(paramPtr), commandName, pStream);


                            //commandHandled = setFly(commandName, cParser, pStream);
                        }
                        else
                        {
                            // недостаточно параметров
                            commandHandled = printBackSETResult(false, commandName, pStream);
                        }

                    }




                    //if (!strcmp_P(commandName, CLEAR_COMMAND))        // Стереть всю память
                    //{
                    //    //!!commandHandled = clearMemory(commandName, cParser);
                    //} // 



		   //         if (!strcmp_P(commandName, TIME_AKK_COMMAND))    //  
		   //         {
			  //          // запросили установить время работы прибора от аккумулятора 
			  //          if (cParser.argsCount() > 3)
			  //          {
     //                      commandHandled = setTIMEAKK(commandName, cParser, pStream);
			  //          }
			  //          else
			  //          {
				 //           // недостаточно параметров
				 //           commandHandled = printBackSETResult(false, commandName, pStream);
			  //          }
		   //         } // TIMEAKK_COMMAND  // Установить время работы прибора от аккумулятора    
     //               if (!strcmp_P(commandName, TIME_TFT_COMMAND)) //  
     //               {
     //                   // запросили установить время работы дисплея
     //                   if (cParser.argsCount() > 3)
     //                   {
     //                       commandHandled = setTIMELCD(commandName, cParser, pStream);
     //                   }
     //                   else
     //                   {
     //                       // недостаточно параметров
     //                       commandHandled = printBackSETResult(false, commandName, pStream);
     //                   }
     //               } // TIMELCD_COMMAND  // Установить время работы дисплея

					////TODO: тут разбор команды !!!

				} // SET COMMAND

				else if (textString.startsWith(CORE_COMMAND_GET)) // команда на получение свойств
				{
				    const char* commandName = cParser.getArg(2);
                        
                    if (!strcmp_P(commandName, VERSION_COMMAND)) // получение версии ПО 
				    {
				        commandHandled = getVER(pStream);
				    }
				    else if (!strcmp_P(commandName, VOLTAGE_AKK_COMMAND)) // получение напряжения на аккумуляторе
					{
					  	commandHandled = getVOLTAGEAKK(commandName, cParser, pStream);

					} // VOLTAGE_COMMAND      
                    else if (!strcmp_P(commandName, TIME_AKK_COMMAND)) // получить время работы аккумулятора в часах. Пример #1#GET#TIMEAKK
                    {
                        commandHandled = getTIMEAKK(commandName, cParser, pStream);

                    } // TIMEAKK      
 				    //TODO: тут разбор команды !!!
				}// GET COMMAND
			}
			else  // Получить текстовую команду
			{
				commandHandled = setTXT(commandName, cParser, pStream, textString);
			}
        }
    }
 
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
    pStream->print(CORE_COMMAND_PARAM_DELIMITER);
    pStream->print(commandPassed);
    pStream->print(CORE_COMMAND_PARAM_DELIMITER);

    //float PowerAkk = SettingsMail.getPowerVoltageAkk(POWER_BATTERY);         // Контроль источника питания +3.7в
    //float Akk = PowerAkk / 100.0;
    //pStream->println(Akk);

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

    //int TimeAkk = SettingsMail.GetTimeAkk();                                            // Получить время работы аккумулятора
  
    //pStream->println(TimeAkk);

    return true;
}

bool CommandHandlerClass::setTIMEAKK(const char* commandPassed, CommandParser& parser, Stream* pStream)      // установить время работы аккумулятора в часах. Пример #1#GET#TIMEAKK#8
{
    if (parser.argsCount() < 4)
        return false;
    
    int16_t TimeAkk = atoi(parser.getArg(3));
 
    //SettingsMail.SetTimeAkk(TimeAkk); // Записать  время работы аккумулятора
   
    //pStream->print(CORE_COMMAND_ANSWER_OK);

    //pStream->print(parser.getArg(2));
    //pStream->print(CORE_COMMAND_PARAM_DELIMITER);
    //pStream->print(TimeAkk);

    return true;
}


//--------------------------------------------------------------------------------------------------------------------------------------
bool CommandHandlerClass::getTIMELCD(const char* commandPassed, const CommandParser& parser, Stream* pStream) // получение времени работы дисплея
{
    if (parser.argsCount() < 1)
        return false;

    pStream->print(CORE_COMMAND_ANSWER_OK);
    pStream->print(CORE_COMMAND_PARAM_DELIMITER);
    pStream->print(commandPassed);
    pStream->print(CORE_COMMAND_PARAM_DELIMITER);

    //int TimeLCD = SettingsMail.GetTimeLedLCD();                                            // Получить время работы дисплея

    //pStream->println(TimeLCD);

    return true;
}

bool CommandHandlerClass::setTIMELCD(const char* commandPassed, CommandParser& parser, Stream* pStream)      // установить время работы дисплея в секундах. Пример #1#SET#TIMELCD#8
{
    if (parser.argsCount() < 4)
        return false;

    int16_t TimeLCD = atoi(parser.getArg(3));

    //SettingsMail.SetTimeLedLCD(TimeLCD); // Записать  время работы дисплея

    //pStream->print(CORE_COMMAND_ANSWER_OK);

    pStream->print(parser.getArg(2));
    pStream->print(CORE_COMMAND_PARAM_DELIMITER);
    pStream->print(TimeLCD);

    return true;
}


//--------------------------------------------------------------------------------------------------------------------------------------
bool CommandHandlerClass::getVER(Stream* pStream) // получение версии ПО
{  
    char str_ver[32];
    String ver = MainScreen->getVer();
    ver.toCharArray(str_ver, 32);

    pStream->print(CORE_COMMAND_ANSWER_OK);
    pStream->print(CORE_COMMAND_PARAM_DELIMITER);

    pStream->print(F("FlyRf "));
    pStream->println(str_ver);

    return true;
}

//--------------------------------------------------------------------------------------------------------------------------------------
bool CommandHandlerClass::printBackSETResult(bool isOK, const char* command, Stream* pStream) // печать ответа на команду
{
  if(isOK)
    pStream->print(CORE_COMMAND_ANSWER_OK);
  else
    pStream->print(CORE_COMMAND_ANSWER_ERROR);

  pStream->print(command);
  pStream->print(CORE_COMMAND_PARAM_DELIMITER);

  if(isOK)
    pStream->println(F("OK"));
  else
    pStream->println(F("BAD_PARAMS"));

  return true;
}

//--------------------------------------------------------------------------------------------------------------------------------
//void MemWriteChars(unsigned int address, char* data, int length)
//{
//    MAIL_DATA.writeChars(address, data, length);
//}





//--------------------------------------------------------------------------------------------------------------------------------------
bool CommandHandlerClass::setTXT(const char* commandPassed, CommandParser& parser, Stream* pStream, String textString) // Программа приема текстового сообщения
{
    if (commandPassed)
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
             //**************************************************
        const char* Number_from_Message = parser.getArg(0);                  //  Получить текстовую строку номера сообщения Number_from_Message
        String timeString = parser.getArg(2);                                // Получить текстовую строку из сообщения 

        int8_t NumberMessage = atoi(parser.getArg(0));                       // Получить номер сообщения в виде int                

        char msg_tmp_all[128] = "";
        char msg_tmp[Number_of_bytes_block] = "";                            // Массив для приема текстовых сообщений
        char time_msg_tmp[Number_of_bytes_time] = "";                        // Массив для приема времени текстовых сообщений
       
        char msgOK_Trecker[4] = "#";                                         // Формирование строки для ответного сообщения 
        strcat(msgOK_Trecker, Number_from_Message);                          // Добавили в ответ номер ответного сообщения
        pStream->println(msgOK_Trecker);                                     // Передать подтерждение о получении сообщения в треккер

  /*      uint8_t num_from_message = atoi(parser.getArg(0));
        SettingsMail.setNumber_from_Message(num_from_message);  // Применить по другому как то
        */ 


        info.utc.hour = 0;
        info.utc.min = 0;

       // info.utc.isValid = gnss.time.isValid();

        if (gnss.time.isValid())
        {
            info.utc.hour = gnss.time.hour();
            info.utc.min = gnss.time.minute();
        }
        else
        {
            info.utc.hour = 10;
            info.utc.min = 10;
        }

  
        //if (textString.length() != 0)
        //{
        //    strncpy(msg_tmp, textString.c_str(), textString.length() + 1);       // Преобразование принятую строку String в массив char для последующей обработки
        //}

        if (timeString.length() != 0)
        {
            int index_hour = timeString.indexOf(':');
            if (index_hour != -1)
            {
                String hour_tmp = timeString.substring(0, index_hour);
                int hour_msg = hour_tmp.toInt();
                String min_tmp = timeString.substring(index_hour+1);
                int min_msg = min_tmp.toInt();

              //Serial.print("hour - ");
              //Serial.println(hour_msg);
              //Serial.print("min - ");
              //Serial.println(min_msg);

            }

           strncpy(time_msg_tmp, timeString.c_str(), timeString.length() + 1);  // Преобразование принятую строку c времени, String в массив char для последующей обработки
        }


        if (textString.length() != 0)
        {

            strncpy(msg_tmp, textString.c_str(), textString.length() + 1);       // Преобразование принятую строку String в массив char для последующей обработки


            if (timeString.length() != 0)
            {

                strncpy(time_msg_tmp, timeString.c_str(), timeString.length() + 1);  // Преобразование принятую строку c времени, String в массив char для последующей обработки
                strcat(msg_tmp_all, time_msg_tmp);                                   // Записать в строку время отправки сообщения
                strcat(msg_tmp_all, " ");                                            // Добавить пробел между временем и сообщением
                strcat(msg_tmp_all, msg_tmp);                                        // Записать в строку само сообщение
            }
            else
            {
                //strcat(msg_tmp_all, Number_from_Message);
                //strcat(msg_tmp_all, " ");
                strcat(msg_tmp_all, msg_tmp);
            }
        }

    
        /*Процедура записи сообщения в память*/


      //***********************************************************
//!!++++++++++++++++++++++++++++++++++++++ Переделать ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


      uint8_t not_read = SettingsMail.getCoutNotReadMessage();             // получить показания счетчика не подтвержденного количества сообщений
      not_read++;
      SettingsMail.setCoutNotReadMessage(not_read);                        // сохранить новое состояние счетчика не подтвержденного количества сообщений 

	  uint8_t  count_message = SettingsMail.getCurrentCountMessage();      // получить номер текущего сообщения 

      count_message++;
      if (count_message > Max_Count_Block_Message)
      {
          count_message = 1;
      } 

      SettingsMail.setCurrentCountMessage(count_message);                    // записать новый номер текущего сообщения 

      unsigned int cur_adr = (count_message * Number_of_bytes_block) + Start_Block_Text_ADDRESS;                      // получить  адрес текущего сообщения.
     // pStream->print(count_message);                                                       //
      delay(50);

      //Сохранить сообщение во внешнюю память
      MemWrite(cur_adr, 1);                                                                // 1 байт - флаг наличия сообщения. "1" - есть новое сообщение, иначе нет
      MemWrite(cur_adr + addr_read_NOT_TRANSMITTED, MESSAGE_CONFIRMED);                    // 1 байт - флаг передачи подтверждения "ОК". "MESSAGE_CONFIRMED" подтверждение  прочтения НЕ ПЕРЕДАНО
      MemWrite(cur_adr + addr_number_this_message, NumberMessage);                         // Сохранить номер сообщения из центра
      MemWriteChars(cur_adr + addr_time_this_message, time_msg_tmp, sizeof(time_msg_tmp)); // Записать время соббщения в память по текущему адресу 
      MemWriteChars(cur_adr + addr_current_message, msg_tmp_all, sizeof(msg_tmp_all));     // Записать соббщение в память по текущему адресу 
      MemCommit();
      SettingsMail.setNewMessageFlag(true);                                                 // Установить флаг нового сообщения. Программа извещена и приступила к обработке нового сообщения.
    }

   return true;
    
}

//--------------------------------------------------------------------------------------------------------------------------------------
//
//bool CommandHandlerClass::setCount(const char* commandPassed, CommandParser& parser, Stream* pStream) // Установить новое значение текущего счетчика сообщений
//{
//   
//        if (parser.argsCount() < 1)
//            return false;
//
//        byte pinNumber = atoi(parser.getArg(2));
//        Settings.setCurrentCountMessage(pinNumber);               // записать новый номер текущего сообщения 
//		//Settings.setAllCoutMessage(0);                            // Сбросить счетчик общего количества сообщений
//		Settings.setCoutNotReadMessage(0);                          // Сбросить счетчик не прочитанного количества записей 
//
//
//  return true;
//
//}
//!!----------------------------------- Переделать ---------------------------------------------------------------------------------------------------
//
//bool  CommandHandlerClass::clearMemory(const char* commandPassed, CommandParser& parser)      // Стереть всю память
//{
//
//    if (parser.argsCount() < 1)
//        return false;
//
// 	DBGLN(F("Start EEPROM clearance..."));
//
//    //MemClear();                                                 // Стереть всю память
//    //ClearMessage();                                             // Стереть все сообщения
//
//    DBGLN(F("EEPROM clearance END"));
//
//    Stm32_SoftReset();
//}

bool  CommandHandlerClass::clearMail(const char* commandPassed, CommandParser& parser, Stream* pStream)      // Стереть всю почту
{

    if (parser.argsCount() < 1)
        return false;
    clear_mail = true; // Стереть всю почту
    pStream->print(CORE_COMMAND_PARAM_DELIMITER);
    pStream->print(CORE_COMMAND_ANSWER_OK);
   // pStream->print(parser.getArg(1));
    return true;
}

//bool CommandHandlerClass::setFly(const char* commandPassed, CommandParser& parser, Stream* pStream) // Установить параметры стороннего самолета
//bool CommandHandlerClass::setFly(const char* param) // Установить параметры стороннего самолета
//{
//   
//  /*      if (parser.argsCount() < 1)
//            return false;*/
//
//        /*#1#SET#FLY#адрес#Squawk#номер рейса#altitude#speed#course#vert_rate#latitude#longitude#aircraft_type*/
//        /*
//            uint32_t  addr;
//            uint8_t   addr_type;
//            float     latitude;
//            float     longitude;
//            float     altitude;
//            float     pressure_altitude;
//            float     course;    
//            float     speed;      // ground speed in knots 
//            uint8_t   aircraft_type;
//            char      flight[8];     // Flight number
//            int       vert_rate;     // Vertical rate.
//            int       Squawk;        // Squawk
//
//        */
//
//  //      uint32_t  addr = atoi(parser.getArg(3));
//  //     // Serial.println(addr/*, HEX*/);
//  //      int Squawk     = atoi(parser.getArg(4));
//  //     // Serial.println(Squawk);
//  //     // char  flight[8] = parser.getArg(4);
//  ////      byte pinNumber = atoi(parser.getArg(2));
//  ////      Settings.setCurrentCountMessage(pinNumber);                 // записать новый номер текущего сообщения 
//		//////Settings.setAllCoutMessage(0);                            // Сбросить счетчик общего количества сообщений
//		////Settings.setCoutNotReadMessage(0);                          // Сбросить счетчик не прочитанного количества записей 
//
//  //      pStream->print(CORE_COMMAND_PARAM_DELIMITER);
//  //      pStream->print(CORE_COMMAND_ANSWER_OK);
//  //      pStream->print(parser.getArg(0));
//
//
//  return true;
//
//}

bool CommandHandlerClass::setFly(const char* param)
{
    // разбираем параметр на составные части
     /*#1#SET#FLY#адрес#Squawk#номер рейса#altitude#speed#course#vert_rate#latitude#longitude#aircraft_type*/

    uint32_t  addr;
    int       Squawk;        // Squawk
    char      flight[8];     // Flight number
    float     altitude;
    float     speed;      // ground speed in knots 
    float     course;
    int       vert_rate;     // Vertical rate.
    float     latitude;
    float     longitude;
    uint8_t   aircraft_type;


    //
   // float     pressure_altitude;
    //   uint8_t   addr_type;


    //int value = strtoul("0x01",NULL,16);
   // Serial.println(value, HEX);

    int8_t day = 0;
    int8_t month = 0;
    int16_t year = 0;
    int8_t hour = 0;
    int8_t minute = 0;
    int8_t second = 0;

    // буфер под промежуточные данные
    char workBuff[10] = { 0 };
    char* writePtr = workBuff;

    // извлекаем день
    const char* delim = strchr(param, ',');
    if (!delim || (delim - param > 6))
        return false;

    while (param < delim)
        *writePtr++ = *param++;
    *writePtr = 0;
    writePtr = workBuff;

    addr = strtoul(workBuff, NULL, 16); // Извлеч адрес из строки

    Serial.println(addr, HEX);
 

    param = delim + 1; // перемещаемся на следующий компонент - месяц
    delim = strchr(param, ',');
    if (!delim || (delim - param > 4))
        return false;

    while (param < delim)
        *writePtr++ = *param++;
    *writePtr = 0;
    writePtr = workBuff;

    Squawk = atoi(workBuff);
    Serial.println(Squawk);


    param = delim + 1; // перемещаемся на следующий компонент - год
    delim = strchr(param, ',');
    if (!delim || (delim - param > 8))
        return false;

    while (param < delim)
        *writePtr++ = *param++;
    *writePtr = 0;
    writePtr = workBuff;

    strncpy(flight, workBuff, strlen(workBuff));  //strlenПреобразование принятую строку
   // strncpy(flight, workBuff, sizeof(workBuff));  //strlenПреобразование принятую строку

    Serial.println(flight);

    //year = atoi(workBuff);

    //param = delim + 1; // перемещаемся на следующий компонент - час
    //delim = strchr(param, ':');
    //if (!delim || (delim - param > 4))
    //    return false;

    //while (param < delim)
    //    *writePtr++ = *param++;
    //*writePtr = 0;
    //writePtr = workBuff;

    //hour = atoi(workBuff);

    //param = delim + 1; // перемещаемся на следующий компонент - минута
    //delim = strchr(param, ':');
    //if (!delim || (delim - param > 4))
    //    return false;

    //while (param < delim)
    //    *writePtr++ = *param++;
    //*writePtr = 0;
    //writePtr = workBuff;

    //minute = atoi(workBuff);

    //param = delim + 1; // перемещаемся на следующий компонент - секунда

    //while (*param && writePtr < &(workBuff[4]))
    //    *writePtr++ = *param++;
    //*writePtr = 0;

    //second = atoi(workBuff);

    //Serial.print(day);
    //Serial.print(",");
    //Serial.print(month);
    //Serial.print(",");
    //Serial.print(year);
    //Serial.print(",");
    //Serial.print(hour);
    //Serial.print(",");
    //Serial.print(minute);
    //Serial.print(",");
    //Serial.print(second);
    //Serial.println();

    //setCurrentDateTime(day, month, year, hour, minute, second);

    return true;

}



//bool CommandHandlerClass::setFly(const char* param)
//{
//    // разбираем параметр на составные части
//    int8_t day = 0;
//    int8_t month = 0;
//    int16_t year = 0;
//    int8_t hour = 0;
//    int8_t minute = 0;
//    int8_t second = 0;
//
//    // буфер под промежуточные данные
//    char workBuff[5] = { 0 };
//    char* writePtr = workBuff;
//
//    // извлекаем день
//    const char* delim = strchr(param, '.');
//    if (!delim || (delim - param > 4))
//        return false;
//
//    while (param < delim)
//        *writePtr++ = *param++;
//    *writePtr = 0;
//    writePtr = workBuff;
//
//    day = atoi(workBuff);
//
//    param = delim + 1; // перемещаемся на следующий компонент - месяц
//    delim = strchr(param, '.');
//    if (!delim || (delim - param > 4))
//        return false;
//
//    while (param < delim)
//        *writePtr++ = *param++;
//    *writePtr = 0;
//    writePtr = workBuff;
//
//    month = atoi(workBuff);
//
//    param = delim + 1; // перемещаемся на следующий компонент - год
//    delim = strchr(param, ' ');
//    if (!delim || (delim - param > 4))
//        return false;
//
//    while (param < delim)
//        *writePtr++ = *param++;
//    *writePtr = 0;
//    writePtr = workBuff;
//
//    year = atoi(workBuff);
//
//    param = delim + 1; // перемещаемся на следующий компонент - час
//    delim = strchr(param, ':');
//    if (!delim || (delim - param > 4))
//        return false;
//
//    while (param < delim)
//        *writePtr++ = *param++;
//    *writePtr = 0;
//    writePtr = workBuff;
//
//    hour = atoi(workBuff);
//
//    param = delim + 1; // перемещаемся на следующий компонент - минута
//    delim = strchr(param, ':');
//    if (!delim || (delim - param > 4))
//        return false;
//
//    while (param < delim)
//        *writePtr++ = *param++;
//    *writePtr = 0;
//    writePtr = workBuff;
//
//    minute = atoi(workBuff);
//
//    param = delim + 1; // перемещаемся на следующий компонент - секунда
//
//    while (*param && writePtr < &(workBuff[4]))
//        *writePtr++ = *param++;
//    *writePtr = 0;
//
//    second = atoi(workBuff);
//
//    Serial.print(day);
//    Serial.print(",");
//    Serial.print(month);
//    Serial.print(",");
//    Serial.print(year);
//    Serial.print(",");
//    Serial.print(hour);
//    Serial.print(",");
//    Serial.print(minute);
//    Serial.print(",");
//    Serial.print(second);
//    Serial.println();
//
//    //setCurrentDateTime(day, month, year, hour, minute, second);
//
//    return true;
//
//}






////--------------------------------------------------------------------------------------------------------------------------------------
///*
// * Функциональная функция: функция мягкого сброса STM32
// */
//void  CommandHandlerClass::Stm32_SoftReset(void)
//{
//    __set_FAULTMASK(1);// Запрещаем все маскируемые прерывания
//    NVIC_SystemReset();// Программный сброс
//}
//// --------------------------------------------------------------------------------------------------------------------------------------
