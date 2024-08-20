/*
 * GNSSHelper.h
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

#ifndef GNSSHELPER_H
#define GNSSHELPER_H

#include <TinyGPS++.h>

typedef enum
{
  GNSS_MODULE_NONE,
  GNSS_MODULE_NMEA, /* generic NMEA */
  GNSS_MODULE_U6,   /* Ublox 6 */
 } gnss_id_t;

typedef struct gnss_chip_ops_struct {
  gnss_id_t (*probe)();
  uint16_t  gga_ms;
  uint16_t  rmc_ms;
} gnss_chip_ops_t;


#define NMEA_EXP_TIME  3500 /* 3.5 seconds */

bool isValidGNSSFix  (void);
byte GNSS_setup      (void);
void GNSS_loop       (void);
void GNSS_fini       (void);
void GNSSTimeSync    (void);
void PickGNSSFix     (void);
int LookupSeparation (float, float);
void displayInfo(void);

extern TinyGPSPlus gnss;
extern volatile unsigned long PPS_TimeMarker;
extern const char *GNSS_name[];

#endif /* GNSSHELPER_H */
