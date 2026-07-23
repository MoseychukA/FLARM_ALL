#include "RS485Display.h"
#include "DeviceInfo.h"
#include "HardwareConfig.h"
#include "DisplayRemote.h"
#include <string.h>

static full_packet_net_t g_lastPacket = {};
static aux_t g_inAux = {};
static aux_t g_outAux = {};
static volatile uint8_t g_inButton = 0;
static volatile uint8_t g_localButton = 0;
static uint32_t g_lastRx = 0;
static uint32_t g_rxPackets = 0;
static uint32_t g_txPackets = 0;
static uint32_t g_rxBytes = 0;
static uint32_t g_crcErrors = 0;
static uint32_t g_frameErrors = 0;
static uint32_t g_lastDiagnosticMs = 0;
static uint32_t g_lastDiagnosticBytes = 0;
static bool g_baseConnected = false;
static bool g_peerUsesTime64 = false;
static size_t g_lastPayloadBytes = 0;

typedef struct __attribute__((packed)) {
    uint32_t addr;
    int      squawk;
    uint8_t  callsign[8];
    float    altitude;
    float    pressure_altitude;
    float    course;
    float    speed;
    float    distance;
    float    bearing;
    int      vert_rate;
    float    latitude;
    float    longitude;
    int64_t  timestamp;
    int8_t   rssi_LoRa;
    int8_t   rssi_rp2040;
    uint8_t  signal_source;
} ufo_net_time64_t;

typedef struct __attribute__((packed)) {
    ufo_net_time64_t ThisAircraft;
    ufo_net_time64_t Container[MAX_TRACKING_OBJECTS];
    aux_t            AuxData;
    uint8_t          BUTTON1;
    uint8_t          BUTTON2;
} full_packet_net_time64_t;

static uint16_t crc16_ccitt(const uint8_t* data, size_t len)
{
    uint16_t crc = 0x0000;
    for (size_t i = 0; i < len; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; ++j) crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
    }
    return crc;
}

static void rs485SetTX(bool enable)
{
    digitalWrite(RS485_DE_PIN, enable ? HIGH : LOW);
    if (enable) delayMicroseconds(50);
}

static void time64ToNative(const ufo_net_time64_t& src, ufo_net_t& dst)
{
    dst.addr = src.addr;
    dst.squawk = src.squawk;
    memcpy(dst.callsign, src.callsign, sizeof(dst.callsign));
    dst.altitude = src.altitude;
    dst.pressure_altitude = src.pressure_altitude;
    dst.course = src.course;
    dst.speed = src.speed;
    dst.distance = src.distance;
    dst.bearing = src.bearing;
    dst.vert_rate = src.vert_rate;
    dst.latitude = src.latitude;
    dst.longitude = src.longitude;
    dst.timestamp = (time_t)src.timestamp;
    dst.rssi_LoRa = src.rssi_LoRa;
    dst.rssi_rp2040 = src.rssi_rp2040;
    dst.signal_source = src.signal_source;
}

static void packetTime64ToNative(const full_packet_net_time64_t& src, full_packet_net_t& dst)
{
    memset(&dst, 0, sizeof(dst));
    time64ToNative(src.ThisAircraft, dst.ThisAircraft);
    for (int index = 0; index < MAX_TRACKING_OBJECTS; ++index) {
        time64ToNative(src.Container[index], dst.Container[index]);
    }
    dst.AuxData = src.AuxData;
    dst.BUTTON1 = src.BUTTON1;
    dst.BUTTON2 = src.BUTTON2;
}

static void netToLocalThis(const ufo_net_t& src)
{
    ThisAircraft.addr = src.addr;
    ThisAircraft.squawk = src.squawk;
    memcpy(ThisAircraft.callsign, src.callsign, sizeof(ThisAircraft.callsign));
    ThisAircraft.altitude = src.altitude;
    ThisAircraft.pressure_altitude = src.pressure_altitude;
    ThisAircraft.course = src.course;
    ThisAircraft.speed = src.speed;
    ThisAircraft.vert_rate = src.vert_rate;
    ThisAircraft.latitude = src.latitude;
    ThisAircraft.longitude = src.longitude;
    ThisAircraft.local_latitude = src.latitude;
    ThisAircraft.local_longitude = src.longitude;
    ThisAircraft.timestamp = src.timestamp;
}

