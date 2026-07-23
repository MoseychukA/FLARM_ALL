/*
  Приёмник RS485 внешнего дисплея.

  Реализация повторяет передатчик FlyRf_Base_26_07_17_00:
  header -> packed payload -> CRC16-CCITT -> footer. UART обслуживается
  отдельной FreeRTOS-задачей, а расшифрованный пакет применяется в SystemLoop.
*/

#include "RS485Display.h"
#include "DeviceInfo.h"
#include "HardwareConfig.h"
#include "DisplayRemote.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <string.h>

// База из приложенного проекта собрана на ESP32 Arduino 2.0.11. В её
// wire-структуре time_t занимает 32 бита. Явный int32_t делает длину пакета
// стабильной при сборке дисплея на ESP32 Arduino 3.x.
typedef struct __attribute__((packed)) {
    uint32_t addr;
    int32_t squawk;
    uint8_t callsign[8];
    float altitude;
    float pressure_altitude;
    float course;
    float speed;
    float distance;
    float bearing;
    int32_t vert_rate;
    float latitude;
    float longitude;
    int32_t timestamp;
    int8_t rssi_LoRa;
    int8_t rssi_rp2040;
    uint8_t signal_source;
} base_ufo_wire_t;

typedef struct __attribute__((packed)) {
    base_ufo_wire_t ThisAircraft;
    base_ufo_wire_t Container[MAX_TRACKING_OBJECTS];
    aux_t AuxData;
    uint8_t BUTTON1;
    uint8_t BUTTON2;
} base_packet_wire_t;

static_assert(sizeof(base_ufo_wire_t) == 59U,
              "FlyRf Base wire aircraft must be 59 bytes");
static_assert(sizeof(aux_t) == 454U,
              "FlyRf Base auxiliary block must be 454 bytes");
static_assert(sizeof(base_packet_wire_t) == 1223U,
              "FlyRf Base payload must be 1223 bytes");

