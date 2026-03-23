
#include "SoC.h"

#if defined(EXCLUDE_WIFI)
void Web_setup()    {}
void Web_loop()     {}
void Web_fini()     {}
#else

#include <Arduino.h>

#include "RF.h"
#include "WebRF.h"
#include "Baro.h"
#include "Bluetooth.h"
#include "TrafficHelper.h"
#include "NMEA.h"
#include "TimeRF.h"
#include "ServiceMain.h"

static uint32_t prev_rx_pkt_cnt = 0;


#include "jquery_min_js.h"

byte getVal(char c)
{
   if(c >= '0' && c <= '9')
     return (byte)(c - '0');
   else
     return (byte)(toupper(c)-'A'+10);
}


static const char about_html[] PROGMEM = "<html>\
  <head>\
    <meta name='viewport' content='width=device-width, initial-scale=1'>\
    <title>About</title>\
  </head>\
<body>\
<h1 align=center>Информация</h1>\
<p align=center>== Эта программа часть проекта FlyRF ==</p>\
<p align=center>== Базовый LAN модуль ==</p>\
<p>URL: https://t.me/flyrf_Support</p>\
<p>URL: https://www.decima.ru/contacts/</p>\
<p>E-mail: decima@decima.ru</p>\
<table width=100%%>\
</table>\
<hr>\
Copyright (C) 2023-2026 &nbsp;&nbsp;&nbsp; OOO Децима\
</body>\
</html>";

