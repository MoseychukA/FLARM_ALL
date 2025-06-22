#include "CoreCommandBuffer.h"
#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>
#include "ServiceMain.h"
#include "Configuration_ESP32.h"
#include <Stream.h>
#include "TrafficHelper.h"
#include "NMEA.h"
#include "GNSS.h"
#include "SoftRF.h"
#include <TimeLib.h>
#include "ESP32RF.h"
#include "EEPROMRF.h"
#include <math.h>
#include "RF.h"
#include "SoC.h"
#include "ServiceMain.h"


//--------------------------------------------------------------------------------------------------------------------------------------
// отправить команду на контроллер дисплея. список поддерживаемых команд
//--------------------------------------------------------------------------------------------------------------------------------------
const char VERSION_COMMAND[]      PROGMEM = "VER";      // отдать информацию о версии.                 Пример #1#GET#VER
const char VERSION_COMMAND_M[]    PROGMEM = "ver";      // отдать информацию о версии.                 Пример #1#GET#VER
const char TEXT_COMMAND[]         PROGMEM = "TXT";      // отправить текст на треккер.                 Пример #1#SET#TXT#далее следует строка текста
const char TEXT_COMMAND_M[]       PROGMEM = "txt";      // отправить текст на треккер.                 Пример #1#SET#TXT#далее следует строка текста
const char CLEAR_MAIL[]           PROGMEM = "CLEARMAIL";// Очистить почту. Пример #1#SET#CLEARMAIL
const char SET_FLY[]              PROGMEM = "FLY";      // Установить параметры стороннего самолета
const char SET_FLY_M[]            PROGMEM = "fly";      // Установить параметры стороннего самолета
const char GET_FLY[]              PROGMEM = "FLY";      // Получить параметры нашего самолета
const char GET_FLY_M[]            PROGMEM = "fly";      // Установить параметры нашего самолета
const char GET_BASE[]             PROGMEM = "BASE";     // Получить параметры бызы сторонних самолетов
const char GET_BASE_M[]           PROGMEM = "base";     // Получить параметры бызы сторонних самолетов
const char SET_GSM[]              PROGMEM = "GSM";      // Установить параметры передачи данных по GSM
const char SET_GSM_M[]            PROGMEM = "gsm";      // Установить параметры передачи данных по GSM
const char SET_BT_OK[]            PROGMEM = "BTOK";     // Установить подтверждение прочтения сообщения по GSM
const char SET_BT_OK_M[]          PROGMEM = "btok";     // Установить подтверждение прочтения сообщения по GSM
//--------------------------------------------------------------------------------------------------------------------------------------
CoreCommandBuffer Commands(&SERIAL_TRACKER);// , Commands(&SERIAL_BLUETOOTH); //SoC->Bluetooth_ops
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
                pStream->print(CORE_COMMAND_ANSWER_ERROR);
                pStream->println(F("The text is very long. !Maximum 78 characters"));
                clearCommand();
                return true;
            } // if
        } // while

       // SettingsMain.setNumber_from_Message(strBuff->length());
       return true;   // Завершили чтение сообщения. Информация находится в strBuff
    }
    return false;     // Новой информации не поступало.
}


bool CoreCommandBuffer::bluetoothCommand()                // проверяет на наличие входящей команды из Bluetooth
{
    if (!(pStream && SoC->Bluetooth_ops->available()))
    {
        return false;
    }

    char ch;

    if (SoC->Bluetooth_ops->available())                       // Определить наличие символа в порту трекера
    {
        while (SoC->Bluetooth_ops->available() > 0)              // читаем данные во внутренний буфер
        {
            ch = (char)SoC->Bluetooth_ops->read();

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
                pStream->print(CORE_COMMAND_ANSWER_ERROR);
                pStream->println(F("The text is very long. !Maximum 72 characters")); 
                clearCommand();
                return true;
            } // if
        } // while

       // SettingsMain.setNumber_from_Message(strBuff->length());
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
  
}

