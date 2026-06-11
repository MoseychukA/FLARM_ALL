#include "SettingsMain.h"
#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>
#include <Arduino.h>
#include "Configuration_ESP32.h"

//--------------------------------------------------------------------------------------------------------------------------------
SettingsClass SettingsMain;
//--------------------------------------------------------------------------------------------------------------------------------
SettingsClass::SettingsClass()
{


}

//--------------------------------------------------------------------------------------------------------------------------------
void SettingsClass::setup()
{

	pinMode(BUTTON_MAIL, INPUT_PULLUP);       // Настроить кнопку "POINT"

}

void SettingsClass::update()
{
	

}

//--------------------------------------------------------------------------------------------------------------------------------
bool SettingsClass::getNewMessageFlag()
{
	return new_msg_flag;  // получить флаг нового сообщения
}

void SettingsClass::setNewMessageFlag(bool new_flag)
{
	new_msg_flag = new_flag; 	// Сохранить флаг нового сообщения
}


//--------------------------------------------------------------------------------------------------------------------------------
void SettingsClass::saveVer(String ver)
{
	Current_version = ver;
}

String SettingsClass::getVer()
{
	return Current_version;
}

//--------------------------------------------------------------------------------------------------------------------------------
void SettingsClass::saveMSG(String msg_save)
{
	Current_MSG = msg_save;
}

String SettingsClass::getMSG()
{
	return Current_MSG;
}


//// --------------------------------------------------------------------------------------------------------------------------------
//uint8_t  SettingsClass::getNumber_from_Message()
//{
//
//	return number_from_message; 	// Получить текущий номер сообщения
//}
//
//void SettingsClass::setNumber_from_Message(uint8_t count_cur)
//{
//	number_from_message = count_cur; 	// Сохранить текущий номер сообщения
//}


//--------------------------------------------------------------------------------------------------------------------------------