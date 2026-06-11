#pragma once

#include "CoreButton.h"           // Обработка кнопок
#include "EEPROMRF.h"

//--------------------------------------------------------------------------------------------------------------------------------
class SettingsClass
{
public:
    SettingsClass();

    void setup();
    void update();                                                // обновить данные
    bool get_empty_buffer_request();
    void set_empty_buffer_request(bool buffer_request);

    uint8_t getCoutNotReadMessage();                              // получить показания счетчика не подтвержденного количества записей
    void setCoutNotReadMessage(uint8_t count);                    // сохранить состояние счетчика не подтвержденного количества записей 

    uint8_t  getCurrentCountMessage();
    void setCurrentCountMessage(uint8_t count_cur);

    bool getNewMessageFlag();
    void setNewMessageFlag(bool new_flag);



     
private:

    bool empty_buffer = false;

};
//--------------------------------------------------------------------------------------------------------------------------------
extern SettingsClass SettingsMail;
