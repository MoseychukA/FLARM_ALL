#include <Arduino.h>
#include <esp_task_wdt.h>
#include "boards.h"
#include <Wire.h>
#include "XPowersLib.h"


// Defined using AXP2102
#define XPOWERS_CHIP_AXP2102


bool  pmu_flag = 0;
XPowersPMU PMU;

const uint8_t i2c_sda = 21;
const uint8_t i2c_scl = 22;
const uint8_t pmu_irq_pin = 35;

void setFlag(void)
{
    pmu_flag = true;
}

#include <TinyGPS.h>

TinyGPS gps;


//bool feedgps();

#define SerialGNSS Serial1
static const uint32_t GPSBaud = 9600;

void setup() 
{

    // Serial.begin(9600);                          //  Инициируем работу с аппаратной шиной UART для вывода данных в монитор последовательного порта на скорости 9600 бит/сек.
    Serial.begin(115200);

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

    SerialGNSS.begin(GPSBaud);
    gps.begin(SerialGNSS);                          //  Инициируем расшифровку строк NMEA указав объект используемой шины UART (вместо аппаратной шины, можно указывать программную).
  
}

void loop() {

  bool newdata = false;

  unsigned long start = millis();

  long lat, lon;

  unsigned long age;

  //задержка в секунду между обновлениями координат

  while (millis() - start < 1000) 
  {
    if (readgps())
     newdata = true;
  }

  if (newdata) 
  {

    gps.get_position(&lat, &lon, &age);
 
  }
}

bool readgps() 
{
  while (SerialGNSS.available()) 
  {
    int b = SerialGNSS.read();

    //в TinyGPS есть баг, когда не обрабатываются данные с \r и \n

    if('\r' != b) 
    {
      if (gps.encode(b))
       return true;
    }
  }
  return false;
}

