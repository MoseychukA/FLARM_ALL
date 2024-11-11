/*
 * OLEDHelper.h
 * Copyright (C) 2019-2023 Linar Yusupov
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

#ifndef TFTHELPER_H
#define TFTHELPER_H

#define isTimeToTFT()          (millis() - TFTTimeMarker > 500)
#include "Configuration_ESP32.h"
#include "TFT_eSPI.h"


#define TFT_Class TFT_eSPI               // класс поддержки TFT
//TFT_Class* tftDC;
//TFT_Class* getDC() { return tftDC; };
 
void TFT_setup(void);
void TFT_loop(void);
void Draw_circular_scale(void);
//void drawMessage(void);
//int mb_strlen(char* source, int letter_n);
//void clearMSG(void);
float bearing_calc(float lat, float lon, float lat2, float lon2);
//double distance_form(double lat1, double long1, double lat2, double long2);
//int alien_count(void);
bool coordinates_waiting(void);
void waiting_txt(void);

#endif /* TFTHELPER_H */
