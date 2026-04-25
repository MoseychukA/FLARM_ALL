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

#if defined(ESP32)
#include "sdkconfig.h"
#endif

#if defined(ESP32) && !defined(CONFIG_IDF_TARGET_ESP32S2) && defined(CONFIG_BT_ENABLED) && defined(CONFIG_BLUEDROID_ENABLED)

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

namespace {

static constexpr uint16_t UART_SERVICE_UUID16        = 0xFFE0;  //
static constexpr uint16_t UART_CHARACTERISTIC_UUID16 = 0xFFE1;  //
static constexpr size_t   TX_BUFFER_SIZE             = 1024U;  //
static constexpr size_t   TX_CHUNK_SIZE              = 20U;  //
static constexpr uint32_t TX_INTERVAL_MS             = 12UL;  //

BLEServer* g_server = nullptr;  //
BLECharacteristic* g_uartCharacteristic = nullptr;  //
BLEAdvertising* g_advertising = nullptr;  //

bool g_initialized = false;  //
bool g_connected = false;  //
bool g_prevConnected = false;  //
bool g_advertisingEnabled = false;  //
uint8_t g_lastMode = 0xFFU;  //
uint32_t g_lastTxMs = 0;  
uint32_t g_disconnectMs = 0;  
String g_btName;  

uint8_t g_txBuffer[TX_BUFFER_SIZE] = {};  // Текстовая строка или буфер: хранит входное сообщение, сформированный ответ либо промежуточный текст.
size_t g_txHead = 0;  
size_t g_txTail = 0;  

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `txAvailable` и обрабатывает передачу available в контексте модуля Bluetooth.cpp.
//------------------------------------------------------------------------------
size_t txAvailable()
{
    if (g_txHead >= g_txTail) return g_txHead - g_txTail;
    return TX_BUFFER_SIZE - g_txTail + g_txHead;
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `txRoom` и обрабатывает передачу room в контексте модуля Bluetooth.cpp.
//------------------------------------------------------------------------------
size_t txRoom()
{
    return (TX_BUFFER_SIZE - 1U) - txAvailable();
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `txWrite` и обрабатывает передачу в контексте модуля Bluetooth.cpp.
// Локальные переменные: written — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
//------------------------------------------------------------------------------
size_t txWrite(const uint8_t* data, size_t size)
{
    if (data == nullptr || size == 0) return 0;
    size_t written = 0;  //
    while (written < size && txRoom() > 0)
    {
        g_txBuffer[g_txHead] = data[written++];
        g_txHead = (g_txHead + 1U) % TX_BUFFER_SIZE;
    }
    return written;
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `txRead` и обрабатывает передачу в контексте модуля Bluetooth.cpp.
// Локальные переменные: read — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
//------------------------------------------------------------------------------
size_t txRead(uint8_t* out, size_t size)
{
    if (out == nullptr || size == 0) return 0;
    size_t read = 0;  //
    while (read < size && g_txTail != g_txHead)
    {
        out[read++] = g_txBuffer[g_txTail];
        g_txTail = (g_txTail + 1U) % TX_BUFFER_SIZE;
    }
    return read;
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `txClear` и обрабатывает передачу в контексте модуля Bluetooth.cpp.
//------------------------------------------------------------------------------
void txClear()
{
    g_txHead = 0;
    g_txTail = 0;
}

class ServerCallbacks : public BLEServerCallbacks
{
//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `onConnect` и обрабатывает connect в контексте модуля Bluetooth.cpp.
//------------------------------------------------------------------------------
    void onConnect(BLEServer* pServer)
    {
        (void)pServer;
        g_connected = true;
    }

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `onDisconnect` и обрабатывает disconnect в контексте модуля Bluetooth.cpp.
//------------------------------------------------------------------------------
    void onDisconnect(BLEServer* pServer)
    {
        (void)pServer;
        g_connected = false;
        g_disconnectMs = millis();
    }
};

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `ensureInitialized` и обрабатывает ensure initialized в контексте модуля Bluetooth.cpp.
// Локальные переменные: service — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
//------------------------------------------------------------------------------
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
        BLECharacteristic::PROPERTY_NOTIFY
    );
    g_uartCharacteristic->addDescriptor(new BLE2902());
    g_uartCharacteristic->setValue((uint8_t*)"", 0);
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

//------------------------------------------------------------------------------
// Назначение функции: Инициализирует advertising if needed, подготавливает связанные объекты и включает работу соответствующего узла.
//------------------------------------------------------------------------------
void startAdvertisingIfNeeded()
{
    if (!g_initialized || g_advertising == nullptr || g_advertisingEnabled) return;
    g_advertising->start();
    g_advertisingEnabled = true;
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `stopAdvertisingIfNeeded` и обрабатывает stop advertising if needed в контексте модуля Bluetooth.cpp.
//------------------------------------------------------------------------------
void stopAdvertisingIfNeeded()
{
    if (!g_initialized || g_advertising == nullptr || !g_advertisingEnabled) return;
    g_advertising->stop();
    g_advertisingEnabled = false;
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `modeEnabled` и обрабатывает режим enabled в контексте модуля Bluetooth.cpp.
//------------------------------------------------------------------------------
bool modeEnabled()
{
    return settings != nullptr && settings->bluetooth != BLUETOOTH_OFF;
}

} // namespace

//------------------------------------------------------------------------------
// Назначение функции: Инициализирует Bluetooth, подготавливает связанные объекты и включает работу соответствующего узла.
//------------------------------------------------------------------------------
void Bluetooth_setup()
{
    g_lastMode = 0xFFU;
    if (modeEnabled())
    {
        ensureInitialized();
        startAdvertisingIfNeeded();
    }
}

//------------------------------------------------------------------------------
// Назначение функции: Обслуживает Bluetooth в основном цикле: проверяет события, обновляет состояние и запускает нужные действия.
// Локальные переменные: uint8_t — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла; count — счетчик или индекс: указывает позицию элемента, номер строки, слота или текущую стадию перебора; sizeof — рабочая переменная модуля: хранит промежуточное значение, используемое в текущей логике файла.
// - chunk: Параметр геометрии, координаты, размера или угла.
//------------------------------------------------------------------------------
void Bluetooth_loop()
{
    const uint8_t mode = settings ? settings->bluetooth : BLUETOOTH_OFF;  //
    if (mode != g_lastMode)
    {
        g_lastMode = mode;
        if (mode == BLUETOOTH_OFF)
        {
            stopAdvertisingIfNeeded();
            txClear();
        }
        else
        {
            ensureInitialized();
            startAdvertisingIfNeeded();
        }
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

    uint8_t chunk[TX_CHUNK_SIZE];  //
    const size_t count = txRead(chunk, sizeof(chunk));
    if (count > 0)
    {
        g_uartCharacteristic->setValue(chunk, count);
        g_uartCharacteristic->notify();
        g_lastTxMs = millis();
    }
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `Bluetooth_fini` и обрабатывает Bluetooth fini в контексте модуля Bluetooth.cpp.
//------------------------------------------------------------------------------
void Bluetooth_fini()
{
    txClear();
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `Bluetooth_write` и обрабатывает Bluetooth в контексте модуля Bluetooth.cpp.
//------------------------------------------------------------------------------
size_t Bluetooth_write(const uint8_t* buffer, size_t size)
{
    if (!modeEnabled() || !g_initialized || buffer == nullptr || size == 0) return 0;
    return txWrite(buffer, size);
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `Bluetooth_connected` и обрабатывает Bluetooth connected в контексте модуля Bluetooth.cpp.
//------------------------------------------------------------------------------
bool Bluetooth_connected()
{
    return g_connected && modeEnabled();
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `Bluetooth_active` и обрабатывает Bluetooth active в контексте модуля Bluetooth.cpp.
//------------------------------------------------------------------------------
bool Bluetooth_active()
{
    return modeEnabled();
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `Bluetooth_supported` и обрабатывает Bluetooth supported в контексте модуля Bluetooth.cpp.
//------------------------------------------------------------------------------
bool Bluetooth_supported()
{
    return true;
}

//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `Bluetooth_name` и обрабатывает Bluetooth name в контексте модуля Bluetooth.cpp.
//------------------------------------------------------------------------------
String Bluetooth_name()
{
    if (g_btName.length() == 0)
    {
        g_btName = String("FlyRf_Base-") + DeviceInfo_chipIdHex() + String("-BLE");
    }
    return g_btName;
}

#else

//------------------------------------------------------------------------------
// Назначение функции: Инициализирует Bluetooth, подготавливает связанные объекты и включает работу соответствующего узла.
//
//------------------------------------------------------------------------------
void Bluetooth_setup() {}
//------------------------------------------------------------------------------
// Назначение функции: Обслуживает Bluetooth в основном цикле: проверяет события, обновляет состояние и запускает нужные действия.
//
//------------------------------------------------------------------------------
void Bluetooth_loop() {}
//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `Bluetooth_fini` и обрабатывает Bluetooth fini в контексте модуля Bluetooth.cpp.
//
//------------------------------------------------------------------------------
void Bluetooth_fini() {}
//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `Bluetooth_write` и обрабатывает Bluetooth в контексте модуля Bluetooth.cpp.
//
//------------------------------------------------------------------------------
size_t Bluetooth_write(const uint8_t* buffer, size_t size) { (void)buffer; (void)size; return 0; }
//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `Bluetooth_connected` и обрабатывает Bluetooth connected в контексте модуля Bluetooth.cpp.
//
//------------------------------------------------------------------------------
bool Bluetooth_connected() { return false; }
//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `Bluetooth_active` и обрабатывает Bluetooth active в контексте модуля Bluetooth.cpp.
//
//------------------------------------------------------------------------------
bool Bluetooth_active() { return settings != nullptr && settings->bluetooth != BLUETOOTH_OFF; }
//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `Bluetooth_supported` и обрабатывает Bluetooth supported в контексте модуля Bluetooth.cpp.
//
//------------------------------------------------------------------------------
bool Bluetooth_supported() { return false; }
//------------------------------------------------------------------------------
// Назначение функции: Выполняет логику функции `Bluetooth_name` и обрабатывает Bluetooth name в контексте модуля Bluetooth.cpp.
//
//------------------------------------------------------------------------------
String Bluetooth_name() { return String("BLE unavailable"); }

#endif
