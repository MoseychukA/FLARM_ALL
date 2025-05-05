

#ifndef SOCHELPER_H
#define SOCHELPER_H

#define SOC_UNUSED_PIN 255

#include "FlyRF.h"
#include "ESP32RF.h"


typedef struct SoC_ops_struct {
  uint8_t id;
  const char name[16];
  void (*setup)();
  void (*post_init)();
  void (*loop)();
  void (*fini)(int);
  void (*reset)();
  uint32_t (*getChipId)();
  void* (*getResetInfoPtr)();
  String (*getResetInfo)();
  String (*getResetReason)();
  uint32_t (*getFreeHeap)();
  long (*random)(long, long);
  uint32_t (*maxSketchSpace)();
  void (*WiFi_set_param)(int, int);
  void (*WiFi_transmit_UDP)(int, byte *, size_t);
  void (*WiFiUDP_stopAll)();
  bool (*WiFi_hostname)(String);
  int  (*WiFi_clients_count)();
  bool (*EEPROM_begin)(size_t);
  void (*EEPROM_extension)(int);
  void (*SPI_begin)();
  void (*swSer_begin)(unsigned long);
  void (*swSer_enableRx)(boolean);
  IODev_ops_t *Bluetooth_ops;
  IODev_ops_t *USB_ops;
  IODev_ops_t *UART_ops;
  byte (*Display_setup)();
  void (*Display_loop)();
  void (*Display_fini)(int);
  bool (*Baro_setup)();
  void (*WDT_setup)();
  void (*WDT_fini)();
} SoC_ops_t;

enum
{
	SOC_NONE,
	SOC_ESP32,
	SOC_ESP32S2,
	SOC_ESP32S3,
	SOC_ESP32C3,
};

extern const SoC_ops_t *SoC;

#if defined(ESP32)
extern const SoC_ops_t ESP32_ops;
#endif

byte SoC_setup(void);
void SoC_fini(int);
uint32_t DevID_Mapper(uint32_t);

#endif /* SOCHELPER_H */
