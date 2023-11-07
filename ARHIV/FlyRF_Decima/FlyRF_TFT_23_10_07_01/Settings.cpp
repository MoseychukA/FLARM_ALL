#include "Settings.h"
#include "TFTMenu.h"
#include <Wire.h>                 //
#include "Configuration_ESP32.h"  // Основные настройки программы
#include "AT24CX.h"
#include "Memory.h"


//============================================================

AT24CX mem;

#define FONT_HEIGHT(dc) dc->fontHeight(1)

SPIClass SDSPI(HSPI);


//--------------------------------------------------------------------------------------------------------------------------------
SettingsClass Settings;
//--------------------------------------------------------------------------------------------------------------------------------
SettingsClass::SettingsClass()
{
	voltage3V.raw      = 0;
    voltage5V.raw      = 0;
	voltage3V.voltage3 = 0;
    voltage5V.voltage5 = 0;

}


//--------------------------------------------------------------------------------------------------------------------------------

int SettingsClass::SettingsClass::GetControllerID(const char* passedUUID)
{
	//byte corr_data = mem.read(ControllerID_ADDRESS);
    uint16_t result = 50;

   /* if (corr_data != CORRECT_DATA)
    {
        result = 0;
    }*/

    return result;
}
//--------------------------------------------------------------------------------------------------------------------------------
void SettingsClass::SetControllerID(uint16_t val)
{
    if (val < 1)
    {
        val = DEFAULT_ControllerID;
    }
    //mem.write(ControllerID_ADDRESS, CORRECT_DATA);
    //mem.writeInt(ControllerID_ADDRESS + 2, val);
}

uint32_t SettingsClass::DevID_Mapper(uint32_t id)
{
    uint8_t id_mask = (id & 0x00FF0000UL) >> 16;

    switch (id_mask)
    {
        /* переназначить адрес, чтобы избежать перекрытия с перегруженным диапазоном FLARM */
    case 0xD0:
    case 0xDD:
    case 0xDE:
    case 0xDF:
        id += 0x100000;
        break;
        /* переназначить адреса 11xxxx, чтобы избежать пересечения с перегруженным диапазоном Skytraxx */
    case 0x11:
        /*
         * OGN 0.2.8+ не декодирует трафик Air V6, когда ведущий байт 24-битного идентификатора равен 0x5B.
         */
    case 0x5B:
        id += 0x010000;
        break;

    default:
        break;
    }
    return id;
}



uint32_t SettingsClass::ESP32_getChipId()
{
#if !defined(SOFTRF_ADDRESS)

    /*!! Пока не применяем */
    /*uint32_t id = (uint32_t)efuse_mac[5] | ((uint32_t)efuse_mac[4] << 8) | \
        ((uint32_t)efuse_mac[3] << 16) | ((uint32_t)efuse_mac[2] << 24);*/

        /* Используем Chip ID только три старших байта*/
    uint32_t id = ESP.getEfuseMac() >> 24 & 0xFFFFFF;
 
    return DevID_Mapper(id);      // Переназначаем ID при условиях, указанных в DevID_Mapper.
#else
    return (SOFTRF_ADDRESS & 0xFFFFFFFFU);
#endif /* SOFTRF_ADDRESS */
}



void SettingsClass::turnPowerOff()
{
     // выключаем питание контроллера
   // digitalWrite(POWER_ON_OUT, LOW);
}
//--------------------------------------------------------------------------------------------------------------------------------
PowerType SettingsClass::getPowerType()
{
    return powerType;
}
//--------------------------------------------------------------------------------------------------------------------------------
void SettingsClass::checkPower() // Определение от какого источника питается прибор. Определяется в момент включения прибора.
{
   /* if (!digitalRead(POWER_ON_IN))
    {
        Settings.powerType = batteryPower;
		DBGLN("");
        DBGLN(F("BATTERY POWER !!!"));
    }
    else
    {
        Settings.powerType = powerViaUSB;
		DBGLN("");
        DBGLN(F("POWER  VIA USB !!!"));
    }*/
}
//--------------------------------------------------------------------------------------------------------------------------------
void SettingsClass::displayBacklight(bool bOn)
{
  /*  digitalWrite(LCD_LED, bOn ? LOW : HIGH);
    backlightFlag = bOn;*/
}
//--------------------------------------------------------------------------------------------------------------------------------
uint16_t SettingsClass::getPowerVoltage5(uint16_t pin) // Контроль напряжения питания внутренних источников (аккумуляторов).
{
    for (int i = 0; i < 20; i++)
    {
        voltage5V.raw += analogRead(pin);
    }
    voltage5V.raw = voltage5V.raw/20;

	dimension_array[array_count] = voltage5V.raw;
	array_count++;
	int val_voltage = 0;
	if (array_count > array_size)                    // проверка заполнения массива первичными данными о уровне напряжения аккумулятора
	{
		array_count = 0;
		array_countMax = true;                        //Разрешить выдавать данные об уровне напряжения аккумулятора
	}

	sum = 0;                                         //

	if (array_countMax)                              // формируем данные об уровне напряжения аккумулятора
	{
		for (int i = 0; i < array_size; i++)
		{
			sum += dimension_array[i];
		}
		val_voltage = sum / array_size;
	}
	else
	{
		for (int i = 0; i < array_count; i++)       //формируем первичные (заполняем массив) данные об уровне напряжения аккумулятора
		{
			sum += dimension_array[array_count - 1];
		}
		val_voltage = sum / array_count;
	}
	// Serial.print("voltage Bat - ");
	//Serial.println(val_voltage);

	voltage5V.voltage5 = map(val_voltage, 1060, 2150, 10, 230);
	if (voltage5V.voltage5 > 230) voltage5V.voltage5 = 230;  // Напряжение питания  

    return voltage5V.voltage5;
}



