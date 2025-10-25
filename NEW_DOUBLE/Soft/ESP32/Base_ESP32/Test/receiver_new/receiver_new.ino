#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// RS485 настройки
#define RS485_SERIAL   Serial1
#define RS485_TX_PIN   39
#define RS485_RX_PIN   38
#define RS485_DE_PIN   40
//#define RS485_BAUD     921600
#define RS485_BAUD 115200
//#define RS485_BAUD     250000
#define RS485_CONFIG   SERIAL_8N1
#define LED            4

#define BTN1_PIN   45  // Используйте реальный рабочий пин!
#define BTN2_PIN   18  // Используйте реальный рабочий пин!

const uint32_t PACKET_HEADER = 0xAABBCCDD;
const uint32_t PACKET_FOOTER = 0xDDCCBBAA;

typedef struct UFO {
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

#define MAX_TRACKING_OBJECTS 12

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
    ufo_t   ThisAircraft;
    ufo_t   Container[MAX_TRACKING_OBJECTS];
    aux_t   AuxData;
    uint8_t BUTTON1;
    uint8_t BUTTON2;
} full_packet_t;

// FreeRTOS объекты
SemaphoreHandle_t serialMutex;
SemaphoreHandle_t containerMutex;

// Прототипы
uint16_t crc16_ccitt(const uint8_t* data, size_t len);
void sendPacket_RS485(const full_packet_t* pkt);
bool receivePacket_RS485(full_packet_t* pkt);

// Глобальные данные
ufo_t ThisAircraft;
ufo_t Container[MAX_TRACKING_OBJECTS];
aux_t AuxData;
volatile bool dataReceivedFlag = false;
uint8_t BUTTON1 = 0, BUTTON2 = 0;

// Для ответа
volatile bool needSendReply = false;
full_packet_t replyPacket;

// Инициализация RS485
void setupRS485() 
{
    RS485_SERIAL.setTxBufferSize(1024); // до вызова begin()
    RS485_SERIAL.begin(RS485_BAUD, RS485_CONFIG, RS485_RX_PIN, RS485_TX_PIN);
    pinMode(RS485_DE_PIN, OUTPUT);
    digitalWrite(RS485_DE_PIN, LOW);
}

// CRC
uint16_t crc16_ccitt(const uint8_t* data, size_t len) {
    uint16_t crc = 0x0000;
    for (size_t i = 0; i < len; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; ++j)
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
    }
    return crc;
}

// Передача ответа
void sendPacket_RS485(const full_packet_t* pkt) 
{
    const size_t plen = sizeof(full_packet_t);
    uint8_t buf[plen];
    memcpy(buf, pkt, plen);
    uint16_t crc = crc16_ccitt(buf, plen);

    //Serial.print("Отправка пакета, полный размер: ");
    //Serial.println(plen + 8); // 4+plen+2+4
    //Serial.print("HEADER: ");
    //uint8_t* h = (uint8_t*)&PACKET_HEADER;
    //Serial.printf("%02X %02X %02X %02X\r\n", h[0], h[1], h[2], h[3]);
    //Serial.print("Первый блок пакета: ");
    //for (int i = 0; i < 8; ++i) Serial.printf("%02X ", buf[i]);
    //Serial.println();
    //Serial.print("CRC: ");
    //Serial.printf("%02X %02X\r\n", ((uint8_t*)&crc)[0], ((uint8_t*)&crc)[1]);
    //Serial.print("FOOTER: ");
    //h = (uint8_t*)&PACKET_FOOTER;
    //Serial.printf("%02X %02X %02X %02X\r\n", h[0], h[1], h[2], h[3]);

    xSemaphoreTake(serialMutex, portMAX_DELAY);
    digitalWrite(RS485_DE_PIN, HIGH);
    delay(25); // Задержка! (важна на MAX скорости)
    RS485_SERIAL.write((uint8_t*)&PACKET_HEADER, sizeof(PACKET_HEADER));
    RS485_SERIAL.write(buf, plen);
    RS485_SERIAL.write((uint8_t*)&crc, sizeof(crc));
    RS485_SERIAL.write((uint8_t*)&PACKET_FOOTER, sizeof(PACKET_FOOTER));
    RS485_SERIAL.flush();
    delay(25); // Задержка! (важна на MAX скорости)
    digitalWrite(RS485_DE_PIN, LOW);
    xSemaphoreGive(serialMutex);
}