void handleSettings() // Ручной ввод основных настроек
{

  size_t size = 5100;
  char *offset;
  size_t len = 0;
  char *Settings_temp = (char *) malloc(size);

  if (Settings_temp == NULL) {
    return;
  }

  offset = Settings_temp;

  /* Common part 2 */
  snprintf_P ( offset, size,
    PSTR("\
<head>\
<meta name='viewport' content='width=device-width, initial-scale=1'>\
<title>Settings</title>\
</head>\
<body>\
<h1 align=center>Настройки</h1>\
<form action='/input' method='GET'>\
<table width=100%%>\
<tr>\
<th align = left>TRACKER send</th>\
<td align = right>\
<select name = 'tracker_send'>\
<option % s value = '%d'>Off</option>\
<option % s value = '%d'>Single</option>\
<option % s value = '%d'>Auto</option>\
<option % s value = '%d'>Mini</option>\
</select>\
</td>\
</tr>\
"),
  (settings->tracker_send == TRACKER_SEND_OFF ? "selected" : ""), TRACKER_SEND_OFF,
  (settings->tracker_send == TRACKER_SEND_SINGLE ? "selected" : ""), TRACKER_SEND_SINGLE,
  (settings->tracker_send == TRACKER_SEND_AUTO ? "selected" : ""), TRACKER_SEND_AUTO,
  (settings->tracker_send == TRACKER_SEND_MINI ? "selected" : ""), TRACKER_SEND_MINI
  );

  len = strlen(offset);
  offset += len;
  size -= len;

//#if !defined(EXCLUDE_BLUETOOTH)
  /* SoC specific part 1 */
  if (SoC->id == SOC_ESP32S3) 
  {
    snprintf_P ( offset, size,
      PSTR("\
<tr>\
<th align=left>Built-in Bluetooth</th>\
<td align=right>\
<select name='bluetooth'>\
<option %s value='%d'>Off</option>\
<option %s value='%d'>SPP</option>\
<option %s value='%d'>LE</option>\
</select>\
</td>\
</tr>"),
    (settings->bluetooth == BLUETOOTH_NONE ? "selected" : ""), BLUETOOTH_NONE,
    (settings->bluetooth == BLUETOOTH_SPP  ? "selected" : ""), BLUETOOTH_SPP,
    (settings->bluetooth == BLUETOOTH_LE_HM10_SERIAL ? "selected" : ""), BLUETOOTH_LE_HM10_SERIAL
    );

    len = strlen(offset);
    offset += len;
    size -= len;

  }

//#endif /* EXCLUDE_BLUETOOTH */

  /* Common part 3 */
  snprintf_P ( offset, size,
    PSTR("\
<tr>\
<th align=left>NMEA output</th>\
<td align=right>\
<select name='nmea_out'>\
<option %s value='%d'>Off</option>\
<option %s value='%d'>Serial</option>\
<option %s value='%d'>UDP</option>"),

  (settings->nmea_out == NMEA_OFF  ? "selected" : ""), NMEA_OFF,
  (settings->nmea_out == NMEA_UART ? "selected" : ""), NMEA_UART,
  (settings->nmea_out == NMEA_UDP  ? "selected" : ""), NMEA_UDP);

  len = strlen(offset);
  offset += len;
  size -= len;

  /* Common part 4 */

      snprintf_P(offset, size,
          PSTR("\
<option %s value='%d'>TCP</option>\
<option %s value='%d'>Bluetooth</option>"),
(settings->nmea_out == NMEA_TCP ? "selected" : ""), NMEA_TCP,
(settings->nmea_out == NMEA_BLUETOOTH ? "selected" : ""), NMEA_BLUETOOTH);

      len = strlen(offset);
      offset += len;
      size -= len;

  snprintf_P(offset, size,
      PSTR("\
<tr>\
<th align=left>Полушарие(северное или южное) </th>\
<td align=right>\
<select name='input_N_S'>\
<option % s value='%d'>N</option>\
<option % s value='%d'>S</option>\
</select>\
</td>\
</tr>\
<tr>\
<th align=left>latitude(широта 0&#176; - 90&#176) </th>\
<td align=right>\
   <INPUT type='tel' name='local_latitude' pattern='[0-9]{1,2}\\.[0-9]{5}' size='10' maxlength='10' min='0' max='90' value='%.05f' title='Введите широту в формате: 1-2 цифры, точка, 5 цифр (например: 7.12345, 37.12345)'>\
</td>\
</tr>\
<tr>\
<th align=left>Долгота(восточная или западная)</th>\
<td align=right>\
<select name='input_E_W'>\
<option % s value='%d'>E</option>\
<option % s value='%d'>W</option>\
</select>\
</td>\
</tr>\
<tr>\
<th align=left>longitude(долгота 0&#176 - 180&#176) Формат: от 1 до 3 цифр.5 цифр</th>\
<td align=right>\
<INPUT type='tel' name='local_longitude' pattern='[0-9]{1,3}\\.[0-9]{5}' size='11' maxlength='11' min='0' max='180' value='%.05f' title='Введите долготу в формате: 1-3 цифры, точка, 5 цифр (например: 7.12345, 37.12345, 137.12345)'>\
</td>\
</tr>\
"),
(settings->input_N_S == IMPUT_N ? "selected" : ""), IMPUT_N,
(settings->input_N_S == IMPUT_S ? "selected" : ""), IMPUT_S,
settings->local_latitude,
(settings->input_E_W == IMPUT_E ? "selected" : ""), IMPUT_E,
(settings->input_E_W == IMPUT_W ? "selected" : ""), IMPUT_W,
settings->local_longitude
);

  len = strlen(offset);
  offset += len;
  size -= len;

    snprintf_P(offset, size,
      PSTR("<tr>\
    <th align=left>Local IP(UDP) address</th>\
    <td align=right>\
    <input type='text' name='local_ip' size='15' maxlength='15' pattern='[0-9]{1,3}(\\.[0-9]{1,3}){3}' \
           value='%d.%d.%d.%d' placeholder='192.168.1.50'>\
    </td></tr>\
    <tr>\
    <th align=left>Gateway IP(UDP) address</th>\
    <td align=right>\
    <input type='text' name='gateway_ip' size='15' maxlength='15' pattern='[0-9]{1,3}(\\.[0-9]{1,3}){3}' \
           value='%d.%d.%d.%d' placeholder='192.168.1.1'>\
    </td></tr>\
    <tr>\
    <th align=left>Subnet mask(UDP)</th>\
    <td align=right>\
    <input type='text' name='subnet_mask' size='15' maxlength='15' pattern='[0-9]{1,3}(\\.[0-9]{1,3}){3}' \
           value='%d.%d.%d.%d' placeholder='255.255.255.0'>\
    </td></tr>\
    <tr>\
    <th align=left>DNS Server(UDP)</th>\
    <td align=right>\
    <input type='text' name='dns_server' size='15' maxlength='15' pattern='[0-9]{1,3}(\\.[0-9]{1,3}){3}' \
           value='%d.%d.%d.%d' placeholder='8.8.8.8'>\
    </td></tr>\
    "),
      // подставляем текущие переменные
      settings->g_localIP[0], settings->g_localIP[1], settings->g_localIP[2], settings->g_localIP[3],
      settings->g_gatewayIP[0], settings->g_gatewayIP[1], settings->g_gatewayIP[2], settings->g_gatewayIP[3],
      settings->g_subnetMask[0], settings->g_subnetMask[1], settings->g_subnetMask[2], settings->g_subnetMask[3],
      settings->g_dns_server[0], settings->g_dns_server[1], settings->g_dns_server[2], settings->g_dns_server[3]);

  len = strlen(offset);
  offset += len;
  size -= len;





  /* Common part 8 */
  snprintf_P ( offset, size,
    PSTR("\
</table>\
<p align=center><INPUT type='submit' value='Сохранить и обновить'></p>\
</form>\
</body>\
</html>")
  );

  SoC->swSer_enableRx(false);
  server.sendHeader(String(F("Cache-Control")), String(F("no-cache, no-store, must-revalidate")));
  server.sendHeader(String(F("Pragma")), String(F("no-cache")));
  server.sendHeader(String(F("Expires")), String(F("-1")));
  server.send ( 200, "text/html", Settings_temp );
  SoC->swSer_enableRx(true);
  free(Settings_temp);
}



