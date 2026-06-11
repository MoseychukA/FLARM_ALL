/*
 * RFHelper.cpp
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
#if defined(ARDUINO)
#include <SPI.h>
#endif /* ARDUINO */

#include "RF.h"
#include "SoC.h"
#include "EEPROMRF.h"
//#include "WebRF.h"
#include <LibAPRSesp.h>

byte RxBuffer[MAX_PKT_SIZE] __attribute__((aligned(sizeof(uint32_t))));

unsigned long TxTimeMarker = 0;
byte TxBuffer[MAX_PKT_SIZE] __attribute__((aligned(sizeof(uint32_t))));

uint32_t tx_packets_counter = 0;
uint32_t rx_packets_counter = 0;

int8_t RF_last_rssi = 0;

FreqPlan RF_FreqPlan;

static size_t RF_tx_size = 0;

const rfchip_ops_t *rf_chip = NULL;
bool RF_SX12XX_RST_is_connected = true;

const char *Protocol_ID[] = {
  [RF_PROTOCOL_LEGACY]    = "LEG",
  [RF_PROTOCOL_OGNTP]     = "OGN",
  [RF_PROTOCOL_P3I]       = "P3I",
  [RF_PROTOCOL_ADSB_1090] = "ADS",
};

size_t (*protocol_encode)(void *, ufo_t *);
bool   (*protocol_decode)(void *, ufo_t *, ufo_t *);

static Slots_descr_t Time_Slots, *ts;
static uint8_t       RF_timing = RF_TIMING_INTERVAL;

//!!extern const gnss_chip_ops_t *gnss_chip;

static bool sx1276_probe(void);
static bool sx1262_probe(void);
static void sx12xx_setup(void);
static void sx12xx_channel(int8_t);
static bool sx12xx_receive(void);
static void sx12xx_transmit(void);
static void sx1276_shutdown(void);
static void sx1262_shutdown(void);

static bool ognrf_probe(void);
static void ognrf_setup(void);
static void ognrf_channel(int8_t);
static bool ognrf_receive(void);
static void ognrf_transmit(void);
static void ognrf_shutdown(void);


#if !defined(EXCLUDE_SX12XX)
const rfchip_ops_t sx1276_ops = {
  RF_IC_SX1276,
  "SX127x",
  sx1276_probe,
  sx12xx_setup,
  sx12xx_channel,
  sx12xx_receive,
  sx12xx_transmit,
  sx1276_shutdown
};
#if defined(USE_BASICMAC)
const rfchip_ops_t sx1262_ops = {
  RF_IC_SX1262,
  "SX126x",
  sx1262_probe,
  sx12xx_setup,
  sx12xx_channel,
  sx12xx_receive,
  sx12xx_transmit,
  sx1262_shutdown
};
#endif /* USE_BASICMAC */
#endif /*EXCLUDE_SX12XX */

#if defined(USE_OGN_RF_DRIVER)

#define vTaskDelay  delay

#if defined(WITH_SI4X32)
#include <rf/si4x32/rfm.h>
#else
#include <rf/combo/rfm.h>
#endif /* WITH_SI4X32 */

const rfchip_ops_t ognrf_ops = {
  RF_DRV_OGN,
  "OGNDRV",
  ognrf_probe,
  ognrf_setup,
  ognrf_channel,
  ognrf_receive,
  ognrf_transmit,
  ognrf_shutdown
};
#endif /* USE_OGN_RF_DRIVER */


String Bin2Hex(byte *buffer, size_t size)
{
  String str = "";
  for (int i=0; i < size; i++) {
    byte c = buffer[i];
    str += (c < 0x10 ? "0" : "") + String(c, HEX);
  }
  return str;
}

uint8_t parity(uint32_t x) 
{
    uint8_t parity=0;
    while (x > 0) {
      if (x & 0x1) {
          parity++;
      }
      x >>= 1;
    }
    return (parity % 2);
}
 
