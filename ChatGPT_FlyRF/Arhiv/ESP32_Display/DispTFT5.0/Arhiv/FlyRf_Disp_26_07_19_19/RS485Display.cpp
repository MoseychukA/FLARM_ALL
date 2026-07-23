/*
  Код RS485 взят из рабочего FlyRf_Disp_26_05_18_01.
  Единственное дополнение: payload сначала принимается в full_packet_rx_t,
  затем после CRC переводится в рабочие типы дисплея.
*/

#include "RS485Display.h"
#include "DeviceInfo.h"
#include "HardwareConfig.h"
#include "DisplayRemote.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

static full_packet_net_t g_lastPacket = {};
static aux_t g_inAux = {};
static aux_t g_outAux = {};
static volatile uint8_t g_inButton = 0;
static volatile uint8_t g_localButton = 0;
static uint32_t g_lastRx = 0;
static uint32_t g_rxPackets = 0;
static uint32_t g_txPackets = 0;

static uint16_t crc16_ccitt(const uint8_t* data, size_t length)
{
    uint16_t crc = 0x0000;
    for (size_t index = 0; index < length; ++index)
    {
        crc ^= (uint16_t)data[index] << 8;
        for (uint8_t bit = 0; bit < 8; ++bit)
            crc = (crc & 0x8000U) ?
                  (uint16_t)((crc << 1) ^ 0x1021U) : (uint16_t)(crc << 1);
    }
    return crc;
}

static void rs485SetTX(bool enabled)
{
    digitalWrite(RS485_DE_PIN, enabled ? HIGH : LOW);
    if (enabled) delayMicroseconds(50);
}

static void netToLocalThis(const ufo_net_t& source)
{
    ThisAircraft.addr = source.addr;
    ThisAircraft.squawk = source.squawk;
    memcpy(ThisAircraft.callsign, source.callsign, sizeof(ThisAircraft.callsign));
    ThisAircraft.altitude = source.altitude;
    ThisAircraft.pressure_altitude = source.pressure_altitude;
    ThisAircraft.course = source.course;
    ThisAircraft.speed = source.speed;
    ThisAircraft.vert_rate = source.vert_rate;
    ThisAircraft.latitude = source.latitude;
    ThisAircraft.longitude = source.longitude;
    ThisAircraft.local_latitude = source.latitude;
    ThisAircraft.local_longitude = source.longitude;
    ThisAircraft.timestamp = source.timestamp;
}

static void netToLocalContainer(const ufo_net_t& source, ufo_t& target)
{
    target.addr = source.addr;
    target.squawk = source.squawk;
    memcpy(target.callsign, source.callsign, sizeof(target.callsign));
    target.altitude = source.altitude;
    target.pressure_altitude = source.pressure_altitude;
    target.course = source.course;
    target.speed = source.speed;
    target.distance = source.distance;
    target.bearing = source.bearing;
    target.vert_rate = source.vert_rate;
    target.latitude = source.latitude;
    target.longitude = source.longitude;
    target.timestamp = source.timestamp;
    target.lastUpdate = millis();
    target.rssi_LoRa = source.rssi_LoRa;
    target.rssi_rp2040 = source.rssi_rp2040;
    target.signal_source = source.signal_source;
    target.valid = source.addr != 0U;
}

static void rxAircraftToNative(const ufo_net_rx_t& source, ufo_net_t& target)
{
    memset(&target, 0, sizeof(target));
    target.addr = source.addr;
    target.squawk = source.squawk;
    memcpy(target.callsign, source.callsign, sizeof(target.callsign));
    target.altitude = source.altitude;
    target.pressure_altitude = source.pressure_altitude;
    target.course = source.course;
    target.speed = source.speed;
    target.distance = source.distance;
    target.bearing = source.bearing;
    target.vert_rate = source.vert_rate;
    target.latitude = source.latitude;
    target.longitude = source.longitude;
    // Расширение выполняется только здесь, уже после проверки CRC всего кадра.
    // Сначала сохраняем беззнаковое 32-битное значение базы, затем переводим
    // его в нативный time_t дисплея (на новом core он 64-битный).
    const uint64_t timestamp64 = (uint64_t)source.timestamp;
    target.timestamp = (time_t)timestamp64;
    target.rssi_LoRa = source.rssi_LoRa;
    target.rssi_rp2040 = source.rssi_rp2040;
    target.signal_source = source.signal_source;
}

