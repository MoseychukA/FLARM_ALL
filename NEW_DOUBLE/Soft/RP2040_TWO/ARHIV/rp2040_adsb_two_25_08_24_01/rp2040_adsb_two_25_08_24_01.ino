#include <mutex>
#include "Arduino.h"
#include "pico/multicore.h"
#include "pico/binary_info.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "adsbee.h"
#include "bsp.h"
#include "data_structures.h"

#include <LittleFS.h>
#include <hardware/pio.h>
#include <hardware/dma.h>
#include <hardware/regs/dreq.h>
#include <pico/multicore.h>
#include <hardware/sync.h>
#include "ringbuffer.h"
#include "adsb_decoder.h"
#include "config.h"
#include "esp_packet.h"
#include "utils.h"
#include <atomic>
#include <string>

//===============================================================

//struct RawMessage {
//    uint32_t rx_buffer[kMaxPacketLenWords32];  // исходные данные
//    uint16_t rx_buffer_len_words32;           // длина в словах
//    int16_t source_in;                        // источник
//    int16_t sigs_dbm_in;                      // уровень сигнала
//    int16_t sigq_db_in;                       // качество сигнала
//    uint64_t mlat_48mhz_64bit_counts;         // MLAT счетчик
//
//    // Новые поля для расширенной информации
//    float latitude;                           // широта
//    float longitude;                          // долгота
//    int altitude_m_geo;                       // высота (геоид)
//    int altitude_m_baro;                      // высота по баро‑датчику
//    int vert_rate_mpm;                        // вертикальная скорость
//    int baro_geo_diff_m;                      // баро-гео разность
//    uint32_t cpr_timestamp;                   // время хранения пар even/odd
//    bool pos_surface;                         // поверхность (SURFACE)
//    uint16_t buffer_len_bits;                 // длина пакета в битах
//    uint8_t aircraft_type;                    // тип судна
//    uint8_t src_type;                         // тип источника
//    uint16_t reserved;                        // резерв
//};



//=================================================================



BSP bsp = BSP({});
ADSBee adsbee = ADSBee({});

static constexpr uint8_t UART2_TX = 4, UART2_RX = 5;

/*Настройки только для теста*/
const int ledPin = 15;                       // the number of the LED pin
int ledState = LOW;                          // ledState used to set the LED
unsigned long previousMillis = 0;            // will store last time LED was updated
const long interval = 1000;                  // interval at which to blink (milliseconds)

const int ledPin1 = 25;
int ledState1 = LOW;
unsigned long previousMillis1 = 0;
const long interval1 = 300;
//=============================================================================================

static constexpr uint8_t PIN_RX1 = 19;
static constexpr uint8_t PIN_RX2 = 22;
static constexpr uint8_t PIN_RX3 = 18;
static constexpr uint8_t PIN_PRE1 = 17;
static constexpr uint8_t PIN_PRE2 = 20;
static constexpr uint8_t PIN_PRE3 = 23;
static constexpr uint8_t PIN_RSSI_ADC = 26;
static constexpr uint8_t PIN_BLINK_CORE0 = 15;
static constexpr uint8_t PIN_BLINK_CORE1 = 25;
static constexpr uint8_t UART1_TX = 2;
static constexpr uint8_t UART1_RX = 1;
//static constexpr uint8_t UART2_TX = 4;
//static constexpr uint8_t UART2_RX = 5;
static constexpr uint32_t SAMPLE_FREQ_HZ = 20000000; // 20 MHz
static constexpr uint32_t DMA_RING_WORDS = 1024;      // 1024 words -> 4096 bytes ring
static constexpr size_t PACKET_QUEUE_CAP = 128;


struct PioDmaChannel
{
    PIO pio;
    uint sm;
    int dma_chan;
    volatile uint32_t* ring;
    uint32_t ring_words;
    volatile uint32_t widx;
    volatile uint32_t ridx;
    uint dreq;
    uint pin;
    uint pre_pin;
};

