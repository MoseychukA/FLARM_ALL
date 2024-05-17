/*
  Read NMEA sentences over sERIAL using Ublox module SAM-M8Q, NEO-M8P, etc
  Base on SparkFun_Ublox_Arduino_Library //https://github.com/sparkfun/SparkFun_Ublox_Arduino_Library
*/


//#define XPOWERS_CHIP_AXP2102

#include <Wire.h>
#include <Arduino.h>
//#include "XPowersLib.h"



#include "SparkFun_Ublox_Arduino_Library.h"
#include "boards.h"

SFE_UBLOX_GPS myGPS;

#include <MicroNMEA.h> //https://github.com/stevemarple/MicroNMEA

char nmeaBuffer[100];
MicroNMEA nmea(nmeaBuffer, sizeof(nmeaBuffer));
int count = 0;


#define SerialGNSS Serial1

uint8_t buffer[256];

int getAck(uint8_t* buffer, uint16_t size, uint8_t requestedClass, uint8_t requestedID)
{
    uint16_t    ubxFrameCounter = 0;
    bool        ubxFrame = 0;
    uint32_t    startTime = millis();
    uint16_t    needRead;

    while (millis() - startTime < 800) {
        while (SerialGNSS.available()) {
            int c = SerialGNSS.read();
            switch (ubxFrameCounter) {
            case 0:
                if (c == 0xB5) {
                    ubxFrameCounter++;
                }
                break;
            case 1:
                if (c == 0x62) {
                    ubxFrameCounter++;
                }
                else {
                    ubxFrameCounter = 0;
                }
                break;
            case 2:
                if (c == requestedClass) {
                    ubxFrameCounter++;
                }
                else {
                    ubxFrameCounter = 0;
                }
                break;
            case 3:
                if (c == requestedID) {
                    ubxFrameCounter++;
                }
                else {
                    ubxFrameCounter = 0;
                }
                break;
            case 4:
                needRead = c;
                ubxFrameCounter++;
                break;
            case 5:
                needRead |= (c << 8);
                ubxFrameCounter++;
                break;
            case 6:
                if (needRead >= size) {
                    ubxFrameCounter = 0;
                    break;
                }
                if (SerialGNSS.readBytes(buffer, needRead) != needRead) {
                    ubxFrameCounter = 0;
                }
                else {
                    return needRead;
                }
                break;

            default:
                break;
            }
        }
    }
    return 0;
}

bool recovery()
{
    uint8_t cfg_clear1[] = { 0xB5, 0x62, 0x06, 0x09, 0x0D, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x1C, 0xA2 };
    uint8_t cfg_clear2[] = { 0xB5, 0x62, 0x06, 0x09, 0x0D, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x1B, 0xA1 };
    uint8_t cfg_clear3[] = { 0xB5, 0x62, 0x06, 0x09, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x03, 0x1D, 0xB3 };
    SerialGNSS.write(cfg_clear1, sizeof(cfg_clear1));

    if (getAck(buffer, 256, 0x05, 0x01)) {
        Serial.println("Get ack successed!");
    }
    SerialGNSS.write(cfg_clear2, sizeof(cfg_clear2));
    if (getAck(buffer, 256, 0x05, 0x01)) {
        Serial.println("Get ack successed!");
    }
    SerialGNSS.write(cfg_clear3, sizeof(cfg_clear3));
    if (getAck(buffer, 256, 0x05, 0x01)) {
        Serial.println("Get ack successed!");
    }

    // UBX-CFG-RATE, Size 8, 'Navigation/measurement rate settings'
    uint8_t cfg_rate[] = { 0xB5, 0x62, 0x06, 0x08, 0x00, 0x00, 0x0E, 0x30 };
    SerialGNSS.write(cfg_rate, sizeof(cfg_rate));
    if (getAck(buffer, 256, 0x06, 0x08)) {
        Serial.println("Get ack successed!");
    }
    else {
        return false;
    }
    return true;
}









void setup()
{
    initBoard();
    // When the power is turned on, a delay is required.
    delay(1500);

    Serial.println("SparkFun Ublox Example");

    u8g2->clearBuffer();
    u8g2->setFlipMode(0);
    u8g2->setFontMode(1); // Transparent
    u8g2->setDrawColor(1);
    u8g2->setFontDirection(0);
    u8g2->setFont(u8g2_font_fur11_tf);

    if (recovery()) {
        Serial.println("recovery successed!");
    }
    else {
        Serial.println("recovery failed!");
    }

    if (myGPS.begin(Serial1) == false) 
    {
        Serial.println(F("Ublox GPS not detected . Please check wiring. Freezing."));
        while (1);
    }
    else
    {
        u8g2->drawStr(0, 15, "Ublox GPS Ok!");
        u8g2->sendBuffer();
        delay(1500);
    }

}

char s[3];
char s_count[5];
char s_lat[10];
char s_lon[10];
float latitude = 55.980951* 1000000;
float longitude = 37.130935* 1000000;

void loop()
{
    myGPS.checkUblox(); //See if new data is available. Process bytes as they come in.


    if (nmea.isValid() == true) 
    {
        long latitude_mdeg = nmea.getLatitude();
        long longitude_mdeg = nmea.getLongitude();

        Serial.print("Latitude (deg): ");
        Serial.println(latitude_mdeg / 1000000., 6);
        Serial.print("Longitude (deg): ");
        Serial.println(longitude_mdeg / 1000000., 6);


        latitude = latitude_mdeg / 1000000.0;
        longitude = longitude_mdeg / 1000000.0;

        u8g2->setDrawColor(0);// Black
        u8g2->drawRBox(0, 3, 100, 13, 0);
        u8g2->drawRBox(0, 17, 100, 13, 0);
        u8g2->setDrawColor(1);
        u8g2->drawStr(0, 15, "Lat: ");
        u8g2->drawStr(0, 30, "Lon: ");
        sprintf(s_lat, "%.5f", latitude);
        sprintf(s_lon, "%.5f", longitude);
        u8g2->drawStr(30, 15, s_lat);
        u8g2->drawStr(30, 30, s_lon);
        u8g2->sendBuffer();



    }
    else 
    {
        uint8_t num_sat = nmea.getNumSatellites();
        char s[3];
        sprintf(s, "%d", num_sat);
        Serial.print("No Fix - ");
        Serial.print("Num. satellites: ");
        Serial.println(num_sat);

        u8g2->setDrawColor(0);// Black
        u8g2->drawRBox(0, 3, 100, 13, 0);
        u8g2->drawRBox(0, 17, 100, 13, 0);
        u8g2->drawRBox(62, 32, 20, 14, 0);
        u8g2->setDrawColor(1);
        u8g2->drawStr(0, 45, "Num.sat: ");
        u8g2->drawStr(65, 45,  s);
        u8g2->sendBuffer();

    }

    u8g2->setDrawColor(0);// Black
    u8g2->drawRBox(42, 48, 35, 13,0);
    u8g2->setDrawColor(1);
    sprintf(s_count, "%d", count);
    u8g2->drawStr(0, 60, "Test: ");
    u8g2->drawStr(45, 60, s_count);
    u8g2->sendBuffer();
    count++;

    if (count > 999) count = 0;


    delay(250); //Don't pound too hard on the I2C bus
}


void SFE_UBLOX_GPS::processNMEA(char incoming)
{
    //Take the incoming char from the Ublox I2C port and pass it on to the MicroNMEA lib
    //for sentence cracking
    nmea.process(incoming);
}