//===
void hardwareSettings() //Ручной ввод аппаратных настроек
{
    //hardware_settings
    size_t size = 5420;
    char* offset;
    size_t len = 0;
    char* Settings_temp = (char*)malloc(size);

    if (Settings_temp == NULL) {
        return;
    }

    offset = Settings_temp;

    /* Common part 1 */

    snprintf_P(offset, size,
        PSTR("<html>\
    <head>\
    <meta name='viewport' content='width=device-width, initial-scale=1'>\
    <title>hardwareSettings</title>\
    </head>\
    <body>\
    <h1 align=center>Аппаратные настройки</h1>\
    <form action='/hardware_input' method='GET'>\
    <table width=100%%>\
    <tr>\
    <th align=left>Отобразить уровень сигнала LoRa</th>\
    <td align=right>\
    <select name='rssi_view'>\
    <option % s value = '%d'>Выключен</option>\
    <option % s value = '%d'>Включен</option>\
    </select>\
    </td>\
    </tr>\
    <tr>\
    <th align=left>Вывод информации с базового модуля</th>\
    <td align=right>\
    <select name='display_set'>\
    <option % s value = '%d'>Нет вывода</option>\
    <option % s value = '%d'>Только с координатами</option>\
    <option % s value = '%d'>Полная информация</option>\
    </select>\
    </td>\
    </tr>\
    <tr>\
    <th align=left>Вывод данных в Serial</th>\
    <td align=right>\
    <select name='serial_out'>\
    <option % s value = '%d'>Не выводить</option>\
    <option % s value = '%d'>Данные в базе самолетов</option>\
    <option % s value = '%d'>Техничесские данные</option>\
    </select>\
    </td>\
    </tr>\
    <tr>\
    <th align=left>Отображение тестовых координат</th>\
    <td align=right>\
    <select name='view_test_coord'>\
    <option % s value='%d'>Выключить</option>\
    <option % s value='%d'>Включить</option>\
    </select>\
    </td>\
    </tr>\
    <tr>\
    <th align=left>Установить настройки по умолчанию</th>\
    <td align=right>\
    <select name='default_settings'>\
    <option % s value = '%d'>Не устанавливать</option>\
    <option % s value = '%d'>Установить</option>\
    </select>\
    </td>\
    </tr>\
    <tr>\
    <th align=left>Блокировка адреса стороннего устройства</th>\
    <td align=right>\
    <INPUT type='text' name='block_addr' maxlength='6' size='6' value='%06X'>\
    </td>\
    </tr>\
    <tr>\
    <th align = left>Уровень порога усилителя 1090мГц (мв)</th>\
    <td align = right>\
    <INPUT type = 'number' name = 'threshold_level' min = '300' max = '1400'  step = '0.01' value = '%d'>\
    </td>\
    </tr>"),

    (settings->rssi_view == VIEW_RSSI_OFF ? "selected" : ""), VIEW_RSSI_OFF,
    (settings->rssi_view == VIEW_RSSI_ON ? "selected" : ""), VIEW_RSSI_ON,
    (settings->display_set == INFO_DISTLAY_OFF ? "selected" : ""), INFO_DISTLAY_OFF,
    (settings->display_set == INFO_DISPLAY_COORDINATE ? "selected" : ""), INFO_DISPLAY_COORDINATE,
    (settings->display_set == INFO_DISPLAY_MAXI ? "selected" : ""), INFO_DISPLAY_MAXI,
    (settings->serial_out == SEND_SERIAL_OFF ? "selected" : ""), SEND_SERIAL_OFF,
    (settings->serial_out == SEND_SERIAL_DISPLAY ? "selected" : ""), SEND_SERIAL_DISPLAY,
    (settings->serial_out == SEND_SERIAL_TECHNICAL_INFO ? "selected" : ""), SEND_SERIAL_TECHNICAL_INFO,
    (settings->view_test_coord == VIEW_COORD_OFF ? "selected" : ""), VIEW_COORD_OFF,
    (settings->view_test_coord == VIEW_COORD_ON ? "selected" : ""), VIEW_COORD_ON,
    (settings->default_settings == SETTINGS_OFF ? "selected" : ""), SETTINGS_OFF,
    (settings->default_settings == SETTINGS_ON ? "selected" : ""), SETTINGS_ON,
    settings->block_addr,
    settings->threshold_level

    );

        len = strlen(offset);
        offset += len;
        size -= len;


        /* Common part 8 */
        snprintf_P(offset, size,
            PSTR("\
    </table>\
    <p align=center><INPUT type='submit' value='Сохранить и обновить'></p>\
    </form>\
    </body>\
    </html>")
    );

        SoC->swSer_enableRx(false);
        server.sendHeader(String(F("Cache-Control")), String(F("no-cache, no-store, must-revalidate")));
        server.sendHeader(String(F("Pragma")), String(F("no-cache")));
        server.sendHeader(String(F("Expires")), String(F("-1")));
        server.send(200, "text/html", Settings_temp);
        SoC->swSer_enableRx(true);
        free(Settings_temp);
}


