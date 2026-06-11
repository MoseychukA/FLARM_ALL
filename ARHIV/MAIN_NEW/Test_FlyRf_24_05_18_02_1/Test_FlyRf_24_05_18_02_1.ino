#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <stdio.h>                // define I/O functions
#include <Arduino.h>              // define I/O functions
#include "SPI.h"
#include <TFT_eSPI.h>             // Поддержка TFT дисплея  
#include "TFTModule.h" 
#include "Configuration_ESP32.h"
#include <esp_task_wdt.h>
#include "boards.h"
#include "WebRF.h"
#include "Settings.h"

// Defined using AXP2102
#define XPOWERS_CHIP_AXP2102

#include <Wire.h>
#include <Arduino.h>
#include "XPowersLib.h"

#include "SparkFun_Ublox_Arduino_Library.h"
#include <MicroNMEA.h> //https://github.com/stevemarple/MicroNMEA

bool  pmu_flag = 0;
XPowersPMU PMU;

const uint8_t i2c_sda = 21;
const uint8_t i2c_scl = 22;
const uint8_t pmu_irq_pin = 35;

void setFlag(void)
{
    pmu_flag = true;
}

/* Установите здесь свои SSID и пароль */
const char* ssid = "ESP32";
const char* password = "12345678";
/* Настройки IP адреса */
IPAddress local_ip(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
 
char nmeaBuffer[100];
MicroNMEA nmea(nmeaBuffer, sizeof(nmeaBuffer));

#define SerialGNSS Serial1
SFE_UBLOX_GPS myGPS;
uint8_t buffer[256];

TFTModule tftModule;
uint32_t screenIdleTimer = 0;

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



void setup(void) {
  Serial.begin(115200);

  Settings.setup();

  Serial.println();
  Serial.print(F(" DEV.ID: "));
  Serial.println(String(Settings.ESP32_getChipId() & 0x00FFFFFF, HEX));


  String ver_soft = __FILE__;
  int val_srt = ver_soft.lastIndexOf('\\');
  ver_soft.remove(0, val_srt + 1);
  val_srt = ver_soft.lastIndexOf('.');
  ver_soft.remove(val_srt);
  Serial.println(ver_soft);


  initBoard();
  // When the power is turned on, a delay is required.

  PoverPMU();

  delay(1500);

  u8g2->clearBuffer();
  u8g2->setFlipMode(0);
  u8g2->setFontMode(1); // Transparent
  u8g2->setDrawColor(1);
  u8g2->setFontDirection(0);
  u8g2->setFont(u8g2_font_fur11_tf);

  WiFi.softAP(ssid, password);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  Serial.println("");
  Web_setup();

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

  Settings.saveVer(ver_soft);  // Сохранить строку с текущей версией.
  tftModule.Setup();

}

char s[3];
char s_count[5];
char s_lat[10];
char s_lon[10];
char s_alt[5];
float latitude = 55.980951 * 1000000;
float longitude = 37.130935 * 1000000;


void loop(void) 
{
  Web_loop();

 
  static uint32_t tmr = millis();

  /* Проверяем наличие новой информации GPS*/
  if (millis() - tmr > 250)
  {
      tmr = millis();
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

          Settings.setCurrentLatitude(latitude_mdeg);
          Settings.setLCurrentLongitude(longitude_mdeg);

          float altitude = 0.0;
          long alt;
          if (nmea.getAltitude(alt))
          {
              altitude = alt / 1000.0;
              Serial.println(alt / 1000., 1);
          }
       
          else
          {
              Serial.println("not available");
              altitude = 0.0;
          }

          char NavSystem = nmea.getNavSystem();
          Settings.setnavSystem(NavSystem);

          if (nmea.getNavSystem())
              Serial.println(nmea.getNavSystem());
          else
              Serial.println("none");

          Settings.setAltitude(altitude);

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

          uint8_t num_sat = nmea.getNumSatellites();
          Settings.setNumSat(num_sat);
          char s[3];
          sprintf(s, "%d", num_sat);
          Serial.print("No Fix - ");
          Serial.print("Num. satellites: ");
          Serial.println(num_sat);

          u8g2->setDrawColor(0);// Black
          u8g2->drawRBox(62, 32, 20, 14, 0);
          u8g2->setDrawColor(1);
          u8g2->drawStr(0, 45, "Num.sat: ");
          u8g2->drawStr(65, 45, s);

          u8g2->setDrawColor(0);// Black
          u8g2->drawRBox(45, 48, 60, 13, 0);
          u8g2->setDrawColor(1);
          sprintf(s_alt, "%.1f", altitude);
          u8g2->drawStr(0, 60, "Alt m: ");
          u8g2->drawStr(50, 60, s_alt);
          u8g2->sendBuffer();
      }
  }


 /* Web_loop();*/
  tftModule.Update();

}

