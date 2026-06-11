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


}

//--------------------------------------------------------------------------------------------------------------------------------
void SettingsClass::setup()
{

	pinMode(BUTTON_MAIL, INPUT_PULLUP);       // Настроить кнопку "POINT"

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
/* Получить состояние счетчика не подтвержденных сообщений */
uint8_t SettingsClass::getCoutNotReadMessage()
{
	// Получить количество полученных сообщений
	uint8_t mess_count = settings->CountNotReadMessage;  
	return mess_count;
}

/* Сохранить в памяти количество не подтвержденных сообщений */
void SettingsClass::setCoutNotReadMessage(uint8_t count)
{
	// Сохранить количество полученных сообщений
	settings->CountNotReadMessage = count;  // сохранить состояние счетчика не прочитанного количества записей 
	EEPROM_store();
}

//2
//--------------------------------------------------------------------------------------------------------------------------------
uint8_t  SettingsClass::getCurrentCountMessage()
{
	// Получить текущий номер сообщения
	uint8_t current_count_mess = settings->CurrentCountMessage;   // получить состояние текущего счетчика
	return current_count_mess;
}

void SettingsClass::setCurrentCountMessage(uint8_t count_cur)
{
	// Сохранить текущий номер сообщения
	settings->CurrentCountMessage = count_cur;  // сохранить текущее состояние счетчика
	EEPROM_store();
}

//--------------------------------------------------------------------------------------------------------------------------------
bool SettingsClass::getNewMessageFlag()
{
	// Получить текущий номер сообщения  Message_Not_Confirmed_flag

	bool new_flag = settings->Message_Not_Confirmed_flag; // получить флаг нового сообщения
	return new_flag;
}

void SettingsClass::setNewMessageFlag(bool new_flag)
{
	// Сохранить текущий номер сообщения
	settings->Message_Not_Confirmed_flag = new_flag;      // сохранить флаг нового сообщения
	EEPROM_store();

}

//--------------------------------------------------------------------------------------------------------------------------------
void SettingsClass::SetButtonRetention(bool val) // Записать
{
	ButtonRetention = val;
}

//--------------------------------------------------------------------------------------------------------------------------------
/*  Программы поддержки функции листания сообщений*/
//--------------------------------------------------------------------------------------------------------------------------------
uint8_t SettingsClass::getFlippingCountMessage()
{
	// Получить текущий номер сообщения
	uint8_t current_count_mess;//!! = MemRead(Flipping_Counter_Message); // получить текущее состояние положения счетчика
	return current_count_mess;
}

void SettingsClass::setFlippingCountMessage(uint8_t count_cur)
{
	// Сохранить текущий номер сообщения
	//!!MemWrite(Flipping_Counter_Message, count_cur);      // сохранить текущее положение счетчика
}
