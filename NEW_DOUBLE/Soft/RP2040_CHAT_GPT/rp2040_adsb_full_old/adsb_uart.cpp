#include "adsb_packet.h"

// Настройка Serial2 для RP2040
#define UART_TX_PIN 4
#define UART_RX_PIN 5
#define UART_BAUD   115200

void uartInit() {
    Serial2.setTX(UART_TX_PIN);
    Serial2.setRX(UART_RX_PIN);
    Serial2.begin(UART_BAUD);
}

void uartSendPacket(const ToDUMP1090& pkt) {
    // Передача бинарного пакета целиком
    Serial2.write((const uint8_t*)&pkt, sizeof(pkt));
}
