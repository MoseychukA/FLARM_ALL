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


//unsigned long previousMillis_msg = 0;        // will store last time LED was updated
//const long interval_on = 2500;           // interval at which to blink (milliseconds)
//const long interval_off = 500;           // interval at which to blink (milliseconds)
//int divider_num = 1;
//bool isActive;
////word color = TFT_RED;
//
//uint8_t  confirmation_OK;
//
////char msg[Number_of_bytes_block] = "";
///*char time_msg[Number_of_bytes_time] = "";*/
//
//bool Allow_flashing;   //Разрешить мигание
//bool flashing_on_off;  //Режим включен или потушен
//bool confirm_message;
//char msg_mem_tmp[Number_of_bytes_block] = "";
//
//int control_X = 10;
//uint32_t tmr = 0;
//bool charge_on = false;
//int y_val = 45;
//
//long unsigned int startMillis;
//short unsigned int iter = 0;              // used to calculate the frames per second (FPS)
//int winkel = 0;
//bool wifi_set = false;
//int distance_var = 2;
//
//int test_curse = 0;
//float Aircraft_latitude_old = 0;
//float Aircraft_longitude_old = 0;
//
//............................dont edit this
//int cx = 160;
//int cy = 160;
//int rx = 158;
//int nn = 0;

//float fx[360]; //outer points of Speed gaouges
//float fy[360];
//float px[360]; //ineer point of Speed gaouges
//float py[360];
//float px1[360]; //ineer point of Speed gaouges
//float py1[360];
//float lx[360]; //text of Speed gaouges
//float ly[360];
//float nx[360]; //needle low of Speed gaouges
//float ny[360];

//uint8_t  fix_tmp = false;

//double rad = 0.01745;
//
//int16_t  tbx, tby;
//uint16_t tbw, tbh;
//
//
//int32_t divider = 2000;  //делитель равен половине полной шкалы
//uint16_t x_cont;
//uint16_t y_cont;
//uint16_t radar_x = 0;
//uint16_t radar_y = 0; //(tft_radar->width() - tft_radar->height()) / 2;
//uint16_t radar_w = 320; //tft->width();
//
//uint16_t radar_center_x = radar_w / 2;
//uint16_t radar_center_y = radar_y + radar_w / 2;
//uint16_t radius = radar_w / 2 - 2;
//
//int16_t rel_x;
//int16_t rel_y;
//int16_t new_rel_x;
//int16_t new_rel_y;
//int16_t new_form_x;
//int16_t new_form_y;
//
//int16_t xx1;
//int16_t yy1;
//int16_t new_x;
//int16_t new_y;
//
//int16_t form_x = 0;
//int16_t form_y = 0;
//int16_t form_arrow_x = 0;
//int16_t form_arrow_y = 0;
//
//int16_t alient_course0 = 0; // Курс ближайшего стороннего самолета
//int16_t alient_speed0 = 0;  // Скорость ближайшего стороннего самолета
//int8_t  txt_loc_speed = 87; // место вывода текста скорости стороннегосамолета
//
///* Переменные для фильтра высоты искорости нашего самолета*/
//
//int thisAircraft_altitude_tmr = 0;           // ThisAircraft
//int thisAircraft_speed_tmr = 0;
//int thisAircraft_course_tmr = 0;
//
//uint8_t index_nearest_aircraft = 0;          // индекс ближайшего самолета
//
//int view_alien_count = 0;                    // Переменная для определения количества сторонних самолетов.
//
//bool text_call = false;



void TFT_setup(void);
void TFT_loop(void);


#endif /* TFTHELPER_H */
