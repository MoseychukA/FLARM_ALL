#include "RP2040Bridge.h"
#include "TrafficDB.h"
#include "Log.h"
#include "EEPROMRF.h"
#include "NMEA.h"
#include <string.h>
#include <esp_task_wdt.h>

static RP2040BridgeDiag g_diag = {};
static uint8_t g_rxBuffer[RP2040BRIDGE_PACKET_SIZE] = {0};
static size_t g_rxIndex = 0;
static uint16_t g_gainValues[RP2040BRIDGE_GAIN_VALUES_MAX] = {0U};
static uint8_t g_gainCount = 1U;
static TaskHandle_t g_rp2040Task = nullptr;

static uint32_t swap32(uint32_t val)
{
    return ((val & 0x000000FFUL) << 24) |
           ((val & 0x0000FF00UL) << 8)  |
           ((val & 0x00FF0000UL) >> 8)  |
           ((val & 0xFF000000UL) >> 24);
}

static float swapFloat(float val)
{
    uint32_t temp = 0;
    memcpy(&temp, &val, sizeof(temp));
    temp = swap32(temp);
    float out = 0.0f;
    memcpy(&out, &temp, sizeof(out));
    return out;
}

static uint16_t swap16(uint16_t val)
{
    return (uint16_t)(((val & 0x00FFU) << 8) | ((val & 0xFF00U) >> 8));
}

__attribute__((weak)) bool unpack_ToDUMP1090(const ToDUMP1090_RAW* inRaw, ToDUMP1090* packet)
{
    if (inRaw == nullptr || packet == nullptr)
    {
        return false;
    }

    memset(packet, 0, sizeof(*packet));
    packet->addr = swap32(inRaw->addr);
    packet->squawk = swap16(inRaw->squawk);
    memcpy(packet->callsign, inRaw->callsign, sizeof(packet->callsign));
    packet->altitude = (int32_t)swap32((uint32_t)inRaw->altitude);
    packet->speed = (int32_t)swap32((uint32_t)inRaw->speed);
    packet->course = (int32_t)swap32((uint32_t)inRaw->course);
    packet->vert_rate = (int32_t)swap32((uint32_t)inRaw->vert_rate);
    packet->lat_msg = swapFloat(inRaw->lat_msg);
    packet->lon_msg = swapFloat(inRaw->lon_msg);
    packet->rssi_rp2040 = inRaw->rssi_rp2040;
    return true;
}



static bool isRP2040SerialEnabled()
{
    return (settings != nullptr && settings->serial_out == OUTPUT_MODE_RP2040);
}

static void printRP2040PacketToSerial(const ToDUMP1090& packet)
{
    if (!isRP2040SerialEnabled())
    {
        return;
    }

    NMEA_announceSerialModeIfNeeded();

    char callsign[9] = {};
    memcpy(callsign, packet.callsign, 8);
    callsign[8] = 0;
    Serial.printf("%06X:%d:%8s:%.0f:%.0f:%.0f:%.0f:%d:%.6f:%.6f:%d:%d:%d\r\n",
                  (unsigned)(packet.addr & 0xFFFFFFUL),
                  (int)packet.squawk,
                  callsign,
                  (double)(packet.altitude * 0.3048f),
                  (double)(packet.altitude * 0.3048f),
                  (double)packet.speed,
                  (double)packet.course,
                  (int)packet.vert_rate,
                  (double)packet.lat_msg,
                  (double)packet.lon_msg,
                  0,
                  (int)packet.rssi_rp2040,
                  1);
    vTaskDelay(pdMS_TO_TICKS(4));
}

static void printRawPacketToSerial(const uint8_t* data, size_t len, const char* prefix)
{
    if (!isRP2040SerialEnabled() || data == nullptr || len == 0)
    {
        return;
    }

    NMEA_announceSerialModeIfNeeded();

    if (prefix && prefix[0] != 0)
    {
        Serial.print(prefix);
    }

    for (size_t i = 0; i < len; ++i)
    {
        if (data[i] < 16U) Serial.print('0');
        Serial.print(data[i], HEX);
        if (i + 1U < len) Serial.print(' ');
    }
    Serial.print("\r\n");
    vTaskDelay(pdMS_TO_TICKS(4));
}

static void pushDecodedPacketToContainer(const ToDUMP1090& packet)
{
    fo = EmptyFO;
    fo.addr = packet.addr & 0xFFFFFFUL;
    fo.squawk = packet.squawk;
    memcpy((char*)fo.callsign, packet.callsign, 8);
    fo.altitude = packet.altitude * 0.3048f;
    fo.pressure_altitude = packet.altitude * 0.3048f;
    fo.speed = (float)packet.speed;
    fo.course = (float)packet.course;
    fo.vert_rate = packet.vert_rate;
    fo.latitude = packet.lat_msg;
    fo.longitude = packet.lon_msg;
    fo.rssi_rp2040 = packet.rssi_rp2040;
    fo.signal_source = 2;
    fo.aircraft_type = 1;
    fo.timestamp = (time_t)(millis() / 1000UL);
    fo.timemsg = fo.timestamp;
    Traffic_Update(&fo);
    Traffic_Add(&fo);
}

