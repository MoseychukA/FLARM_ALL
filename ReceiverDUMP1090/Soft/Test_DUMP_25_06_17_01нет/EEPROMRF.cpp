/*
 * EEPROMHelper.cpp
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

#if defined(EXCLUDE_EEPROM)
void EEPROM_setup()    {}
void EEPROM_store()    {}
#else

#include "EEPROMRF.h"
#include "RF.h"
#include "Bluetooth.h"
#include "TrafficHelper.h"
#include "NMEA.h"
#include "D1090.h"
#include <protocol.h>
#include <freqplan.h>

// start reading from the first byte (address 0) of the EEPROM

eeprom_t eeprom_block;
settings_t *settings;

void EEPROM_setup()
{
  int cmd = EEPROM_EXT_LOAD;

  if (!SoC->EEPROM_begin(sizeof(eeprom_t)))
  {
    Serial.print(F("ERROR: Failed to initialize "));
    Serial.print(sizeof(eeprom_t));
    Serial.println(F(" bytes of EEPROM!"));
    Serial.flush();
    delay(1000000);
  }

  for (int i=0; i<sizeof(eeprom_t); i++) 
  {
    eeprom_block.raw[i] = EEPROM.read(i);
  }

  if (eeprom_block.field.magic != FLYRF_EEPROM_MAGIC) {
    Serial.println(F("WARNING! User defined settings are not initialized yet. Loading defaults..."));

    EEPROM_defaults();
    cmd = EEPROM_EXT_DEFAULTS;
  }
  else 
  {
    Serial.print(F("EEPROM version: "));
    Serial.println(eeprom_block.field.version);

    if (eeprom_block.field.version != FLYRF_EEPROM_VERSION) 
    {
      Serial.println(F("WARNING! Version mismatch of user defined settings. Loading defaults..."));

      EEPROM_defaults();
      cmd = EEPROM_EXT_DEFAULTS;
    }
  }
  settings = &eeprom_block.field.settings;

  SoC->EEPROM_extension(cmd);
}

void EEPROM_defaults()
{
  eeprom_block.field.magic                  = FLYRF_EEPROM_MAGIC;
  eeprom_block.field.version                = FLYRF_EEPROM_VERSION;
  eeprom_block.field.settings.mode          = FLYRF_MODE_NORMAL;
  eeprom_block.field.settings.rf_protocol   = RF_PROTOCOL_OGNTP;
  eeprom_block.field.settings.band          = RF_BAND_RU;
  eeprom_block.field.settings.aircraft_type = AIRCRAFT_TYPE_GLIDER;
  eeprom_block.field.settings.txpower       = RF_TX_POWER_FULL;
  eeprom_block.field.settings.bluetooth     = BLUETOOTH_NONE;
  eeprom_block.field.settings.alarm         = TRAFFIC_ALARM_DISTANCE;
  eeprom_block.field.settings.nmea_g        = true;
  eeprom_block.field.settings.nmea_p        = false;
  eeprom_block.field.settings.nmea_l        = true;
  eeprom_block.field.settings.nmea_s        = true;
  eeprom_block.field.settings.nmea_out      = NMEA_UART;

  eeprom_block.field.settings.d1090         = D1090_OFF;
  eeprom_block.field.settings.stealth       = false;
  eeprom_block.field.settings.no_track      = false;
  eeprom_block.field.settings.power_save    = POWER_SAVE_NONE;
  eeprom_block.field.settings.freq_corr     = 0;
  eeprom_block.field.settings.igc_key[0]    = 0;
  eeprom_block.field.settings.igc_key[1]    = 0;
  eeprom_block.field.settings.igc_key[2]    = 0;
  eeprom_block.field.settings.igc_key[3]    = 0;

  eeprom_block.field.settings.alarm_attention = 2000;
  eeprom_block.field.settings.alarm_warning   = 1000;
  eeprom_block.field.settings.alarm_danger    = 500;
  eeprom_block.field.settings.alarm_height    = 50;
 
  eeprom_block.field.settings.CountNotReadMessage = 0;  // Счетчик количества непрочитанных сообщений
  eeprom_block.field.settings.CurrentCountMessage = 0;  // Счетчик количества сообщений

  eeprom_block.field.settings.block_addr  = 0x000000;
  eeprom_block.field.settings.rssi_view   = VIEW_RSSI_OFF;
  eeprom_block.field.settings.gsm_send    = GSM_SEND_OFF;
  eeprom_block.field.settings.ram_view    = VIEW_RAM_OFF;
  eeprom_block.field.settings.akk_view    = VIEW_AKK_OFF;
  eeprom_block.field.settings.sos_view    = VIEW_SOS_OFF;
  eeprom_block.field.settings.akk_min     = 320;         // Минимальное значение заряда аккумулятора
  eeprom_block.field.settings.akk_max     = 420;         // Максимальное значение заряда аккумулятора
  eeprom_block.field.settings.akk_koef    = 90;          // Коэффициет пересчета заряда аккумулятора
  eeprom_block.field.settings.input_coordinates = IMPUT_COORD_AUTO;
  eeprom_block.field.settings.test_latitude   = 55.12345;
  eeprom_block.field.settings.test_longitude  = 37.12345;
  eeprom_block.field.settings.input_N_S = IMPUT_N;
  eeprom_block.field.settings.input_E_W = IMPUT_E;
  eeprom_block.field.settings.view_test_coord = VIEW_TEST_COORD_OFF;
  eeprom_block.field.settings.coord_max100 = COORD_MIN;
 }

void EEPROM_store()
{
  for (int i=0; i<sizeof(eeprom_t); i++) 
  {
    EEPROM.write(i, eeprom_block.raw[i]);
  }

  SoC->EEPROM_extension(EEPROM_EXT_STORE); 

  EEPROM_commit();
}

#endif /* EXCLUDE_EEPROM */
