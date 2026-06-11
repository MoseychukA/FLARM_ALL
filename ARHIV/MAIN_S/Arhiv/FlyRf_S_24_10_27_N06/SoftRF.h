/*
 * SoftRF.h
 * Copyright (C) 2016-2023 Linar Yusupov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef FLYRF_H
#define FLYRF_H

#if defined(ARDUINO)
#include <Arduino.h>
#endif /* ARDUINO */

#define FLYRF_IDENT            "FLYRF"
#define FLYRF_FIRMWARE_VERSION "1.3"
#define FLYRF_USB_FW_VERSION   0x0103

#define ENTRY_EXPIRATION_TIME   10 /* seconds */
#define LED_EXPIRATION_TIME     5 /* seconds */
#define EXPORT_EXPIRATION_TIME  5 /* seconds */

/*
 * If you need for SoftRF to operate in wireless
 * client mode - specify your local AP's SSID/PSK:
 *
 * #define MY_ACCESSPOINT_SSID "My_AP_SSID"
 * #define MY_ACCESSPOINT_PSK  "My_AP_PSK"
 *
 * If SoftRF's built-in AP is not stable enough for you, consider
 * to use "reverse" operation when your smartphone is acting
 * as an AP for the SoftRF unit as a client:
 *
 * #define MY_ACCESSPOINT_SSID "AndroidAP"
 * #define MY_ACCESSPOINT_PSK  "12345678"
 */

// Default mode is AP with
// SSID: SoftRF-XXXXXX
// KEY:  12345678
// IP: 192.168.1.1
// NETMASK: 255.255.255.0
#define MY_ACCESSPOINT_SSID ""
#define MY_ACCESSPOINT_PSK  ""

#define RELAY_DST_PORT  12390
#define RELAY_SRC_PORT  (RELAY_DST_PORT - 1)

#define NMEA_UDP_PORT     10110
#define NMEA_TCP_PORT     2000

#if !defined(SERIAL_IN_BR)
/*
 * 9600 is default value of NMEA baud rate
 * for most of GNSS modules
 * being used in SoftRF project
 */
#define SERIAL_IN_BR      9600
#endif
#if !defined(SERIAL_IN_BITS)
#define SERIAL_IN_BITS    SERIAL_8N1
#endif

/*
 * 38400 is known as maximum baud rate
 * that HC-05 Bluetooth module
 * can handle without symbols loss.
 *
 * Applicable for Standalone Edition. Inherited by most of other SoftRF platforms.
 */
#define STD_OUT_BR        115200//38400
#define STD_OUT_BITS      SERIAL_8N1

#if !defined(SERIAL_OUT_BR)
#define SERIAL_OUT_BR     STD_OUT_BR
#endif
#if !defined(SERIAL_OUT_BITS)
#define SERIAL_OUT_BITS   STD_OUT_BITS
#endif

#define UAT_RECEIVER_BR   2000000

#if defined(PREMIUM_PACKAGE) && !defined(RASPBERRY_PI)
#define ENABLE_AHRS
#endif /* PREMIUM_PACKAGE */

typedef struct UFO {
    uint8_t   raw[34];
    time_t    timestamp;

    uint8_t   protocol;

    uint32_t  addr;
    uint8_t   addr_type;
    float     latitude;
    float     longitude;
    float     altitude;
    float     pressure_altitude;
    float     course;     /* CoG */
    float     speed;      /* ground speed in knots */
    uint8_t   aircraft_type;

    float     vs; /* feet per minute */

    bool      stealth;
    bool      no_track;

    int8_t    ns[4];
    int8_t    ew[4];

    float     geoid_separation; /* metres */
    uint16_t  hdop; /* cm */
    int8_t    rssi; /* SX1276 only */

    /* 'legacy' specific data */
    float     distance;
    float     bearing;
    int8_t    alarm_level;

    /* bitmap of issued voice/tone/ble/... alerts */
    uint8_t   alert;

    /* ADS-B (ES, UAT, GDL90) specific data */
    uint8_t   callsign[8];
} ufo_t;

