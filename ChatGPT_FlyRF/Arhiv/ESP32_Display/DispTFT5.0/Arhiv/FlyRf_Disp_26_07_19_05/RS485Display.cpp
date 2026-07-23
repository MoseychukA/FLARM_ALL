#include "RS485Display.h"
#include "DeviceInfo.h"
#include "HardwareConfig.h"
#include "DisplayRemote.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

// В исходном протоколе time_t передавался напрямую. В зависимости от версии
// ESP32 Arduino он занимает 4 или 8 байт, поэтому поддерживаем оба формата.
typedef struct __attribute__((packed)) {
    uint32_t addr;
    int squawk;
    uint8_t callsign[8];
    float altitude;
    float pressure_altitude;
    float course;
    float speed;
    float distance;
    float bearing;
    int vert_rate;
    float latitude;
    float longitude;
    int32_t timestamp;
    int8_t rssi_LoRa;
    int8_t rssi_rp2040;
    uint8_t signal_source;
} ufo_net_time32_t;

typedef struct __attribute__((packed)) {
    ufo_net_time32_t ThisAircraft;
    ufo_net_time32_t Container[MAX_TRACKING_OBJECTS];
    aux_t AuxData;
    uint8_t BUTTON1;
    uint8_t BUTTON2;
} full_packet_time32_t;

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
static uint8_t g_rxBuffer[sizeof(full_packet_net_t) + 16] = {};
static size_t g_rxBuffered = 0;
static size_t g_lastAcceptedPayloadSize = 0;
static size_t g_lastAcceptedFrameSize = 0;
static bool g_lastPacketUsesTime32 = false;
static bool g_bufferUsesReadableMarkers = false;
static bool g_bufferUsesBigEndianCrc = false;
static bool g_lastPacketUsesReadableMarkers = false;
static bool g_lastPacketUsesBigEndianCrc = false;
static uint8_t g_recentRxBytes[16] = {};
static uint8_t g_recentRxPosition = 0;
static uint8_t g_recentRxCount = 0;

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

