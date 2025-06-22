/*
 * WebHelper.cpp
 * Copyright (C) 2016-2023 Linar Yusupov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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


static uint32_t prev_rx_pkt_cnt = 0;


#include "jquery_min_js.h"

byte getVal(char c)
{
   if(c >= '0' && c <= '9')
     return (byte)(c - '0');
   else
     return (byte)(toupper(c)-'A'+10);
}

#if DEBUG
void Hex2Bin(String str, byte *buffer)
{
  char hexdata[2 * PKT_SIZE + 1];
  
  str.toCharArray(hexdata, sizeof(hexdata));
  for(int j = 0; j < PKT_SIZE * 2 ; j+=2)
  {
    buffer[j>>1] = getVal(hexdata[j+1]) + (getVal(hexdata[j]) << 4);
  }
}
#endif

static const char about_html[] PROGMEM = "<html>\
  <head>\
    <meta name='viewport' content='width=device-width, initial-scale=1'>\
    <title>About</title>\
  </head>\
<body>\
<h1 align=center>Информация</h1>\
<p>Эта программа часть проекта FlyRF.</p>\
<p>URL: https://t.me/flyrf_Support</p>\
<p>URL: https://www.decima.ru/contacts/</p>\
<p>E-mail: decima@decima.ru</p>\
<table width=100%%>\
</table>\
<hr>\
Copyright (C) 2023-2024 &nbsp;&nbsp;&nbsp; OOO Децима\
</body>\
</html>";

void handleSettings() 
{

  size_t size = 5100;
  char *offset;
  size_t len = 0;
  char *Settings_temp = (char *) malloc(size);

  if (Settings_temp == NULL) {
    return;
  }

  offset = Settings_temp;

  /* Common part 1 */
  snprintf_P ( offset, size,
    PSTR("<html>\
<head>\
<meta name='viewport' content='width=device-width, initial-scale=1'>\
<title>Settings</title>\
</head>\
<body>\
<h1 align=center>Настройки</h1>\
<form action='/input' method='GET'>\
<table width=100%%>\
<tr>\
<th align=left>Mode</th>\
<td align=right>\
<select name='mode'>\
<option %s value='%d'>Normal</option>\
<option %s value='%d'>Тест самолет зафиксирован</option>\
<option %s value='%d'>Тест самолет в полете</option>\
<option %s value='%d'>Комби. самолет на месте + в полете</option>\
<option %s value='%d'>Самолет на экваторе 0</option>\
<option %s value='%d'>Самолет на экваторе 180</option>\
</select>\
</td>\
</tr>"),
(settings->mode == FLYRF_MODE_NORMAL ? "selected" : ""), FLYRF_MODE_NORMAL,
(settings->mode == FLYRF_MODE_TXRX_TEST1 ? "selected" : ""), FLYRF_MODE_TXRX_TEST1,
(settings->mode == FLYRF_MODE_TXRX_TEST2 ? "selected" : ""), FLYRF_MODE_TXRX_TEST2,
(settings->mode == FLYRF_MODE_TXRX_TEST3 ? "selected" : ""), FLYRF_MODE_TXRX_TEST3,
(settings->mode == FLYRF_MODE_TXRX_TEST4 ? "selected" : ""), FLYRF_MODE_TXRX_TEST4,
(settings->mode == FLYRF_MODE_TXRX_TEST5 ? "selected" : ""), FLYRF_MODE_TXRX_TEST5
);

  len = strlen(offset);
  offset += len;
  size -= len;


  /* Common part 2 */
  snprintf_P ( offset, size,
    PSTR("\
<tr>\
<th align=left>Aircraft type</th>\
<td align=right>\
<select name='acft_type'>\
<option %s value='%d'>Glider</option>\
<option %s value='%d'>Towplane</option>\
<option %s value='%d'>Powered</option>\
<option %s value='%d'>Helicopter</option>\
<option %s value='%d'>UAV</option>\
<option %s value='%d'>Hangglider</option>\
<option %s value='%d'>Paraglider</option>\
<option %s value='%d'>Balloon</option>\
<option %s value='%d'>Static</option>\
</select>\
</td>\
</tr>\
<tr>\
<th align = left>GSM send</th>\
<td align = right>\
<select name = 'gsm_send'>\
<option % s value = '%d'>Off</option>\
<option % s value = '%d'>Single</option>\
<option % s value = '%d'>Auto</option>\
<option % s value = '%d'>Mini</option>\
</select>\
</td>\
</tr>"),
  (settings->aircraft_type == AIRCRAFT_TYPE_GLIDER ? "selected" : ""),  AIRCRAFT_TYPE_GLIDER,
  (settings->aircraft_type == AIRCRAFT_TYPE_TOWPLANE ? "selected" : ""),  AIRCRAFT_TYPE_TOWPLANE,
  (settings->aircraft_type == AIRCRAFT_TYPE_POWERED ? "selected" : ""),  AIRCRAFT_TYPE_POWERED,
  (settings->aircraft_type == AIRCRAFT_TYPE_HELICOPTER ? "selected" : ""),  AIRCRAFT_TYPE_HELICOPTER,
  (settings->aircraft_type == AIRCRAFT_TYPE_UAV ? "selected" : ""),  AIRCRAFT_TYPE_UAV,
  (settings->aircraft_type == AIRCRAFT_TYPE_HANGGLIDER ? "selected" : ""),  AIRCRAFT_TYPE_HANGGLIDER,
  (settings->aircraft_type == AIRCRAFT_TYPE_PARAGLIDER ? "selected" : ""),  AIRCRAFT_TYPE_PARAGLIDER,
  (settings->aircraft_type == AIRCRAFT_TYPE_BALLOON ? "selected" : ""),  AIRCRAFT_TYPE_BALLOON,
  (settings->aircraft_type == AIRCRAFT_TYPE_STATIC ? "selected" : ""),  AIRCRAFT_TYPE_STATIC,
  (settings->gsm_send == GSM_SEND_OFF ? "selected" : ""), GSM_SEND_OFF,
  (settings->gsm_send == GSM_SEND_SINGLE ? "selected" : ""), GSM_SEND_SINGLE,
  (settings->gsm_send == GSM_SEND_AUTO ? "selected" : ""), GSM_SEND_AUTO,
  (settings->gsm_send == GSM_SEND_MINI ? "selected" : ""), GSM_SEND_MINI
  );

  len = strlen(offset);
  offset += len;
  size -= len;

