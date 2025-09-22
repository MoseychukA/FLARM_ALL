#include "adsbee.h"
#include "beast_utils.h"
#include "comms.h"
#include "csbee_utils.h"
#include "hal.h"  // For timestamping.
#include "raw_utils.h"
#include "unit_conversions.h"

extern ADSBee adsbee;

//bool CommsManager::InitReporting() { return true; }

bool CommsManager::UpdateReporting() 
{
    bool ret = true;
 
    uint32_t timestamp_ms = millis();

    //if (timestamp_ms - last_raw_report_timestamp_ms_ <= kRawReportingIntervalMs) 
    //{
    //   return true;  // Nothing to update.
    //}
    // Продолжить обновление и записать временную метку.
    last_raw_report_timestamp_ms_ = timestamp_ms;
    if (message_ESP)
    {

        for (uint16_t i = 0; i < SettingsManager::SerialInterface::kCommsUART; i++)
        {
            // if (timestamp_ms - last_csbee_report_timestamp_ms_ >= kCSBeeReportingIntervalMs) // 1000U
            //if (message_ESP)
            //{
                ret = ReportCSBee(SettingsManager::SerialInterface::kCommsUART);
                last_csbee_report_timestamp_ms_ = timestamp_ms;
                message_ESP = false;
            //}
            break;

        }
    }

    return ret;
}

bool CommsManager::ReportRaw(SettingsManager::SerialInterface iface, const Decoded1090Packet packets_to_report_1090[],
                             uint16_t num_packets_to_report) 
{
    for (uint16_t i = 0; i < num_packets_to_report; i++) 
    {
        char raw_frame_buf[kRaw1090FrameMaxNumChars];
        uint16_t num_bytes_in_frame = BuildRaw1090Frame(packets_to_report_1090[i].GetRaw(), raw_frame_buf);
        SendBuf(iface, (char *)raw_frame_buf, num_bytes_in_frame);
        comms_manager.iface_puts(iface, (char *)"\r\n");  // Send delimiter.
    }
    return true;
}

bool CommsManager::ReportBeast(SettingsManager::SerialInterface iface, const Decoded1090Packet packets_to_report_1090[],
                               uint16_t num_packets_to_report) {
    for (uint16_t i = 0; i < num_packets_to_report; i++) 
    {
        uint8_t beast_frame_buf[kBeastFrameMaxLenBytes];
        uint16_t num_bytes_in_frame = Build1090BeastFrame(packets_to_report_1090[i], beast_frame_buf);
        comms_manager.iface_putc(iface, char(0x1a));  // Send beast escape char to denote beginning of frame.
        SendBuf(iface, (char *)beast_frame_buf, num_bytes_in_frame);
    }
    return true;
}

bool CommsManager::ReportCSBee(SettingsManager::SerialInterface iface)
{
    // Записать сообщение CSBee Aircraft для каждого самолета в словаре самолетов.
    for (auto &itr : adsbee.aircraft_dictionary.dict) 
    {
        const Aircraft1090 &aircraft = itr.second;

        char message[kCSBeeMessageStrMaxLen];
        int16_t message_len_bytes = WriteCSBeeAircraftMessageStr(message, aircraft);
        if (message_len_bytes < 0) 
        {
            //CONSOLE_ERROR("CommsManager::ReportCSBee",
            //              "Encountered an error in WriteCSBeeAircraftMessageStr, error code %d.", message_len_bytes);
            return false;
        }
 
        SendBuf(iface, message, message_len_bytes);
 
    }


    // Записать сообщение CSBee Aircraft для каждого самолета в словаре самолетов.
    for (auto& itr : adsbee.aircraft_dictionary.dict)
    {
        const Aircraft1090& aircraft = itr.second;

        char message[kCSBeeMessageStrMaxLen];
        int16_t message_len_bytes = WriteCSBeeStatisticsMessageStr(message, aircraft);
        if (message_len_bytes < 0)
        {
           return false;
        }

        SendBufSerial(iface, message, message_len_bytes);

    }

    return true;
}
