/*
 * BluetoothHelper.cpp
 * Copyright (C) 2018-2023 Linar Yusupov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#if defined(ESP32)
#include "sdkconfig.h"
#endif

#if defined(ESP32) && !defined(CONFIG_IDF_TARGET_ESP32S2)

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled!
#endif

/*
    BLE code is based on Neil Kolban example for IDF:
      https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleNotify.cpp
    Ported to Arduino ESP32 by Evandro Copercini    
    HM-10 emulation and adaptation for SoftRF is done by Linar Yusupov.
*/

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>


#include "esp_gap_bt_api.h"
 
#include "SoC.h"
#include "EEPROMRF.h"
#include "Bluetooth.h"
#include "WiFiRF.h"


#include <core_version.h>

BLEServer* pServer = NULL;
BLECharacteristic* pUARTCharacteristic          = NULL;
BLECharacteristic* pSoftwareCharacteristic      = NULL;

bool deviceConnected    = false;
bool oldDeviceConnected = false;

cbuf *BLE_FIFO_RX, *BLE_FIFO_TX;

#include <BluetoothSerial.h>
BluetoothSerial SerialBT;

String BT_name = HOSTNAME;

static unsigned long BLE_Notify_TimeMarker = 0;
static unsigned long BLE_Advertising_TimeMarker = 0;

BLEDescriptor UserDescriptor(BLEUUID((uint16_t)0x2901));

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      BLE_Advertising_TimeMarker = millis();
    }
};

class UARTCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pUARTCharacteristic) 
    {
      std::string rxValue = pUARTCharacteristic->getValue();

      if (rxValue.length() > 0) 
      {
        BLE_FIFO_RX->write(rxValue.c_str(), (BLE_FIFO_RX->room() > rxValue.length() ?  rxValue.length() : BLE_FIFO_RX->room()));
      }
    }
};

static void ESP32_Bluetooth_setup()
{
  BT_name += "-";
  BT_name += String(SoC->getChipId() & 0x00FFFFFFU, HEX);
  BLEDevice::setMTU(256);
  switch (settings->bluetooth)
  {
  case BLUETOOTH_SPP:
    {
      esp_bt_controller_mem_release(ESP_BT_MODE_BLE);

      SerialBT.begin(BT_name.c_str());
    }
    break;
  case BLUETOOTH_LE_HM10_SERIAL:
    {
      BLE_FIFO_RX = new cbuf(BLE_FIFO_RX_SIZE);
      BLE_FIFO_TX = new cbuf(BLE_FIFO_TX_SIZE);

      esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);

      // Create the BLE Device
      BLEDevice::init((BT_name+"-BLE").c_str());

      /*
       * Set the MTU of the packets sent,
       * maximum is 500, Apple needs 23 apparently.
       */
      //BLEDevice::setMTU(256);

      // Create the BLE Server
      pServer = BLEDevice::createServer();
      pServer->setCallbacks(new MyServerCallbacks());

      // Create the BLE Service
      BLEService *pService = pServer->createService(BLEUUID(UART_SERVICE_UUID16));

      // Create a BLE Characteristic
      pUARTCharacteristic = pService->createCharacteristic(
                              BLEUUID(UART_CHARACTERISTIC_UUID16),
                              BLECharacteristic::PROPERTY_READ   |
                              BLECharacteristic::PROPERTY_NOTIFY |
                              BLECharacteristic::PROPERTY_WRITE_NR
                            );

      UserDescriptor.setValue("FlyRfSoft");
      pUARTCharacteristic->addDescriptor(&UserDescriptor);
      pUARTCharacteristic->addDescriptor(new BLE2902());

      pUARTCharacteristic->setCallbacks(new UARTCallbacks());

      // Start the service
      pService->start();

      
      // Create the BLE Service
      pService = pServer->createService(BLEUUID(UUID16_SVC_DEVICE_INFORMATION));

      pSoftwareCharacteristic = pService->createCharacteristic(
                              BLEUUID(UUID16_CHR_SOFTWARE_REVISION_STRING),
                              BLECharacteristic::PROPERTY_READ
                            );
  
      const char *Software      = FLYRF_FIRMWARE_VERSION;

      pSoftwareCharacteristic->    setValue((uint8_t *) Software,     strlen(Software));
  
      // Start the service
      pService->start();


      // Start advertising
      BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
      pAdvertising->addServiceUUID(BLEUUID(UART_SERVICE_UUID16));
      pAdvertising->setScanResponse(true);
      pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
      pAdvertising->setMaxPreferred(0x12);
      BLEDevice::startAdvertising();

      BLE_Advertising_TimeMarker = millis();
    }
    break;
  case BLUETOOTH_NONE:
  default:
    break;
  }
}

