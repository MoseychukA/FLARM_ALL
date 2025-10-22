#include "comms.h"
#include "hal.h"  // For timestamping.
#include "settings.h"
#include "comms_reporting.h"

// Reporting utils.
#include "beast_utils.h"
#include "csbee_utils.h"
#include "raw_utils.h"
#include "adsbee.h"  // For access to the aircraft dictionary.

AircraftDictionary &aircraft_dictionary = adsbee.aircraft_dictionary;



bool CommsManager::UpdateReporting(const ReportSink *sinks, const SettingsManager::ReportingProtocol *sink_protocols, uint16_t num_sinks, const CompositeArray::RawPackets *packets_to_report) 
{
    bool ret = true;
    uint32_t timestamp_ms = get_time_since_boot_ms();

    // Build lists of sinks for each reporting protocol.
    ReportSink raw_sinks[SettingsManager::kNumSerialInterfaces];
    ReportSink beast_sinks[SettingsManager::kNumSerialInterfaces];
    ReportSink csbee_sinks[SettingsManager::kNumSerialInterfaces];
 

    uint16_t num_raw_sinks = 0, num_beast_sinks = 0, num_csbee_sinks = 0;

    for (uint16_t i = 0; i < num_sinks; i++) {
        switch (sink_protocols[i]) {
            case SettingsManager::kNoReports:
                break;  // Not a reporting protocol.
            case SettingsManager::kRaw:
                raw_sinks[num_raw_sinks++] = sinks[i];
                break;
            case SettingsManager::kBeast:
                beast_sinks[num_beast_sinks++] = sinks[i];
                break;
            case SettingsManager::kCSBee:
                csbee_sinks[num_csbee_sinks++] = sinks[i];
                break;
            default:
                CONSOLE_ERROR("CommsManager::UpdateReporting",
                              "Unrecognized reporting protocol %s on interface %s, skipping.",
                              SettingsManager::kSerialInterfaceStrs[sinks[i]],
                              SettingsManager::kReportingProtocolStrs[sink_protocols[i]]);
                break;  // Not a periodic report protocol.
        }
    }

    /**  Report Raw Packets **/
    if (packets_to_report->len_bytes > sizeof(CompositeArray::RawPackets::Header)) {
        // Only report raw packets if they are provided (still send locally decoded reports even if no raw packets).
        if (!ReportRaw(raw_sinks, num_raw_sinks, *packets_to_report)) {
            CONSOLE_ERROR("CommsManager::UpdateReporting", "Error during ReportRaw.");
            ret = false;
        }
        // Send all inclusive Beast reports.
        if (!ReportBeast(beast_sinks, num_beast_sinks, *packets_to_report)) {
            CONSOLE_ERROR("CommsManager::UpdateReporting", "Error during ReportBeast.");
            ret = false;
        }
    }

    /** Locally Decoded Reports **/
    if (num_csbee_sinks > 0 && timestamp_ms - last_csbee_report_timestamp_ms_ >= kCSBeeReportingIntervalMs) {
        if (!ReportCSBee(csbee_sinks, num_csbee_sinks)) {
            CONSOLE_ERROR("CommsManager::UpdateReporting", "Error during ReportCSBee.");
            ret = false;
        }
        last_csbee_report_timestamp_ms_ = timestamp_ms;
    }

    return ret;
}

bool CommsManager::ReportRaw(ReportSink *sinks, uint16_t num_sinks, const CompositeArray::RawPackets &packets) 
{
    char error_msg[CompositeArray::RawPackets::kErrorMessageMaxLen] = {0};
    if (!packets.IsValid(error_msg)) {
        CONSOLE_ERROR("CommsManager::ReportRaw", "Invalid CompositeArray::RawPackets: %s", error_msg);
        return false;
    }

    bool ret = true;
    for (uint16_t i = 0; i < packets.header->num_mode_s_packets; i++) {
        char raw_frame_buf[kRawModeSFrameMaxNumChars];
        uint16_t num_bytes_in_frame = BuildRawModeSFrame(packets.mode_s_packets[i], raw_frame_buf);
        for (uint16_t j = 0; j < num_sinks; j++) {
            ret &= SendBuf(sinks[j], (char *)raw_frame_buf, num_bytes_in_frame);
        }
    }
    return ret;
}