void RP2040Bridge_setGainValues(const uint16_t* data, uint8_t count)
{
    if (data == nullptr || count == 0U)
    {
        return;
    }

    if (count > RP2040BRIDGE_GAIN_VALUES_MAX)
    {
        count = RP2040BRIDGE_GAIN_VALUES_MAX;
    }

    memcpy(g_gainValues, data, (size_t)count * sizeof(uint16_t));
    g_gainCount = count;
}

void RP2040Bridge_sendGainNow()
{
    RP2040BRIDGE_STREAM.write((uint8_t)START_MARK);
    RP2040BRIDGE_STREAM.write((uint8_t)g_gainCount);

    for (uint8_t i = 0; i < g_gainCount; ++i)
    {
        RP2040BRIDGE_STREAM.write(highByte(g_gainValues[i]));
        RP2040BRIDGE_STREAM.write(lowByte(g_gainValues[i]));
    }

    RP2040BRIDGE_STREAM.write(highByte(END_MARK));
    RP2040BRIDGE_STREAM.write(lowByte(END_MARK));
    RP2040BRIDGE_STREAM.flush();
    ++g_diag.gainPacketsSent;
}

void RP2040Bridge_sendGainSingle(uint16_t gain)
{
    RP2040Bridge_setGainValues(&gain, 1U);
    RP2040Bridge_sendGainNow();
}

bool RP2040Bridge_processPacket(const uint8_t* data, size_t len)
{
    ++g_diag.packetsReceived;

    if (data == nullptr || len != sizeof(ToDUMP1090_RAW))
    {
        ++g_diag.packetsRejected;
        printRawPacketToSerial(data, len, "RP2040:SIZE_ERR:");
        return false;
    }

    if (data[len - 3] != 0xFF || data[len - 2] != 0xFF || data[len - 1] != 0xFF)
    {
        ++g_diag.packetsRejected;
        ++g_diag.badTailCount;
        printRawPacketToSerial(data, len, "RP2040:TAIL_ERR:");
        return false;
    }

    ToDUMP1090_RAW inRaw = {};
    ToDUMP1090 packet = {};
    memcpy(&inRaw, data, sizeof(inRaw));

    if (!unpack_ToDUMP1090(&inRaw, &packet))
    {
        ++g_diag.packetsRejected;
        ++g_diag.unpackFailedCount;
        printRawPacketToSerial(data, len, "RP2040:UNPACK_ERR:");
        return false;
    }

    printRP2040PacketToSerial(packet);

    pushDecodedPacketToContainer(packet);
    ++g_diag.packetsAccepted;
    g_diag.lastPacketMs = millis();
    g_diag.lastAddress = (packet.addr & 0xFFFFFFUL);
    g_diag.lastRssi = (int)packet.rssi_rp2040;
    g_diag.lastPacketSize = (uint8_t)len;
    return true;
}

void receiveRP2040(void* param)
{
    (void)param;
    esp_task_wdt_add(NULL);
    g_diag.taskRunning = true;

    for (;;)
    {
        esp_task_wdt_reset();

        while (RP2040BRIDGE_STREAM.available())
        {
            const int rd = RP2040BRIDGE_STREAM.read();
            if (rd < 0)
            {
                break;
            }

            const uint8_t b = (uint8_t)rd;
            ++g_diag.bytesReceived;

            if (g_rxIndex < sizeof(g_rxBuffer))
            {
                g_rxBuffer[g_rxIndex++] = b;
            }
            else
            {
                ++g_diag.overflowCount;
                g_rxIndex = 0;
                memset(g_rxBuffer, 0, sizeof(g_rxBuffer));
                continue;
            }

            if (g_rxIndex >= sizeof(ToDUMP1090_RAW))
            {
                RP2040Bridge_processPacket(g_rxBuffer, sizeof(ToDUMP1090_RAW));
                memset(g_rxBuffer, 0, sizeof(g_rxBuffer));
                g_rxIndex = 0;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void RP2040Bridge_setup()
{
    memset(&g_diag, 0, sizeof(g_diag));
    g_rxIndex = 0U;
    RP2040BRIDGE_STREAM.begin(SERIAL_RP2040_SPEED, SERIAL_IN_BITS, SOC_GPIO_PIN_RP2040_RX, SOC_GPIO_PIN_RP2040_TX);
    RP2040BRIDGE_STREAM.setRxBufferSize(RP2040BRIDGE_RX_BUFFER_SIZE);
    g_diag.ready = true;

    uint16_t configuredGain = RP2040BRIDGE_DEFAULT_GAIN;
    if (settings != nullptr && settings->threshold_level > 0)
    {
        configuredGain = (uint16_t)settings->threshold_level;
    }
    RP2040Bridge_setGainValues(&configuredGain, 1U);

    if (g_rp2040Task == nullptr)
    {
        const BaseType_t rc = xTaskCreatePinnedToCore(receiveRP2040, "RP2040", 8192, NULL, 2, &g_rp2040Task, 0);
        if (rc != pdPASS)
        {
            ++g_diag.restartCount;
            Serial.println(F("[RP2040] task create failed"));
        }
    }

}

void RP2040Bridge_loop()
{
    static uint32_t lastGainMs = 0;
    const uint32_t now = millis();
    if (g_diag.ready && (now - lastGainMs) > 1000UL && g_diag.gainPacketsSent == 0UL)
    {
        lastGainMs = now;
        RP2040Bridge_sendGainNow();
    }
}

bool RP2040Bridge_getDiag(RP2040BridgeDiag& outDiag)
{
    outDiag = g_diag;
    return g_diag.ready;
}
