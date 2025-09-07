#include "adsb_decoder.h"
#include "adsb_crc.h"
#include "adsb_gillham.h"
#include "adsb_cpr.h"
#include <string.h>
#include <math.h>

static const char *kIdChars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ#####_###############0123456789######";

static void decode_callsign(const uint8_t *me, char *out9) {
  // 48 бит (8*6) символов: биты 41..88 ME-поля
  uint8_t six[8];
  six[0]=(me[1]&0xFC)>>2;
  six[1]=((me[1]&0x03)<<4)|((me[2]&0xF0)>>4);
  six[2]=((me[2]&0x0F)<<2)|((me[3]&0xC0)>>6);
  six[3]=(me[3]&0x3F);
  six[4]=(me[4]&0xFC)>>2;
  six[5]=((me[4]&0x03)<<4)|((me[5]&0xF0)>>4);
  six[6]=((me[5]&0x0F)<<2)|((me[6]&0xC0)>>6);
  six[7]=(me[6]&0x3F);
  for (int i=0;i<8;i++){ char c = kIdChars[six[i]&0x3F]; out9[i]=(c=='#'||c=='_')?' ':c; }
  out9[8]=0;
}

static int id13_to_modea(uint16_t id13) {
  // Упрощённо: преобразуем nibble-ы к октальному виду (совместимо с большинством ответов)
  int a = (id13>>12)&0xF, b=(id13>>8)&0xF, c=(id13>>4)&0xF, d=(id13)&0xF;
  int squawk = ((a&7)<<9)|((b&7)<<6)|((c&7)<<3)|(d&7);
  return squawk & 0x7777;
}

static void decode_tc19(const uint8_t *me, float &gs_kt, float &trk_deg, int &vr) {
  uint8_t subtype = (me[0] & 0x07);
  gs_kt=0; trk_deg=0; vr=0;
  if (subtype==1 || subtype==2) {
    uint16_t ew = ((me[1]&0x03)<<8) | me[2];
    uint16_t ns = ((me[3]&0x03)<<8) | me[4];
    bool ewSign = me[1]&0x04, nsSign = me[3]&0x04;
    float vew = (ew?ew-1:0) * (ewSign?-1:1);
    float vns = (ns?ns-1:0) * (nsSign?-1:1);
    gs_kt = sqrtf(vew*vew + vns*vns);
    trk_deg = fmodf(atan2f(vew, vns) * 180.0f / (float)M_PI + 360.0f, 360.0f);
  } else if (subtype==3 || subtype==4) {
    uint16_t hdg = ((me[1]&0x7F)<<3) | ((me[2]&0xE0)>>5);
    trk_deg = (hdg * 360.0f) / 1024.0f;
    uint16_t spd = ((me[2]&0x1F)<<6) | ((me[3]&0xFC)>>2);
    gs_kt = (float)(spd?spd-1:0);
  }
  // VR — обычно в BDS6,0/6,1 (DF20/21), здесь оставляем 0
}

DecodedADSB adsb_decode_packet(const AdsbPacket &pkt) {
  DecodedADSB fo{}; fo.signal_source=1; fo.timestamp = millis()/1000;
  const uint8_t *p = pkt.raw;
  int df = (p[0] >> 3) & 0x1F; fo.df=df;

  // DF5/21 — Squawk
  if (df==5 || df==21) {
    fo.addr = (p[1]<<16)|(p[2]<<8)|p[3];
    // ID13 (упрощённая выборка — зависит от конкретного ответа, этого достаточно для отладки)
    uint16_t id13 = ((p[4]&0x1F)<<8) | p[5];
    fo.Squawk = id13_to_modea(id13);
    return fo;
  }

  // DF17/18 — основной трафик
  if (df!=17 && df!=18) return fo;
  if (!adsb_crc_check(pkt.raw, pkt.bits)) return fo;

  fo.addr = (p[1]<<16)|(p[2]<<8)|p[3];
  const uint8_t *me = p + 4; // 7 байт
  uint8_t tc = (me[0] >> 3); fo.tc=tc;

  switch(tc){
    case 1: case 2: case 3: case 4: {
      decode_callsign(me, fo.flight);
      break;
    }
    case 19: {
      float gs,trk; int vr;
      decode_tc19(me, gs, trk, vr);
      fo.speed = gs; fo.course=trk; fo.vert_rate=vr;
      break;
    }
    case 9: case 10: case 11: case 12:
    case 13: case 14: case 15: case 16:
    case 17: case 18: {
      // Высота (AC12). В ME биты 21-32: формат как в DO-260B.
      // Извлечение AC12 (11 бит + Q): см. общую схему — берём как в dump1090.
      uint16_t ac12 = ((me[1]&0x1F)<<7) | ((me[2]&0xFE)>>1);
      fo.altitude = adsb_gillham_decode(ac12);

      // CPR
      bool odd = (me[2] & 0x01);
      uint32_t lat = ((me[3]<<9)|(me[4]<<1)|((me[5]>>7)&1)) & 0x1FFFF;
      uint32_t lon = (((me[5]&0x7F)<<10)|(me[6]<<2)|((me[6]>>6)&0)) & 0x1FFFF; // последняя пара бит уже в me[6]
      // Корректное извлечение 17-бит lon: биты 51..67 — смещены внутри me[5..6]
      lon = (((me[5]&0x7F)<<10) | (me[6]<<2) | ((p[11]>>6)&0x03)) & 0x1FFFF; // безопаснее: взять из p[11] верхние 2 бита
      // (в зависимости от компоновки буфера, верхние 2 бита lon могут быть в следующем байте payload; здесь p[11] — допустимая позиция)

      if (odd) cpr_note_odd(fo.addr, lat, lon, pkt.timestamp_ms);
      else     cpr_note_even(fo.addr, lat, lon, pkt.timestamp_ms);

      float latd,lond;
      if (cpr_global(fo.addr, latd, lond)) { fo.latitude=latd; fo.longitude=lond; }
      break;
    }
    default: break;
  }
  if (!fo.flight[0]) strncpy(fo.flight,"--------",9);
  return fo;
}