namespace
{
constexpr uint32_t RX_TASK_PERIOD_MS = 20UL;
constexpr int RX_READ_BUDGET = 256;
constexpr size_t BASE_FRAME_SIZE = sizeof(base_packet_wire_t) + 10U;
constexpr size_t NATIVE_FRAME_SIZE = sizeof(full_packet_net_t) + 10U;
constexpr size_t MAX_PAYLOAD_SIZE = sizeof(full_packet_net_t) > sizeof(base_packet_wire_t) ?
                                    sizeof(full_packet_net_t) : sizeof(base_packet_wire_t);

struct WireFormat
{
    size_t payloadSize;
    size_t frameSize;
    bool baseTime32;
    bool readableMarkers;
    bool bigEndianCrc;
};

full_packet_net_t g_lastPacket = {};
full_packet_net_t g_queuedPacket = {};
aux_t g_inAux = {};
aux_t g_outAux = {};
WireFormat g_queuedFormat = {};
WireFormat g_lastFormat = {};
volatile bool g_packetQueued = false;
volatile uint8_t g_inButton = 0;
volatile uint8_t g_localButton = 0;
volatile uint32_t g_lastRx = 0;
volatile uint32_t g_rxPackets = 0;
volatile uint32_t g_txPackets = 0;
volatile uint32_t g_rxBytes = 0;
volatile uint32_t g_crcErrors = 0;
volatile uint32_t g_frameErrors = 0;
volatile uint32_t g_headerMatches = 0;
volatile uint32_t g_printableBytes = 0;
volatile uint32_t g_ffBytes = 0;
uint32_t g_lastDiagnosticMs = 0;

SemaphoreHandle_t g_serialMutex = nullptr;
SemaphoreHandle_t g_packetMutex = nullptr;
SemaphoreHandle_t g_stateMutex = nullptr;
TaskHandle_t g_rxTask = nullptr;

uint8_t g_rxBuffer[MAX_PAYLOAD_SIZE + 64U] = {};
size_t g_rxBuffered = 0;
bool g_bufferReadableMarkers = false;
bool g_bufferBigEndianCrc = false;
uint8_t g_recentRxBytes[16] = {};
uint8_t g_recentRxPosition = 0;
uint8_t g_recentRxCount = 0;

uint16_t crc16Ccitt(const uint8_t* data, size_t length)
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

void rs485SetTx(bool enabled)
{
    digitalWrite(RS485_DE_PIN, enabled ? HIGH : LOW);
    if (enabled) delayMicroseconds(50);
}

void noteRawByte(uint8_t value)
{
    ++g_rxBytes;
    if (value == 0xFFU) ++g_ffBytes;
    if ((value >= 0x20U && value <= 0x7EU) || value == '\r' || value == '\n' || value == '\t')
        ++g_printableBytes;
    g_recentRxBytes[g_recentRxPosition] = value;
    g_recentRxPosition = (g_recentRxPosition + 1U) % sizeof(g_recentRxBytes);
    if (g_recentRxCount < sizeof(g_recentRxBytes)) ++g_recentRxCount;
}

void readUartPortion()
{
    int budget = RX_READ_BUDGET;
    if (g_serialMutex) xSemaphoreTake(g_serialMutex, portMAX_DELAY);
    while (RS485_SERIAL.available() && budget > 0 && g_rxBuffered < sizeof(g_rxBuffer))
    {
        const int value = RS485_SERIAL.read();
        if (value < 0) break;
        g_rxBuffer[g_rxBuffered++] = (uint8_t)value;
        noteRawByte((uint8_t)value);
        --budget;
    }
    if (g_serialMutex) xSemaphoreGive(g_serialMutex);
}

bool findAndAlignHeader()
{
    if (g_rxBuffered < 4U) return false;
    size_t start = 0;
    while (g_rxBuffered - start >= 4U)
    {
        const bool baseOrder = g_rxBuffer[start] == 0xDD &&
                               g_rxBuffer[start + 1] == 0xCC &&
                               g_rxBuffer[start + 2] == 0xBB &&
                               g_rxBuffer[start + 3] == 0xAA;
        const bool readableOrder = g_rxBuffer[start] == 0xAA &&
                                   g_rxBuffer[start + 1] == 0xBB &&
                                   g_rxBuffer[start + 2] == 0xCC &&
                                   g_rxBuffer[start + 3] == 0xDD;
        if (baseOrder || readableOrder)
        {
            g_bufferReadableMarkers = readableOrder;
            ++g_headerMatches;
            break;
        }
        ++start;
    }

    if (start > 0U)
    {
        memmove(g_rxBuffer, g_rxBuffer + start, g_rxBuffered - start);
        g_rxBuffered -= start;
    }
    return g_rxBuffered >= 4U;
}

enum FrameCheck : uint8_t { FRAME_INCOMPLETE, FRAME_FOOTER_ERROR, FRAME_CRC_ERROR, FRAME_VALID };

FrameCheck checkBufferedFrame(size_t payloadSize)
{
    const size_t frameSize = payloadSize + 10U;
    if (g_rxBuffered < frameSize) return FRAME_INCOMPLETE;
    const size_t crcOffset = 4U + payloadSize;
    const size_t footerOffset = crcOffset + 2U;
    const uint8_t footer0 = g_bufferReadableMarkers ? 0xDD : 0xAA;
    const uint8_t footer1 = g_bufferReadableMarkers ? 0xCC : 0xBB;
    const uint8_t footer2 = g_bufferReadableMarkers ? 0xBB : 0xCC;
    const uint8_t footer3 = g_bufferReadableMarkers ? 0xAA : 0xDD;
    if (g_rxBuffer[footerOffset] != footer0 ||
        g_rxBuffer[footerOffset + 1] != footer1 ||
        g_rxBuffer[footerOffset + 2] != footer2 ||
        g_rxBuffer[footerOffset + 3] != footer3) return FRAME_FOOTER_ERROR;

    const uint16_t receivedLe = (uint16_t)g_rxBuffer[crcOffset] |
                                ((uint16_t)g_rxBuffer[crcOffset + 1] << 8);
    const uint16_t receivedBe = ((uint16_t)g_rxBuffer[crcOffset] << 8) |
                                (uint16_t)g_rxBuffer[crcOffset + 1];
    const uint16_t calculated = crc16Ccitt(&g_rxBuffer[4], payloadSize);
    if (receivedLe == calculated)
    {
        g_bufferBigEndianCrc = false;
        return FRAME_VALID;
    }
    if (receivedBe == calculated)
    {
        g_bufferBigEndianCrc = true;
        return FRAME_VALID;
    }
    return FRAME_CRC_ERROR;
}

void consumeFrame(size_t frameSize)
{
    g_rxBuffered -= frameSize;
    if (g_rxBuffered > 0U)
        memmove(g_rxBuffer, g_rxBuffer + frameSize, g_rxBuffered);
}

void baseAircraftToNative(const base_ufo_wire_t& source, ufo_net_t& target)
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
    target.timestamp = (time_t)source.timestamp;
    target.rssi_LoRa = source.rssi_LoRa;
    target.rssi_rp2040 = source.rssi_rp2040;
    target.signal_source = source.signal_source;
}

