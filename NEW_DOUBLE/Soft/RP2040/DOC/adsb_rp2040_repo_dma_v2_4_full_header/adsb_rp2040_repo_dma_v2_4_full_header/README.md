
# ADS-B RP2040 FreeRTOS Receiver — v2.4 (full, header-included)

Особенности v2.4:
- PIO -> DMA (3 канала) в кольцевые буферы, без опроса RX FIFO CPU
- Адаптивный коррелятор преамбулы (EMA): профили Normal / High-EMI / Urban / Remote
- Демодуляция PPM 1 Mbps, фильтр импульсов 0.4–0.7 μs, нормализация 0.5 μs
- ADS-B DF17/DF18 (SRC=ADSB/TISB/ADSR/MLAT), CPR global/local, TC=19 1/2/3/4
- Mode S Comm-B DF20/DF21: BDS 6,0/6,1 (VS, baro-geo)
- Высота: Q=1 (25ft), Q=0 (Gillham AC13)
- Вывод RAW/CSV/NMEA/JSON/UBX, UART2 @ 921600
- NVS (LittleFS): SAVE/LOAD/SHOW настроек; автозагрузка при старте

Header-included:
- Файл adsb_sampler.pio.h включён в проект, поэтому pioasm не требуется

Команды Serial2: MODE, PROFILE, LOG, REF, SAVE, LOAD, SHOW, HELP