void SFE_UBLOX_GPS::processNMEA(char incoming)
{
    //Take the incoming char from the Ublox I2C port and pass it on to the MicroNMEA lib
    //for sentence cracking
    nmea.process(incoming);
}


void PoverPMU()
{
    bool result = PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, i2c_sda, i2c_scl);

    if (result == false) {
        Serial.println("PMU is not online..."); while (1)delay(50);
    }

    Serial.printf("getID:0x%x\n", PMU.getChipID());

    // Set the minimum common working voltage of the PMU VBUS input,
    // below this value will turn off the PMU
    PMU.setVbusVoltageLimit(XPOWERS_AXP2101_VBUS_VOL_LIM_4V36);

    // Set the maximum current of the PMU VBUS input,
    // higher than this value will turn off the PMU
    PMU.setVbusCurrentLimit(XPOWERS_AXP2101_VBUS_CUR_LIM_1500MA);


    // Get the VSYS shutdown voltage
    uint16_t vol = PMU.getSysPowerDownVoltage();
    Serial.printf("->  getSysPowerDownVoltage:%u\n", vol);

    // Set VSY off voltage as 2600mV , Adjustment range 2600mV ~ 3300mV
    PMU.setSysPowerDownVoltage(2600);

    vol = PMU.getSysPowerDownVoltage();
    Serial.printf("->  getSysPowerDownVoltage:%u\n", vol);


    // DC1 IMAX=2A
    // 1500~3400mV,100mV/step,20steps
    PMU.setDC1Voltage(3300);
    Serial.printf("DC1  : %s   Voltage:%u mV \n", PMU.isEnableDC1() ? "+" : "-", PMU.getDC1Voltage());

    // DC2 IMAX=2A
    // 500~1200mV  10mV/step,71steps
    // 1220~1540mV 20mV/step,17steps
    PMU.setDC2Voltage(1000);
    Serial.printf("DC2  : %s   Voltage:%u mV \n", PMU.isEnableDC2() ? "+" : "-", PMU.getDC2Voltage());

    // DC3 IMAX = 2A
    // 500~1200mV,10mV/step,71steps
    // 1220~1540mV,20mV/step,17steps
    // 1600~3400mV,100mV/step,19steps
    PMU.setDC3Voltage(3300);
    Serial.printf("DC3  : %s   Voltage:%u mV \n", PMU.isEnableDC3() ? "+" : "-", PMU.getDC3Voltage());

    // DCDC4 IMAX=1.5A
    // 500~1200mV,10mV/step,71steps
    // 1220~1840mV,20mV/step,32steps
    PMU.setDC4Voltage(1000);
    Serial.printf("DC4  : %s   Voltage:%u mV \n", PMU.isEnableDC4() ? "+" : "-", PMU.getDC4Voltage());

    // DC5 IMAX=2A
    // 1200mV
    // 1400~3700mV,100mV/step,24steps
    PMU.setDC5Voltage(3300);
    Serial.printf("DC5  : %s   Voltage:%u mV \n", PMU.isEnableDC5() ? "+" : "-", PMU.getDC5Voltage());

    //ALDO1 IMAX=300mA
    //500~3500mV, 100mV/step,31steps
    PMU.setALDO1Voltage(3300);

    //ALDO2 IMAX=300mA
    //500~3500mV, 100mV/step,31steps
    PMU.setALDO2Voltage(3300);

    //ALDO3 IMAX=300mA
    //500~3500mV, 100mV/step,31steps
    PMU.setALDO3Voltage(3300);

    //ALDO4 IMAX=300mA
    //500~3500mV, 100mV/step,31steps
    PMU.setALDO4Voltage(3300);

    //BLDO1 IMAX=300mA
    //500~3500mV, 100mV/step,31steps
    PMU.setBLDO1Voltage(3300);

    //BLDO2 IMAX=300mA
    //500~3500mV, 100mV/step,31steps
    PMU.setBLDO2Voltage(3300);

    //CPUSLDO IMAX=30mA
    //500~1400mV,50mV/step,19steps
    PMU.setCPUSLDOVoltage(1000);

    //DLDO1 IMAX=300mA
    //500~3400mV, 100mV/step,29steps
    PMU.setDLDO1Voltage(3300);

    //DLDO2 IMAX=300mA
    //500~1400mV, 50mV/step,2steps
    PMU.setDLDO2Voltage(3300);


    PMU.enableDC1();
    PMU.enableDC2();
    PMU.enableDC3();
    PMU.enableDC4();
    PMU.enableDC5();
    PMU.enableALDO1();
    PMU.enableALDO2();
    PMU.enableALDO3();
    PMU.enableALDO4();
    PMU.enableBLDO1();
    PMU.enableBLDO2();
    PMU.enableCPUSLDO();
    PMU.enableDLDO1();
    PMU.enableDLDO2();


    Serial.println("DCDC=======================================================================");
    Serial.printf("DC1  : %s   Voltage:%u mV \n", PMU.isEnableDC1() ? "+" : "-", PMU.getDC1Voltage());
    Serial.printf("DC2  : %s   Voltage:%u mV \n", PMU.isEnableDC2() ? "+" : "-", PMU.getDC2Voltage());
    Serial.printf("DC3  : %s   Voltage:%u mV \n", PMU.isEnableDC3() ? "+" : "-", PMU.getDC3Voltage());
    Serial.printf("DC4  : %s   Voltage:%u mV \n", PMU.isEnableDC4() ? "+" : "-", PMU.getDC4Voltage());
    Serial.printf("DC5  : %s   Voltage:%u mV \n", PMU.isEnableDC5() ? "+" : "-", PMU.getDC5Voltage());
    Serial.println("ALDO=======================================================================");
    Serial.printf("ALDO1: %s   Voltage:%u mV\n", PMU.isEnableALDO1() ? "+" : "-", PMU.getALDO1Voltage());
    Serial.printf("ALDO2: %s   Voltage:%u mV\n", PMU.isEnableALDO2() ? "+" : "-", PMU.getALDO2Voltage());
    Serial.printf("ALDO3: %s   Voltage:%u mV\n", PMU.isEnableALDO3() ? "+" : "-", PMU.getALDO3Voltage());
    Serial.printf("ALDO4: %s   Voltage:%u mV\n", PMU.isEnableALDO4() ? "+" : "-", PMU.getALDO4Voltage());
    Serial.println("BLDO=======================================================================");
    Serial.printf("BLDO1: %s   Voltage:%u mV\n", PMU.isEnableBLDO1() ? "+" : "-", PMU.getBLDO1Voltage());
    Serial.printf("BLDO2: %s   Voltage:%u mV\n", PMU.isEnableBLDO2() ? "+" : "-", PMU.getBLDO2Voltage());
    Serial.println("CPUSLDO====================================================================");
    Serial.printf("CPUSLDO: %s Voltage:%u mV\n", PMU.isEnableCPUSLDO() ? "+" : "-", PMU.getCPUSLDOVoltage());
    Serial.println("DLDO=======================================================================");
    Serial.printf("DLDO1: %s   Voltage:%u mV\n", PMU.isEnableDLDO1() ? "+" : "-", PMU.getDLDO1Voltage());
    Serial.printf("DLDO2: %s   Voltage:%u mV\n", PMU.isEnableDLDO2() ? "+" : "-", PMU.getDLDO2Voltage());
    Serial.println("===========================================================================");

    // Set the time of pressing the button to turn off
    PMU.setPowerKeyPressOffTime(XPOWERS_POWEROFF_4S);
    uint8_t opt = PMU.getPowerKeyPressOffTime();
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
    // Set the button power-on press time
    PMU.setPowerKeyPressOnTime(XPOWERS_POWERON_128MS);
    opt = PMU.getPowerKeyPressOnTime();
    Serial.print("PowerKeyPressOnTime:");
    switch (opt) {
    case XPOWERS_POWERON_128MS: Serial.println("128 Ms");
        break;
    case XPOWERS_POWERON_512MS: Serial.println("512 Ms");
        break;
    case XPOWERS_POWERON_1S: Serial.println("1 Second");
        break;
    case XPOWERS_POWERON_2S: Serial.println("2 Second");
        break;
    default:
        break;
    }

    Serial.println("===========================================================================");

    bool en;

    // DCDC 120%(130%) high voltage turn off PMIC function
    en = PMU.getDCHighVoltagePowerDowmEn();
    Serial.print("getDCHighVoltagePowerDowmEn:");
    Serial.println(en ? "ENABLE" : "DISABLE");
    // DCDC1 85% low voltage turn off PMIC function
    en = PMU.getDC1LowVoltagePowerDowmEn();
    Serial.print("getDC1LowVoltagePowerDowmEn:");
    Serial.println(en ? "ENABLE" : "DISABLE");
    // DCDC2 85% low voltage turn off PMIC function
    en = PMU.getDC2LowVoltagePowerDowmEn();
    Serial.print("getDC2LowVoltagePowerDowmEn:");
    Serial.println(en ? "ENABLE" : "DISABLE");
    // DCDC3 85% low voltage turn off PMIC function
    en = PMU.getDC3LowVoltagePowerDowmEn();
    Serial.print("getDC3LowVoltagePowerDowmEn:");
    Serial.println(en ? "ENABLE" : "DISABLE");
    // DCDC4 85% low voltage turn off PMIC function
    en = PMU.getDC4LowVoltagePowerDowmEn();
    Serial.print("getDC4LowVoltagePowerDowmEn:");
    Serial.println(en ? "ENABLE" : "DISABLE");
    // DCDC5 85% low voltage turn off PMIC function
    en = PMU.getDC5LowVoltagePowerDowmEn();
    Serial.print("getDC5LowVoltagePowerDowmEn:");
    Serial.println(en ? "ENABLE" : "DISABLE");

    // PMU.setDCHighVoltagePowerDowm(true);
    // PMU.setDC1LowVoltagePowerDowm(true);
    // PMU.setDC2LowVoltagePowerDowm(true);
    // PMU.setDC3LowVoltagePowerDowm(true);
    // PMU.setDC4LowVoltagePowerDowm(true);
    // PMU.setDC5LowVoltagePowerDowm(true);

    // It is necessary to disable the detection function of the TS pin on the board
    // without the battery temperature detection function, otherwise it will cause abnormal charging
    PMU.disableTSPinMeasure();

    // PMU.enableTemperatureMeasure();

    // Enable internal ADC detection
    PMU.enableBattDetection();
    PMU.enableVbusVoltageMeasure();
    PMU.enableBattVoltageMeasure();
    PMU.enableSystemVoltageMeasure();


    /*
      The default setting is CHGLED is automatically controlled by the PMU.
    - XPOWERS_CHG_LED_OFF,
    - XPOWERS_CHG_LED_BLINK_1HZ,
    - XPOWERS_CHG_LED_BLINK_4HZ,
    - XPOWERS_CHG_LED_ON,
    - XPOWERS_CHG_LED_CTRL_CHG,
    * */
    PMU.setChargingLedMode(XPOWERS_CHG_LED_OFF);


    pinMode(pmu_irq_pin, INPUT);
    attachInterrupt(pmu_irq_pin, setFlag, FALLING);


    // Disable all interrupts
    PMU.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
    // Clear all interrupt flags
    PMU.clearIrqStatus();
    // Enable the required interrupt function
    PMU.enableIRQ(
        XPOWERS_AXP2101_BAT_INSERT_IRQ | XPOWERS_AXP2101_BAT_REMOVE_IRQ |              //BATTERY
        XPOWERS_AXP2101_VBUS_INSERT_IRQ | XPOWERS_AXP2101_VBUS_REMOVE_IRQ |            //VBUS
        XPOWERS_AXP2101_PKEY_SHORT_IRQ | XPOWERS_AXP2101_PKEY_LONG_IRQ |               //POWER KEY
        XPOWERS_AXP2101_BAT_CHG_DONE_IRQ | XPOWERS_AXP2101_BAT_CHG_START_IRQ           //CHARGE
        // XPOWERS_AXP2101_PKEY_NEGATIVE_IRQ | XPOWERS_AXP2101_PKEY_POSITIVE_IRQ   |   //POWER KEY
    );

    // Set the precharge charging current
    PMU.setPrechargeCurr(XPOWERS_AXP2101_PRECHARGE_50MA);
    // Set constant current charge current limit
    PMU.setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_200MA);
    // Set stop charging termination current
    PMU.setChargerTerminationCurr(XPOWERS_AXP2101_CHG_ITERM_25MA);

    // Set charge cut-off voltage
    PMU.setChargeTargetVoltage(XPOWERS_AXP2101_CHG_VOL_4V1);

    // Set the watchdog trigger event type
    PMU.setWatchdogConfig(XPOWERS_AXP2101_WDT_IRQ_TO_PIN);
    // Set watchdog timeout
    PMU.setWatchdogTimeout(XPOWERS_AXP2101_WDT_TIMEOUT_4S);
    // Enable watchdog to trigger interrupt event
    PMU.enableWatchdog();

    PMU.disableWatchdog();



}
