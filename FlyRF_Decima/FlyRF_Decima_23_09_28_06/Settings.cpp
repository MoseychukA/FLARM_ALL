#include "Settings.h"
#include "TFTMenu.h"
#include <Wire.h>                 //
#include "Configuration_ESP32.h"  // Основные настройки программы
#include "AT24CX.h"
#include "Memory.h"

/*!! Добавил*/
#include "ESP32.h"
static union {
    uint8_t efuse_mac[6];
    uint64_t chipmacid;
};


//============================================================

AT24CX mem;

#define FONT_HEIGHT(dc) dc->fontHeight(1)

#ifdef HAS_DISPLAY
#include <U8g2lib.h>

#ifndef DISPLAY_MODEL
#define DISPLAY_MODEL U8G2_SSD1306_128X64_NONAME_F_HW_I2C
#endif

DISPLAY_MODEL* u8g2 = nullptr;
#endif

#ifndef OLED_WIRE_PORT
#define OLED_WIRE_PORT Wire
#endif


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
        /* remap address to avoid overlapping with congested FLARM range */
    case 0xD0:
    case 0xDD:
    case 0xDE:
    case 0xDF:
        id += 0x100000;
        break;
        /* remap 11xxxx addresses to avoid overlapping with congested Skytraxx range */
    case 0x11:
        /*
         * OGN 0.2.8+ does not decode 'Air V6' traffic when leading byte of 24-bit Id is 0x5B
         */
    case 0x5B:
        id += 0x010000;
        break;

    default:
        break;
    }
    Serial.print("ChipId = ");
    Serial.println(id); 
    return id;
}


