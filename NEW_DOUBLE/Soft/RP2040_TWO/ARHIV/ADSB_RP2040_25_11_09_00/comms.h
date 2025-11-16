//#pragma once

#ifndef COMMS_H_
#define COMMS_H_

#include "Arduino.h"
#include "data_structures.h"  // For PFBQueue.
#include "hardware/uart.h"
#include "settings.h"
#include "transponder_packet.h"
#include <stdlib.h> // дл€ free, если используете


class CommsManager 
{

public:

       CommsManager();
       ~CommsManager();

    static constexpr uint16_t kPrintfBufferMaxSize        = 500;
    static constexpr uint32_t kRawReportingIntervalMs     = 50;  // Report packets internally at 20Hz.
    static constexpr uint32_t kCSBeeReportingIntervalMs   = 1000;
 
    struct CommsManagerConfig 
    {
        uart_inst_t* gnss_uart_handle = uart0;
        uint16_t gnss_uart_tx_pin = 0;
        uint16_t gnss_uart_rx_pin = 1;
        uart_inst_t *comms_uart_handle = uart1;
        uint16_t comms_uart_tx_pin = 4;
        uint16_t comms_uart_rx_pin = 5;
        uint16_t uart_timeout_us = 0;  // Time to wait for a character if there isn't one alredy available.
    };


    CommsManager(CommsManagerConfig config_in);

    /**
      * »нициализирует CommsManager. Ќастраивает UART и другие необходимые периферийные устройства.
      * @retval True, если инициализаци€ прошла успешно, в противном случае false.
      */
    bool Init();

    /**
     * ќбновить CommsManager. «апустить все подпрограммы обновлени€, необходимые дл€ нормальной работы.
     * @retval True, если обновление прошло успешно, в противном случае false.
     */
    bool Update();


    int console_printf(const char *format, ...);
    int console_level_printf(SettingsManager::LogLevel level, const char *format, ...);
    int iface_printf(SettingsManager::SerialInterface iface, const char *format, ...);
    int iface_vprintf(SettingsManager::SerialInterface iface, const char *format, va_list args);
    bool iface_putc(SettingsManager::SerialInterface iface, char c);
    bool iface_getc(SettingsManager::SerialInterface iface, char &c);
    bool iface_puts(SettingsManager::SerialInterface iface, const char *buf);

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



    // Public console settings.
    SettingsManager::LogLevel log_level = SettingsManager::LogLevel::kInfo;  // Ќачните с максимальной детализации по умолчанию.

    // Queue for storing transponder packets before they get reported.
    PFBQueue<Decoded1090Packet> transponder_packet_reporting_queue =
        PFBQueue<Decoded1090Packet>({.buf_len_num_elements = SettingsManager::Settings::kMaxNumTransponderPackets,
                                     .buffer = transponder_packet_reporting_queue_buffer_});

   private:

    // Reporting Functions

    bool UpdateReporting();

    bool ReportRaw(SettingsManager::SerialInterface iface, const Decoded1090Packet packets_to_report_1090[],
                   uint16_t num_packets_to_report);
    /**
    * ќтправл€ет данные транспондера в формате Mode S Beast по выбранному последовательному интерфейсу. —ообщает обо всех пакетах транспондера
    * в предоставленном массиве packets_to_report, который используетс€ дл€ печати произвольных блоков пакетов транспондера,
    * полученных через встроенную в CommsManager очередь transponder_packet_reporting_queue_.
    * @param[in] iface SerialInterface дл€ трансл€ции сообщений Mode S Beast.
    * @param[in] packets_to_report ћассив пакетов транспондера дл€ отчЄта.
    * @param[in] num_packets_to_report  оличество пакетов дл€ отчЄта из массива packets_to_report.
    * @retval True в случае успешного выполнени€, false в случае возникновени€ ошибки.
    */
    bool ReportBeast(SettingsManager::SerialInterface iface, const Decoded1090Packet packets_to_report_1090[],
                     uint16_t num_packets_to_report);


    /**
     * Sends out comma separated aircraft information for each aircraft in the aircraft dictionary.
     * @param[in] iface SerialInterface to broadcast aircraft information on.
     * @retval True if successful, false if something broke.
     */
    bool ReportCSBee(SettingsManager::SerialInterface iface);

    CommsManagerConfig config_;

    // Queue for holding new transponder packets before they get reported.
    Decoded1090Packet transponder_packet_reporting_queue_buffer_[SettingsManager::Settings::kMaxNumTransponderPackets];

    // Reporting protocol timestamps
    // NOTE: Raw reporting interval used for RAW and BEAST protocols as well as internal functions.
    uint32_t last_raw_report_timestamp_ms_ = 0;
    uint32_t last_csbee_report_timestamp_ms_ = 0;


};

extern CommsManager comms_manager;

#endif /* COMMS_H_ */