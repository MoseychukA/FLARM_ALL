/*
  Модуль Bluetooth.cpp
  Назначение:
  - Организация BLE/UART-совместимого канала обмена для передачи данных устройства.

  Основные задачи модуля:
  - Инициализировать BLE только на поддерживаемых вариантах ESP32.
  - Поднимать сервис, похожий на последовательный порт, и обслуживать подключения.
  - Буферизовать передачу и отдавать данные из проекта наружу по Bluetooth LE.
  - Учитывать настройки режима Bluetooth из EEPROM и состояние клиента.
*/

#include "Bluetooth.h"
#include "EEPROMRF.h"
#include "DeviceInfo.h"
#include "WiFiRF.h"
#include "NMEA.h"

#if defined(ESP32)
#include "sdkconfig.h"
#endif

#if defined(ESP32) && !defined(CONFIG_IDF_TARGET_ESP32S2) && defined(CONFIG_BT_ENABLED) && defined(CONFIG_BLUEDROID_ENABLED)

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <string>

namespace {

static constexpr uint16_t UART_SERVICE_UUID16        = 0xFFE0;
static constexpr uint16_t UART_CHARACTERISTIC_UUID16 = 0xFFE1;
static constexpr size_t   TX_BUFFER_SIZE             = 1024U;
static constexpr size_t   TX_CHUNK_SIZE              = 20U;
static constexpr uint32_t TX_INTERVAL_MS             = 12UL;

BLEServer* g_server = nullptr;
BLECharacteristic* g_uartCharacteristic = nullptr;
BLEAdvertising* g_advertising = nullptr;

bool g_initialized = false;
bool g_connected = false;
bool g_prevConnected = false;
bool g_advertisingEnabled = false;
uint8_t g_lastMode = 0xFFU;
uint32_t g_lastTxMs = 0;
uint32_t g_disconnectMs = 0;
uint32_t g_restartAtMs = 0;
String g_btName;

uint8_t g_txBuffer[TX_BUFFER_SIZE] = {};  // Текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
size_t g_txHead = 0;
size_t g_txTail = 0;

size_t txAvailable()
{
    if (g_txHead >= g_txTail) return g_txHead - g_txTail;
    return TX_BUFFER_SIZE - g_txTail + g_txHead;
}

size_t txRoom()
{
    return (TX_BUFFER_SIZE - 1U) - txAvailable();
}

size_t txWrite(const uint8_t* data, size_t size)
{
    if (data == nullptr || size == 0) return 0;
    size_t written = 0;
    while (written < size && txRoom() > 0)
    {
        g_txBuffer[g_txHead] = data[written++];
        g_txHead = (g_txHead + 1U) % TX_BUFFER_SIZE;
    }
    return written;
}

size_t txRead(uint8_t* out, size_t size)
{
    if (out == nullptr || size == 0) return 0;
    size_t read = 0;
    while (read < size && g_txTail != g_txHead)
    {
        out[read++] = g_txBuffer[g_txTail];
        g_txTail = (g_txTail + 1U) % TX_BUFFER_SIZE;
    }
    return read;
}

void txClear()
{
    g_txHead = 0;
    g_txTail = 0;
}

bool modeEnabled()
{
    return settings != nullptr;
}

bool dataOutputEnabled()
{
    return settings != nullptr && settings->nmea_out == NMEA_OUTPUT_BLUETOOTH;
}

void enqueueText(const String& text)
{
    txWrite((const uint8_t*)text.c_str(), text.length());
}

String statusText()
{
    String text = F("STATUS WIFI=");
    text += WiFi_controlModeName(WiFi_controlMode());
    text += WiFi_ready() ? F(" READY") : F(" OFF");
    text += F(" IP=");
    text += WiFi_apIP().toString();
    text += F(" BT=");
    text += dataOutputEnabled() ? F("DATA") : F("SERVICE");
    text += F("\r\n");
    return text;
}

void scheduleRestart()
{
    if (g_restartAtMs == 0)
    {
        g_restartAtMs = millis() + 800UL;
    }
}

void handleCommand(String command)
{
    command.trim();
    command.toUpperCase();

    if (command.length() == 0)
    {
        return;
    }
    if (command == F("WIFI OFF"))
    {
        WiFi_setControlMode(WIFI_CONTROL_OFF, true);
        enqueueText(F("OK WIFI OFF\r\n"));
        return;
    }
    if (command == F("WIFI ON"))
    {
        const bool ok = WiFi_setControlMode(WIFI_CONTROL_ON, true);
        enqueueText(ok ? F("OK WIFI ON\r\n") : F("ERR WIFI ON\r\n"));
        return;
    }
    if (command == F("STATUS"))
    {
        enqueueText(statusText());
        return;
    }
    if (command == F("RESTART") || command == F("REBOOT"))
    {
        enqueueText(F("OK RESTART\r\n"));
        scheduleRestart();
        return;
    }

    enqueueText(F("ERR UNKNOWN COMMAND\r\n"));
}

class UartCallbacks : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic* pCharacteristic)
    {
        if (pCharacteristic == nullptr) return;
        String value = String(pCharacteristic->getValue().c_str());
        if (value.length() == 0) return;

        String chunk;
        chunk.reserve(value.length());
        for (size_t i = 0; i < value.length(); ++i)
        {
            const char c = value.charAt(i);
            if (c == '\r' || c == '\n' || c == ';')
            {
                handleCommand(chunk);
                chunk = "";
            }
            else
            {
                chunk += c;
            }
        }
        handleCommand(chunk);
    }
};

