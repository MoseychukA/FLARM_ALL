RP2040 ADS-B Receiver (Arduino)

- USB Serial 115200 for control and debug (HELP/SHOW/SAVE/LOAD/PROFILE/RAW)
- Serial2 UART TX=4 RX=5 at 921600 to ESP32S3
- Inputs: ch1=pin18, ch2=pin19, ch3=pin22
- Preamble indicators: ch1=17, ch2=20, ch3=23
- RSSI on ADC0 pin26
- DMA rings 1024 words per channel, each word = 32 samples @20MHz
- Core1: PIO->DMA sampling, correlator, slicing to RawMessage queues
- Core0: CRC, single-bit fix, ADS-B decode (DF17/18/20/21), CPR helpers, TC=19 subtypes 1..4, Gillham Q=0 scaffold
- LittleFS config /config.txt with profiles Normal/High-EMI/Urban/Remote (dynamic thresholds)
- Blink pin15 (core0, 1s), pin25 (core1, 0.5s)

Build in Arduino IDE: open rp2040_adsb_receiver.ino.
