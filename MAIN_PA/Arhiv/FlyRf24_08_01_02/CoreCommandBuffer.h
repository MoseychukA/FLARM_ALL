#pragma once

#include <Arduino.h>
#include "TinyVector.h"
#include "SettingsMain.h"
#include "SoftRF.h"
#include "ESP32RF.h"
#include "SoC.h"


typedef struct _MailTime {
    unsigned int year; /**< Years                    - [1900, 2089]                 */
    unsigned int mon;  /**< Months                   - [   1,   12]                 */
    unsigned int day;  /**< Day of the month         - [   1,   31]                 */
    unsigned int hour; /**< Hours since midnight     - [   0,   23]                 */
    unsigned int min;  /**< Minutes after the hour   - [   0,   59]                 */
    unsigned int sec;  /**< Seconds after the minute - [   0,   60] (1 leap second) */
    unsigned int hsec; /**< Hundredth part of second - [   0,   99]                 */
    bool isValid;
} MailTime;


typedef struct _TimeInfo {

    MailTime       utc;        /**< UTC of the position data                                        */

} TimeInfo;




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
    bool clear_mail = false; // Стереть всю почту
    bool Traffic_Msg_Add(ufo_t* fop);
    void SendTraffic_Msg();
    /* вариант второй */
  
 private:

    void onUnknownCommand(const String& command, Stream* outStream);
    bool printBackSETResult(bool isOK, const char* command, Stream* pStream);
    bool setTXT(const char* commandPassed, CommandParser& parser, Stream* pStream, String textString); // 
    bool clearMail(const char* commandPassed, CommandParser& parser, Stream* pStream);                 // Стереть всю почту
    bool setFly(const char* param);               // Установить параметры стороннего самолета
    int i_msg = 0;
    uint8_t air_msg_count = 0;

    /* вариант первый */
    void SphereDirect(double pt1[], double azi, double dist, double pt2[]);
    void Rotate(double x[], double a, int i);
    void SpherToCart(double y[], double x[]);
    double CartToSpher(double x[], double y[]);
    void SphereInverse(double pt1[], double pt2[], double* azi, double* dist);
    int test1coordinat(float lat1, float lon1, double azi1, double dist);
   // int test1coordinat();
   
    ///* вариант второй */
    void test2coordinat(double lat1, double lon1, double d, double brng);
    void test3coordinat(double lat1, double lon1, double d, double brng);


    float ConvertData(float RawDegrees);

    double lat2;
    double lon2;

    double lat11;
    double lon11;

    double R = 6371000; //radius of earth in meters

    int DD, MM;
    double SS;

    void DD_DDDDDtoDDMMSS(double DD_DDDDD, int* DD, int* MM, double* SS);

    char buf[1024] = { 56.097114,37.243040,1000.0,1.0 };
    double pt1[2], pt2[2];
    double lat1, lon1, azi1, dist, azi2;
    double dr;


    float CurLon = 0; //37.243040;
    float CurLat = 0; // 56.097114;
    float r_CurLon;
    float r_CurLat;
    float Bearing = 1; // Bearing of travel
    float r_Bearing;
    float Distance = 1; // km per update 
    int Eradius = 6371; // mean radius of the earth 



};
//--------------------------------------------------------------------------------------------------------------------------------------
extern ufo_t fo, fo_msg, Container_msg[MAX_TRACKING_OBJECTS];
extern CommandHandlerClass CommandHandler;