class ServerCallbacks : public BLEServerCallbacks
{
    void onConnect(BLEServer* pServer)
    {
        (void)pServer;
        g_connected = true;
    }

    void onDisconnect(BLEServer* pServer)
    {
        (void)pServer;
        g_connected = false;
        g_disconnectMs = millis();
    }
};

void ensureInitialized()
{
    if (g_initialized) return;

    g_btName = String("FlyRf_Base-") + DeviceInfo_chipIdHex() + String("-BLE");

    BLEDevice::init(g_btName.c_str());

    g_server = BLEDevice::createServer();
    g_server->setCallbacks(new ServerCallbacks());

    BLEService* service = g_server->createService(BLEUUID(UART_SERVICE_UUID16));
    g_uartCharacteristic = service->createCharacteristic(
        BLEUUID(UART_CHARACTERISTIC_UUID16),
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_NOTIFY |
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_WRITE_NR
    );
    g_uartCharacteristic->setCallbacks(new UartCallbacks());
    g_uartCharacteristic->addDescriptor(new BLE2902());
    g_uartCharacteristic->setValue((uint8_t*)"", 0);
    BLECharacteristic* rxCharacteristic = service->createCharacteristic(
        BLEUUID((uint16_t)0xFFE2),
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_WRITE_NR
    );
    rxCharacteristic->setCallbacks(new UartCallbacks());
    rxCharacteristic->setValue((uint8_t*)"", 0);
    service->start();

    g_advertising = BLEDevice::getAdvertising();
    if (g_advertising != nullptr)
    {
        g_advertising->addServiceUUID(BLEUUID(UART_SERVICE_UUID16));
        g_advertising->setScanResponse(true);
        g_advertising->setMinPreferred(0x06);
        g_advertising->setMaxPreferred(0x12);
    }

    g_initialized = true;
}

void startAdvertisingIfNeeded()
{
    if (!g_initialized || g_advertising == nullptr || g_advertisingEnabled) return;
    g_advertising->start();
    g_advertisingEnabled = true;
}

void stopAdvertisingIfNeeded()
{
    if (!g_initialized || g_advertising == nullptr || !g_advertisingEnabled) return;
    g_advertising->stop();
    g_advertisingEnabled = false;
}

} // namespace

void Bluetooth_setup()
{
    g_lastMode = 0xFFU;
    ensureInitialized();
    startAdvertisingIfNeeded();
}

// - chunk: Параметр геометрии, координаты, размера или угла.
void Bluetooth_loop()
{
    if (g_restartAtMs != 0 && (int32_t)(millis() - g_restartAtMs) >= 0)
    {
        ESP.restart();
    }

    const uint8_t mode = dataOutputEnabled() ? BLUETOOTH_LE : BLUETOOTH_OFF;
    if (mode != g_lastMode)
    {
        g_lastMode = mode;
        ensureInitialized();
        startAdvertisingIfNeeded();
    }

    if (!g_initialized) return;

    if (!g_connected && g_prevConnected)
    {
        g_prevConnected = false;
        txClear();
    }

    if (!g_connected && !g_prevConnected && modeEnabled())
    {
        if ((uint32_t)(millis() - g_disconnectMs) > 500UL)
        {
            startAdvertisingIfNeeded();
        }
    }

    if (g_connected && !g_prevConnected)
    {
        g_prevConnected = true;
        stopAdvertisingIfNeeded();
    }

    if (!modeEnabled() || !g_connected || g_uartCharacteristic == nullptr) return;
    if ((uint32_t)(millis() - g_lastTxMs) < TX_INTERVAL_MS) return;

    uint8_t chunk[TX_CHUNK_SIZE];
    const size_t count = txRead(chunk, sizeof(chunk));
    if (count > 0)
    {
        g_uartCharacteristic->setValue(chunk, count);
        g_uartCharacteristic->notify();
        g_lastTxMs = millis();
    }
}

void Bluetooth_fini()
{
    txClear();
}

size_t Bluetooth_write(const uint8_t* buffer, size_t size)
{
    if (!dataOutputEnabled() || !g_initialized || buffer == nullptr || size == 0) return 0;
    return txWrite(buffer, size);
}

bool Bluetooth_connected()
{
    return g_connected && modeEnabled();
}

bool Bluetooth_active()
{
    return modeEnabled();
}

bool Bluetooth_supported()
{
    return true;
}

String Bluetooth_name()
{
    if (g_btName.length() == 0)
    {
        g_btName = String("FlyRf_Base-") + DeviceInfo_chipIdHex() + String("-BLE");
    }
    return g_btName;
}

#else

void Bluetooth_setup() {}
void Bluetooth_loop() {}
void Bluetooth_fini() {}
size_t Bluetooth_write(const uint8_t* buffer, size_t size) { (void)buffer; (void)size; return 0; }
bool Bluetooth_connected() { return false; }
bool Bluetooth_active() { return settings != nullptr; }
bool Bluetooth_supported() { return false; }
String Bluetooth_name() { return String("BLE unavailable"); }

#endif
