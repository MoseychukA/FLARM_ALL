#include "RS485Display.h"
#include "EEPROMRF.h"
#include "DeviceInfo.h"
#include "NMEA.h"
#include <string.h>
#include <esp_task_wdt.h>
#include <freertos/semphr.h>

static aux_t g_auxTx = {};
static aux_t g_auxRx = {};
static uint8_t g_btn1 = 0;
static uint8_t g_btn2 = 0;
static volatile uint8_t g_pendingButton = 0;
static volatile uint32_t g_lastRxTickMs = 0;
static uint32_t previousMillis = 0;
static const uint32_t interval = 1000UL;
static SemaphoreHandle_t rs485Mutex = nullptr;
static SemaphoreHandle_t auxMutex = nullptr;
static TaskHandle_t g_rs485RxTask = nullptr;

static void rs485SetTX(bool enable)
{
    digitalWrite(RS485_DE_PIN, enable ? HIGH : LOW);
    if (enable) delayMicroseconds(50);
}

uint16_t crc16_ccitt(const uint8_t* data, size_t len)
{
    uint16_t crc = 0x0000;
    for (size_t i = 0; i < len; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; ++j)
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
    return crc;
}

void net_to_ufo_Container(const ufo_t* src, ufo_net_t* dst)
{
    if (!src || !dst) return;
    memset(dst, 0, sizeof(*dst));
    dst->addr = src->addr;
    dst->squawk = src->squawk;
    memcpy(dst->callsign, src->callsign, 8);
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

void net_to_ufo_ThisAircraft(const ufo_t* src, ufo_net_t* dst)
{
    if (!src || !dst) return;
    memset(dst, 0, sizeof(*dst));
    dst->addr = src->addr;
    dst->squawk = src->squawk;
    memcpy(dst->callsign, src->callsign, 8);
    dst->altitude = src->altitude;
    dst->pressure_altitude = src->pressure_altitude;
    dst->course = src->course;
    dst->speed = src->speed;
    dst->vert_rate = src->vert_rate;
    dst->latitude = src->latitude;
    dst->longitude = src->longitude;
}

void RS485Display_setOutgoingAux(const aux_t* aux)
{
    if (!aux) return;
    if (auxMutex) xSemaphoreTake(auxMutex, portMAX_DELAY);
    memcpy(&g_auxTx, aux, sizeof(g_auxTx));
    if (auxMutex) xSemaphoreGive(auxMutex);
}

void RS485Display_getOutgoingAux(aux_t* aux)
{
    if (!aux) return;
    if (auxMutex) xSemaphoreTake(auxMutex, portMAX_DELAY);
    memcpy(aux, &g_auxTx, sizeof(g_auxTx));
    if (auxMutex) xSemaphoreGive(auxMutex);
}

void RS485Display_getIncomingAux(aux_t* aux, uint8_t* btn1, uint8_t* btn2)
{
    if (auxMutex) xSemaphoreTake(auxMutex, portMAX_DELAY);
    if (aux) memcpy(aux, &g_auxRx, sizeof(g_auxRx));
    if (btn1) *btn1 = g_btn1;
    if (btn2) *btn2 = g_btn2;
    if (auxMutex) xSemaphoreGive(auxMutex);
}

bool RS485Display_hasIncomingButton()
{
    return g_pendingButton != 0;
}

uint8_t RS485Display_takeIncomingButton()
{
    uint8_t v = g_pendingButton;
    g_pendingButton = 0;
    return v;
}

uint32_t RS485Display_lastRxMs()
{
    return g_lastRxTickMs;
}


size_t RS485Display_payloadSize()
{
    return sizeof(full_packet_net_t);
}

size_t RS485Display_frameSize()
{
    return sizeof(full_packet_net_t) + sizeof(PACKET_HEADER) + sizeof(uint16_t) + sizeof(PACKET_FOOTER);
}

void sendPacket_RS485(const full_packet_net_t* pkt)
{
    if (!pkt) return;
    const size_t plen = sizeof(full_packet_net_t);
    static uint8_t buf[sizeof(full_packet_net_t)];
    memcpy(buf, pkt, plen);
    uint16_t crc = crc16_ccitt(buf, plen);
    if (rs485Mutex) xSemaphoreTake(rs485Mutex, portMAX_DELAY);
    rs485SetTX(true);
    RS485_SERIAL.write((const uint8_t*)&PACKET_HEADER, sizeof(PACKET_HEADER));
    RS485_SERIAL.write(buf, plen);
    RS485_SERIAL.write((const uint8_t*)&crc, sizeof(crc));
    RS485_SERIAL.write((const uint8_t*)&PACKET_FOOTER, sizeof(PACKET_FOOTER));
    RS485_SERIAL.flush();
    delayMicroseconds(200);
    rs485SetTX(false);
    if (rs485Mutex) xSemaphoreGive(rs485Mutex);
}



bool receivePacket_RS485(full_packet_net_t* pkt, uint8_t* btn1, uint8_t* btn2)
{
    if (!pkt) return false;
    static uint8_t buffer[sizeof(full_packet_net_t) + 64];
    static size_t idx = 0;

    const int READ_BUDGET = 256;
    int read_left = READ_BUDGET;
    if (rs485Mutex) xSemaphoreTake(rs485Mutex, portMAX_DELAY);
    while (RS485_SERIAL.available() && read_left > 0)
    {
        int to_read = RS485_SERIAL.available();
        if (to_read > read_left) to_read = read_left;
        if (to_read + (int)idx > (int)sizeof(buffer)) to_read = sizeof(buffer) - idx;
        if (to_read <= 0) break;
        int n = RS485_SERIAL.readBytes(&buffer[idx], to_read);
        idx += (size_t)n;
        read_left -= n;
    }
    if (rs485Mutex) xSemaphoreGive(rs485Mutex);
    if (idx < 4) return false;

    size_t start = 0;
    for (;;)
    {
        if (idx - start < 4)
        {
            if (start > 0)
            {
                memmove(buffer, buffer + start, idx - start);
                idx -= start;
            }
            return false;
        }
        if (buffer[start] == 0xDD && buffer[start + 1] == 0xCC && buffer[start + 2] == 0xBB && buffer[start + 3] == 0xAA)
            break;
        start++;
    }

    if (start > 0)
    {
        memmove(buffer, buffer + start, idx - start);
        idx -= start;
    }

    const size_t frame_len = sizeof(full_packet_net_t) + 10;
    if (idx < frame_len) return false;

    const size_t footer_off = sizeof(full_packet_net_t) + 4 + 2;
    const bool footer_ok =
        (buffer[footer_off] == 0xAA && buffer[footer_off + 1] == 0xBB &&
         buffer[footer_off + 2] == 0xCC && buffer[footer_off + 3] == 0xDD);
    if (!footer_ok)
    {
        idx = 0;
        return false;
    }

    uint8_t* data = &buffer[4];
    uint16_t crc_rx = *(uint16_t*)&buffer[4 + sizeof(full_packet_net_t)];
    uint16_t crc_calc = crc16_ccitt(data, sizeof(full_packet_net_t));
    if (crc_rx != crc_calc)
    {
        idx = 0;
        return false;
    }

    memcpy(pkt, data, sizeof(full_packet_net_t));
    if (btn1) *btn1 = pkt->BUTTON1;
    if (btn2) *btn2 = pkt->BUTTON2;
    idx -= frame_len;
    if (idx > 0) memmove(buffer, buffer + frame_len, idx);
    return true;
}

void rxTask(void* param)
{
    esp_task_wdt_add(NULL);
    static full_packet_net_t recpkt;
    uint8_t btn1 = 0, btn2 = 0;
    for (;;)
    {
        esp_task_wdt_reset();
        if (receivePacket_RS485(&recpkt, &btn1, &btn2))
        {
            if (auxMutex) xSemaphoreTake(auxMutex, portMAX_DELAY);
            memcpy(&g_auxRx, &recpkt.AuxData, sizeof(g_auxRx));
            g_btn1 = btn1;
            g_btn2 = btn2;
            if (auxMutex) xSemaphoreGive(auxMutex);
            g_lastRxTickMs = millis();
            if (recpkt.AuxData.new_button_M != 0)
            {
                g_pendingButton = recpkt.AuxData.new_button_M;
                Serial.printf("[RS485 RX] btn=%u B1=%u B2=%u\r\n",
                              recpkt.AuxData.new_button_M, btn1, btn2);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void RS485Display_setup()
{
    if (rs485Mutex == nullptr) rs485Mutex = xSemaphoreCreateMutex();
    if (auxMutex == nullptr) auxMutex = xSemaphoreCreateMutex();
    RS485_SERIAL.setRxBufferSize(1024);
    RS485_SERIAL.setTxBufferSize(1024);
    RS485_SERIAL.begin(RS485_BAUD, RS485_CONFIG, RS485_RX_PIN, RS485_TX_PIN);
    Serial.printf("[RS485] payload=%u frame=%u aux=%u ufo=%u hdr=%08lX ftr=%08lX\r\n",
                  (unsigned)RS485Display_payloadSize(),
                  (unsigned)RS485Display_frameSize(),
                  (unsigned)sizeof(aux_t),
                  (unsigned)sizeof(ufo_net_t),
                  (unsigned long)PACKET_HEADER,
                  (unsigned long)PACKET_FOOTER);
    pinMode(RS485_DE_PIN, OUTPUT);
    digitalWrite(RS485_DE_PIN, LOW);
    rs485SetTX(false);
    if (g_rs485RxTask == nullptr)
    {
        xTaskCreatePinnedToCore(rxTask, "RX", 8192, NULL, 1, &g_rs485RxTask, 0);
    }
}

void RS485Display_loop()
{
    if (settings == nullptr || settings->rs485_out != OUTPUT_MODE_RS485_DISPLAY) return;
    uint32_t now = millis();
    if (now - previousMillis < interval) return;
    previousMillis = now;

    static full_packet_net_t packet = {};
    memset(&packet, 0, sizeof(packet));
    ufo_t self = EmptyFO;
    self.addr = ThisAircraft.addr;
    self.squawk = ThisAircraft.squawk;
    memcpy(self.callsign, ThisAircraft.callsign, 8);
    self.altitude = ThisAircraft.altitude;
    self.pressure_altitude = ThisAircraft.pressure_altitude;
    self.course = ThisAircraft.course;
    self.speed = ThisAircraft.speed;
    self.vert_rate = ThisAircraft.vert_rate;
    self.latitude = ThisAircraft.latitude;
    self.longitude = ThisAircraft.longitude;
    self.local_latitude = ThisAircraft.local_latitude;
    self.local_longitude = ThisAircraft.local_longitude;
    net_to_ufo_ThisAircraft(&self, &packet.ThisAircraft);
    for (int i = 0; i < MAX_TRACKING_OBJECTS; ++i)
        net_to_ufo_Container(&Container[i], &packet.Container[i]);
    aux_t auxCopy = {};
    RS485Display_getOutgoingAux(&auxCopy);
    memcpy(&packet.AuxData, &auxCopy, sizeof(aux_t));
    packet.BUTTON1 = 0x01;
    packet.BUTTON2 = 0x02;
    sendPacket_RS485(&packet);
    static uint32_t lastDiag = 0;
    if (millis() - lastDiag >= 5000UL)
    {
        lastDiag = millis();
        Serial.printf("[RS485 TX] payload=%u frame=%u aux=%u ufo=%u this=%06lX tracks=%u hdr=%08lX ftr=%08lX\r\n",
                      (unsigned)RS485Display_payloadSize(),
                      (unsigned)RS485Display_frameSize(),
                      (unsigned)sizeof(aux_t),
                      (unsigned)sizeof(ufo_net_t),
                      (unsigned long)packet.ThisAircraft.addr,
                      (unsigned)MAX_TRACKING_OBJECTS,
                      (unsigned long)PACKET_HEADER,
                      (unsigned long)PACKET_FOOTER);
    }
}

void RS485Display_fini()
{
    if (g_rs485RxTask != nullptr)
    {
        vTaskDelete(g_rs485RxTask);
        g_rs485RxTask = nullptr;
    }
    RS485_SERIAL.flush();
    RS485_SERIAL.end();
}
