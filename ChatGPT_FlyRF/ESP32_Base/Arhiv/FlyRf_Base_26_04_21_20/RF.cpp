/*
  Модуль RF.cpp
  Назначение:
  - Основная радиочасть проекта: прием, передача и разбор LoRa/FLARM пакетов.

  Основные задачи модуля:
  - Настраивать радиочип SX1276 и рабочий протокол.
  - Принимать пакеты, декодировать их и передавать данные в Container.
  - Формировать и передавать пакеты нашего самолета.
  - Вести счетчики пакетов, контролировать частоту и диагностировать радиоканал.
*/

#include <Arduino.h>
#include "RF.h"
#include "FlarmDecoder.h"
#include "Log.h"
#include "EEPROMRF.h"
#include "NMEA.h"
#include "Mavlink.h"
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
static size_t g_lastRxSize = 0;
static uint32_t g_lastRfPacketMs = 0;
static uint32_t g_lastRfRefreshMs = 0;

static const uint32_t kLoRaFixedFreqHz[] = {
    868000000UL, 868100000UL, 868200000UL, 868300000UL, 868400000UL,
    868500000UL, 868600000UL, 868700000UL, 868800000UL, 868900000UL
};
static const uint32_t kRfRxRefreshMs = 120000UL;
static const uint32_t kRfNoPacketRestartMs = 900000UL;

static uint8_t g_lastAppliedLoRaProfile = 0xFF;
static uint8_t g_lastAppliedLoRaFixedMode = 0xFF;
static uint8_t g_lastAppliedLoRaFixedFreq = 0xFF;
static uint8_t g_lastAppliedRfProtocol = 0xFF;
static bool g_rfRuntimeReconfigurePending = false;
static const int8_t kLoRaMaxTxPowerDbm = 20; // Максимальная мощность передачи LoRa для SX127x

static void RF_ApplyMaxLoRaTxPower()
{
    LMIC.txpow = kLoRaMaxTxPowerDbm;
}

static uint8_t getLoRaProfile()
{
    return settings ? settings->lora_profile : 0;
}

static uint8_t getLoRaFixedFreqIndex()
{
    uint8_t idx = settings ? settings->lora_fixed_freq : 0;
    if (idx >= (sizeof(kLoRaFixedFreqHz) / sizeof(kLoRaFixedFreqHz[0]))) idx = 0;
    return idx;
}

const rfchip_ops_t* rf_chip = NULL;
bool RF_SX12XX_RST_is_connected = true;

size_t(*protocol_encode)(void*, ufo_t*);
bool   (*protocol_decode)(void*, ufo_t*, ufo_t*);

static Slots_descr_t Time_Slots, * ts;
static uint8_t       RF_timing = RF_TIMING_INTERVAL;


static bool sx1276_probe(void);
static u1_t sx1276_readReg(u1_t addr);
static void sx12xx_setup(void);
static void sx12xx_setvars(void);
static void sx12xx_channel(int8_t);
static bool sx12xx_receive(void);
static void sx12xx_transmit(void);
static void fillProtocolSelfAircraft(ufo_t *dst);
static void RF_ServiceReceiveWatchdog(void);
extern bool sx12xx_receive_active;
extern int8_t sx12xx_channel_prev;

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

bool RF_GetLastRawPacket(FlarmRawPacket &outPacket)
{
    if (!g_lastPacket.valid)
    {
        outPacket = {};
        return false;
    }

    outPacket = g_lastPacket;
    return true;
}

uint32_t RF_GetCurrentFrequencyHz(void)
{
    return (uint32_t)LMIC.freq;
}

uint32_t RF_GetSelectedFrequencyHz(void)
{
    return kLoRaFixedFreqHz[getLoRaFixedFreqIndex()];
}

static uint16_t rfBandwidthToKHz(uint8_t bw)
{
    switch (bw)
    {
        case BW125: return 125;
        case BW250: return 250;
        case BW500: return 500;
        default:    return 0;
    }
}

static uint8_t rfSpreadingFactorToValue(uint8_t sf)
{
    if (sf >= SF7 && sf <= SF12)
    {
        return (uint8_t)(sf + 6);
    }
    return 0;
}

static uint8_t rfCodingRateToDenom(uint8_t cr)
{
    switch (cr)
    {
        case CR_4_5: return 5;
        case CR_4_6: return 6;
        case CR_4_7: return 7;
        case CR_4_8: return 8;
        default:     return 0;
    }
}

