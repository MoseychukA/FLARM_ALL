#include "Settings.h"
//#include <malloc.h>
//#include <stdlib.h>
//#include <stdio.h>
//#include <Arduino.h>
#include "Configuration_ESP32.h"

//--------------------------------------------------------------------------------------------------------------------------------
SettingsClass Settings;
//--------------------------------------------------------------------------------------------------------------------------------
SettingsClass::SettingsClass()
{
	canUseBlinkerMessage = true;
	BlinkerReadyGreen = true;
	BlinkerReadyRed = true;
}


//--------------------------------------------------------------------------------------------------------------------------------
SettingsClass Settings;
//--------------------------------------------------------------------------------------------------------------------------------
SettingsClass::SettingsClass()
{
	//canUseBlinkerMessage = true;
	//BlinkerReadyGreen = true;
	//BlinkerReadyRed = true;
}

//--------------------------------------------------------------------------------------------------------------------------------

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

