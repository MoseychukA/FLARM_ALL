//#pragma once

#ifndef COMMS_H_
#define COMMS_H_

#include "Arduino.h"
#include "data_structures.h"  // For PFBQueue.
#include "hardware/uart.h"
#include "settings.h"
#include "transponder_packet.h"
#include <stdlib.h> // дл¤ free, если используете

extern PFBQueue<char*> decode_debug_message_out_queue;
extern volatile bool decode_message_available;

extern volatile bool message_ready;
extern volatile bool message_ESP;
extern uint8_t message_buffer[];
extern size_t message_len_bytes;

class CommsManager 
{

public:

       CommsManager();
       ~CommsManager();
    static constexpr uint16_t kATCommandBufMaxLen         = 1000;
    static constexpr uint16_t kNetworkConsoleBufMaxLen    = 4096;
    static constexpr uint16_t kPrintfBufferMaxSize        = 500;
    static constexpr uint32_t kRawReportingIntervalMs     = 50;  // Report packets internally at 20Hz.
    static constexpr uint32_t kCSBeeReportingIntervalMs   = 1000;
 

    ////!!!
    //struct CommsManagerConfig 
    //{
    //    uart_inst_t *comms_uart_handle = uart1;
    //    uint16_t comms_uart_tx_pin = 4;
    //    uint16_t comms_uart_rx_pin = 5;
    //    uint16_t uart_timeout_us = 0;  // Time to wait for a character if there isn't one alredy available.
    //};


    //CommsManager(CommsManagerConfig config_in);

    /**
     * Initialize the CommsManager. Sets up UARTs and other necessary peripherals.
     * @retval True if initialization succeeded, false otherwise.
     */
    bool Init();

    /**
    * Обновить CommsManager. Запускает все подпрограммы обновления, необходимые для нормальной работы.
    * @retval True, если обновление прошло успешно, в противном случае — false.
    */
    bool Update();

    int console_printf(const char *format, ...);
    int console_level_printf(SettingsManager::LogLevel level, const char *format, ...);
    int iface_printf(SettingsManager::SerialInterface iface, const char *format, ...);
    int iface_vprintf(SettingsManager::SerialInterface iface, const char *format, va_list args);
    bool iface_putc(SettingsManager::SerialInterface iface, char c);
    bool iface_getc(SettingsManager::SerialInterface iface, char &c);
    bool iface_puts(SettingsManager::SerialInterface iface, const char *buf);

    bool network_console_putc(char c);
    bool network_console_puts(const char *buf, uint16_t len = UINT16_MAX);

    void SendBuf(SettingsManager::SerialInterface iface, char *buf, uint16_t buf_len) 
    {
        for (uint16_t i = 0; i < buf_len; i++) 
        {
            iface_putc(iface, buf[i]);
        }
    }


    void SendBufSerial(SettingsManager::SerialInterface iface, char* buf, uint16_t buf_len)
    {
        for (uint16_t i = 0; i < buf_len; i++)
        {
            Serial.write(buf[i]);
        }
    }



    /**
     * Sets the baudrate for a serial interface.
     * @param[in] iface SerialInterface to set baudrate for.
     * @param[in] baudrate Baudrate to set.
     * @retval True if the baudrate could be set, false if the interface specified does not support a baudrate.
     */
    bool SetBaudrate(SettingsManager::SerialInterface iface, uint32_t baudrate) 
    {
        switch (iface) 
        {
            case SettingsManager::kCommsUART:
                // Save the actual set value as comms_uart_baudrate_.
                comms_uart_baudrate_ = uart_set_baudrate(comms_uart_handle, baudrate);
                return true;
                break;
            default:
                return false;  // Other interfaces don't have a baudrate.
        }
        return false;  // Should never get here.
    }

    /**
     * Returns the currently set baudrate for a serial interface.
     * @param[in] iface SerialInterface to get the baudrate for.
     * @param[out] baudrate Reference to uint32_t to fill with retrieved value.
     * @retval True if baudrate retrieval succeeded, false if iface does not support a baudrate.
     */
    bool GetBaudrate(SettingsManager::SerialInterface iface, uint32_t &baudrate) 
    {
        switch (iface) 
        {
            case SettingsManager::kCommsUART:
                // Save the actual set value as comms_uart_baudrate_.
                baudrate = comms_uart_baudrate_;
                return true;
                break;
            default:
                return false;  // Other interfaces don't have a baudrate.
        }
        return false;  // Should never get here.
    }

