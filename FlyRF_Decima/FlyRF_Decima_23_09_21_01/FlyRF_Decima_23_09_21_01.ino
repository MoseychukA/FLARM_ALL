/*
 Программа анализатора концентрации кислорода

 Применяется микроконтроллер на базе ESP32   
 ESP32 Dev Module
 
 Версия печатной платы " "
*/

#include <stdio.h>                // define I/O functions
#include <Arduino.h>              // define I/O functions
#include "SPI.h"
#include <SD.h>                   // Поддержка SD карты
#include "SPIFFS.h"
#include "FS.h"
#include "Configuration_ESP32.h"  // Основные настройки программы
#include "Settings.h"             //  
#include "CoreButton.h"           //
#include "CoreCommandBuffer.h"    // обработчик входящих по UART команд
#include <Wire.h>                 // 


#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
const char* host = "esp32";
//const char* ssid = "DAP-1155";
//const char* ssid = "ASUS";
//const char* password = "panasonic";
WebServer server(80);

#include <Preferences.h>

Preferences preferences;

const char* ssid = "ASUS";
const char* password = "panasonic";





#ifdef USE_TFT_MODULE
#include "TFTModule.h"
#endif

char daysOfTheWeek[7][12] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };

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

	// Connect to WiFi network
	/*memWiFi.begin("my-app");;*/
	//char ssid[20] = "";
	//char password[20] = "";
	String ssid;
	String password;

	//preferences.putString("SSID", ssid);
	//preferences.putString("Password", password);

	preferences.begin("credentials", false);

	ssid = preferences.getString("ssid", "");
	password = preferences.getString("password", "");

	if (ssid == "" || password == "") {
		Serial.println("No values saved for ssid or password");
	}



	//String ssid1 = "";
	//String password1 = "";

	//ssid1 = Settings.GetRouterSSID();
	//memWiFi.readChars(ROUTER_ID_EEPROM_ADDR + 2, ssid, sizeof(ssid));
	//preferences.getString(ssid, ssid);
	Serial.print("SSID =  ");
	Serial.println(ssid);
	delay(100);
	//preferences.getString(password, password);
	//password1 = Settings.GetRouterPassword();
	//memWiFi.readChars(ROUTER_PASSWORD_EEPROM_ADDR + 2, password, sizeof(password));
	Serial.print("Password =  ");
	Serial.println(password);

	WiFi.begin(ssid, password);
	int count_connect = 0;
	SerialDEBUG.println("Wait for connection");
	// Wait for connection
	while (WiFi.status() != WL_CONNECTED)
	{
		delay(500);
		SerialDEBUG.print(".");
		count_connect++;
		if (count_connect > 20)
		{
			break;
		}
	}

	if (WiFi.status() == WL_CONNECTED)
	{
		SerialDEBUG.println("");
		SerialDEBUG.print("Connected to ");
		SerialDEBUG.println(ssid);
		SerialDEBUG.print("IP address: ");
		SerialDEBUG.println(WiFi.localIP());
		Settings.SetWiFiConnect(true);
		/*use mdns for host name resolution*/
		if (!MDNS.begin(host)) { //http://esp32.local
			SerialDEBUG.println("Error setting up MDNS responder!");
			while (1) {
				delay(1000);
			}
		}
		SerialDEBUG.println("mDNS responder started");
		/*return index page which is stored in serverIndex */
		server.on("/", HTTP_GET, []() {
			server.sendHeader("Connection", "close");
			server.send(200, "text/html", loginIndex);
		});
		server.on("/serverIndex", HTTP_GET, []() {
			server.sendHeader("Connection", "close");
			server.send(200, "text/html", serverIndex);
		});
		/*handling uploading firmware file */
		server.on("/update", HTTP_POST, []() {
			server.sendHeader("Connection", "close");
			server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
			ESP.restart();
		}, []() {
			HTTPUpload& upload = server.upload();
			if (upload.status == UPLOAD_FILE_START) {
				SerialDEBUG.printf("Update: %s\n", upload.filename.c_str());
				if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
					Update.printError(SerialDEBUG);
				}
			}
			else if (upload.status == UPLOAD_FILE_WRITE) {
				/* flashing firmware to ESP*/
				if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
					Update.printError(SerialDEBUG);
				}
			}
			else if (upload.status == UPLOAD_FILE_END) {
				if (Update.end(true)) { //true to set the size to the current progress
					SerialDEBUG.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
				}
				else {
					Update.printError(SerialDEBUG);
				}
			}
		});
		server.begin();
	}
	else
	{
		SerialDEBUG.println("");
		SerialDEBUG.println("****** Not connected to WiFi! *******");
		Settings.SetWiFiConnect(false);
	}

	while (true)
	{
		server.handleClient();
		delay(10);

		static uint32_t tmr = millis();
		if (millis() - tmr > 2000)
		{
			if (WiFi.status() == WL_CONNECTED)
			{
				Settings.SetWiFiConnect(true);
			}
			else
			{
				Settings.SetWiFiConnect(false);
			}


			tmr = millis();
		}



	}
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
 
  preferences.begin("credentials", false);
  preferences.putString("ssid", ssid);
  preferences.putString("password", password);

  Serial.println("Network Credentials Saved using Preferences");

  preferences.end();


  Settings.setup();                     // настраиваем хранилище в EEPROM. Настраиваем кнопку управления питанием
  pinMode(23, OUTPUT);            // 
  digitalWrite(13, HIGH);         // 

  pinMode(18, OUTPUT);            // 
  digitalWrite(18, HIGH);         // 
 

 /* if (Settings.getPowerType() == batteryPower)
  {
     batteryPowerOn();
     if (power_ON == true)
     {
         pinMode(POWER_ON_OUT, OUTPUT);
         digitalWrite(POWER_ON_OUT, HIGH);

     }
  }
  else
  {

  }*/


  
