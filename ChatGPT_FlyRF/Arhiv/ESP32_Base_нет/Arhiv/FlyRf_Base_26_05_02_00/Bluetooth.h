/*
  Модуль Bluetooth.h
  Назначение:
  - Публичный интерфейс BLE-подсистемы проекта.

  Что описано в этом файле:
  - Константы режимов Bluetooth.
  - Функции инициализации, обслуживания и завершения работы BLE.
  - Функции передачи данных и получения состояния подключения.
*/

#pragma once
#include <Arduino.h>

#define BLUETOOTH_OFF 0U
#define BLUETOOTH_LE  1U

void Bluetooth_setup();
void Bluetooth_loop();
void Bluetooth_fini();

size_t Bluetooth_write(const uint8_t* buffer, size_t size);
bool Bluetooth_connected();
bool Bluetooth_active();
bool Bluetooth_supported();
String Bluetooth_name();
