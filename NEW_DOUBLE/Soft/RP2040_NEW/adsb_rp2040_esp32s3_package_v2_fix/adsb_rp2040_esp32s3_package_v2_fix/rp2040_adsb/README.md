RP2040 ADS-B Receiver v2 FIX
- Исправлен заголовок PIO: без переопределения pio_program_t, корректный массив инструкций
- Исправлена конфигурация DMA: кольцо 1024 слов (4096 байт), ring bits = __builtin_ctz(1024)+2 = 12; старт DMA через dma_channel_configure(..., start=true)
- Остальной функционал — как в v2 (CPR even/odd 10s, Gillham Q=0, TC19, DF18 src, коррелятор EMA/строгие интервалы, предфильтр 0.4–0.7 мкс, выводы LOG/CSV/JSON/NMEA)
