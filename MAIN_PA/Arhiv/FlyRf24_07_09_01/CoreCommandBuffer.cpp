#include "CoreCommandBuffer.h"
#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>
#include "SettingsMain.h"
#include "Configuration_ESP32.h"
#include <Stream.h>
#include "TinyVector.h"
//#include "TFTModule.h" 
#include "Memory.h"               // Работа с энергонезависимой памятью
#include "TrafficHelper.h"
#include "NMEA.h"
#include "GNSS.h"
#include "SoftRF.h"
#include <TimeLib.h>


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
//extern ufo_t fo_msg;

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

        SettingsMain.setNumber_from_Message(strBuff->length());
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
        SettingsMain.set_empty_buffer_request(set_empty); //Buffer Full полный
        //DBGLN("Buffer Full полный");
    }
    else if (command.startsWith(BUFFER_REQUEST_OK)) // буфер трекера пустой
    {
        bool set_empty = true;
        SettingsMain.set_empty_buffer_request(set_empty);
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

    //float PowerAkk = SettingsMain.getPowerVoltageAkk(POWER_BATTERY);         // Контроль источника питания +3.7в
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

    //int TimeAkk = SettingsMain.GetTimeAkk();                                            // Получить время работы аккумулятора
  
    //pStream->println(TimeAkk);

    return true;
}

bool CommandHandlerClass::setTIMEAKK(const char* commandPassed, CommandParser& parser, Stream* pStream)      // установить время работы аккумулятора в часах. Пример #1#GET#TIMEAKK#8
{
    if (parser.argsCount() < 4)
        return false;
    
    int16_t TimeAkk = atoi(parser.getArg(3));
 
    //SettingsMain.SetTimeAkk(TimeAkk); // Записать  время работы аккумулятора
   
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

    //int TimeLCD = SettingsMain.GetTimeLedLCD();                                            // Получить время работы дисплея

    //pStream->println(TimeLCD);

    return true;
}

