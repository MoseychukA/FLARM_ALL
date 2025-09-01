
```mermaid
flowchart LR
  P18[GPIO 18] --> PIO0_SM0
  P19[GPIO 19] --> PIO0_SM1
  P22[GPIO 22] --> PIO0_SM2

  subgraph PIO0[PIO0 @ 10 MHz]
    PIO0_SM0 -->|RX FIFO| RX0
    PIO0_SM1 -->|RX FIFO| RX1
    PIO0_SM2 -->|RX FIFO| RX2
  end

  subgraph CORE1[Core 1: RX]
    RX0 & RX1 & RX2 --> Filt[Digital filter]
    Filt --> Preamble[Detector]
    Preamble --> Demod[PPM 1 Mbps]
    Demod --> Qraw[Queue: raw]
    Preamble -->|GPIO 17/20/23| LEDs
    CORE1 -->|GPIO 25 500ms| Blinker1
  end

  subgraph CORE0[Core 0: Decode]
    Qraw --> CRC[CRC24 + 1-bit fix]
    CRC --> Decode[DF, ICAO, Callsign, CPR, V, Alt]
    Decode --> Serial2[(UART2 TX4/RX5)]
    RSSI[ADC GPIO 26] --> Serial2
    CORE0 -->|GPIO 15 1000ms| Blinker0
  end
```