static PioDmaChannel chn[3];
//static LockFreePacketQueue pktq[3];
static volatile uint16_t rssi_adc_last = 0;
static float rssi_ema = 0.0f;
static Config g_cfg;
static String cmd_line;




static uint32_t crc32_(const uint8_t* data, size_t len)
{
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) crc = (crc >> 1) ^ (0xEDB88320 & (-(int)(crc & 1)));
    } return ~crc;
}


static void handle_usb_command(const String& cmd) {
    String u = cmd; u.trim(); u.toUpperCase();
    if (u == "HELP") {
        Serial.println("Commands: HELP, SHOW, SAVE, LOAD, PROFILE <NORMAL|HIGH-EMI|URBAN|REMOTE>, RAW ON|OFF, FORMAT <LOG|CSV|JSON|NMEA|UBX>, REF <lat> <lon>");
        return;
    }
    if (u == "SHOW") {
        print_config(g_cfg);
        return;
    }
    if (u == "SAVE") {
        save_config(g_cfg);
        Serial.println("Saved to /config.txt");
        return;
    }
    if (u == "LOAD") {
        load_config(g_cfg);
        Serial.println("Loaded from /config.txt");
        return;
    }
    if (u.startsWith("PROFILE ")) {
        String p = u.substring(8);
        p.trim();
        if (p == "NORMAL") g_cfg.profile = Profile::Normal;
        else if (p == "HIGH-EMI") g_cfg.profile = Profile::HighEMI;
        else if (p == "URBAN") g_cfg.profile = Profile::Urban;
        else if (p == "REMOTE") g_cfg.profile = Profile::Remote;
        else {
            Serial.println("Bad profile");
            return;
        } Serial.print("Profile set: ");
        Serial.println(profile_name(g_cfg.profile));
        return;
    }
    if (u == "RAW ON") {
        g_cfg.output_raw = true;
        Serial.println("RAW=ON");
        return;
    }
    if (u == "RAW OFF") {
        g_cfg.output_raw = false;
        Serial.println("RAW=OFF");
        return;
    }
    if (u.startsWith("FORMAT ")) {
        String f = u.substring(7);
        f.trim();
        int fmt = 0;
        if (f == "LOG") fmt = 0;
        else if (f == "CSV") fmt = 1;
        else if (f == "JSON") fmt = 2;
        else if (f == "NMEA") fmt = 3;
        else if (f == "UBX") fmt = 4;
        else {
            Serial.println("Bad format");
            return;
        } g_cfg.out_format = fmt;
        Serial.print("Format set: ");
        Serial.println(f);
        return;
    }
    if (u.startsWith("REF ")) {
        float a, b;
        int c = sscanf(u.c_str() + 4, "%f %f", &a, &b);
        if (c == 2) {
            g_cfg.ref_lat = a;
            g_cfg.ref_lon = b;
            Serial.print("REF set: ");
            Serial.print(a, 6);
            Serial.print(", ");
            Serial.println(b, 6);
        }
        else Serial.println("REF usage: REF <lat> <lon>");
        return;
    }
    Serial.println("Unknown command");
}

static void send_to_esp32(const FlightOutput& fo)
{
    struct __attribute__((packed)) Frame {
        uint8_t sof[2];
        uint16_t len;
        FlightOutput fo;
        uint32_t crc;
    } f;
    f.sof[0] = 0x55;
    f.sof[1] = 0xAA;
    f.len = sizeof(FlightOutput);
    memcpy(&f.fo, &fo, sizeof(FlightOutput));
    f.crc = crc32_((uint8_t*)&f.fo, sizeof(FlightOutput));
    Serial2.write((uint8_t*)&f, sizeof(f));
}

static void emit_debug_raw(const RawMessage& rm)
{
    Serial.print("CH");
    Serial.print(rm.channel + 1);
    Serial.print(" ");
    Serial.print(rm.bits);
    Serial.print("b ");
    for (size_t i = 0; i < rm.bytes; i++)
    {
        if (i) Serial.print(' ');
        char buf[4];
        sprintf(buf, "%02X", rm.payload[i]);
        Serial.print(buf);
    }
    Serial.println();
}