//--------------------------------------------------------------------------------------------------------------------------------------
uint8_t SettingsClass::read8(uint16_t address, uint8_t defaultVal)
{
   
    uint8_t curVal = mem.read(address);
    if (curVal == 0xFF)
        curVal = defaultVal;

    return curVal;
}

//--------------------------------------------------------------------------------------------------------------------------------------
uint16_t SettingsClass::read16(uint16_t address, uint16_t defaultVal)
{
    uint16_t val = 0;
    byte* b = (byte*)&val;

    for (byte i = 0; i < 2; i++)
        *b++ = mem.read(address + i);

    if (val == 0xFFFF)
        val = defaultVal;

    return val;
}
//--------------------------------------------------------------------------------------------------------------------------------------
void SettingsClass::write16(uint16_t address, uint16_t val)
{
    byte* b = (byte*)&val;

    for (byte i = 0; i < 2; i++)
        mem.write(address + i, *b++);

}
//--------------------------------------------------------------------------------------------------------------------------------------
unsigned long SettingsClass::read32(uint16_t address, unsigned long defaultVal)
{
    unsigned long val = 0;
    byte* b = (byte*)&val;

    for (byte i = 0; i < 4; i++)
        *b++ = mem.read(address + i);

    if (val == 0xFFFFFFFF)
        val = defaultVal;

    return val;
}
//--------------------------------------------------------------------------------------------------------------------------------------
void SettingsClass::write32(uint16_t address, unsigned long val)
{
    byte* b = (byte*)&val;

    for (byte i = 0; i < 4; i++)
        mem.write(address + i, *b++);
}
//--------------------------------------------------------------------------------------------------------------------------------------
String SettingsClass::readString(uint16_t address, byte maxlength)
{
    String result;
    Serial.println("readString ..");
    for (byte i = 0; i < maxlength; i++)
    {
        byte b = mem.read(address++);
        if (b == 0/*'\0'*/)
        {
            Serial.print("break ..");
            Serial.println((char)b, HEX);
            Serial.print("address ..");
            Serial.println(address);
            break;
        }
 
        result += (char)b;
 		Serial.print((char)b);

    }
 
	//Serial.println(result);
    Serial.println();
    return result;
}
//--------------------------------------------------------------------------------------------------------------------------------------
void SettingsClass::writeString(uint16_t address, const String& v, byte maxlength)
{
    byte val = v.length();
    Serial.println("writeString ..");
    for (byte i = 0; i < maxlength; i++)
    {
        if (i >= v.length())
            break;

		mem.write(address++, v[i]);
		Serial.print(v[i]);
    }
    // пишем завершающий ноль
	mem.write(address + v.length()+1, 0/*'\0'*/);
 /*   Serial.print("address ..");
    Serial.println(address);*/
    Serial.println();
}

//--------------------------------------------------------------------------------------------------------------------------------------
bool SettingsClass::GetWiFiState()
{
    wifiState = mem.read(WIFI_STATE_EEPROM_ADDR);
    return wifiState;
}
//--------------------------------------------------------------------------------------------------------------------------------------
void SettingsClass::SetWiFiState(bool st)
{
    wifiState = st;
	mem.write(WIFI_STATE_EEPROM_ADDR, st);
}

//--------------------------------------------------------------------------------------------------------------------------------------
bool SettingsClass::GetWiFiConnect()
{
    return wifiConnect;
}
//--------------------------------------------------------------------------------------------------------------------------------------
void SettingsClass::SetWiFiConnect(bool WiFiOn)
{
    wifiConnect = WiFiOn;
}


