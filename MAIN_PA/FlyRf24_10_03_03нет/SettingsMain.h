#pragma once

#include "EEPROMRF.h"

//--------------------------------------------------------------------------------------------------------------------------------
class SettingsClass
{
public:
    SettingsClass();

    void setup();
    void update();                                                // обновить данные
 
    bool getNewMessageFlag();
    void setNewMessageFlag(bool new_flag);

 /*   uint8_t  getNumber_from_Message();
    void setNumber_from_Message(uint8_t count_cur);*/

    void saveVer(String ver);
    String getVer();
   // String Current_version;

    void saveMSG(String msg_save);
    String getMSG();
   // String Current_MSG;

   // uint8_t  air_msg[Number_of_bytes_msg][MAX_TRACKING_OBJECTS];  // ћассив сохранени€ текстовых данных о чужом самолете



private:

  //  bool empty_buffer = false;
    bool ButtonRetention = false;
    bool new_msg_flag = false;
    String Current_MSG;
    String Current_version;
    uint8_t number_from_message;

};
//--------------------------------------------------------------------------------------------------------------------------------
extern SettingsClass SettingsMain;
