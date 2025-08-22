
# Architecture (DMA version)
- PIO -> DMA -> ring buffers
- Core1: ring consume -> filter -> weighted preamble correlation -> demod -> queue
- Core0: CRC + 1-bit fix -> decode (callsign, CPR global/local, velocity TC19, Gillham) -> UART2 @ 921600
