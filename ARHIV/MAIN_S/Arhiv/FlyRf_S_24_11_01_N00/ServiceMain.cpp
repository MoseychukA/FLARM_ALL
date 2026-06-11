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
	pinMode(BUTTON_MAIL, INPUT_PULLUP);       // Настроить кнопку "POINT"
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

void ServiceClass::saveMail_on(bool On_Off)
{
	MailOn = On_Off;
}

bool ServiceClass::getMail_on()
{
	return MailOn;
}


//--------------------------------------------------------------------------------------------------------------------------------