static void ESP32_Bluetooth_loop()
{
  switch (settings->bluetooth)
  {
  case BLUETOOTH_LE_HM10_SERIAL:
    {
      // уведомить об изменении значения
      // стек Bluetooth перегрузится, если будет отправлено слишком много пакетов
      if (deviceConnected && (millis() - BLE_Notify_TimeMarker > 10)) { /* < 18000 baud */

          uint8_t chunk[BLE_MAX_WRITE_CHUNK_SIZE];
          size_t size = BLE_FIFO_TX->available();
          size = size < BLE_MAX_WRITE_CHUNK_SIZE ? size : BLE_MAX_WRITE_CHUNK_SIZE; 

          if (size > 0) 
          {
            BLE_FIFO_TX->read((char *) chunk, size);

            pUARTCharacteristic->setValue(chunk, size);
            pUARTCharacteristic->notify();
          }

          BLE_Notify_TimeMarker = millis();
      }
      // disconnecting
      if (!deviceConnected && oldDeviceConnected && (millis() - BLE_Advertising_TimeMarker > 500) ) 
      {
          // give the bluetooth stack the chance to get things ready
          pServer->startAdvertising(); // restart advertising
          oldDeviceConnected = deviceConnected;
          BLE_Advertising_TimeMarker = millis();
      }
      // connecting
      if (deviceConnected && !oldDeviceConnected) 
      {
          // do stuff here on connecting
          oldDeviceConnected = deviceConnected;
      }
    }
    break;
  case BLUETOOTH_NONE:
  case BLUETOOTH_SPP:
  default:
    break;
  }
}

static void ESP32_Bluetooth_fini()
{
  /* TBD */
}

static int ESP32_Bluetooth_available()
{
  int rval = 0;

  switch (settings->bluetooth)
  {
  case BLUETOOTH_SPP:
    rval = SerialBT.available();
    break;
  case BLUETOOTH_LE_HM10_SERIAL:
    rval = BLE_FIFO_RX->available();
    break;
  case BLUETOOTH_NONE:
  default:
    break;
  }

  return rval;
}

static int ESP32_Bluetooth_read()
{
  int rval = -1;

  switch (settings->bluetooth)
  {
  case BLUETOOTH_SPP:
    rval = SerialBT.read();
    break;
  case BLUETOOTH_LE_HM10_SERIAL:
    rval = BLE_FIFO_RX->read();
    break;
  case BLUETOOTH_NONE:
  default:
    break;
  }

  return rval;
}

static size_t ESP32_Bluetooth_write(const uint8_t *buffer, size_t size)
{
  size_t rval = size;

  switch (settings->bluetooth)
  {
  case BLUETOOTH_SPP:
    rval = SerialBT.write(buffer, size);
    break;
  case BLUETOOTH_LE_HM10_SERIAL:
    rval = BLE_FIFO_TX->write((char *) buffer, (BLE_FIFO_TX->room() > size ? size : BLE_FIFO_TX->room()));
    break;
  case BLUETOOTH_NONE:
  default:
    break;
  }

  return rval;
}

IODev_ops_t ESP32_Bluetooth_ops = {
  "ESP32 Bluetooth",
  ESP32_Bluetooth_setup,
  ESP32_Bluetooth_loop,
  ESP32_Bluetooth_fini,
  ESP32_Bluetooth_available,
  ESP32_Bluetooth_read,
  ESP32_Bluetooth_write
};



//#endif /* EXCLUDE_BLUETOOTH */
#endif /* ESP32 */