void handleRoot() // Отображение основных настроек
{
    unsigned int timestamp = (unsigned int)ThisAircraft.timestamp;
  
    char str_local_lat[16];
    char str_local_lon[16];
    char str_ver[32];

     String ver = service.getVer();
     ver.toCharArray(str_ver, 32);

    size_t size = 3050; //2420;3920
    char* offset;
    size_t len = 0;

    char* Root_temp = (char*)malloc(size);
    if (Root_temp == NULL) {
        return;
    }
    offset = Root_temp;

    dtostrf(ThisAircraft.local_latitude, 8, 5, str_local_lat);
    dtostrf(ThisAircraft.local_longitude, 8, 5, str_local_lon);

    snprintf_P(offset, size,
        PSTR("<html>\
  <head>\
  <meta name='viewport' content='width=device-width, initial-scale=1'>\
  <title>FlytRF status</title>\
  </head>\
  <body>\
  <table width=100%%>\
  <td align=center><h1>FlyRF Lan статус</h1></td>\
  </table>\
  <table width=100%%>\
  <tr><th align=left>Идентификатор устройства</th><td align=right>%06X</td></tr>\
  <tr><th align=left>Блокировать адрес</th><td align=right>%06X</td></tr>\
  <tr><th align=left>Версия ПО</th><td align=right>%s</td></tr>\
  <tr><th align=left>Radio</th><td align=right>%s</td></tr>"
  "</table>\
  <table width=100%%>\
  <tr><th align=left>Пакетов</th>\
  <td align=right><table><tr>\
  <th align=left>Tx&nbsp;&nbsp;</th><td align=right>%u</td>\
  <th align=left>&nbsp;&nbsp;&nbsp;&nbsp;Rx&nbsp;&nbsp;</th><td align=right>%u</td>\
  </tr></table></td></tr>\
  </table>\
  <table width=100%%>\
  <tr><th align=left>Локальная latitude</th><td align=right>%s</td></tr>\
  <tr><th align=left>Локальная longitude</th><td align=right>%s</td></tr>\
  <tr><th align=left>Local IP(UDP)</th><td align=right>%d.%d.%d.%d</td></tr>\
  <tr><th align=left>Gateway(UDP)</th><td align=right>%d.%d.%d.%d</td></tr>\
  <tr><th align=left>Subnet mask(UDP)</th><td align=right>%d.%d.%d.%d</td></tr>\
  <tr><th align=left>DNS Server(UDP)</th><td align=right>%d.%d.%d.%d</td></tr>\
  </table>\
  <hr>\
  <table width=100%%>\
  <tr>\
  <td align=left><input type=button onClick=\"location.href='/settings'\" value='Настройки'></td>\
  <td align=center><input type=button onClick=\"location.href='/about'\" value='Информация'></td>"),
        ThisAircraft.addr, settings->block_addr, str_ver,
        rf_chip->name,
        tx_packets_counter, rx_packets_counter,
        str_local_lat, str_local_lon,
        settings->g_localIP[0], settings->g_localIP[1], settings->g_localIP[2], settings->g_localIP[3],
        settings->g_gatewayIP[0], settings->g_gatewayIP[1], settings->g_gatewayIP[2], settings->g_gatewayIP[3],
        settings->g_subnetMask[0], settings->g_subnetMask[1], settings->g_subnetMask[2], settings->g_subnetMask[3],
        settings->g_dns_server[0], settings->g_dns_server[1], settings->g_dns_server[2], settings->g_dns_server[3]);

    len = strlen(offset);
    offset += len;
    size -= len;


    /* SoC specific part 1 */

    snprintf_P(offset, size, PSTR("\
    <p td align=right><input type=button onClick=\"location.href='/firmware'\" value='Обновление программы'></td></p>"));
    len = strlen(offset);
    offset += len;
    size -= len;

    snprintf_P(offset, size, PSTR("\
    <td align=right><input type=button onClick=\"location.href='/hardware_settings'\" value='Аппаратные настройки'></td>"));
    len = strlen(offset);
    offset += len;
    size -= len;


    snprintf_P(offset, size, PSTR("\
  </tr>\
 </table>\
</body>\
</html>")
);

    SoC->swSer_enableRx(false);
    server.sendHeader(String(F("Cache-Control")), String(F("no-cache, no-store, must-revalidate")));
    server.sendHeader(String(F("Pragma")), String(F("no-cache")));
    server.sendHeader(String(F("Expires")), String(F("-1")));
    server.send(200, "text/html", Root_temp);
    SoC->swSer_enableRx(true);
    free(Root_temp);
}