bool receivePacket_RS485(full_packet_t * pkt)
{
    static uint8_t buffer[sizeof(full_packet_t) + 32];
    static size_t idx = 0;

    // Прочитать все накопившиеся байты из UART
    while (RS485_SERIAL.available()) {
        int bytes = RS485_SERIAL.available();
        // Переполнения буфера не будет
        if (bytes + idx > sizeof(buffer)) bytes = sizeof(buffer) - idx;
        int n = RS485_SERIAL.readBytes(&buffer[idx], bytes);
        idx += n;
    }
   // Serial.print("IDX: "); Serial.println(idx);
    //// Отладка: дамп начала буфера (первые 24 байта + последние 8)
    //if (idx > 0) 
    //{
    //    Serial.print("HEX DUMP START: ");
    //    for (size_t i = 0; i < 24 && i < idx; ++i) Serial.printf("%02X ", buffer[i]);
    //    Serial.println();
    //    // Для отладки можно вывести и конец буфера — так увидим footer
    //    if (idx > (sizeof(full_packet_t) + 4 + 2 + 4 - 8)) 
    //    {
    //        Serial.print("HEX DUMP END: ");
    //        for (size_t i = idx - 8; i < idx; ++i) Serial.printf("%02X ", buffer[i]);
    //        Serial.println();
    //    }
    //}

    // Проверка наличия полного пакета
    while (idx >= sizeof(full_packet_t) + 8) // header(4)+packet+crc(2)+footer(4)
    {
        // Проверяем little endian header (0xAABBCCDD → DD CC BB AA)
        bool header_ok = (buffer[0] == 0xDD && buffer[1] == 0xCC && buffer[2] == 0xBB && buffer[3] == 0xAA);
        size_t footer_off = sizeof(full_packet_t) + 4 + 2;
        // Проверяем footer — должен быть (0xDDCCBBAA → AA BB CC DD)
        bool footer_ok = (buffer[footer_off] == 0xAA && buffer[footer_off + 1] == 0xBB &&
            buffer[footer_off + 2] == 0xCC && buffer[footer_off + 3] == 0xDD);

        //Serial.print("HEADER OK: "); Serial.println(header_ok ? "YES" : "NO");
        //Serial.print("FOOTER OK: "); Serial.println(footer_ok ? "YES" : "NO");

        if (header_ok && footer_ok) {
            // CRC и содержимое
            uint8_t* data = &buffer[4];
            uint16_t crc_rx = *(uint16_t*)&buffer[4 + sizeof(full_packet_t)];
            uint16_t crc_calc = crc16_ccitt(data, sizeof(full_packet_t));
            //Serial.print("CRC RX: "); Serial.println(crc_rx, HEX);
            //Serial.print("CRC CALC: "); Serial.println(crc_calc, HEX);

            if (crc_rx == crc_calc) {
                memcpy(pkt, data, sizeof(full_packet_t));
                //Serial.println("=== Packet accepted! ===");
                //// Распечатать важные поля (покажи кнопки и что-то по содержимому)
                //Serial.print("BUTTON1: "); Serial.println(pkt->BUTTON1);
                //Serial.print("BUTTON2: "); Serial.println(pkt->BUTTON2);
                //Serial.print("ThisAircraft.lat: "); Serial.println(pkt->ThisAircraft.latitude, 8);
                //Serial.print("AuxData.Time_Hour_M: "); Serial.println(pkt->AuxData.Time_Hour_M);
                //// и т.д. по желанию

                // Сдвигаем буфер: вдруг следом следующий пакет!
                size_t msgLen = sizeof(full_packet_t) + 8;
                idx -= msgLen;
                if (idx)
                    memmove(buffer, buffer + msgLen, idx);
                else
                    idx = 0;
                return true;
            }
            else {
                Serial.println("!!! CRC ERROR !!!");
            }
        } // if header/footer

        // Если не нашли header/footer/CRC — смещаем буфер на 1 байт
        memmove(buffer, buffer + 1, --idx);
    }
    return false;
}