static void print_by_format(const DecodedFrame& df, const FlightOutput& fo, int rssi)
{
    switch (g_cfg.out_format)
    {
    case 0: Serial.print("ICAO="); Serial_printHex(df.icao, 6); Serial.print(" src="); Serial.print(df.src_type); Serial.print(" cs="); Serial.print(df.callsign); Serial.print(" lat="); Serial.print(df.lat, 6); Serial.print(" lon="); Serial.print(df.lon, 6); Serial.print(" spd="); Serial.print(fo.speed); Serial.print(" alt="); Serial.print(fo.altitude); Serial.print(" vs="); Serial.print(fo.vert_rate); Serial.print(" trk="); Serial.print(fo.course); Serial.print(" rssi="); Serial.print(rssi); Serial.println(); break;
    case 1: Serial.print(df.icao, HEX); Serial.print(','); Serial.print(df.callsign); Serial.print(','); Serial.print(df.lat, 6); Serial.print(','); Serial.print(df.lon, 6); Serial.print(','); Serial.print(fo.altitude); Serial.print(','); Serial.print(fo.speed); Serial.print(','); Serial.print(fo.vert_rate); Serial.print(','); Serial.print(fo.course); Serial.print(','); Serial.println(rssi); break;
    case 2: Serial.printf("{icao: %06X, src:%d, cs: %s ,lat :%.6f,lon:%.6f,alt:%d,spd:%d,vs:%d,trk:%d,rssi:%d}", df.icao, df.src_type, df.callsign, df.lat, df.lon, fo.altitude, fo.speed, fo.vert_rate, fo.course, rssi); break;
    case 3: Serial.printf("$PADSB,%06X,%s,%.6f,%.6f,%d,%d,%d,%d*", df.icao, df.callsign, df.lat, df.lon, fo.altitude, fo.speed, fo.vert_rate, fo.course); break;
    case 4: Serial.println("UBX not implemented"); break;
    }
}

//static void setup_pio_dma_channel(PioDmaChannel& c, PIO pio, uint sm, uint pin_in, uint pre_pin, int8_t prog_offset)
//{
//    c.pio = pio; c.sm = sm; c.pin = pin_in; c.pre_pin = pre_pin; c.ring_words = DMA_RING_WORDS; c.ridx = 0; c.widx = 0;
//    c.ring = (volatile uint32_t*)malloc(sizeof(uint32_t) * DMA_RING_WORDS); memset((void*)c.ring, 0, sizeof(uint32_t) * DMA_RING_WORDS);
//    adsb_sampler_program_init(pio, sm, prog_offset, pin_in, SAMPLE_FREQ_HZ);
//    uint dreq = (pio == pio0) ? (sm == 0 ? DREQ_PIO0_RX0 : sm == 1 ? DREQ_PIO0_RX1 : sm == 2 ? DREQ_PIO0_RX2 : DREQ_PIO0_RX3) : (sm == 0 ? DREQ_PIO1_RX0 : sm == 1 ? DREQ_PIO1_RX1 : sm == 2 ? DREQ_PIO1_RX2 : DREQ_PIO1_RX3);
//    c.dreq = dreq; int chan = dma_claim_unused_channel(true); c.dma_chan = chan;
//    dma_channel_config cfg = dma_channel_get_default_config(chan);
//    channel_config_set_read_increment(&cfg, false);
//    channel_config_set_write_increment(&cfg, true);
//    channel_config_set_dreq(&cfg, dreq);
//    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
//    channel_config_set_ring(&cfg, true, __builtin_ctz(DMA_RING_WORDS) + 2);
//    dma_channel_configure(chan, &cfg, (void*)c.ring, (const void*)&pio->rxf[sm], 0x7FFFFFFF, true);
//}

