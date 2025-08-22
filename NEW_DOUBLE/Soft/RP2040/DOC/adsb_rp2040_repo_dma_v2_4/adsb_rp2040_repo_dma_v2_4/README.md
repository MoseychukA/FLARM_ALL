
# ADS-B RP2040 FreeRTOS Receiver — v2.4 (DMA + Adaptive + Gillham + TC19 + BDS60/61 + NVS + DF18 + Profiles)

Новое в v2.4:
- Автозагрузка настроек при старте (LittleFS): формат вывода, профиль коррелятора, лог CSV, REF.
- Больше профилей преамбулы: Normal / High-EMI / Urban / Remote (переключение через PROFILE).
- Поддержка DF18 (ADS-B Extended Squitter для non-transponder): разбор как DF17, маркировка источника по CF (SRC=ADSB/TISB/ADSR/MLAT).
- Маркировка TIS‑B/ADS‑R/MLAT в CSV/NMEA/JSON/RAW/UBX (поле SRC).
- Совместимо с v2.3: BDS 6,0/6,1 (VS, baro-geo), форматы CSV/NMEA/JSON/UBX/RAW, DMA-кольца, адаптивный коррелятор.

Команды Serial2: MODE, PROFILE, LOG, REF, SAVE, LOAD, SHOW, HELP.
