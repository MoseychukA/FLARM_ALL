/*
 Программа анализатора концентрации кислорода

 Применяется микроконтроллер на базе ESP32   
 ESP32 Dev Module
 
 Версия печатной платы " "
*/

#include <stdio.h>                // define I/O functions
#include <Arduino.h>              // define I/O functions
#include "SPI.h"
#include <TFT_eSPI.h>             // Поддержка TFT дисплея 
#include <SD.h>                   // Поддержка SD карты
#include "SPIFFS.h"
#include "FS.h"
#include "Configuration_ESP32.h"  // Основные настройки программы
#include "Settings.h"             //  
#include "CoreButton.h"           //
#include "CoreCommandBuffer.h"    // обработчик входящих по UART команд
#include <Wire.h>                 // 
#include "AT24CX.h"               // Поддержка энергонезависимой памяти

/* Файлы SoftRF*/
#include "WiFi.h"
#include "SoftRF.h"
#include "WiFiHelper.h"
//#include "src/system/OTA.h"
//#include "src/system/Time.h"
//#include "src/system/SoC.h"
#include "ESP32.h"



#if !defined(SERIAL_FLUSH)
#define SERIAL_FLUSH() Serial.flush()
#endif

#define DEBUG 0
#define DEBUG_TIMING 0

#define isTimeToDisplay() (millis() - LEDTimeMarker     > 1000)
#define isTimeToExport()  (millis() - ExportTimeMarker  > 1000)

//ufo_t ThisAircraft;
//
//hardware_info_t hw_info = {
//  //.model = DEFAULT_SOFTRF_MODEL,
//  .revision = 0,
//  .soc = SOC_NONE,
// // .rf = RF_IC_NONE,
// // .gnss = GNSS_MODULE_NONE,
// // .baro = BARO_MODULE_NONE,
// // .display = DISPLAY_NONE,
//  .storage = STORAGE_NONE,
//  .rtc = RTC_NONE,
//  .imu = IMU_NONE,
//  .mag = MAG_NONE,
// // .pmu = PMU_NONE,
//};
//
//unsigned long LEDTimeMarker = 0;
//unsigned long ExportTimeMarker = 0;
//






//====================================================================================
AT24CX memWiFi;

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
const char* host = "esp32";
//const char* ssid = "DAP-1155";
//const char* ssid = "ASUS";
//const char* password = "panasonic";
//WebServer server(80);



#ifdef USE_TFT_MODULE
#include "TFTModule.h"
#endif



#ifdef USE_TFT_MODULE
TFTModule tftModule;

#endif

//--------------------------------------------------------------------------------------------------------------------------------
bool canCallYield = false;
//--------------------------------------------------------------------------------------------------------------------------------
bool lcd_ON = false;
uint32_t screenIdleTimer = 0;
uint32_t backlightTimer = 0;
uint32_t powerOffTimer = 0;
bool power_ON = false;

//--------------------------------------------------------------------------------------------------------------------------------
void screenAction(AbstractTFTScreen* screen)
{
    // какое-то действие на экране произошло.
    // тут просто сбрасываем таймер ничегонеделанья.
    screenIdleTimer = millis();           // Таймер переключения на главный экран
    backlightTimer = millis();            // Таймер отключения подсветки дисплея
	powerOffTimer = millis();             // Таймер отключения питания прибора
}
//--------------------------------------------------------------------------------------------------------------------------------
void batteryPowerOn()                     // Включение питания от аккумулятора
{
    int i = 0;
    int time_i = 20;                      // время нажатия на кнопку включения питания.
 /*   do
    {
        delay(100);         
        if (!digitalRead(POWER_ON_IN))
        {
            i++;

            power_ON = true;
        }
        else
        {
            power_ON = false;
            break;
        }

    } while (i < time_i);*/

}

//--------------------------------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------------------------------
/*
 * Login page
 */
const char* loginIndex = 
"<form name='loginForm'>"
"<table width='20%' bgcolor='A09F9F' align='center'>"
"<tr>"
"<td colspan=2>"
"<center><font size=4><b>ESP32 Login Page</b></font></center>"
"<br>"
"</td>"
"<br>"
"<br>"
"</tr>"
"<td>Username:</td>"
"<td><input type='text' size=25 name='userid'><br></td>"
"</tr>"
"<br>"
"<br>"
"<tr>"
"<td>Password:</td>"
"<td><input type='Password' size=25 name='pwd'><br></td>"
"<br>"
"<br>"
"</tr>"
"<tr>"
"<td><input type='submit' onclick='check(this.form)' value='Login'></td>"
"</tr>"
"</table>"
"</form>"
"<script>"
"function check(form)"
"{"
"if(form.userid.value=='admin' && form.pwd.value=='admin')"
"{"
"window.open('/serverIndex')"
"}"
"else"
"{"
" alert('Error Password or Username')/*displays error message*/"
"}"
"}"
"</script>";
/*
 * Server Index Page
 */
