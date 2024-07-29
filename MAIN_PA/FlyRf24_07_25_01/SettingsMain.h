#pragma once

#include "CoreButton.h"           // ��������� ������
#include "EEPROMRF.h"

//--------------------------------------------------------------------------------------------------------------------------------
class SettingsClass
{
public:
    SettingsClass();

    void setup();
    void update();                                                // �������� ������
    bool get_empty_buffer_request();
    void set_empty_buffer_request(bool buffer_request);

    uint8_t getCoutNotReadMessage();                              // �������� ��������� �������� �� ��������������� ���������� �������
    void setCoutNotReadMessage(uint8_t count);                    // ��������� ��������� �������� �� ��������������� ���������� ������� 

    uint8_t  getCurrentCountMessage();
    void setCurrentCountMessage(uint8_t count_cur);

    uint8_t  getNumber_from_Message();
    void setNumber_from_Message(uint8_t count_cur);

    uint8_t  getFlippingCountMessage();
    void setFlippingCountMessage(uint8_t count_cur);

    bool getNewMessageFlag();
    void setNewMessageFlag(bool new_flag);

    bool GetButtonRetention() { return ButtonRetention; }         // �������� ���� ����������� ������� ������
    void SetButtonRetention(bool val);                            // �������� ���� ����������� ������� ������

    void saveVer(String ver);
    String getVer();
    String Current_version;
    uint8_t  air_msg[Number_of_bytes_msg][MAX_TRACKING_OBJECTS];  // ������ ���������� ��������� ������ � ����� ��������
private:

    bool empty_buffer = false;
    bool ButtonRetention = false;

};
//--------------------------------------------------------------------------------------------------------------------------------
extern SettingsClass SettingsMain;
