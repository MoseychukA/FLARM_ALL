
#if defined(ARDUINO)
#include <SPI.h>
#endif /* ARDUINO */

#include "RF.h"
#include "SoC.h"
#include "EEPROMRF.h"
#include "WebRF.h"

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
};

size_t (*protocol_encode)(void *, ufo_t *);
bool   (*protocol_decode)(void *, ufo_t *, ufo_t *);

static Slots_descr_t Time_Slots, *ts;
static uint8_t       RF_timing = RF_TIMING_INTERVAL;

extern const gnss_chip_ops_t *gnss_chip;

static bool sx1276_probe(void);
static bool sx1262_probe(void);
static void sx12xx_setup(void);
static void sx12xx_channel(int8_t);
static bool sx12xx_receive(void);
static void sx12xx_transmit(void);
static void sx1276_shutdown(void);
static void sx1262_shutdown(void);

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


String Bin2Hex(byte *buffer, size_t size)
{
  String str = "";
  for (int i=0; i < size; i++) {
    byte c = buffer[i];
    str += (c < 0x10 ? "0" : "") + String(c, HEX);
  }
  return str;
}

uint8_t parity(uint32_t x) {
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
    #if !defined(EXCLUDE_SX12XX)
    #if !defined(EXCLUDE_SX1276)
    if (sx1276_ops.probe()) 
    {
      rf_chip = &sx1276_ops;
    #else
        if (false) 
        {
        #endif
        #if defined(USE_BASICMAC)
        #if !defined(EXCLUDE_SX1276)
            SX12XX_LL = &sx127x_ll_ops;
        #endif
    }
    else if (sx1262_ops.probe()) 
    {
      rf_chip = &sx1262_ops;
      SX12XX_LL = &sx126x_ll_ops;
    #endif /* USE_BASICMAC */
    #else
        if (false) 
    {
    #endif /* EXCLUDE_SX12XX */
    }
    if (rf_chip && rf_chip->name) 
    {
        Serial.print(rf_chip->name);
        Serial.println(F(" LoRa is detected."));
    } 
    else 
    {
        Serial.println(F("WARNING! None of supported RFICs is detected!"));
    }
  }
   
  if (rf_chip) 
  {
    rf_chip->setup();

    const rf_proto_desc_t *p;

    switch (settings->rf_protocol)
    {
      case RF_PROTOCOL_OGNTP:     p = &ogntp_proto_desc;  break;
      case RF_PROTOCOL_ADSB_1090: p = &es1090_proto_desc; break;
      case RF_PROTOCOL_LEGACY:
      default:                    p = &legacy_proto_desc; break;
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
  }
  else 
  {
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
  case FLYRF_MODE_TXRX_TEST1:
  case FLYRF_MODE_TXRX_TEST2:
  case FLYRF_MODE_TXRX_TEST3:
  case FLYRF_MODE_TXRX_TEST4:
  case FLYRF_MODE_TXRX_TEST5:
    Time = now();
    RF_timing = RF_timing;// == RF_TIMING_2SLOTS_PPS_SYNC ? RF_TIMING_INTERVAL : RF_timing;
    break;
  case FLYRF_MODE_NORMAL:
  default:
   // pps_btime_ms = SoC->get_PPS_TimeMarker();
    unsigned long time_corr_neg;
    unsigned long ms_since_boot = millis();

    //if (pps_btime_ms) 
    //{
    //  unsigned long last_Commit_Time = ms_since_boot - gnss.time.age();
    //  if (pps_btime_ms <= last_Commit_Time) {
    //    time_corr_neg = (last_Commit_Time - pps_btime_ms) % 1000;
    //  } else {
    //    time_corr_neg = 1000 - ((pps_btime_ms - last_Commit_Time) % 1000);
    //  }
    //  ref_time_ms = (ms_since_boot - pps_btime_ms) <= 1010 ?
    //                pps_btime_ms :
    //                ms_since_boot-(ms_since_boot % 1000)+(pps_btime_ms % 1000);
    //} else 
    //{
      unsigned long last_RMC_Commit = ms_since_boot - gnss.date.age();
      time_corr_neg = gnss_chip ? gnss_chip->rmc_ms : 100;
      ref_time_ms = last_RMC_Commit - time_corr_neg;
    //}

    int yr    = gnss.date.year();
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

    Time = makeTime(tm) + (gnss.time.age() - time_corr_neg) / 1000;
    break;
  }

  uint8_t OGN = (settings->rf_protocol == RF_PROTOCOL_OGNTP ? 1 : 0);
  int8_t chan = -1;

  switch (RF_timing)
  {
  //case RF_TIMING_2SLOTS_PPS_SYNC:
  //  {
  //    unsigned long ms_since_boot = millis();
  //    if ((ms_since_boot - ts->s0.tmarker) >= ts->interval_mid) {
  //      ts->s0.tmarker = ref_time_ms + ts->s0.begin - ts->adj;
  //      ts->current = 0;
  //      chan = (int8_t) RF_FreqPlan.getChannel(Time, ts->current, OGN);
  //    }
  //    if ((ms_since_boot - ts->s1.tmarker) >= ts->interval_mid) {
  //      ts->s1.tmarker = ref_time_ms + ts->s1.begin;
  //      ts->current = 1;
  //      chan = (int8_t) RF_FreqPlan.getChannel(Time, ts->current, OGN);
  //    }
  //  }
  //  break;
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

  if (rf_chip) 
  {
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

  if (protocol_encode) 
  {
    if (millis() > TxTimeMarker) 
    {
      size = (*protocol_encode)((void *) &TxBuffer[0], fop);
    }
  }
  return size;
}

int count_test = 0;

bool RF_Transmit(size_t size, bool wait)
{

  if (rf_chip && (size > 0)) 
  {
    RF_tx_size = size;


    if (!wait || millis() > TxTimeMarker) 
    {

      time_t timestamp = now();

      if (memcmp(TxBuffer, RxBuffer, RF_tx_size) != 0) 
      { 
        rf_chip->transmit();

        if (settings->nmea_p) 
        {

            for (int i = 0; i < RF_tx_size; i++)
            {
                StdOut.print(TxBuffer[i], HEX);
                StdOut.print(" ");
            }

            StdOut.println("End TxBuffer");
            StdOut.print(F("$PSRFO,"));
            StdOut.print((unsigned long) timestamp);
            StdOut.print(F(","));
            StdOut.println(Bin2Hex((byte *) &TxBuffer[0], RF_Payload_Size(settings->rf_protocol)));
        }
        tx_packets_counter++;
      }
      else 
      {

        if (settings->nmea_p) 
        {
          StdOut.println(F("$PSRFE,RF loopback is detected on Tx"));
        }
      }

      RF_tx_size = 0;

      Slot_descr_t *next;
      unsigned long adj;

      TxTimeMarker = millis() + SoC->random(ts->interval_min, ts->interval_max) - ts->air_time;

      //!!switch (RF_timing)
      //{
      ////case RF_TIMING_2SLOTS_PPS_SYNC:
      ////  next = RF_FreqPlan.Channels == 1 ? &(ts->s0) :
      ////         ts->current          == 1 ? &(ts->s0) : &(ts->s1);
      ////  adj  = ts->current ? ts->adj   : 0;
      ////  TxTimeMarker = next->tmarker    +
      ////                 ts->interval_mid +
      ////                 SoC->random(adj, next->duration - ts->air_time);
      ////  break;
      //case RF_TIMING_INTERVAL:
      //default:
      //  TxTimeMarker = millis() + SoC->random(ts->interval_min, ts->interval_max) - ts->air_time;
      //  break;
      //}


      return true;
    }
  }
  return false;
}

bool RF_Receive(void)
{
  bool rval = false;

  if (rf_chip) 
  {
    rval = rf_chip->receive();
  }
  
  return rval;
}

void RF_Shutdown(void)
{
  if (rf_chip) 
  {
    rf_chip->shutdown();
  }
}

uint8_t RF_Payload_Size(uint8_t protocol)
{
  switch (protocol)
  {
    case RF_PROTOCOL_LEGACY:    return legacy_proto_desc.payload_size;
    case RF_PROTOCOL_OGNTP:     return ogntp_proto_desc.payload_size;
    case RF_PROTOCOL_ADSB_1090: return es1090_proto_desc.payload_size;
    default:                    return 0;
  }
}



#if !defined(EXCLUDE_SX12XX)
/*
 * SX12XX-specific code
 *
 *
 */

osjob_t sx12xx_txjob;
osjob_t sx12xx_timeoutjob;

static void sx12xx_tx_func (osjob_t* job);
static void sx12xx_rx_func (osjob_t* job);
static void sx12xx_rx(osjobcb_t func);

static bool sx12xx_receive_complete = false;
bool sx12xx_receive_active = false;
static bool sx12xx_transmit_complete = false;

static int8_t sx12xx_channel_prev = (int8_t) -1;

#if defined(USE_BASICMAC)
void os_getDevEui (u1_t* buf) { }
u1_t os_getRegion (void) { return REGCODE_EU868; }
#else
#if !defined(DISABLE_INVERT_IQ_ON_RX)
#error This example requires DISABLE_INVERT_IQ_ON_RX to be set. Update \
       config.h in the lmic library to set it.
#endif
#endif

#define SX1276_RegVersion          0x42 // common

static u1_t sx1276_readReg (u1_t addr) 
{
#if defined(USE_BASICMAC)
    hal_spi_select(1);
#else
    hal_pin_nss(0);
#endif
    hal_spi(addr & 0x7F);
    u1_t val = hal_spi(0x00);
#if defined(USE_BASICMAC)
    hal_spi_select(0);
#else
    hal_pin_nss(1);
#endif
    return val;
}

static bool sx1276_probe()
{
  u1_t v, v_reset;

  SoC->SPI_begin();

  lmic_hal_init (nullptr);

  // manually reset radio
  hal_pin_rst(0); // drive RST pin low
  hal_waitUntil(os_getTime()+ms2osticks(1)); // wait >100us

  v_reset = sx1276_readReg(SX1276_RegVersion);

  hal_pin_rst(2); // configure RST pin floating!
  hal_waitUntil(os_getTime()+ms2osticks(5)); // wait 5ms

  v = sx1276_readReg(SX1276_RegVersion);

  Serial.print("*** sx1276 v(0x13) = ");
  Serial.println(v,HEX);

  pinMode(lmic_pins.nss, INPUT);
  SPI.end();

  if (v == 0x12 || v == 0x13) 
  {

    if (v_reset == 0x12 || v_reset == 0x13) 
    {
      RF_SX12XX_RST_is_connected = false;
    }

    return true;
  }
  else 
  {
    return false;
  }
}

#if defined(USE_BASICMAC)

#define CMD_READREGISTER            0x1D
#define REG_LORASYNCWORDLSB         0x0741
#define SX126X_DEF_LORASYNCWORDLSB  0x24

static void sx1262_ReadRegs (uint16_t addr, uint8_t* data, uint8_t len) 
{
    hal_spi_select(1);
    hal_pin_busy_wait();
    hal_spi(CMD_READREGISTER);
    hal_spi(addr >> 8);
    hal_spi(addr);
    hal_spi(0x00); // NOP
    for (uint8_t i = 0; i < len; i++) 
    {
        data[i] = hal_spi(0x00);
    }
    hal_spi_select(0);
}

static uint8_t sx1262_ReadReg (uint16_t addr) 
{
    uint8_t val;
    sx1262_ReadRegs(addr, &val, 1);
    return val;
}

static bool sx1262_probe()
{
  u1_t v, v_reset;
 /* pinMode(14, OUTPUT);
  pinMode(17, OUTPUT);
  digitalWrite(14, HIGH);
  digitalWrite(17, LOW);*/

  SoC->SPI_begin();

  lmic_hal_init (nullptr); // initialize hardware (IO, SPI, TIMER, IRQ)

  // manually reset radio
  hal_pin_rst(0); // drive RST pin low
  hal_waitUntil(os_getTime()+ms2osticks(1)); // wait >100us

  v_reset = sx1262_ReadReg(REG_LORASYNCWORDLSB);

  hal_pin_rst(2); // configure RST pin floating!
  hal_waitUntil(os_getTime()+ms2osticks(5)); // wait 5ms

  v = sx1262_ReadReg(REG_LORASYNCWORDLSB);
   
  Serial.print("*** sx1262 v(0x24) = ");
  Serial.println(v, HEX);

  pinMode(lmic_pins.nss, INPUT); 
  SPI.end();

 // u1_t fanet_sw_lsb = ((fanet_proto_desc.syncword[0]  & 0x0F) << 4) | 0x04;
  if (v == SX126X_DEF_LORASYNCWORDLSB/* || v == fanet_sw_lsb*/) 
  {

    if (v_reset == SX126X_DEF_LORASYNCWORDLSB/* || v == fanet_sw_lsb*/) 
    {
      RF_SX12XX_RST_is_connected = false;
    }

    return true;
  } 
  else 
  {
    return false;
  }
}
#endif

static void sx12xx_channel(int8_t channel)
{
  if (channel != -1 && channel != sx12xx_channel_prev) 
  {
    uint32_t frequency = RF_FreqPlan.getChanFrequency((uint8_t) channel);
    int8_t fc = settings->freq_corr;

   // Serial.print("frequency: "); Serial.println(frequency);

    if (sx12xx_receive_active) 
    {
      os_radio(RADIO_RST);
      sx12xx_receive_active = false;
    }

    if (rf_chip->type == RF_IC_SX1276) 
    {
      /* correction of not more than 30 kHz is allowed */
      if (fc > 30)
      {
        fc = 30;
      }
      else if (fc < -30) 
      {
        fc = -30;
      };
    }
    else 
    {
      /* Most of SX1262 designs use TCXO */
      fc = 0;
    }

    /* Actual RF chip's channel registers will be updated before each Tx or Rx session */
    //LMIC.freq = frequency + (fc * 1000);
    LMIC.freq = 868800000UL + (fc * 1000);

    sx12xx_channel_prev = channel;

    //Serial.print("LMIC.freq: "); Serial.println(LMIC.freq);
    //Serial.print("channel: "); Serial.println(channel);

  }
}

static void sx12xx_setup()
{
  SoC->SPI_begin();

  // initialize runtime env
  os_init (nullptr);

  // Reset the MAC state. Session and pending data transfers will be discarded.
  LMIC_reset();

  switch (settings->rf_protocol)
  {
  case RF_PROTOCOL_OGNTP:
    LMIC.protocol   = &ogntp_proto_desc;
    protocol_encode = &ogntp_encode;
    protocol_decode = &ogntp_decode;
    break;
  case RF_PROTOCOL_LEGACY:
  default:
    LMIC.protocol   = &legacy_proto_desc;
    protocol_encode = &legacy_encode;
    protocol_decode = &legacy_decode;
    /*
     * Enforce legacy protocol setting for SX1276
     * if other value (UAT) left in EEPROM from other (UATM) radio
     */
    settings->rf_protocol = RF_PROTOCOL_LEGACY;
    break;
  }

  RF_FreqPlan.setPlan(settings->band, settings->rf_protocol);

    /* Load regional max. EIRP at first */
    LMIC.txpow = RF_FreqPlan.MaxTxPower;

    if (rf_chip->type == RF_IC_SX1262) 
    {
      /* SX1262 is unable to give more than 22 dBm */
      if (LMIC.txpow > 22)
        LMIC.txpow = 22;
    }
    else 
    {
      /* SX1276 is unable to give more than 20 dBm */
      if (LMIC.txpow > 20)
        LMIC.txpow = 20;
    }

}

static void sx12xx_setvars()
{
  if (LMIC.protocol && LMIC.protocol->modulation_type == RF_MODULATION_TYPE_LORA) 
  {
    LMIC.datarate = LMIC.protocol->bitrate;
    LMIC.syncword = LMIC.protocol->syncword[0];
  }
  else 
  {
    LMIC.datarate = DR_FSK;
  }

#if defined(USE_BASICMAC)

#define updr2rps  LMIC_updr2rps

  // LMIC.rps = MAKERPS(sf, BW250, CR_4_5, 0, 0);

  LMIC.noRXIQinversion = true;
  LMIC.rxsyms = 100;

#endif /* USE_BASICMAC */

  // This sets CR 4/5, BW125 (except for DR_SF7B, which uses BW250)
  LMIC.rps = updr2rps(LMIC.datarate);

  //Serial.print("*** LMIC.datarate ");
  //Serial.println(LMIC.datarate);

  //Serial.print("*** LMIC.syncword ");
  //Serial.println(LMIC.syncword);

  //Serial.print("*** LMIC.rps ");
  //Serial.println(LMIC.rps);

}

static bool sx12xx_receive()
{
  bool success = false;

  sx12xx_receive_complete = false;

  if (!sx12xx_receive_active) 
  {
    if (settings->power_save & POWER_SAVE_NORECEIVE) 
    {
      LMIC_shutdown();
    }
    else 
    {
      sx12xx_setvars();
      sx12xx_rx(sx12xx_rx_func);
    }
    sx12xx_receive_active = true;
  }

  if (sx12xx_receive_complete == false) 
  {
    // execute scheduled jobs and events
    os_runstep();
  };

  if (sx12xx_receive_complete == true) 
  {

    u1_t size = LMIC.dataLen - LMIC.protocol->payload_offset - LMIC.protocol->crc_size;

    if (size > sizeof(RxBuffer)) 
    {
      size = sizeof(RxBuffer);
    }


    for (u1_t i=0; i < size; i++) 
    {
        RxBuffer[i] = LMIC.frame[i + LMIC.protocol->payload_offset];
    }

    RF_last_rssi = LMIC.rssi;
    rx_packets_counter++;
    success = true;

    //Serial.print("RF_last_rssi: ");
    //Serial.println(RF_last_rssi);
    //Serial.print("counter: ");
    //Serial.println(rx_packets_counter);

  }

  return success;
}

static void sx12xx_transmit()
{
    digitalWrite(SOC_GPIO_PIN_LED, LOW);
    sx12xx_transmit_complete = false;
    sx12xx_receive_active = false;

    sx12xx_setvars();
    os_setCallback(&sx12xx_txjob, sx12xx_tx_func);

    unsigned long tx_timeout = LMIC.protocol ? (LMIC.protocol->air_time + 25) : 60;
    unsigned long tx_start   = millis();

    while (sx12xx_transmit_complete == false) 
    {
      if ((millis() - tx_start) > tx_timeout) 
      {
        os_radio(RADIO_RST);
        //Serial.println("TX timeout");
        break;
      }

      // execute scheduled jobs and events
      os_runstep();

      yield();
    };

    digitalWrite(SOC_GPIO_PIN_LED, HIGH);
}

static void sx1276_shutdown()
{
  LMIC_shutdown();

  SPI.end();
}

#if defined(USE_BASICMAC)
static void sx1262_shutdown()
{
  os_init (nullptr);
  sx126x_ll_ops.radio_sleep();
  delay(1);

  SPI.end();
}
#endif /* USE_BASICMAC */

// Enable rx mode and call func when a packet is received
static void sx12xx_rx(osjobcb_t func) 
{
  LMIC.osjob.func = func;
  LMIC.rxtime = os_getTime(); // RX _now_
  // Enable "continuous" RX for LoRa only (e.g. without a timeout,
  // still stops after receiving a packet)
  os_radio(LMIC.protocol && LMIC.protocol->modulation_type == RF_MODULATION_TYPE_LORA ? RADIO_RXON : RADIO_RX);
  //Serial.println("RX");
}

static void sx12xx_rx_func (osjob_t* job) 
{

  u1_t crc8, pkt_crc8;
  u2_t crc16, pkt_crc16;
  u1_t i;

  // SX1276 is in SLEEP after IRQ handler, Force it to enter RX mode
  sx12xx_receive_active = false;

  /* FANET (LoRa) LMIC IRQ handler may deliver empty packets here when CRC is invalid. */
  if (LMIC.dataLen == 0) {
    return;
  }

  switch (LMIC.protocol->crc_type)
  {
  case RF_CHECKSUM_TYPE_GALLAGER:
  case RF_CHECKSUM_TYPE_NONE:
     /* crc16 left not initialized */
    break;
  case RF_CHECKSUM_TYPE_CRC8_107:
    crc8 = 0x71;     /* seed value */
    break;
  case RF_CHECKSUM_TYPE_CCITT_0000:
    crc16 = 0x0000;  /* seed value */
    break;
  case RF_CHECKSUM_TYPE_CCITT_FFFF:
  default:
    crc16 = 0xffff;  /* seed value */
    break;
  }

  //Serial.print("Got ");
  //Serial.print(LMIC.dataLen);
  //Serial.println(" bytes");

  switch (LMIC.protocol->type)
  {
  case RF_PROTOCOL_LEGACY:
    /* take in account NRF905/FLARM "address" bytes */
    crc16 = update_crc_ccitt(crc16, 0x31);
    crc16 = update_crc_ccitt(crc16, 0xFA);
    crc16 = update_crc_ccitt(crc16, 0xB6);
    break;
  case RF_PROTOCOL_OGNTP:
  default:
    break;
  }

  for (i = LMIC.protocol->payload_offset;
       i < (LMIC.dataLen - LMIC.protocol->crc_size);
       i++)
  {

    switch (LMIC.protocol->crc_type)
    {
    case RF_CHECKSUM_TYPE_GALLAGER:
    case RF_CHECKSUM_TYPE_NONE:
      break;
    case RF_CHECKSUM_TYPE_CRC8_107:
      update_crc8(&crc8, (u1_t)(LMIC.frame[i]));
      break;
    case RF_CHECKSUM_TYPE_CCITT_FFFF:
    case RF_CHECKSUM_TYPE_CCITT_0000:
    default:
      crc16 = update_crc_ccitt(crc16, (u1_t)(LMIC.frame[i]));
      break;
    }

    switch (LMIC.protocol->whitening)
    {
    case RF_WHITENING_NICERF:
      LMIC.frame[i] ^= pgm_read_byte(&whitening_pattern[i - LMIC.protocol->payload_offset]);
      break;
    case RF_WHITENING_MANCHESTER:
    case RF_WHITENING_NONE:
    default:
      break;
    }

#if DEBUG
    Serial.printf("%02x", (u1_t)(LMIC.frame[i]));
#endif
  }

  switch (LMIC.protocol->crc_type)
  {
  case RF_CHECKSUM_TYPE_NONE:
    sx12xx_receive_complete = true;
    break;
  case RF_CHECKSUM_TYPE_GALLAGER:
    if (LDPC_Check((uint8_t  *) &LMIC.frame[0])) {
#if DEBUG
      Serial.printf(" %02x%02x%02x%02x%02x%02x is wrong FEC",
        LMIC.frame[i], LMIC.frame[i+1], LMIC.frame[i+2],
        LMIC.frame[i+3], LMIC.frame[i+4], LMIC.frame[i+5]);
#endif
      sx12xx_receive_complete = false;
    } else {
      sx12xx_receive_complete = true;
    }
    break;
  case RF_CHECKSUM_TYPE_CRC8_107:
    pkt_crc8 = LMIC.frame[i];
#if DEBUG
    if (crc8 == pkt_crc8 ) {
      Serial.printf(" %02x is valid crc", pkt_crc8);
    } else {
      Serial.printf(" %02x is wrong crc", pkt_crc8);
    }
#endif
    if (crc8 == pkt_crc8) {
      sx12xx_receive_complete = true;
    } else {
      sx12xx_receive_complete = false;
    }
    break;
  case RF_CHECKSUM_TYPE_CCITT_FFFF:
  case RF_CHECKSUM_TYPE_CCITT_0000:
  default:
    pkt_crc16 = (LMIC.frame[i] << 8 | LMIC.frame[i+1]);
#if DEBUG
    if (crc16 == pkt_crc16 ) {
      Serial.printf(" %04x is valid crc", pkt_crc16);
    } else {
      Serial.printf(" %04x is wrong crc", pkt_crc16);
    }
#endif
    if (crc16 == pkt_crc16) {
      sx12xx_receive_complete = true;
    } else {
      sx12xx_receive_complete = false;
    }
    break;
  }

#if DEBUG
  Serial.println();
#endif

}

// Transmit the given string and call the given function afterwards
static void sx12xx_tx(unsigned char *buf, size_t size, osjobcb_t func) {

  u1_t crc8;
  u2_t crc16;

  switch (LMIC.protocol->crc_type)
  {
  case RF_CHECKSUM_TYPE_GALLAGER:
  case RF_CHECKSUM_TYPE_NONE:
     /* crc16 left not initialized */
    break;
  case RF_CHECKSUM_TYPE_CRC8_107:
    crc8 = 0x71;     /* seed value */
    break;
  case RF_CHECKSUM_TYPE_CCITT_0000:
    crc16 = 0x0000;  /* seed value */
    break;
  case RF_CHECKSUM_TYPE_CCITT_FFFF:
  default:
    crc16 = 0xffff;  /* seed value */
    break;
  }
  
  os_radio(RADIO_RST); // Stop RX first
  delay(1); // Wait a bit, without this os_radio below asserts, apparently because the state hasn't changed yet

  LMIC.dataLen = 0;

  switch (LMIC.protocol->type)
  {
    case RF_PROTOCOL_OGNTP:
    default:
    break;
  }

  for (u1_t i=0; i < size; i++) 
  {

    switch (LMIC.protocol->whitening)
    {
    case RF_WHITENING_NICERF:
      LMIC.frame[LMIC.dataLen] = buf[i] ^ pgm_read_byte(&whitening_pattern[i]);
      break;
    case RF_WHITENING_MANCHESTER:
    case RF_WHITENING_NONE:
    default:
      LMIC.frame[LMIC.dataLen] = buf[i];
      break;
    }

    switch (LMIC.protocol->crc_type)
    {
    case RF_CHECKSUM_TYPE_GALLAGER:
    case RF_CHECKSUM_TYPE_NONE:
      break;
    case RF_CHECKSUM_TYPE_CRC8_107:
      update_crc8(&crc8, (u1_t)(LMIC.frame[LMIC.dataLen]));
      break;
    case RF_CHECKSUM_TYPE_CCITT_FFFF:
    case RF_CHECKSUM_TYPE_CCITT_0000:
    default:
      crc16 = update_crc_ccitt(crc16, (u1_t)(LMIC.frame[LMIC.dataLen]));
      break;
    }

    LMIC.dataLen++;
  }

  switch (LMIC.protocol->crc_type)
  {
  case RF_CHECKSUM_TYPE_GALLAGER:
  case RF_CHECKSUM_TYPE_NONE:
    break;
  case RF_CHECKSUM_TYPE_CRC8_107:
    LMIC.frame[LMIC.dataLen++] = crc8;
    break;
  case RF_CHECKSUM_TYPE_CCITT_FFFF:
  case RF_CHECKSUM_TYPE_CCITT_0000:
  default:
    LMIC.frame[LMIC.dataLen++] = (crc16 >>  8) & 0xFF;
    LMIC.frame[LMIC.dataLen++] = (crc16      ) & 0xFF;
    break;
  }

  LMIC.osjob.func = func;
  os_radio(RADIO_TX);
  //Serial.println("TX");
}

static void sx12xx_txdone_func (osjob_t* job) {
  sx12xx_transmit_complete = true;
}

static void sx12xx_tx_func (osjob_t* job) {

  if (RF_tx_size > 0) {
    sx12xx_tx((unsigned char *) &TxBuffer[0], RF_tx_size, sx12xx_txdone_func);
  }
}
#endif /* EXCLUDE_SX12XX */