static void rxPacketToNative(const full_packet_rx_t& source,
                             full_packet_net_t& target)
{
    memset(&target, 0, sizeof(target));
    rxAircraftToNative(source.ThisAircraft, target.ThisAircraft);
    for (int index = 0; index < MAX_TRACKING_OBJECTS; ++index)
        rxAircraftToNative(source.Container[index], target.Container[index]);
    target.AuxData = source.AuxData;
    target.BUTTON1 = source.BUTTON1;
    target.BUTTON2 = source.BUTTON2;
}

void net_to_ufo_Container(const ufo_t* source, ufo_net_t* target)
{
    if (!source || !target) return;
    memset(target, 0, sizeof(*target));
    target->addr = source->addr;
    target->squawk = source->squawk;
    memcpy(target->callsign, source->callsign, sizeof(target->callsign));
    target->altitude = source->altitude;
    target->pressure_altitude = source->pressure_altitude;
    target->course = source->course;
    target->speed = source->speed;
    target->distance = source->distance;
    target->bearing = source->bearing;
    target->vert_rate = source->vert_rate;
    target->latitude = source->latitude;
    target->longitude = source->longitude;
    target->timestamp = source->timestamp;
    target->rssi_LoRa = source->rssi_LoRa;
    target->rssi_rp2040 = source->rssi_rp2040;
    target->signal_source = source->signal_source;
}

void net_to_ufo_ThisAircraft(const ufo_t* source, ufo_net_t* target)
{
    net_to_ufo_Container(source, target);
}

bool receivePacket_RS485(full_packet_net_t* packet, uint8_t* button1, uint8_t* button2)
{
    if (!packet) return false;
    // Ниже сохранён алгоритм рабочего исходника. Изменён только тип payload.
    static uint8_t buffer[sizeof(full_packet_rx_t) + 16U] = {};
    static size_t index = 0U;
    while (RS485_SERIAL.available() && index < sizeof(buffer))
        buffer[index++] = (uint8_t)RS485_SERIAL.read();
    if (index < 4U) return false;
    size_t start = 0;
    while (index - start >= 4U) {
        if (buffer[start] == 0xDD && buffer[start + 1U] == 0xCC &&
            buffer[start + 2U] == 0xBB && buffer[start + 3U] == 0xAA)
            break;
        ++start;
    }
    if (start > 0U) {
        memmove(buffer, buffer + start, index - start);
        index -= start;
    }
    const size_t frameLength = sizeof(full_packet_rx_t) + 10U;
    if (index < frameLength) return false;
    const size_t crcOffset = 4U + sizeof(full_packet_rx_t);
    const size_t footerOffset = crcOffset + 2U;
    const bool footerOk =
        buffer[footerOffset] == 0xAA && buffer[footerOffset + 1U] == 0xBB &&
        buffer[footerOffset + 2U] == 0xCC && buffer[footerOffset + 3U] == 0xDD;
    if (!footerOk) {
        index = 0U;
        return false;
    }
    uint16_t receivedCrc = 0U;
    memcpy(&receivedCrc, &buffer[crcOffset], sizeof(receivedCrc));
    const uint16_t calculatedCrc =
        crc16_ccitt(&buffer[4], sizeof(full_packet_rx_t));
    if (receivedCrc != calculatedCrc) {
        index = 0U;
        return false;
    }
    full_packet_rx_t receivedPacket = {};
    memcpy(&receivedPacket, &buffer[4], sizeof(receivedPacket));
    rxPacketToNative(receivedPacket, *packet);
    if (button1) *button1 = packet->BUTTON1;
    if (button2) *button2 = packet->BUTTON2;
    index -= frameLength;
    if (index > 0U) memmove(buffer, buffer + frameLength, index);
    return true;
}

