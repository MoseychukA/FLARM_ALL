#include "Bluetooth.h"

#include "DeviceInfo.h"
#include "WiFiRF.h"

#if defined(ESP32)
#include "sdkconfig.h"
#endif

#if defined(ESP32) && !defined(CONFIG_IDF_TARGET_ESP32S2) && defined(CONFIG_BT_ENABLED) && \
    (defined(CONFIG_BLUEDROID_ENABLED) || defined(CONFIG_NIMBLE_ENABLED))

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <freertos/semphr.h>

namespace
{
    constexpr uint16_t UART_SERVICE_UUID = 0xFFE0;
    constexpr uint16_t UART_TX_UUID = 0xFFE1;
    constexpr uint16_t UART_RX_UUID = 0xFFE2;
    constexpr size_t TX_CHUNK_SIZE = 20U;
    constexpr uint32_t TX_INTERVAL_MS = 12UL;

    BLECharacteristic* g_txCharacteristic = nullptr;
    BLEAdvertising* g_advertising = nullptr;
    bool g_initialized = false;
    bool g_connected = false;
    bool g_wasConnected = false;
    bool g_advertisingActive = false;
    uint32_t g_lastTxMs = 0U;
    uint32_t g_disconnectedMs = 0U;
    uint32_t g_restartAtMs = 0U;
    String g_name;
    String g_pendingReply;
    size_t g_replyOffset = 0U;
    SemaphoreHandle_t g_replyMutex = nullptr;

    void setReply(const String& text)
    {
        if (g_replyMutex != nullptr) xSemaphoreTake(g_replyMutex, portMAX_DELAY);
        g_pendingReply = text;
        g_replyOffset = 0U;
        if (g_replyMutex != nullptr) xSemaphoreGive(g_replyMutex);
    }

    String statusText()
    {
        String text = F("STATUS WIFI=");
        text += WiFi_controlModeName(WiFi_controlMode());
        text += WiFi_ready() ? F(" READY IP=") : F(" OFF IP=");
        text += WiFi_apIP().toString();
        text += F("\r\n");
        return text;
    }

    void handleCommand(String command)
    {
        command.trim();
        command.toUpperCase();
        if (command.length() == 0U) return;

        if (command == F("WIFI OFF"))
        {
            WiFi_setControlMode(WIFI_CONTROL_OFF, true);
            setReply(F("OK WIFI OFF\r\n"));
        }
        else if (command == F("WIFI ON"))
        {
            const bool ok = WiFi_setControlMode(WIFI_CONTROL_ON, true);
            setReply(ok ? F("OK WIFI ON\r\n") : F("ERR WIFI ON\r\n"));
        }
        else if (command == F("STATUS"))
        {
            setReply(statusText());
        }
        else if (command == F("RESTART") || command == F("REBOOT"))
        {
            setReply(F("OK RESTART\r\n"));
            g_restartAtMs = millis() + 800UL;
        }
        else
        {
            setReply(F("ERR UNKNOWN COMMAND\r\n"));
        }
    }

    class UartCallbacks : public BLECharacteristicCallbacks
    {
        void onWrite(BLECharacteristic* characteristic) override
        {
            if (characteristic == nullptr) return;
            String value(characteristic->getValue().c_str());
            String command;
            for (size_t i = 0U; i < value.length(); ++i)
            {
                const char c = value.charAt(i);
                if (c == '\r' || c == '\n' || c == ';')
                {
                    handleCommand(command);
                    command = "";
                }
                else
                {
                    command += c;
                }
            }
            handleCommand(command);
        }
    };

    class ServerCallbacks : public BLEServerCallbacks
    {
        void onConnect(BLEServer*) override { g_connected = true; }
        void onDisconnect(BLEServer*) override
        {
            g_connected = false;
            g_disconnectedMs = millis();
        }
    };

    void startAdvertising()
    {
        if (g_advertising != nullptr && !g_advertisingActive)
        {
            g_advertising->start();
            g_advertisingActive = true;
        }
    }

    void stopAdvertising()
    {
        if (g_advertising != nullptr && g_advertisingActive)
        {
            g_advertising->stop();
            g_advertisingActive = false;
        }
    }
}

void Bluetooth_setup()
{
    if (g_initialized)
    {
        startAdvertising();
        return;
    }

    g_name = String("FlyRf_Disp-") + DeviceInfo_chipIdHex() + String("-BLE");
    if (g_replyMutex == nullptr) g_replyMutex = xSemaphoreCreateMutex();
    BLEDevice::init(g_name.c_str());
    BLEServer* server = BLEDevice::createServer();
    server->setCallbacks(new ServerCallbacks());
    BLEService* service = server->createService(BLEUUID(UART_SERVICE_UUID));
    g_txCharacteristic = service->createCharacteristic(
        BLEUUID(UART_TX_UUID),
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY |
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
    g_txCharacteristic->setCallbacks(new UartCallbacks());
    g_txCharacteristic->addDescriptor(new BLE2902());
    BLECharacteristic* rx = service->createCharacteristic(
        BLEUUID(UART_RX_UUID), BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
    rx->setCallbacks(new UartCallbacks());
    service->start();

    g_advertising = BLEDevice::getAdvertising();
    g_advertising->addServiceUUID(BLEUUID(UART_SERVICE_UUID));
    g_advertising->setScanResponse(true);
    g_initialized = true;
    startAdvertising();
}

void Bluetooth_loop()
{
    if (g_restartAtMs != 0U && (int32_t)(millis() - g_restartAtMs) >= 0) ESP.restart();
    if (!g_initialized) return;

    if (g_connected && !g_wasConnected)
    {
        g_wasConnected = true;
        stopAdvertising();
    }
    else if (!g_connected && g_wasConnected)
    {
        g_wasConnected = false;
        g_disconnectedMs = millis();
    }
    if (!g_connected && (uint32_t)(millis() - g_disconnectedMs) > 500UL) startAdvertising();
    if (!g_connected || g_txCharacteristic == nullptr ||
        (uint32_t)(millis() - g_lastTxMs) < TX_INTERVAL_MS) return;

    uint8_t chunk[TX_CHUNK_SIZE];
    size_t count = 0U;
    if (g_replyMutex != nullptr && xSemaphoreTake(g_replyMutex, 0) == pdTRUE)
    {
        while (count < sizeof(chunk) && g_replyOffset < g_pendingReply.length())
        {
            chunk[count++] = (uint8_t)g_pendingReply.charAt(g_replyOffset++);
        }
        if (g_replyOffset >= g_pendingReply.length())
        {
            g_pendingReply = "";
            g_replyOffset = 0U;
        }
        xSemaphoreGive(g_replyMutex);
    }

    if (count > 0U)
    {
        g_txCharacteristic->setValue(chunk, count);
        g_txCharacteristic->notify();
        g_lastTxMs = millis();
    }
}

void Bluetooth_fini() { g_pendingReply = ""; }
bool Bluetooth_connected() { return g_connected; }
bool Bluetooth_supported() { return true; }
String Bluetooth_name() { return g_name; }

#else

void Bluetooth_setup() {}
void Bluetooth_loop() {}
void Bluetooth_fini() {}
bool Bluetooth_connected() { return false; }
bool Bluetooth_supported() { return false; }
String Bluetooth_name() { return String("BLE unavailable"); }

#endif
