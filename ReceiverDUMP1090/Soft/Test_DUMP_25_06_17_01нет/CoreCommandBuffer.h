#pragma once

#include <Arduino.h>
#include "TinyVector.h"
#include "ServiceMain.h"
#include "SoftRF.h"
#include "ESP32RF.h"
#include "SoC.h"
#include "ESP32RF.h"

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
//



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
  bool bluetoothCommand();
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
    bool clear_message = false;           // Стереть всю почту
    bool Traffic_Msg_Add(ufo_t* fop);
    void SendTraffic_Msg();
    void send_ThisAircraft();
    void send_base();
    void GPS_send_base();
    char msg_tmp_all[150] = "";
  
 private:

    void onUnknownCommand(const String& command, Stream* outStream);
    bool printBackSETResult(bool isOK, const char* command, Stream* pStream);
    int mb_strlen(char* source, int letter_n);
    bool setTXT(const char* commandPassed, CommandParser& parser, Stream* pStream, String textString); // 
    bool clearMail(const char* commandPassed, CommandParser& parser, Stream* pStream);                 // Стереть всю почту
    bool setFly(const char* param);               // Установить параметры стороннего самолета
    bool getVER(Stream* pStream);
    bool getFly(Stream* pStream); // получение данных в базе
    bool getBase(Stream* pStream); // получение информацию с базы данных
    bool setGSM(const char* commandPassed, CommandParser& parser, Stream* pStream);
    bool setBTOK(const char* commandPassed, CommandParser& parser, Stream* pStream);

    int i_msg = 0;
    uint8_t air_msg_count = 0;

 
    void test3coordinat(float lat1, float lon1, float dist, float brng, uint8_t i_Container);

    float CurLon = 0; //37.243040;
    float CurLat = 0; // 56.097114;
    float r_CurLon;
    float r_CurLat;
    float Bearing; // Bearing of travel
    float r_Bearing;
    float Distance; // km per update 
    int Eradius = 6371000; // mean radius of the earth 

    int minute_all_d[MAX_TRACKING_OBJECTS];
    int minute_msg_d[MAX_TRACKING_OBJECTS];
    int hour_gps_tmp_d[MAX_TRACKING_OBJECTS];
    int minute_gps_tmp_d[MAX_TRACKING_OBJECTS];
    int minute_msg_tmp_d[MAX_TRACKING_OBJECTS];


    uint8_t gsm_alien_count_old = 0;
    uint8_t gsm_alien_count = 0;
    uint32_t  addr_old[MAX_TRACKING_OBJECTS];

};
//--------------------------------------------------------------------------------------------------------------------------------------
extern ufo_t fo, fo_msg, Container_msg[MAX_TRACKING_OBJECTS];
extern CommandHandlerClass CommandHandler;

