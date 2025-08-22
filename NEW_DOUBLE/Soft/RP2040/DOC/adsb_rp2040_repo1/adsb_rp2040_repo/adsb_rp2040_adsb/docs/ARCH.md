
# Architecture
- PIO @ 10 MHz -> digital pulse filter -> preamble detector -> PPM demod (56/112 bits)
- Core1 handles RX; Core0 handles CRC + 1-bit fix + ADS-B decode + Serial2 output.
- Queues between cores; LEDs indicate preamble on each channel.
- RSSI via ADC0 (GPIO26), averaged over 16 samples per packet.