bool CommandHandlerClass::setTIMELCD(const char* commandPassed, CommandParser& parser, Stream* pStream)      // установить время работы дисплея в секундах. Пример #1#SET#TIMELCD#8
{
    if (parser.argsCount() < 4)
        return false;

    int16_t TimeLCD = atoi(parser.getArg(3));

    //SettingsMain.SetTimeLedLCD(TimeLCD); // Записать  время работы дисплея

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
  /*  String ver = MainScreen->getVer();
    ver.toCharArray(str_ver, 32);

    pStream->print(CORE_COMMAND_ANSWER_OK);
    pStream->print(CORE_COMMAND_PARAM_DELIMITER);

    pStream->print(F("FlyRf "));
    pStream->println(str_ver);*/

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
        SettingsMain.setNumber_from_Message(num_from_message);  // Применить по другому как то
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


      uint8_t not_read = SettingsMain.getCoutNotReadMessage();             // получить показания счетчика не подтвержденного количества сообщений
      not_read++;
      SettingsMain.setCoutNotReadMessage(not_read);                        // сохранить новое состояние счетчика не подтвержденного количества сообщений 

	  uint8_t  count_message = SettingsMain.getCurrentCountMessage();      // получить номер текущего сообщения 

      count_message++;
      if (count_message > Max_Count_Block_Message)
      {
          count_message = 1;
      } 

      SettingsMain.setCurrentCountMessage(count_message);                    // записать новый номер текущего сообщения 

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
      SettingsMain.setNewMessageFlag(true);                                                 // Установить флаг нового сообщения. Программа извещена и приступила к обработке нового сообщения.
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


bool CommandHandlerClass::setFly(const char* param)
{
    // разбираем параметр на составные части
     /*#1#SET#FLY#адрес#Squawk#номер рейса#G#altitude#speed#course#vert_rate#latitude#longitude#aircraft_type*/

    uint32_t  addr;
    int       Squawk;         // Squawk
    char      flight[8];      // Flight number 
    float     altitude;
    float     pressure_altitude;
    float     speed;          // ground speed in knots 
    float     course;
    int       vert_rate;      // Vertical rate.
    float     latitude;
    float     longitude;
    uint8_t   aircraft_type;

    // буфер под промежуточные данные
    char workBuff[13] = { 0 };
    char* writePtr = workBuff;

    const char* delim = strchr(param, ',');  // Определяем первые данные
    if (!delim || (delim - param > 6))
        return false;

    while (param < delim)
        *writePtr++ = *param++;
    *writePtr = 0;
    writePtr = workBuff;

    addr = strtoul(writePtr, NULL, 16); // Извлечение адреса из строки

   // Serial.println(addr, HEX);
 
    param = delim + 1;                  // Извлечение Squawk из строки
    delim = strchr(param, ',');
    if (!delim || (delim - param > 4))
        return false;

    while (param < delim)
        *writePtr++ = *param++;
    *writePtr = 0;
    writePtr = workBuff;

    Squawk = atoi(writePtr);
   // Serial.println(Squawk);


    param = delim + 1;                   // Извлечение номера рейса из строки
    delim = strchr(param, ',');
    if (!delim || (delim - param > 8))
        return false;

    while (param < delim)
        *writePtr++ = *param++;
    *writePtr = 0;
    writePtr = workBuff;

    strncpy(flight, writePtr, strlen(writePtr));  //strlenПреобразование принятую строку
 //    Serial.println(flight);
 
    param = delim + 1;               // Вариант измерения высоты геоид
    delim = strchr(param, ',');
    if (!delim || (delim - param > 8))
        return false;

    while (param < delim)
        *writePtr++ = *param++;
    *writePtr = 0;
    writePtr = workBuff;

    altitude = atof(writePtr);
    // Serial.println(altitude,2);

    param = delim + 1;               // Извлечение altitude из строки
    delim = strchr(param, ',');
    if (!delim || (delim - param > 8))
        return false;

    while (param < delim)
        *writePtr++ = *param++;
    *writePtr = 0;
    writePtr = workBuff;

    pressure_altitude = atof(writePtr);
   // Serial.println(pressure_altitude,2);


    param = delim + 1;               // Извлечение speed из строки
    delim = strchr(param, ',');
    if (!delim || (delim - param > 6))
        return false;

    while (param < delim)
        *writePtr++ = *param++;
    *writePtr = 0;
    writePtr = workBuff;

    speed = atof(writePtr);
   // Serial.println(speed,1);

    param = delim + 1;               // Извлечение course из строки
    delim = strchr(param, ',');
    if (!delim || (delim - param > 6))
        return false;

    while (param < delim)
        *writePtr++ = *param++;
    *writePtr = 0;
    writePtr = workBuff;

    course = atof(writePtr);
   // Serial.println(course,2);

    param = delim + 1;               // Извлечение vert_rate из строки
    delim = strchr(param, ',');
    if (!delim || (delim - param > 4))
        return false;

    while (param < delim)
        *writePtr++ = *param++;
    *writePtr = 0;
    writePtr = workBuff;

    vert_rate = atoi(writePtr);
   // Serial.println(vert_rate);

    param = delim + 1;               // Извлечение latitude из строки
    delim = strchr(param, ',');
    if (!delim || (delim - param > 10))
        return false;

    while (param < delim)
        *writePtr++ = *param++;
    *writePtr = 0;
    writePtr = workBuff;

    latitude = atof(writePtr);
    Serial.println(latitude, 7);
   // Serial.println(writePtr);

    param = delim + 1;               // Извлечение longitude из строки
    delim = strchr(param, ',');
    if (!delim || (delim - param > 10))
        return false;

    while (param < delim)
        *writePtr++ = *param++;
    *writePtr = 0;
    writePtr = workBuff;

    longitude = atof(writePtr);
    Serial.println(longitude, 7);
   // Serial.println(writePtr);

    param = delim + 1; // перемещаемся на следующий компонент - секунда

    while (*param && writePtr < &(workBuff[2]))
        *writePtr++ = *param++;
    *writePtr = 0;

    aircraft_type = atoi(workBuff);
   // Serial.println(aircraft_type);

    fo.addr = addr;
    fo.Squawk = Squawk;
    memcpy((char*)fo.flight, flight, strlen(flight));
    fo.altitude = altitude;
    fo.pressure_altitude = pressure_altitude;
    fo.speed = speed;
    fo.course = course;
    fo.vert_rate = vert_rate;
    fo.latitude = latitude;
    fo.longitude = longitude;
    fo.aircraft_type = aircraft_type;
    fo.signal_source = 2;
    fo.timestamp = now(); // 

               // Расчет расстояния, курса и уровня опастности сближения нашего и стороннего самолета
    if (fo.latitude != 0 && fo.longitude != 0) // Расчет возможен если получены координаты нашего и стороннего самолета
    {
        Traffic_Update(&fo);   // 
    }

    // Остальные параметры записываем в базу 
    Traffic_Add(&fo);
   //#4#SET#FLY#3BC7C9,1234,SMD6406,1,100.0,810.0,20.0,50,56.033575,37.289899,2 // Пример строки
   //#4#SET#FLY#3BC7C7,1234,SMD6406,2,100.0,810.0,20.0,50,55.933575,37.589899,2
   //#4#SET#FLY#3BC7C1,1234,SMD6406,3,100.0,210.0,20.0,50,55.833575,37.189899,2
    
    return true;

}



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