static void time32ToCurrent(const ufo_net_time32_t& src, ufo_net_t& dst)
{
    memset(&dst, 0, sizeof(dst));
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

static void time32PacketToCurrent(const full_packet_time32_t& src,
                                  full_packet_net_t& dst)
{
    memset(&dst, 0, sizeof(dst));
    time32ToCurrent(src.ThisAircraft, dst.ThisAircraft);
    for (int i = 0; i < MAX_TRACKING_OBJECTS; ++i)
        time32ToCurrent(src.Container[i], dst.Container[i]);
    dst.AuxData = src.AuxData;
    dst.BUTTON1 = src.BUTTON1;
    dst.BUTTON2 = src.BUTTON2;
}

static bool bufferedFrameValid(size_t payloadSize)
{
    const size_t frameSize = payloadSize + 10U;
    if (g_rxBuffered < frameSize) return false;
    const size_t crcOffset = 4U + payloadSize;
    const size_t footerOffset = crcOffset + 2U;
    const uint8_t footer0 = g_bufferUsesReadableMarkers ? 0xDD : 0xAA;
    const uint8_t footer1 = g_bufferUsesReadableMarkers ? 0xCC : 0xBB;
    const uint8_t footer2 = g_bufferUsesReadableMarkers ? 0xBB : 0xCC;
    const uint8_t footer3 = g_bufferUsesReadableMarkers ? 0xAA : 0xDD;
    if (g_rxBuffer[footerOffset] != footer0 ||
        g_rxBuffer[footerOffset + 1] != footer1 ||
        g_rxBuffer[footerOffset + 2] != footer2 ||
        g_rxBuffer[footerOffset + 3] != footer3) return false;
    const uint16_t receivedLittle = (uint16_t)g_rxBuffer[crcOffset] |
                                    ((uint16_t)g_rxBuffer[crcOffset + 1] << 8);
    const uint16_t receivedBig = ((uint16_t)g_rxBuffer[crcOffset] << 8) |
                                 (uint16_t)g_rxBuffer[crcOffset + 1];
    const uint16_t calculated = crc16_ccitt(&g_rxBuffer[4], payloadSize);
    if (receivedLittle == calculated) {
        g_bufferUsesBigEndianCrc = false;
        return true;
    }
    if (receivedBig == calculated) {
        g_bufferUsesBigEndianCrc = true;
        return true;
    }
    return false;
}

static void consumeBufferedFrame(size_t frameSize)
{
    g_rxBuffered -= frameSize;
    if (g_rxBuffered > 0)
        memmove(g_rxBuffer, g_rxBuffer + frameSize, g_rxBuffered);
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
    if (!pkt) return false;
    while (RS485_SERIAL.available() && g_rxBuffered < sizeof(g_rxBuffer)) {
        const uint8_t value = (uint8_t)RS485_SERIAL.read();
        g_rxBuffer[g_rxBuffered++] = value;
        g_recentRxBytes[g_recentRxPosition] = value;
        g_recentRxPosition = (g_recentRxPosition + 1U) % sizeof(g_recentRxBytes);
        if (g_recentRxCount < sizeof(g_recentRxBytes)) ++g_recentRxCount;
        ++g_rxBytes;
    }
    if (g_rxBuffered < 4) return false;
    size_t start = 0;
    while (g_rxBuffered - start >= 4) {
        const bool nativeOrder = g_rxBuffer[start] == 0xDD &&
                                 g_rxBuffer[start + 1] == 0xCC &&
                                 g_rxBuffer[start + 2] == 0xBB &&
                                 g_rxBuffer[start + 3] == 0xAA;
        const bool readableOrder = g_rxBuffer[start] == 0xAA &&
                                   g_rxBuffer[start + 1] == 0xBB &&
                                   g_rxBuffer[start + 2] == 0xCC &&
                                   g_rxBuffer[start + 3] == 0xDD;
        if (nativeOrder || readableOrder) {
            g_bufferUsesReadableMarkers = readableOrder;
            break;
        }
        ++start;
    }
    if (start > 0) {
        memmove(g_rxBuffer, g_rxBuffer + start, g_rxBuffered - start);
        g_rxBuffered -= start;
    }
    const size_t currentPayloadSize = sizeof(full_packet_net_t);
    const size_t currentFrameSize = currentPayloadSize + 10U;
    const size_t time32PayloadSize = sizeof(full_packet_time32_t);
    const size_t time32FrameSize = time32PayloadSize + 10U;
    const size_t minimumFrameSize = currentFrameSize < time32FrameSize ?
                                    currentFrameSize : time32FrameSize;
    if (g_rxBuffered < minimumFrameSize) return false;

    if (bufferedFrameValid(currentPayloadSize)) {
        memcpy(pkt, &g_rxBuffer[4], currentPayloadSize);
        g_lastPacketUsesTime32 = false;
        g_lastPacketUsesReadableMarkers = g_bufferUsesReadableMarkers;
        g_lastPacketUsesBigEndianCrc = g_bufferUsesBigEndianCrc;
        g_lastAcceptedPayloadSize = currentPayloadSize;
        g_lastAcceptedFrameSize = currentFrameSize;
        consumeBufferedFrame(currentFrameSize);
    } else if (bufferedFrameValid(time32PayloadSize)) {
        full_packet_time32_t time32Packet = {};
        memcpy(&time32Packet, &g_rxBuffer[4], time32PayloadSize);
        time32PacketToCurrent(time32Packet, *pkt);
        g_lastPacketUsesTime32 = true;
        g_lastPacketUsesReadableMarkers = g_bufferUsesReadableMarkers;
        g_lastPacketUsesBigEndianCrc = g_bufferUsesBigEndianCrc;
        g_lastAcceptedPayloadSize = time32PayloadSize;
        g_lastAcceptedFrameSize = time32FrameSize;
        consumeBufferedFrame(time32FrameSize);
    } else {
        const size_t maximumFrameSize = currentFrameSize > time32FrameSize ?
                                        currentFrameSize : time32FrameSize;
        // Короткий кадр может быть только началом длинного — ждём. Если уже
        // накоплены оба возможных размера, сдвигаемся к следующему заголовку.
        if (g_rxBuffered < maximumFrameSize) return false;
        ++g_frameErrors;
        ++g_crcErrors;
        memmove(g_rxBuffer, g_rxBuffer + 1, --g_rxBuffered);
        return false;
    }
    if (btn1) *btn1 = pkt->BUTTON1;
    if (btn2) *btn2 = pkt->BUTTON2;
    return true;
}

static void sendReply()
{
    full_packet_net_t currentReply = {};
    currentReply.AuxData = g_outAux;
    currentReply.AuxData.new_button_M = g_localButton;
    g_localButton = 0;
    const uint8_t* payload = (const uint8_t*)&currentReply;
    size_t payloadSize = sizeof(currentReply);
    full_packet_time32_t time32Reply = {};
    if (g_lastPacketUsesTime32) {
        time32Reply.AuxData = currentReply.AuxData;
        time32Reply.BUTTON1 = currentReply.BUTTON1;
        time32Reply.BUTTON2 = currentReply.BUTTON2;
        payload = (const uint8_t*)&time32Reply;
        payloadSize = sizeof(time32Reply);
    }
    const uint16_t crc = crc16_ccitt(payload, payloadSize);
    const uint8_t readableHeader[4] = {0xAA, 0xBB, 0xCC, 0xDD};
    const uint8_t readableFooter[4] = {0xDD, 0xCC, 0xBB, 0xAA};
    const uint8_t crcBytes[2] = {
        (uint8_t)(g_lastPacketUsesBigEndianCrc ? (crc >> 8) : crc),
        (uint8_t)(g_lastPacketUsesBigEndianCrc ? crc : (crc >> 8))
    };
    rs485SetTX(true);
    if (g_lastPacketUsesReadableMarkers)
        RS485_SERIAL.write(readableHeader, sizeof(readableHeader));
    else
        RS485_SERIAL.write((const uint8_t*)&PACKET_HEADER, sizeof(PACKET_HEADER));
    RS485_SERIAL.write(payload, payloadSize);
    RS485_SERIAL.write(crcBytes, sizeof(crcBytes));
    if (g_lastPacketUsesReadableMarkers)
        RS485_SERIAL.write(readableFooter, sizeof(readableFooter));
    else
        RS485_SERIAL.write((const uint8_t*)&PACKET_FOOTER, sizeof(PACKET_FOOTER));
    RS485_SERIAL.flush();
    ++g_txPackets;
    delayMicroseconds(200);
    rs485SetTX(false);
}

static void applyPacket(const full_packet_net_t& p)
{
    g_lastPacket = p;
    g_inAux = p.AuxData;
    netToLocalThis(p.ThisAircraft);
    for (int i = 0; i < MAX_TRACKING_OBJECTS; ++i) netToLocalContainer(p.Container[i], Container[i]);
    if (p.AuxData.new_button_M != 0) g_inButton = p.AuxData.new_button_M;
    g_lastRx = millis();
    ++g_rxPackets;
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
    pinMode(RS485_DE_PIN, OUTPUT);
    rs485SetTX(false);
    RS485_SERIAL.setRxBufferSize(4096);
    RS485_SERIAL.setTxBufferSize(2048);
    RS485_SERIAL.begin(RS485_BAUD, RS485_CONFIG, RS485_RX_PIN, RS485_TX_PIN);
    Serial.print(F("[RS485] RX GPIO="));
    Serial.print(RS485_RX_PIN);
    Serial.print(F(" TX GPIO="));
    Serial.print(RS485_TX_PIN);
    Serial.print(F(" DE GPIO="));
    Serial.print(RS485_DE_PIN);
    Serial.print(F(" baud="));
    Serial.println(RS485_BAUD);
    Serial.print(F("[RS485] expected payload="));
    Serial.print(sizeof(full_packet_net_t));
    Serial.print(F(" bytes, full frame="));
    Serial.print(sizeof(full_packet_net_t) + 10U);
    Serial.println(F(" bytes"));
    Serial.print(F("[RS485] compatible time32 payload="));
    Serial.print(sizeof(full_packet_time32_t));
    Serial.print(F(" bytes, full frame="));
    Serial.print(sizeof(full_packet_time32_t) + 10U);
    Serial.println(F(" bytes"));
    Serial.println(F("[RS485] accepted headers: DD CC BB AA and AA BB CC DD"));
}

void RS485Display_loop()
{
    // За один проход разбираем все уже принятые полные кадры (не более
    // четырёх), чтобы модель дисплея получила самый свежий пакет до отрисовки.
    bool packetDecoded = false;
    for (uint8_t frame = 0; frame < 4; ++frame) {
        full_packet_net_t pkt = {};
        uint8_t b1 = 0, b2 = 0;
        if (!receivePacket_RS485(&pkt, &b1, &b2)) break;
        applyPacket(pkt);
        packetDecoded = true;
        Serial.print(F("[RS485] RX OK: payload="));
        Serial.print(g_lastAcceptedPayloadSize);
        Serial.print(F(" bytes, frame="));
        Serial.print(g_lastAcceptedFrameSize);
        Serial.print(g_lastPacketUsesTime32 ? F(" bytes, time_t=32 bit, packet #") :
                                             F(" bytes, native time_t, packet #"));
        Serial.print(g_rxPackets);
        Serial.print(g_lastPacketUsesReadableMarkers ? F(", header=AA BB CC DD") :
                                                       F(", header=DD CC BB AA"));
        Serial.println(g_lastPacketUsesBigEndianCrc ? F(", CRC=BE") : F(", CRC=LE"));
    }
    if (packetDecoded && (g_localButton != 0 || g_outAux.confirm_message_M)) sendReply();

    const uint32_t now = millis();
    if (!packetDecoded && (g_lastDiagnosticMs == 0 ||
        (uint32_t)(now - g_lastDiagnosticMs) >= 2000UL)) {
        g_lastDiagnosticMs = now;
        Serial.print(F("[RS485] waiting: total bytes="));
        Serial.print(g_rxBytes);
        Serial.print(F(", buffered="));
        Serial.print(g_rxBuffered);
        Serial.print(F(", UART available="));
        Serial.print(RS485_SERIAL.available());
        Serial.print(F(", footer errors="));
        Serial.print(g_frameErrors);
        Serial.print(F(", CRC errors="));
        Serial.print(g_crcErrors);
        Serial.print(F(", recent="));
        const uint8_t first = (g_recentRxPosition + sizeof(g_recentRxBytes) -
                               g_recentRxCount) % sizeof(g_recentRxBytes);
        for (uint8_t i = 0; i < g_recentRxCount; ++i) {
            const uint8_t value = g_recentRxBytes[(first + i) % sizeof(g_recentRxBytes)];
            char byteText[4];
            snprintf(byteText, sizeof(byteText), "%02X", (unsigned)value);
            Serial.print(byteText);
            if (i + 1U < g_recentRxCount) Serial.print(' ');
        }
        Serial.println();
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
uint32_t RS485Display_lastRxMs() { return g_lastRx; }
size_t RS485Display_payloadSize() { return sizeof(full_packet_net_t); }
size_t RS485Display_frameSize() { return sizeof(full_packet_net_t) + 10; }
uint32_t RS485Display_rxPackets() { return g_rxPackets; }
uint32_t RS485Display_txPackets() { return g_txPackets; }
