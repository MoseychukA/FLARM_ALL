#include <stdio.h>                // define I/O functions//#include "MAVLinkRF.h"
#include <Arduino.h>              // define I/O functions
#include "SPI.h"
#include <esp_task_wdt.h>
#include <iostream>
#include <locale.h>

#include "OTA.h"
#include "TimeRF.h"
#include "GNSS.h"
#include "RF.h"
#include "EEPROMRF.h"
#include "NMEA.h"
#include "D1090.h"
#include "SoC.h"
#include "WiFiRF.h"
#include "WebRF.h"
#include "Baro.h"
#include "TrafficHelper.h"
#include "ESP32RF.h"
#include <TimeLib.h>
#include <TinyGPS++.h>
#include "ServiceMain.h"
#include "Configuration_ESP32.h"
#include "CoreCommandBuffer.h"    // обработчик входящих по UART команд
#include "Module1090.h"
#include "Button.h"

int set_air = 0;   //  
bool set_auto_coordinate = false; // Признак автоматического ввода текущих координат 

void txrx_test();
void normal();

#if !defined(SERIAL_FLUSH)
#define SERIAL_FLUSH() Serial.flush()
#endif

#define DEBUG 0
#define DEBUG_TIMING 0

#define isTimeToDisplay() (millis() - LEDTimeMarker     > 1000)
#define isTimeToExport()  (millis() - ExportTimeMarker  > 1000)

ufo_t ThisAircraft;

hardware_info_t hw_info = {
  .model    = DEFAULT_FLYRF_MODEL,
  .revision = 0,
  .soc      = SOC_NONE,
  .rf       = RF_IC_NONE,
  .gnss     = GNSS_MODULE_NONE,
  .baro     = BARO_MODULE_NONE,
  .display  = DISPLAY_NONE,
};

unsigned long LEDTimeMarker = 0;
unsigned long ExportTimeMarker = 0;

static void onButtonPressDownCb(void* button_handle, void* usr_data) 
{
   service.set_num_buttton(1);
}

static void onButtonDoubleClickEventCb(void* button_handle, void* usr_data)
{
   service.set_num_buttton(2);
}

static void onButtonLongPressStartEventCb(void* button_handle, void* usr_data)
{
   service.set_num_buttton(3);
}


void setup()
{
    rst_info* resetInfo;

    hw_info.soc = SoC_setup(); // Has to be very first procedure in the execution order

    resetInfo = (rst_info*)SoC->getResetInfoPtr();

    Serial.println();
    Serial.print(F(FLYRF_IDENT "-"));
    Serial.print(SoC->name);
    Serial.print(F(" FW.REV: " FLYRF_FIRMWARE_VERSION " DEV.ID: "));
    Serial.println(String(SoC->getChipId(), HEX));

    String ver_soft = __FILE__;
    int val_srt = ver_soft.lastIndexOf('\\');
    ver_soft.remove(0, val_srt + 1);
    val_srt = ver_soft.lastIndexOf('.');
    ver_soft.remove(val_srt);
    Serial.println(ver_soft);
    service.saveVer(ver_soft);  // Сохранить строку с текущей версией.

    pinMode(lmic_pins.nss, INPUT);


  SERIAL_FLUSH();

  if (resetInfo)
  {
    Serial.println(""); Serial.print(F("Reset reason: ")); Serial.println(resetInfo->reason);
  }
  Serial.println(SoC->getResetReason());
  Serial.print(F("Free heap size: ")); Serial.println(SoC->getFreeHeap());
  Serial.println(SoC->getResetInfo()); Serial.println("");

  SERIAL_FLUSH();

  EEPROM_setup();

 /* SoC->Button_setup();*/

  ThisAircraft.addr = SoC->getChipId() & 0x00FFFFFF;

  hw_info.rf = RF_setup();

  delay(100);

  hw_info.baro = Baro_setup();

  hw_info.display = SoC->Display_setup();

  hw_info.gnss = GNSS_setup();
  ThisAircraft.aircraft_type = settings->aircraft_type;
 
  ThisAircraft.protocol = settings->rf_protocol;
  ThisAircraft.stealth  = settings->stealth;
  ThisAircraft.no_track = settings->no_track;

  Traffic_setup();

  SoC->swSer_enableRx(false);

  WiFi_setup();
 
  if (SoC->Bluetooth_ops) 
  {
     SoC->Bluetooth_ops->setup();
  }

  OTA_setup();
  Web_setup();
  NMEA_setup();

  delay(1000);

  switch (settings->mode)
  {
  case FLYRF_MODE_TXRX_TEST0:
      Time_setup();
      set_air = 0;
      break;
  case FLYRF_MODE_TXRX_TEST1:
      Time_setup();
      set_air = 1;
      break;
  case FLYRF_MODE_TXRX_TEST2:
      set_air = 2;
      Time_setup();
      break;
  case FLYRF_MODE_TXRX_TEST3:
      set_air = 3;
      Time_setup();
      break;
  case FLYRF_MODE_TXRX_TEST4:
      set_air = 4;
      Time_setup();
      break;
  case FLYRF_MODE_TXRX_TEST5:
      set_air = 5;
      Time_setup();
      break;
  case FLYRF_MODE_NORMAL:
  default:
      SoC->swSer_enableRx(true);
      set_air = 0;
      break;
  }
  
  SoC->post_init();

  if (psramInit() == false)
      Serial.println("PSRAM failed to initialize");
  else
      Serial.println("PSRAM initialized");

  Serial.printf("PSRAM Size available (bytes): %d\r\n", ESP.getFreePsram());

  heap_caps_malloc_extmem_enable(8000); //Use PSRAM for memory requests larger than 1,000 bytes

  moduleDump1090.setup();


  // initializing a button
  Button* btn = new Button(GPIO_NUM_48, false);

  btn->attachPressDownEventCb(&onButtonPressDownCb, NULL);
  btn->attachDoubleClickEventCb(&onButtonDoubleClickEventCb, NULL);
  btn->attachLongPressStartEventCb(onButtonLongPressStartEventCb, NULL);
 
  if (settings->input_coordinates == IMPUT_COORD_MANUAL)
  {
      //ThisAircraft.test_latitude =  ;
      //ThisAircraft.test_longitude =  ;
  }



  SoC->WDT_setup();
}