//--------------------------------------------------------------------------------------------------------------------------------------
String SettingsClass::GetStationPassword()
{
 
    byte v_length = byte(mem.read(STATION_PASSWORD_EEPROM_ADDR + 1));
	stationPassword = readString(STATION_PASSWORD_EEPROM_ADDR + 2, 20/*v_length*/);
    return stationPassword;
}
//--------------------------------------------------------------------------------------------------------------------------------------
void SettingsClass::SetStationPassword(const String& v)
{
   // stationPassword = v;

	mem.write(STATION_PASSWORD_EEPROM_ADDR, CORRECT_DATA);
	mem.write(STATION_PASSWORD_EEPROM_ADDR + 1, v.length());
	writeString(STATION_PASSWORD_EEPROM_ADDR + 2, v, 20/*v_length*/);
}
//--------------------------------------------------------------------------------------------------------------------------------------
String SettingsClass::GetStationID()
{

    byte v_length = byte(mem.read(STATION_ID_EEPROM_ADDR + 1));
	stationID = readString(STATION_ID_EEPROM_ADDR + 2, 20/*v_length*/);

    return stationID;
}
//--------------------------------------------------------------------------------------------------------------------------------------
void SettingsClass::SetStationID(const String& v)
{

	mem.write(STATION_ID_EEPROM_ADDR, CORRECT_DATA);
	mem.write(STATION_ID_EEPROM_ADDR+1, v.length());
    writeString(STATION_ID_EEPROM_ADDR + 2, v, 20/*v_length*/);
}
//--------------------------------------------------------------------------------------------------------------------------------------

String SettingsClass::GetRouterSSID()
{

  


	routerSSID = readString(ROUTER_ID_EEPROM_ADDR + 2, 20/*v_length*/);
    return routerSSID;
}
//--------------------------------------------------------------------------------------------------------------------------------------
void SettingsClass::SetRouterSSID(const String& val)
{
    //routerID = val;
    byte len = val.length();
    writeString(ROUTER_ID_EEPROM_ADDR + 2, val, 20/*len*/);
	
}
//--------------------------------------------------------------------------------------------------------------------------------------
String SettingsClass::GetRouterPassword()
{

   byte v_length = byte(mem.read(ROUTER_PASSWORD_EEPROM_ADDR + 1));
   routerPassword = readString(ROUTER_PASSWORD_EEPROM_ADDR + 2, 20/*v_length*/);
   return routerPassword;
}
//--------------------------------------------------------------------------------------------------------------------------------------
void SettingsClass::SetRouterPassword(const String& v)
{
    Serial.print("Password1 =  ");
    Serial.println(v);
    byte len = v.length();
    writeString(ROUTER_PASSWORD_EEPROM_ADDR + 2, v, 20/*len*/);

}
//--------------------------------------------------------------------------------------------------------------------------------------




//--------------------------------------------------------------------------------------------------------------------------------
void SettingsClass::setup()
{

 

    pinMode(left_button, INPUT_PULLUP);
    pinMode(right_button, INPUT_PULLUP);
}
//--------------------------------------------------------------------------------------------------------------------------------
void SettingsClass::update()
{

    TFTMenu* menuManager;
  

    static uint32_t tmr = millis();
    if (millis() - tmr > 1000)
    {
        //int Power5 = Settings.getPowerVoltage5(POWER_BATTERY);
		//int Power3 = Settings.getPowerVoltage3(POWER_RTC);
        tmr = millis();
    }

}
//--------------------------------------------------------------------------------------------------------------------------------



//--------------------------------------------------------------------------------------------------------------------------------
void SettingsClass::TestI2C() 
{
    byte error, address;
    int nDevices;

    Serial.println("Scanning...");

    nDevices = 0;
    for (address = 1; address < 127; address++) {
        // The i2c_scanner uses the return value of
        // the Write.endTransmisstion to see if
        // a device did acknowledge to the address.
        Serial.print("address ");
        Serial.println(address);

        Wire.beginTransmission(address);
        error = Wire.endTransmission();

        if (error == 0) {
            Serial.print("I2C device found at address 0x");
            if (address < 16)
                Serial.print("0");
            Serial.println(address, HEX);

            nDevices++;
        }
        else if (error == 4) {
            Serial.print("Unknown error at address 0x");
            if (address < 16)
                Serial.print("0");
            Serial.println(address, HEX);
        }
    }
    if (nDevices == 0)
        Serial.println("No I2C devices found");
    else
        Serial.println("done");

    //delay(5000);           // wait 5 seconds for next scan
}
//--------------------------------------------------------------------------------------------------------------------------------
