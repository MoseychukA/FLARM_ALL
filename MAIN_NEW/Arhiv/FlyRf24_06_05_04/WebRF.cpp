

#include "SoC.h"


#if defined(EXCLUDE_WIFI)
void Web_setup()    {}
void Web_loop()     {}
void Web_fini()     {}
#else

#include <Arduino.h>

#include "BatteryRF.h"
#include "RF.h"
#include "WebRF.h"
#include "Baro.h"
#include "TrafficHelper.h"
#include "NMEA.h"
#include "GDL90.h"
#include "D1090.h"
#include "TimeRF.h"
#include "TFTMenu.h"

static uint32_t prev_rx_pkt_cnt = 0;

String ver_soft1 = "";

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
<p>Author: <b>Александр Мосейчук</b></p>\
<p>E-mail: Aleksandr.Moseychuk@decima.ru</p>\
<p>E-mail: decima@decima.ru</p>\
<table width=100%%>\
</table>\
<hr>\
Copyright (C) 2023-2024 &nbsp;&nbsp;&nbsp; OOO Децима\
</body>\
</html>";

void handleSettings() {

  size_t size = 6990;
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
<th align=left>Режим</th>\
<td align=right>\
<select name='mode'>\
<option %s value='%d'>Normal</option>\
<option %s value='%d'>Тест самолет зафиксирован</option>\
<option %s value='%d'>Тест самолет в полете</option>\
<option %s value='%d'>Комби. самолет на месте + в полете</option>\
<option %s value='%d'>Bridge</option>\
<option %s value='%d'>UAV</option>\
</select>\
</td>\
</tr>"),
  (settings->mode == SOFTRF_MODE_NORMAL ? "selected" : "") , SOFTRF_MODE_NORMAL,
  (settings->mode == SOFTRF_MODE_TXRX_TEST1 ? "selected" : ""), SOFTRF_MODE_TXRX_TEST1,
  (settings->mode == SOFTRF_MODE_TXRX_TEST2 ? "selected" : ""), SOFTRF_MODE_TXRX_TEST2,
  (settings->mode == SOFTRF_MODE_TXRX_TEST3 ? "selected" : ""), SOFTRF_MODE_TXRX_TEST3,
  (settings->mode == SOFTRF_MODE_BRIDGE ? "selected" : ""), SOFTRF_MODE_BRIDGE,
  (settings->mode == SOFTRF_MODE_UAV ? "selected" : ""), SOFTRF_MODE_UAV
  );

  len = strlen(offset);
  offset += len;
  size -= len;

  /* Radio specific part 1 */
  if (hw_info.rf == RF_IC_SX1276 || hw_info.rf == RF_IC_SX1262) {
    snprintf_P ( offset, size,
      PSTR("\
<tr>\
<th align=left>Protocol</th>\
<td align=right>\
<select name='protocol'>\
<option %s value='%d'>%s</option>\
<option %s value='%d'>%s</option>\
<option %s value='%d'>%s</option>\
<option %s value='%d'>%s</option>\
</select>\
</td>\
</tr>"),
    (settings->rf_protocol == RF_PROTOCOL_LEGACY ? "selected" : ""),
     RF_PROTOCOL_LEGACY, legacy_proto_desc.name,
    (settings->rf_protocol == RF_PROTOCOL_OGNTP  ? "selected" : ""),
     RF_PROTOCOL_OGNTP, ogntp_proto_desc.name,
    (settings->rf_protocol == RF_PROTOCOL_P3I    ? "selected" : ""),
     RF_PROTOCOL_P3I, p3i_proto_desc.name,
    (settings->rf_protocol == RF_PROTOCOL_FANET  ? "selected" : ""),
     RF_PROTOCOL_FANET, fanet_proto_desc.name
    );
  } else {
    snprintf_P ( offset, size,
      PSTR("\
<tr>\
<th align=left>Protocol</th>\
<td align=right>%s\
</td>\
</tr>"),
    (settings->rf_protocol == RF_PROTOCOL_LEGACY   ? legacy_proto_desc.name :
    (settings->rf_protocol == RF_PROTOCOL_FANET    ? fanet_proto_desc.name  :
    "UNK"))
    );
  }
  len = strlen(offset);
  offset += len;
  size -= len;

  /* Common part 2 */
  snprintf_P ( offset, size,
    PSTR("\
<tr>\
<th align=left>Region</th>\
<td align=right>\
<select name='band'>\
<option %s value='%d'>RU (868.8 MHz)</option>\
</select>\
</td>\
</tr>\
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
<th align=left>Tx Power</th>\
<td align=right>\
<select name='txpower'>\
<option %s value='%d'>Full</option>\
<option %s value='%d'>Low</option>\
<option %s value='%d'>Off</option>\
</select>\
</td>\
</tr>"),
  (settings->band == RF_BAND_RU ? "selected" : ""), RF_BAND_RU,
  (settings->aircraft_type == AIRCRAFT_TYPE_GLIDER ? "selected" : ""),  AIRCRAFT_TYPE_GLIDER,
  (settings->aircraft_type == AIRCRAFT_TYPE_TOWPLANE ? "selected" : ""),  AIRCRAFT_TYPE_TOWPLANE,
  (settings->aircraft_type == AIRCRAFT_TYPE_POWERED ? "selected" : ""),  AIRCRAFT_TYPE_POWERED,
  (settings->aircraft_type == AIRCRAFT_TYPE_HELICOPTER ? "selected" : ""),  AIRCRAFT_TYPE_HELICOPTER,
  (settings->aircraft_type == AIRCRAFT_TYPE_UAV ? "selected" : ""),  AIRCRAFT_TYPE_UAV,
  (settings->aircraft_type == AIRCRAFT_TYPE_HANGGLIDER ? "selected" : ""),  AIRCRAFT_TYPE_HANGGLIDER,
  (settings->aircraft_type == AIRCRAFT_TYPE_PARAGLIDER ? "selected" : ""),  AIRCRAFT_TYPE_PARAGLIDER,
  (settings->aircraft_type == AIRCRAFT_TYPE_BALLOON ? "selected" : ""),  AIRCRAFT_TYPE_BALLOON,
  (settings->aircraft_type == AIRCRAFT_TYPE_STATIC ? "selected" : ""),  AIRCRAFT_TYPE_STATIC,
  (settings->txpower == RF_TX_POWER_FULL ? "selected" : ""),  RF_TX_POWER_FULL,
  (settings->txpower == RF_TX_POWER_LOW ? "selected" : ""),  RF_TX_POWER_LOW,
  (settings->txpower == RF_TX_POWER_OFF ? "selected" : ""),  RF_TX_POWER_OFF
  );

  len = strlen(offset);
  offset += len;
  size -= len;


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

  /* SoC specific part 2 */
  if (SoC->id == SOC_ESP32   || SoC->id == SOC_ESP32S3 ||
      SoC->id == SOC_ESP32C3) {
    snprintf_P ( offset, size,
      PSTR("\
<option %s value='%d'>TCP</option>"),
      (settings->nmea_out == NMEA_TCP       ? "selected" : ""), NMEA_TCP);

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
<option %s value='%d'>Serial</option>"),
  (settings->d1090 == D1090_OFF  ? "selected" : ""), D1090_OFF,
  (settings->d1090 == D1090_UART ? "selected" : ""), D1090_UART);

  len = strlen(offset);
  offset += len;
  size -= len;

  /* Common part 6 */
  snprintf_P ( offset, size,
    PSTR("\
</select>\
</td>\
</tr>\
<tr>\
<th align=left>Power save</th>\
<td align=right>\
<select name='power_save'>\
<option %s value='%d'>Disabled</option>\
<option %s value='%d'>WiFi OFF (10 min.)</option>\
<option %s value='%d'>GNSS</option>"),
  (settings->power_save == POWER_SAVE_NONE ? "selected" : ""), POWER_SAVE_NONE,
  (settings->power_save == POWER_SAVE_WIFI ? "selected" : ""), POWER_SAVE_WIFI,
  (settings->power_save == POWER_SAVE_GNSS ? "selected" : ""), POWER_SAVE_GNSS
  );

  len = strlen(offset);
  offset += len;
  size -= len;

  /* Common part 7 */
  snprintf_P ( offset, size,
    PSTR("\
</select>\
</td>\
</tr>\
<tr>\
<th align=left>Stealth</th>\
<td align=right>\
<input type='radio' name='stealth' value='0' %s>Off\
<input type='radio' name='stealth' value='1' %s>On\
</td>\
</tr>\
<tr>\
<th align=left>No track</th>\
<td align=right>\
<input type='radio' name='no_track' value='0' %s>Off\
<input type='radio' name='no_track' value='1' %s>On\
</td>\
</tr>"),
  (!settings->stealth  ? "checked" : "") , (settings->stealth  ? "checked" : ""),
  (!settings->no_track ? "checked" : "") , (settings->no_track ? "checked" : "")
  );

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

  /* Radio specific part 3 */
  if (rf_chip && rf_chip->type == RF_IC_SX1276) {
    snprintf_P ( offset, size,
      PSTR("\
<tr>\
<th align=left>Radio CF correction (&#177;, kHz)</th>\
<td align=right>\
<INPUT type='number' name='rfc' min='-30' max='30' value='%d'>\
</td>\
</tr>"),
    settings->freq_corr);

    len = strlen(offset);
    offset += len;
    size -= len;
  }

  snprintf_P(offset, size,
      PSTR("\
<tr>\
<th align=left>Block addr</th>\
<td align=right>\
<INPUT type='text' name='block_addr' maxlength='6' size='6' value='%06X'>\
</td>\
</tr>"),
settings->block_addr
);

  len = strlen(offset);
  offset += len;
  size -= len;

  /* Common part 8 */
  snprintf_P ( offset, size,
    PSTR("\
</table>\
<p align=center>\
<INPUT type='submit' value='Сохранить и обновить'></p>\
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

void handleRoot() {

  float vdd = Battery_voltage() ;
  bool low_voltage = (Battery_voltage() <= Battery_threshold());

  unsigned int timestamp = (unsigned int) ThisAircraft.timestamp;
  unsigned int sats = gnss.satellites.value(); // Number of satellites in use (u32)

  char str_lat[16];
  char str_lon[16];
  char str_alt[16];
  char str_Vcc[8];
  char str_ver[32];

  String ver = MainScreen->getVer();
  ver.toCharArray(str_ver, 32);

  size_t size = 3920;
  char *offset;
  size_t len = 0;

  char *Root_temp = (char *) malloc(size);
  if (Root_temp == NULL) {
    return;
  }
  offset = Root_temp;

  dtostrf(ThisAircraft.latitude,  8, 4, str_lat);
  dtostrf(ThisAircraft.longitude, 8, 4, str_lon);
  dtostrf(ThisAircraft.altitude,  7, 1, str_alt);
  dtostrf(vdd, 4, 2, str_Vcc);

  snprintf_P ( offset, size,
    PSTR("<html>\
  <head>\
    <meta name='viewport' content='width=device-width, initial-scale=1'>\
    <title>FlytRF status</title>\
  </head>\
<body>\
 <table width=100%%>\
  <tr><!-- <td align=left><h1>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</h1></td> -->\
  <td align=center><h1>FlyRF статус</h1></td>\
  </table>\
 <table width=100%%>\
  <tr><th align=left>Идентификатор устройства</th><td align=right>%06X</td></tr>\
  <tr><th align=left>Блокировать адрес</th><td align=right>%06X</td></tr>\
  <tr><th align=left>Версия ПО</th><td align=right>%s</td></tr>"
#if !defined(ENABLE_AHRS)
 "</table><table width=100%%>\
  <tr><td align=left><table><tr><th align=left>GNSS&nbsp;&nbsp;</th><td align=right>%s</td></tr></table></td>\
  <td align=center><table><tr><th align=left>Radio&nbsp;&nbsp;</th><td align=right>%s</td></tr></table></td>\
  <td align=right><table><tr><th align=left>Baro&nbsp;&nbsp;</th><td align=right>%s</td></tr></table></td></tr>\
  </table><table width=100%%>"
#endif /* ENABLE_AHRS */
 "<tr><th align=left>Время работы</th><td align=right>%02d:%02d:%02d</td></tr>\
  <tr><th align=left>Свободная память</th><td align=right>%u</td></tr>\
  <tr><th align=left>Напряжение батареи</th><td align=right><font color=%s>%s</font></td></tr>"
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
  <tr><th align=left>Время</th><td align=right>%u</td></tr>\
  <tr><th align=left>Спутников</th><td align=right>%d</td></tr>\
  <tr><th align=left>Latitude</th><td align=right>%s</td></tr>\
  <tr><th align=left>Longitude</th><td align=right>%s</td></tr>\
  <tr><td align=left><b>Высота над уровнем моря</b></td><td align=right>%s</td></tr>\
 </table>\
 <hr>\
 <table width=100%%>\
  <tr>\
    <td align=left><input type=button onClick=\"location.href='/settings'\" value='Настройки'></td>\
    <td align=center><input type=button onClick=\"location.href='/about'\" value='Информация'></td>"), 
    ThisAircraft.addr, settings->block_addr, str_ver,
    GNSS_name[hw_info.gnss],
    (rf_chip   == NULL ? "NONE" : rf_chip->name),
    (baro_chip == NULL ? "NONE" : baro_chip->name),
    UpTime.hours, UpTime.minutes, UpTime.seconds, SoC->getFreeHeap(),
    low_voltage ? "red" : "green", str_Vcc,
    tx_packets_counter, rx_packets_counter,
    timestamp, sats, str_lat, str_lon, str_alt
  );

  len = strlen(offset);
  offset += len;
  size -= len;


  /* SoC specific part 1 */
    snprintf_P ( offset, size, PSTR("\
    <td align=right><input type=button onClick=\"location.href='/firmware'\" value='Обновление программы'></td>"));
    len = strlen(offset);
    offset += len;
    size -= len;

  snprintf_P ( offset, size, PSTR("\
  </tr>\
 </table>\
</body>\
</html>")
  );

  SoC->swSer_enableRx(false);
  server.sendHeader(String(F("Cache-Control")), String(F("no-cache, no-store, must-revalidate")));
  server.sendHeader(String(F("Pragma")), String(F("no-cache")));
  server.sendHeader(String(F("Expires")), String(F("-1")));
  server.send ( 200, "text/html", Root_temp );
  SoC->swSer_enableRx(true);
  free(Root_temp);
}

void handleInput() {

  size_t size = 3100;

  char *Input_temp = (char *) malloc(size);
  if (Input_temp == NULL) {
    return;
  }

  for ( uint8_t i = 0; i < server.args(); i++ ) {
    if (server.argName(i).equals("mode")) {
      settings->mode = server.arg(i).toInt();
    } else if (server.argName(i).equals("protocol")) {
      settings->rf_protocol = server.arg(i).toInt();
    } else if (server.argName(i).equals("band")) {
      settings->band = server.arg(i).toInt();
    } else if (server.argName(i).equals("acft_type")) {
      settings->aircraft_type = server.arg(i).toInt();
    } else if (server.argName(i).equals("alarm")) {
      settings->alarm = server.arg(i).toInt();
    } else if (server.argName(i).equals("txpower")) {
      settings->txpower = server.arg(i).toInt();
    } else if (server.argName(i).equals("volume")) {
      settings->volume = server.arg(i).toInt();
    } else if (server.argName(i).equals("pointer")) {
      settings->pointer = server.arg(i).toInt();
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
    } else if (server.argName(i).equals("stealth")) {
      settings->stealth = server.arg(i).toInt();
    } else if (server.argName(i).equals("no_track")) {
      settings->no_track = server.arg(i).toInt();
    } else if (server.argName(i).equals("power_save")) {
      settings->power_save = server.arg(i).toInt();
    } else if (server.argName(i).equals("rfc")) {
      settings->freq_corr = server.arg(i).toInt();
	} else if (server.argName(i).equals("attention")) {
      settings->alarm_attention = server.arg(i).toInt();
    } else if (server.argName(i).equals("warning")) {
      settings->alarm_warning = server.arg(i).toInt();
    } else if (server.argName(i).equals("danger")) {
      settings->alarm_danger = server.arg(i).toInt();
    } else if (server.argName(i).equals("height")) {
      settings->alarm_height = server.arg(i).toInt();
    } else if (server.argName(i).equals("block_addr")) 
    {
        char buf[6 + 1];
        server.arg(i).toCharArray(buf, sizeof(buf));
        settings->block_addr = strtoul(buf, NULL, 16);
    }
  }
  snprintf_P ( Input_temp, size,
PSTR("<html>\
<head>\
<meta http-equiv='refresh' content='15; url=/'>\
<meta name='viewport' content='width=device-width, initial-scale=1'>\
<title>FlyRF Settings</title>\
</head>\
<body>\
<h2 align=center>Новые настройки:</h2>\
<table width=100%%>\
<tr><th align=left>Mode</th><td align=right>%d</td></tr>\
<tr><th align=left>Protocol</th><td align=right>%d</td></tr>\
<tr><th align=left>Band</th><td align=right>%d</td></tr>\
<tr><th align=left>Aircraft type</th><td align=right>%d</td></tr>\
<tr><th align=left>Tx Power</th><td align=right>%d</td></tr>\
<tr><th align=left>Volume</th><td align=right>%d</td></tr>\
<tr><th align=left>NMEA GNSS</th><td align=right>%s</td></tr>\
<tr><th align=left>NMEA Private</th><td align=right>%s</td></tr>\
<tr><th align=left>NMEA Legacy</th><td align=right>%s</td></tr>\
<tr><th align=left>NMEA Sensors</th><td align=right>%s</td></tr>\
<tr><th align=left>NMEA Out</th><td align=right>%d</td></tr>\
<tr><th align=left>DUMP1090</th><td align=right>%d</td></tr>\
<tr><th align=left>Stealth</th><td align=right>%s</td></tr>\
<tr><th align=left>No track</th><td align=right>%s</td></tr>\
<tr><th align=left>Power save</th><td align=right>%d</td></tr>\
<tr><th align=left>Freq. correction</th><td align=right>%d</td></tr>\
<tr><th align=left>Тревога внимание</th><td align=right>%d</td></tr>\
<tr><th align=left>Тревога предупреждение</th><td align=right>%d</td></tr>\
<tr><th align=left>Тревога опасность</th><td align=right>%d</td></tr>\
<tr><th align=left>Тревога высота</th><td align=right>%d</td></tr>\
<tr><th align=left>Block addr</th><td align=right>%06X</td></tr>\
</table>\
<hr>\
<p align=center><h2 align=center>Выполняется перезагрузка... Пожалуйста, подождите!</h2></p>\
</body>\
</html>"),
  settings->mode, settings->rf_protocol, settings->band,
  settings->aircraft_type,settings->txpower, settings->volume,
  BOOL_STR(settings->nmea_g), BOOL_STR(settings->nmea_p),
  BOOL_STR(settings->nmea_l), BOOL_STR(settings->nmea_s),
  settings->nmea_out,settings->d1090,
  BOOL_STR(settings->stealth), BOOL_STR(settings->no_track),
  settings->power_save, settings->freq_corr, settings->alarm_attention, 
  settings->alarm_warning, settings->alarm_danger, settings->alarm_height,
  settings->block_addr
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


void handleNotFound() 
{
    String message = "File Not Found\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";

    for ( uint8_t i = 0; i < server.args(); i++ ) 
    {
      message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
    }

    server.send ( 404, "text/plain", message );
}


void Web_setup()
{
  server.on ( "/", handleRoot );
  server.on ( "/settings", handleSettings );
  server.on ( "/about", []() {
    SoC->swSer_enableRx(false);
    server.sendHeader(String(F("Cache-Control")), String(F("no-cache, no-store, must-revalidate")));
    server.sendHeader(String(F("Pragma")), String(F("no-cache")));
    server.sendHeader(String(F("Expires")), String(F("-1")));
    server.send_P ( 200, PSTR("text/html"), about_html);
    SoC->swSer_enableRx(true);
  } );

  server.on ( "/input", handleInput );
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

 /* server.on ( "/logo.png", []() 
      {
    server.send_P ( 200, "image/png", Logo, sizeof(Logo) );
  } );*/

/* FLASH memory usage optimization */

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
