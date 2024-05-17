#pragma once

#include "CoreButton.h"           // Обработка кнопок


//--------------------------------------------------------------------------------------------------------------------------------
class SettingsClass
{
public:
    SettingsClass();

    void setup();
    void update();                                               // обновить данные
    bool get_empty_buffer_request();
    void set_empty_buffer_request(bool buffer_request);

private:

    bool empty_buffer = false;

};
//--------------------------------------------------------------------------------------------------------------------------------
extern SettingsClass SettingsMail;
