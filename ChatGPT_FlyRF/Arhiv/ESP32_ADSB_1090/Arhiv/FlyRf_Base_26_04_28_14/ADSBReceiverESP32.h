#pragma once

#include <Arduino.h>

#ifndef ADSB_ESP32_RX_PIN
#define ADSB_ESP32_RX_PIN 2
#endif

#ifndef ADSB_ESP32_OK_PULSE_PIN
#define ADSB_ESP32_OK_PULSE_PIN 4
#endif

#ifndef ADSB_ESP32_THRESHOLD_PWM_PIN
#define ADSB_ESP32_THRESHOLD_PWM_PIN 48
#endif

#ifndef ADSB_ESP32_DMA_CAPTURE
#define ADSB_ESP32_DMA_CAPTURE 1
#endif

// I2S DMA needs BCLK/WS output pins even when only GPIO2 data is sampled.
// These pins are not connected to the ADS-B comparator; leave them unused on the PCB.
#ifndef ADSB_ESP32_DMA_BCLK_PIN
#define ADSB_ESP32_DMA_BCLK_PIN 14
#endif

#ifndef ADSB_ESP32_DMA_WS_PIN
#define ADSB_ESP32_DMA_WS_PIN 47
#endif

#ifndef ADSB_ESP32_DMA_BIT_RATE_HZ
#define ADSB_ESP32_DMA_BIT_RATE_HZ 8000000UL
#endif

#ifndef ADSB_ESP32_CRC_CONFIRM_HITS
#define ADSB_ESP32_CRC_CONFIRM_HITS 1
#endif

struct ADSBReceiverESP32Diag
{
    bool ready;
    bool taskRunning;
    uint32_t sampledFrames;
    uint32_t packetsAccepted;
    uint32_t packetsRejected;
    uint32_t preambleRejected;
    uint32_t crcRejected;
    uint32_t apAccepted;
    uint32_t df17Accepted;
    uint32_t squawkAccepted;
    uint32_t aliasSuppressed;
    uint32_t apSuppressed;
    uint32_t crcPending;
    uint32_t multiPhaseUsed;
    uint32_t containerUpdates;
    uint32_t lastPacketMs;
    uint32_t lastAddress;
    uint16_t lastSquawk;
    uint8_t lastDf;
    uint8_t lastTypeCode;
    uint8_t lastPacketBits;
    uint8_t lastPhase;
    uint16_t lastSampleSlots;
    uint16_t thresholdPwmDuty;
};

void ADSBReceiverESP32_setup();
void ADSBReceiverESP32_loop();
bool ADSBReceiverESP32_getDiag(ADSBReceiverESP32Diag& outDiag);
bool ADSBReceiverESP32_internalEnabled();
void ADSBReceiverESP32_applySettings();
uint16_t ADSBReceiverESP32_thresholdPwmDuty();