void handleInput() // Ввод основных настроек
{
    size_t size = 3600;
    char* Input_temp = (char*)malloc(size);
    if (Input_temp == NULL) {
        return;
    }

    for (uint8_t i = 0; i < server.args(); i++)
    {
        if (server.argName(i).equals("tracker_send")) {
            settings->tracker_send = server.arg(i).toInt();
        }
        else if (server.argName(i).equals("bluetooth")) {
            settings->bluetooth = server.arg(i).toInt();
        }
        else if (server.argName(i).equals("nmea_out")) {
            settings->nmea_out = server.arg(i).toInt();
        }
        else if (server.argName(i).equals("input_N_S")) {
            settings->input_N_S = server.arg(i).toInt();
        }
        else if (server.argName(i).equals("local_latitude")) {
            settings->local_latitude = server.arg(i).toFloat();
        }
        else if (server.argName(i).equals("input_E_W")) {
            settings->input_E_W = server.arg(i).toInt();
        }
        else if (server.argName(i).equals("local_longitude")) {
            settings->local_longitude = server.arg(i).toFloat();
        }
        else if (server.argName(i).equals("local_ip")) {
            int ip[4];
            if (sscanf(server.arg(i).c_str(), "%d.%d.%d.%d", &ip[0], &ip[1], &ip[2], &ip[3]) == 4) {
                for (int k = 0; k < 4; k++) settings->g_localIP[k] = (uint8_t)ip[k];
            }
        }
        else if (server.argName(i).equals("gateway_ip")) {
            int ipg[4];
            if (sscanf(server.arg(i).c_str(), "%d.%d.%d.%d", &ipg[0], &ipg[1], &ipg[2], &ipg[3]) == 4)
            {
                for (int k = 0; k < 4; k++) settings->g_gatewayIP[k] = (uint8_t)ipg[k];
            }
        }
        else if (server.argName(i).equals("subnet_mask")) {
            int ipm[4];
            if (sscanf(server.arg(i).c_str(), "%d.%d.%d.%d", &ipm[0], &ipm[1], &ipm[2], &ipm[3]) == 4) {
                for (int k = 0; k < 4; k++) settings->g_subnetMask[k] = (uint8_t)ipm[k];
            }
        }
        else if (server.argName(i).equals("dns_server")) {
            int ipd[4];
            if (sscanf(server.arg(i).c_str(), "%d.%d.%d.%d", &ipd[0], &ipd[1], &ipd[2], &ipd[3]) == 4) {
                for (int k = 0; k < 4; k++) settings->g_dns_server[k] = (uint8_t)ipd[k];
            }
        }
    }

    const char* txt_tracker = (settings->tracker_send == TRACKER_SEND_OFF) ? "Off" :
        (settings->tracker_send == TRACKER_SEND_SINGLE) ? "Single" :
        (settings->tracker_send == TRACKER_SEND_AUTO) ? "Auto" : "Mini";

    const char* txt_bt = (settings->bluetooth == BLUETOOTH_NONE) ? "Off" :
        (settings->bluetooth == BLUETOOTH_SPP) ? "SPP" : "LE";

    const char* txt_nmea = (settings->nmea_out == NMEA_OFF) ? "Off" :
        (settings->nmea_out == NMEA_UART) ? "Serial" :
        (settings->nmea_out == NMEA_UDP) ? "UDP" :
        (settings->nmea_out == NMEA_TCP) ? "TCP" : "Bluetooth";

    const char* txt_ns = (settings->input_N_S == IMPUT_N) ? "N" : "S";
    const char* txt_ew = (settings->input_E_W == IMPUT_E) ? "E" : "W";


    snprintf_P(Input_temp, size,
        PSTR("<html>\
        <head>\
        <meta http-equiv='refresh' content='15; url=/'>\
        <meta name='viewport' content='width=device-width, initial-scale=1'>\
        <title>FLYRF Settings</title>\
        </head>\
        <body>\
        <h1 align=center>Новые настройки:</h1>\
        <table width=100%%>\
        <tr><th align=left>TRACKER send</th><td align=right>%s</td></tr>\
        <tr><th align=left>Bluetooth</th><td align=right>%s</td></tr>\
        <tr><th align=left>NMEA Out</th><td align=right>%s</td></tr>\
        <tr><th align=left>Полушарие</th><td align=right>%s</td></tr>\
        <tr><th align=left>Latitude</th><td align=right>%.05f</td></tr>\
        <tr><th align=left>Долгота</th><td align=right>%s</td></tr>\
        <tr><th align=left>Longitude</th><td align=right>%.05f</td></tr>\
        <tr><th align=left>Local IP(UDP)</th><td align=right>%d.%d.%d.%d</td></tr>\
        <tr><th align=left>Gateway IP(UDP)</th><td align=right>%d.%d.%d.%d</td></tr>\
        <tr><th align=left>Subnet Mask(UDP)</th><td align=right>%d.%d.%d.%d</td></tr>\
        <tr><th align=left>DNS Server(UDP)</th><td align=right>%d.%d.%d.%d</td></tr>\
        </table>\
        <hr>\
        <p align=center><h2 align=center>Выполняется перезагрузка... Пожалуйста, подождите!</h2></p>\
        </body>\
        </html>"),
        txt_tracker,   // %s
        txt_bt,        // %s
        txt_nmea,      // %s
        txt_ns,        // %s
        settings->local_latitude,
        txt_ew,        // %s
        settings->local_longitude,
        settings->g_localIP[0], settings->g_localIP[1], settings->g_localIP[2], settings->g_localIP[3],
        settings->g_gatewayIP[0], settings->g_gatewayIP[1], settings->g_gatewayIP[2], settings->g_gatewayIP[3],
        settings->g_subnetMask[0], settings->g_subnetMask[1], settings->g_subnetMask[2], settings->g_subnetMask[3],
        settings->g_dns_server[0], settings->g_dns_server[1], settings->g_dns_server[2], settings->g_dns_server[3]);

    SoC->swSer_enableRx(false);
    server.send(200, "text/html", Input_temp);
    delay(1000);
    free(Input_temp);
    EEPROM_store();
    RF_Shutdown();
    delay(1000);
    SoC->reset();
}