//static void process_packets_loop()
//{
//    RawMessage rm;
//    DecodedFrame df;
//    while (pktq[0].pop(rm) || pktq[1].pop(rm) || pktq[2].pop(rm))
//    {
//        if (g_cfg.output_raw) emit_debug_raw(rm);
//        CrcStatus cs = check_and_fix_crc(rm);
//        if (cs == CRC_FIXED1) g_cfg.stat_crc_fix++;
//        else if (cs == CRC_FAIL) g_cfg.stat_crc_fail++;
//        if (decode_adsb_frame(rm, df, g_cfg))
//        {
//            g_cfg.stat_msgs++;
//            FlightOutput fo = { 0 };
//            fo.addr = df.icao;
//            fo.Squawk = df.squawk;
//            memcpy(fo.flight, df.callsign, sizeof(fo.flight));
//            fo.altitude = df.altitude_m_geo;
//            fo.pressure_altitude = df.altitude_m_baro;
//            fo.speed = df.speed_kmh;
//            fo.course = df.track_deg;
//            fo.vert_rate = df.vert_rate_mpm;
//            fo.latitude = df.lat;
//            fo.longitude = df.lon;
//            fo.seen = df.seen_time_ms;
//            fo.timestamp = millis();
//            fo.signal_source = 1;
//            fo.aircraft_type = df.aircraft_type;
//            print_by_format(df, fo, (int)rssi_adc_last);
//            send_to_esp32(fo);
//        }
//    }
//}


//static const uint16_t kMaxPacketLenWords32 = 4;
//
//struct Raw1090Packet {
//    uint32_t rx_buffer[kMaxPacketLenWords32];
//    uint16_t rx_buffer_len_words32;
//    int16_t source_in;
//    int16_t sigs_dbm_in;
//    int16_t sigq_db_in;
//    uint64_t mlat_48mhz_64bit_counts;
//
//    //// конструктор
//    //Raw1090Packet(
//    //    uint32_t buffer[], uint16_t len_words,
//    //    int16_t source_in = -1, int16_t sigs_dbm_in = INT16_MIN, int16_t sigq_db_in = INT16_MIN,
//    //    uint64_t mlat_counts = 0
//    //);
//};
//
//static constexpr uint16_t kMaxNumTransponderPackets = 100;  // Определяет размер кольцевого буфера ADSBPacket (PFBQueue).
//Raw1090Packet raw_1090_packet_queue_buffer_[kMaxNumTransponderPackets];
//PFBQueue<Raw1090Packet> raw_1090_packet_queue = PFBQueue<Raw1090Packet>({ .buf_len_num_elements = kMaxNumTransponderPackets, .buffer = raw_1090_packet_queue_buffer_ });
//
//
//raw_1090_packet_queue
//
//static constexpr uint16_t kPacketQueueLen = 100;       // Длина очереди пакетов
//Raw1090Packet raw_1090_packet_in_queue_buffer_[kPacketQueueLen];
////PFBQueue<Raw1090Packet> raw_1090_packet_in_queue = PFBQueue<Raw1090Packet>({ .buf_len_num_elements = kPacketQueueLen, .buffer = raw_1090_packet_in_queue_buffer_ });
//PFBQueue<Raw1090Packet> raw_1090_packet_queue = PFBQueue<Raw1090Packet>({ .buf_len_num_elements = kPacketQueueLen, .buffer = raw_1090_packet_in_queue_buffer_ });

//bool UpdatePacketLoop()
//{
//    uint16_t num_packets_to_process = raw_1090_packet_queue.Length();
//    if (num_packets_to_process == 0)
//    {
//        return true;  // Nothing to do.
//    }
//
//    for (uint16_t i = 0; i < num_packets_to_process; i++)
//    {
//        Raw1090Packet raw_packet;
//        if (!raw_1090_packet_queue.Pop(raw_packet))
//        {
// 
//            return false;
//        }
//
//        Serial.println(" Call: ");
//    }
//
//    return true;
//}

//Raw1090Packet raw_packet;