    /**
     * Specify the reporting protocol for a given serial interface.
     * @param[in] iface SerialInterface to set reporting protocol on.
     * @param[in] protocol Reporting protocol to set on iface.
     * @retval True if succeeded, false otherwise.
     */
    bool SetReportingProtocol(SettingsManager::SerialInterface iface, SettingsManager::ReportingProtocol protocol) 
    {
        reporting_protocols_[iface] = protocol;
        return true;
    }

    /**
     * Get the reporting protocol for a given serial interface.
     * @param[in] iface SerialInterface to get the reporting protocol from.
     * @param[out] protocol reference to ReportingProtocol to fill with result.
     * @retval True if reportig protocol could be retrieved, false otherwise.
     */
    bool GetReportingProtocol(SettingsManager::SerialInterface iface, SettingsManager::ReportingProtocol &protocol) 
    {
        protocol = reporting_protocols_[iface];
        return true;
    }

    // Public console settings.
    SettingsManager::LogLevel log_level = SettingsManager::LogLevel::kInfo;  // Start with highest verbosity by default.

    // Queue for storing transponder packets before they get reported.
    PFBQueue<Decoded1090Packet> transponder_packet_reporting_queue =  PFBQueue<Decoded1090Packet>({.buf_len_num_elements = SettingsManager::Settings::kMaxNumTransponderPackets,
                                     .buffer = transponder_packet_reporting_queue_buffer_});

    // Queues for incoming / outgoing network characters.
    PFBQueue<char> esp32_console_rx_queue =
        PFBQueue<char>({.buf_len_num_elements = kNetworkConsoleBufMaxLen, .buffer = esp32_console_rx_queue_buffer_});
    PFBQueue<char> esp32_console_tx_queue =
        PFBQueue<char>({.buf_len_num_elements = kNetworkConsoleBufMaxLen, .buffer = esp32_console_tx_queue_buffer_});

   private:
 
    bool UpdateReporting();


    uart_inst_t* comms_uart_handle = uart1;
    uint16_t comms_uart_tx_pin = 4;
    uint16_t comms_uart_rx_pin = 5;
    uint16_t uart_timeout_us = 0;  // Time to wait for a character if there isn't one alredy available.

    bool ReportRaw(SettingsManager::SerialInterface iface, const Decoded1090Packet packets_to_report_1090[],
                   uint16_t num_packets_to_report);

    /**
     * Sends out Mode S Beast formatted transponder data on the selected serial interface. Reports all transponder
     * packets in the provided packets_to_report array, which is used to allow printing arbitrary blocks of transponder
     * packets received via the CommsManager's built-in transponder_packet_reporting_queue_.
     * @param[in] iface SerialInterface to broadcast Mode S Beast messages on.
     * @param[in] packets_to_report Array of transponder packets to report.
     * @param[in] num_packets_to_report Number of packets to report from the packets_to_report array.
     * @retval True if successful, false if something broke.
     */
    bool ReportBeast(SettingsManager::SerialInterface iface, const Decoded1090Packet packets_to_report_1090[],
                     uint16_t num_packets_to_report);

    /**
     * Sends out comma separated aircraft information for each aircraft in the aircraft dictionary.
     * @param[in] iface SerialInterface to broadcast aircraft information on.
     * @retval True if successful, false if something broke.
     */
    bool ReportCSBee(SettingsManager::SerialInterface iface);

    // Очереди для входящих/исходящих символов сетевой консоли.
    char esp32_console_rx_queue_buffer_[kNetworkConsoleBufMaxLen];
    char esp32_console_tx_queue_buffer_[kNetworkConsoleBufMaxLen];

    // Queue for holding new transponder packets before they get reported.
    Decoded1090Packet transponder_packet_reporting_queue_buffer_[SettingsManager::Settings::kMaxNumTransponderPackets];

    // Reporting Settings
    uint32_t comms_uart_baudrate_ = SettingsManager::Settings::kDefaultCommsUARTBaudrate;
    SettingsManager::ReportingProtocol
        reporting_protocols_[SettingsManager::SerialInterface::kNumSerialInterfaces - 1] = {
            SettingsManager::ReportingProtocol::kNoReports };  

    // Reporting protocol timestamps
    // NOTE: Raw reporting interval used for RAW and BEAST protocols as well as internal functions.
    uint32_t last_raw_report_timestamp_ms_ = 0;
    uint32_t last_csbee_report_timestamp_ms_ = 0;


    // OTA configuration. Used to ignore incoming UART commands while processing OTA data.
    uint32_t ota_transfer_begin_timestamp_ms_ = 0;
    uint32_t ota_transfer_bytes_remaining_ = 0;
};

extern CommsManager comms_manager;

#endif /* COMMS_H_ */