void basePacketToNative(const base_packet_wire_t& source, full_packet_net_t& target)
{
    memset(&target, 0, sizeof(target));
    baseAircraftToNative(source.ThisAircraft, target.ThisAircraft);
    for (int i = 0; i < MAX_TRACKING_OBJECTS; ++i)
        baseAircraftToNative(source.Container[i], target.Container[i]);
    target.AuxData = source.AuxData;
    target.BUTTON1 = source.BUTTON1;
    target.BUTTON2 = source.BUTTON2;
}

bool decodeBufferedPacket(full_packet_net_t& packet, WireFormat& format)
{
    readUartPortion();
    if (!findAndAlignHeader()) return false;

    // Формат приложенной базы (1223 байта) проверяется первым.
    const FrameCheck baseCheck = checkBufferedFrame(sizeof(base_packet_wire_t));
    if (baseCheck == FRAME_VALID)
    {
        base_packet_wire_t basePacket = {};
        memcpy(&basePacket, &g_rxBuffer[4], sizeof(basePacket));
        basePacketToNative(basePacket, packet);
        format = {sizeof(base_packet_wire_t), BASE_FRAME_SIZE, true,
                  g_bufferReadableMarkers, g_bufferBigEndianCrc};
        consumeFrame(BASE_FRAME_SIZE);
        return true;
    }

    // Совместимость с базой, пересобранной на core 3.x с native time_t.
    const FrameCheck nativeCheck = checkBufferedFrame(sizeof(full_packet_net_t));
    if (nativeCheck == FRAME_VALID)
    {
        memcpy(&packet, &g_rxBuffer[4], sizeof(packet));
        format = {sizeof(full_packet_net_t), NATIVE_FRAME_SIZE, false,
                  g_bufferReadableMarkers, g_bufferBigEndianCrc};
        consumeFrame(NATIVE_FRAME_SIZE);
        return true;
    }

    const size_t maximumFrame = BASE_FRAME_SIZE > NATIVE_FRAME_SIZE ?
                                BASE_FRAME_SIZE : NATIVE_FRAME_SIZE;
    if (g_rxBuffered < maximumFrame) return false;
    if (baseCheck == FRAME_CRC_ERROR || nativeCheck == FRAME_CRC_ERROR) ++g_crcErrors;
    else ++g_frameErrors;
    memmove(g_rxBuffer, g_rxBuffer + 1, --g_rxBuffered);
    return false;
}

void queueDecodedPacket(const full_packet_net_t& packet, const WireFormat& format)
{
    if (g_packetMutex) xSemaphoreTake(g_packetMutex, portMAX_DELAY);
    g_queuedPacket = packet;
    g_queuedFormat = format;
    g_packetQueued = true;
    if (g_packetMutex) xSemaphoreGive(g_packetMutex);
}

bool takeDecodedPacket(full_packet_net_t& packet, WireFormat& format)
{
    bool available = false;
    if (g_packetMutex) xSemaphoreTake(g_packetMutex, portMAX_DELAY);
    if (g_packetQueued)
    {
        packet = g_queuedPacket;
        format = g_queuedFormat;
        g_packetQueued = false;
        available = true;
    }
    if (g_packetMutex) xSemaphoreGive(g_packetMutex);
    return available;
}

void netToLocalThis(const ufo_net_t& source)
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

void netToLocalContainer(const ufo_net_t& source, ufo_t& target)
{
    if (source.addr == 0U)
    {
        target = EmptyFO;
        return;
    }
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
    target.valid = true;
}

