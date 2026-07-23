#include "RS485Display.h"
#include "DeviceInfo.h"
#include "HardwareConfig.h"
#include "DisplayRemote.h"
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

static full_packet_net_t g_lastPacket = {};
static aux_t g_inAux = {};
static aux_t g_outAux = {};
static volatile uint8_t g_inButton = 0;
static volatile uint8_t g_localButton = 0;
// Обновляется сразу после приема корректного кадра в отдельной задаче RS485.
// Поэтому длительная отрисовка экрана не вызывает ложную потерю связи.
static volatile uint32_t g_lastRx = 0;
static uint32_t g_rxPackets = 0;
static uint32_t g_txPackets = 0;
static full_packet_net_t g_pendingPacket = {};
static SemaphoreHandle_t g_pendingMutex = nullptr;
static TaskHandle_t g_rs485RxTask = nullptr;
static bool g_pendingPacketReady = false;

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
    static uint8_t buffer[sizeof(full_packet_net_t) + 64];
    static size_t idx = 0;
    const size_t frameLen = sizeof(full_packet_net_t) + 10;

    while (RS485_SERIAL.available() > 0 && idx < sizeof(buffer))
    {
        const int value = RS485_SERIAL.read();
        if (value < 0) break;
        buffer[idx++] = (uint8_t)value;
    }

    for (;;)
    {
        size_t start = 0;
        while (idx - start >= 4U)
        {
            if (buffer[start] == 0xDD && buffer[start + 1U] == 0xCC &&
                buffer[start + 2U] == 0xBB && buffer[start + 3U] == 0xAA)
            {
                break;
            }
            ++start;
        }

        if (start > 0U)
        {
            memmove(buffer, buffer + start, idx - start);
            idx -= start;
        }
        if (idx < frameLen) return false;

        const size_t crcOff = 4U + sizeof(full_packet_net_t);
        const size_t footerOff = crcOff + sizeof(uint16_t);
        const bool footerOk =
            buffer[footerOff] == 0xAA && buffer[footerOff + 1U] == 0xBB &&
            buffer[footerOff + 2U] == 0xCC && buffer[footerOff + 3U] == 0xDD;

        uint16_t crcRx = 0U;
        memcpy(&crcRx, &buffer[crcOff], sizeof(crcRx));
        const uint16_t crcCalc = crc16_ccitt(&buffer[4], sizeof(full_packet_net_t));
        if (!footerOk || crcRx != crcCalc)
        {
            // Keep following bytes: the next valid frame may already be in the
            // UART buffer.  Dropping everything here caused long link outages.
            memmove(buffer, buffer + 1U, idx - 1U);
            --idx;
            continue;
        }

        memcpy(pkt, &buffer[4], sizeof(full_packet_net_t));
        if (btn1) *btn1 = pkt->BUTTON1;
        if (btn2) *btn2 = pkt->BUTTON2;
        idx -= frameLen;
        if (idx > 0U) memmove(buffer, buffer + frameLen, idx);
        return true;
    }
}

static void sendReply()
{
    full_packet_net_t reply = {};
    reply.AuxData = g_outAux;
    reply.AuxData.new_button_M = g_localButton;
    g_localButton = 0;
    uint16_t crc = crc16_ccitt((const uint8_t*)&reply, sizeof(reply));
    rs485SetTX(true);
    RS485_SERIAL.write((const uint8_t*)&PACKET_HEADER, sizeof(PACKET_HEADER));
    RS485_SERIAL.write((const uint8_t*)&reply, sizeof(reply));
    RS485_SERIAL.write((const uint8_t*)&crc, sizeof(crc));
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
    if (g_pendingMutex == nullptr) g_pendingMutex = xSemaphoreCreateMutex();
    pinMode(RS485_DE_PIN, OUTPUT);
    rs485SetTX(false);
    RS485_SERIAL.setRxBufferSize(4096);
    RS485_SERIAL.setTxBufferSize(4096);
    RS485_SERIAL.begin(RS485_BAUD, RS485_CONFIG, RS485_RX_PIN, RS485_TX_PIN);
    if (g_rs485RxTask == nullptr)
    {
        xTaskCreatePinnedToCore(rxTask, "RS485_RX", 4096, nullptr, 4, &g_rs485RxTask, 0);
    }
}

void RS485Display_loop()
{
    full_packet_net_t pkt = {};
    bool packetReady = false;
    if (g_pendingMutex != nullptr && xSemaphoreTake(g_pendingMutex, 0) == pdTRUE)
    {
        if (g_pendingPacketReady)
        {
            pkt = g_pendingPacket;
            g_pendingPacketReady = false;
            packetReady = true;
        }
        xSemaphoreGive(g_pendingMutex);
    }

    if (packetReady)
    {
        applyPacket(pkt);
        if (g_localButton != 0 || g_outAux.confirm_message_M) sendReply();
    }
}

void RS485Display_fini()
{
    if (g_rs485RxTask != nullptr)
    {
        vTaskDelete(g_rs485RxTask);
        g_rs485RxTask = nullptr;
    }
    RS485_SERIAL.end();
}

void rxTask(void* param)
{
    (void)param;
    full_packet_net_t pkt = {};
    uint8_t b1 = 0U;
    uint8_t b2 = 0U;
    for (;;)
    {
        if (receivePacket_RS485(&pkt, &b1, &b2))
        {
            g_lastRx = millis();
            if (g_pendingMutex != nullptr && xSemaphoreTake(g_pendingMutex, portMAX_DELAY) == pdTRUE)
            {
                g_pendingPacket = pkt;
                g_pendingPacketReady = true;
                xSemaphoreGive(g_pendingMutex);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
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