byte RF_setup(void)
{

  if (rf_chip == NULL) 
  {
#if !defined(USE_OGN_RF_DRIVER)
#if !defined(EXCLUDE_SX12XX)
#if !defined(EXCLUDE_SX1276)
    if (sx1276_ops.probe()) {
      rf_chip = &sx1276_ops;
#else
    if (false) {
#endif
#if defined(USE_BASICMAC)
#if !defined(EXCLUDE_SX1276)
      SX12XX_LL = &sx127x_ll_ops;
#endif
    } else if (sx1262_ops.probe()) {
      rf_chip = &sx1262_ops;
      SX12XX_LL = &sx126x_ll_ops;
#endif /* USE_BASICMAC */
#else
    if (false) {
#endif /* EXCLUDE_SX12XX */
    }
    if (rf_chip && rf_chip->name) {
      Serial.print(rf_chip->name);
      Serial.println(F(" RFIC is detected."));
    } else {
      Serial.println(F("WARNING! None of supported RFICs is detected!"));
    }
#else /* USE_OGN_RF_DRIVER */
    if (ognrf_ops.probe()) {
      rf_chip = &ognrf_ops;
      Serial.println(F("OGN_DRV: RFIC is detected."));
    } else {
      Serial.println(F("WARNING! RFIC is NOT detected."));
    }
#endif /* USE_OGN_RF_DRIVER */
  }

  if (rf_chip) {
    rf_chip->setup();

    const rf_proto_desc_t *p;

    switch (settings->rf_protocol)
    {
      case RF_PROTOCOL_OGNTP:     p = &ogntp_proto_desc;  break;
      //case RF_PROTOCOL_P3I:       p = &p3i_proto_desc;    break;
      //case RF_PROTOCOL_FANET:     p = &fanet_proto_desc;  break;
      //case RF_PROTOCOL_ADSB_1090: p = &es1090_proto_desc; break;
      case RF_PROTOCOL_APRS:
#if defined(ENABLE_PROL)
        if (rf_chip->type == RF_IC_SX1276 || rf_chip->type == RF_IC_SX1262) {
          p = &prol_proto_desc;
        } else
#endif /* ENABLE_PROL */
        {
          //p = &aprs_proto_desc;
        }
        break;
      case RF_PROTOCOL_LEGACY:
      default:      
      //p = &legacy_proto_desc; 
      break;
    }

    RF_timing         = p->tm_type;

    ts                = &Time_Slots;
    ts->air_time      = p->air_time;
    ts->interval_min  = p->tx_interval_min;
    ts->interval_max  = p->tx_interval_max;
    ts->interval_mid  = (p->tx_interval_max + p->tx_interval_min) / 2;
    ts->s0.begin      = p->slot0.begin;
    ts->s1.begin      = p->slot1.begin;
    ts->s0.duration   = p->slot0.end - p->slot0.begin;
    ts->s1.duration   = p->slot1.end - p->slot1.begin;

    uint16_t duration = ts->s0.duration + ts->s1.duration;
    ts->adj = duration > ts->interval_mid ? 0 : (ts->interval_mid - duration) / 2;

    return rf_chip->type;
  } else {
    return RF_IC_NONE;
  }
}

void RF_SetChannel(void)
{
  tmElements_t  tm;
  time_t        Time;
  unsigned long pps_btime_ms, ref_time_ms;

  switch (settings->mode)
  {
  case FLYRF_MODE_TXRX_TEST0:
  case FLYRF_MODE_TXRX_TEST1:
  case FLYRF_MODE_TXRX_TEST2:
  case FLYRF_MODE_TXRX_TEST3:
  case FLYRF_MODE_TXRX_TEST4:
  case FLYRF_MODE_TXRX_TEST5:
    Time = now();
    RF_timing = RF_timing == RF_TIMING_2SLOTS_PPS_SYNC ?
                RF_TIMING_INTERVAL : RF_timing;
    break;
  case FLYRF_MODE_NORMAL:
  default:
    pps_btime_ms = SoC->get_PPS_TimeMarker();
    unsigned long time_corr_neg;
    unsigned long ms_since_boot = millis();

    /*if (pps_btime_ms) 
    {
      unsigned long last_Commit_Time = ms_since_boot - gnss.time.age();
      if (pps_btime_ms <= last_Commit_Time) {
        time_corr_neg = (last_Commit_Time - pps_btime_ms) % 1000;
      } else {
        time_corr_neg = 1000 - ((pps_btime_ms - last_Commit_Time) % 1000);
      }
      ref_time_ms = (ms_since_boot - pps_btime_ms) <= 1010 ?
                    pps_btime_ms :
                    ms_since_boot-(ms_since_boot % 1000)+(pps_btime_ms % 1000);
    }
    else 
    {
      unsigned long last_RMC_Commit = ms_since_boot - gnss.date.age();
      time_corr_neg = gnss_chip ? gnss_chip->rmc_ms : 100;
      ref_time_ms = last_RMC_Commit - time_corr_neg;
    }*/

    /*int yr    = gnss.date.year();
    if( yr > 99)
        yr    = yr - 1970;
    else
        yr    += 30;
    tm.Year   = yr;
    tm.Month  = gnss.date.month();
    tm.Day    = gnss.date.day();
    tm.Hour   = gnss.time.hour();
    tm.Minute = gnss.time.minute();
    tm.Second = gnss.time.second();

    Time = makeTime(tm) + (gnss.time.age() - time_corr_neg) / 1000;*/
    break;
  }

  uint8_t OGN = (settings->rf_protocol == RF_PROTOCOL_OGNTP ? 1 : 0);
  int8_t chan = -1;

  switch (RF_timing)
  {
  case RF_TIMING_2SLOTS_PPS_SYNC:
    {
      unsigned long ms_since_boot = millis();
      if ((ms_since_boot - ts->s0.tmarker) >= ts->interval_mid) {
        ts->s0.tmarker = ref_time_ms + ts->s0.begin - ts->adj;
        ts->current = 0;
        chan = (int8_t) RF_FreqPlan.getChannel(Time, ts->current, OGN);
      }
      if ((ms_since_boot - ts->s1.tmarker) >= ts->interval_mid) {
        ts->s1.tmarker = ref_time_ms + ts->s1.begin;
        ts->current = 1;
        chan = (int8_t) RF_FreqPlan.getChannel(Time, ts->current, OGN);
      }
    }
    break;
  case RF_TIMING_INTERVAL:
  default:
    chan = (int8_t) RF_FreqPlan.getChannel(Time, 0, OGN);
    break;
  }

#if DEBUG
  Serial.print("Plan: "); Serial.println(RF_FreqPlan.Plan);
  Serial.print("OGN: "); Serial.println(OGN);
  Serial.print("Channel: "); Serial.println(chan);
#endif

  if (rf_chip) {
    rf_chip->channel(chan);
  }
}

void RF_loop()
{
  RF_SetChannel();
}

size_t RF_Encode(ufo_t *fop)
{
  size_t size = 0;
  if (protocol_encode) {

    if (settings->txpower == RF_TX_POWER_OFF ) {
      return size;
    }

    if (millis() > TxTimeMarker) {
      size = (*protocol_encode)((void *) &TxBuffer[0], fop);
    }
  }
  return size;
}

bool RF_Transmit(size_t size, bool wait)
{
  if (rf_chip && (size > 0)) {
    RF_tx_size = size;

    if (settings->txpower == RF_TX_POWER_OFF ) {
      return true;
    }

    if (!wait || millis() > TxTimeMarker) {

      time_t timestamp = now();

      if (memcmp(TxBuffer, RxBuffer, RF_tx_size) != 0) {

        rf_chip->transmit();

        if (settings->nmea_p) {
          StdOut.print(F("$PSRFO,"));
          StdOut.print((unsigned long) timestamp);
          StdOut.print(F(","));
          StdOut.println(Bin2Hex((byte *) &TxBuffer[0],
                                 RF_Payload_Size(settings->rf_protocol)));
        }
        tx_packets_counter++;

      } else {

        if (settings->nmea_p) {
          StdOut.println(F("$PSRFE,RF loopback is detected on Tx"));
        }
      }

      RF_tx_size = 0;

      Slot_descr_t *next;
      unsigned long adj;

      switch (RF_timing)
      {
      case RF_TIMING_2SLOTS_PPS_SYNC:
        next = RF_FreqPlan.Channels == 1 ? &(ts->s0) :
               ts->current          == 1 ? &(ts->s0) : &(ts->s1);
        adj  = ts->current ? ts->adj   : 0;
        TxTimeMarker = next->tmarker    +
                       ts->interval_mid +
                       SoC->random(adj, next->duration - ts->air_time);
        break;
      case RF_TIMING_INTERVAL:
      default:
        TxTimeMarker = millis() + SoC->random(ts->interval_min, ts->interval_max) - ts->air_time;
        break;
      }

      return true;
    }
  }
  return false;
}

bool RF_Receive(void)
{
  bool rval = false;

  if (rf_chip) {
    rval = rf_chip->receive();
  }
  
  return rval;
}

void RF_Shutdown(void)
{
  if (rf_chip) {
    rf_chip->shutdown();
  }
}

uint8_t RF_Payload_Size(uint8_t protocol)
{
  switch (protocol)
  {
    case RF_PROTOCOL_LEGACY:    return legacy_proto_desc.payload_size;
    case RF_PROTOCOL_OGNTP:     return ogntp_proto_desc.payload_size;
    //case RF_PROTOCOL_P3I:       return p3i_proto_desc.payload_size;
    //case RF_PROTOCOL_FANET:     return fanet_proto_desc.payload_size;
    //case RF_PROTOCOL_ADSB_1090: return es1090_proto_desc.payload_size;
    //case RF_PROTOCOL_APRS:
#if defined(ENABLE_PROL)
      if (rf_chip->type == RF_IC_SX1276 || rf_chip->type == RF_IC_SX1262) {
        return prol_proto_desc.payload_size;
      } else
#endif /* ENABLE_PROL */
      {
        //return aprs_proto_desc.payload_size;
      }
    default:                    return 0;
  }
}

#if defined(USE_OGN_RF_DRIVER)
/*
 * OGN driver specific code
 *
 *
 */

static RFM_TRX  TRX;

static int8_t ognrf_channel_prev  = (int8_t) -1;
static bool ognrf_receive_active   = false;

void RFM_Select  (void)                 { hal_pin_nss(0); }
void RFM_Deselect(void)                 { hal_pin_nss(1); }
uint8_t RFM_TransferByte(uint8_t Byte)  { return hal_spi(Byte); }

bool RFM_IRQ_isOn(void)   { return lmic_pins.dio[0] == LMIC_UNUSED_PIN ? \
                                  false : digitalRead(lmic_pins.dio[0]); }

#ifdef WITH_RFM95                     // RESET is active LOW
void RFM_RESET(uint8_t On)
{ if(On) hal_pin_rst(0);
    else hal_pin_rst(1); }
#endif

#if defined(WITH_RFM69) || defined(WITH_SX1272) // RESET is active HIGH
void RFM_RESET(uint8_t On)
{ if(On) hal_pin_rst(1);
    else hal_pin_rst(0); }
#endif

static bool ognrf_probe()
{
  bool success = false;

  TRX.Select       = RFM_Select;
  TRX.Deselect     = RFM_Deselect;
  TRX.TransferByte = RFM_TransferByte;
  TRX.RESET        = RFM_RESET;

  SoC->SPI_begin();
  lmic_hal_init (nullptr);

  TRX.RESET(1);                      // RESET active
  vTaskDelay(10);                    // wait 10ms
  TRX.RESET(0);                      // RESET released
  vTaskDelay(10);                    // wait 10ms

  uint8_t ChipVersion = TRX.ReadVersion();

  pinMode(lmic_pins.nss, INPUT);
  SPI.end();

#if defined(WITH_RFM95)
  if (ChipVersion == 0x12 || ChipVersion == 0x13) success = true;
#endif /* WITH_RFM95 */
#if defined(WITH_RFM69)
  if (ChipVersion == 0x24) success = true;
#endif /* WITH_RFM69 */
#if defined(WITH_SX1272)
  if (ChipVersion == 0x22) success = true;
#endif /* WITH_SX1272 */
#if defined(WITH_SI4X32)
  if (ChipVersion == 0x06 /* 4032 */ ||
      ChipVersion == 0x08 /* 4432 */ ) success = true;
#endif /* WITH_SI4X32 */

  return success;
}

static void ognrf_channel(int8_t channel)
{
  if (channel != -1 && channel != ognrf_channel_prev) {

    if (ognrf_receive_active) {

      TRX.WriteMode(RF_OPMODE_STANDBY);
      vTaskDelay(1);

      /* restart Rx upon a channel switch */
      ognrf_receive_active = false;
    }

    TRX.setChannel(channel & 0x7F);

    ognrf_channel_prev = channel;
  }
}

static void ognrf_setup()
{
  uint8_t TxPower = 0;

  /* Enforce radio settings to follow OGNTP protocol's RF specs */
  settings->rf_protocol = RF_PROTOCOL_OGNTP;

  RF_FreqPlan.setPlan(settings->band, settings->rf_protocol);

  LMIC.protocol = &ogntp_proto_desc;

  protocol_encode = &ogntp_encode;
  protocol_decode = &ogntp_decode;

  TRX.Select       = RFM_Select;
  TRX.Deselect     = RFM_Deselect;
  TRX.TransferByte = RFM_TransferByte;
  TRX.DIO0_isOn    = RFM_IRQ_isOn;
  TRX.RESET        = RFM_RESET;

  SoC->SPI_begin();
  lmic_hal_init (nullptr);

  TRX.RESET(1);                      // RESET active
  vTaskDelay(10);                    // wait 10ms
  TRX.RESET(0);                      // RESET released
  vTaskDelay(10);                    // wait 10ms

  // set TRX base frequency and channel separation
  TRX.setBaseFrequency(RF_FreqPlan.BaseFreq);
  TRX.setChannelSpacing(RF_FreqPlan.ChanSepar);
  TRX.setFrequencyCorrection(0);

  TRX.Configure(0, ogntp_proto_desc.syncword);  // setup RF chip parameters and set to channel #0
  TRX.WriteMode(RF_OPMODE_STANDBY);             // set RF chip mode to STANDBY

  switch(settings->txpower)
  {
  case RF_TX_POWER_FULL:

    /* Load regional max. EIRP at first */
    TxPower = RF_FreqPlan.MaxTxPower;

    if (TxPower > 20)
      TxPower = 20;
#if 1
    if (TxPower > 17)
      TxPower = 17;
#endif

#ifdef WITH_RFM69
    TRX.WriteTxPower(TxPower, RFM69_POWER_RATING == 1 ? true : false);
#else
    TRX.WriteTxPower(TxPower);
#endif /* WITH_RFM69 */
    break;
  case RF_TX_POWER_OFF:
  case RF_TX_POWER_LOW:
  default:
    TRX.WriteTxPowerMin();
    break;
  }

  /* Leave IC in standby mode */
}

static bool ognrf_receive()
{
  bool success = false;

#if !defined(WITH_SI4X32)

  uint8_t RxRSSI = 0;
  uint8_t Err [OGNTP_PAYLOAD_SIZE + OGNTP_CRC_SIZE];

  // Put into receive mode
  if (!ognrf_receive_active) {

//    TRX.ClearIrqFlags();

    TRX.WriteSYNC(7, 7, ogntp_proto_desc.syncword); // Shorter SYNC for RX
    TRX.WriteMode(RF_OPMODE_RECEIVER);
    vTaskDelay(1);

    ognrf_receive_active = true;
  }

  if(TRX.DIO0_isOn()) {
    RxRSSI = TRX.ReadRSSI();

    TRX.ReadPacket(RxBuffer, Err);
    if (LDPC_Check((uint8_t  *) RxBuffer) == 0) {
      success = true;
    }
  }

  if (success) {
    RF_last_rssi = RxRSSI;
    rx_packets_counter++;
  }

#endif /* WITH_SI4X32 */

  return success;
}

static void ognrf_transmit()
{
  ognrf_receive_active = false;

#if defined(WITH_SI4X32)

  TRX.WritePacket((uint8_t *) &TxBuffer[0]);
  TRX.Transmit();
  vTaskDelay(6);

#else

  TRX.WriteMode(RF_OPMODE_STANDBY);
  vTaskDelay(1);

  TRX.WriteSYNC(8, 7, ogntp_proto_desc.syncword);            // Full SYNC for TX

  TRX.ClearIrqFlags();
  TRX.WritePacket((uint8_t *) &TxBuffer[0]);

  TRX.WriteMode(RF_OPMODE_TRANSMITTER);
  vTaskDelay(5);

  uint8_t Break=0;
  for(uint16_t Wait=400; Wait; Wait--)        // wait for transmission to end
  {
    uint16_t Flags=TRX.ReadIrqFlags();
    if(Flags&RF_IRQ_PacketSent) Break++;
    if(Break>=2) break;
  }

  TRX.WriteMode(RF_OPMODE_STANDBY);

#endif /* WITH_SI4X32 */
}

static void ognrf_shutdown()
{
  TRX.WriteMode(RF_OPMODE_STANDBY);
  SPI.end();

  pinMode(lmic_pins.nss, INPUT);
}

#endif /* USE_OGN_RF_DRIVER */

AX25Msg Incoming_APRS_Packet;