void hardwareInput() // Отображение аппаратных настроек
{
    size_t size = 4180;
    char* Input_temp = (char*)malloc(size);
    if (Input_temp == NULL) {
        return;
    }

    // 1. Сначала обрабатываем входящие аргументы от серверного запроса
    for (uint8_t i = 0; i < server.args(); i++)
    {
        if (server.argName(i).equals("rssi_view")) {
            settings->rssi_view = server.arg(i).toInt();
        }
        else if (server.argName(i).equals("display_set")) {
            settings->display_set = server.arg(i).toInt();
        }
        else if (server.argName(i).equals("serial_out")) {
            settings->serial_out = server.arg(i).toInt();
        }
        else if (server.argName(i).equals("view_test_coord")) {
            settings->view_test_coord = server.arg(i).toInt();
        }
        else if (server.argName(i).equals("out_of_sync")) {
            settings->out_of_sync = server.arg(i).toInt();
        }
        else if (server.argName(i).equals("default_settings")) {
            settings->default_settings = server.arg(i).toInt();
        }
        else if (server.argName(i).equals("block_addr")) {
            char buf[6 + 1];
            server.arg(i).toCharArray(buf, sizeof(buf));
            settings->block_addr = strtoul(buf, NULL, 16);
        }
        else if (server.argName(i).equals("threshold_level")) {
            settings->threshold_level = server.arg(i).toInt();
        }
        else if (server.argName(i).equals("local_ip")) {
            int ip[4];
            if (sscanf(server.arg(i).c_str(), "%d.%d.%d.%d", &ip[0], &ip[1], &ip[2], &ip[3]) == 4) {
                for (int k = 0; k < 4; k++) settings->g_localIP[k] = (uint8_t)ip[k];
            }
        }
        else if (server.argName(i).equals("gateway_ip")) {
            int ipg[4];
            if (sscanf(server.arg(i).c_str(), "%d.%d.%d.%d", &ipg[0], &ipg[1], &ipg[2], &ipg[3]) == 4) {
                for (int k = 0; k < 4; k++) settings->g_gatewayIP[k] = (uint8_t)ipg[k];
            }
        }
        else if (server.argName(i).equals("subnet_mask")) {
            int ipm[4];
            if (sscanf(server.arg(i).c_str(), "%d.%d.%d.%d", &ipm[0], &ipm[1], &ipm[2], &ipm[3]) == 4) {
                for (int k = 0; k < 4; k++) settings->g_subnetMask[k] = (uint8_t)ipm[k];
            }
        }
        else if (server.argName(i).equals("subnet_mask")) {
            int ipm[4];
            if (sscanf(server.arg(i).c_str(), "%d.%d.%d.%d", &ipm[0], &ipm[1], &ipm[2], &ipm[3]) == 4) {
                for (int k = 0; k < 4; k++) settings->g_subnetMask[k] = (uint8_t)ipm[k];
            }
        }
        else if (server.argName(i).equals("dns_server")) {
            int ipd[4];
            if (sscanf(server.arg(i).c_str(), "%d.%d.%d.%d", &ipd[0], &ipd[1], &ipd[2], &ipd[3]) == 4) {
                for (int k = 0; k < 4; k++) settings->g_dns_server[k] = (uint8_t)ipd[k];
            }
        }
    }

    // 2. Подготавливаем текстовые описания для отображения пользователю
    const char* txt_rssi = (settings->rssi_view == VIEW_RSSI_ON) ? "Включен" : "Выключен";

    const char* txt_display = (settings->display_set == INFO_DISTLAY_OFF) ? "Нет вывода" :
        (settings->display_set == INFO_DISPLAY_COORDINATE) ? "Только с координатами" : "Полная информация";

    const char* txt_serial = (settings->serial_out == SEND_SERIAL_OFF) ? "Не выводить" :
        (settings->serial_out == SEND_SERIAL_DISPLAY) ? "Данные в базе" : "Технические данные";

    const char* txt_test_coord = (settings->view_test_coord == VIEW_COORD_ON) ? "Включить" : "Выключить";

    const char* txt_def_set = (settings->default_settings == SETTINGS_ON) ? "Установить" : "Не устанавливать";

    // 3. Формируем HTML страницу подтверждения
    snprintf_P(Input_temp, size,
        PSTR("<html>\
        <head>\
        <meta http-equiv='refresh' content='15; url=/'>\
        <meta name='viewport' content='width=device-width, initial-scale=1'>\
        <title>hardware Settings</title>\
        </head>\
        <body>\
        <h1 align=center>Новые аппаратные настройки:</h1>\
        <table width=100%%>\
        <tr><th align=left>Отобразить уровень сигнала LoRa</th><td align=right>%s</td></tr>\
        <tr><th align=left>Режим работы дисплея</th><td align=right>%s</td></tr>\
        <tr><th align=left>Вывод данных в Serial</th><td align=right>%s</td></tr>\
        <tr><th align=left>Отображение локальных координат</th><td align=right>%s</td></tr>\
        <tr><th align=left>Установить настройки по умолчанию</th><td align=right>%s</td></tr>\
        <tr><th align=left>Block addr</th><td align=right>%06X</td></tr>\
        <tr><th align=left>Уровень порога усилителя 1090мГц (мв)</th><td align=right>%d</td></tr>\
        <tr><th align=left>Local IP(UDP)</th><td align=right>%d.%d.%d.%d</td></tr>\
        <tr><th align=left>Gateway IP(UDP)</th><td align=right>%d.%d.%d.%d</td></tr>\
        <tr><th align=left>Subnet mask(UDP)</th><td align=right>%d.%d.%d.%d</td></tr>\
        <tr><th align=left>DNS Server(UDP)</th><td align=right>%d.%d.%d.%d</td></tr>\
        </table>\
        <hr>\
        <p align=center><h2 align=center>Выполняется перезагрузка... Пожалуйста, подождите!</h2></p>\
        </body>\
        </html>"),
        txt_rssi,         // %s
        txt_display,      // %s
        txt_serial,       // %s
        txt_test_coord,   // %s
        txt_def_set,      // %s
        settings->block_addr,
        settings->threshold_level,
        settings->g_localIP[0], settings->g_localIP[1], settings->g_localIP[2], settings->g_localIP[3],
        settings->g_gatewayIP[0], settings->g_gatewayIP[1], settings->g_gatewayIP[2], settings->g_gatewayIP[3],
        settings->g_subnetMask[0], settings->g_subnetMask[1], settings->g_subnetMask[2], settings->g_subnetMask[3],
        settings->g_dns_server[0], settings->g_dns_server[1], settings->g_dns_server[2], settings->g_dns_server[3]);

    // 4. Отправка ответа и перезагрузка
    SoC->swSer_enableRx(false);
    server.send(200, "text/html", Input_temp);
    delay(1000);
    free(Input_temp);
    EEPROM_store();
    RF_Shutdown();
    delay(1000);
    SoC->reset();
}