static String rfFormatFrequencyMHz(uint32_t hz)
{
    char buf[24];
    snprintf(buf, sizeof(buf), "%.3f MHz", (double)hz / 1000000.0);
    return String(buf);
}

static const char* rfProfileNameFromRuntime(const RF_LoraRuntimeInfo &info)
{
    if (!info.valid)
    {
        return "недоступно";
    }

    if (info.bandwidthKHz == 125 && info.spreadingFactor == 9 && info.codingRateDenom == 6)
    {
        return "Long range";
    }
    if (info.bandwidthKHz == 125 && info.spreadingFactor == 12 && info.codingRateDenom == 8)
    {
        return "Max range";
    }
    if (info.bandwidthKHz == 125 && info.spreadingFactor == 8 && info.codingRateDenom == 6)
    {
        return "Fast robust";
    }
    return "OGN compatible / custom";
}

bool RF_GetLoraRuntimeInfo(RF_LoraRuntimeInfo &info)
{
    memset(&info, 0, sizeof(info));
    info.txPowerDbm = -127;

    if (!rf_chip)
    {
        return false;
    }

    info.valid = true;
    info.frequencyHz = (uint32_t)LMIC.freq;
    info.spreadingFactor = rfSpreadingFactorToValue(getSf(LMIC.rps));
    info.bandwidthKHz = rfBandwidthToKHz(getBw(LMIC.rps));
    info.codingRateDenom = rfCodingRateToDenom(getCr(LMIC.rps));
    info.syncWord = LMIC.syncword;
    info.rxSymbols = LMIC.rxsyms;
    info.txPowerDbm = (int8_t)LMIC.txpow;
    info.lowDataRateOptimize = enDro(LMIC.rps) != 0;
    info.crcOnPayload = getNocrc(LMIC.rps) == 0;
    info.implicitHeader = getIh(LMIC.rps) != 0;
    info.fromRegisters = false;

    if (!g_rfRuntimeReconfigurePending && rf_chip->type == RF_IC_SX1276)
    {
        const uint8_t regFrMsb = sx1276_readReg(0x06);
        const uint8_t regFrMid = sx1276_readReg(0x07);
        const uint8_t regFrLsb = sx1276_readReg(0x08);
        const uint8_t regModem1 = sx1276_readReg(0x1D);
        const uint8_t regModem2 = sx1276_readReg(0x1E);
        const uint8_t regModem3 = sx1276_readReg(0x26);
        const uint8_t regSyncWord = sx1276_readReg(0x39);
        const uint8_t regPaConfig = sx1276_readReg(0x09);

        const uint32_t frf = ((uint32_t)regFrMsb << 16) | ((uint32_t)regFrMid << 8) | (uint32_t)regFrLsb;
        const uint64_t freqHz = ((uint64_t)frf * 32000000ULL) / 524288ULL;
        const uint8_t bwCode = (uint8_t)((regModem1 >> 4) & 0x0F);
        const uint8_t crCode = (uint8_t)((regModem1 >> 1) & 0x07);
        const uint8_t sfCode = (uint8_t)((regModem2 >> 4) & 0x0F);

        info.fromRegisters = true;
        info.frequencyHz = (uint32_t)freqHz;
        info.syncWord = regSyncWord;
        info.lowDataRateOptimize = (regModem3 & 0x08) != 0;
        info.crcOnPayload = (regModem2 & 0x04) != 0;
        info.implicitHeader = (regModem1 & 0x01) != 0;

        switch (bwCode)
        {
            case 7: info.bandwidthKHz = 125; break;
            case 8: info.bandwidthKHz = 250; break;
            case 9: info.bandwidthKHz = 500; break;
            default: break;
        }

        if (sfCode >= 6 && sfCode <= 12)
        {
            info.spreadingFactor = sfCode;
        }

        if (crCode >= 1 && crCode <= 4)
        {
            info.codingRateDenom = (uint8_t)(crCode + 4);
        }

        const uint8_t paSelect = (regPaConfig >> 7) & 0x01;
        const uint8_t outputPower = regPaConfig & 0x0F;
        info.txPowerDbm = paSelect ? (int8_t)(2 + outputPower) : (int8_t)(outputPower - 1);
    }

    return info.valid;
}

