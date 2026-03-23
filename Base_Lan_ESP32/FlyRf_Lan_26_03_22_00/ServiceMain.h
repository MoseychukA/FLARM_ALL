#pragma once

#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>
#include <Arduino.h>
#include "Configuration_ESP32.h"
#include <Arduino.h>


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
    void setMessageRead(bool On_Message);         // Установить флаг прихода нового сообщения
    bool getMessageRead();                        // Получить флаг прихода нового сообщения
    void setAllow_flashing(bool On_Off);          // Установить флаг разрешения мигания сообщения
    bool getAllow_flashing();                     // Получить флаг разрешения мигания сообщения
    void setViewTime(bool On_Off);
    bool getViewTime();
    void set_deletion_time(bool On_Off);          // Установить флаг отсчета времени удаления сообщения через 10 минут
    bool get_deletion_time();                     // Получить флаг отсчета времени удаления сообщения через 10 минут
    void set_confirm_message(bool On_Off);        // Установить флаг разрешения отправки подтверждения прочтения сообщения
    bool get_confirm_message();                   // Получить флаг разрешения отправки подтверждения прочтения сообщения
    void setClearMessage(bool On_Off);            // Установить флаг удаления сообщения
    bool get_ClearMessage();                      // Получить флаг удаления сообщения
    void setClearButton(bool On_Off);             // Установить флаг удаления номера кнопки
    bool get_ClearButton();                       // Получить флаг удаления номера кнопки
    void set_count_buttton(uint8_t button);       // Установить номер кнопки
    uint8_t get_count_buttton();                  // Получить номер кнопки

    void set_num_button(uint8_t button);
    uint8_t get_num_button();
    float  battery_read();
    void set_GNSS_on_off(bool GNSSOn_Off);
    bool get_GNSS_on_off();
    void set_SOS_on_off(bool SOSOn);
    bool get_SOS_on_off();
    void set_time_hour(uint8_t hour);
    uint8_t get_time_hour();
    void set_time_minute(uint8_t minute);
    uint8_t get_time_minute();
    void initContainerMutex();
    bool lockContainer(TickType_t timeout = portMAX_DELAY);
    void unlockContainer();
    char msg_tmp_all[BUFFER_SIZE] = "";
    bool get_connection_base();
    void set_connection_base(bool connection);

private:

    bool ButtonRetention = false;
    bool new_msg_flag = false;
    bool MailOn = false;
    String Current_MSG;
    String Current_version;
    uint8_t number_from_message;

    uint8_t count_button = 0;
    uint8_t num_button = 0;
    bool ClearButton = false;
    bool ClearMessage = false;
    bool MessageRead = false;
    bool flashing = false;
    bool flags_ViewTime = false;
    bool flags_MailOn = false;
    bool confirm_message = false;
    bool GNSS_On_Off = false;
    uint8_t hour_m = 10;
    uint8_t minute_m = 10;
    bool SOSOn_Off = false;
    bool connection_ = false;

};
//--------------------------------------------------------------------------------------------------------------------------------
extern ServiceClass service;
//ServiceClass service;