//--------------------------------------------------------------------------------------------------------------------------------------
void CommandHandlerClass::handleCommands() // обработчик входящих сообщений в loop
{
  if(Commands.hasCommand() || Commands.bluetoothCommand())   // Пришло новое сообщение 
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
    }
    else if (command.startsWith(BUFFER_REQUEST_OK)) // буфер трекера пустой
    {
        bool set_empty = true;
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
	
			if (textString.startsWith(CORE_COMMAND_GET) || textString.startsWith(CORE_COMMAND_GET_M) || textString.startsWith(CORE_COMMAND_SET) || textString.startsWith(CORE_COMMAND_SET_M)) // Определение типа принятой команды. Остальные игнорируются
			{
				if (textString.startsWith(CORE_COMMAND_SET)|| textString.startsWith(CORE_COMMAND_SET_M))          // Если команда "SET=" продолжить разбор
				{
					const char* commandName = cParser.getArg(2);      // в первом аргументе тип команды.

                    if (!strcmp_P(commandName, CLEAR_MAIL))           // Стереть всю память
					{
    					commandHandled = clearMail(commandName, cParser, pStream);
					}  

                    else if (!strcmp_P(commandName, SET_FLY)|| !strcmp_P(commandName, SET_FLY_M))              // Установить параметры стороннего самолета
                    {
                        if (cParser.argsCount() > 1)
                        {

                            const char* paramPtr = cParser.getArg(3);
                            commandHandled = printBackSETResult(setFly(paramPtr), commandName, pStream);
                        }
                        else
                        {
                            // недостаточно параметров
                            commandHandled = printBackSETResult(false, commandName, pStream);
                        }

                    }
                    else if (!strcmp_P(commandName, SET_GSM)|| !strcmp_P(commandName, SET_GSM_M))              // Установить параметры передачи данных по GSM
                    {
                        if (cParser.argsCount() > 2)
                        {

                            const char* paramPtr = cParser.getArg(3);
                            commandHandled = setGSM(commandName, cParser, pStream);
                        }
                        else
                        {
                            // недостаточно параметров
                            commandHandled = printBackSETResult(false, commandName, pStream);
                        }
                    }
                    else if (!strcmp_P(commandName, SET_BT_OK) || !strcmp_P(commandName, SET_BT_OK_M))              // Установить подтверждение прочтения сообщения по GSM
                    {
                        if (cParser.argsCount() > 2)
                        {

                            const char* paramPtr = cParser.getArg(3);
                            commandHandled = setBTOK(commandName, cParser, pStream);
                        }
                        else
                        {
                            // недостаточно параметров
                            commandHandled = printBackSETResult(false, commandName, pStream);
                        }
                    }
                    else if (!strcmp_P(commandName, TEXT_COMMAND) || !strcmp_P(commandName, TEXT_COMMAND_M))              // Установить подтверждение прочтения сообщения по GSM
                    {
                        if (cParser.argsCount() > 2)
                        {
                            const char* paramPtr = cParser.getArg(1);
                            commandHandled = setTXT(commandName, cParser, pStream, textString);
                        }
                        else
                        {
                            // недостаточно параметров
                            commandHandled = printBackSETResult(false, commandName, pStream);
                        }
                    }

				} // SET COMMAND

				else if (textString.startsWith(CORE_COMMAND_GET)|| textString.startsWith(CORE_COMMAND_GET_M)) // команда на получение свойств
				{
				    const char* commandName = cParser.getArg(2);
                        
                    if (!strcmp_P(commandName, VERSION_COMMAND)||!strcmp_P(commandName, VERSION_COMMAND_M)) // получение версии ПО 
				    {
                        if (cParser.argsCount() > 1)
                        {
                            commandHandled = getVER(pStream);
                        }
                        else
                        {
                            // недостаточно параметров
                            commandHandled = printBackSETResult(false, commandName, pStream);
                        }
				    }
                    else if (!strcmp_P(commandName, GET_FLY)|| !strcmp_P(commandName, GET_FLY_M)) // получение данных из базы самолетов. 
                    {
                        if (cParser.argsCount() > 2)
                        {
                            commandHandled = getFly(pStream); // получение информацию с базы данных
                        }
                        else
                        {
                            // недостаточно параметров
                            commandHandled = printBackSETResult(false, commandName, pStream);
                        }
                    }
                    else if (!strcmp_P(commandName, GET_BASE) || !strcmp_P(commandName, GET_BASE_M)) // получение данных из базы самолетов. 
                    {
                        if (cParser.argsCount() > 2)
                        {
                            commandHandled = getBase(pStream); // получение информацию с базы данных
                        }
                        else
                        {
                            // недостаточно параметров
                            commandHandled = printBackSETResult(false, commandName, pStream);
                        }
                    }

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
bool CommandHandlerClass::getVER(Stream* pStream) // получение версии ПО
{  
    char str_ver[32];
    String ver = service.getVer();
    ver.toCharArray(str_ver, 32);

    pStream->print(CORE_COMMAND_ANSWER_OK);
    pStream->print(CORE_COMMAND_PARAM_DELIMITER);

    //pStream->print(F("FlyRf "));
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

int CommandHandlerClass::mb_strlen(char* source, int letter_n) 
{
    int i, k;
    int target = 0;
    unsigned char n;
    char m[2] = { '0', '\0' };
    k = strlen(source);
    i = 0;

    while (i < k) 
    {
        n = source[i]; i++;

        if (n >= 0xBF)
        {
            switch (n)
            {
                case 0xD0:
                {
                    n = source[i]; i++;
                    if (n == 0x81) { n = 0xA8; break; }
                    if (n >= 0x90 && n <= 0xBF) n = n + 0x2F;
                    break;
                }
                case 0xD1:
                {
                    n = source[i]; i++;
                    if (n == 0x91) { n = 0xB7; break; }
                    if (n >= 0x80 && n <= 0x8F) n = n + 0x6F;
                    break;
                }
            }
        }
        m[0] = n; target = target + 1;
        if (target == letter_n)
            break;
    }
    return target; // i;
}




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

        char msg_tmp[Number_of_bytes_block] = "";                            // Массив для приема текстовых сообщений
        char time_msg_tmp[Number_of_bytes_time] = "";                        // Массив для приема времени текстовых сообщений
        char msg_tmp_bt[Number_of_bytes_block] = "";                         // Массив для отправки текстовых сообщений по Bluetooth

        char msgOK_Trecker[4] = "#";                                         // Формирование строки для ответного сообщения 
        strcat(msgOK_Trecker, Number_from_Message);                          // Добавили в ответ номер ответного сообщения
        pStream->println(msgOK_Trecker);                                     // Передать подтерждение о получении сообщения в треккер

        int minute_all = 0;
        int minute_msg = 0;
        int hour_msg = 0;
        int min_msg = 0;

        if (gnss.time.isValid())
        {
            minute_all = (int)(gnss.time.hour() * 60) + (int)gnss.time.minute();
        }
        else
        {
            minute_all = (10 * 60) + 20;
        }

        if (timeString.length() != 0)
        {
            int index_hour = timeString.indexOf(':');
            if (index_hour != -1)
            {
                String hour_tmp = timeString.substring(0, index_hour);
                hour_msg = hour_tmp.toInt();
                String min_tmp = timeString.substring(index_hour + 1);
                min_msg = min_tmp.toInt();
            }
            strncpy(time_msg_tmp, timeString.c_str(), timeString.length() + 1);  // Преобразование принятую строку c времени, String в массив char для последующей обработки
        }

        // Определить не просрочено ли время прихода сообщения.
        minute_msg = (hour_msg * 60) + min_msg;

        strncpy(msg_tmp, textString.c_str(), textString.length() + 1);          // Преобразование принятую строку String в массив char для последующей обработки
        int tmp_msg_len = mb_strlen(msg_tmp, textString.length()+1);
        pStream->println(tmp_msg_len);

        if ((minute_all - minute_msg) <= 10 && minute_msg < minute_all || (minute_all - minute_msg) == 0)
        {

            if (textString.length() != 0)
            {
                if (tmp_msg_len > 72)  // Проверить максимальную длину сообщения
                {
                    pStream->print(CORE_COMMAND_ANSWER_ERROR);
                    pStream->println(F("The text is very long. !Maximum 72 characters"));
                    msg_tmp[textString.length() + 1] = { 0 };
                    strncpy(msg_tmp, textString.c_str(), 72 + 1);          // Преобразование принятую строку String в массив char для последующей обработки
                }

                if (timeString.length() != 0)
                {
                    strncpy(time_msg_tmp, timeString.c_str(), timeString.length() + 1);  // Преобразование принятую строку c временем, String в массив char для последующей обработки
                    strcat(msg_tmp_all, time_msg_tmp);                                   // Записать в строку время отправки сообщения
                    strcat(msg_tmp_all, " ");                                            // Добавить пробел между временем и сообщением
                    strcat(msg_tmp_all, msg_tmp);                                        // Записать в строку само сообщение
                }

                snprintf_P(msg_tmp_bt, sizeof(msg_tmp_bt),
                    PSTR("$FLYMSG,%s,%s" PFLAA_EXT1_FMT "*"),
                    time_msg_tmp,
                    msg_tmp
                    PFLAA_EXT1_ARGS);

                NMEA_add_checksum(msg_tmp_bt, sizeof(msg_tmp_bt) - strlen(msg_tmp_bt));

                NMEA_Out(settings->nmea_out, (byte*)msg_tmp_bt, strlen(msg_tmp_bt), false);

                Commands.clearCommand();
                service.setMessageRead(true);
                service.setNewMessageFlag(true);    // Установить флаг нового сообщения. Программа извещена и приступила к обработке нового сообщения.
            }
        }
        else
        {
            pStream->print(CORE_COMMAND_ANSWER_ERROR);
            pStream->println(F("Incorrect time! Please check the time"));
        }

    }
   return true;
}


bool  CommandHandlerClass::clearMail(const char* commandPassed, CommandParser& parser, Stream* pStream)      // Стереть всю почту
{

    if (parser.argsCount() < 1)
        return false;
    clear_message = true; // Стереть всю почту
    pStream->print(CORE_COMMAND_PARAM_DELIMITER);
    pStream->print(CORE_COMMAND_ANSWER_OK);
    return true;
}


bool CommandHandlerClass::setFly(const char* param)
{
    /*
     Разбираем параметр данных самолета на составные части
     #1#SET#FLY#адрес,Squawk,номер рейса,altitude,pressure_altitude,speed,course,vert_rate,latitude,longitude,aircraft_type,hour,minute
    */
    // буфер под промежуточные данные
    char workBuff[13] = { 0 };
    char* writePtr = workBuff;

    fo_msg = EmptyFO;

    const char* delim = strchr(param, ',');  // Определяем первые данные
    if (!delim || (delim - param > 6))
        return false;
    while (param < delim)
        *writePtr++ = *param++;
    *writePtr = 0;
    writePtr = workBuff;
    fo_msg.addr = strtoul(writePtr, NULL, 16); // Извлечение адреса из строки
 
    param = delim + 1;                  // Извлечение Squawk из строки
    delim = strchr(param, ',');
    if (!delim || (delim - param > 4))
        return false;
    while (param < delim)
        *writePtr++ = *param++;
    *writePtr = 0;
    writePtr = workBuff;
    fo_msg.Squawk = atoi(writePtr);
 
    param = delim + 1;                   // Извлечение номера рейса из строки
    delim = strchr(param, ',');
    if (!delim || (delim - param > 8))
        return false;
    while (param < delim)
        *writePtr++ = *param++;
    *writePtr = 0;
    writePtr = workBuff;
    strncpy(fo_msg.flight, writePtr, strlen(writePtr));  //strlenПреобразование принятую строку
  
    param = delim + 1;               // Вариант измерения высоты геоид
    delim = strchr(param, ',');
    if (!delim || (delim - param > 8))
        return false;
    while (param < delim)
        *writePtr++ = *param++;
    *writePtr = 0;
    writePtr = workBuff;
    fo_msg.altitude = atof(writePtr);
 
    param = delim + 1;               // Извлечение pressure_altitude из строки
    delim = strchr(param, ',');
    if (!delim || (delim - param > 8))
        return false;

    while (param < delim)
        *writePtr++ = *param++;
    *writePtr = 0;
    writePtr = workBuff;
    fo_msg.pressure_altitude = atof(writePtr);
 
    param = delim + 1;               // Извлечение speed из строки
    delim = strchr(param, ',');
    if (!delim || (delim - param > 6))
        return false;
    while (param < delim)
        *writePtr++ = *param++;
    *writePtr = 0;
    writePtr = workBuff;
    fo_msg.speed = atof(writePtr);
 
    param = delim + 1;               // Извлечение course из строки
    delim = strchr(param, ',');
    if (!delim || (delim - param > 6))
        return false;
    while (param < delim)
        *writePtr++ = *param++;
    *writePtr = 0;
    writePtr = workBuff;
    fo_msg.course = atof(writePtr);
 
    param = delim + 1;               // Извлечение vert_rate из строки
    delim = strchr(param, ',');
    if (!delim || (delim - param > 4))
        return false;
    while (param < delim)
        *writePtr++ = *param++;
    *writePtr = 0;
    writePtr = workBuff;
    fo_msg.vert_rate = atoi(writePtr);
 
    param = delim + 1;               // Извлечение latitude из строки
    delim = strchr(param, ',');
    if (!delim || (delim - param > 10))
        return false;
    while (param < delim)
        *writePtr++ = *param++;
    *writePtr = 0;
    writePtr = workBuff;
    fo_msg.latitude = atof(writePtr);
 
    param = delim + 1;               // Извлечение longitude из строки
    delim = strchr(param, ',');
    if (!delim || (delim - param > 10))
        return false;
    while (param < delim)
        *writePtr++ = *param++;
    *writePtr = 0;
    writePtr = workBuff;
    fo_msg.longitude = atof(writePtr);
 
    param = delim + 1;               // Извлечение aircraft_type из строки
    delim = strchr(param, ',');
    if (!delim || (delim - param > 2))
        return false;
    while (param < delim)
        *writePtr++ = *param++;
    *writePtr = 0;
    writePtr = workBuff;
    fo_msg.aircraft_type = atoi(workBuff);

    param = delim + 1;               // Извлечение часов  из строки
    delim = strchr(param, ',');
    if (!delim || (delim - param > 3))
        return false;
    while (param < delim) 
        *writePtr++ = *param++;
    *writePtr = 0;
    writePtr = workBuff;
    fo_msg.hour_msg = atoi(workBuff);  // Получить часы
 
    param = delim + 1; // перемещаемся на следующий компонент - минута
    while (*param && writePtr < &(workBuff[2]))
        *writePtr++ = *param++;
    *writePtr = 0;
    fo_msg.min_msg = atoi(workBuff);   // Получить минуты

    fo_msg.timestamp = now(); //  
    fo_msg.signal_source = 2;
 
    int minute_all = 0;
    int minute_msg = 0;

    if (gnss.time.isValid())
    {
        minute_all = ((int)gnss.time.hour() * 60) + gnss.time.minute();
    }
    else
    {
        minute_all = (10 * 60) + 20;
    }

    // Определить не просрочено ли время прихода сообщения.
    minute_msg = (fo_msg.hour_msg * 60) + fo_msg.min_msg;

    if ((minute_all - minute_msg) <= 10 && minute_msg < minute_all || (minute_all - minute_msg) == 0)
    {
        // Расчет расстояния, курса и уровня опастности сближения нашего и стороннего самолета
        if (fo_msg.latitude != 0 && fo_msg.longitude != 0) // Расчет возможен если получены координаты нашего и стороннего самолета
        {
            Traffic_Update(&fo_msg);   // Определяем дистанцию и курс стороннего самолета
        }

        // Остальные параметры записываем в базу 
        Traffic_Msg_Add(&fo_msg);
        return true;
    }
    /* Пример строки
    #1#SET#FLY#адрес, Squawk, номер рейса, altitude, pressure_altitude, speed, course, vert_rate, latitude, longitude, aircraft_type, hour, minute

    */

    return false;
}

bool CommandHandlerClass::Traffic_Msg_Add(ufo_t* fop)
{
    int i;

    for (i = 0; i < MAX_TRACKING_OBJECTS; i++)
    {
        if (Container_msg[i].addr == fo_msg.addr && fo_msg.addr != 0) // Если объект записан - обновить и завершить
        {
            Container_msg[i] = fo_msg;
            return true;
        }
    }

    for (i = 0; i < MAX_TRACKING_OBJECTS; i++)
    {
        if(Container_msg[i].addr == 0)
        {
            Container_msg[i] = fo_msg;
            Container_msg[i].delay_time_msg = 0;
            return true;
        }
    }

    return true;
}

void CommandHandlerClass::SendTraffic_Msg()
{
 
    static uint32_t tmr_msg = millis();

    if (millis() - tmr_msg > 2000)
    {
        tmr_msg = millis();

        for (int i = 0; i < MAX_TRACKING_OBJECTS; i++)
        {
 
            fo.addr = Container_msg[i].addr;
            fo.Squawk = Container_msg[i].Squawk;
            memcpy((char*)fo.flight, Container_msg[i].flight, strlen(Container_msg[i].flight));
            fo.altitude = Container_msg[i].altitude;
            fo.pressure_altitude = Container_msg[i].pressure_altitude;
            fo.speed = Container_msg[i].speed;
            fo.course = Container_msg[i].course;
            fo.vert_rate = Container_msg[i].vert_rate;
            fo.latitude = Container_msg[i].latitude;
            fo.longitude = Container_msg[i].longitude;
            fo.aircraft_type = Container_msg[i].aircraft_type;
            fo.timestamp = now(); //  
            fo.signal_source = 2;
            fo.hour_msg = Container_msg[i].hour_msg;  // Получить часы
            fo.min_msg = Container_msg[i].min_msg;    // Получить минуты


            // Расчет расстояния, курса и уровня опастности сближения нашего и стороннего самолета
            if (fo.latitude != 0 && fo.longitude != 0) // Расчет возможен если получены координаты нашего и стороннего самолета
            {
                Traffic_Update(&fo);   // Определяем дистанцию и курс стороннего самолета
            }

            // Остальные параметры записываем в базу 
            Traffic_Add(&fo);

            if (now() - Container_msg[i].timestamp > MSG_OFF_EXPIRATION_TIME && Container_msg[i].addr != 0) // Удалить данные по самолету через 10 минут
            {
                Container_msg[i] = EmptyFO;
                Container[i] = EmptyFO;
            } 

            if (Container_msg[i].latitude != 0 && Container_msg[i].longitude != 0) // Проверим на всякий случай.
            {
                //int minute_msg_tmp = 0;
                if(Container_msg[i].delay_time_msg == 0) // Первая запись в базу
                { 
     
                    if (gnss.time.isValid()) // Текущее время
                    {
                        hour_gps_tmp_d[i] = (int)gnss.time.hour() * 60;
                        minute_gps_tmp_d[i] = (int)gnss.time.minute();
                        minute_all_d[i] = hour_gps_tmp_d[i] + minute_gps_tmp_d[i]; // Текущее GPS время в минутах
                    }
                    else
                    {
                        minute_all_d[i] = (10 * 60) + 20;             // Текущее тестовое время (10-20) в минутах
                    }

                    // Определить не сколько прошло секунд со времени отправки сообщения.
                    minute_msg_d[i] = (Container_msg[i].hour_msg * 60) + Container_msg[i].min_msg; // Время отправки сообщения в минутах

                    minute_msg_tmp_d[i] = (minute_all_d[i] - minute_msg_d[i]) * 60;   // Разность времени текущего и времени отправки сообщения в секундах
                    Container_msg[i].delay_time_msg = now();                          // Записать в базу стороннего самолета текущее время 
                }
                else
                {
                    minute_msg_tmp_d[i] = 2.0; 
                    Container_msg[i].delay_time_msg = now();
                }
                 
                if (Container_msg[i].speed > 5 && Container_msg[i].altitude > 10)
                {
                    float speed_tmp = (Container_msg[i].speed * 1000) / 3600 * minute_msg_tmp_d[i];
                    test3coordinat(Container_msg[i].latitude, Container_msg[i].longitude, speed_tmp, Container_msg[i].course, i);
                }
            }
        }
    }
}

//====================== Расчет координаты точки при наличии курса и расстояния ================================

void CommandHandlerClass::test3coordinat(float lat1, float lon1, float dist, float brng, uint8_t i_Container)
{
    CurLon   = lon1;   //37.243040;
    CurLat   = lat1;   // 56.097114;
    Bearing  = brng;   // Bearing of travel
    Distance = dist;   // m per update 

    r_CurLon = radians(CurLon);
    r_CurLat = radians(CurLat);
    r_Bearing = radians(Bearing);
    float DestLat = asin(sin(r_CurLat) * cos(Distance / Eradius) + cos(r_CurLat) * sin(Distance / Eradius) * cos(r_Bearing));
    float DestLon = r_CurLon + atan2(sin(r_Bearing) * sin(Distance / Eradius) * cos(r_CurLat), cos(Distance / Eradius) - sin(r_CurLat) * sin(DestLat));

    DestLon = (DestLon + 3 * PI) / (2 * PI);
    int i = DestLon;
    DestLon = (DestLon - i) * (2 * PI) - PI;  // normalise to -180..+180º 
    Container_msg[i_Container].latitude = degrees(DestLat);
    Container_msg[i_Container].longitude = degrees(DestLon);
}

 
//--------------------------------------------------------------------------------------------------------------------------------------
bool CommandHandlerClass::getFly(Stream* pStream) // получение информацию с базы данных
{
    pStream->println(CORE_COMMAND_ANSWER_OK);
    send_ThisAircraft();
    return true;
}

void CommandHandlerClass::send_ThisAircraft()
{
    char FLYBuffer[128];
 
            snprintf_P(FLYBuffer, sizeof(FLYBuffer),
                PSTR("$%06X,%d,%d,%d,%d,%6f,%6f,%d""*"),
                ThisAircraft.addr,                   // Адрес нашего устройства, которому отправлен запрос
                (int)ThisAircraft.altitude,          // Высота геоид (GPS)
                (int)ThisAircraft.pressure_altitude, // Высота по датчику давления
                (int)ThisAircraft.speed,             // Скорость  km/h IAS
                (int)ThisAircraft.course,            // Курс в градусах
                ThisAircraft.latitude,               // Широта
                ThisAircraft.longitude,              // Долгота
                ThisAircraft.aircraft_type           // Тип воздушного судна
             );   
                                                     // *
                                                     // Контрольная сумма пакета
            NMEA_add_checksum(FLYBuffer, sizeof(FLYBuffer) - strlen(FLYBuffer));
            SERIAL_TRACKER.print(FLYBuffer);
 
}


//--------------------------------------------------------------------------------------------------------------------------------------
bool CommandHandlerClass::getBase(Stream* pStream) // получение информацию с базы данных
{
    pStream->println(CORE_COMMAND_ANSWER_OK);
    send_base();
    return true;
}

void CommandHandlerClass::send_base()
{
   // char FLYBuffer[128];

    for (int i = 0; i < MAX_TRACKING_OBJECTS; i++)
    {
        if (Container[i].addr)  // Если есть самолет в базе
        {
            char FLYBuffer[128];
            snprintf_P(FLYBuffer, sizeof(FLYBuffer),
                PSTR("$%06X,%d,%8s,%d,%d,%d,%d,%d,%6f,%6f,%d,%d,%d,%d""*"),
                Container[i].addr,                   // Адрес устройства стороннего самолета
                Container[i].Squawk,                 // Номер, назначаемый диспетчером для обмена с локатором.
                Container[i].flight,                 // Номер рейса
                (int)Container[i].altitude,          // Высота геоид (GPS)
                (int)Container[i].pressure_altitude, // Высота по датчику давления
                (int)Container[i].speed,             // Скорость  km/h IAS
                (int)Container[i].course,            // Курс в градусах
                Container[i].vert_rate,              // Скорость подъема или снижения ft/m
                Container[i].latitude,               // Широта
                Container[i].longitude,              // Долгота
                Container[i].aircraft_type,          // Тип воздушного судна
                Container[i].signal_source,          // Источник данных (0-LoRa, 1- DUMP1090, 2 - текстовая строка из GSM или Иридиум)
                Container[i].hour_msg,               // Время час
                Container[i].min_msg                 // Время минуты 
            );                    // 
                                                 // *
                                                 // Контрольная сумма пакета
            NMEA_add_checksum(FLYBuffer, sizeof(FLYBuffer) - strlen(FLYBuffer));
            SERIAL_TRACKER.print(FLYBuffer);
        }
    }

}

bool  CommandHandlerClass::setGSM(const char* commandPassed, CommandParser& parser, Stream* pStream)      //
{
    if (parser.argsCount() > 2)
    {
        String GSMCommand;

        GSMCommand = parser.getArg(3);
        pStream->print(CORE_COMMAND_ANSWER_OK);
        pStream->println(GSMCommand);

        if (GSMCommand.equalsIgnoreCase("OFF"))// -возвращает true, если myString совпадает с myString2.Регистр букв неважен
        {
            settings->gsm_send = GSM_SEND_OFF;
            delay(1000);
            EEPROM_store();
            RF_Shutdown();
            delay(1000);
            SoC->reset();
        }
        else if (GSMCommand.equalsIgnoreCase("SINGLE"))
        {
            settings->gsm_send = GSM_SEND_SINGLE;
            delay(1000);
            EEPROM_store();
            RF_Shutdown();
            delay(1000);
            SoC->reset();
        }
        else if (GSMCommand.equalsIgnoreCase("AUTO"))
        {
            settings->gsm_send = GSM_SEND_AUTO;
            delay(1000);
            EEPROM_store();
            RF_Shutdown();
            delay(1000);
            SoC->reset();
        }
        else if (GSMCommand.equalsIgnoreCase("MINI"))
        {
            settings->gsm_send = GSM_SEND_MINI;
            delay(1000);
            EEPROM_store();
            RF_Shutdown();
            delay(1000);
            SoC->reset();
        }
        else
        {
            return false;
        }

        return true;
    }

    return false;
}

void CommandHandlerClass::GPS_send_base()
{
    gsm_alien_count = Traffic_Count();
    if ((settings->gsm_send == GSM_SEND_AUTO))
    {
        static uint32_t gsm_tmr = millis();
        if (millis() - gsm_tmr > 1000*60)
        {
            gsm_alien_count = Traffic_Count();
            gsm_tmr = millis();
            if (gsm_alien_count != 0)
            {
                send_base();
            }
        }
    }
    else if (settings->gsm_send == GSM_SEND_SINGLE)
    {
        if (gsm_alien_count == 0) return;
        if (gsm_alien_count != 0 && gsm_alien_count != gsm_alien_count_old)
        {
            gsm_alien_count_old = gsm_alien_count;
            send_base();
        }
    }

    else if (settings->gsm_send == GSM_SEND_MINI)
    {
        if (gsm_alien_count == 0) return;
        for (int i = 0; i < MAX_TRACKING_OBJECTS; i++)
        {
            if ((Container[i].addr != addr_old[i]) && (Container[i].addr != 0) && (Container[i].latitude != 0) && (Container[i].addr != ThisAircraft.addr))
            {
                addr_old[i] = Container[i].addr;
                char FLYBuffer[128];
                snprintf_P(FLYBuffer, sizeof(FLYBuffer),
                    PSTR("$%06X,%d,%8s,%d,%d,%d,%d,%d,%6f,%6f,%d,%d,%d,%d""*"),
                    Container[i].addr,                   // Адрес устройства стороннего самолета
                    Container[i].Squawk,                 // Номер, назначаемый диспетчером для обмена с локатором.
                    Container[i].flight,                 // Номер рейса
                    (int)Container[i].altitude,          // Высота геоид (GPS)
                    (int)Container[i].pressure_altitude, // Высота по датчику давления
                    (int)Container[i].speed,             // Скорость  km/h IAS
                    (int)Container[i].course,            // Курс в градусах
                    Container[i].vert_rate,              // Скорость подъема или снижения ft/m
                    Container[i].latitude,               // Широта
                    Container[i].longitude,              // Долгота
                    Container[i].aircraft_type,          // Тип воздушного судна
                    Container[i].signal_source,          // Источник данных (0-LoRa, 1- DUMP1090, 2 - текстовая строка из GSM или Иридиум)
                    Container[i].hour_msg,               // Время час
                    Container[i].min_msg                 // Время минуты 
                );                    // 
                                                     // *
                                                     // Контрольная сумма пакета
                NMEA_add_checksum(FLYBuffer, sizeof(FLYBuffer) - strlen(FLYBuffer));
                SERIAL_TRACKER.print(FLYBuffer);
            }
            else if (!Container[i].addr)
            {
                addr_old[i] = 0;
            }
        }
    }
}

bool  CommandHandlerClass::setBTOK(const char* commandPassed, CommandParser& parser, Stream* pStream)      //
{
    if (parser.argsCount() > 1)
    {
       // String GSMCommand;
        service.setMessageRead(true);
        return true;
    }
    return false;
}

// --------------------------------------------------------------------------------------------------------------------------------------