uint32_t SettingsClass::ESP32_getChipId()
{
#if !defined(SOFTRF_ADDRESS)
    uint32_t id = (uint32_t)efuse_mac[5] | ((uint32_t)efuse_mac[4] << 8) | \
        ((uint32_t)efuse_mac[3] << 16) | ((uint32_t)efuse_mac[2] << 24);

   /* uint32_t id = DevID_Mapper(id1);
    Serial.print("ChipId = ");
    Serial.println(id);*/
    return DevID_Mapper(id);
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

void SettingsClass::initBoard()
{
    Serial.begin(115200);
    Serial.println("initBoard");
    SPI.begin(RADIO_SCLK_PIN, RADIO_MISO_PIN, RADIO_MOSI_PIN);

    Wire.begin(I2C_SDA, I2C_SCL);


    pinMode(LCD_Led, OUTPUT);
    digitalWrite(LCD_Led, HIGH);  // Включить подсветку дисплея TFT

#ifdef HAS_GPS
    Serial1.begin(GPS_BAUD_RATE, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
#endif


    initPMU();


#ifdef BOARD_LED
    /*
    * T-BeamV1.0, V1.1 LED defaults to low level as trun on,
    * so it needs to be forced to pull up
    * * * * */
#if LED_ON == LOW
    gpio_hold_dis(GPIO_NUM_4);
#endif
    pinMode(BOARD_LED, OUTPUT);
    digitalWrite(BOARD_LED, LED_ON);
#endif


#ifdef HAS_DISPLAY
    Wire.beginTransmission(0x3C);
    if (Wire.endTransmission() == 0) {
        Serial.println("Started OLED");
        u8g2 = new DISPLAY_MODEL(U8G2_R0, U8X8_PIN_NONE);
        u8g2->begin();
        u8g2->clearBuffer();
        u8g2->setFlipMode(0);
        u8g2->setFontMode(1); // Transparent
        u8g2->setDrawColor(1);
        u8g2->setFontDirection(0);
        u8g2->firstPage();
        do {
            u8g2->setFont(u8g2_font_inb19_mr);
            u8g2->drawStr(0, 30, "Decima");
            u8g2->drawHLine(2, 35, 47);
            u8g2->drawHLine(3, 36, 47);
            u8g2->drawVLine(45, 32, 12);
            u8g2->drawVLine(46, 33, 12);
            u8g2->setFont(u8g2_font_inb19_mf);
            u8g2->drawStr(58, 60, "RF");
        } while (u8g2->nextPage());
        u8g2->sendBuffer();
        u8g2->setFont(u8g2_font_fur11_tf);
        delay(3000);
    }
#endif
}




#if defined(HAS_PMU)
#include "XPowersLib.h"


XPowersLibInterface* PMU = NULL;

#ifndef PMU_WIRE_PORT
#define PMU_WIRE_PORT   Wire
#endif


bool pmuInterrupt;


void setPmuFlag()
{
    pmuInterrupt = true;
}


bool SettingsClass::initPMU()
{
    if (!PMU) {
        PMU = new XPowersAXP2101(PMU_WIRE_PORT);
        if (!PMU->init())
        {
            Serial.println("Warning: Failed to find AXP2101 power management");
            delete PMU;
            PMU = NULL;
        }
        else
        {
            Serial.println("AXP2101 PMU init succeeded, using AXP2101 PMU");
        }
    }

    if (!PMU)
    {
        return false;
    }

    PMU->setChargingLedMode(XPOWERS_CHG_LED_BLINK_1HZ);

    pinMode(PMU_IRQ, INPUT_PULLUP);
    attachInterrupt(PMU_IRQ, setPmuFlag, FALLING);

    //if (PMU->getChipModel() == XPOWERS_AXP192)
    //{

    //    PMU->setProtectedChannel(XPOWERS_DCDC3);

    //    // lora
    //    PMU->setPowerChannelVoltage(XPOWERS_LDO2, 3300);
    //    // gps
    //    PMU->setPowerChannelVoltage(XPOWERS_LDO3, 3300);
    //    // oled
    //    PMU->setPowerChannelVoltage(XPOWERS_DCDC1, 3300);

    //    PMU->enablePowerOutput(XPOWERS_LDO2);
    //    PMU->enablePowerOutput(XPOWERS_LDO3);

    //    //protected oled power source
    //    PMU->setProtectedChannel(XPOWERS_DCDC1);
    //    //protected esp32 power source
    //    PMU->setProtectedChannel(XPOWERS_DCDC3);
    //    // enable oled power
    //    PMU->enablePowerOutput(XPOWERS_DCDC1);

    //    //disable not use channel
    //    PMU->disablePowerOutput(XPOWERS_DCDC2);

    //    PMU->disableIRQ(XPOWERS_AXP192_ALL_IRQ);

    //    PMU->enableIRQ(XPOWERS_AXP192_VBUS_REMOVE_IRQ |
    //        XPOWERS_AXP192_VBUS_INSERT_IRQ |
    //        XPOWERS_AXP192_BAT_CHG_DONE_IRQ |
    //        XPOWERS_AXP192_BAT_CHG_START_IRQ |
    //        XPOWERS_AXP192_BAT_REMOVE_IRQ |
    //        XPOWERS_AXP192_BAT_INSERT_IRQ |
    //        XPOWERS_AXP192_PKEY_SHORT_IRQ
    //    );

    //}
    //else 
    if (PMU->getChipModel() == XPOWERS_AXP2101)
    {

        //Unuse power channel
        PMU->disablePowerOutput(XPOWERS_DCDC2);
        PMU->disablePowerOutput(XPOWERS_DCDC3);
        PMU->disablePowerOutput(XPOWERS_DCDC4);
        PMU->disablePowerOutput(XPOWERS_DCDC5);
        // PMU->disablePowerOutput(XPOWERS_ALDO1);
        PMU->disablePowerOutput(XPOWERS_ALDO4);
        PMU->disablePowerOutput(XPOWERS_BLDO1);
        PMU->disablePowerOutput(XPOWERS_BLDO2);
        PMU->disablePowerOutput(XPOWERS_DLDO1);
        PMU->disablePowerOutput(XPOWERS_DLDO2);

        // GNSS RTC PowerVDD 3300mV
        PMU->setPowerChannelVoltage(XPOWERS_VBACKUP, 3300);
        PMU->enablePowerOutput(XPOWERS_VBACKUP);

        //ESP32 VDD 3300mV
        // ! No need to set, automatically open , Don't close it
        // PMU->setPowerChannelVoltage(XPOWERS_DCDC1, 3300);
        // PMU->setProtectedChannel(XPOWERS_DCDC1);
        PMU->setProtectedChannel(XPOWERS_DCDC1);

        // NEO-6M  батарейка ms621fe 3000mV
        PMU->setPowerChannelVoltage(XPOWERS_ALDO1, 3000);   // 
        PMU->enablePowerOutput(XPOWERS_ALDO1);

        // LoRa VDD 3300mV
        PMU->setPowerChannelVoltage(XPOWERS_ALDO2, 3300);
        PMU->enablePowerOutput(XPOWERS_ALDO2);

        //GNSS VDD 3300mV
        PMU->setPowerChannelVoltage(XPOWERS_ALDO3, 3300);
        PMU->enablePowerOutput(XPOWERS_ALDO3);

    }

    PMU->enableSystemVoltageMeasure();
    PMU->enableVbusVoltageMeasure();
    PMU->enableBattVoltageMeasure();
    // It is necessary to disable the detection function of the TS pin on the board
    // without the battery temperature detection function, otherwise it will cause abnormal charging
    PMU->disableTSPinMeasure();

    Serial.printf("=========================================\n");
    if (PMU->isChannelAvailable(XPOWERS_DCDC1)) {
        Serial.printf("DC1  : %s   Voltage: %04u mV \n", PMU->isPowerChannelEnable(XPOWERS_DCDC1) ? "+" : "-", PMU->getPowerChannelVoltage(XPOWERS_DCDC1));
    }
    if (PMU->isChannelAvailable(XPOWERS_DCDC2)) {
        Serial.printf("DC2  : %s   Voltage: %04u mV \n", PMU->isPowerChannelEnable(XPOWERS_DCDC2) ? "+" : "-", PMU->getPowerChannelVoltage(XPOWERS_DCDC2));
    }
    if (PMU->isChannelAvailable(XPOWERS_DCDC3)) {
        Serial.printf("DC3  : %s   Voltage: %04u mV \n", PMU->isPowerChannelEnable(XPOWERS_DCDC3) ? "+" : "-", PMU->getPowerChannelVoltage(XPOWERS_DCDC3));
    }
    if (PMU->isChannelAvailable(XPOWERS_DCDC4)) {
        Serial.printf("DC4  : %s   Voltage: %04u mV \n", PMU->isPowerChannelEnable(XPOWERS_DCDC4) ? "+" : "-", PMU->getPowerChannelVoltage(XPOWERS_DCDC4));
    }
    if (PMU->isChannelAvailable(XPOWERS_DCDC5)) {
        Serial.printf("DC5  : %s   Voltage: %04u mV \n", PMU->isPowerChannelEnable(XPOWERS_DCDC5) ? "+" : "-", PMU->getPowerChannelVoltage(XPOWERS_DCDC5));
    }
    if (PMU->isChannelAvailable(XPOWERS_LDO2)) {
        Serial.printf("LDO2 : %s   Voltage: %04u mV \n", PMU->isPowerChannelEnable(XPOWERS_LDO2) ? "+" : "-", PMU->getPowerChannelVoltage(XPOWERS_LDO2));
    }
    if (PMU->isChannelAvailable(XPOWERS_LDO3)) {
        Serial.printf("LDO3 : %s   Voltage: %04u mV \n", PMU->isPowerChannelEnable(XPOWERS_LDO3) ? "+" : "-", PMU->getPowerChannelVoltage(XPOWERS_LDO3));
    }
    if (PMU->isChannelAvailable(XPOWERS_ALDO1)) {
        Serial.printf("ALDO1: %s   Voltage: %04u mV \n", PMU->isPowerChannelEnable(XPOWERS_ALDO1) ? "+" : "-", PMU->getPowerChannelVoltage(XPOWERS_ALDO1));
    }
    if (PMU->isChannelAvailable(XPOWERS_ALDO2)) {
        Serial.printf("ALDO2: %s   Voltage: %04u mV \n", PMU->isPowerChannelEnable(XPOWERS_ALDO2) ? "+" : "-", PMU->getPowerChannelVoltage(XPOWERS_ALDO2));
    }
    if (PMU->isChannelAvailable(XPOWERS_ALDO3)) {
        Serial.printf("ALDO3: %s   Voltage: %04u mV \n", PMU->isPowerChannelEnable(XPOWERS_ALDO3) ? "+" : "-", PMU->getPowerChannelVoltage(XPOWERS_ALDO3));
    }
    if (PMU->isChannelAvailable(XPOWERS_ALDO4)) {
        Serial.printf("ALDO4: %s   Voltage: %04u mV \n", PMU->isPowerChannelEnable(XPOWERS_ALDO4) ? "+" : "-", PMU->getPowerChannelVoltage(XPOWERS_ALDO4));
    }
    if (PMU->isChannelAvailable(XPOWERS_BLDO1)) {
        Serial.printf("BLDO1: %s   Voltage: %04u mV \n", PMU->isPowerChannelEnable(XPOWERS_BLDO1) ? "+" : "-", PMU->getPowerChannelVoltage(XPOWERS_BLDO1));
    }
    if (PMU->isChannelAvailable(XPOWERS_BLDO2)) {
        Serial.printf("BLDO2: %s   Voltage: %04u mV \n", PMU->isPowerChannelEnable(XPOWERS_BLDO2) ? "+" : "-", PMU->getPowerChannelVoltage(XPOWERS_BLDO2));
    }

    if (PMU->isVbusIn()) {
        Serial.printf("Vbus:      Voltage: %04u mV \n", PMU->getVbusVoltage());
    }
    //Serial.print("VbusVoltage:"); Serial.print(PMU->getVbusVoltage()); Serial.println("mV");
    //Serial.print("VbusCurrentLimit:"); Serial.print(PMU->getVbusCurrentLimit()); Serial.println();
    //Serial.print("VbusVoltageLimit:"); Serial.print(PMU->getVbusVoltageLimit()); Serial.println();

    if (PMU->isBatteryConnect()) {
        Serial.print("getBatteryPercent:"); Serial.print(PMU->getBatteryPercent()); Serial.println("%");
    }

    Serial.printf("=========================================\n");


    // Set the time of pressing the button to turn off
    PMU->setPowerKeyPressOffTime(XPOWERS_POWEROFF_4S);
    uint8_t opt = PMU->getPowerKeyPressOffTime();
    Serial.print("PowerKeyPressOffTime:");
    switch (opt) {
    case XPOWERS_POWEROFF_4S: Serial.println("4 Second");
        break;
    case XPOWERS_POWEROFF_6S: Serial.println("6 Second");
        break;
    case XPOWERS_POWEROFF_8S: Serial.println("8 Second");
        break;
    case XPOWERS_POWEROFF_10S: Serial.println("10 Second");
        break;
    default:
        break;
    }

    return true;
}
#endif
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