void handleNotFound() {
#if defined(ENABLE_RECORDER)
  if (!handleFileRead(server.uri()))
#endif /* ENABLE_RECORDER */
  {
    String message = "File Not Found\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";

    for ( uint8_t i = 0; i < server.args(); i++ ) {
      message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
    }

    server.send ( 404, "text/plain", message );
  }
}

void Web_setup()
{
  server.on ( "/", handleRoot );
  server.on ( "/settings", handleSettings );
  server.on ("/hardware_settings", hardwareSettings);
  server.on ( "/about", []() {
    SoC->swSer_enableRx(false);
    server.sendHeader(String(F("Cache-Control")), String(F("no-cache, no-store, must-revalidate")));
    server.sendHeader(String(F("Pragma")), String(F("no-cache")));
    server.sendHeader(String(F("Expires")), String(F("-1")));
    server.send_P ( 200, PSTR("text/html"), about_html);
    SoC->swSer_enableRx(true);
  } );

  server.on ( "/input", handleInput );
  server.on ("/hardware_input", hardwareInput);
  server.on ( "/inline", []() {
    server.send ( 200, "text/plain", "this works as well" );
  } );
  server.on("/firmware", HTTP_GET, [](){
    SoC->swSer_enableRx(false);
    server.sendHeader(String(F("Connection")), String(F("close")));
    server.sendHeader(String(F("Access-Control-Allow-Origin")), String(F("*")));
    server.send_P(200,
      PSTR("text/html"),
      PSTR("\
<html>\
  <head>\
    <meta name='viewport' content='width=device-width, initial-scale=1'>\
    <title>Firmware update</title>\
  </head>\
<body>\
<body>\
 <h1 align=center>Обновление программы</h1>\
 <hr>\
 <table width=100%%>\
  <tr>\
    <td align=left>\
<script src='/jquery.min.js'></script>\
<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>\
    <input type='file' name='update'>\
    <input type='submit' value='Обновление'>\
</form>\
<div id='prg'>progress: 0%</div>\
<script>\
$('form').submit(function(e){\
    e.preventDefault();\
      var form = $('#upload_form')[0];\
      var data = new FormData(form);\
       $.ajax({\
            url: '/update',\
            type: 'POST',\
            data: data,\
            contentType: false,\
            processData:false,\
            xhr: function() {\
                var xhr = new window.XMLHttpRequest();\
                xhr.upload.addEventListener('progress', function(evt) {\
                    if (evt.lengthComputable) {\
                        var per = evt.loaded / evt.total;\
                        $('#prg').html('progress: ' + Math.round(per*100) + '%');\
                    }\
               }, false);\
               return xhr;\
            },\
            success:function(d, s) {\
                console.log('success!')\
           },\
            error: function (a, b, c) {\
            }\
          });\
});\
</script>\
    </td>\
  </tr>\
 </table>\
 <p align=center><h2 align=center>После обновления программы устройство перезагрузится !</h2></p>\
</body>\
</html>")
    );
  SoC->swSer_enableRx(true);
  });
  server.onNotFound ( handleNotFound );

  server.on("/update", HTTP_POST, [](){
    SoC->swSer_enableRx(false);
    server.sendHeader(String(F("Connection")), String(F("close")));
    server.sendHeader(String(F("Access-Control-Allow-Origin")), "*");
    server.send(200, String(F("text/plain")), (Update.hasError())?"FAIL":"OK");
//    SoC->swSer_enableRx(true);
    RF_Shutdown();
    delay(1000);
    SoC->reset();
  },[](){
    HTTPUpload& upload = server.upload();
    if(upload.status == UPLOAD_FILE_START){
      Serial_setDebugOutput(true);
      SoC->WiFiUDP_stopAll();
      SoC->WDT_fini();
      Serial.printf("Update: %s\r\n", upload.filename.c_str());
      uint32_t maxSketchSpace = SoC->maxSketchSpace();
      if(!Update.begin(maxSketchSpace)){//start with max available size
        Update.printError(Serial);
      }
    } else if(upload.status == UPLOAD_FILE_WRITE){
      if(Update.write(upload.buf, upload.currentSize) != upload.currentSize){
        Update.printError(Serial);
      }
    } else if(upload.status == UPLOAD_FILE_END){
      if(Update.end(true)){ //true to set the size to the current progress
        Serial.printf("Update Success: %u\r\nRebooting...\r\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
      Serial_setDebugOutput(false);
    }
    yield();
  });


  server.on ( "/jquery.min.js", []() {

    PGM_P content = jquery_min_js_gz;
    size_t bytes_left = jquery_min_js_gz_len;
    size_t chunk_size;

    server.setContentLength(bytes_left);
    server.sendHeader(String(F("Content-Encoding")),String(F("gzip")));
    server.send(200, String(F("application/javascript")), "");

    do {
      chunk_size = bytes_left > JS_MAX_CHUNK_SIZE ? JS_MAX_CHUNK_SIZE : bytes_left;
      server.sendContent_P(content, chunk_size);
      content += chunk_size;
      bytes_left -= chunk_size;
    } while (bytes_left > 0) ;

  } );

#if defined(ENABLE_RECORDER)
  server.on("/flights", HTTP_GET, Handle_Flight_Download);
#endif /* ENABLE_RECORDER */

  server.begin();
  Serial.println (F("HTTP server has started at port: 80"));

  delay(1000);
}

void Web_loop()
{
  server.handleClient();
}

void Web_fini()
{
  server.stop();
}

#endif /* EXCLUDE_WIFI */
