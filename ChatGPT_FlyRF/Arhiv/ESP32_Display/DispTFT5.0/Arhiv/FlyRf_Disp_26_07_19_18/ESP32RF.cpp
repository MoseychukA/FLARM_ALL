/*
  FlyRF display model for GL050001C0-40.

  This module contains no objects from the previous SPI graphics layer. It keeps
  the original FlyRF data processing, alarms, scale control and button logic,
  then sends one hardware-independent state to the RGB-panel renderer.
*/

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_INA219.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>
#include <string.h>

#include "ESP32RF.h"
#include "HardwareConfig.h"
#include "ProjectHardware.h"
#include "GL050001C0_40_Display.h"
#include "EEPROMRF.h"
#include "RS485Display.h"
#include "DisplayRemote.h"
#include "DeviceInfo.h"
#include "System.h"

namespace
{
constexpr uint32_t MODEL_UPDATE_MS = DATA_MEASURE_THRESHOLD;
constexpr uint32_t SOS_ON_MS = 3000UL;
constexpr uint32_t SOS_OFF_MS = 500UL;
constexpr float EARTH_RADIUS_M = 6371000.0f;
// Усреднение за окно до 16 секунд: изменения напряжения, тока и процента
// батареи отображаются заметно плавнее, чем в версии 08 (4 секунды).
constexpr uint8_t POWER_FILTER_SAMPLES = 16;
constexpr uint32_t POWER_SAMPLE_INTERVAL_MS = 1000UL;

struct RadarScale
{
    uint16_t ringMeters;
    uint16_t maximumMeters;
};

// Same eight scales as the original display implementation.
constexpr RadarScale RADAR_SCALES[] = {
    {32000, 64000}, {16000, 32000}, {8000, 16000}, {4000, 8000},
    {2000, 4000}, {1000, 2000}, {500, 1000}, {250, 500}
};
constexpr uint8_t RADAR_SCALE_COUNT = sizeof(RADAR_SCALES) / sizeof(RADAR_SCALES[0]);

struct TargetFilter
{
    uint32_t address;
    int speed[speed_array_size];
    int altitude[altitude_array_size];
    uint8_t speedPos;
    uint8_t altitudePos;
    uint8_t speedCount;
    uint8_t altitudeCount;
    int filteredSpeed;
    int filteredAltitude;
    int hysteresisAltitude;
    int previousTrendAltitude;
    int8_t verticalTrend;
    uint32_t trendMs;
};

GL050001C0_40_State g_state = {};
TargetFilter g_filters[MAX_TRACKING_OBJECTS] = {};
Adafruit_INA219 g_ina219;
TaskHandle_t g_buttonTask = nullptr;
volatile uint8_t g_pendingButton = 0;
bool g_displayReady = false;
bool g_inaReady = false;
bool g_powerValid = false;
float g_voltageV = 0.0f;
float g_currentMa = 0.0f;
uint8_t g_batteryPercent = 0;
uint32_t g_lastInaProbeMs = 0;
uint32_t g_lastInaReadMs = 0;
float g_voltageSamples[POWER_FILTER_SAMPLES] = {};
float g_currentSamples[POWER_FILTER_SAMPLES] = {};
float g_voltageSampleSum = 0.0f;
float g_currentSampleSum = 0.0f;
uint8_t g_powerSamplePosition = 0;
uint8_t g_powerSampleCount = 0;
uint8_t g_invalidPowerSamples = 0;
uint32_t g_manualRangeActivatedMs = 0;
uint8_t g_lastAutomaticScale = 0;
uint8_t g_lastEffectiveScale = 0;
uint32_t g_sosCycleMs = 0;
uint32_t g_trackerCycleMs = 0;
char g_lastTrackerMessage[161] = {};

int normalizeHeading(int value)
{
    value %= 360;
    return value < 0 ? value + 360 : value;
}

int16_t toInt16(int value)
{
    if (value < -32768) return -32768;
    if (value > 32767) return 32767;
    return (int16_t)value;
}

uint16_t toUint16(int value)
{
    if (value < 0) return 0;
    if (value > 65535) return 65535;
    return (uint16_t)value;
}

bool finiteCoordinate(float latitude, float longitude)
{
    return isfinite(latitude) && isfinite(longitude) &&
           latitude >= -90.0f && latitude <= 90.0f &&
           longitude >= -180.0f && longitude <= 180.0f &&
           !(latitude == 0.0f && longitude == 0.0f);
}

bool ownCoordinates(float& latitude, float& longitude)
{
    if (finiteCoordinate(ThisAircraft.local_latitude, ThisAircraft.local_longitude))
    {
        latitude = ThisAircraft.local_latitude;
        longitude = ThisAircraft.local_longitude;
        return true;
    }
    if (finiteCoordinate(ThisAircraft.latitude, ThisAircraft.longitude))
    {
        latitude = ThisAircraft.latitude;
        longitude = ThisAircraft.longitude;
        return true;
    }
    if (GNSS_coordinatesValid() && finiteCoordinate(GNSS_latitude(), GNSS_longitude()))
    {
        latitude = GNSS_latitude();
        longitude = GNSS_longitude();
        return true;
    }
    return false;
}

bool updateTargetPolar(ufo_t& target)
{
    float ownLat = 0.0f;
    float ownLon = 0.0f;
    if (!ownCoordinates(ownLat, ownLon) ||
        !finiteCoordinate(target.latitude, target.longitude))
    {
        return target.distance > 0.0f;
    }

    const float lat1 = radians(ownLat);
    const float lat2 = radians(target.latitude);
    const float dLat = radians(target.latitude - ownLat);
    const float dLon = radians(target.longitude - ownLon);
    const float a = sinf(dLat * 0.5f) * sinf(dLat * 0.5f) +
                    cosf(lat1) * cosf(lat2) *
                    sinf(dLon * 0.5f) * sinf(dLon * 0.5f);
    const float c = 2.0f * atan2f(sqrtf(a), sqrtf(fmaxf(0.0f, 1.0f - a)));
    target.distance = EARTH_RADIUS_M * c;

    const float y = sinf(dLon) * cosf(lat2);
    const float x = cosf(lat1) * sinf(lat2) -
                    sinf(lat1) * cosf(lat2) * cosf(dLon);
    target.bearing = (float)normalizeHeading((int)lroundf(degrees(atan2f(y, x))));
    return true;
}

void resetFilter(TargetFilter& filter, uint32_t address)
{
    memset(&filter, 0, sizeof(filter));
    filter.address = address;
}

void updateFilter(TargetFilter& filter, const ufo_t& aircraft, int ownAltitude)
{
    if (filter.address != aircraft.addr) resetFilter(filter, aircraft.addr);

    const int speed = (int)lroundf(aircraft.speed);
    filter.speed[filter.speedPos] = speed;
    filter.speedPos = (filter.speedPos + 1U) % speed_array_size;
    if (filter.speedCount < speed_array_size) ++filter.speedCount;
    long speedSum = 0;
    for (uint8_t i = 0; i < filter.speedCount; ++i) speedSum += filter.speed[i];
    filter.filteredSpeed = filter.speedCount ? (int)(speedSum / filter.speedCount) : speed;

    const int altitude = (int)lroundf(aircraft.altitude);
    if (altitude != 0)
    {
        filter.altitude[filter.altitudePos] = altitude;
        filter.altitudePos = (filter.altitudePos + 1U) % altitude_array_size;
        if (filter.altitudeCount < altitude_array_size) ++filter.altitudeCount;
    }
    long altitudeSum = 0;
    for (uint8_t i = 0; i < filter.altitudeCount; ++i) altitudeSum += filter.altitude[i];
    filter.filteredAltitude = filter.altitudeCount ?
        (int)(altitudeSum / filter.altitudeCount) : altitude;

    if (filter.hysteresisAltitude == 0 ||
        abs(filter.filteredAltitude - filter.hysteresisAltitude) > 10)
    {
        filter.hysteresisAltitude = filter.filteredAltitude;
    }

    const uint32_t now = millis();
    if (filter.trendMs == 0 || (uint32_t)(now - filter.trendMs) >= 2100UL)
    {
        if (filter.previousTrendAltitude != 0)
        {
            filter.verticalTrend = filter.hysteresisAltitude > filter.previousTrendAltitude ? 1 :
                                   filter.hysteresisAltitude < filter.previousTrendAltitude ? -1 : 0;
        }
        filter.previousTrendAltitude = filter.hysteresisAltitude;
        filter.trendMs = now;
    }
    (void)ownAltitude;
}

uint8_t alarmLevel(uint32_t horizontalM, int verticalM, bool verticalValid)
{
    if (settings == nullptr || settings->alarm_attention <= 0 ||
        horizontalM > (uint32_t)settings->alarm_attention) return 0;
    if (verticalValid && settings->alarm_height > 0 &&
        verticalM > settings->alarm_height) return 0;
    if (settings->alarm_danger > 0 && horizontalM <= (uint32_t)settings->alarm_danger) return 3;
    if (settings->alarm_warning > 0 && horizontalM <= (uint32_t)settings->alarm_warning) return 2;
    return 1;
}

uint8_t automaticScale(uint32_t nearestM)
{
    for (int i = RADAR_SCALE_COUNT - 1; i >= 0; --i)
        if (nearestM <= RADAR_SCALES[i].maximumMeters) return (uint8_t)i;
    return 0;
}

uint8_t configuredScale()
{
    if (settings == nullptr || settings->radar_range_mode == 0) return g_lastAutomaticScale;
    const uint8_t index = settings->radar_range_mode - 1U;
    return index < RADAR_SCALE_COUNT ? index : 0;
}

uint8_t effectiveScale()
{
    if (set_view_range != 0)
    {
        const uint8_t index = set_view_range - 1U;
        if (index < RADAR_SCALE_COUNT) return index;
    }
    return configuredScale();
}

void clearManualScale()
{
    set_view_range = 0;
    g_manualRangeActivatedMs = 0;
}

void selectSmallerScale()
{
    uint8_t index = effectiveScale();
    if (index + 1U < RADAR_SCALE_COUNT) ++index;
    set_view_range = index + 1U;
    g_manualRangeActivatedMs = millis();
}

void pushButton(uint8_t event)
{
    if (event == 0) return;
    g_pendingButton = event;
    RS485Display_setLocalButtonEvent(event);
}

uint8_t takeButton()
{
    if (RS485Display_hasIncomingButton()) return RS485Display_takeIncomingButton();
    const uint8_t event = g_pendingButton;
    g_pendingButton = 0;
    return event;
}

void buttonTask(void*)
{
    pinMode(DISPLAY_BUTTON_PIN, INPUT_PULLUP);
    bool stable = digitalRead(DISPLAY_BUTTON_PIN) == DISPLAY_BUTTON_ACTIVE_LEVEL;
    bool rawPrevious = stable;
    bool pressed = false;
    bool longSent = false;
    bool waitingSecond = false;
    uint32_t rawChanged = millis();
    uint32_t pressedAt = 0;
    uint32_t releasedAt = 0;

    for (;;)
    {
        const uint32_t now = millis();
        const bool raw = digitalRead(DISPLAY_BUTTON_PIN) == DISPLAY_BUTTON_ACTIVE_LEVEL;
        if (raw != rawPrevious) { rawPrevious = raw; rawChanged = now; }
        if ((uint32_t)(now - rawChanged) >= DISPLAY_BUTTON_DEBOUNCE_MS && raw != stable)
        {
            stable = raw;
            if (stable) { pressed = true; longSent = false; pressedAt = now; }
            else if (pressed)
            {
                pressed = false;
                if (!longSent)
                {
                    if (waitingSecond && (uint32_t)(now - releasedAt) <= DISPLAY_BUTTON_DOUBLE_MS)
                    { waitingSecond = false; pushButton(2); }
                    else { waitingSecond = true; releasedAt = now; }
                }
            }
        }
        if (pressed && !longSent && (uint32_t)(now - pressedAt) >= DISPLAY_BUTTON_LONG_MS)
        { longSent = true; waitingSecond = false; pushButton(3); }
        if (waitingSecond && !pressed && (uint32_t)(now - releasedAt) > DISPLAY_BUTTON_DOUBLE_MS)
        { waitingSecond = false; pushButton(1); }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void processControls()
{
    if (set_view_range != 0 && g_manualRangeActivatedMs != 0 &&
        (uint32_t)(millis() - g_manualRangeActivatedMs) >= BUTTON_OFF_DELAY)
        clearManualScale();

    switch (takeButton())
    {
        case 1: (void)Tracker_confirmActiveTextMessage(); break;
        case 2: selectSmallerScale(); break;
        case 3: clearManualScale(); break;
        default: break;
    }
}

uint8_t batteryPercent(float voltage)
{
    // Линейная шкала внешнего аккумулятора:
    // 7.00 В = 0%, 12.00 В = 100%, между ними 20% на каждый вольт.
    if (voltage <= INA219_BATTERY_MIN_V) return 0;
    if (voltage >= INA219_BATTERY_MAX_V) return 100;
    return (uint8_t)constrain((int)lroundf(
        (voltage - INA219_BATTERY_MIN_V) * 100.0f /
        (INA219_BATTERY_MAX_V - INA219_BATTERY_MIN_V)), 0, 100);
}

bool probeIna219()
{
    Wire.begin(PROJECT_PIN_I2C_SDA, PROJECT_PIN_I2C_SCL);
    Wire.beginTransmission(PROJECT_INA219_I2C_ADDR);
    if (Wire.endTransmission() != 0) return false;
    g_ina219.begin(&Wire);
    g_ina219.setCalibration_32V_2A();
    return true;
}

void addPowerSample(float voltage, float currentMa)
{
    // Первым корректным измерением заполняем всё окно. Следующее значение
    // изменяет среднее только на 1/16, поэтому после включения показания не
    // проходят короткую фазу быстрого усреднения.
    if (g_powerSampleCount == 0U)
    {
        for (uint8_t index = 0; index < POWER_FILTER_SAMPLES; ++index)
        {
            g_voltageSamples[index] = voltage;
            g_currentSamples[index] = currentMa;
        }
        g_voltageSampleSum = voltage * POWER_FILTER_SAMPLES;
        g_currentSampleSum = currentMa * POWER_FILTER_SAMPLES;
        g_powerSampleCount = POWER_FILTER_SAMPLES;
        g_powerSamplePosition = 0U;
        g_voltageV = voltage;
        g_currentMa = currentMa;
        g_batteryPercent = batteryPercent(g_voltageV);
        g_powerValid = true;
        return;
    }

    if (g_powerSampleCount == POWER_FILTER_SAMPLES)
    {
        g_voltageSampleSum -= g_voltageSamples[g_powerSamplePosition];
        g_currentSampleSum -= g_currentSamples[g_powerSamplePosition];
    }
    else
    {
        ++g_powerSampleCount;
    }

    g_voltageSamples[g_powerSamplePosition] = voltage;
    g_currentSamples[g_powerSamplePosition] = currentMa;
    g_voltageSampleSum += voltage;
    g_currentSampleSum += currentMa;
    g_powerSamplePosition = (g_powerSamplePosition + 1U) % POWER_FILTER_SAMPLES;

    g_voltageV = g_voltageSampleSum / g_powerSampleCount;
    g_currentMa = g_currentSampleSum / g_powerSampleCount;
    g_batteryPercent = batteryPercent(g_voltageV);
    g_powerValid = true;
}

void updatePower()
{
    const uint32_t now = millis();
    if (!g_inaReady)
    {
        if (g_lastInaProbeMs && (uint32_t)(now - g_lastInaProbeMs) < 5000UL) return;
        g_lastInaProbeMs = now;
        g_inaReady = probeIna219();
        if (!g_inaReady) { g_powerValid = false; return; }
        g_lastInaReadMs = 0;
    }
    if (g_lastInaReadMs &&
        (uint32_t)(now - g_lastInaReadMs) < POWER_SAMPLE_INTERVAL_MS) return;
    g_lastInaReadMs = now;

    digitalWrite(POWER_ON_PIN, LOW);
    delay(20);
    const float bus = g_ina219.getBusVoltage_V();
    const float shunt = g_ina219.getShuntVoltage_mV();
    float current = fabsf(g_ina219.getCurrent_mA());
    digitalWrite(POWER_ON_PIN, HIGH);
    const float voltage = bus + shunt / 1000.0f;
    const bool sampleValid = isfinite(voltage) && isfinite(current) &&
                             voltage > 0.0f && voltage < 40.0f &&
                             current < 10000.0f;
    if (sampleValid)
    {
        g_invalidPowerSamples = 0;
        addPowerSample(voltage, current);
    }
    else if (++g_invalidPowerSamples >= 3U)
    {
        // Один выброс не гасит показания; недостоверными они становятся
        // только после трёх последовательных ошибочных измерений.
        g_powerValid = false;
    }
}

bool sosVisible()
{
    if (settings == nullptr || settings->display_sos == 0) return false;
    aux_t aux = {};
    RS485Display_getIncomingAux(&aux);
    if (!aux.new_SOS_flag_M) { g_sosCycleMs = 0; return false; }
    const uint32_t now = millis();
    if (!g_sosCycleMs) g_sosCycleMs = now;
    return (uint32_t)(now - g_sosCycleMs) % (SOS_ON_MS + SOS_OFF_MS) < SOS_ON_MS;
}

bool trackerVisible(const char* message)
{
    if (!message || !*message) return false;
    if (strncmp(message, g_lastTrackerMessage, sizeof(g_lastTrackerMessage)) != 0)
    {
        strncpy(g_lastTrackerMessage, message, sizeof(g_lastTrackerMessage) - 1U);
        g_lastTrackerMessage[sizeof(g_lastTrackerMessage) - 1U] = '\0';
        g_trackerCycleMs = millis();
    }
    const uint32_t phase = (uint32_t)(millis() - g_trackerCycleMs) % 5000UL;
    return phase < 4000UL;
}

int latestLoRaRssi()
{
    int result = 0;
    uint32_t newest = 0;
    const uint32_t now = millis();
    for (int i = 0; i < MAX_TRACKING_OBJECTS; ++i)
    {
        const ufo_t& item = Container[i];
        if (!item.addr || item.signal_source != TRAFFIC_SOURCE_FLARM_LORA || item.rssi_LoRa >= 0) continue;
        const uint32_t age = now - item.lastUpdate;
        if (age > CONTAINER_STALE_TIMEOUT_MS) continue;
        if (result == 0 || age < newest) { result = item.rssi_LoRa; newest = age; }
    }
    return result;
}

void copyTarget(GL050001C0_40_Target& output, int index, int ownAltitude)
{
    const ufo_t& input = Container[index];
    const TargetFilter& filter = g_filters[index];
    memset(&output, 0, sizeof(output));
    output.address = input.addr;
    memcpy(output.callsign, input.callsign, 8);
    output.callsign[8] = '\0';
    output.squawk = toUint16(input.squawk);
    output.altitudeM = toInt16(filter.hysteresisAltitude);
    output.relativeAltitudeM = toInt16(filter.hysteresisAltitude - ownAltitude);
    output.speedKmh = toUint16(filter.filteredSpeed);
    output.courseDeg = (uint16_t)normalizeHeading((int)lroundf(input.course));
    output.latitude = input.latitude;
    output.longitude = input.longitude;
    output.signalSource = input.signal_source;
    output.signalRssi = input.signal_source == TRAFFIC_SOURCE_ADSB_DUMP1090 ?
                        input.rssi_rp2040 : input.rssi_LoRa;
    output.distanceM = toUint16((int)lroundf(input.distance));
    output.bearingDeg = (uint16_t)normalizeHeading((int)lroundf(input.bearing));
    output.verticalRate = filter.verticalTrend;
    output.alarmLevel = (uint8_t)constrain((int)input.alarm_level, 0, 3);
}

void processTraffic()
{
    const int ownAltitude = (int)lroundf(ThisAircraft.altitude);
    uint32_t nearest = RADAR_SCALES[0].maximumMeters;
    g_state.targetCount = 0;

    for (int i = 0; i < MAX_TRACKING_OBJECTS; ++i)
    {
        ufo_t& item = Container[i];
        if (!item.addr) { if (g_filters[i].address) resetFilter(g_filters[i], 0); continue; }
        updateTargetPolar(item);
        updateFilter(g_filters[i], item, ownAltitude);
        const uint32_t distance = (uint32_t)max(0, (int)lroundf(item.distance));
        if (distance > 0 && distance < nearest) nearest = distance;
        const int relativeAltitude = g_filters[i].hysteresisAltitude - ownAltitude;
        const bool verticalValid = g_filters[i].hysteresisAltitude != 0 && ownAltitude != 0;
        item.alarm_level = alarmLevel(distance, abs(relativeAltitude), verticalValid);
        if (g_state.targetCount < GL050001C0_40_MAX_TARGETS)
            copyTarget(g_state.targets[g_state.targetCount++], i, ownAltitude);
    }

    g_lastAutomaticScale = automaticScale(nearest);
    g_lastEffectiveScale = effectiveScale();
    g_state.radarRangeM = RADAR_SCALES[g_lastEffectiveScale].ringMeters;
}

void buildPlaneTable()
{
    g_state.tableTargetCount = 0;
    const uint8_t mode = settings ? settings->display_set : INFO_DISPLAY_MAXI;
    g_state.tableVisible = mode == INFO_DISPLAY_COORDINATE || mode == INFO_DISPLAY_MAXI;
    if (!g_state.tableVisible || g_state.trackerMessageVisible) return;

    const int ownAltitude = (int)lroundf(ThisAircraft.altitude);
    for (int i = 0; i < MAX_TRACKING_OBJECTS &&
                    g_state.tableTargetCount < GL050001C0_40_MAX_TARGETS; ++i)
    {
        if (!Container[i].addr) continue;
        if (mode == INFO_DISPLAY_COORDINATE &&
            !finiteCoordinate(Container[i].latitude, Container[i].longitude)) continue;
        copyTarget(g_state.tableTargets[g_state.tableTargetCount++], i, ownAltitude);
    }
}

void buildState()
{
    updatePower();
    g_state = {};
    g_state.baseConnected = RS485Display_lastRxMs() != 0 &&
        (uint32_t)(millis() - RS485Display_lastRxMs()) < 3000UL;
    g_state.timeValid = GNSS_timeValid();
    g_state.gnssValid = GNSS_coordinatesValid();
    g_state.powerValid = g_powerValid;
    g_state.sosActive = sosVisible();
    g_state.showPowerVoltage = !settings || settings->power_voltage_view;
    g_state.showPowerCurrent = !settings || settings->power_current_view;
    g_state.showPowerBattery = !settings || settings->power_battery_view;
    g_state.showLoraStatus = !settings || settings->rssi_view == VIEW_RSSI_ON;
    g_state.showGpsStatus = !settings || settings->gps_state_view;
    g_state.showLanStatus = settings && settings->lan_state_view && Remote_lanStatusReceived();
    g_state.lanReady = Remote_lanReady();
    g_state.lanLinkUp = Remote_lanLinkUp();
    g_state.lanUdpWorking = Remote_lanUdpWorking();
    Remote_lanIp(g_state.lanIp);
    g_state.lanUdpPort = Remote_lanUdpPort();
    g_state.lanTxPackets = Remote_lanTxPackets();
    g_state.lanRxPackets = Remote_lanRxPackets();
    g_state.hour = GNSS_hour();
    g_state.minute = GNSS_minute();
    g_state.satellites = GNSS_satellitesValid() ? GNSS_satellites() : 0;
    g_state.batteryPercent = g_batteryPercent;
    g_state.voltageV = g_voltageV;
    g_state.currentMa = g_currentMa;
    g_state.latitude = GNSS_latitude();
    g_state.longitude = GNSS_longitude();
    g_state.altitudeM = toInt16((int)lroundf(ThisAircraft.altitude));
    g_state.speedKmh = toUint16((int)lroundf(ThisAircraft.speed));
    g_state.courseDeg = (uint16_t)normalizeHeading((int)lroundf(SystemDisplayCourseDeg()));
    g_state.loraTxPackets = Remote_loraTxPackets();
    g_state.loraRxPackets = Remote_loraRxPackets();
    g_state.loraRfHz = Remote_loraRfHz();
    g_state.loraRssiDb = toInt16(latestLoRaRssi());
    g_state.rs485TxPackets = RS485Display_txPackets();
    g_state.rs485RxPackets = RS485Display_rxPackets();

    processTraffic();

    const char* message = Tracker_getActiveTextMessage();
    g_state.trackerMessageVisible = Tracker_hasActiveTextMessage() && trackerVisible(message);
    if (message)
    {
        strncpy(g_state.trackerMessage, message, sizeof(g_state.trackerMessage) - 1U);
        g_state.trackerMessage[sizeof(g_state.trackerMessage) - 1U] = '\0';
    }
    else { g_lastTrackerMessage[0] = '\0'; g_trackerCycleMs = 0; }

    aux_t aux = {};
    RS485Display_getIncomingAux(&aux);
    g_state.gnssStatusVisible = !Remote_baseTestMode() &&
                                (aux.gps_waiting_M || aux.gps_no_data_M);
    displayAllPlanes();
}
} // namespace

uint8_t set_view_range = 0;

// Preserved public point from the original project.  The table is now built in
// the GL model instead of drawing rows into an intermediate display object.
void displayAllPlanes()
{
    buildPlaneTable();
}

void Display_clearManualRadarRangeOverride()
{
    clearManualScale();
}

void Display_setup()
{
    if (g_displayReady) return;
    // Сначала выделяем крупные непрерывные DMA bounce-буферы RGB. Задача
    // кнопки создаётся после панели и уже не может фрагментировать этот блок.
    g_displayReady = GL050001C0_40_setup();
    if (g_displayReady)
    {
        if (!g_buttonTask)
            xTaskCreatePinnedToCore(buttonTask, "displayButton", 3072, nullptr, 2,
                                    &g_buttonTask, ARDUINO_RUNNING_CORE);
        GL050001C0_40_showStartup("FlyRf_Disp_26_07_19_18");
        buildState();
        GL050001C0_40_updateState(g_state);
    }
}

void Display_loop()
{
    if (!g_displayReady) return;
    processControls();
    static uint32_t lastModelMs = 0;
    const uint32_t now = millis();
    if (!lastModelMs || (uint32_t)(now - lastModelMs) >= MODEL_UPDATE_MS)
    {
        lastModelMs = now;
        buildState();
        GL050001C0_40_updateState(g_state);
    }
    GL050001C0_40_loop();
}

void Display_powerOff()
{
    if (g_displayReady) GL050001C0_40_showPowerOff();
}
