#pragma once

#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>
#include <Arduino.h>
#include "Configuration_ESP32.h"
//--------------------------------------------------------------------------------------------------------------------------------
class ServiceClass
{
public:
    ServiceClass();

    void setup();
    void update();                                                // обновить данные

    bool getNewMessageFlag();
    void setNewMessageFlag(bool new_flag);

    void saveVer(String ver);
    String getVer();
 
    void saveMSG(String msg_save);
    String getMSG();
    void saveMail_on(bool On_Off);
    bool getMail_on();

private:

    bool ButtonRetention = false;
    bool new_msg_flag = false;
    bool MailOn = false;
    String Current_MSG;
    String Current_version;
    uint8_t number_from_message;

};
//--------------------------------------------------------------------------------------------------------------------------------
extern ServiceClass service;