const char* serverIndex =
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
"<input type='file' name='update'>"
"<input type='submit' value='Update'>"
"</form>"
"<div id='prg'>progress: 0%</div>"
"<script>"
"$('form').submit(function(e){"
"e.preventDefault();"
"var form = $('#upload_form')[0];"
"var data = new FormData(form);"
" $.ajax({"
"url: '/update',"
"type: 'POST',"
"data: data,"
"contentType: false,"
"processData:false,"
"xhr: function() {"
"var xhr = new window.XMLHttpRequest();"
"xhr.upload.addEventListener('progress', function(evt) {"
"if (evt.lengthComputable) {"
"var per = evt.loaded / evt.total;"
"$('#prg').html('progress: ' + Math.round(per*100) + '%');"
"}"
"}, false);"
"return xhr;"
"},"
"success:function(d, s) {"
"console.log('success!')"
"},"
"error: function (a, b, c) {"
"}"
"});"
"});"
"</script>";
/*
 * setup function
 */
 //--------------------------------------------------------------------------------------------------------------------------------

void bridge(void* pvParameters)
{
	//// Connect to WiFi network


	//char ssid[20] = "";
	//char password[20] = "";

	//memWiFi.readChars(ROUTER_ID_EEPROM_ADDR + 2, ssid, sizeof(ssid));

	//Serial.print("SSID =  ");
	//Serial.println(ssid);
	//delay(100);
	//memWiFi.readChars(ROUTER_PASSWORD_EEPROM_ADDR + 2, password, sizeof(password));
	//Serial.print("Password =  ");
	//Serial.println(password);

	//WiFi.begin(ssid, password);
	//int count_connect = 0;
	//SerialDEBUG.println("Wait for connection");
	//// Wait for connection
	//while (WiFi.status() != WL_CONNECTED)
	//{
	//	delay(500);
	//	SerialDEBUG.print(".");
	//	count_connect++;
	//	if (count_connect > 20)
	//	{
	//		break;
	//	}
	//}

	//if (WiFi.status() == WL_CONNECTED)
	//{
	//	SerialDEBUG.println("");
	//	SerialDEBUG.print("Connected to ");
	//	SerialDEBUG.println(ssid);
	//	SerialDEBUG.print("IP address: ");
	//	SerialDEBUG.println(WiFi.localIP());
	//	Settings.SetWiFiConnect(true);
	//	/*use mdns for host name resolution*/
	//	if (!MDNS.begin(host)) { //http://esp32.local
	//		SerialDEBUG.println("Error setting up MDNS responder!");
	//		while (1) {
	//			delay(1000);
	//		}
	//	}
	//	SerialDEBUG.println("mDNS responder started");
	//	/*return index page which is stored in serverIndex */
	//	server.on("/", HTTP_GET, []() {
	//		server.sendHeader("Connection", "close");
	//		server.send(200, "text/html", loginIndex);
	//	});
	//	server.on("/serverIndex", HTTP_GET, []() {
	//		server.sendHeader("Connection", "close");
	//		server.send(200, "text/html", serverIndex);
	//	});
	//	/*handling uploading firmware file */
	//	server.on("/update", HTTP_POST, []() {
	//		server.sendHeader("Connection", "close");
	//		server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
	//		ESP.restart();
	//	}, []() {
	//		HTTPUpload& upload = server.upload();
	//		if (upload.status == UPLOAD_FILE_START) {
	//			SerialDEBUG.printf("Update: %s\n", upload.filename.c_str());
	//			if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
	//				Update.printError(SerialDEBUG);
	//			}
	//		}
	//		else if (upload.status == UPLOAD_FILE_WRITE) {
	//			/* flashing firmware to ESP*/
	//			if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
	//				Update.printError(SerialDEBUG);
	//			}
	//		}
	//		else if (upload.status == UPLOAD_FILE_END) {
	//			if (Update.end(true)) { //true to set the size to the current progress
	//				SerialDEBUG.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
	//			}
	//			else {
	//				Update.printError(SerialDEBUG);
	//			}
	//		}
	//	});
	//	server.begin();
	//}
	//else
	//{
	//	SerialDEBUG.println("");
	//	SerialDEBUG.println("****** Not connected to WiFi! *******");
	//	Settings.SetWiFiConnect(false);
	//}

	//while (true)
	//{
	//	server.handleClient();
	//	delay(10);

	//	static uint32_t tmr = millis();
	//	if (millis() - tmr > 2000)
	//	{
	//		if (WiFi.status() == WL_CONNECTED)
	//		{
	//			Settings.SetWiFiConnect(true);
	//			Settings.SetWiFiState(false);
	//		}
	//		else
	//		{
	//			Settings.SetWiFiConnect(false);
	//			Settings.SetWiFiState(true);
	//		}


	//		tmr = millis();
	//	}



	//}
}
//--------------------------------------------------------------------------------------------------------------------------------