bool construct_raw_message_from_rx_buffer(const Raw1090Packet& pkt, RawMessage& rm) 
{
    // В этом примере предполагается, что rx_buffer содержит сырые байты 
    // (например, кадр ADS-B в виде битов или слов).
    // Вам нужно понять точный формат, и преобразовать его в RawMessage.

    // Пример: считаем, что rx_buffer содержит 56- или 112-битный пакет (разбить байты)

    // Проверка длины
    if (pkt.kMaxPacketLenWords32 == 0) return false;

    // Допустим, что rx_buffer сразу содержит байты (подразумевается конвертация из слов)
    // Создадим временный буфер байт
    uint8_t buf[1024]; // максимум
    size_t byte_len = pkt.kMaxPacketLenWords32 * 4; // слова в байты

    // Копируем из rx_buffer в буфер байтов
    for (size_t i = 0; i < pkt.kMaxPacketLenWords32; i++) 
    {
        uint32_t word = pkt.buffer[i];
        buf[i * 4 + 0] = (word >> 0) & 0xFF;
        buf[i * 4 + 1] = (word >> 8) & 0xFF;
        buf[i * 4 + 2] = (word >> 16) & 0xFF;
        buf[i * 4 + 3] = (word >> 24) & 0xFF;
    }

    // Инструкция: определить, есть ли действительно пакет, например, по старт‑байту или префиксу.
    // В чистом виде — принять весь буфер до длины, указанной в rx_buffer_len_words32.
    // В реальности — по формату пакета (например, кадру ADS-B) ищем преамбулу.

    // В данном случае, сделайте простую проверку, и сконвертируйте байты в RawMessage:
    // Это — упрощенная версия, вам надо адаптировать под конкретный формат.

    // В этом примере — полностью копируем буфер в RawMessage
    memset(&rm, 0, sizeof(rm));
    size_t payload_bytes = (byte_len >= 14) ? 14 : byte_len; // максимум 14 байт (112 бита)
    memcpy(rm.payload, buf, payload_bytes);
    rm.bytes = payload_bytes;
    rm.bits = payload_bytes * 8;
    // В реальности, нужно считать правильное положение, CRC и длину пакета, исходя из протокола.

    // Пример: если есть стандартная преамбула, поиск её по буферу и установка t0 (при наличии)
    // Пока предполагаем, что всё в порядке.
    return true;
}

//=====================================================================

extern std::atomic<Raw1090Packet*> latest_packets[3];

// Функция преобразования
RawMessage convertPacketToRawMessage(const Raw1090Packet& pkt) 
{
    RawMessage rm;

    // Размер в битах
    rm.buffer_len_bits = pkt.kMaxPacketLenWords32 * 32;

    // В байтах (окурс: используем только buffer в виде массива слов)
    rm.bytes = (rm.buffer_len_bits + 7) / 8;

    // Копируем buffer
    for (int i = 0; i < pkt.kMaxPacketLenWords32; i++)
    {
        rm.rx_buffer[i] = pkt.buffer[i];
    }

    // Остальные поля
    rm.source_in = pkt.source;
    //rm.sigs_dbm_in = pkt.sigs_dbm_in;
    rm.mlat_48mhz_64bit_counts = pkt.mlat_48mhz_64bit_counts;

    return rm;
}


void process_latest_packet(int sm_idx) 
{
    auto pkt_ptr = latest_packets[sm_idx].load(std::memory_order_relaxed);
    if (pkt_ptr != nullptr) 
    {
        // Создаем копию
        Raw1090Packet pkt_copy;
        memcpy(&pkt_copy, &(*pkt_ptr), sizeof(Raw1090Packet));

        // Конвертация в RawMessage
        RawMessage rm = convertPacketToRawMessage(pkt_copy);

        // Расшифровка
        DecodedFrame df;
        if (decode_adsb_frame(rm, df, g_cfg)) {
            // Формируем FlightOutput
            FlightOutput fo = { 0 };
            fo.addr = df.icao;
            fo.Squawk = df.squawk;
            memcpy(fo.flight, df.callsign, sizeof(fo.flight));
            fo.altitude = df.altitude_m_geo;
            fo.pressure_altitude = df.altitude_m_baro;
            fo.speed = df.speed_kmh;
            fo.course = df.track_deg;
            fo.vert_rate = df.vert_rate_mpm;
            fo.latitude = df.lat;
            fo.longitude = df.lon;
            fo.seen = df.seen_time_ms;
            fo.timestamp = millis();
            fo.signal_source = 1;
            fo.aircraft_type = df.aircraft_type;

            // Передача ESP
            send_to_esp32(fo);
            // Вывод
            print_by_format(df, fo, (int)rssi_adc_last);
        }
    }
}

