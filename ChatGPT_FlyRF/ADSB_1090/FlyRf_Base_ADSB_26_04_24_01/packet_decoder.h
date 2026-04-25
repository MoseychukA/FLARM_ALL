#ifndef PACKET_DECODER_H_
#define PACKET_DECODER_H_

#include "data_structures.h"
#include "settings.h"
#include "transponder_packet.h"
#include "aircraft_dictionary.h"

class PacketDecoder 
{
   public:
    static constexpr uint16_t kPacketQueueLen = 100;       //   
    static constexpr uint16_t kDebugMessageQueueLen = 100;//!! 20;  //    

    AircraftDictionary aircraft_dictionary;

    struct PacketDecoderConfig 
    {
        bool enable_1090_error_correction = true;
        bool max_1090_error_correction_num_bits = 1;
    };

    struct DebugMessage
    {
        static const uint16_t kMessageMaxLen = 200;
        char message[kMessageMaxLen + 1] = {'\0'};
        SettingsManager::LogLevel log_level = SettingsManager::LogLevel::kInfo; 
    };

   private:
    PacketDecoderConfig config_;
    Raw1090Packet raw_1090_packet_in_queue_buffer_[kPacketQueueLen];
    Decoded1090Packet decoded_1090_packet_out_queue_buffer_[kPacketQueueLen];
    uint16_t decoded_1090_packet_bit_flip_locations_out_queue_buffer_[kPacketQueueLen];
    DebugMessage debug_message_out_queue_buffer_[kDebugMessageQueueLen];

    static typename PFBQueue<Raw1090Packet>::PFBQueueConfig MakeRawQueueConfig(Raw1090Packet *buffer)
    {
        typename PFBQueue<Raw1090Packet>::PFBQueueConfig cfg;
        cfg.buf_len_num_elements = kPacketQueueLen;
        cfg.buffer = buffer;
        cfg.overwrite_when_full = false;
        return cfg;
    }

    static typename PFBQueue<Decoded1090Packet>::PFBQueueConfig MakeDecodedQueueConfig(Decoded1090Packet *buffer)
    {
        typename PFBQueue<Decoded1090Packet>::PFBQueueConfig cfg;
        cfg.buf_len_num_elements = kPacketQueueLen;
        cfg.buffer = buffer;
        cfg.overwrite_when_full = false;
        return cfg;
    }

    static typename PFBQueue<uint16_t>::PFBQueueConfig MakeBitFlipQueueConfig(uint16_t *buffer)
    {
        typename PFBQueue<uint16_t>::PFBQueueConfig cfg;
        cfg.buf_len_num_elements = kPacketQueueLen;
        cfg.buffer = buffer;
        cfg.overwrite_when_full = false;
        return cfg;
    }

    static typename PFBQueue<DebugMessage>::PFBQueueConfig MakeDebugQueueConfig(DebugMessage *buffer)
    {
        typename PFBQueue<DebugMessage>::PFBQueueConfig cfg;
        cfg.buf_len_num_elements = kDebugMessageQueueLen;
        cfg.buffer = buffer;
        cfg.overwrite_when_full = false;
        return cfg;
    }

   public:
    PacketDecoder(PacketDecoderConfig config_in)
        : config_(config_in),
          raw_1090_packet_in_queue(MakeRawQueueConfig(raw_1090_packet_in_queue_buffer_)),
          decoded_1090_packet_out_queue(MakeDecodedQueueConfig(decoded_1090_packet_out_queue_buffer_)),
          decoded_1090_packet_bit_flip_locations_out_queue(MakeBitFlipQueueConfig(decoded_1090_packet_bit_flip_locations_out_queue_buffer_)),
          debug_message_out_queue(MakeDebugQueueConfig(debug_message_out_queue_buffer_))
    {}

    bool Init() { return true; }
    bool UpdateLogLoop();
    bool UpdateDecoderLoop();

    PFBQueue<Raw1090Packet> raw_1090_packet_in_queue;
    PFBQueue<Decoded1090Packet> decoded_1090_packet_out_queue;
    PFBQueue<uint16_t> decoded_1090_packet_bit_flip_locations_out_queue;
    PFBQueue<DebugMessage> debug_message_out_queue;
};

extern PacketDecoder decoder;

#endif /* PACKET_DECODER_HH_ */
