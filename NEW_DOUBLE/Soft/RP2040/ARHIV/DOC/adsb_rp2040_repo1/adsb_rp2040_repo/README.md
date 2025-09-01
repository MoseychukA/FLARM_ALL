
# ADS-B RP2040 FreeRTOS Receiver (3 channels)

Проект под Arduino IDE для Raspberry Pi Pico (RP2040):
- FreeRTOS (SMP, 2 ядра)
- PIO-сэмплинг 10 МГц, цифровой фильтр импульсов 0.3–0.7 μs, нормализация 0.5 μs
- Детектор преамбулы, PPM демодуляция 56/112 бит
- 3 независимых входных канала GPIO 18/19/22, индикация GPIO 17/20/23
- RSSI по ADC GPIO 26
- CRC-24 (256-элементная таблица) + коррекция одиночной битовой ошибки
- Полный декод ADS-B: ICAO, рейс, позиция (CPR), скорость, высота
- Serial2: TX GPIO4, RX GPIO5 (230400 бод)
- Без использования digitalWrite — быстрые GPIO/PIO

## Проверенные версии
- Arduino IDE 2.3.2
- Earle Philhower RP2040 core 3.7.2
  - Boards Manager URL: https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json
- FreeRTOS_RP2040 by Khoih-prog 1.3.2 (Library Manager)

## Настройки Tools
- Board: Raspberry Pi Pico
- CPU Speed: 250 MHz
- Optimize: Optimize Most (-O3)
- USB Stack: Pico SDK
- Debug Port: Serial
- Upload Method: Default (UF2)
- C++ Exceptions: Disabled

## Пины
- Serial2 TX: GPIO4, RX: GPIO5
- Каналы RX: CH1 GPIO18, CH2 GPIO19, CH3 GPIO22
- Индикация преамбулы: CH1 GPIO17, CH2 GPIO20, CH3 GPIO23
- RSSI (ADC0): GPIO26
- Blinker: core0 GPIO15 (1000 ms), core1 GPIO25 (500 ms)

## Сборка и запуск
1. Откройте в Arduino IDE папку `adsb_rp2040_adsb` как скетч.
2. Установите зависимости и выставьте настройки Tools как выше.
3. Скомпилируйте и прошейте на Raspberry Pi Pico.
4. Подключите приёмник ADS-B к входам 18/19/22 (уровень 3.3В).
5. Подключите UART2 TX (GPIO4) к USB-UART адаптеру, 230400 бод.
6. В монитор порта пойдут строки:
   - RAW (до CRC) и затем декодированные поля: DF/ICAO/FLT/LAT/LON/GS/TRK/ALT/RSSI/CRC_OK.

## Архитектура
- Core 1: приём (PIO), фильтрация, детектор преамбулы, PPM демодуляция, формирование пакетов — в очередь.
- Core 0: CRC, одноразрядная коррекция, декод ADS-B (включая CPR), вывод в Serial2.

## Лицензия
MIT — см. файл LICENSE.
