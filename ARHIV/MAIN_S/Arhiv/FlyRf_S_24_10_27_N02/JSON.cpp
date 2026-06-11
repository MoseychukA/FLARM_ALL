/*
 * JSONHelper.cpp
 * Copyright (C) 2018-2023 Linar Yusupov
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

#if defined(RASPBERRY_PI) || defined(ARDUINO_ARCH_NRF52) || defined(ARDUINO_ARCH_RP2040)

#include "SoC.h"
#include <TinyGPS++.h>
#include "EEPROMRF.h"
#include "RF.h"
#include "Baro.h"
#include "EPD.h"
#include "TrafficHelper.h"
#include "NMEA.h"
#include "D1090.h"

#undef DEPRECATED
#include "JSON.h"

extern eeprom_t eeprom_block;
extern settings_t *settings;

#endif /* RASPBERRY_PI || ARDUINO_ARCH_NRF52 || ARDUINO_ARCH_RP2040 */