void applyPacket(const full_packet_net_t& packet)
{
    g_lastPacket = packet;
    netToLocalThis(packet.ThisAircraft);
    for (int i = 0; i < MAX_TRACKING_OBJECTS; ++i)
        netToLocalContainer(packet.Container[i], Container[i]);

    if (g_stateMutex) xSemaphoreTake(g_stateMutex, portMAX_DELAY);
    g_inAux = packet.AuxData;
    if (packet.AuxData.new_button_M != 0U) g_inButton = packet.AuxData.new_button_M;
    if (g_stateMutex) xSemaphoreGive(g_stateMutex);

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

void sendReply()
{
    aux_t outgoing = {};
    if (g_stateMutex) xSemaphoreTake(g_stateMutex, portMAX_DELAY);
    outgoing = g_outAux;
    outgoing.new_button_M = g_localButton;
    g_localButton = 0;
    if (g_stateMutex) xSemaphoreGive(g_stateMutex);

    full_packet_net_t nativeReply = {};
    nativeReply.AuxData = outgoing;
    base_packet_wire_t baseReply = {};
    baseReply.AuxData = outgoing;
    const uint8_t* payload = g_lastFormat.baseTime32 ?
                             (const uint8_t*)&baseReply :
                             (const uint8_t*)&nativeReply;
    const size_t payloadSize = g_lastFormat.baseTime32 ?
                               sizeof(baseReply) : sizeof(nativeReply);
    const uint16_t crc = crc16Ccitt(payload, payloadSize);
    const uint8_t crcBytes[2] = {
        (uint8_t)(g_lastFormat.bigEndianCrc ? (crc >> 8) : crc),
        (uint8_t)(g_lastFormat.bigEndianCrc ? crc : (crc >> 8))
    };
    const uint8_t readableHeader[4] = {0xAA, 0xBB, 0xCC, 0xDD};
    const uint8_t readableFooter[4] = {0xDD, 0xCC, 0xBB, 0xAA};

    if (g_serialMutex) xSemaphoreTake(g_serialMutex, portMAX_DELAY);
    rs485SetTx(true);
    if (g_lastFormat.readableMarkers)
        RS485_SERIAL.write(readableHeader, sizeof(readableHeader));
    else
        RS485_SERIAL.write((const uint8_t*)&PACKET_HEADER, sizeof(PACKET_HEADER));
    RS485_SERIAL.write(payload, payloadSize);
    RS485_SERIAL.write(crcBytes, sizeof(crcBytes));
    if (g_lastFormat.readableMarkers)
        RS485_SERIAL.write(readableFooter, sizeof(readableFooter));
    else
        RS485_SERIAL.write((const uint8_t*)&PACKET_FOOTER, sizeof(PACKET_FOOTER));
    RS485_SERIAL.flush();
    delayMicroseconds(200);
    rs485SetTx(false);
    if (g_serialMutex) xSemaphoreGive(g_serialMutex);
    ++g_txPackets;
}

void printRecentBytes()
{
    const uint8_t first = (g_recentRxPosition + sizeof(g_recentRxBytes) -
                           g_recentRxCount) % sizeof(g_recentRxBytes);
    for (uint8_t index = 0; index < g_recentRxCount; ++index)
    {
        const uint8_t value = g_recentRxBytes[(first + index) % sizeof(g_recentRxBytes)];
        char text[4];
        snprintf(text, sizeof(text), "%02X", (unsigned)value);
        Serial.print(text);
        if (index + 1U < g_recentRxCount) Serial.print(' ');
    }
}
} // namespace

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
    WireFormat format = {};
    if (!decodeBufferedPacket(*packet, format)) return false;
    g_lastFormat = format;
    if (button1) *button1 = packet->BUTTON1;
    if (button2) *button2 = packet->BUTTON2;
    return true;
}

void rxTask(void*)
{
    for (;;)
    {
        for (uint8_t count = 0; count < 4U; ++count)
        {
            full_packet_net_t packet = {};
            WireFormat format = {};
            if (!decodeBufferedPacket(packet, format)) break;
            queueDecodedPacket(packet, format);
        }
        vTaskDelay(pdMS_TO_TICKS(RX_TASK_PERIOD_MS));
    }
}

void RS485Display_setup()
{
    if (!g_serialMutex) g_serialMutex = xSemaphoreCreateMutex();
    if (!g_packetMutex) g_packetMutex = xSemaphoreCreateMutex();
    if (!g_stateMutex) g_stateMutex = xSemaphoreCreateMutex();
    pinMode(RS485_DE_PIN, OUTPUT);
    rs485SetTx(false);
    // Один полный кадр базы занимает 1233 байта. 2048 байт достаточно для
    // приёма кадра с запасом и экономит DMA-SRAM для RGB bounce-буферов.
    RS485_SERIAL.setRxBufferSize(2048);
    RS485_SERIAL.setTxBufferSize(1024);
    RS485_SERIAL.begin(RS485_BAUD, RS485_CONFIG, RS485_RX_PIN, RS485_TX_PIN);

    Serial.print(F("[RS485] display pins RX/TX/DE="));
    Serial.print(RS485_RX_PIN); Serial.print('/');
    Serial.print(RS485_TX_PIN); Serial.print('/');
    Serial.print(RS485_DE_PIN); Serial.print(F(", baud="));
    Serial.println(RS485_BAUD);
    Serial.print(F("[RS485] FlyRf_Base_26_07_17_00 payload/frame="));
    Serial.print(sizeof(base_packet_wire_t)); Serial.print('/');
    Serial.println(BASE_FRAME_SIZE);
    Serial.println(F("[RS485] Base setting required: RS485 output = External display (value 3)"));

    if (!g_rxTask)
        xTaskCreatePinnedToCore(rxTask, "RS485_RX", 8192, nullptr, 2, &g_rxTask, 0);
}

