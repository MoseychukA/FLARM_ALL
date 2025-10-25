#include "ServiceMain.h"
#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>
#include <Arduino.h>
#include "Configuration_ESP32.h"

//--------------------------------------------------------------------------------------------------------------------------------
ServiceClass service;
//--------------------------------------------------------------------------------------------------------------------------------
ServiceClass::ServiceClass()
{


}

//--------------------------------------------------------------------------------------------------------------------------------
void ServiceClass::setup()
{
	//pinMode(BUTTON_MAIL, INPUT_PULLUP);       // Настроить кнопку "POINT"
}

void ServiceClass::update()
{


}

//--------------------------------------------------------------------------------------------------------------------------------
bool ServiceClass::getNewMessageFlag()
{
	return new_msg_flag;  // получить флаг нового сообщения
}

void ServiceClass::setNewMessageFlag(bool new_flag)
{
	new_msg_flag = new_flag; 	// Сохранить флаг нового сообщения
}


//--------------------------------------------------------------------------------------------------------------------------------
void ServiceClass::saveVer(String ver)
{
	Current_version += ver;
}

String ServiceClass::getVer()
{
	//Serial.print("***version Service - ");
 //   Serial.println(Current_version);
	return Current_version;
}

//--------------------------------------------------------------------------------------------------------------------------------
void ServiceClass::saveMSG(String msg_save)
{
	Current_MSG = msg_save;
}

String ServiceClass::getMSG()
{
	return Current_MSG;
}

void ServiceClass::setMail_on(bool On_Off)
{
	MailOn = On_Off;
}

bool ServiceClass::getMail_on()
{
	return MailOn;
}

void ServiceClass::setMessageRead(bool On_Message)
{
	MessageRead = On_Message;
}

bool ServiceClass::getMessageRead()
{
	return MessageRead;
}

void ServiceClass::setAllow_flashing(bool On_Off)
{
	flashing = On_Off;
}

bool ServiceClass::getAllow_flashing()
{
	return flashing;
}

void ServiceClass::setMailOn(bool On_Off)
{
	flags_MailOn = On_Off;
}

bool ServiceClass::getMailOn()
{
	return flags_MailOn;
}

void ServiceClass::set_confirm_message(bool On_Off)
{
	confirm_message = On_Off;
}

bool ServiceClass::get_confirm_message()
{
	return confirm_message;
}


void ServiceClass::setClearMessage(bool On_Off)
{
	ClearMessage = On_Off;
}

bool ServiceClass::get_ClearMessage()
{
	return ClearMessage;
}


void ServiceClass::setClearButton(bool On_Off)
{
	ClearButton = On_Off;
}

bool ServiceClass::get_ClearButton()
{
	return ClearButton;
}

void ServiceClass::set_count_buttton(uint8_t button)
{
	count_buttton = button;
}

uint8_t ServiceClass::get_count_buttton()
{
	return count_buttton;
}

void ServiceClass::set_num_buttton(uint8_t button)
{
	num_buttton = button;
}

uint8_t ServiceClass::get_num_buttton()
{
	return num_buttton;
}


void  ServiceClass::set_analog_value(uint16_t value)
{
	analog_value = value;

}
uint16_t  ServiceClass::get_analog_value()
{
	return analog_value;

}

void ServiceClass::set_GNSS_on_off(bool GNSSOn_Off)
{
	GNSS_On_Off = GNSSOn_Off;

}
bool ServiceClass::get_GNSS_on_off()
{

	return GNSS_On_Off;
}

void ServiceClass::set_SOS_on_off(bool SOSOn)
{
	SOSOn_Off = SOSOn;

}
bool ServiceClass::get_SOS_on_off()
{

	return SOSOn_Off;
}

void ServiceClass::set_time_hour(uint8_t hour)
{
	hour_m = hour;
}

uint8_t ServiceClass::get_time_hour()
{
	return hour_m;
}

void ServiceClass::set_time_minute(uint8_t minute)
{
	minute_m = minute;
}

uint8_t ServiceClass::get_time_minute()
{
	return minute_m;
}



//--------------------------------------------------------------------------------------------------------------------------------

