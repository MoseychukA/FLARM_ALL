#include <Arduino.h>
#include "hardware/uart.h"
#include "hardware/gpio.h"

uint16_t comms_uart_tx_pin = 4;
uint16_t comms_uart_rx_pin = 5;

// Структура
struct __attribute__((packed)) ToDUMP1090 {
    uint32_t  addr;
    char      squawk[5];
    char      flight[16];
    int32_t   altitude;
    int32_t   speed;
    int32_t   track;
    int32_t   vert_rate;
    float     lat_msg;
    float     lon_msg;
    int32_t   seen_time;
    char      endOfPacket[3];
};

// Глобально объявляем переменную
ToDUMP1090 packet;

uint32_t toBigEndian32(uint32_t val) {
    return ((val & 0xFF) << 24) |
        ((val & 0xFF00) << 8) |
        ((val & 0xFF0000) >> 8) |
        ((val & 0xFF000000) >> 24);
}

float floatToBigEndian(float val) {
    uint32_t temp;
    memcpy(&temp, &val, 4);
    temp = toBigEndian32(temp);
    float res;
    memcpy(&res, &temp, 4);
    return res;
}

void sendToDUMP1090_UART1(const ToDUMP1090& src) {
    ToDUMP1090 out;

    // Преобразование BigEndian как раньше
    out.addr = toBigEndian32(src.addr);
    memcpy(out.squawk, src.squawk, 5);
    memcpy(out.flight, src.flight, 16);
    out.altitude = toBigEndian32(src.altitude);
    out.speed = toBigEndian32(src.speed);
    out.track = toBigEndian32(src.track);
    out.vert_rate = toBigEndian32(src.vert_rate);
    out.lat_msg = floatToBigEndian(src.lat_msg);
    out.lon_msg = floatToBigEndian(src.lon_msg);
    out.seen_time = toBigEndian32(src.seen_time);
    out.endOfPacket[0] = 0xFF;
    out.endOfPacket[1] = 0xFF;
    out.endOfPacket[2] = 0xFF;

    // Отправка структуры как бинарного массива через UART1
    uart_write_blocking(uart1, reinterpret_cast<const uint8_t*>(&out), sizeof(ToDUMP1090));
}

void setup() {
    Serial.begin(115200);

    // Настройка пинов
    gpio_set_function(comms_uart_tx_pin, GPIO_FUNC_UART);
    gpio_set_function(comms_uart_rx_pin, GPIO_FUNC_UART);

    // Инициализация UART1
    uart_init(uart1, 115200);

    uart_set_hw_flow(uart1, false, false);

    // Заполняем данные
    packet.addr = 0x11223344;
    strcpy(packet.squawk, "7021");
    strcpy(packet.flight, "RUS1432");
    packet.altitude = 12345;
    packet.speed = 500;
    packet.track = 180;
    packet.vert_rate = 10;
    packet.lat_msg = 56.123456f;
    packet.lon_msg = 37.987654f;
    packet.seen_time = 1713450998;
    Serial1.println("Start END");
}

void loop() 
{

    sendToDUMP1090_UART1(packet);
    Serial.println("Отправлен бинарный пакет по uart1");
    delay(1000);
}