// Опрос кнопок
void buttonsTask(void* param) 
{
    for (;;)
    {
        BUTTON1 = digitalRead(BTN1_PIN);
        BUTTON2 = digitalRead(BTN2_PIN);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// Прием и формирование ответа (теперь только после запроса мастер)
void rxTask(void* param) {
    full_packet_t packet;
    for (;;) {
        if (receivePacket_RS485(&packet)) {
            xSemaphoreTake(containerMutex, portMAX_DELAY);
            // Вот здесь обновляются твои переменные!
            memcpy(&ThisAircraft, &packet.ThisAircraft, sizeof(ufo_t));
            memcpy(&Container, &packet.Container, sizeof(Container));
            memcpy(&AuxData, &packet.AuxData, sizeof(aux_t));
            xSemaphoreGive(containerMutex);

            // Если нужны кнопки:
            uint8_t btn1 = packet.BUTTON1;
            uint8_t btn2 = packet.BUTTON2;

            //// Готовим ответ
            //memcpy(&replyPacket.ThisAircraft, &ThisAircraft, sizeof(ufo_t));
            //memcpy(&replyPacket.Container, &Container, sizeof(Container));
            //memcpy(&replyPacket.AuxData, &AuxData, sizeof(aux_t));
            replyPacket.BUTTON1 = BUTTON1;
            replyPacket.BUTTON2 = BUTTON2;
            needSendReply = true;
           // xSemaphoreGive(containerMutex);
            dataReceivedFlag = true;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// Ответ только по запросу!
void replyTask(void* param) 
{
    for (;;) 
    {
        if (needSendReply) 
        {
            sendPacket_RS485(&replyPacket);
            needSendReply = false;
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

void printAircraft(const char* prefix, const ufo_t& ac)
{
    Serial.print(prefix); Serial.print(":");
    Serial.print(":"); Serial.print(ac.addr);
    Serial.print(":"); Serial.print(ac.squawk);
    Serial.print(":");
    for (int i = 0; i < 8; ++i) Serial.print((char)ac.callsign[i]);
    Serial.print(":"); Serial.print(ac.altitude);
    Serial.print(":"); Serial.print(ac.pressure_altitude);
    Serial.print(":"); Serial.print(ac.course);
    Serial.print(":"); Serial.print(ac.speed);
    Serial.print(":"); Serial.print(ac.vert_rate);
    Serial.print(":"); Serial.print(ac.latitude, 6);
    Serial.print(":"); Serial.print(ac.longitude, 6);
    Serial.print(":"); Serial.print(ac.rssi);
    Serial.print(":"); Serial.print(ac.last_message_signal_strength_dbm);
    Serial.print(":"); Serial.println(ac.last_message_signal_quality_db);
}

//void printAircraft(const char* prefix, const ufo_t& ac) 
//{
//    Serial.print(prefix); Serial.println(":");
//    Serial.print("  Addr: "); Serial.println(ac.addr);
//    Serial.print("  Squawk: "); Serial.println(ac.squawk);
//    Serial.print("  Callsign: ");
//    for (int i = 0; i < 8; ++i) Serial.print((char)ac.callsign[i]);
//    Serial.println();
//    Serial.print("  Altitude: "); Serial.println(ac.altitude);
//    Serial.print("  Pressure Altitude: "); Serial.println(ac.pressure_altitude);
//    Serial.print("  Course: "); Serial.println(ac.course);
//    Serial.print("  Speed: "); Serial.println(ac.speed);
//    Serial.print("  Vert rate: "); Serial.println(ac.vert_rate);
//    Serial.print("  Lat: "); Serial.println(ac.latitude, 7);
//    Serial.print("  Lon: "); Serial.println(ac.longitude, 7);
//    Serial.print("  RSSI: "); Serial.println(ac.rssi);
//    Serial.print("  Signal (dBm): "); Serial.println(ac.last_message_signal_strength_dbm);
//    Serial.print("  Quality (dB): "); Serial.println(ac.last_message_signal_quality_db);
//}

void printAux(const aux_t& aux) 
{
    Serial.println("AUX DATA:");
    Serial.print("  new_flag_M: "); Serial.println(aux.new_flag_M);
    Serial.print("  new_buttton_M: "); Serial.println(aux.new_buttton_M);
    Serial.print("  setMessageRead_M: "); Serial.println(aux.setMessageRead_M);
    Serial.print("  MessageRead_M: "); Serial.println(aux.MessageRead_M);
    Serial.print("  Time_Hour_M: "); Serial.println(aux.Time_Hour_M);
    Serial.print("  Time_Minute_M: "); Serial.println(aux.Time_Minute_M);
    Serial.print("  new_SOS_flag_M: "); Serial.println(aux.new_SOS_flag_M);
    Serial.print("  msg_resp_M: "); Serial.println(aux.msg_resp_M);
    Serial.print("  isValidGNSS_M: "); Serial.println(aux.isValidGNSS_M);
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("Start Receiver");
    pinMode(BTN1_PIN, INPUT);
    pinMode(BTN2_PIN, INPUT);
    pinMode(LED, OUTPUT);
    digitalWrite(LED, HIGH);
    Serial.print("Sizeof full_packet_t: "); Serial.println(sizeof(full_packet_t));

    serialMutex = xSemaphoreCreateMutex();
    containerMutex = xSemaphoreCreateMutex();
    setupRS485();

    xTaskCreatePinnedToCore(rxTask, "RX", 4096, NULL, 2, NULL, 0);
    xTaskCreatePinnedToCore(replyTask, "REP", 2048, NULL, 2, NULL, 0);
    xTaskCreatePinnedToCore(buttonsTask, "BTN", 2048, NULL, 1, NULL, 0);

    Serial.println("Start End");
}

void loop() 
{

    static uint32_t tmr = 0;
    if (millis() - tmr > 1000) 
    { // раз в 1 секунду
        tmr = millis();

        // Защита от одновременного доступа
        xSemaphoreTake(containerMutex, portMAX_DELAY);

        // 1. Собственный самолет
        printAircraft("ThisAircraft", ThisAircraft);
        Serial.println("--------------------------------------------------");
        // 2. Сторонние (tracked) самолеты
        for (int i = 0; i < MAX_TRACKING_OBJECTS; ++i) 
        {
            char buf[32];
            sprintf(buf, "Container[%d]", i);
            printAircraft(buf, Container[i]);
        }
        Serial.println("--------------------------------------------------");
        // 3. Служебные данные
        printAux(AuxData);

        xSemaphoreGive(containerMutex);

        Serial.println("===================================================");
    }

    // Пример управления выходом по кнопкам (если нужно)
    if (BUTTON1 == LOW || BUTTON2 == LOW) 
    {
         digitalWrite(LED, LOW); // если хотите управлять пьезо/светодиодом
    }
    else 
    {
         digitalWrite(LED, HIGH);
    }
}