static void netToLocalContainer(const ufo_net_t& src, ufo_t& dst)
{
    dst.addr = src.addr;
    dst.squawk = src.squawk;
    memcpy(dst.callsign, src.callsign, sizeof(dst.callsign));
    dst.altitude = src.altitude;
    dst.pressure_altitude = src.pressure_altitude;
    dst.course = src.course;
    dst.speed = src.speed;
    dst.distance = src.distance;
    dst.bearing = src.bearing;
    dst.vert_rate = src.vert_rate;
    dst.latitude = src.latitude;
    dst.longitude = src.longitude;
    dst.timestamp = src.timestamp;
    dst.lastUpdate = millis();
    dst.rssi_LoRa = src.rssi_LoRa;
    dst.rssi_rp2040 = src.rssi_rp2040;
    dst.signal_source = src.signal_source;
    dst.valid = (src.addr != 0);
}

void net_to_ufo_Container(const ufo_t* src, ufo_net_t* dst)
{
    if (!src || !dst) return;
    memset(dst, 0, sizeof(*dst));
    dst->addr = src->addr;
    dst->squawk = src->squawk;
    memcpy(dst->callsign, src->callsign, sizeof(dst->callsign));
    dst->altitude = src->altitude;
    dst->pressure_altitude = src->pressure_altitude;
    dst->course = src->course;
    dst->speed = src->speed;
    dst->distance = src->distance;
    dst->bearing = src->bearing;
    dst->vert_rate = src->vert_rate;
    dst->latitude = src->latitude;
    dst->longitude = src->longitude;
    dst->timestamp = src->timestamp;
    dst->rssi_LoRa = src->rssi_LoRa;
    dst->rssi_rp2040 = src->rssi_rp2040;
    dst->signal_source = src->signal_source;
}

void net_to_ufo_ThisAircraft(const ufo_t* src, ufo_net_t* dst) { net_to_ufo_Container(src, dst); }

bool receivePacket_RS485(full_packet_net_t* pkt, uint8_t* btn1, uint8_t* btn2)
{
    static uint8_t buffer[(sizeof(full_packet_net_time64_t) + 10) * 2 + 16];
    static full_packet_net_time64_t packetTime64;
    static size_t idx = 0;
    const size_t nativePayloadLen = sizeof(full_packet_net_t);
    const size_t time64PayloadLen = sizeof(full_packet_net_time64_t);
    const size_t nativeFrameLen = nativePayloadLen + 10;
    const size_t time64FrameLen = time64PayloadLen + 10;
    const size_t minimumFrameLen = nativeFrameLen < time64FrameLen ? nativeFrameLen : time64FrameLen;
    const size_t maximumFrameLen = nativeFrameLen > time64FrameLen ? nativeFrameLen : time64FrameLen;

    while (RS485_SERIAL.available() && idx < sizeof(buffer)) {
        const int value = RS485_SERIAL.read();
        if (value < 0) break;
        buffer[idx++] = (uint8_t)value;
        ++g_rxBytes;
    }

    while (idx >= 4) {
        size_t start = 0;
        while (idx - start >= 4) {
            if (buffer[start] == 0xDD && buffer[start + 1] == 0xCC &&
                buffer[start + 2] == 0xBB && buffer[start + 3] == 0xAA) {
                break;
            }
            ++start;
        }

        if (start > 0) {
            memmove(buffer, buffer + start, idx - start);
            idx -= start;
        }
        if (idx < minimumFrameLen) return false;

        size_t acceptedPayloadLen = 0;
        bool candidateFooterFound = false;
        const size_t candidatePayloadLen[2] = { nativePayloadLen, time64PayloadLen };
        for (uint8_t candidate = 0; candidate < 2; ++candidate) {
            const size_t payloadLen = candidatePayloadLen[candidate];
            const size_t frameLen = payloadLen + 10;
            if (idx < frameLen) continue;

            const size_t crcOff = 4 + payloadLen;
            const size_t footerOff = crcOff + 2;
            const bool footerOk = buffer[footerOff] == 0xAA && buffer[footerOff + 1] == 0xBB &&
                                  buffer[footerOff + 2] == 0xCC && buffer[footerOff + 3] == 0xDD;
            if (!footerOk) continue;
            candidateFooterFound = true;

            uint16_t crcRx = 0;
            memcpy(&crcRx, &buffer[crcOff], sizeof(crcRx));
            const uint16_t crcCalc = crc16_ccitt(&buffer[4], payloadLen);
            if (crcRx == crcCalc) {
                acceptedPayloadLen = payloadLen;
                break;
            }
        }

        if (acceptedPayloadLen == 0) {
            if (idx < maximumFrameLen) return false;
            if (candidateFooterFound) ++g_crcErrors;
            else ++g_frameErrors;
            memmove(buffer, buffer + 1, --idx);
            continue;
        }

        if (acceptedPayloadLen == nativePayloadLen) {
            memcpy(pkt, &buffer[4], nativePayloadLen);
            g_peerUsesTime64 = false;
        } else {
            memcpy(&packetTime64, &buffer[4], time64PayloadLen);
            packetTime64ToNative(packetTime64, *pkt);
            g_peerUsesTime64 = true;
        }
        g_lastPayloadBytes = acceptedPayloadLen;
        if (btn1) *btn1 = pkt->BUTTON1;
        if (btn2) *btn2 = pkt->BUTTON2;
        const size_t acceptedFrameLen = acceptedPayloadLen + 10;
        idx -= acceptedFrameLen;
        if (idx > 0) memmove(buffer, buffer + acceptedFrameLen, idx);
        return true;
    }

    return false;
}

