#ifndef BLUETOOTHHELPER_H
#define BLUETOOTHHELPER_H

enum
{
	BLUETOOTH_NONE,
	BLUETOOTH_SPP,
	BLUETOOTH_LE_HM10_SERIAL,
};

#if defined(ESP32)
#include "sdkconfig.h"
#endif

#if defined(ESP32) && !defined(CONFIG_IDF_TARGET_ESP32S2)

#define UART_SERVICE_UUID16                 ((uint16_t) 0xFFE0)
#define UART_CHARACTERISTIC_UUID16          ((uint16_t) 0xFFE1)
#define UART_SERVICE_UUID128            "0000ffe0-0000-1000-8000-00805f9b34fb"
#define UART_CHARACTERISTIC_UUID128     "0000ffe1-0000-1000-8000-00805f9b34fb"

#define UUID16_SVC_DEVICE_INFORMATION       ((uint16_t) 0x180A)
#define UUID16_CHR_SOFTWARE_REVISION_STRING ((uint16_t) 0x2A28)

/* (FLAA x MAX_TRACKING_OBJECTS + GNGGA + GNRMC + FLAU) x 80 symbols */
#define BLE_FIFO_TX_SIZE          1024
#define BLE_FIFO_RX_SIZE          256

#define BLE_MAX_WRITE_CHUNK_SIZE  256

extern IODev_ops_t ESP32_Bluetooth_ops;

#endif /* ESP32  */

#endif /* BLUETOOTHHELPER_H */