#if !defined(EXCLUDE_BLUETOOTH)
  /* SoC specific part 1 */
  if (SoC->id == SOC_ESP32S3) {
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

#endif /* EXCLUDE_BLUETOOTH */

  /* Common part 3 */
  snprintf_P ( offset, size,
    PSTR("\
<tr>\
<th align=left>NMEA sentences:</th>\
</tr>\
<tr>\
<th align=left>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;GNSS</th>\
<td align=right>\
<input type='radio' name='nmea_g' value='0' %s>Off\
<input type='radio' name='nmea_g' value='1' %s>On\
</td>\
</tr>\
<tr>\
<th align=left>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Private</th>\
<td align=right>\
<input type='radio' name='nmea_p' value='0' %s>Off\
<input type='radio' name='nmea_p' value='1' %s>On\
</td>\
</tr>\
<tr>\
<th align=left>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Legacy</th>\
<td align=right>\
<input type='radio' name='nmea_l' value='0' %s>Off\
<input type='radio' name='nmea_l' value='1' %s>On\
</td>\
</tr>\
<tr>\
<th align=left>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Sensors</th>\
<td align=right>\
<input type='radio' name='nmea_s' value='0' %s>Off\
<input type='radio' name='nmea_s' value='1' %s>On\
</td>\
</tr>\
<tr>\
<th align=left>NMEA output</th>\
<td align=right>\
<select name='nmea_out'>\
<option %s value='%d'>Off</option>\
<option %s value='%d'>Serial</option>\
<option %s value='%d'>UDP</option>"),
  (!settings->nmea_g ? "checked" : "") , (settings->nmea_g ? "checked" : ""),
  (!settings->nmea_p ? "checked" : "") , (settings->nmea_p ? "checked" : ""),
  (!settings->nmea_l ? "checked" : "") , (settings->nmea_l ? "checked" : ""),
  (!settings->nmea_s ? "checked" : "") , (settings->nmea_s ? "checked" : ""),
  (settings->nmea_out == NMEA_OFF  ? "selected" : ""), NMEA_OFF,
  (settings->nmea_out == NMEA_UART ? "selected" : ""), NMEA_UART,
  (settings->nmea_out == NMEA_UDP  ? "selected" : ""), NMEA_UDP);

  len = strlen(offset);
  offset += len;
  size -= len;

  /* Common part 4 */
  if (SoC->id == SOC_ESP32 || SoC->id == SOC_ESP32S3 || SoC->id == SOC_ESP32C3)
  {
      snprintf_P(offset, size,
          PSTR("\
<option %s value='%d'>TCP</option>\
<option %s value='%d'>Bluetooth</option>"),
(settings->nmea_out == NMEA_TCP ? "selected" : ""), NMEA_TCP,
(settings->nmea_out == NMEA_BLUETOOTH ? "selected" : ""), NMEA_BLUETOOTH);

      len = strlen(offset);
      offset += len;
      size -= len;
  }


  /* Common part 5 */
  snprintf_P ( offset, size,
    PSTR("\
</select>\
</td>\
</tr>\
<tr>\
<th align=left>Dump1090</th>\
<td align=right>\
<select name='d1090'>\
<option %s value='%d'>Off</option>\
<option %s value='%d'>Только с координатами</option>\
<option %s value='%d'>Полная запись в базу</option>"),
  (settings->d1090 == D1090_UART_OFF ? "selected" : ""), D1090_UART_OFF,
  (settings->d1090 == D1090_UART_MINI ? "selected" : ""), D1090_UART_MINI,
  (settings->d1090 == D1090_UART_FULL ? "selected" : ""), D1090_UART_FULL);

  len = strlen(offset);
  offset += len;
  size -= len;


  snprintf_P(offset, size,
      PSTR("\
<tr>\
<th align=left>Тревога внимание(2000-3000m)</th>\
<td align=right>\
<INPUT type='number' name='attention' min='2000' max='3000' value='%d'>\
</td>\
</tr>"),
settings->alarm_attention);

  len = strlen(offset);
  offset += len;
  size -= len;

  snprintf_P(offset, size,
      PSTR("\
<tr>\
<th align=left>Тревога предупреждение(200-2000m)</th>\
<td align=right>\
<INPUT type='number' name='warning' min='200' max='2000' value='%d'>\
</td>\
</tr>"),
settings->alarm_warning);

  len = strlen(offset);
  offset += len;
  size -= len;

  snprintf_P(offset, size,
      PSTR("\
<tr>\
<th align=left>Тревога опасность(10-500m)</th>\
<td align=right>\
<INPUT type='number' name='danger' min='10' max='500' value='%d'>\
</td>\
</tr>"),
settings->alarm_danger);
   
  len = strlen(offset);
  offset += len;
  size -= len;

  snprintf_P(offset, size,
      PSTR("\
<tr>\
<th align=left>Тревога высота(30-300m)</th>\
<td align=right>\
<INPUT type='number' name='height' min='30' max='300' value='%d'>\
</td>\
</tr>"),
settings->alarm_height);

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
void hardwareSettings()
{
    //hardware_settings
    size_t size = 5100;
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
<th align=left>Отобразить размер памяти RAM</th>\
<td align=right>\
<select name='ram_view'>\
<option % s value = '%d'>Выключен</option>\
<option % s value = '%d'>Включен</option>\
</select>\
</td>\
</tr>\
<tr>\
<th align=left>Отобразить заряд аккумулятора</th>\
<td align=right>\
<select name='akk_view'>\
<option % s value = '%d'>Выключен</option>\
<option % s value = '%d'>Включен</option>\
</select>\
</td>\
</tr>\
<tr>\
<th align=left>Отобразить кнопку SOS</th>\
<td align=right>\
<select name='sos_view'>\
<option % s value = '%d'>Выключен</option>\
<option % s value = '%d'>Включен</option>\
</select>\
</td>\
</tr>\
<tr>\
<th align=left>Блокировка адреса стороннего устройства</th>\
<td align=right>\
<INPUT type='text' name='block_addr' maxlength='6' size='6' value='%06X'>\
</td>\
</tr>"),
(settings->rssi_view == VIEW_RSSI_OFF ? "selected" : ""), VIEW_RSSI_OFF,
(settings->rssi_view == VIEW_RSSI_ON ? "selected" : ""), VIEW_RSSI_ON,
(settings->ram_view == VIEW_RAM_OFF ? "selected" : ""), VIEW_RAM_OFF,
(settings->ram_view == VIEW_RAM_ON ? "selected" : ""), VIEW_RAM_ON,
(settings->akk_view == VIEW_AKK_OFF ? "selected" : ""), VIEW_AKK_OFF,
(settings->akk_view == VIEW_AKK_ON ? "selected" : ""), VIEW_AKK_ON, 
(settings->sos_view == VIEW_SOS_OFF ? "selected" : ""), VIEW_SOS_OFF,
(settings->sos_view == VIEW_SOS_ON ? "selected" : ""), VIEW_SOS_ON,
 settings->block_addr
);

    len = strlen(offset);
    offset += len;
    size -= len;

    snprintf_P(offset, size,
        PSTR("\
<tr>\
<th align=left>Radio CF correction(&#177;30,kHz) </th>\
<td align=right>\
<INPUT type='number' name='rfc' min='-30' max='30' value='%d'>\
</td>\
</tr>\
<tr>\
<th align=left>Вариант ввода координат </th>\
<td align=right>\
<select name='input_coordinates'>\
<option % s value='%d'>Автоматический</option>\
<option % s value='%d'>Ручной</option>\
</select>\
</td>\
</tr>\
<tr>\
<th align=left>Отображение тестовых координат</th>\
<td align=right>\
<select name='view_test_coord'>\
<option % s value='%d'>Включено</option>\
<option % s value='%d'>Выключено</option>\
</select>\
</td>\
</tr>\
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
<INPUT type='tel' name='test_latitude'pattern ='[0-9]{2}.[0-9]{5}' size = '8' maxlength = '8' min='0' max='90' value='%.05f'>\
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
<th align=left>Величина значения долготы</th>\
<td align=right>\
<select name='coord_max100'>\
<option % s value='%d'>Меньше 100</option>\
<option % s value='%d'>Больше 99</option>\
</select>\
</td>\
</tr>\
"),
settings->freq_corr,
(settings->input_coordinates == IMPUT_COORD_AUTO ? "selected" : ""), IMPUT_COORD_AUTO,
(settings->input_coordinates == IMPUT_COORD_MANUAL ? "selected" : ""), IMPUT_COORD_MANUAL,
(settings->view_test_coord == VIEW_TEST_COORD_ON ? "selected" : ""), VIEW_TEST_COORD_ON,
(settings->view_test_coord == VIEW_TEST_COORD_OFF ? "selected" : ""), VIEW_TEST_COORD_OFF,
(settings->input_N_S == IMPUT_N ? "selected" : ""), IMPUT_N,
(settings->input_N_S == IMPUT_S ? "selected" : ""), IMPUT_S,
 settings->test_latitude, 
(settings->input_E_W == IMPUT_E ? "selected" : ""), IMPUT_E,
(settings->input_E_W == IMPUT_W ? "selected" : ""), IMPUT_W,
(settings->coord_max100 == COORD_MIN ? "selected" : ""), COORD_MIN,
(settings->coord_max100 == COORD_MAX ? "selected" : ""), COORD_MAX
);

len = strlen(offset);
offset += len;
size -= len;

    if (settings->coord_max100 == COORD_MIN)
    {
    snprintf_P ( offset, size,
        PSTR("\
    <tr>\
    <th align=left>longitude(долгота 0&#176 - 180&#176)Внимание! Пример 037.12345</th>\
    <td align=right>\
    <INPUT type='tel' name='test_longitude' pattern ='[0-9]{3}.[0-9]{5}' size = '9' maxlength = '9' min='0' max='180' value='0%.05f'>\
    </td>\
    </tr>\
    "),
    settings->test_longitude
    );
  
    len = strlen(offset);
    offset += len;
    size -= len;

    }
    else if (settings->coord_max100 == COORD_MAX) 
    {
    snprintf_P ( offset, size,
        PSTR("\
    <tr>\
    <th align=left>longitude(долгота 0&#176 - 180&#176)Внимание! Пример 037.12345</th>\
    <td align=right>\
    <INPUT type='tel' name='test_longitude' pattern ='[0-9]{3}.[0-9]{5}' size = '9' maxlength = '9' min='0' max='180' value='%.05f'>\
    </td>\
    </tr>\
    "),
    settings->test_longitude
    );

    len = strlen(offset);
    offset += len;
    size -= len;
    }

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



void handleRoot() {

    int vdd = (int)battery_read();
    bool low_voltage = false;

    if (vdd < 15)
    {
        low_voltage = true;
    }
    else
    {
        low_voltage = false;
    }


    unsigned int timestamp = (unsigned int)ThisAircraft.timestamp;
    unsigned int sats = gnss.satellites.value(); // Number of satellites in use (u32)

    char str_lat[16];
    char str_lon[16];
    char str_test_lat[16];
    char str_test_lon[16];
    char str_alt[16];
    char str_Vcc[4];
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

    dtostrf(ThisAircraft.latitude, 8, 6, str_lat);
    dtostrf(ThisAircraft.longitude, 8, 6, str_lon);
    dtostrf(ThisAircraft.test_latitude, 8, 5, str_test_lat);
    dtostrf(ThisAircraft.test_longitude, 8, 5, str_test_lon);
    dtostrf(ThisAircraft.altitude, 7, 1, str_alt);
    dtostrf(vdd, 3, 0, str_Vcc);

    snprintf_P(offset, size,
        PSTR("<html>\
  <head>\
  <meta name='viewport' content='width=device-width, initial-scale=1'>\
  <title>FlytRF status</title>\
  </head>\
  <body>\
  <table width=100%%>\
  <td align=center><h1>FlyRF статус</h1></td>\
  </table>\
  <table width=100%%>\
  <tr><th align=left>Идентификатор устройства</th><td align=right>%06X</td></tr>\
  <tr><th align=left>Блокировать адрес</th><td align=right>%06X</td></tr>\
  <tr><th align=left>Версия ПО</th><td align=right>%s</td></tr>"
  "</table><table width=100%%>\
  <tr><td align=left><table><tr><th align=left>GNSS&nbsp;&nbsp;</th><td align=right>%s</td></tr></table></td>\
  <td align=center><table><tr><th align=left>Radio&nbsp;&nbsp;</th><td align=right>%s</td></tr></table></td>\
  <td align=right><table><tr><th align=left>Baro&nbsp;&nbsp;</th><td align=right>%s</td></tr></table></td></tr>\
  </table><table width=100%%>"
  "<tr><th align=left>Время работы</th><td align=right>%02d:%02d:%02d</td></tr>\
  <tr><th align=left>Свободная память</th><td align=right>%u</td></tr>\
  <tr><th align=left>Напряжение батареи(проценты)</th><td align=right><font color=%s>%s&#37</font></td></tr>"
  "</table>\
  <table width=100%%>\
  <tr><th align=left>Пакетов</th>\
  <td align=right><table><tr>\
  <th align=left>Tx&nbsp;&nbsp;</th><td align=right>%u</td>\
  <th align=left>&nbsp;&nbsp;&nbsp;&nbsp;Rx&nbsp;&nbsp;</th><td align=right>%u</td>\
  </tr></table></td></tr>\
  </table>\
  <h2 align=center>Последние данные GNSS</h2>\
  <table width=100%%>\
  <tr><th align=left>Время</th><td align=right>%02d:%02d:%02d</td></tr>\
  <tr><th align=left>Спутников</th><td align=right>%d</td></tr>\
  <tr><th align=left>Latitude</th><td align=right>%s</td></tr>\
  <tr><th align=left>Longitude</th><td align=right>%s</td></tr>\
  <tr><td align=left><b>Высота над уровнем моря</b></td><td align=right>%s</td></tr>\
  <tr><th align=left>Ввод тестовых координат</th><td align=right>%s</td></tr>\
  <tr><th align=left>Тестовая latitude</th><td align=right>%s</td></tr>\
  <tr><th align=left>Тестовая longitude</th><td align=right>%s</td></tr>\
  </table>\
  <hr>\
  <table width=100%%>\
  <tr>\
  <td align=left><input type=button onClick=\"location.href='/settings'\" value='Настройки'></td>\
  <td align=center><input type=button onClick=\"location.href='/about'\" value='Информация'></td>"),
        ThisAircraft.addr, settings->block_addr, str_ver,
        GNSS_name[hw_info.gnss],
        (rf_chip == NULL ? "NONE" : rf_chip->name),
        (baro_chip == NULL ? "NONE" : baro_chip->name),
        UpTime.hours, UpTime.minutes, UpTime.seconds, SoC->getFreeHeap(),
        low_voltage ? "red" : "green", str_Vcc,
        tx_packets_counter, rx_packets_counter,
        gnss.time.hour(), gnss.time.minute(), gnss.time.second(), sats,
        str_lat, str_lon, str_alt,
        (settings->input_coordinates == NULL ? "Автоматический" : "Ручной"),
        str_test_lat, str_test_lon
    );

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


void handleInput() {

  size_t size = 2120; //1700;3100

  char *Input_temp = (char *) malloc(size);
  if (Input_temp == NULL) {
    return;
  }

  for ( uint8_t i = 0; i < server.args(); i++ ) 
  {
    if (server.argName(i).equals("mode")) {
      settings->mode = server.arg(i).toInt();
    } else if (server.argName(i).equals("acft_type")) {
      settings->aircraft_type = server.arg(i).toInt();
    } else if (server.argName(i).equals("gsm_send")) {
      settings->gsm_send = server.arg(i).toInt();
    } else if (server.argName(i).equals("bluetooth")) {
      settings->bluetooth = server.arg(i).toInt();
    } else if (server.argName(i).equals("nmea_g")) {
      settings->nmea_g = server.arg(i).toInt();
    } else if (server.argName(i).equals("nmea_p")) {
      settings->nmea_p = server.arg(i).toInt();
    } else if (server.argName(i).equals("nmea_l")) {
      settings->nmea_l = server.arg(i).toInt();
    } else if (server.argName(i).equals("nmea_s")) {
      settings->nmea_s = server.arg(i).toInt();
    } else if (server.argName(i).equals("nmea_out")) {
      settings->nmea_out = server.arg(i).toInt();
    } else if (server.argName(i).equals("d1090")) {
      settings->d1090 = server.arg(i).toInt();
    } else if (server.argName(i).equals("attention")) {
      settings->alarm_attention = server.arg(i).toInt();
    } else if (server.argName(i).equals("warning")) {
      settings->alarm_warning = server.arg(i).toInt();
    } else if (server.argName(i).equals("danger")) {
      settings->alarm_danger = server.arg(i).toInt();
    } else if (server.argName(i).equals("height")) {
      settings->alarm_height = server.arg(i).toInt();
    }
  }
  snprintf_P ( Input_temp, size,
PSTR("<html>\
<head>\
<meta http-equiv='refresh' content='15; url=/'>\
<meta name='viewport' content='width=device-width, initial-scale=1'>\
<title>FLYRF Settings</title>\
</head>\
<body>\
<h1 align=center>Новые настройки:</h1>\
<table width=100%%>\
<tr><th align=left>Mode</th><td align=right>%d</td></tr>\
<tr><th align=left>Aircraft type</th><td align=right>%d</td></tr>\
<tr><th align=left>GSM send</th><td align=right>%d</td></tr>\
<tr><th align=left>Bluetooth</th><td align=right>%d</td></tr>\
<tr><th align=left>NMEA GNSS</th><td align=right>%s</td></tr>\
<tr><th align=left>NMEA Private</th><td align=right>%s</td></tr>\
<tr><th align=left>NMEA Legacy</th><td align=right>%s</td></tr>\
<tr><th align=left>NMEA Sensors</th><td align=right>%s</td></tr>\
<tr><th align=left>NMEA Out</th><td align=right>%d</td></tr>\
<tr><th align=left>DUMP1090</th><td align=right>%d</td></tr>\
<tr><th align=left>Тревога внимание</th><td align=right>%d</td></tr>\
<tr><th align=left>Тревога предупреждение</th><td align=right>%d</td></tr>\
<tr><th align=left>Тревога опасность</th><td align=right>%d</td></tr>\
<tr><th align=left>Тревога высота</th><td align=right>%d</td></tr>\
</table>\
<hr>\
 <p align=center><h2 align=center>Выполняется перезагрузка... Пожалуйста, подождите!</h2></p>\
</body>\
</html>"),
  settings->mode,
  settings->aircraft_type, settings->gsm_send, settings->bluetooth,
  BOOL_STR(settings->nmea_g), BOOL_STR(settings->nmea_p),
  BOOL_STR(settings->nmea_l), BOOL_STR(settings->nmea_s),
  settings->nmea_out, settings->d1090,
  settings->alarm_attention, settings->alarm_warning,
  settings->alarm_danger, settings->alarm_height
  );
  SoC->swSer_enableRx(false);
  server.send ( 200, "text/html", Input_temp );
  delay(1000);
  free(Input_temp);
  EEPROM_store();
  RF_Shutdown();
  delay(1000);
  SoC->reset();
}

//==
void hardwareInput() 
{
    size_t size = 3520; //1700;3100

    char* Input_temp = (char*)malloc(size);
    if (Input_temp == NULL) {
        return;
    }

    for (uint8_t i = 0; i < server.args(); i++) 
    {
        if (server.argName(i).equals("rfc")) {
            settings->freq_corr = server.arg(i).toInt();
        }
        else if (server.argName(i).equals("rssi_view")) {
            settings->rssi_view = server.arg(i).toInt();
        }
        else if (server.argName(i).equals("ram_view")) {
            settings->ram_view = server.arg(i).toInt();
        }
        else if (server.argName(i).equals("akk_view")) {
            settings->akk_view = server.arg(i).toInt();
        }
        else if (server.argName(i).equals("sos_view")) {
            settings->sos_view = server.arg(i).toInt();
        }
        else if (server.argName(i).equals("block_addr")) {
            char buf[6 + 1];
            server.arg(i).toCharArray(buf, sizeof(buf));
            settings->block_addr = strtoul(buf, NULL, 16);
        }
        else if (server.argName(i).equals("input_coordinates")) {
            settings->input_coordinates = server.arg(i).toInt();
        }
        else if (server.argName(i).equals("view_test_coord")) {
            settings->view_test_coord = server.arg(i).toInt();
        }
        else if (server.argName(i).equals("input_N_S")) {
            settings->input_N_S = server.arg(i).toInt();
        }
        else if (server.argName(i).equals("test_latitude")) {
            settings->test_latitude = server.arg(i).toFloat();
        }
        else if (server.argName(i).equals("input_E_W")) {
            settings->input_E_W = server.arg(i).toInt();
        }
        else if (server.argName(i).equals("coord_max100")) {
            settings->coord_max100 = server.arg(i).toInt();
        }
        else if (server.argName(i).equals("test_longitude")) {
            settings->test_longitude = server.arg(i).toFloat();
        }
    }

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
<tr><th align=left>Freq. correction</th><td align=right>%d</td></tr>\
<tr><th align=left>Отобразить уровень сигнала LoRa</th><td align=right>%d</td></tr>\
<tr><th align=left>Отобразить размер памяти RAM</th><td align=right>%d</td></tr>\
<tr><th align=left>Отобразить заряд аккумулятора</th><td align=right>%d</td></tr>\
<tr><th align=left>Отобразить кнопку SOS</th><td align=right>%d</td></tr>\
<tr><th align=left>Block addr</th><td align=right>%06X</td></tr>\
<tr><th align=left>Вариант ввода координат</th><td align=right>%d</td></tr>\
<tr><th align=left>Отображение тестовых координат</th><td align=right>%d</td></tr>\
<tr><th align=left>Полушарие (северное или южное)</th><td align=right>%d</td></tr>\
<tr><th align=left>latitude</th><td align=right>%.05f</td></tr>\
<tr><th align=left>Долгота(восточная или западная)</th><td align=right>%d</td></tr>\
<tr><th align=left>Величина значения долготы</th><td align=right>%d</td></tr>\
<tr><th align=left>longitude</th><td align=right>%.05f</td></tr>\
</table>\
<hr>\
 <p align=center><h2 align=center>Выполняется перезагрузка... Пожалуйста, подождите!</h2></p>\
</body>\
</html>"),
settings->freq_corr,
settings->rssi_view, settings->ram_view,
settings->akk_view, settings->sos_view,
settings->block_addr,
settings->input_coordinates,
settings->view_test_coord,
settings->input_N_S,
settings->test_latitude,
settings->input_E_W,
settings->coord_max100,
settings->test_longitude
);
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