void loop()
{
    RF_loop();                       // Сначала выполните общие действия с радиочастотами
    moduleDump1090.update();         // Проверить прием пакета от DUMP1090
   esp_task_wdt_reset();
  switch (settings->mode)
  {
#if !defined(EXCLUDE_TEST_MODE)
 // case FLYRF_MODE_TXRX_TEST0:
  case FLYRF_MODE_TXRX_TEST1:
  case FLYRF_MODE_TXRX_TEST2:
  case FLYRF_MODE_TXRX_TEST3:
  case FLYRF_MODE_TXRX_TEST4:
  case FLYRF_MODE_TXRX_TEST5:
    txrx_test();
    break;
#endif /* EXCLUDE_TEST_MODE */
  case FLYRF_MODE_NORMAL:
  default:
    normal();
    break;
  }

  // Show status info on tiny OLED display
  SoC->Display_loop();

  // Handle DNS
  WiFi_loop();

  // Handle Web
  Web_loop();

  // Handle OTA update.
  OTA_loop();

  SoC->loop();

  if (SoC->Bluetooth_ops) 
  {
    SoC->Bluetooth_ops->loop();
  }

  if (SoC->UART_ops) {
     SoC->UART_ops->loop();
  }

  CommandHandler.handleCommands();
  CommandHandler.SendTraffic_Msg();
  CommandHandler.GPS_send_base();

  Time_loop();

  yield();
}

void shutdown(int reason)
{
  SoC->WDT_fini();

  SoC->swSer_enableRx(false);

   NMEA_fini();

  Web_fini();

  if (SoC->Bluetooth_ops) {
     SoC->Bluetooth_ops->fini();
  }

  if (SoC->USB_ops) {
     SoC->USB_ops->fini();
  }

  WiFi_fini();

  GNSS_fini();
 
  SoC->Display_fini(reason);

  Baro_fini();

  RF_Shutdown();

  SoC_fini(reason);
}

void normal()
{
  bool success;

  Baro_loop();

  GNSS_loop();

  ThisAircraft.timestamp = now();
  if (isValidFix()) 
  {
    ThisAircraft.latitude  = gnss.location.lat();
    ThisAircraft.longitude = gnss.location.lng();
    ThisAircraft.altitude  = gnss.altitude.meters();
    ThisAircraft.course    = gnss.course.deg();
    ThisAircraft.speed     = gnss.speed.knots();
    ThisAircraft.hdop      = (uint16_t) gnss.hdop.value();
    ThisAircraft.geoid_separation = gnss.separation.meters();

    if (ThisAircraft.latitude != 0 || ThisAircraft.longitude != 0)
    {
        ThisAircraft.old_latitude = gnss.location.lat();
        ThisAircraft.old_longitude = gnss.location.lng();
    }


#if !defined(EXCLUDE_EGM96)
    /*
     * When geoidal separation is zero or not available - use approx. EGM96 value
     */
    if (ThisAircraft.geoid_separation == 0.0) 
    {
      ThisAircraft.geoid_separation = (float) LookupSeparation(ThisAircraft.latitude, ThisAircraft.longitude);
      /* we can assume the GPS unit is giving ellipsoid height */
      ThisAircraft.altitude -= ThisAircraft.geoid_separation;
    }
#endif /* EXCLUDE_EGM96 */

    RF_Transmit(RF_Encode(&ThisAircraft), true);   // Передать параметры посредством LoRa
  }
  else
  {
      if (ThisAircraft.old_latitude != 0 || ThisAircraft.old_longitude != 0)
      {
          ThisAircraft.altitude = 25000.0;

          RF_Transmit(RF_Encode(&ThisAircraft), true);  // Передать параметры посредством LoRa в случае если нет сигналов GPS
      }
  }
  success = RF_Receive();  //

#if DEBUG
  success = true;
#endif

  if (success && isValidFix()) ParseData();

  if (isValidFix()) 
  {
    Traffic_loop();
  }

  if (isTimeToDisplay()) 
  {
     LEDTimeMarker = millis();
  }

  if (isTimeToExport()) 
  { 
    NMEA_Export();
    D1090_Export();  

    ExportTimeMarker = millis();
  }

  // Handle Air Connect
  NMEA_loop();

  ClearExpired();
}

#if !defined(EXCLUDE_TEST_MODE)

unsigned int pos_ndx = 0;
unsigned long TxPosUpdMarker = 0;

float altitude0 = 100.0;
float altitude1 = 100.0;
float altitude2 = 100.0;
float altitude3 = 100.0;
float altitude4 = 100.0;


float speed0 = 300.0;
float speed1 = 300.0;
float speed2 = 300.0;
float speed3 = 300.0;
float speed4 = 300.0;