void setup() 
{
  canCallYield = false;

  // поднимаем первый UART
  SerialDEBUG.begin(Serial_SPEED);
  while (!SerialDEBUG && millis() < 1000);

  Settings.initBoard();                  // Инициализация модулей устройства
 // SPIFFS.begin(true);
  
  Settings.setup();                     // настраиваем хранилище в EEPROM. Настраиваем кнопку управления питанием

  pinMode(23, OUTPUT);            // 
  digitalWrite(23, HIGH);         // 

  pinMode(18, OUTPUT);            // 
  digitalWrite(18, HIGH);         // 
 

  
#ifdef USE_TFT_MODULE 
 tftModule.Setup();
#endif

 screenIdleTimer = millis();

 TFTScreen->onAction(screenAction);  // 


 // Печатаем в SerialDEBUG готовность
 SerialDEBUG.println("READY");
 


 //SerialDEBUG.println();

 if (!SPIFFS.begin(true)) {
     SerialDEBUG.println("An Error has occurred while mounting SPIFFS");
     //  "При монтировании SPIFFS возникла ошибка"
     return;
 }

 

 // // тест EEPROM
  SerialDEBUG.println();

 screenIdleTimer = millis();         // Таймер переключения на главный экран
 backlightTimer  = millis();         // Таймер отключения подсветки дисплея
 powerOffTimer   = millis();         // Таймер отключения питания прибора

  // выводим в UART версию прошивки
 CommandHandler.getVER(&SerialDEBUG);
 
 canCallYield = true;
 //SerialDEBUG.print("setup() running on core ");
 ////  "Блок setup() выполняется на ядре "
 //SerialDEBUG.println(xPortGetCoreID());


 // xTaskCreatePinnedToCore(
 //Task1code, /* Функция, содержащая код задачи */
 // "Task1", /* Название задачи */
 //     10000, /* Размер стека в словах */
 //     NULL, /* Параметр создаваемой задачи */
 //     0, /* Приоритет задачи */
 //     & Task1, /* Идентификатор задачи */
 //     0); /* Ядро, на котором будет выполняться задача */
 // 

 /*xTaskCreatePinnedToCore(bridge, "bridge", 4096, NULL, 1, NULL, 0);*/

 WiFi_setup();




 disableCore0WDT();
 disableCore1WDT();
 disableLoopWDT(); // You forgot this one !

 // Settings.TestI2C();

 
 /*pinMode(left_button, INPUT_PULLUP);
 pinMode(right_button, INPUT_PULLUP);*/


 /*tft.init();
 tft.fillScreen(background1);
 tft.setSwapBytes(true);
 tft.drawString("LOCATION", 10, 300);
 tft.setTextColor(TFT_ORANGE, background1);
 tft.loadFont(NotoSansBold15);
 tft.drawString("43768554'", 10, 260);
 tft.loadFont(NotoSansBold15);
 tft.drawString("23758554'", 10, 280);
 tft.pushImage(100, 260, 48, 48, position);

 data.createSprite(140, 60);
 data.loadFont(NotoSansMonoSCB20);
 data.setTextColor(TFT_WHITE, background2);

 bck.createSprite(171, 171);
 bck.setSwapBytes(true);

 sprite.createSprite(171, 171);
 sprite.setSwapBytes(true);

 sprite1.createSprite(121, 25);
 sprite.setPivot(85, 85);*/
  

 SerialDEBUG.flush();
}

//int angle = 0;
//int angle2 = 359;
//int sx = 60;
//int sy = 12;

//void drawData()
//{
//	data.fillSprite(background2);
//	data.drawString("needle: " + String(angle), 10, 8);
//	data.drawString("compas: " + String(angle2), 10, 34);
//	data.pushSprite(15, 190);
//}

//void drawCompas()
//{
//	bck.fillSprite(0x29CC);
//	bck.fillSmoothCircle(152, 5, 5, TFT_ORANGE);
//	sprite.fillSprite(0x29CC);
//	sprite.pushImage(1, 1, 170, 170, compas);
//	sprite1.fillSprite(TFT_WHITE);
//
//	sprite1.drawWedgeLine(6, sy, sx, sy, 1, 10, TFT_RED);
//	sprite1.drawWedgeLine(sx, sy, 115, sy, 10, 1, TFT_BLUE);
//	sprite1.fillSmoothCircle(sx, sy, 12, gray);
//	sprite1.fillSmoothCircle(sx, sy, 6, TFT_WHITE);
//
//	sprite.pushRotated(&bck, angle2, background1);
//	sprite1.pushRotated(&bck, angle, TFT_WHITE);
//	bck.pushSprite(0, 8);
//}


//--------------------------------------------------------------------------------------------------------------------------------
void loop()
{

//	 if(digitalRead(left_button)==0)
//  angle=angle+4;
//  if(angle==360)
//  angle=0;
//  if(digitalRead(right_button)==0)
//   angle2=angle2-2;
//  if(angle2<0)
//  angle2=359;
//
//drawCompas();
//drawData();




	Settings.update();                    // Проверяем состояние кнопки питания
 
	#ifdef USE_TFT_MODULE
		tftModule.Update();
	#endif 


#ifdef _COM_COMMANDS_OFF
    // обрабатываем входящие команды
    CommandHandler.handleCommands();
#endif // _COM_COMMANDS_OFF


	WiFi_loop();




}


//--------------------------------------------------------------------------------------------------------------------------------
