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
    void setMail_on(bool On_Off);
    bool getMail_on();
    void setMessageRead(bool On_Message);
    bool getMessageRead();
    void setAllow_flashing(bool On_Off);
    bool getAllow_flashing();
    void setMailOn(bool On_Off);
    bool getMailOn();
    void set_confirm_message(bool On_Off);
    bool get_confirm_message();
    void setClearMessage(bool On_Off);
    bool get_ClearMessage();
    void setClearButton(bool On_Off);
    bool get_ClearButton();
    void set_count_buttton(uint8_t button);
    uint8_t get_count_buttton();

    void set_num_buttton(uint8_t button);
    uint8_t get_num_buttton();
 
private:

    bool ButtonRetention = false;
    bool new_msg_flag = false;
    bool MailOn = false;
    String Current_MSG;
    String Current_version;
    uint8_t number_from_message;

    uint8_t count_buttton = 0;
    uint8_t num_buttton = 0;
    bool ClearButton = false;
    bool ClearMessage = false;
    bool MessageRead = false;
    bool flashing = false;
    bool flags_MailOn = false;
    bool confirm_message = false;
 

};
//--------------------------------------------------------------------------------------------------------------------------------
extern ServiceClass service;
