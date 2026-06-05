#include "comms.h"
#include <cstdarg>  // For debug printf.
#include <cstdio>   // Regular pico/stdio.h doesn't support vprint functions.
#include "pico/stdlib.h"
#include <stdio.h>  // for printing

#include <cstring>   // for strcat
#include <iostream>  // for AT command ingestion


CommsManager comms_manager;

CommsManager::CommsManager()
{

}

CommsManager::~CommsManager()
{

}

CommsManager::CommsManager(CommsManagerConfig config_in)
    : config_(config_in) {}

bool CommsManager::Init() 
{
    gpio_set_function(config_.comms_uart_tx_pin, GPIO_FUNC_UART);    // uart1  comms_uart_tx_pin = 4;
    gpio_set_function(config_.comms_uart_rx_pin, GPIO_FUNC_UART);    // uart1  comms_uart_rx_pin = 5;
    uart_set_translate_crlf(config_.comms_uart_handle, true);        // Если true, преобразовать перевод строки в возврат каретки при передаче
    uart_init(config_.comms_uart_handle, 115200);                    // 

    /* ============== Serial1 не используется =====================*/
 /* gpio_set_function(config_.gnss_uart_tx_pin, GPIO_FUNC_UART);
    gpio_set_function(config_.gnss_uart_rx_pin, GPIO_FUNC_UART);
    uart_set_translate_crlf(config_.gnss_uart_handle, false);
    uart_init(config_.gnss_uart_handle, SettingsManager::Settings::kDefaultGNSSUARTBaudrate);*/

    return true;
}

bool CommsManager::Update() 
{
    UpdateReporting();
    return true;
}


int CommsManager::console_printf(const char *format, ...) 
{
    va_list args;
    va_start(args, format);
    int res = iface_vprintf(SettingsManager::SerialInterface::kCommsUART, format, args);
    va_end(args);
    return res;
}

int CommsManager::console_level_printf(SettingsManager::LogLevel level, const char *format, ...) 
{
    if (log_level < level) return 0;
    va_list args;
    va_start(args, format);
    int res = iface_vprintf(SettingsManager::SerialInterface::kCommsUART, format, args);
    va_end(args);
    return res;
}

int CommsManager::iface_printf(SettingsManager::SerialInterface iface, const char *format, ...) 
{
    va_list args;
    va_start(args, format);
    int res = iface_vprintf(iface, format, args);
    va_end(args);
    return res;
}

int CommsManager::iface_vprintf(SettingsManager::SerialInterface iface, const char *format, va_list args) 
{
    char buf[kPrintfBufferMaxSize];

    // Formatted print to buffer.
    int res = vsnprintf(buf, kPrintfBufferMaxSize, format, args);
    if (res <= 0) {
        return res;  // vsnprintf failed.
    }
    return -1;  // puts failed.
}

bool CommsManager::iface_putc(SettingsManager::SerialInterface iface, char c) 
{
    uart_putc_raw(config_.comms_uart_handle, c);
    return true;  // Function is void so we won't know if it succeeds.
}

bool CommsManager::iface_getc(SettingsManager::SerialInterface iface, char &c) 
{

    if (uart_is_readable_within_us(config_.comms_uart_handle, config_.uart_timeout_us))
    {
        c = uart_getc(config_.comms_uart_handle);
        return true;
    }
    return false;  // No chars to read.
}

bool CommsManager::iface_puts(SettingsManager::SerialInterface iface, const char *buf) 
{

    uart_puts(config_.comms_uart_handle, buf);
    return true;  
}