#ifdef USE_TFT_MODULE 
 tftModule.Setup();
#endif

 screenIdleTimer = millis();

 TFTScreen->onAction(screenAction);  // 


 // Печатаем в SerialDEBUG готовность
 SerialDEBUG.println("READY");
 

#ifdef USE_DS3231_REALTIME_CLOCK

 RTC_DS3231  rtc = Settings.GetClock();
 DateTime now = rtc.now();

 //rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); // Установка даты и времени ПК  при загрузки программы

 SerialDEBUG.print(now.year(), DEC);
 SerialDEBUG.print('/');
 SerialDEBUG.print(now.month(), DEC);
 SerialDEBUG.print('/');
 SerialDEBUG.print(now.day(), DEC);
 SerialDEBUG.print(" (");
 SerialDEBUG.print(daysOfTheWeek[now.dayOfWeek()]);
 SerialDEBUG.print(") ");
 SerialDEBUG.print(now.hour(), DEC);
 SerialDEBUG.print(':');
 SerialDEBUG.print(now.minute(), DEC);
 SerialDEBUG.print(':');
 SerialDEBUG.print(now.second(), DEC);
 SerialDEBUG.println();

#endif 

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

 xTaskCreatePinnedToCore(bridge, "bridge", 4096, NULL, 1, NULL, 0);

 disableCore0WDT();
 disableCore1WDT();
 disableLoopWDT(); // You forgot this one !

 
 SerialDEBUG.flush();
}


uint16_t touch_x, touch_y;




//--------------------------------------------------------------------------------------------------------------------------------
void loop()
{

	Settings.update();                    // Проверяем состояние кнопки питания
 
	//lcd_ON = Settings.isBacklightOn();
	//int time_LCD_Led = Settings.GetTimeLedLCD();
	//int time_PowerOff = Settings.GetTimePowerOff();
	#ifdef USE_TFT_MODULE
		tftModule.Update();
	#endif 

	

	//if (Settings.getPowerType() == powerViaUSB)
	//{
	//	screenIdleTimer = millis();
	//	Settings.displayBacklight(true);
	//}
	//else
	//{
	//	if (lcd_ON)
	//	{

	//		if (millis() - screenIdleTimer >= RESET_TO_MAIN_SCREEN_DELAY) // через XX секунд ничегонеделанья переключаемся на главный экран
	//		{
	//			AbstractTFTScreen* activeScreen = TFTScreen->getActiveScreen();
	//			if (activeScreen != TouchCalibrationScreen) // пока идёт калибровка тача - переключаться никуда нельзя
	//			{
	//				if (activeScreen != MainScreen)
	//					TFTScreen->switchToScreen(MainScreen);

	//			}

	//			screenIdleTimer = millis();
	//		}

	//		//// При питании от внутреннего источника, отключать подсветку дисплея через XX минут при отсутствии активности на кнопках
	//		//if (Settings.getPowerType() == batteryPower)
	//		//{
	//		//	if (millis() - backlightTimer > (time_LCD_Led * 1000))
	//		//	{
	//		//		// backlightTimer = millis();
	//		//		TFTScreen->resetIdleTimer();
	//		//		Settings.displayBacklight(false);
	//		//	}
	//		//}
	//	}
	//	else if (Settings.getPowerType() == batteryPower)
	//	{

	//		if (Settings.getTouch() == false)
	//		{

	//			Settings.displayBacklight(true); // включаем подсветку
	//			TFTScreen->resetIdleTimer();

	//			while (Settings.getTouch() == false)
	//			{
	//				yield();
	//			}
	//			delay(1000);

	//		}
	//	}

	//	// При питании от внутреннего источника, отключать прибор через XX минут при отсутствии активности на кнопках
	//	//if (Settings.getPowerType() == batteryPower)
	//	//{
	//	//	if (millis() - powerOffTimer > (time_PowerOff * 1000))
	//	//	{
	//	//		//powerOffTimer = millis();  // Таймер отключения питания прибора

	//	//		Settings.turnPowerOff();
	//	//	}
	//	//}
	//}


#ifdef _COM_COMMANDS_OFF
    // обрабатываем входящие команды
    CommandHandler.handleCommands();



#endif // _COM_COMMANDS_OFF


}


//--------------------------------------------------------------------------------------------------------------------------------