bool alt_high0 = false;
bool alt_high1 = false;
bool alt_high2 = false;
bool alt_high3 = false;
bool alt_high4 = false;

bool alien_dist0 = false;
bool alien_dist1 = false;
bool alien_dist2 = false;
bool alien_dist3 = false;
bool alien_dist4 = false;


int alien_route1 = 1;
int alien_route2 = 1;
int alien_route3 = 1;
int alien_route4 = 1;

float test_curse0 = 0.0;
float test_curse1 = 0.0;
float test_curse2 = 0.0;
float test_curse3 = 0.0;
float test_curse4 = 0.0;

//улица Кутузова, 1  Средняя точка
//55.958388, 37.243838
// Точка центр
float alien_lat0 = 55.958388;  // 
float alien_lon0 = 37.243838;


// Линия Северозапад - Юговосток
float alien_lat5 = 55.935742;  // 
float alien_lon5 = 37.348739;
// +/-
//Молжаниновский район
//55.935742, 37.348739
float alien_lat1 = 55.935742;
float alien_lon1 = 37.348739;


//рабочий посёлок Андреевка
//55.980395, 37.141351
float alien_lat2 = 55.980395;
float alien_lon2 = 37.141351;

//===================================
// Линия Югозапад - Северовосток
float alien_lat6 = 55.933575;
float alien_lon6 = 37.189899;
//деревня Брёхово
//55.933575, 37.189899
float alien_lat3 = 55.933575;
float alien_lon3 = 37.189899;

//территориальное управление Лунёвское
//55.987884, 37.307315
float alien_lat4 = 55.987884;
float alien_lon4 = 37.307315;


float Aircraft_latitude_old = 0.0;
float Aircraft_longitude_old = 0.0;


//55.912729, 37.254800
//территориальное управление Кутузовское

//56.010913, 37.240080
//территориальное управление Ржавки - Менделеево

float alien_lat7 = 55.912729;

float alien_lat8 = 55.912729;
float alien_lon8 = 37.254800;

float alien_lat9 = 56.010913;
float alien_lon9 = 37.240080;




//56.005065, 37.177184
//Зеленоградский лесопарк

//55.925354, 37.292806
//Захарьинская пойма

float alien_lat10 = 55.925354;

float alien_lat11 = 55.925354;
float alien_lon11 = 37.292806;

float alien_lat12 = 56.005065;
float alien_lon12 = 37.177184;


//Атлантический океан
/*
0.075397, 0.029420
-0.004039, 0.029420
-0.004039, -0.054865
 0.075397, -0.054865
*/

int track_air = 0;
float alien_lat13 = 0.075397;
float alien_lon13 = 0.029420;

float alien_lat14 = -0.004039;
float alien_lon14 = 0.029420;

float alien_lat15 = -0.004039;
float alien_lon15 = -0.054865;

float alien_lat16 = 0.075397;
float alien_lon16 = -0.054865;

float alien_lat20 = 0.075397;
float alien_lon20 = 0.029420;


/*
  0.053769, -179.953328
  -0.025508, -179.953328
  -0.025508, 179.953328
  0.053769, 179.953328
*/

float alien_lat23 = 0.053769;
float alien_lon23 = -179.953328;

float alien_lat24 = -0.025508;
float alien_lon24 = -179.953328;

float alien_lat25 = -0.025508;
float alien_lon25 = 179.953328;

float alien_lat26 = 0.053769;
float alien_lon26 = 179.953328;

float alien_lat30 = 0.053769;
float alien_lon30 = -179.953328;

char fly1[] = "AFL1118";
char fly2[] = "AFL2122";
char fly3[] = "AFL1684";
char fly4[] = "SMD6405";

int test_filtre = 0;

float coord[2];

void coordinate_calculation(float lat, float lon, float course, int distance)
{

    float LAT = lat;
    float LON = lon;
    float LAT1 = 0;
    float LON1 = 0;
    float AZIMUT = course;
    int L = distance;

    LAT1 = LAT + L * cos(AZIMUT * PI / 180) / (6371000 * PI / 180);
    LON1 = LON + L * sin(AZIMUT * PI / 180) / cos(LAT * PI / 180) / (6371000 * PI / 180);

    coord[0] = LAT1;
    coord[1] = LON1;
}
//int count_test = 0;

