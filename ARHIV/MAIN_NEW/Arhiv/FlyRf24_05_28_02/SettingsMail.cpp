#include "SettingsMail.h"
//#include <malloc.h>
//#include <stdlib.h>
//#include <stdio.h>
//#include <Arduino.h>
#include "Configuration_ESP32.h"
#include "EEPROMRF.h"

//--------------------------------------------------------------------------------------------------------------------------------
SettingsClass SettingsMail;
//--------------------------------------------------------------------------------------------------------------------------------
SettingsClass::SettingsClass()
{
	//canUseBlinkerMessage = true;
	//BlinkerReadyGreen = true;
	//BlinkerReadyRed = true;
}

//--------------------------------------------------------------------------------------------------------------------------------
void SettingsClass::setup()
{



}

void SettingsClass::update()
{


}



bool  SettingsClass::get_empty_buffer_request()
{
	return empty_buffer;
}

void SettingsClass::set_empty_buffer_request(bool buffer_request)
{
	empty_buffer = buffer_request;
}

//1
//--------------------------------------------------------------------------------------------------------------------------------
/* ѕолучить состо€ние счетчика не подтвержденных сообщений */
uint8_t SettingsClass::getCoutNotReadMessage()
{
	// ѕолучить количество полученных сообщений
	uint8_t mess_count = settings->CountNotReadMessage;  
	return mess_count;
}

/* —охранить в пам€ти количество не подтвержденных сообщений */
void SettingsClass::setCoutNotReadMessage(uint8_t count)
{
	// —охранить количество полученных сообщений
	settings->CountNotReadMessage = count;  // сохранить состо€ние счетчика не прочитанного количества записей 
	EEPROM_store();
}

//2
//--------------------------------------------------------------------------------------------------------------------------------
uint8_t  SettingsClass::getCurrentCountMessage()
{
	// ѕолучить текущий номер сообщени€
	uint8_t current_count_mess = settings->CurrentCountMessage;   // получить состо€ние текущего счетчика
	return current_count_mess;
}

void SettingsClass::setCurrentCountMessage(uint8_t count_cur)
{
	// —охранить текущий номер сообщени€
	settings->CurrentCountMessage = count_cur;  // сохранить текущее состо€ние счетчика
	EEPROM_store();
}

//--------------------------------------------------------------------------------------------------------------------------------
bool SettingsClass::getNewMessageFlag()
{
	// ѕолучить текущий номер сообщени€  Message_Not_Confirmed_flag

	bool new_flag = settings->Message_Not_Confirmed_flag; // получить флаг нового сообщени€
	return new_flag;
}

void SettingsClass::setNewMessageFlag(bool new_flag)
{
	// —охранить текущий номер сообщени€
	settings->Message_Not_Confirmed_flag = new_flag;      // сохранить флаг нового сообщени€
	EEPROM_store();

}