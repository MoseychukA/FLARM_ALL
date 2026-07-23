#include "SoC.h"
#include "EEPROMRF.h"
#include "RF.h"
#include "Bluetooth.h"
#include "TrafficHelper.h"
#include "NMEA.h"
#include "GDL90.h"
#include <protocol.h>
#include <freqplan.h>
#include <EEPROM.h>

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

  if (eeprom_block.field.magic != FLYRF_EEPROM_MAGIC) 
  {
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
  eeprom_block.field.settings.nmea_out      = NMEA_UDP;
  eeprom_block.field.settings.gdl90         = hw_info.model == FLYRF_MODEL_ES ?
                                              GDL90_USB : GDL90_OFF;
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
  eeprom_block.field.settings.tracker_send = TRACKER_SEND_OFF;
  eeprom_block.field.settings.power_view = VIEW_AKK_OFF;
  eeprom_block.field.settings.voltage_view = VOLTAGE_VIEW_OFF;
  eeprom_block.field.settings.sos_view    = VIEW_SOS_OFF;
  eeprom_block.field.settings.out_of_sync = OUT_OF_SYNC_OFF;
  eeprom_block.field.settings.default_settings = SETTINGS_OFF;
  eeprom_block.field.settings.input_coordinates = IMPUT_COORD_MANUAL;
  eeprom_block.field.settings.test_latitude   = 55.93574;
  eeprom_block.field.settings.test_longitude  = 37.34873;
  eeprom_block.field.settings.input_E_W = IMPUT_E;
  eeprom_block.field.settings.input_N_S = IMPUT_N;
  eeprom_block.field.settings.view_test_coord = VIEW_TEST_COORD_OFF;
  eeprom_block.field.settings.serial_out = SEND_SERIAL_OFF;
  eeprom_block.field.settings.threshold_level = 910;
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

void EEPROM_clear()
{
    memset(&eeprom_block, 0xFF, sizeof(eeprom_block));
    for (unsigned int i = 0; i < sizeof(eeprom_t); i++)
    {
        EEPROM.write(i, 0xFF);
    }
    EEPROM.commit();
    EEPROM_defaults();
    EEPROM_store();
    Serial.println(F("EEPROM: Настройки сброшены и установлены значения по умолчанию."));
}