void RS485Display_loop()
{
    full_packet_net_t packet = {};
    WireFormat format = {};
    const bool received = takeDecodedPacket(packet, format);
    if (received)
    {
        g_lastFormat = format;
        applyPacket(packet);
        Serial.print(F("[RS485] RX OK payload/frame="));
        Serial.print(format.payloadSize); Serial.print('/');
        Serial.print(format.frameSize);
        Serial.print(format.baseTime32 ? F(", BASE core2 time32") : F(", native time_t"));
        Serial.print(format.readableMarkers ? F(", header=AA BB CC DD") :
                                             F(", header=DD CC BB AA"));
        Serial.print(format.bigEndianCrc ? F(", CRC=BE, #") : F(", CRC=LE, #"));
        Serial.println(g_rxPackets);
        if (g_localButton != 0 || g_outAux.confirm_message_M) sendReply();
    }

    const uint32_t now = millis();
    if (!received && (g_lastDiagnosticMs == 0U ||
        (uint32_t)(now - g_lastDiagnosticMs) >= 2000UL))
    {
        g_lastDiagnosticMs = now;
        Serial.print(F("[RS485] waiting bytes/buffer/headers/footer/crc="));
        Serial.print(g_rxBytes); Serial.print('/');
        Serial.print(g_rxBuffered); Serial.print('/');
        Serial.print(g_headerMatches); Serial.print('/');
        Serial.print(g_frameErrors); Serial.print('/');
        Serial.print(g_crcErrors);
        if (g_rxBytes > 200U && g_headerMatches == 0U &&
            ((uint64_t)g_printableBytes * 100ULL / g_rxBytes) > 85ULL)
            Serial.print(F(" WARNING: text stream; set Base RS485 mode to External display"));
        else if (g_rxBytes > 500U && g_headerMatches == 0U &&
                 ((uint64_t)g_ffBytes * 100ULL / g_rxBytes) > 20ULL)
            Serial.print(F(" WARNING: likely inverted RS485; check/swap A and B"));
        Serial.print(F(" recent="));
        printRecentBytes();
        Serial.println();
    }
}

void RS485Display_fini()
{
    if (g_rxTask)
    {
        vTaskDelete(g_rxTask);
        g_rxTask = nullptr;
    }
    if (g_serialMutex) xSemaphoreTake(g_serialMutex, portMAX_DELAY);
    RS485_SERIAL.flush();
    RS485_SERIAL.end();
    if (g_serialMutex) xSemaphoreGive(g_serialMutex);
}

void RS485Display_setOutgoingAux(const aux_t* auxiliary)
{
    if (!auxiliary) return;
    if (g_stateMutex) xSemaphoreTake(g_stateMutex, portMAX_DELAY);
    g_outAux = *auxiliary;
    if (g_stateMutex) xSemaphoreGive(g_stateMutex);
}

void RS485Display_getOutgoingAux(aux_t* auxiliary)
{
    if (!auxiliary) return;
    if (g_stateMutex) xSemaphoreTake(g_stateMutex, portMAX_DELAY);
    *auxiliary = g_outAux;
    if (g_stateMutex) xSemaphoreGive(g_stateMutex);
}

void RS485Display_getIncomingAux(aux_t* auxiliary, uint8_t* button1, uint8_t* button2)
{
    if (g_stateMutex) xSemaphoreTake(g_stateMutex, portMAX_DELAY);
    if (auxiliary) *auxiliary = g_inAux;
    if (button1) *button1 = g_lastPacket.BUTTON1;
    if (button2) *button2 = g_lastPacket.BUTTON2;
    if (g_stateMutex) xSemaphoreGive(g_stateMutex);
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
size_t RS485Display_payloadSize() { return sizeof(base_packet_wire_t); }
size_t RS485Display_frameSize() { return BASE_FRAME_SIZE; }
