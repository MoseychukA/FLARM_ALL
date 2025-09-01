
# ADS-B RP2040 Receiver v2.4 (full, header-included, fix)

Исправления:
- Правильные объявления/порядок функций в .ino, устранены дубли переменных.
- adsb_sampler.pio.h: явные приведения типов к uint16_t для инструкций PIO (убраны предупреждения).
- Полный набор файлов v2.4 с предсобранным заголовком .pio.h (не требуется pioasm).

См. описание v2.4 в предыдущем релизе: DMA-кольца, адаптивный коррелятор (профили Normal/High-EMI/Urban/Remote), DF17/DF18 (SRC), DF20/DF21 BDS 6,0/6,1, Gillham(Q=0), TC=19 1/2/3/4, RAW/CSV/NMEA/JSON/UBX, UART2 921600, LittleFS SAVE/LOAD/SHOW.