bool CommsManager::ReportBeast(ReportSink *sinks, uint16_t num_sinks, const CompositeArray::RawPackets &packets, SettingsManager::ReportingProtocol protocol)
{
    char error_msg[CompositeArray::RawPackets::kErrorMessageMaxLen] = {0};
    if (!packets.IsValid(error_msg)) 
    {
        CONSOLE_ERROR("CommsManager::ReportBeast", "Invalid CompositeArray::RawPackets: %s", error_msg);
        return false;
    }

    bool ret = true;
    for (uint16_t i = 0; i < packets.header->num_mode_s_packets; i++) 
    {
        uint8_t beast_frame_buf[BeastReporter::kModeSBeastFrameMaxLenBytes];
        uint16_t num_bytes_in_frame = BeastReporter::BuildModeSBeastFrame(beast_frame_buf, packets.mode_s_packets[i]);

        for (uint16_t j = 0; j < num_sinks; j++) 
        {
            ret &= SendBuf(sinks[j], (char *)beast_frame_buf, num_bytes_in_frame);
        }
    }


    return ret;
}

bool CommsManager::ReportCSBee(ReportSink *sinks, uint16_t num_sinks) 
{
    bool ret = true;

    // Write out a CSBee Aircraft message for each aircraft in the aircraft dictionary.
    for (auto &itr : aircraft_dictionary.dict) {
        char message[kCSBeeMessageStrMaxLen];
        int message_len_bytes = -1;

        if (ModeSAircraft *mode_s_aircraft = get_if<ModeSAircraft>(&(itr.second)); mode_s_aircraft) 
        {
            message_len_bytes = WriteCSBeeModeSAircraftMessageStr(message, *mode_s_aircraft);
        }
        else
        {
            CONSOLE_WARNING("CommsManager::ReportCSBee", "Unknown aircraft type in dictionary for UID 0x%lx.",
                            itr.first);
            continue;
        }

        if (message_len_bytes < 0) 
        {
            CONSOLE_ERROR("CommsManager::ReportCSBee",
                          "Encountered an error in WriteCSBeeAircraftMessageStr, error code %d.", message_len_bytes);
            return false;
        }
        for (uint16_t i = 0; i < num_sinks; i++)
        {
            ret &= SendBuf(sinks[i], message, message_len_bytes);
        }
    }

    // Write a CSBee Statistics message.
    char message[kCSBeeMessageStrMaxLen];
    int16_t message_len_bytes = WriteCSBeeStatisticsMessageStr(
        message,                                                     // Buffer to write into.
        aircraft_dictionary.metrics.demods_1090,                     // DPS
        aircraft_dictionary.metrics.raw_squitter_frames,             // RAW_SFPS
        aircraft_dictionary.metrics.valid_squitter_frames,           // SFPS
        aircraft_dictionary.metrics.raw_extended_squitter_frames,    // RAW_ESFPS
        aircraft_dictionary.metrics.valid_extended_squitter_frames,  // ESFPS
        aircraft_dictionary.GetNumAircraft(),                     // NUM_AIRCRAFT
        0u,                                                       // TSCAL
        get_time_since_boot_ms() / 1000                           // UPTIME
    );
    if (message_len_bytes < 0) 
    {
        CONSOLE_ERROR("CommsManager::ReportCSBee",
                      "Encountered an error in WriteCSBeeStatisticsMessageStr, error code %d.", message_len_bytes);
        return false;
    }

    for (uint16_t i = 0; i < num_sinks; i++) {
        ret &= SendBuf(sinks[i], message, message_len_bytes);
    }
    return ret;
}

