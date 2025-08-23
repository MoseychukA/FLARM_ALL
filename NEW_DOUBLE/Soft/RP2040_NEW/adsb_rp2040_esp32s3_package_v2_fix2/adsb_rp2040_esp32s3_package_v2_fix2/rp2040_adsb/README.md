RP2040 ADS-B Receiver v2 FIX2
- Добавлен include "config.h" в adsb_decoder.cpp для доступа к cfg.ref_lat/ref_lon (устраняет invalid use of incomplete type Config)
- Сохранены фиксы v2 FIX (PIO header, DMA ring +2, старт DMA)