static void sendReply()
{
    static full_packet_net_t reply = {};
    static full_packet_net_time64_t replyTime64 = {};
    const uint8_t buttonEvent = g_localButton;
    g_localButton = 0;

    const uint8_t* payload = nullptr;
    size_t payloadLen = 0;
    if (g_peerUsesTime64) {
        memset(&replyTime64, 0, sizeof(replyTime64));
        replyTime64.AuxData = g_outAux;
        replyTime64.AuxData.new_button_M = buttonEvent;
        payload = (const uint8_t*)&replyTime64;
        payloadLen = sizeof(replyTime64);
    } else {
        memset(&reply, 0, sizeof(reply));
        reply.AuxData = g_outAux;
        reply.AuxData.new_button_M = buttonEvent;
        payload = (const uint8_t*)&reply;
        payloadLen = sizeof(reply);
    }

    const uint16_t crc = crc16_ccitt(payload, payloadLen);
    rs485SetTX(true);
    RS485_SERIAL.write((const uint8_t*)&PACKET_HEADER, sizeof(PACKET_HEADER));
    RS485_SERIAL.write(payload, payloadLen);
    RS485_SERIAL.write((const uint8_t*)&crc, sizeof(crc));
    RS485_SERIAL.write((const uint8_t*)&PACKET_FOOTER, sizeof(PACKET_FOOTER));
    RS485_SERIAL.flush();
    ++g_txPackets;
    delayMicroseconds(200);
    rs485SetTX(false);
}

static void applyPacket(const full_packet_net_t& p)
{
    const bool linkWasConnected = g_baseConnected;
    g_lastPacket = p;
    g_inAux = p.AuxData;
    netToLocalThis(p.ThisAircraft);
    for (int i = 0; i < MAX_TRACKING_OBJECTS; ++i) netToLocalContainer(p.Container[i], Container[i]);
    if (p.AuxData.new_button_M != 0) g_inButton = p.AuxData.new_button_M;
    g_lastRx = millis();
    g_baseConnected = true;
    ++g_rxPackets;
    if (!linkWasConnected) {
        uint8_t targetCount = 0;
        for (int index = 0; index < MAX_TRACKING_OBJECTS; ++index) {
            if (p.Container[index].addr != 0U) ++targetCount;
        }
        Serial.print(F("[RS485] Base connected. Payload bytes: "));
        Serial.print(g_lastPayloadBytes);
        Serial.print(F(" format="));
        Serial.print(g_peerUsesTime64 ? F("time64") : F("time32"));
        Serial.print(F(" targets="));
        Serial.print(targetCount);
        Serial.print(F(", frame bytes: "));
        Serial.println(g_lastPayloadBytes + 10U);
    }
    Remote_setGnssState(p.AuxData.isValidGNSS_M, p.AuxData.gps_time_valid_M, p.AuxData.gps_satellites_valid_M,
                        p.AuxData.Time_Hour_M, p.AuxData.Time_Minute_M,
                        ThisAircraft.latitude, ThisAircraft.longitude, ThisAircraft.altitude, ThisAircraft.hdop);
    Remote_setBaseStatus(p.AuxData.base_test_mode_M, p.AuxData.gps_rx_M, p.AuxData.gps_satellites_M,
                         p.AuxData.display_coord_valid_M, p.AuxData.display_coord_is_local_M,
                         p.AuxData.display_latitude_M, p.AuxData.display_longitude_M,
                         p.AuxData.lora_tx_packets_M, p.AuxData.lora_rx_packets_M, p.AuxData.lora_rf_hz_M);
    Remote_setLanStatus(p.AuxData.lan_state_view_M, p.AuxData.lan_ready_M, p.AuxData.lan_link_up_M,
                        p.AuxData.lan_udp_working_M, p.AuxData.lan_dhcp_M, p.AuxData.lan_ip_M,
                        p.AuxData.lan_udp_port_M, p.AuxData.lan_tx_packets_M, p.AuxData.lan_rx_packets_M,
                        p.AuxData.lan_udp_tx_packets_M, p.AuxData.lan_udp_rx_packets_M);
    Remote_setTrackerMessage(p.AuxData.new_message || p.AuxData.message_received, p.AuxData.msg_resp_M,
                             p.AuxData.Time_Hour_M, p.AuxData.Time_Minute_M);
}

