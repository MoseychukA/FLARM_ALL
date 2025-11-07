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
    void update();                                   // обновить данные

    bool getNewMessageFlag();                        // Получить признак прихода нового сообщения               
    void setNewMessageFlag(bool new_flag);           // Установить признак прихода нового сообщения   
    bool getMessageRead();                           // Получить признак о том что новое сообщение получено в выносном дисплее
    void setMessageRead(bool On_Message);            // Установить признак о том что новое сообщение получено в выносном дисплее

    void saveVer(String ver);
    String getVer();
 
    void saveMSG(String msg_save);
    String getMSG();
    void setMail_on(bool On_Off);
    bool getMail_on();
    void setAllow_flashing(bool On_Off);
    bool getAllow_flashing();
    void setViewTime(bool On_Off);
    bool getViewTime();
    void set_confirm_message(bool On_Off);
    bool get_confirm_message();
    void setClearMessage(bool On_Off);
    bool get_ClearMessage();
    void setClearButton(bool On_Off);
    bool get_ClearButton();
    void set_count_buttton(uint8_t button);
    uint8_t get_count_buttton();

    void set_voltage_value(float value);
    float get_voltage_value();
    void set_current_value(float current);
    float get_current_value();
    void set_GNSS_on_off(bool GNSSOn_Off);
    bool get_GNSS_on_off();
    void set_SOS_on_off(bool SOSOn);
    bool get_SOS_on_off();
    void set_num_buttton(uint8_t button);
    uint8_t get_num_buttton();
    void set_time_hour(uint8_t hour);
    uint8_t get_time_hour();
    void set_time_minute(uint8_t minute);
    uint8_t get_time_minute();
    char msg_tmp_all[170] = "";
    bool get_connection_base();
    void set_connection_base(bool connection);
 
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
    bool flags_ViewTime = false;
    bool confirm_message = false;

    float voltage_value = 0;
    float current_value = 0;
    bool GNSS_On_Off = false;
    bool SOSOn_Off = false;
    uint8_t hour_m =  0;
    uint8_t minute_m = 0;
    bool connection_ = false;

};
//--------------------------------------------------------------------------------------------------------------------------------
extern ServiceClass service;