static void sendReply()
{
    full_packet_rx_t reply = {};
    aux_t outgoing = g_outAux;
    outgoing.new_button_M = g_localButton;
    g_localButton = 0;
    reply.AuxData = outgoing;
    const uint16_t crc = crc16_ccitt((const uint8_t*)&reply, sizeof(reply));
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

static void applyPacket(const full_packet_net_t& packet)
{
    g_lastPacket = packet;
    g_inAux = packet.AuxData;
    netToLocalThis(packet.ThisAircraft);
    for (int index = 0; index < MAX_TRACKING_OBJECTS; ++index)
        netToLocalContainer(packet.Container[index], Container[index]);
    if (packet.AuxData.new_button_M != 0U)
        g_inButton = packet.AuxData.new_button_M;
    g_lastRx = millis();
    ++g_rxPackets;

    Remote_setGnssState(packet.AuxData.isValidGNSS_M,
                        packet.AuxData.gps_time_valid_M,
                        packet.AuxData.gps_satellites_valid_M,
                        packet.AuxData.Time_Hour_M,
                        packet.AuxData.Time_Minute_M,
                        ThisAircraft.latitude, ThisAircraft.longitude,
                        ThisAircraft.altitude, ThisAircraft.hdop);
    Remote_setBaseStatus(packet.AuxData.base_test_mode_M,
                         packet.AuxData.gps_rx_M,
                         packet.AuxData.gps_satellites_M,
                         packet.AuxData.display_coord_valid_M,
                         packet.AuxData.display_coord_is_local_M,
                         packet.AuxData.display_latitude_M,
                         packet.AuxData.display_longitude_M,
                         packet.AuxData.lora_tx_packets_M,
                         packet.AuxData.lora_rx_packets_M,
                         packet.AuxData.lora_rf_hz_M);
    Remote_setLanStatus(packet.AuxData.lan_state_view_M,
                        packet.AuxData.lan_ready_M,
                        packet.AuxData.lan_link_up_M,
                        packet.AuxData.lan_udp_working_M,
                        packet.AuxData.lan_dhcp_M,
                        packet.AuxData.lan_ip_M,
                        packet.AuxData.lan_udp_port_M,
                        packet.AuxData.lan_tx_packets_M,
                        packet.AuxData.lan_rx_packets_M,
                        packet.AuxData.lan_udp_tx_packets_M,
                        packet.AuxData.lan_udp_rx_packets_M);
    Remote_setTrackerMessage(packet.AuxData.new_message ||
                             packet.AuxData.message_received,
                             packet.AuxData.msg_resp_M,
                             packet.AuxData.Time_Hour_M,
                             packet.AuxData.Time_Minute_M);
}

void RS485Display_setup()
{
    pinMode(RS485_DE_PIN, OUTPUT);
    rs485SetTX(false);
    RS485_SERIAL.setRxBufferSize(2048);
    RS485_SERIAL.setTxBufferSize(2048);
    RS485_SERIAL.begin(RS485_BAUD, RS485_CONFIG, RS485_RX_PIN, RS485_TX_PIN);
    Serial.print(F("[RS485] source RX structure payload/frame="));
    Serial.print(sizeof(full_packet_rx_t)); Serial.print('/');
    Serial.println(sizeof(full_packet_rx_t) + 10U);
    Serial.println(F("[RS485] ufo_net_rx_t -> ufo_net_t after CRC16"));
}

void RS485Display_loop()
{
    // Парсер и структура остаются исходными. Если во время перерисовки
    // накопилось несколько кадров, разбираем их последовательно за один вход.
    for (uint8_t frame = 0; frame < 4U; ++frame)
    {
        full_packet_net_t packet = {};
        uint8_t button1 = 0;
        uint8_t button2 = 0;
        if (!receivePacket_RS485(&packet, &button1, &button2)) break;
        applyPacket(packet);
        Serial.print(F("[RS485] RX OK payload/frame="));
        Serial.print(sizeof(full_packet_rx_t)); Serial.print('/');
        Serial.print(sizeof(full_packet_rx_t) + 10U);
        Serial.print(F(", #"));
        Serial.println(g_rxPackets);
        if (g_localButton != 0U || g_outAux.confirm_message_M) sendReply();
    }
}

void RS485Display_fini()
{
    RS485_SERIAL.flush();
    RS485_SERIAL.end();
}

void rxTask(void* parameter)
{
    (void)parameter;
    for (;;)
    {
        RS485Display_loop();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void RS485Display_setOutgoingAux(const aux_t* auxiliary)
{
    if (auxiliary) g_outAux = *auxiliary;
}
void RS485Display_getOutgoingAux(aux_t* auxiliary)
{
    if (auxiliary) *auxiliary = g_outAux;
}
void RS485Display_getIncomingAux(aux_t* auxiliary, uint8_t* button1, uint8_t* button2)
{
    if (auxiliary) *auxiliary = g_inAux;
    if (button1) *button1 = g_lastPacket.BUTTON1;
    if (button2) *button2 = g_lastPacket.BUTTON2;
}
bool RS485Display_hasIncomingButton() { return g_inButton != 0U; }
uint8_t RS485Display_takeIncomingButton()
{
    const uint8_t value = g_inButton;
    g_inButton = 0;
    return value;
}
void RS485Display_setLocalButtonEvent(uint8_t event)
{
    if (event >= 1U && event <= 3U) g_localButton = event;
}
uint32_t RS485Display_lastRxMs() { return g_lastRx; }
uint32_t RS485Display_rxPackets() { return g_rxPackets; }
uint32_t RS485Display_txPackets() { return g_txPackets; }
size_t RS485Display_payloadSize()
{
    return sizeof(full_packet_rx_t);
}
size_t RS485Display_frameSize()
{
    return sizeof(full_packet_rx_t) + 10U;
}