void RS485Display_setup()
{
    g_lastRx = 0U;
    g_baseConnected = false;
    g_peerUsesTime64 = false;
    g_lastPayloadBytes = 0U;
    pinMode(RS485_DE_PIN, OUTPUT);
    rs485SetTX(false);
    RS485_SERIAL.setRxBufferSize(2048);
    RS485_SERIAL.setTxBufferSize(2048);
    RS485_SERIAL.begin(RS485_BAUD, RS485_CONFIG, RS485_RX_PIN, RS485_TX_PIN);
    Serial.print(F("[RS485] Serial1 RX=GPIO"));
    Serial.print(RS485_RX_PIN);
    Serial.print(F(" TX=GPIO"));
    Serial.print(RS485_TX_PIN);
    Serial.print(F(" DE=GPIO"));
    Serial.print(RS485_DE_PIN);
    Serial.print(F(" baud="));
    Serial.print(RS485_BAUD);
    Serial.print(F(" frames="));
    Serial.print(sizeof(full_packet_net_t) + 10U);
    Serial.print(F("/"));
    Serial.println(sizeof(full_packet_net_time64_t) + 10U);
}

void RS485Display_loop()
{
    for (uint8_t processed = 0; processed < 4; ++processed) {
        full_packet_net_t pkt;
        uint8_t b1 = 0;
        uint8_t b2 = 0;
        if (!receivePacket_RS485(&pkt, &b1, &b2)) break;
        applyPacket(pkt);
        if (g_localButton != 0 || g_outAux.confirm_message_M) sendReply();
    }

    const uint32_t nowMs = millis();
    if (g_baseConnected && g_lastRx != 0U &&
        (uint32_t)(nowMs - g_lastRx) >= RS485_LINK_TIMEOUT_MS) {
        g_baseConnected = false;
        Serial.println(F("[RS485] Base connection lost"));
    }

    if (!g_baseConnected && g_rxBytes != g_lastDiagnosticBytes &&
        (uint32_t)(nowMs - g_lastDiagnosticMs) >= 2000UL) {
        g_lastDiagnosticMs = nowMs;
        g_lastDiagnosticBytes = g_rxBytes;
        Serial.print(F("[RS485] RX bytes="));
        Serial.print(g_rxBytes);
        Serial.print(F(" valid="));
        Serial.print(g_rxPackets);
        Serial.print(F(" CRC errors="));
        Serial.print(g_crcErrors);
        Serial.print(F(" frame errors="));
        Serial.print(g_frameErrors);
        Serial.print(F(" expected frame="));
        Serial.print(sizeof(full_packet_net_t) + 10U);
        Serial.print(F(" or "));
        Serial.println(sizeof(full_packet_net_time64_t) + 10U);
    }
}

void RS485Display_fini() { RS485_SERIAL.end(); }
void rxTask(void* param) { (void)param; for(;;) { RS485Display_loop(); vTaskDelay(pdMS_TO_TICKS(10)); } }
void RS485Display_setOutgoingAux(const aux_t* aux) { if (aux) g_outAux = *aux; }
void RS485Display_getOutgoingAux(aux_t* aux) { if (aux) *aux = g_outAux; }
void RS485Display_getIncomingAux(aux_t* aux, uint8_t* btn1, uint8_t* btn2)
{
    if (aux) *aux = g_inAux;
    if (btn1) *btn1 = g_lastPacket.BUTTON1;
    if (btn2) *btn2 = g_lastPacket.BUTTON2;
}
bool RS485Display_hasIncomingButton() { return g_inButton != 0; }
uint8_t RS485Display_takeIncomingButton() { uint8_t v = g_inButton; g_inButton = 0; return v; }
void RS485Display_setLocalButtonEvent(uint8_t event) { if (event == 0 || event > 3) return; g_localButton = event; }
bool RS485Display_baseConnected() { return g_baseConnected; }
uint32_t RS485Display_lastRxMs() { return g_lastRx; }
size_t RS485Display_payloadSize() { return sizeof(full_packet_net_t); }
size_t RS485Display_frameSize() { return sizeof(full_packet_net_t) + 10; }
uint32_t RS485Display_rxPackets() { return g_rxPackets; }
uint32_t RS485Display_txPackets() { return g_txPackets; }
