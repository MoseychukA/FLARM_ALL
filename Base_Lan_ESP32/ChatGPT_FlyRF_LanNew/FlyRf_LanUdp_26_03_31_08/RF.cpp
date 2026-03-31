#include <Arduino.h>
#include "RF.h"
#include "FlarmDecoder.h"
#include "Log.h"
#include "EEPROMRF.h"
#include "NMEA.h"
#include <SPI.h>
#include <string.h>


static uint32_t g_flarmPackets = 0;
static FlarmRawPacket g_lastPacket = {};

//==============================================================================

#include "Container.h"

byte RxBuffer[MAX_PKT_SIZE] __attribute__((aligned(sizeof(uint32_t))));

unsigned long TxTimeMarker = 0;
byte TxBuffer[MAX_PKT_SIZE] __attribute__((aligned(sizeof(uint32_t))));

static portMUX_TYPE g_packetCountersMux = portMUX_INITIALIZER_UNLOCKED;
volatile uint32_t tx_packets_counter = 0;
volatile uint32_t rx_packets_counter = 0;

int8_t RF_last_rssi = 0;

FreqPlan RF_FreqPlan;

static size_t RF_tx_size = 0;
static volatile bool g_rxPacketPending = false;

const rfchip_ops_t* rf_chip = NULL;
bool RF_SX12XX_RST_is_connected = true;

size_t(*protocol_encode)(void*, ufo_t*);
bool   (*protocol_decode)(void*, ufo_t*, ufo_t*);

static Slots_descr_t Time_Slots, * ts;
static uint8_t       RF_timing = RF_TIMING_INTERVAL;


static bool sx1276_probe(void);
static void sx12xx_setup(void);
static void sx12xx_channel(int8_t);
static bool sx12xx_receive(void);
static void sx12xx_transmit(void);
static void fillProtocolSelfAircraft(ufo_t *dst);

const rfchip_ops_t sx1276_ops = {
  RF_IC_SX1276,
  "SX127x",
  sx1276_probe,
  sx12xx_setup,
  sx12xx_channel,
  sx12xx_receive,
  sx12xx_transmit,
};


void RF_GetPacketCounters(uint32_t &tx, uint32_t &rx)
{
    portENTER_CRITICAL(&g_packetCountersMux);
    tx = tx_packets_counter;
    rx = rx_packets_counter;
    portEXIT_CRITICAL(&g_packetCountersMux);
}

static void printDecodedLoRaPacketToSerial(const ufo_t *pkt, int16_t rawRssi, size_t rxSize)
{
    if (pkt == nullptr)
    {
        return;
    }

    char callsign[9] = {};
    memcpy(callsign, pkt->callsign, 8);
    callsign[8] = 0;

    Serial.printf("$PFLRD,ADDR=%06lX,CALL=%s,ALT=%.0f,PALT=%.0f,SPD=%.1f,CRS=%.1f,VS=%d,LAT=%.6f,LON=%.6f,RSSI=%d,SRC=%u,LEN=%u\r\n",
                  (unsigned long)(pkt->addr & 0xFFFFFFUL),
                  callsign,
                  (double)pkt->altitude,
                  (double)pkt->pressure_altitude,
                  (double)pkt->speed,
                  (double)pkt->course,
                  pkt->vert_rate,
                  (double)pkt->latitude,
                  (double)pkt->longitude,
                  (int)rawRssi,
                  (unsigned)pkt->signal_source,
                  (unsigned)rxSize);
    vTaskDelay(pdMS_TO_TICKS(2));
}