String RF_GetLoraProfileDetailsText(void)
{
    RF_LoraRuntimeInfo info = {};
    if (!RF_GetLoraRuntimeInfo(info) || !info.valid)
    {
        return String("недоступно");
    }

    char buf[128];
    snprintf(buf, sizeof(buf),
             "%s, %s, SF%u BW%u CR4/%u, Sync 0x%02X, RX syms %u, TX %d dBm",
             rfProfileNameFromRuntime(info),
             rfFormatFrequencyMHz(info.frequencyHz).c_str(),
             (unsigned)info.spreadingFactor,
             (unsigned)info.bandwidthKHz,
             (unsigned)info.codingRateDenom,
             (unsigned)info.syncWord,
             (unsigned)info.rxSymbols,
             (int)info.txPowerDbm);
    return String(buf);
}

String RF_GetLoraRegistersSourceText(void)
{
    RF_LoraRuntimeInfo info = {};
    if (!RF_GetLoraRuntimeInfo(info) || !info.valid)
    {
        return String("LMIC runtime");
    }
    return String(info.fromRegisters ? "регистры LoRa" : "LMIC runtime");
}

const char* RF_GetProfileName(uint8_t profile)
{
    switch (profile)
    {
        case 1: return "Long range";
        case 2: return "Max range";
        case 3: return "Fast robust";
        default: return "OGN compatible";
    }
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
    dst->old_latitude = ThisAircraft.old_latitude;
    dst->old_longitude = ThisAircraft.old_longitude;
    dst->local_latitude = ThisAircraft.local_latitude;
    dst->local_longitude = ThisAircraft.local_longitude;
    dst->hdop = ThisAircraft.hdop;
    dst->aircraft_type = ThisAircraft.aircraft_type;
    dst->geoid_separation = ThisAircraft.geoid_separation;
    dst->timestamp = (ThisAircraft.timestamp > 0) ? ThisAircraft.timestamp : (time_t)(millis() / 1000UL);
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
        SX12XX_LL = &sx127x_ll_ops;
        rf_chip = &sx1276_ops;

        if (rf_chip && rf_chip->probe && rf_chip->probe())
        {
            Serial.print(rf_chip->name);
            Serial.println(F(" LoRa is detected."));
        }
        else
        {
            rf_chip = NULL;
            Serial.println(F("WARNING! None of supported RFICs is detected!"));
        }
    }

    if (rf_chip)
    {
        g_lastRfPacketMs = millis();
        g_lastRfRefreshMs = g_lastRfPacketMs;
        rf_chip->setup();

        const rf_proto_desc_t* p;
        p = LMIC.protocol ? LMIC.protocol : &ogntp_proto_desc;

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

static void RF_ApplySelectedProtocol()
{
    if (settings == nullptr)
    {
        return;
    }

    switch (settings->rf_protocol)
    {
    case RF_PROTOCOL_MAVLINK:
        /*
         * MAVLink использует собственный физический RF-профиль LoRa,
         * без подмены на профиль OGNTP.
         */
        LMIC.protocol = &mavlink_lora_proto_desc;
        protocol_encode = &mavlink_lora_encode;
        protocol_decode = &mavlink_lora_decode;
        break;
    case RF_PROTOCOL_OGNTP:
    default:
        LMIC.protocol = &ogntp_proto_desc;
        protocol_encode = &ogntp_encode;
        protocol_decode = &ogntp_decode;
        settings->rf_protocol = RF_PROTOCOL_OGNTP;
        break;
    }

    /* Для каждого протокола выбираем его собственный частотный план. */
    RF_FreqPlan.setPlan(settings->band, settings->rf_protocol);

    const rf_proto_desc_t* p = LMIC.protocol ? LMIC.protocol : &ogntp_proto_desc;
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
    const uint16_t duration = ts->s0.duration + ts->s1.duration;
    ts->adj = duration > ts->interval_mid ? 0 : (ts->interval_mid - duration) / 2;
}

void RF_NotifySettingsChanged(void)
{
    g_rfRuntimeReconfigurePending = true;

    if (!rf_chip || !settings)
    {
        return;
    }

    RF_ApplySelectedProtocol();
    sx12xx_setvars();
    RF_ApplyMaxLoRaTxPower();
    g_lastAppliedRfProtocol = settings->rf_protocol;
    if (settings->lora_fixed_channel)
    {
        LMIC.freq = RF_GetSelectedFrequencyHz();
    }
    os_radio(RADIO_RST);
    sx12xx_receive_active = false;
    sx12xx_channel_prev = (int8_t)-1;
    g_lastRfRefreshMs = millis();
}

static void RF_ServiceRuntimeSettingsChange()
{
    if (!rf_chip || !settings)
    {
        return;
    }

    const uint8_t profile = getLoRaProfile();
    const uint8_t fixedMode = settings->lora_fixed_channel;
    const uint8_t fixedFreq = getLoRaFixedFreqIndex();
    const uint8_t rfProtocol = settings->rf_protocol;

    const bool changed = g_rfRuntimeReconfigurePending ||
                         g_lastAppliedLoRaProfile != profile ||
                         g_lastAppliedLoRaFixedMode != fixedMode ||
                         g_lastAppliedLoRaFixedFreq != fixedFreq ||
                         g_lastAppliedRfProtocol != rfProtocol;

    if (!changed)
    {
        return;
    }

    g_lastAppliedLoRaProfile = profile;
    g_lastAppliedLoRaFixedMode = fixedMode;
    g_lastAppliedLoRaFixedFreq = fixedFreq;
    g_lastAppliedRfProtocol = rfProtocol;
    g_rfRuntimeReconfigurePending = false;

    RF_ApplySelectedProtocol();
    sx12xx_setvars();
    RF_ApplyMaxLoRaTxPower();
    os_radio(RADIO_RST);
    sx12xx_receive_active = false;
    sx12xx_channel_prev = (int8_t)-1;
    g_lastRfRefreshMs = millis();
}

void RF_SetChannel(void)
{
    if (!rf_chip)
    {
        return;
    }

    if (settings && settings->lora_fixed_channel)
    {
        const uint32_t target = RF_GetSelectedFrequencyHz();
        if ((uint32_t)LMIC.freq != target)
        {
            os_radio(RADIO_RST);
            LMIC.freq = target;
        }
        return;
    }

    time_t currentTime = now();
    if (currentTime <= 0)
    {
        currentTime = (time_t)(millis() / 1000UL);
    }

    const uint8_t protocol = (settings != nullptr) ? settings->rf_protocol : RF_PROTOCOL_OGNTP;
    const uint8_t OGN = (protocol == RF_PROTOCOL_OGNTP ? 1 : 0);
    const int8_t chan = (int8_t)RF_FreqPlan.getChannel(currentTime, 0, OGN);
    rf_chip->channel(chan);
}

void RF_loop()
{
    if (!rf_chip)
    {
       return;
    }

    RF_ServiceReceiveWatchdog();
    RF_ServiceRuntimeSettingsChange();
    RF_SetChannel();

    if (RF_Receive())
    {
        g_rxPacketPending = true;
        ++g_flarmPackets;
        g_lastPacket.length = g_lastRxSize ? g_lastRxSize : RF_Payload_Size((settings != nullptr) ? settings->rf_protocol : RF_PROTOCOL_OGNTP);
        if (g_lastPacket.length > sizeof(g_lastPacket.data))
        {
            g_lastPacket.length = sizeof(g_lastPacket.data);
        }
        memcpy(g_lastPacket.data, RxBuffer, g_lastPacket.length);
        g_lastPacket.rssi = RF_last_rssi;
        g_lastPacket.snr = 0.0f;
        g_lastPacket.receivedAt = millis();
        g_lastPacket.valid = true;
        g_lastRfPacketMs = millis();
        g_lastRfRefreshMs = g_lastRfPacketMs;
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

bool RF_TransmitThisAircraft(bool wait)
{
    ufo_t selfAircraft = EmptyFO;
    fillProtocolSelfAircraft(&selfAircraft);
    return RF_Transmit(RF_Encode(&selfAircraft), wait);
}

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
                    Serial.println(Bin2Hex((byte*)&TxBuffer[0], RF_tx_size));
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
    case RF_PROTOCOL_MAVLINK:   return mavlink_lora_proto_desc.payload_size;
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

int8_t sx12xx_channel_prev = (int8_t)-1;

static void RF_ServiceReceiveWatchdog()
{
    if (!rf_chip)
    {
        return;
    }

    const uint32_t nowMs = millis();

    if (sx12xx_receive_active && (uint32_t)(nowMs - g_lastRfRefreshMs) >= kRfRxRefreshMs)
    {
        os_radio(RADIO_RST);
        sx12xx_receive_active = false;
        g_lastRfRefreshMs = nowMs;
    }

    if ((uint32_t)(nowMs - g_lastRfPacketMs) >= kRfNoPacketRestartMs)
    {
        os_radio(RADIO_RST);
        sx12xx_receive_active = false;
        g_lastRfPacketMs = nowMs;
        g_lastRfRefreshMs = nowMs;
    }
}


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

    RF_ApplySelectedProtocol();

    /* Во всех режимах используем максимальную мощность передачи LoRa */
    RF_ApplyMaxLoRaTxPower();

    g_lastAppliedLoRaProfile = getLoRaProfile();
    g_lastAppliedLoRaFixedMode = settings ? settings->lora_fixed_channel : 0;
    g_lastAppliedLoRaFixedFreq = getLoRaFixedFreqIndex();
    g_lastAppliedRfProtocol = settings ? settings->rf_protocol : RF_PROTOCOL_OGNTP;
    g_rfRuntimeReconfigurePending = false;
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

    switch (getLoRaProfile())
    {
    case 1:
        LMIC.rps = makeRps(SF9, BW125, CR_4_6, 0, 0);
        LMIC.rxsyms = 160;
        break;
    case 2:
        LMIC.rps = makeRps(SF12, BW125, CR_4_8, 0, 0);
        LMIC.rxsyms = 255;
        break;
    case 3:
        LMIC.rps = makeRps(SF8, BW125, CR_4_6, 0, 0);
        LMIC.rxsyms = 120;
        break;
    default:
        break;
    }

    RF_ApplyMaxLoRaTxPower();
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

        g_lastRxSize = size;
        RF_last_rssi = LMIC.rssi;
        portENTER_CRITICAL(&g_packetCountersMux);
        rx_packets_counter++;
        portEXIT_CRITICAL(&g_packetCountersMux);
        success = true;
        g_lastRfRefreshMs = millis();
    }

    return success;
}

static void sx12xx_transmit()
{
    sx12xx_transmit_complete = false;
    sx12xx_receive_active = false;

    sx12xx_setvars();
    os_setCallback(&sx12xx_txjob, sx12xx_tx_func);

    unsigned long tx_timeout = 60;
    if (LMIC.protocol)
    {
        tx_timeout = (unsigned long)LMIC.protocol->air_time + 25UL;
    }
    if (RF_tx_size > 0)
    {
        const ostime_t airTimeTicks = LMIC_calcAirTime(LMIC.rps, (u1_t)RF_tx_size);
        const unsigned long airTimeMs = (unsigned long)osticks2ms(airTimeTicks);
        if (airTimeMs > 0)
        {
            tx_timeout = airTimeMs + 80UL;
        }
    }
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
    const uint8_t protocol = settings ? settings->rf_protocol : RF_PROTOCOL_OGNTP;
    size_t rx_size = g_lastRxSize ? g_lastRxSize : RF_Payload_Size(protocol);
    const size_t raw_copy = rx_size > sizeof(fo.raw) ? sizeof(fo.raw) : rx_size;

    memset(fo.raw, 0, sizeof(fo.raw));
    memcpy(fo.raw, RxBuffer, raw_copy);

    if (settings->nmea_p) 
    {
        Serial.print(F("$PSRFI,"));
        Serial.print((unsigned long)now()); Serial.print(F(","));
        Serial.print(Bin2Hex((byte*)RxBuffer, rx_size)); Serial.print(F(","));
        Serial.println(RF_last_rssi);
    }

    if (settings != nullptr && settings->serial_out == OUTPUT_MODE_LORA_RAW)
    {
        Serial.print(F("LORA_RX_RAW,"));
        Serial.print((unsigned)rx_size);
        Serial.print(F(",RSSI="));
        Serial.print(RF_last_rssi);
        Serial.print(F(",HEX="));
        Serial.println(Bin2Hex((byte*)RxBuffer, rx_size));
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

        if (fo.addr == ThisAircraft.addr)
        {
            return;
        }

        Traffic_Update(&fo);
        Traffic_Add(&fo);
    }
}

