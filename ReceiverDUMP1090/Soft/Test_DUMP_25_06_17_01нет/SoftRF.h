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

#define FLYRF_IDENT            "FlyRF"
#define FLYRF_FIRMWARE_VERSION "1.3"
#define FLYRF_USB_FW_VERSION   0x0103

#define ENTRY_EXPIRATION_TIME   10 /* seconds */
#define LED_EXPIRATION_TIME     5 /* seconds */
#define EXPORT_EXPIRATION_TIME  5 /* seconds */
#define MSG_EXPORT_EXPIRATION_TIME  2 /* seconds */
#define MSG_OFF_EXPIRATION_TIME  600 /* seconds */


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
#define STD_OUT_BR        115200 
#define STD_OUT_BITS      SERIAL_8N1

#if !defined(SERIAL_OUT_BR)
#define SERIAL_OUT_BR     STD_OUT_BR
#endif
#if !defined(SERIAL_OUT_BITS)
#define SERIAL_OUT_BITS   STD_OUT_BITS
#endif

#define UAT_RECEIVER_BR   2000000


typedef struct UFO {
    uint8_t   raw[34];
    time_t    timestamp;
    uint8_t   protocol;
    uint32_t  addr;
    uint8_t   addr_type;
    float     latitude;
    float     longitude;
    float     old_latitude;
    float     old_longitude;
    float     altitude;
    float     pressure_altitude;
    float     course;     /* CoG */
    float     speed;      /* ground speed in knots */
    uint8_t   aircraft_type;
    char      flight[16];    // Flight number
    int       vert_rate;     // Vertical rate.
    int       Squawk;        // Squawk
    time_t    timemsg;       // Время передачи сообщения о координатах стороннего самолета

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

    uint8_t   signal_source;
    time_t    seen;           // Time at which the last packet was received
    unsigned int  pSignal;
    uint8_t   hour_msg;
    uint8_t   min_msg;
    uint16_t  delay_time_msg;
    /* ADS-B (ES, UAT, GDL90) specific data */
    uint8_t   callsign[8];
    float     test_latitude;
    float     test_longitude;
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
    FLYRF_MODE_TXRX_TEST0,
    FLYRF_MODE_TXRX_TEST1,
    FLYRF_MODE_TXRX_TEST2,
    FLYRF_MODE_TXRX_TEST3,
    FLYRF_MODE_TXRX_TEST4,
    FLYRF_MODE_TXRX_TEST5,
    FLYRF_MODE_RECEIVER,
};

enum
{
	FLYRF_MODEL_UNKNOWN,
	FLYRF_MODEL_STANDALONE,
	FLYRF_MODEL_PRIME,
	FLYRF_MODEL_DONGLE,
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


extern ufo_t ThisAircraft;
extern hardware_info_t hw_info;
extern const float txrx_test_positions[90][2] PROGMEM;
extern const uint8_t whitening_pattern[] PROGMEM;


extern void shutdown(int);

#define TXRX_TEST_NUM_POSITIONS (sizeof(txrx_test_positions) / sizeof(float) / 2)
#define TXRX_TEST_ALTITUDE    438.0
#define TXRX_TEST_COURSE      280.0
#define TXRX_TEST_SPEED        50.0
#define TXRX_TEST_VS         -300.0

#define StdOut  Serial

#define VOLUME_0              1.8  // Напряжение разряженного аккумулятора 3.2 вольта
#define VOLUME_100            2.4  // Напряжение заряженного аккумулятора 4.2 вольта

#endif /* FLYRF_H */
