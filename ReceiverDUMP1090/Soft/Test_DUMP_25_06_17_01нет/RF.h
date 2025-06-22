/*
 * RFHelper.h
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

#ifndef RFHELPER_H
#define RFHELPER_H

#include <TimeLib.h>

#include "SoC.h"

#if defined(USE_BASICMAC)
#include <basicmac.h>
#else
#include <lmic.h>
#endif
#include <hal/hal.h>
#include <lib_crc.h>
#include <protocol.h>
#include <freqplan.h>

#include "GNSS.h"
#include "Legacy.h"
#include "OGNTP.h"
#include "ES1090.h"

#define maxof2(a,b)       (a > b ? a : b)
#define maxof3(a,b,c)     maxof2(maxof2(a,b),c)
#define maxof5(a,b,c,d,e) maxof2(maxof2(a,b),maxof3(c,d,e))

/* Max. paket's payload size for all supported RF protocols */
#define MAX_PKT_SIZE  32 /* 48 = UAT LONG_FRAME_DATA_BYTES */

#define RXADDR {0x31, 0xfa , 0xb6} // Address of this device (4 bytes)
#define TXADDR {0x31, 0xfa , 0xb6} // Address of device to send to (4 bytes)

enum
{
  RF_IC_NONE,
  RF_IC_SX1276,
  RF_DRV_OGN,
  RF_IC_SX1262,
  RF_IC_R820T,
};

enum
{
  RF_TX_POWER_FULL,
  RF_TX_POWER_LOW,
  RF_TX_POWER_OFF
};

typedef struct rfchip_ops_struct {
  byte type;
  const char name[8];
  bool (*probe)();
  void (*setup)();
  void (*channel)(int8_t);
  bool (*receive)();
  void (*transmit)();
  void (*shutdown)();
} rfchip_ops_t;

typedef struct Slot_descr_struct {
  uint16_t begin;
  uint16_t duration;
  unsigned long tmarker;
} Slot_descr_t;

typedef struct Slots_descr_struct {
  uint32_t      interval_min;
  uint32_t      interval_max;
  uint32_t      interval_mid;
  uint32_t      adj;
  uint16_t      air_time;
  Slot_descr_t  s0;
  Slot_descr_t  s1;
  uint8_t       current;
} Slots_descr_t;

String Bin2Hex(byte *, size_t);
uint8_t parity(uint32_t);

byte    RF_setup(void);
void    RF_SetChannel(void);
void    RF_loop(void);
size_t  RF_Encode(ufo_t *);
bool    RF_Transmit(size_t, bool);
bool    RF_Receive(void);
void    RF_Shutdown(void);
uint8_t RF_Payload_Size(uint8_t);

extern byte TxBuffer[MAX_PKT_SIZE], RxBuffer[MAX_PKT_SIZE];
extern unsigned long TxTimeMarker;

extern const rfchip_ops_t *rf_chip;
extern bool RF_SX12XX_RST_is_connected;
extern size_t (*protocol_encode)(void *, ufo_t *);
extern bool (*protocol_decode)(void *, ufo_t *, ufo_t *);

extern int8_t RF_last_rssi;
extern const char *Protocol_ID[];

#endif /* RFHELPER_H */