void txrx_test()
{
    bool success = false;
#if DEBUG_TIMING
    unsigned long baro_start_ms, baro_end_ms;
    unsigned long tx_start_ms, tx_end_ms, rx_start_ms, rx_end_ms;
    unsigned long parse_start_ms, parse_end_ms, led_start_ms, led_end_ms;
    unsigned long export_start_ms, export_end_ms;
    unsigned long oled_start_ms, oled_end_ms;
#endif
    ThisAircraft.timestamp = now(); 


    if (set_auto_coordinate == false && settings->input_coordinates == IMPUT_COORD_AUTO)
    {
        GNSS_loop();

        if (isValidFix())
        {
            ThisAircraft.test_latitude = gnss.location.lat();
            ThisAircraft.test_longitude = gnss.location.lng();
            set_auto_coordinate = true;
        }
    }
    else if (set_auto_coordinate == false && settings->input_coordinates == IMPUT_COORD_MANUAL)
    {
        if(settings->input_N_S == IMPUT_N)
        {
            ThisAircraft.test_latitude = (float)settings->test_latitude;
        }
        else
        {
            ThisAircraft.test_latitude = (float)-settings->test_latitude;
        }

        if (settings->input_E_W == IMPUT_E)
        {
            ThisAircraft.test_longitude = (float)settings->test_longitude;
        }
        else
        {
            ThisAircraft.test_longitude = (float)-settings->test_longitude;
        }

        set_auto_coordinate = true;
    }


    if (TxPosUpdMarker == 0 || (millis() - TxPosUpdMarker) > 2000)
    {
        // ThisAircraft.latitude  = pgm_read_float( &txrx_test_positions[pos_ndx][0]);
        // ThisAircraft.longitude = pgm_read_float( &txrx_test_positions[pos_ndx][1]);
       // pos_ndx = (pos_ndx + 1) % TXRX_TEST_NUM_POSITIONS;

        switch (set_air)
        {
            //case 0:
            //    ThisAircraft.latitude = pgm_read_float(&txrx_test_positions[pos_ndx][0]);
            //    ThisAircraft.longitude = pgm_read_float(&txrx_test_positions[pos_ndx][1]);

            //    if (!alt_high0)
            //    {
            //        altitude0 += 100.0;
            //        if (altitude0 > 4000.0)
            //        {
            //            altitude0 = 4000.0;
            //            alt_high0 = true;
            //        }
            //    }
            //    if (alt_high0)
            //    {
            //        altitude0 -= 100.0;
            //        if (altitude0 < 100.0)
            //        {
            //            altitude0 = 100.0;
            //            alt_high0 = false;
            //        }
            //    }
            //    break;
        case 1:

            ThisAircraft.latitude = ThisAircraft.test_latitude;    // 
            ThisAircraft.longitude = ThisAircraft.test_longitude;   // 

            speed0 = 200.0;
            altitude0 = 1000.0;


            /* тест на вращение*/
            ThisAircraft.course = ThisAircraft.course + 2.0;
            if (ThisAircraft.course >= 360.0)
                ThisAircraft.course = 0.0;

            ThisAircraft.altitude = altitude0;
            ThisAircraft.course = 1.0;      // test_curse0;
            ThisAircraft.speed = speed0;
            ThisAircraft.vs = TXRX_TEST_VS;  //футов в минуту
            break;
        case 2:
            /*
                // Линия Северозапад - Юговосток
                float alien_lat5 = 55.935742;  //
                float alien_lon5 = 37.348739;
                //Молжаниновский район
                //55.935742, 37.348739
                float alien_lat1 = 55.935742;
                float alien_lon1 = 37.348739;

                //рабочий посёлок Андреевка
                //55.980395, 37.141351
                float alien_lat2 = 55.980395;
                float alien_lon2 = 37.141351;
            */

            if (alien_route1 == 1)
            {
                if (!alien_dist1)
                {
                    alien_lat5 += 0.001213275;
                    alien_lon5 -= 0.005565125;

                    if (alien_lat5 >= alien_lat2)
                    {
                        alien_lat5 = alien_lat2;
                        alien_dist1 = true;
                    }
                    test_curse1 = 290;
                }

                if (alien_dist1)
                {


                    alien_lat5 -= 0.001213275;
                    alien_lon5 += 0.005565125;

                    if (alien_lat5 <= alien_lat1)
                    {
                        alien_lat5 = alien_lat1;
                        alien_dist1 = false;
                        alien_route1 = 2;
                    }
                    test_curse1 = 110;
                }

                ThisAircraft.latitude = alien_lat5;
                ThisAircraft.longitude = alien_lon5;
            }

            if (alien_route1 == 2)
            {

                /*
                 // Линия Югозапад - Северовосток
               float alien_lat6 = 55.933575;
               float alien_lon6 = 37.189899;
               //деревня Брёхово
               //55.933575, 37.189899
               float alien_lat3 = 55.933575;
               float alien_lon3 = 37.189899;

               //территориальное управление Лунёвское
               //55.987884, 37.307315
               float alien_lat4 = 55.987884;
               float alien_lon4 = 37.307315;

                */

                if (alien_dist2 == false)
                {
                    alien_lat6 += 0.001213275;
                    alien_lon6 += 0.005565125;

                    if (alien_lat6 >= alien_lat4)
                    {
                        alien_lat6 = alien_lat4;
                        alien_dist2 = true;
                    }
                    test_curse1 = 70;
                }

                if (alien_dist2 == true)
                {
                    alien_lat6 -= 0.001213275;
                    alien_lon6 -= 0.005565125;

                    if (alien_lat6 <= alien_lat3)
                    {
                        alien_lat6 = alien_lat3;
                        alien_dist2 = false;
                        alien_route1 = 1;
                    }
                    test_curse1 = 250;
                }

                ThisAircraft.latitude = alien_lat6;
                ThisAircraft.longitude = alien_lon6;
            }

            if (!alt_high1)
            {
                altitude1 += 25.0;
                if (altitude1 > 1150.0)
                {
                    altitude1 = 1150.0;
                    alt_high1 = true;
                }
            }
            if (alt_high1)
            {

                altitude1 -= 25.0;
                if (altitude1 < 850.0)
                {
                    altitude1 = 850.0;
                    alt_high1 = false;
                }
            }

            speed1 = 550.0;
            //speed1 -= 30.0;
            //if (speed1 <= 30.0)
            //    speed1 = 1020.0;

            ThisAircraft.altitude = altitude1;
            ThisAircraft.course = test_curse1;
            ThisAircraft.speed = speed1;
            ThisAircraft.vs = TXRX_TEST_VS;


            break;
            //====================================================================================================
        case 3:
            //Фирсановка улица Кутузова, 1  Средняя точка
           //55.958388, 37.243838
            ThisAircraft.latitude = alien_lat0;    // 
            ThisAircraft.longitude = alien_lon0;   // 

            test_curse0 = 360.0;
            speed0 = 200.0;
            altitude0 = 1000.0;

            esp_task_wdt_reset();
            //================ Самолет №1 ================================
            /*
                // Линия Северозапад - Юговосток
                float alien_lat5 = 55.935742;  //
                float alien_lon5 = 37.348739;
                //Молжаниновский район
                //55.935742, 37.348739
                float alien_lat1 = 55.935742;
                float alien_lon1 = 37.348739;

                //рабочий посёлок Андреевка
                //55.980395, 37.141351
                float alien_lat2 = 55.980395;
                float alien_lon2 = 37.141351;
            */

            if (!alien_dist1)
            {
                alien_lat5 += 0.001213275;
                alien_lon5 -= 0.005565125;

                if (alien_lat5 >= alien_lat2)
                {
                    alien_lat5 = alien_lat2;
                    alien_dist1 = true;
                }
                test_curse1 = 290;
            }

            if (alien_dist1)
            {


                alien_lat5 -= 0.001213275;
                alien_lon5 += 0.005565125;

                if (alien_lat5 <= alien_lat1)
                {
                    alien_lat5 = alien_lat1;
                    alien_dist1 = false;
                    // alien_route = 2;
                }
                test_curse1 = 110;
            }

            fo.latitude = alien_lat5;
            fo.longitude = alien_lon5;


            if (!alt_high1)
            {
                altitude1 += 50.0;
                if (altitude1 > 1200.0)
                {
                    altitude1 = 1200.0;
                    alt_high1 = true;
                }
            }
            if (alt_high1)
            {

                altitude1 -= 50.0;
                if (altitude1 < 50.0)
                {
                    altitude1 = 50.0;
                    alt_high1 = false;
                }
            }
            speed1 -= 30.0;
            if (speed1 <= 30.0)
                speed1 = 1020.0;

            fo.addr = 0x151DC8;
            fo.Squawk = 1521;
            memcpy((char*)fo.flight, fly1, strlen(fly1));
            fo.altitude = altitude1;
            fo.pressure_altitude = altitude1;
            fo.speed = speed1;
            fo.vert_rate = 50;
            fo.signal_source = 1;
            fo.timestamp = now(); // 
            fo.course = test_curse1;
            fo.aircraft_type = AIRCRAFT_TYPE_JET;
            /* Расчет расстояния, курса и уровня опастности сближения нашего и стороннего самолета*/
            if (fo.latitude != 0 && fo.longitude != 0) // Расчет возможен если получены координаты нашего и стороннего самолета
            {
                Traffic_Update(&fo);   // 
            }

            /* Остальные параметры записываем в базу */
            Traffic_Add(&fo);

            //======================== Самолет №2 ================================================

                            /*
                                 // Линия Югозапад - Северовосток
                               float alien_lat6 = 55.933575;
                               float alien_lon6 = 37.189899;
                               //деревня Брёхово
                               //55.933575, 37.189899
                               float alien_lat3 = 55.933575;
                               float alien_lon3 = 37.189899;

                               //территориальное управление Лунёвское
                               //55.987884, 37.307315
                               float alien_lat4 = 55.987884;
                               float alien_lon4 = 37.307315;

                            */

            esp_task_wdt_reset();

            if (alien_dist2 == false)
            {
                alien_lat6 += 0.001213275;
                alien_lon6 += 0.005565125;

                if (alien_lat6 >= alien_lat4)
                {
                    alien_lat6 = alien_lat4;
                    alien_dist2 = true;
                }
                test_curse2 = 70;
            }

            if (alien_dist2 == true)
            {
                alien_lat6 -= 0.001213275;
                alien_lon6 -= 0.005565125;

                if (alien_lat6 <= alien_lat3)
                {
                    alien_lat6 = alien_lat3;
                    alien_dist2 = false;
                    //alien_route = 1;
                }
                test_curse2 = 250;
            }

            fo.latitude = alien_lat6;
            fo.longitude = alien_lon6;

            if (!alt_high2)
            {
                altitude2 += 50.0;
                if (altitude2 > 1200.0)
                {
                    altitude2 = 1200.0;
                    alt_high2 = true;
                }
            }
            if (alt_high2)
            {

                altitude2 -= 50.0;
                if (altitude2 < 50.0)
                {
                    altitude2 = 50.0;
                    alt_high2 = false;
                }
            }
            speed2 -= 30.0;
            if (speed2 <= 30.0)
                speed2 = 1020.0;


            fo.addr = 0x151DA0;
            fo.Squawk = 2123;
            memcpy((char*)fo.flight, fly2, strlen(fly2));
            fo.altitude = altitude2;
            fo.pressure_altitude = altitude2;
            fo.speed = speed2;
            fo.vert_rate = 100;
            fo.signal_source = 1;
            fo.timestamp = now(); // 
            fo.course = test_curse2;
            fo.aircraft_type = AIRCRAFT_TYPE_JET;
            /* Расчет расстояния, курса и уровня опастности сближения нашего и стороннего самолета*/
            if (fo.latitude != 0 && fo.longitude != 0) // Расчет возможен если получены координаты нашего и стороннего самолета
            {
                Traffic_Update(&fo);   // 
            }

            /* Остальные параметры записываем в базу */
            Traffic_Add(&fo);
            //******************************************************************************************************************

            //================ Самолет №3 ================================
            /*
                 //55.912729, 37.254800
                //территориальное управление Кутузовское

                //56.010913, 37.240080
                //территориальное управление Ржавки - Менделеево

                float alien_lat7 = 55.912729;

                float alien_lat8 = 55.912729;
                float alien_lon8 = 37.254800;

                float alien_lat9 = 56.010913;
                float alien_lon9 = 37.240080;
            */
            esp_task_wdt_reset();

            if (!alien_dist3)
            {
                alien_lat7 += (56.010913 - 55.912729) / 20.0;   //0.001213275;
                alien_lon8 -= (37.254800 - 37.240080) / 20.0;   //0.005565125;

                if (alien_lat7 >= alien_lat9)
                {
                    alien_lat7 = alien_lat9;
                    alien_dist3 = true;
                }
                test_curse3 = 358;
            }

            if (alien_dist3)
            {

                alien_lat7 -= (56.010913 - 55.912729) / 20.0;   //0.001213275;
                alien_lon8 += (37.254800 - 37.240080) / 20.0;   //0.005565125;

                if (alien_lat7 <= alien_lat8)
                {
                    alien_lat7 = alien_lat8;
                    alien_dist3 = false;
                }
                test_curse3 = 178;
            }

            fo.latitude = alien_lat7;
            fo.longitude = alien_lon8;


            if (!alt_high3)
            {
                altitude3 += 40.0;
                if (altitude3 > 1000.0)
                {
                    altitude3 = 1000.0;
                    alt_high3 = true;
                }
            }
            if (alt_high3)
            {

                altitude3 -= 40.0;
                if (altitude3 < 50.0)
                {
                    altitude3 = 50.0;
                    alt_high3 = false;
                }
            }
            speed3 -= 30.0;
            if (speed3 <= 30.0)
                speed3 = 990.0;

            fo.addr = 0x151DCF;
            fo.Squawk = 2751;
            memcpy((char*)fo.flight, fly3, strlen(fly3));
            fo.altitude = altitude3;
            fo.pressure_altitude = altitude3;
            fo.speed = speed3;
            fo.vert_rate = -50;
            fo.signal_source = 1;
            fo.timestamp = now(); // 
            fo.course = test_curse3;
            fo.aircraft_type = AIRCRAFT_TYPE_JET;
            /* Расчет расстояния, курса и уровня опастности сближения нашего и стороннего самолета*/
            if (fo.latitude != 0 && fo.longitude != 0) // Расчет возможен если получены координаты нашего и стороннего самолета
            {
                Traffic_Update(&fo);   // 
            }

            /* Остальные параметры записываем в базу */
            Traffic_Add(&fo);

            //======================== Самолет №4 ================================================

            /*
                 //56.005065, 37.177184
                //Зеленоградский лесопарк

                //55.925354, 37.292806
                //Захарьинская пойма

                float alien_lat10 = 55.925354;

                float alien_lat11 = 55.925354;
                float alien_lon11 = 37.292806;

                float alien_lat12 = 56.005065;
                float alien_lon12 = 37.177184;

            */
            esp_task_wdt_reset();

            if (alien_dist4 == false)
            {
                alien_lat10 += (alien_lat12 - alien_lat11) / 20.0;//0.001213275;
                alien_lon11 -= (alien_lon11 - alien_lon12) / 20.0;//0.005565125;

                if (alien_lat10 >= alien_lat12)
                {
                    alien_lat10 = alien_lat12;
                    alien_dist4 = true;
                }
                test_curse4 = 340;
            }

            if (alien_dist4 == true)
            {
                alien_lat10 -= (alien_lat12 - alien_lat11) / 20.0; //
                alien_lon11 += (alien_lon11 - alien_lon12) / 20.0;//

                if (alien_lat10 <= alien_lat11)
                {
                    alien_lat10 = alien_lat11;
                    alien_dist4 = false;
                }
                test_curse4 = 150;
            }

            fo.latitude = alien_lat10;
            fo.longitude = alien_lon11;

            if (!alt_high4)
            {
                altitude4 += 50.0;
                if (altitude4 > 800.0)
                {
                    altitude4 = 800.0;
                    alt_high4 = true;
                }
            }
            if (alt_high4)
            {

                altitude4 -= 50.0;
                if (altitude4 < 50.0)
                {
                    altitude4 = 50.0;
                    alt_high4 = false;
                }
            }
            speed4 -= 30.0;
            if (speed4 <= 30.0)
                speed4 = 700.0;


            fo.addr = 0x155C11;
            fo.Squawk = 1501;
            memcpy((char*)fo.flight, fly4, strlen(fly4));
            fo.altitude = altitude4;
            fo.pressure_altitude = altitude4;
            fo.speed = speed4;
            fo.vert_rate = -150;
            fo.signal_source = 1;
            fo.timestamp = now(); // 
            fo.course = test_curse4;
            fo.aircraft_type = AIRCRAFT_TYPE_JET;
            /* Расчет расстояния, курса и уровня опастности сближения нашего и стороннего самолета*/
            if (fo.latitude != 0 && fo.longitude != 0) // Расчет возможен если получены координаты нашего и стороннего самолета
            {
                Traffic_Update(&fo);   // 
            }

            /* Остальные параметры записываем в базу */
            Traffic_Add(&fo);
            //******************************************************************************************************************
            break;
        case 4:
            //Атлантический океан, 1  Средняя точка
           //0.007748, 0.007875
            ThisAircraft.latitude = 0.007748;    // 
            ThisAircraft.longitude = 0.007875;   // 

            test_curse0 = 360.0;
            speed0 = 200.0;
            altitude0 = 1000.0;

            esp_task_wdt_reset();

            //    0.075397, 0.029420
            //    - 0.004039, 0.029420
            //    - 0.004039, -0.054865
            //    0.075397, -0.054865

            //================ Самолет №1 ================================
            /*
            //0.075397, 0.029420
            //- 0.004039, 0.029420

            */

            // track_air

            switch (track_air)
            {
            case 0:
                alien_lat20 -= (alien_lat13 - alien_lat14) / 20; // Перемещаемся сверху вниз
                alien_lon20 = alien_lon13;//
                if (alien_lat20 < alien_lat14)
                {
                    alien_lat20 = alien_lat14;
                    track_air = 1;
                }

                fo.course = 180;

                break;
            case 1:
                alien_lat20 = alien_lat14;
                alien_lon20 -= (alien_lon14 - alien_lon15) / 20; // Перемещаемся внизу справа налево
                if (alien_lon20 < alien_lon15)
                {
                    alien_lon20 = alien_lon15;
                    track_air = 2;
                }

                fo.course = 270;
                break;
            case 2:
                alien_lat20 += (alien_lat16 - alien_lat15) / 20; // Перемещаемся снизу вверх
                alien_lon20 = alien_lon15;//
                if (alien_lat20 > alien_lat16)
                {
                    alien_lat20 = alien_lat16;
                    track_air = 3;
                }

                fo.course = 1;
                break;
            case 3:

                // Перемещаемся слева направо в исходную точку
                alien_lat20 = alien_lat16;
                alien_lon20 += (alien_lon13 - alien_lon16) / 20; // Перемещаемся внизу справа налево
                if (alien_lon20 > alien_lon13)
                {
                    alien_lon20 = alien_lon13;
                    track_air = 0;
                }
                fo.course = 90;
                break;
            default:
                break;
            }

            if (!alt_high4)
            {
                altitude4 += 50.0;
                if (altitude4 > 800.0)
                {
                    altitude4 = 800.0;
                    alt_high4 = true;
                }
            }
            if (alt_high4)
            {

                altitude4 -= 50.0;
                if (altitude4 < 50.0)
                {
                    altitude4 = 50.0;
                    alt_high4 = false;
                }
            }
            speed4 -= 30.0;
            if (speed4 <= 30.0)
                speed4 = 700.0;


            fo.addr = 0x155C11;
            fo.Squawk = 1501;
            memcpy((char*)fo.flight, fly4, strlen(fly4));
            fo.altitude = altitude4;
            fo.pressure_altitude = altitude4;
            fo.speed = speed4;
            fo.vert_rate = -150;
            fo.signal_source = 1;
            fo.timestamp = now(); // 
            fo.latitude = alien_lat20;
            fo.longitude = alien_lon20;
            fo.aircraft_type = AIRCRAFT_TYPE_JET;
            /* Расчет расстояния, курса и уровня опастности сближения нашего и стороннего самолета*/
            if (fo.latitude != 0.0 && fo.longitude != 0.0) // Расчет возможен если получены координаты нашего и стороннего самолета
            {
                Traffic_Update(&fo);   // 
            }

            /* Остальные параметры записываем в базу */
            Traffic_Add(&fo);
            //******************************************************************************************************************
            break;

        case 5:
            //Тихий океан, 1  Средняя точка
            //0.000200, 179.992963
            ThisAircraft.latitude = 0.000200;    // 
            ThisAircraft.longitude = 179.999960;   // 

            test_curse0 = 360.0;
            speed0 = 200.0;
            altitude0 = 1000.0;

            esp_task_wdt_reset();

            // 0.053769, -179.953328
            // -0.025508, -179.953328
            // -0.025508, 179.953328
            // 0.053769, 179.953328

            //================ Самолет №1 ================================

            switch (track_air)
            {
            case 0:
                /*
                 alien_lat23 = 0.053769;
                 alien_lon23 = -179.953328;

                 alien_lat24 = -0.025508;
                 alien_lon24 = -179.953328;
                */
                alien_lat30 -= (alien_lat23 - alien_lat24) / 20; // Перемещаемся сверху вниз
                alien_lon30 = alien_lon23;//
                if (alien_lat30 < alien_lat24)
                {
                    alien_lat30 = alien_lat24;
                    track_air = 1;
                }

                fo.course = 180;

                break;
            case 1:
                // Serial.println("case 1");
                 /*
                   alien_lat25 = -0.025508;
                   alien_lon25 = 179.953328;
                   alien_lon24 = -179.953328;
                 */
                alien_lat30 = alien_lat24;

                if (alien_lon30 < 0.0 && alien_lon30 > -180)
                {
                    alien_lon30 -= 0.004667; // Перемещаемся внизу справа налево
                }
                if (alien_lon30 <= -180.0)
                {
                    alien_lon30 = 180.0;
                }

                if (alien_lon30 <= 180.0 && alien_lon30 > 0.0)
                {
                    alien_lon30 -= 0.004667; // Перемещаемся внизу справа налево
                }

                if (alien_lon30 > 0.0 && alien_lon30 < alien_lon25)
                {
                    alien_lon30 = alien_lon25;
                    track_air = 2;
                }

                fo.course = 270;
                break;
            case 2:
                alien_lat30 += (alien_lat26 - alien_lat25) / 20; // Перемещаемся снизу вверх
                alien_lon30 = alien_lon25;//
                if (alien_lat30 > alien_lat26)
                {
                    alien_lat30 = alien_lat26;
                    track_air = 3;
                }

                fo.course = 1;
                break;
            case 3:

                // Перемещаемся слева направо в исходную точку
                alien_lat30 = alien_lat26;

                if (alien_lon30 > 0.0 && alien_lon30 < 180.0)
                {
                    alien_lon30 += 0.004667; // Перемещаемся вверху слево направо
                }

                if (alien_lon30 >= 180.0)
                {
                    alien_lon30 = -180.0;
                }

                if (alien_lon30 < 0.0/* && alien_lon30 > -180*/)
                {
                    alien_lon30 += 0.004667; //  Перемещаемся вверху слево направо
                }

                if (alien_lon30 < 0.0 && alien_lon30 > alien_lon23)
                {
                    alien_lon30 = alien_lon23;
                    track_air = 0;
                }

                fo.course = 90;
                break;
            default:
                break;
            }

            if (!alt_high4)
            {
                altitude4 += 50.0;
                if (altitude4 > 800.0)
                {
                    altitude4 = 800.0;
                    alt_high4 = true;
                }
            }
            if (alt_high4)
            {

                altitude4 -= 50.0;
                if (altitude4 < 50.0)
                {
                    altitude4 = 50.0;
                    alt_high4 = false;
                }
            }
            speed4 -= 30.0;
            if (speed4 <= 30.0)
                speed4 = 700.0;


            fo.addr = 0x155C11;
            fo.Squawk = 1501;
            memcpy((char*)fo.flight, fly4, strlen(fly4));
            fo.altitude = altitude4;
            fo.pressure_altitude = altitude4;
            fo.speed = speed4;
            fo.vert_rate = -150;
            fo.signal_source = 1;
            fo.timestamp = now(); // 
            fo.latitude = alien_lat30;
            fo.longitude = alien_lon30;
            fo.aircraft_type = AIRCRAFT_TYPE_JET;
            /* Расчет расстояния, курса и уровня опастности сближения нашего и стороннего самолета*/
            if (fo.latitude != 0.0 && fo.longitude != 0.0) // Расчет возможен если получены координаты нашего и стороннего самолета
            {
                Traffic_Update(&fo);   // 
            }

            /* Остальные параметры записываем в базу */
            Traffic_Add(&fo);
            //******************************************************************************************************************
            break;
        default:
            break;
        }
        TxPosUpdMarker = millis();
    }


#if DEBUG_TIMING
    baro_start_ms = millis();
#endif
    Baro_loop();
#if DEBUG_TIMING
    baro_end_ms = millis();
#endif


#if DEBUG_TIMING
    tx_start_ms = millis();
#endif
    RF_Transmit(RF_Encode(&ThisAircraft), true);
#if DEBUG_TIMING
    tx_end_ms = millis();
    rx_start_ms = millis();
#endif
    success = RF_Receive();
#if DEBUG_TIMING
    rx_end_ms = millis();
#endif

#if DEBUG_TIMING
    parse_start_ms = millis();
#endif
    if (success) ParseData();
#if DEBUG_TIMING
    parse_end_ms = millis();
#endif

    Traffic_loop();

#if DEBUG_TIMING
    led_start_ms = millis();
#endif
    if (isTimeToDisplay())
    {
        LEDTimeMarker = millis();
    }
#if DEBUG_TIMING
    led_end_ms = millis();
#endif


#if DEBUG_TIMING
    export_start_ms = millis();
#endif
    if (isTimeToExport()) {
#if defined(USE_NMEALIB)
        NMEA_Position();
#endif
        NMEA_Export();
        ExportTimeMarker = millis();
    }
#if DEBUG_TIMING
    export_end_ms = millis();
#endif

#if DEBUG_TIMING
    oled_start_ms = millis();
#endif

#if DEBUG_TIMING
    oled_end_ms = millis();
#endif

#if DEBUG_TIMING
    if (baro_start_ms - baro_end_ms) {
        Serial.print(F("Baro start: "));
        Serial.print(baro_start_ms);
        Serial.print(F(" Baro stop: "));
        Serial.println(baro_end_ms);
    }
    if (tx_end_ms - tx_start_ms) {
        Serial.print(F("TX start: "));
        Serial.print(tx_start_ms);
        Serial.print(F(" TX stop: "));
        Serial.println(tx_end_ms);
    }
    if (rx_end_ms - rx_start_ms) {
        Serial.print(F("RX start: "));
        Serial.print(rx_start_ms);
        Serial.print(F(" RX stop: "));
        Serial.println(rx_end_ms);
    }
    if (parse_end_ms - parse_start_ms) {
        Serial.print(F("Parse start: "));
        Serial.print(parse_start_ms);
        Serial.print(F(" Parse stop: "));
        Serial.println(parse_end_ms);
    }
    if (led_end_ms - led_start_ms) {
        Serial.print(F("LED start: "));
        Serial.print(led_start_ms);
        Serial.print(F(" LED stop: "));
        Serial.println(led_end_ms);
    }
    if (export_end_ms - export_start_ms) {
        Serial.print(F("Export start: "));
        Serial.print(export_start_ms);
        Serial.print(F(" Export stop: "));
        Serial.println(export_end_ms);
    }
    if (oled_end_ms - oled_start_ms) {
        Serial.print(F("OLED start: "));
        Serial.print(oled_start_ms);
        Serial.print(F(" OLED stop: "));
        Serial.println(oled_end_ms);
    }
#endif

    // Handle Air Connect
    NMEA_loop();
    ClearExpired();
}

#endif /* EXCLUDE_TEST_MODE */