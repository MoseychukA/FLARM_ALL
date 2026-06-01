/*
  Модуль RS485Display.cpp
  Назначение:
  - Обмен данными с внешним RS485-дисплеем и вспомогательной периферией.

  Основные задачи модуля:
  - Передавать на внешний дисплей данные нашего самолета и Container.
  - Принимать обратные кадры, кнопки и вспомогательные данные.
  - Использовать отдельную задачу приема и синхронизацию доступа к RS485.
*/

#include "RS485Display.h"
#include "EEPROMRF.h"
#include "DeviceInfo.h"
#include "NMEA.h"
#include "RF.h"
#include "GNSS.h"
#include "LANRF.h"
#include <string.h>
#include <esp_task_wdt.h>
#include <freertos/semphr.h>

static aux_t g_auxTx = {};  // Логический флаг состояния: показывает, разрешена ли операция, активен ли режим или есть ли данные.
static aux_t g_auxRx = {};  // Логический флаг состояния: показывает, разрешена ли операция, активен ли режим или есть ли данные.
static uint8_t g_btn1 = 0;  // Логический флаг состояния: показывает, разрешена ли операция, активен ли режим или есть ли данные.
static uint8_t g_btn2 = 0;  // Логический флаг состояния: показывает, разрешена ли операция, активен ли режим или есть ли данные.
static volatile uint8_t g_pendingButton = 0;  // Событие кнопки, полученное от внешнего дисплея.
static volatile uint8_t g_localButtonEvent = 0; // Последнее локальное событие кнопки GPIO45 для передачи внешнему дисплею.
static volatile uint32_t g_lastRxTickMs = 0;  // Логический флаг состояния: показывает, разрешена ли операция, активен ли режим или есть ли данные.
static uint32_t previousMillis = 0;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
static const uint32_t interval = 1000UL;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
static SemaphoreHandle_t rs485Mutex = nullptr;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
static SemaphoreHandle_t auxMutex = nullptr;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
static TaskHandle_t g_rs485RxTask = nullptr;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `rs485SetTX` и обрабатывает канал RS485 передачу в контексте модуля RS485Display.cpp.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
static void rs485SetTX(bool enable)
{
    digitalWrite(RS485_DE_PIN, enable ? HIGH : LOW);
    if (enable) delayMicroseconds(50);
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `crc16_ccitt` и обрабатывает crc16 ccitt в контексте модуля RS485Display.cpp.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
uint16_t crc16_ccitt(const uint8_t* data, size_t len)
{
    uint16_t crc = 0x0000;  // Навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта.
    for (size_t i = 0; i < len; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; ++j)
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
    return crc;
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `net_to_ufo_Container` и обрабатывает net ufo базу целей Container в контексте модуля RS485Display.cpp.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `net_to_ufo_ThisAircraft` и обрабатывает net ufo this самолет в контексте модуля RS485Display.cpp.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `RS485Display_setOutgoingAux` и обрабатывает канал RS485 outgoing aux в контексте модуля RS485Display.cpp.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
void RS485Display_setOutgoingAux(const aux_t* aux)
{
    if (!aux) return;
    if (auxMutex) xSemaphoreTake(auxMutex, portMAX_DELAY);
    memcpy(&g_auxTx, aux, sizeof(g_auxTx));
    if (auxMutex) xSemaphoreGive(auxMutex);
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `RS485Display_getOutgoingAux` и обрабатывает канал RS485 outgoing aux в контексте модуля RS485Display.cpp.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
void RS485Display_getOutgoingAux(aux_t* aux)
{
    if (!aux) return;
    if (auxMutex) xSemaphoreTake(auxMutex, portMAX_DELAY);
    memcpy(aux, &g_auxTx, sizeof(g_auxTx));
    if (auxMutex) xSemaphoreGive(auxMutex);
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `RS485Display_getIncomingAux` и обрабатывает канал RS485 incoming aux в контексте модуля RS485Display.cpp.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
void RS485Display_getIncomingAux(aux_t* aux, uint8_t* btn1, uint8_t* btn2)
{
    if (auxMutex) xSemaphoreTake(auxMutex, portMAX_DELAY);
    if (aux) memcpy(aux, &g_auxRx, sizeof(g_auxRx));
    if (btn1) *btn1 = g_btn1;
    if (btn2) *btn2 = g_btn2;
    if (auxMutex) xSemaphoreGive(auxMutex);
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `RS485Display_hasIncomingButton` и обрабатывает канал RS485 incoming кнопку в контексте модуля RS485Display.cpp.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
bool RS485Display_hasIncomingButton()
{
    return g_pendingButton != 0;
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `RS485Display_takeIncomingButton` и обрабатывает канал RS485 take incoming кнопку в контексте модуля RS485Display.cpp.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
uint8_t RS485Display_takeIncomingButton()
{
    uint8_t v = g_pendingButton;  // Логический флаг состояния: показывает, разрешена ли операция, активен ли режим или есть ли данные.
    g_pendingButton = 0;
    return v;
}

//------------------------------------------------------------------------------
// Назначение функции: Запоминает локальное событие кнопки GPIO45 для ближайшего пакета внешнему дисплею.
// Код события: 1 - одиночное, 2 - двойное, 3 - длительное нажатие.
//------------------------------------------------------------------------------
void RS485Display_setLocalButtonEvent(uint8_t event)
{
    if (event == 0 || event > 3) return;
    g_localButtonEvent = event;
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `RS485Display_lastRxMs` и обрабатывает канал RS485 last прием ms в контексте модуля RS485Display.cpp.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
uint32_t RS485Display_lastRxMs()
{
    return g_lastRxTickMs;
}


//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `RS485Display_payloadSize` и обрабатывает канал RS485 полезную нагрузку size в контексте модуля RS485Display.cpp.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
size_t RS485Display_payloadSize()
{
    return sizeof(full_packet_net_t);
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `RS485Display_frameSize` и обрабатывает канал RS485 кадр size в контексте модуля RS485Display.cpp.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
size_t RS485Display_frameSize()
{
    return sizeof(full_packet_net_t) + sizeof(PACKET_HEADER) + sizeof(uint16_t) + sizeof(PACKET_FOOTER);
}

//------------------------------------------------------------------------------
// Назначение функции: Формирует и отправляет пакет канал RS485 через нужный интерфейс связи или вывода.
// Локальные переменные: plen — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; uint8_t — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
//------------------------------------------------------------------------------
// ============== ПЕРЕДАЧА ПАКЕТА ВНЕШНЕМУ ДИСПЛЕЮ ===================
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



//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `receivePacket_RS485` и обрабатывает прием пакет канал RS485 в контексте модуля RS485Display.cpp.
// Локальные переменные: uint8_t — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; idx — счетчик или индекс: указывает позицию элемента, номер строки, слота или текущую стадию перебора; READ_BUDGET — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; read_left — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; start — временной параметр или отметка времени: используется для тайм-аутов, задержек, мигания или контроля давности данных; frame_len — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
//------------------------------------------------------------------------------
// ================ ПРИЁМ ОТВЕТНОГО ПАКЕТА с внешнего дисплея ================
// Порционный парсер + быстрый ресинк, структура кадра синхронизирована с внешним дисплеем.
bool receivePacket_RS485(full_packet_net_t* pkt, uint8_t* btn1, uint8_t* btn2)
{
    if (!pkt) return false;
    static uint8_t buffer[sizeof(full_packet_net_t) + 64];
    static size_t idx = 0;  // Счетчик или индекс: указывает позицию элемента, номер строки, слота или текущую стадию перебора.

    const int READ_BUDGET = 256;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
    int read_left = READ_BUDGET;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
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

    size_t start = 0;  // Временной параметр или отметка времени: используется для тайм-аутов, задержек, мигания или контроля давности данных.
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

    uint8_t* data = &buffer[4];  // Структура настроек, состояния или набора рабочих данных.
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

//------------------------------------------------------------------------------
// Назначение функции: Обслуживает прием задачу в основном цикле: проверяет события, обновляет состояние и запускает нужные действия.
// Локальные переменные: recpkt — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
// - recpkt: Служебная переменная, используемая для промежуточных вычислений и логики модуля.
// - btn2: Объект внешнего интерфейса, экрана, порта или канала связи.
//------------------------------------------------------------------------------
void rxTask(void* param)
{
    esp_task_wdt_add(NULL);
    static full_packet_net_t recpkt;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
    uint8_t btn1 = 0, btn2 = 0;  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
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
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

//------------------------------------------------------------------------------
// Назначение функции: Инициализирует канал RS485, подготавливает связанные объекты и включает работу соответствующего узла.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
void RS485Display_setup()
{
    if (rs485Mutex == nullptr) rs485Mutex = xSemaphoreCreateMutex();
    if (auxMutex == nullptr) auxMutex = xSemaphoreCreateMutex();

    pinMode(RS485_DE_PIN, OUTPUT);
    digitalWrite(RS485_DE_PIN, LOW);
    rs485SetTX(false);

    // Важно: UART2 должен иметь только одного владельца.
    // Если режим внешнего RS485-дисплея не выбран, этот модуль не должен
    // открывать Serial2, иначе NMEA_setup() откроет тот же порт повторно.
    if (settings == nullptr || settings->rs485_out != OUTPUT_MODE_RS485_DISPLAY)
    {
        if (g_rs485RxTask != nullptr)
        {
            vTaskDelete(g_rs485RxTask);
            g_rs485RxTask = nullptr;
        }
        return;
    }

    RS485_SERIAL.setRxBufferSize(1024);
    RS485_SERIAL.setTxBufferSize(1024);
    RS485_SERIAL.begin(RS485_BAUD, RS485_CONFIG, RS485_RX_PIN, RS485_TX_PIN);

    if (g_rs485RxTask == nullptr)
    {
        xTaskCreatePinnedToCore(rxTask, "RX", 8192, NULL, 1, &g_rs485RxTask, 0);
    }
}

//------------------------------------------------------------------------------
// Назначение функции: Обслуживает канал RS485 в основном цикле: проверяет события, обновляет состояние и запускает нужные действия.
// Локальные переменные: self — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
//------------------------------------------------------------------------------
static bool auxCoordinatesPresent(float lat, float lon)
{
    return lat > 0.000001f || lat < -0.000001f ||
           lon > 0.000001f || lon < -0.000001f;
}

static void RS485Display_fillBaseAux(aux_t& aux)
{
    const bool testMode = (settings != nullptr && FlyRfMode_usesLocalCoordinates(settings->mode));
    aux.base_test_mode_M = testMode;
    aux.gps_time_valid_M = GNSS_timeValid();
    aux.gps_satellites_valid_M = GNSS_satellitesValid();
    aux.gps_waiting_M = GNSS_waitingForInitialFix() || GNSS_waitingForRecovery();
    aux.gps_no_data_M = GNSS_noDataTimeout();
    aux.gps_rx_M = GNSS_coordinatesValid() || GNSS_timeValid() || GNSS_satellitesValid();
    aux.gps_satellites_M = GNSS_satellitesValid() ? GNSS_satellites() : 0U;

    if (testMode && settings != nullptr)
    {
        aux.display_coord_is_local_M = true;
        aux.display_latitude_M = settings->local_latitude;
        aux.display_longitude_M = settings->local_longitude;
        aux.display_coord_valid_M = auxCoordinatesPresent(aux.display_latitude_M, aux.display_longitude_M);
    }
    else
    {
        aux.display_coord_is_local_M = false;
        aux.display_latitude_M = GNSS_latitude();
        aux.display_longitude_M = GNSS_longitude();
        aux.display_coord_valid_M = GNSS_coordinatesValid();
    }

    uint32_t txPackets = 0;
    uint32_t rxPackets = 0;
    RF_GetPacketCounters(txPackets, rxPackets);
    aux.lora_tx_packets_M = txPackets;
    aux.lora_rx_packets_M = rxPackets;
    aux.lora_rf_hz_M = RF_GetCurrentFrequencyHz();

    uint32_t lanTxPackets = 0;
    uint32_t lanRxPackets = 0;
    uint32_t lanUdpTxPackets = 0;
    uint32_t lanUdpRxPackets = 0;
    uint32_t lanTcpTxPackets = 0;
    uint32_t lanTcpRxPackets = 0;
    LAN_getPacketCounters(lanTxPackets, lanRxPackets, lanUdpTxPackets, lanUdpRxPackets, lanTcpTxPackets, lanTcpRxPackets);

    const IPAddress lanIp = LAN_localIP();
    aux.lan_state_view_M = (settings != nullptr && settings->lan_state_view != 0);
    aux.lan_ready_M = LAN_ready();
    aux.lan_link_up_M = LAN_linkUp();
    aux.lan_udp_working_M = LAN_udpWorking();
    aux.lan_dhcp_M = LAN_dhcpLeaseActive();
    aux.lan_ip_M[0] = lanIp[0];
    aux.lan_ip_M[1] = lanIp[1];
    aux.lan_ip_M[2] = lanIp[2];
    aux.lan_ip_M[3] = lanIp[3];
    aux.lan_udp_port_M = (settings != nullptr && settings->udp_port != 0U) ? settings->udp_port : (uint16_t)NMEA_UDP_PORT;
    aux.lan_tx_packets_M = lanTxPackets;
    aux.lan_rx_packets_M = lanRxPackets;
    aux.lan_udp_tx_packets_M = lanUdpTxPackets;
    aux.lan_udp_rx_packets_M = lanUdpRxPackets;
}

void RS485Display_loop()
{
    if (settings == nullptr || settings->rs485_out != OUTPUT_MODE_RS485_DISPLAY) return;
    uint32_t now = millis();
    if (now - previousMillis < interval) return;
    previousMillis = now;

    static full_packet_net_t packet = {};  // Рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
    memset(&packet, 0, sizeof(packet));
    ufo_t self = EmptyFO;  // Навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта.
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
    aux_t auxCopy = {};  // Навигационный или геометрический параметр: координаты, угол, дальность, размер или положение объекта.
    RS485Display_getOutgoingAux(&auxCopy);
    auxCopy.new_button_M = g_localButtonEvent;
    g_localButtonEvent = 0;
    RS485Display_fillBaseAux(auxCopy);
    memcpy(&packet.AuxData, &auxCopy, sizeof(aux_t));
    packet.BUTTON1 = 0x01;
    packet.BUTTON2 = 0x02;
    sendPacket_RS485(&packet);
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `RS485Display_fini` и обрабатывает канал RS485 fini в контексте модуля RS485Display.cpp.
// Локальные переменные: явные локальные переменные не используются; функция работает через параметры, глобальные структуры и результаты вызовов.
//------------------------------------------------------------------------------
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