typedef struct hardware_info {
    byte  model;
    byte  revision;
    byte  soc;
    byte  rf;
    byte  gnss;
    byte  baro;
    byte  display;
    byte  storage;
    byte  rtc;
    byte  imu;
    byte  mag;
    byte  pmu;
} hardware_info_t;

typedef struct IODev_ops_struct {
  const char name[16];
  void (*setup)();
  void (*loop)();
  void (*fini)();
  int (*available)(void);
  int (*read)(void);
  size_t (*write)(const uint8_t *buffer, size_t size);
} IODev_ops_t;

typedef struct DB_ops_struct {
  bool (*setup)();
  bool (*fini)();
  bool (*query)(uint8_t, uint32_t, char *, size_t);
} DB_ops_t;

enum
{
	FLYRF_MODE_NORMAL,
	FLYRF_MODE_WATCHOUT,
	FLYRF_MODE_BRIDGE,
	FLYRF_MODE_RELAY,
	FLYRF_MODE_TXRX_TEST,
	FLYRF_MODE_LOOPBACK,
	FLYRF_MODE_UAV,
	FLYRF_MODE_RECEIVER,
	FLYRF_MODE_CASUAL,
};

enum
{
	FLYRF_MODEL_UNKNOWN,
	FLYRF_MODEL_STANDALONE,
	FLYRF_MODEL_PRIME,
	FLYRF_MODEL_UAV,
	FLYRF_MODEL_PRIME_MK2,
	FLYRF_MODEL_RASPBERRY,
	FLYRF_MODEL_UAT,
	FLYRF_MODEL_SKYVIEW,
	FLYRF_MODEL_RETRO,
	FLYRF_MODEL_SKYWATCH,
	FLYRF_MODEL_DONGLE,
	FLYRF_MODEL_OCTAVE,
	FLYRF_MODEL_UNI,
	FLYRF_MODEL_WEBTOP_SERIAL,
	FLYRF_MODEL_MINI,
	FLYRF_MODEL_BADGE,
	FLYRF_MODEL_ES,
	FLYRF_MODEL_BRACELET,
	FLYRF_MODEL_ACADEMY,
	FLYRF_MODEL_LEGO,
	FLYRF_MODEL_WEBTOP_USB,
	FLYRF_MODEL_PRIME_MK3,
	FLYRF_MODEL_BALKAN,
	FLYRF_MODEL_HAM,
	FLYRF_MODEL_MIDI,
};

enum
{
	FLYRF_SHUTDOWN_NONE,
	FLYRF_SHUTDOWN_DEFAULT,
	FLYRF_SHUTDOWN_DEBUG,
	FLYRF_SHUTDOWN_ABORT,
	FLYRF_SHUTDOWN_WATCHDOG,
	FLYRF_SHUTDOWN_NMEA,
	FLYRF_SHUTDOWN_BUTTON,
	FLYRF_SHUTDOWN_LOWBAT,
	FLYRF_SHUTDOWN_SENSOR,
};

enum
{
	STORAGE_NONE,
	STORAGE_FLASH,
	STORAGE_CARD,
	STORAGE_FLASH_AND_CARD,
};

enum
{
	IMU_NONE,
	ACC_BMA423,
	ACC_ADXL362,
	IMU_MPU6886,
	IMU_MPU9250,
	IMU_BNO080,
	IMU_ICM20948,
	IMU_QMI8658,
};

enum
{
	MAG_NONE,
	MAG_AK8963,
	MAG_AK09916,
	MAG_IIS2MDC,
	MAG_QMC6310,
};

extern ufo_t ThisAircraft;
extern hardware_info_t hw_info;
extern const float txrx_test_positions[90][2] PROGMEM;
extern const uint8_t whitening_pattern[] PROGMEM;


extern void shutdown(int);

#define TXRX_TEST_NUM_POSITIONS (sizeof(txrx_test_positions) / sizeof(float) / 2)
#define TXRX_TEST_ALTITUDE    438.0
#define TXRX_TEST_COURSE      280.0
#define TXRX_TEST_SPEED       50.0
#define TXRX_TEST_VS          -300.0

#define StdOut  Serial

#endif /* FLYRF_H */
