#pragma once

#include <Arduino.h>


class SettingsClass
{

public:

	SettingsClass();

	void setup();
	void update();
	void draw();
	long getCurrentLatitude();
	void setCurrentLatitude(long lat);
	long getLCurrentLongitude();
	void setLCurrentLongitude(long lon);
	int countTest();
	bool checData() { return dataisValid; };
	bool checSat() { return satisValid; };

	void setNumSat(uint8_t n_sat);
	uint8_t getNumSat();

	void setAltitude(float alt);
	float getAltitude();

	void setnavSystem(char navSys);
	char getnavSystem();

	void saveVer(String ver);
	String getVer();
	uint32_t DevID_Mapper(uint32_t id);
	uint32_t ESP32_getChipId();

	long curLatitude;
	long curLongitude;
	float Altitude_m;
	char satnavSystem;
	bool dataisValid;
	bool satisValid;
	int count = 0;
	uint8_t num_sat = 0;
	String Current_version;


private:

	
};

extern SettingsClass Settings;