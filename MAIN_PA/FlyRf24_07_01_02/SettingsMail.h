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

    uint8_t  getNumber_from_Message();
    void setNumber_from_Message(uint8_t count_cur);

    uint8_t  getFlippingCountMessage();
    void setFlippingCountMessage(uint8_t count_cur);

    bool getNewMessageFlag();
    void setNewMessageFlag(bool new_flag);


    bool GetButtonRetention() { return ButtonRetention; }         // Получить флаг длительного нажатия кнопки
    void SetButtonRetention(bool val);                            // Записать флаг длительного нажатия кнопки

     
private:

    bool empty_buffer = false;
    bool ButtonRetention = false;
 /*   uint32_t _timer;
    uint16_t blinkInterval;
    uint16_t timer;*/
};
//--------------------------------------------------------------------------------------------------------------------------------
extern SettingsClass SettingsMail;
