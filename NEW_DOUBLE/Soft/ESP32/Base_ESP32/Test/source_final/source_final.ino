#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define MAX_TRACKING_OBJECTS 12
#define RS485_SERIAL   Serial2
#define RS485_TX_PIN   18
#define RS485_RX_PIN   17
#define RS485_DE_PIN   21
#define RS485_BAUD     115200
#define RS485_CONFIG   SERIAL_8N1
#define LED            4

const uint32_t PACKET_HEADER = 0xAABBCCDD;
const uint32_t PACKET_FOOTER = 0xDDCCBBAA;

// Полная структура
typedef struct {
    uint32_t addr;
    int      squawk;
    uint8_t  callsign[8];
    float    altitude;
    float    pressure_altitude;
    float    course;
    float    speed;
    int      vert_rate;
    float    latitude;
    float    longitude;
    int8_t   rssi;
    uint16_t last_message_signal_strength_dbm;
    uint16_t last_message_signal_quality_db;
} ufo_t;


typedef struct __attribute__((packed)) {
    uint32_t addr;
    int      squawk;
    uint8_t  callsign[8];
    float    altitude;
    float    pressure_altitude;
    float    course;
    float    speed;
    int      vert_rate;
    float    latitude;
    float    longitude;
    int8_t   rssi;
    uint16_t last_message_signal_strength_dbm;
    uint16_t last_message_signal_quality_db;
} ufo_net_t;

// Доп служебная структура
typedef struct __attribute__((packed)) {
    bool     new_flag_M;
    uint8_t  new_buttton_M;
    bool     setMessageRead_M;
    bool     MessageRead_M;
    uint8_t  Time_Hour_M;
    uint8_t  Time_Minute_M;
    bool     new_SOS_flag_M;
    char     msg_resp_M[170];
    bool     isValidGNSS_M;
} aux_t;

typedef struct __attribute__((packed)) {
    ufo_net_t ThisAircraft;
    ufo_net_t Container[MAX_TRACKING_OBJECTS];
    aux_t AuxData;
    uint8_t BUTTON1;
    uint8_t BUTTON2;
} full_packet_net_t;

// Данные для передачи
ufo_t ThisAircraft;
ufo_t Container[MAX_TRACKING_OBJECTS];
aux_t AuxData;

SemaphoreHandle_t serialMutex;
SemaphoreHandle_t containerMutex;

// Прототипы
uint16_t crc16_ccitt(const uint8_t* data, size_t len);
void sendPacket_RS485(const full_packet_net_t* pkt);
bool receivePacket_RS485(full_packet_net_t* pkt, uint8_t* btn1, uint8_t* btn2);

void ufo_to_net(const ufo_t *src, ufo_net_t *dst) {
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
    dst->rssi = src->rssi;
    dst->last_message_signal_strength_dbm = src->last_message_signal_strength_dbm;
    dst->last_message_signal_quality_db = src->last_message_signal_quality_db;
}

void setupRS485() {
    RS485_SERIAL.setTxBufferSize(1024);
    RS485_SERIAL.begin(RS485_BAUD, RS485_CONFIG, RS485_RX_PIN, RS485_TX_PIN);
    pinMode(RS485_DE_PIN, OUTPUT);
    digitalWrite(RS485_DE_PIN, LOW);
    pinMode(LED, OUTPUT);
    digitalWrite(LED, HIGH);
}

uint16_t crc16_ccitt(const uint8_t* data, size_t len) {
    uint16_t crc = 0x0000;
    for (size_t i = 0; i < len; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; ++j)
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
    }
    return crc;
}

void sendPacket_RS485(const full_packet_net_t* pkt) {
    const size_t plen = sizeof(full_packet_net_t);
    static uint8_t buf[sizeof(full_packet_net_t)];
    memcpy(buf, pkt, plen);
    uint16_t crc = crc16_ccitt(buf, plen);

    xSemaphoreTake(serialMutex, portMAX_DELAY);
    digitalWrite(RS485_DE_PIN, HIGH);
    digitalWrite(LED, LOW);
    delay(15);
    RS485_SERIAL.write((uint8_t*)&PACKET_HEADER, sizeof(PACKET_HEADER));
    RS485_SERIAL.write(buf, plen);
    RS485_SERIAL.write((uint8_t*)&crc, sizeof(crc));
    RS485_SERIAL.write((uint8_t*)&PACKET_FOOTER, sizeof(PACKET_FOOTER));
    RS485_SERIAL.flush();
    delay(15);
    digitalWrite(RS485_DE_PIN, LOW);
    digitalWrite(LED, HIGH);
    xSemaphoreGive(serialMutex);
    Serial.println("Пакет отправлен!");
}

