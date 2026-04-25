#include "packet_decoder.h"
#include "crc.h"
#include <Arduino.h>
#include <string.h>

static inline void packetDecoderLog(const char* prefix, const char* msg)
{
    if (msg == nullptr || msg[0] == '\0')
    {
        return;
    }
    if (prefix != nullptr && prefix[0] != '\0')
    {
        Serial.print(prefix);
    }
    Serial.println(msg);
}

bool PacketDecoder::UpdateLogLoop() 
{
    uint16_t num_messages = debug_message_out_queue.Length();

    for (uint16_t i = 0; i < num_messages; i++) 
    {
        DebugMessage message;
        debug_message_out_queue.Pop(message);
        switch (message.log_level) 
        {
            case SettingsManager::LogLevel::kInfo:
                if (strlen(message.message) > 0)
                {
                    packetDecoderLog("", message.message);
                }
                break;
            case SettingsManager::LogLevel::kWarnings:
                packetDecoderLog("PacketDecoder2..  ", message.message);
                break;
            case SettingsManager::LogLevel::kErrors:
                packetDecoderLog("PacketDecoderp3.. ", message.message);
                break;
            default:
                break;  // Don't do anything when logs are silent.
        }
    }
    uint16_t bit_flip_index;
    while (decoded_1090_packet_bit_flip_locations_out_queue.Pop(bit_flip_index))
    {
        //!!comms_manager.console_printf("PacketDecoder::DecoderLoop4.. Corrected single bit error at bit index %d.\r\n", bit_flip_index);
    }
    return true;
}

//==============================================================================

bool PacketDecoder::UpdateDecoderLoop() 
{
    uint16_t num_packets_to_process = raw_1090_packet_in_queue.Length();
    if (num_packets_to_process == 0) 
    {
        return true;  // Nothing to do.
    }

    for (uint16_t i = 0; i < num_packets_to_process; i++)
    {
        Raw1090Packet raw_packet;
        if (!raw_1090_packet_in_queue.Pop(raw_packet))
        {
            DebugMessage pop_error_message;
            strncpy(pop_error_message.message, "Failed to pop raw packet from input queue.", DebugMessage::kMessageMaxLen);
            pop_error_message.message[DebugMessage::kMessageMaxLen] = '\0';
            pop_error_message.log_level = SettingsManager::LogLevel::kErrors;
            debug_message_out_queue.Push(pop_error_message);
            return false;
        }

        Decoded1090Packet decoded_packet = Decoded1090Packet(raw_packet);
        DebugMessage decode_debug_message;
        decode_debug_message.message[0] = '\0';
        decode_debug_message.log_level = SettingsManager::LogLevel::kInfo;

        if (decoded_packet.IsValid() || decoded_packet.GetDownlinkFormat() == 4 || decoded_packet.GetDownlinkFormat() == 5 || decoded_packet.GetDownlinkFormat() == 11)
        {
            decoded_1090_packet_out_queue.Push(decoded_packet);
            strncpy(decode_debug_message.message, "[VALID     ] ", DebugMessage::kMessageMaxLen);
        }
        else if (config_.enable_1090_error_correction && decoded_packet.GetBufferLenBits() == Raw1090Packet::kExtendedSquitterPacketLenBits)
        {
            // Checksum correction is enabled, and we have a packet worth correcting.
            Raw1090Packet* raw_packet_ptr = decoded_packet.GetRawPtr();
            uint16_t packet_len_bytes = raw_packet_ptr->buffer_len_bits / kBitsPerByte;
            uint8_t raw_buffer[packet_len_bytes];
            WordBufferToByteBuffer(raw_packet_ptr->buffer, raw_buffer, packet_len_bytes);
            int16_t bit_flip_index = crc24_find_single_bit_error(crc24_syndrome(raw_buffer, packet_len_bytes), raw_packet_ptr->buffer_len_bits);
            if (bit_flip_index > 0)
            {
                // Found a single bit error: flip it and push the corrected packet to the output queue.
                flip_bit(raw_packet_ptr->buffer, bit_flip_index);
                decoded_1090_packet_bit_flip_locations_out_queue.Push(bit_flip_index);
                decoded_1090_packet_out_queue.Push(Decoded1090Packet(*raw_packet_ptr));

                strncpy(decode_debug_message.message, "[1FIXD     ] ", DebugMessage::kMessageMaxLen);
            }
            else
            {
                // Checksum correction failed.
               // strncpy(decode_debug_message.message, "[     NOFIX] ", DebugMessage::kMessageMaxLen); 
            }
        }
        else
        {
            // Invalid and not worth correcting.
           // strncpy(decode_debug_message.message, "[     INVLD] ", DebugMessage::kMessageMaxLen);
        }

        // Append packet contents to debug message.
        uint16_t message_len = strlen(decode_debug_message.message);
            message_len += snprintf(decode_debug_message.message + message_len, DebugMessage::kMessageMaxLen - message_len,
        "df=%02d icao=0x%06x 0x", decoded_packet.GetDownlinkFormat(), decoded_packet.GetICAOAddress());
        // Append a print of the packet contents.
        raw_packet.PrintBuffer(decode_debug_message.message + message_len, DebugMessage::kMessageMaxLen - message_len);
 
        debug_message_out_queue.Push(decode_debug_message);

        //char* msg_copy = strdup(decode_debug_message.message); // создаем копию строки
        //if (decode_debug_message_out_queue.Push(msg_copy)) {
        //    decode_message_available = true;
        //}

    }

    return true;
}

