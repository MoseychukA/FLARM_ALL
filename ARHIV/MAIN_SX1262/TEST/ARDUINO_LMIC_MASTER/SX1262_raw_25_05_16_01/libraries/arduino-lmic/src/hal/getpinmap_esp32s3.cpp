
#if defined(ARDUINO_esp32_esp32s3)

#include <arduino_lmic_hal_boards.h>
#include <Arduino.h>

#include "../lmic/oslmic.h"

namespace Arduino_LMIC {

    class HalConfiguration_esp32_s3 : public HalConfiguration_t {
    public:
        enum DIGITAL_PINS : uint8_t {
            PIN_SX1262_NSS = 46,
            PIN_SX1262_NRESET = 7,
            PIN_SX1262_BUSY = 18,
            PIN_SX1262_DIO1 = 1,
            PIN_SX1262_DIO2 = 2,
            PIN_SX1262_DIO3 = HalPinmap_t::UNUSED_PIN,
            PIN_SX1262_ANT_SWITCH_RX = HalPinmap_t::UNUSED_PIN,
            PIN_SX1262_ANT_SWITCH_TX_BOOST = HalPinmap_t::UNUSED_PIN,
            PIN_SX1262_ANT_SWITCH_TX_RFO = HalPinmap_t::UNUSED_PIN,
            PIN_VDD_BOOST_ENABLE = HalPinmap_t::UNUSED_PIN,
        };

        virtual u1_t queryBusyPin(void) override { return HalConfiguration_esp32_s3::PIN_SX1262_BUSY; };

        virtual bool queryUsingDcdc(void) override { return true; };

        virtual bool queryUsingDIO2AsRfSwitch(void) override { return true; };

        virtual bool queryUsingDIO3AsTCXOSwitch(void) override { return true; };
    };

    static HalConfiguration_esp32_s3 myConfig;

    static const HalPinmap_t myPinmap =
    {
        .nss = HalConfiguration_esp32_s3::PIN_SX1262_NSS,
        .rxtx = HalConfiguration_esp32_s3::PIN_SX1262_ANT_SWITCH_RX,
        .rst = HalConfiguration_esp32_s3::PIN_SX1262_NRESET,
        .dio = {
            HalConfiguration_esp32_s3::PIN_SX1262_DIO1,
            HalConfiguration_esp32_s3::PIN_SX1262_DIO2,
            HalConfiguration_esp32_s3::PIN_SX1262_DIO3,
        },
        .rxtx_rx_active = 0,
        .rssi_cal = 8,
        .spi_freq = 8000000, /* 8MHz */
        .pConfig = &myConfig
    };

    const HalPinmap_t* GetPinmap_esp32_s3(void) {
        return &myPinmap;
    }

}; // namespace Arduino_LMIC

#endif // defined(ARDUINO_TTGO_T_BEAM_S3)