static void fillProtocolSelfAircraft(ufo_t *dst)
{
    if (dst == nullptr)
    {
        return;
    }

    *dst = EmptyFO;
    dst->addr = ThisAircraft.addr;
    dst->squawk = ThisAircraft.squawk;
    memcpy(dst->callsign, ThisAircraft.callsign, sizeof(ThisAircraft.callsign));
    dst->altitude = ThisAircraft.altitude;
    dst->pressure_altitude = ThisAircraft.pressure_altitude;
    dst->course = ThisAircraft.course;
    dst->speed = ThisAircraft.speed;
    dst->vert_rate = ThisAircraft.vert_rate;
    dst->vs = (float)ThisAircraft.vert_rate;
    dst->latitude = ThisAircraft.latitude;
    dst->longitude = ThisAircraft.longitude;
    dst->local_latitude = ThisAircraft.local_latitude;
    dst->local_longitude = ThisAircraft.local_longitude;
    dst->timestamp = (time_t)(millis() / 1000UL);
    dst->timemsg = dst->timestamp;
}

String Bin2Hex(byte* buffer, size_t size)
{
    String str = "";
    for (int i = 0; i < size; i++) {
        byte c = buffer[i];
        str += (c < 0x10 ? "0" : "") + String(c, HEX);
    }
    return str;
}

uint8_t parity(uint32_t x) {
    uint8_t parity = 0;
    while (x > 0) {
        if (x & 0x1) {
            parity++;
        }
        x >>= 1;
    }
    return (parity % 2);
}