//=====================================================================


void setup() 
{
    bi_decl(bi_program_description("ADSBee 1090 ADSB Receiver"));

    Serial.begin(115200);
    unsigned long t0 = millis();
    while (!Serial && !Serial.dtr() && (millis() - t0) < 8000) delay(10);
    delay(2000);
    Serial.println("RP2040 ADS-B RX v2 FIX2 starting...");
    Serial2.setTX(UART2_TX);
    Serial2.setRX(UART2_RX);
    Serial2.begin(921600);

    adsbee.Init();

    LittleFS.begin();
    load_config(g_cfg);
    if (fabs(g_cfg.ref_lat) < 0.0001f && fabs(g_cfg.ref_lon) < 0.0001f)
    {
        g_cfg.ref_lat = 55.93574f;
        g_cfg.ref_lon = 37.34873f;
    }
    Serial.print("Profile: ");
    Serial.println(profile_name(g_cfg.profile));
    analogReadResolution(12);
    analogRead(PIN_RSSI_ADC);

   // for (int i = 0; i < 3; i++) pktq[i].begin(PACKET_QUEUE_CAP);
   // multicore_launch_core1(core1_entry);
    Serial.println("Setup complete. Type HELP for commands.");
    Serial2.println("Setup complete.");
    Serial.print("\r\nSetup End\r\n");
}


void setup1()
{
    pinMode(ledPin1, OUTPUT);
}



void loop() 
{
    for (int sm_idx = 0; sm_idx < 3; sm_idx++) 
    {
        process_latest_packet(sm_idx);
    }
   
    //Raw1090Packet rx_pkt;
    //if (raw_1090_packet_queue.Pop(rx_pkt)) 
    //{
    //    RawMessage rm;
    //    if (construct_raw_message_from_rx_buffer(rx_pkt, rm)) {
    //        // Теперь можно вызвать декодер
    //        DecodedFrame df;
    //        if (decode_adsb_frame(rm, df, g_cfg)) {
    //           // Serial.print("ICAO: "); Serial.printHex(df.icao, 6);
    //            Serial.print(" Call: "); Serial.print(df.callsign);
    //            Serial.print(" Lat: "); Serial.print(df.lat, 6);
    //            Serial.print(" Lon: "); Serial.print(df.lon, 6);
    //            Serial.print(" Alt: "); Serial.print(df.altitude_m_geo);
    //            Serial.print(" Spd: "); Serial.print(df.speed_kmh);
    //            Serial.println();
    //        }
    //    }
    //}

    while (Serial.available())
    {
        char c = Serial.read();
        if (c == '\r' || c == '\n')
        {
            if (cmd_line.length())
            {
                handle_usb_command(cmd_line);
                cmd_line = "";
            }
        }
        else
        {
            cmd_line += c;
            if (cmd_line.length() > 200) cmd_line.remove(0, cmd_line.length() - 200);
        }
    }


  /*  process_packets_loop();*/

   

     unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) 
    {
        previousMillis = currentMillis;
        if (ledState == LOW) 
        {
            ledState = HIGH;
        }
        else 
        {
            ledState = LOW;
        }
        digitalWrite(ledPin, ledState);
    }
}


void loop1()
{

    unsigned long currentMillis1 = millis();
    if (currentMillis1 - previousMillis1 >= interval1)
    { 
        previousMillis1 = currentMillis1;
        if (ledState1 == LOW)
        {
            ledState1 = HIGH;
        }
        else
        {
            ledState1 = LOW;
        }
       digitalWrite(ledPin1, ledState1);
    }
}