bool receivePacket_RS485(full_packet_net_t* pkt, uint8_t* btn1, uint8_t* btn2) {
    static uint8_t buffer[sizeof(full_packet_net_t) + 32];
    static size_t idx = 0;
    while (RS485_SERIAL.available()) {
        int bytes = RS485_SERIAL.available();
        if (bytes + idx > sizeof(buffer)) bytes = sizeof(buffer) - idx;
        int n = RS485_SERIAL.readBytes(&buffer[idx], bytes);
        idx += n;
    }
    while (idx >= sizeof(full_packet_net_t) + 8) {
        bool header_ok = (buffer[0] == 0xDD && buffer[1] == 0xCC &&
                          buffer[2] == 0xBB && buffer[3] == 0xAA);
        size_t footer_off = sizeof(full_packet_net_t) + 4 + 2;
        bool footer_ok = (buffer[footer_off] == 0xAA && buffer[footer_off + 1] == 0xBB &&
                          buffer[footer_off + 2] == 0xCC && buffer[footer_off + 3] == 0xDD);

        if (header_ok && footer_ok) {
            uint8_t* data = &buffer[4];
            uint16_t crc_rx = *(uint16_t*)&buffer[4 + sizeof(full_packet_net_t)];
            uint16_t crc_calc = crc16_ccitt(data, sizeof(full_packet_net_t));
            if (crc_rx == crc_calc) {
                memcpy(pkt, data, sizeof(full_packet_net_t));
                if (btn1) *btn1 = pkt->BUTTON1;
                if (btn2) *btn2 = pkt->BUTTON2;
                size_t msgLen = sizeof(full_packet_net_t) + 8;
                idx -= msgLen;
                if (idx)
                    memmove(buffer, buffer + msgLen, idx);
                else
                    idx = 0;
                return true;
            }
        }
        memmove(buffer, buffer + 1, --idx);
    }
    return false;
}

void txTask(void* param) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    full_packet_net_t packet = {};
    static uint8_t count = 0;
    for (;;) {
        xSemaphoreTake(containerMutex, portMAX_DELAY);

        ufo_to_net(&ThisAircraft, &packet.ThisAircraft);
        for (int i = 0; i < MAX_TRACKING_OBJECTS; ++i)
            ufo_to_net(&Container[i], &packet.Container[i]);
        memcpy(&packet.AuxData, &AuxData, sizeof(aux_t));
        packet.BUTTON1 = count;
        packet.BUTTON2 = 0x02;
        count++;

        xSemaphoreGive(containerMutex);

        sendPacket_RS485(&packet);
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(500));
    }
}

void rxTask(void* param) {
    full_packet_net_t recpkt;
    uint8_t btn1, btn2;
    for (;;) {
        if (receivePacket_RS485(&recpkt, &btn1, &btn2)) {
            Serial.print("Button states from Slave: ");
            Serial.print(btn1);
            Serial.print(' ');
            Serial.println(btn2);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("Start Source");
    ThisAircraft.addr = 777777;
    ThisAircraft.squawk = 1200;
    strcpy((char*)ThisAircraft.callsign, "TEST123 ");
    ThisAircraft.altitude = 1015;
    ThisAircraft.latitude = 55.7;
    ThisAircraft.longitude = 37.6;
    for (int i = 0; i < MAX_TRACKING_OBJECTS; ++i) {
        Container[i].addr = 10000 + i;
        sprintf((char*)Container[i].callsign, "OBJ%04d", i);
        Container[i].altitude = 900 + i * 50;
        Container[i].latitude  = 54.0 + i * 0.1;
        Container[i].longitude = 37.0 + i * 0.1;
    }
    serialMutex = xSemaphoreCreateMutex();
    containerMutex = xSemaphoreCreateMutex();
    setupRS485();
    xTaskCreatePinnedToCore(txTask, "TX", 8192, NULL, 2, NULL, 0); 
    xTaskCreatePinnedToCore(rxTask, "RX",  4096, NULL, 1, NULL, 0);
    Serial.print("Sizeof full_packet_net_t: "); Serial.println(sizeof(full_packet_net_t));
    Serial.println("Start End");
}

void loop() {
    delay(1000);
}