byte RF_setup()
{
    pinMode(FLARM_RFM95_CS_PIN, OUTPUT);
    digitalWrite(FLARM_RFM95_CS_PIN, HIGH);

    pinMode(FLARM_RFM95_RST_PIN, OUTPUT);
    digitalWrite(FLARM_RFM95_RST_PIN, HIGH);

    SPI.begin(FLARM_RFM95_SCK_PIN, FLARM_RFM95_MISO_PIN, FLARM_RFM95_MOSI_PIN, FLARM_RFM95_CS_PIN);

    if (rf_chip == NULL)
    {
        rf_chip = &sx1276_ops;
        SX12XX_LL = &sx127x_ll_ops;

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

        const rf_proto_desc_t* p;
        p = &ogntp_proto_desc;

        RF_timing = p->tm_type;

        ts = &Time_Slots;
        ts->air_time = p->air_time;
        ts->interval_min = p->tx_interval_min;
        ts->interval_max = p->tx_interval_max;
        ts->interval_mid = (p->tx_interval_max + p->tx_interval_min) / 2;
        ts->s0.begin = p->slot0.begin;
        ts->s1.begin = p->slot1.begin;
        ts->s0.duration = p->slot0.end - p->slot0.begin;
        ts->s1.duration = p->slot1.end - p->slot1.begin;

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
 /*   tmElements_t  tm;
    (void)tm;
    time_t        Time = now();
    unsigned long pps_btime_ms = 0, ref_time_ms = 0;
    (void)pps_btime_ms;
    (void)ref_time_ms;*/

    tmElements_t  tm;
    time_t        Time;
    unsigned long pps_btime_ms, ref_time_ms;

    Time = now();
    RF_timing = RF_timing;// == RF_TIMING_2SLOTS_PPS_SYNC ? RF_TIMING_INTERVAL : RF_timing;
    unsigned long time_corr_neg;
    unsigned long ms_since_boot = millis();
    Time = makeTime(tm) /*+ (gnss.time.age() - time_corr_neg)*/ / 1000;

    //unsigned long time_corr_neg = 0;
    //unsigned long ms_since_boot = millis();

    (void)time_corr_neg;
    (void)ms_since_boot;
    const uint8_t protocol = (settings != nullptr) ? settings->rf_protocol : RF_PROTOCOL_OGNTP;
    uint8_t OGN = (protocol == RF_PROTOCOL_OGNTP ? 1 : 0);
    int8_t chan = -1;
    chan = (int8_t)RF_FreqPlan.getChannel(Time, 0, OGN);

    if (rf_chip)
    {
        rf_chip->channel(chan);
    }
}

void RF_loop()
{
    if (!rf_chip)
    {
       return;
    }

    RF_SetChannel();

    if (RF_Receive())
    {
        g_rxPacketPending = true;
        ++g_flarmPackets;
        g_lastPacket.length = RF_Payload_Size((settings != nullptr) ? settings->rf_protocol : RF_PROTOCOL_OGNTP);
        if (g_lastPacket.length > sizeof(g_lastPacket.data))
        {
            g_lastPacket.length = sizeof(g_lastPacket.data);
        }
        memcpy(g_lastPacket.data, RxBuffer, g_lastPacket.length);
        g_lastPacket.rssi = RF_last_rssi;
        g_lastPacket.snr = 0.0f;
        g_lastPacket.receivedAt = millis();
        g_lastPacket.valid = true;
    }
}

size_t RF_Encode(ufo_t* fop)
{
    size_t size = 0;

    if (protocol_encode)
    {
        if (millis() > TxTimeMarker)
        {
            size = (*protocol_encode)((void*)&TxBuffer[0], fop);
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
                        Serial.print(TxBuffer[i], HEX);
                        Serial.print(" ");
                    }

                    Serial.println("End TxBuffer");
                    Serial.print(F("$PSRFO,"));
                    Serial.print((unsigned long)timestamp);
                    Serial.print(F(","));
                    Serial.println(Bin2Hex((byte*)&TxBuffer[0], RF_Payload_Size(settings->rf_protocol)));
                }
                portENTER_CRITICAL(&g_packetCountersMux);
                tx_packets_counter++;
                portEXIT_CRITICAL(&g_packetCountersMux);
            }
            else
            {

                if (settings->nmea_p)
                {
                    Serial.println(F("$PSRFE,RF loopback is detected on Tx"));
                }
            }

            RF_tx_size = 0;

            Slot_descr_t* next;
            unsigned long adj;

            TxTimeMarker = millis();//!! +SoC->random(ts->interval_min, ts->interval_max) - ts->air_time;
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

uint8_t RF_Payload_Size(uint8_t protocol)
{
    switch (protocol)
    {
    case RF_PROTOCOL_OGNTP:     return ogntp_proto_desc.payload_size;
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

static void sx12xx_tx_func(osjob_t* job);
static void sx12xx_rx_func(osjob_t* job);
static void sx12xx_rx(osjobcb_t func);

static bool sx12xx_receive_complete = false;
bool sx12xx_receive_active = false;
static bool sx12xx_transmit_complete = false;

static int8_t sx12xx_channel_prev = (int8_t)-1;

#if defined(USE_BASICMAC)
void os_getDevEui(u1_t* buf) { }
u1_t os_getRegion(void) { return REGCODE_EU868; }
#else

#endif

#define SX1276_RegVersion          0x42 // common

static u1_t sx1276_readReg(u1_t addr)
{

#if defined(USE_BASICMAC)
    hal_spi_select(1);
#endif
    hal_spi(addr & 0x7F);
    u1_t val = hal_spi(0x00);
#if defined(USE_BASICMAC)
    hal_spi_select(0);
#endif
    return val;
}

static bool sx1276_probe()
{
    u1_t v, v_reset;

    lmic_hal_init(nullptr);

    // manually reset radio
    hal_pin_rst(0); // drive RST pin low
    hal_waitUntil(os_getTime() + ms2osticks(1)); // wait >100us

    v_reset = sx1276_readReg(SX1276_RegVersion);

    hal_pin_rst(2); // configure RST pin floating!
    hal_waitUntil(os_getTime() + ms2osticks(5)); // wait 5ms

    v = sx1276_readReg(SX1276_RegVersion);

    Serial.print("*** sx1276 v(0x13) = ");
    Serial.println(v, HEX);

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

static void sx1262_ReadRegs(uint16_t addr, uint8_t* data, uint8_t len)
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

static uint8_t sx1262_ReadReg(uint16_t addr)
{
    uint8_t val;
    sx1262_ReadRegs(addr, &val, 1);
    return val;
}


#endif

static void sx12xx_channel(int8_t channel)
{
    if (channel != -1 && channel != sx12xx_channel_prev)
    {
        uint32_t frequency = RF_FreqPlan.getChanFrequency((uint8_t)channel);
        int8_t fc = 0;

        if (sx12xx_receive_active)
        {
            os_radio(RADIO_RST);
            sx12xx_receive_active = false;
        }

        LMIC.freq = 868800000UL;
        sx12xx_channel_prev = channel;
    }
}

static void sx12xx_setup()
{
     // initialize runtime env
    os_init(nullptr);

    // Reset the MAC state. Session and pending data transfers will be discarded.
    LMIC_reset();

    switch (settings->rf_protocol)
    {
    case RF_PROTOCOL_OGNTP:
        LMIC.protocol = &ogntp_proto_desc;
        protocol_encode = &ogntp_encode;
        protocol_decode = &ogntp_decode;
        break;
    default:
        LMIC.protocol = &ogntp_proto_desc;
        protocol_encode = &ogntp_encode;
        protocol_decode = &ogntp_decode;
        /*
         * Enforce legacy protocol setting for SX1276
         * if other value (UAT) left in EEPROM from other (UATM) radio
         */
        settings->rf_protocol = RF_PROTOCOL_OGNTP;
        break;
    }

    RF_FreqPlan.setPlan(settings->band, settings->rf_protocol);

    /* Load regional max. EIRP at first */
    LMIC.txpow = RF_FreqPlan.MaxTxPower;

    if (LMIC.txpow > 20)
        LMIC.txpow = 20;
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
}

static bool sx12xx_receive()
{
    bool success = false;

    sx12xx_receive_complete = false;

    if (!sx12xx_receive_active)
    {
        sx12xx_setvars();
        sx12xx_rx(sx12xx_rx_func);
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


        for (u1_t i = 0; i < size; i++)
        {
            RxBuffer[i] = LMIC.frame[i + LMIC.protocol->payload_offset];
        }

        RF_last_rssi = LMIC.rssi;
        portENTER_CRITICAL(&g_packetCountersMux);
        rx_packets_counter++;
        portEXIT_CRITICAL(&g_packetCountersMux);
        success = true;
    }

    return success;
}

static void sx12xx_transmit()
{
    sx12xx_transmit_complete = false;
    sx12xx_receive_active = false;

    sx12xx_setvars();
    os_setCallback(&sx12xx_txjob, sx12xx_tx_func);

    unsigned long tx_timeout = LMIC.protocol ? (LMIC.protocol->air_time + 25) : 60;
    unsigned long tx_start = millis();

    while (sx12xx_transmit_complete == false)
    {
        if ((millis() - tx_start) > tx_timeout)
        {
            os_radio(RADIO_RST);
            break;
        }

        // execute scheduled jobs and events
        os_runstep();

        yield();
    };

}


// Enable rx mode and call func when a packet is received
static void sx12xx_rx(osjobcb_t func)
{
    LMIC.osjob.func = func;
    LMIC.rxtime = os_getTime(); // RX _now_
    // Enable "continuous" RX for LoRa only (e.g. without a timeout,
    // still stops after receiving a packet)
    os_radio(LMIC.protocol && LMIC.protocol->modulation_type == RF_MODULATION_TYPE_LORA ? RADIO_RXON : RADIO_RX);
}

static void sx12xx_rx_func(osjob_t* job)
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
        if (LDPC_Check((uint8_t*)&LMIC.frame[0])) {
#if DEBUG
            Serial.printf(" %02x%02x%02x%02x%02x%02x is wrong FEC",
                LMIC.frame[i], LMIC.frame[i + 1], LMIC.frame[i + 2],
                LMIC.frame[i + 3], LMIC.frame[i + 4], LMIC.frame[i + 5]);
#endif
            sx12xx_receive_complete = false;
        }
        else {
            sx12xx_receive_complete = true;
        }
        break;
    case RF_CHECKSUM_TYPE_CRC8_107:
        pkt_crc8 = LMIC.frame[i];
#if DEBUG
        if (crc8 == pkt_crc8) {
            Serial.printf(" %02x is valid crc", pkt_crc8);
        }
        else {
            Serial.printf(" %02x is wrong crc", pkt_crc8);
        }
#endif
        if (crc8 == pkt_crc8) {
            sx12xx_receive_complete = true;
        }
        else {
            sx12xx_receive_complete = false;
        }
        break;
    case RF_CHECKSUM_TYPE_CCITT_FFFF:
    case RF_CHECKSUM_TYPE_CCITT_0000:
    default:
        pkt_crc16 = (LMIC.frame[i] << 8 | LMIC.frame[i + 1]);
#if DEBUG
        if (crc16 == pkt_crc16) {
            Serial.printf(" %04x is valid crc", pkt_crc16);
        }
        else {
            Serial.printf(" %04x is wrong crc", pkt_crc16);
        }
#endif
        if (crc16 == pkt_crc16) {
            sx12xx_receive_complete = true;
        }
        else {
            sx12xx_receive_complete = false;
        }
        break;
    }
}

// Transmit the given string and call the given function afterwards
static void sx12xx_tx(unsigned char* buf, size_t size, osjobcb_t func) {

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

    for (u1_t i = 0; i < size; i++)
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
        LMIC.frame[LMIC.dataLen++] = (crc16 >> 8) & 0xFF;
        LMIC.frame[LMIC.dataLen++] = (crc16) & 0xFF;
        break;
    }

    LMIC.osjob.func = func;
    os_radio(RADIO_TX);
}

static void sx12xx_txdone_func(osjob_t* job) {
    sx12xx_transmit_complete = true;
}

static void sx12xx_tx_func(osjob_t* job) {

    if (RF_tx_size > 0) {
        sx12xx_tx((unsigned char*)&TxBuffer[0], RF_tx_size, sx12xx_txdone_func);
    }
}
#endif /* EXCLUDE_SX12XX */

//==============================================================================

void ParseData()
{
    if (settings == nullptr || !g_rxPacketPending)
    {
        return;
    }

    g_rxPacketPending = false;
    fo = EmptyFO;
    size_t rx_size = RF_Payload_Size(settings->rf_protocol);
    rx_size = rx_size > sizeof(fo.raw) ? sizeof(fo.raw) : rx_size;

    memset(fo.raw, 0, sizeof(fo.raw));
    memcpy(fo.raw, RxBuffer, rx_size);

    if (settings->nmea_p) 
    {
        Serial.print(F("$PSRFI,"));
        Serial.print((unsigned long)now()); Serial.print(F(","));
        Serial.print(Bin2Hex(fo.raw, rx_size)); Serial.print(F(","));
        Serial.println(RF_last_rssi);
    }

    //if (memcmp(RxBuffer, TxBuffer, rx_size) == 0)
    //{
    //    if (settings->nmea_p)
    //    {
    //        Serial.println(F("$PSRFE,RF loopback is detected on Rx"));
    //    }
    //    return;
    //}

    ufo_t selfAircraft = EmptyFO;
    fillProtocolSelfAircraft(&selfAircraft);

    if (protocol_decode && (*protocol_decode)((void*)RxBuffer, &selfAircraft, &fo))
    {
        //if (gnss.time.isValid())
        //{
        //    fo.hour_msg = (int)gnss.time.hour();
        //    fo.min_msg = (int)gnss.time.minute();
        //}
        //else
        //{
        fo.hour_msg = 10;
        fo.min_msg = 20;
        //}


        fo.rssi_LoRa = RF_last_rssi;
        fo.rssi = RF_last_rssi;
        fo.signal_source = TRAFFIC_SOURCE_FLARM_LORA;
        fo.timestamp = (time_t)(millis() / 1000UL);
        fo.timemsg = fo.timestamp;

        if (settings->serial_out == OUTPUT_MODE_FLARM)
        {
            printDecodedLoRaPacketToSerial(&fo, RF_last_rssi, rx_size);
        }

        Traffic_Update(&fo);
        Traffic_Add(&fo);
    }
}

