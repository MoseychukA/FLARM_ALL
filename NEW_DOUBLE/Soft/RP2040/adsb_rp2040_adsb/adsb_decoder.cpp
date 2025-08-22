
#include "adsb_decoder.h"
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
static const char id_charset[] = "#ABCDEFGHIJKLMNOPQRSTUVWXYZ#####_###############0123456789######";

void decode_callsign(const uint8_t* me, char* out8){
  uint64_t acc=0; for(int i=0;i<7;i++) acc=(acc<<8)|me[i];
  for(int i=0;i<8;i++){ int code=(acc>>(56-8-6*(i+1)))&0x3F; out8[i]=id_charset[code]; }
  out8[8]=0;
}

static inline uint32_t getbits56(const uint8_t* d, int off, int len){
  uint64_t acc=0; for(int i=0;i<7;i++) acc=(acc<<8)|d[i];
  int shift=56-(off+len);
  uint64_t mask=(len==32)?0xFFFFFFFFULL:((1ULL<<len)-1);
  return (uint32_t)((acc>>shift)&mask);
}
static inline uint32_t getbits(const uint8_t* d, int off, int len){ return getbits56(d, off, len); }

bool cpr_global_decode(double& lat, double& lon, const uint8_t* even_me, const uint8_t* odd_me){
  uint32_t cprlat_even=getbits(even_me,17,17), cprlon_even=getbits(even_me,34,17), cprlat_odd=getbits(odd_me,17,17), cprlon_odd=getbits(odd_me,34,17);
  auto cprNL=[](double lat)->int{
    double a=fabs(lat);
    if(a<10.47047130) return 59; if(a<14.82817437) return 58; if(a<18.18626357) return 57; if(a<21.02939493) return 56; if(a<23.54504487) return 55; if(a<25.82924707) return 54; if(a<27.93898710) return 53; if(a<29.91135686) return 52; if(a<31.77209708) return 51; if(a<33.53993436) return 50; if(a<35.22899598) return 49; if(a<36.85025108) return 48; if(a<38.41241892) return 47; if(a<39.92256684) return 46; if(a<41.38651832) return 45; if(a<42.80914012) return 44; if(a<44.19454951) return 43; if(a<45.54626723) return 42; if(a<46.86733252) return 41; if(a<48.16039128) return 40; if(a<49.42776439) return 39; if(a<50.67150166) return 38; if(a<51.89342469) return 37; if(a<53.09516153) return 36; if(a<54.27817472) return 35; if(a<55.44378444) return 34; if(a<56.59318756) return 33; if(a<57.72747354) return 32; if(a<58.84763776) return 31; if(a<59.95459277) return 30; if(a<61.04917774) return 29; if(a<62.13216659) return 28; if(a<63.20427479) return 27; if(a<64.26616523) return 26; if(a<65.31845310) return 25; if(a<66.36171008) return 24; if(a<67.39646774) return 23; if(a<68.42322022) return 22; if(a<69.44242631) return 21; if(a<70.45451075) return 20; if(a<71.45986473) return 19; if(a<72.45884545) return 18; if(a<73.45177442) return 17; if(a<74.43893416) return 16; if(a<75.42056257) return 15; if(a<76.39684391) return 14; if(a<77.36789461) return 13; if(a<78.33374083) return 12; if(a<79.29428225) return 11; if(a<80.24923213) return 10; if(a<81.19801349) return 9; if(a<82.13956981) return 8; if(a<83.07199445) return 7; if(a<83.99173563) return 6; if(a<84.89166191) return 5; if(a<85.75541621) return 4; if(a<86.53536998) return 3; if(a<87.00000000) return 2; return 1;
  };
  int j=(int)floor((59.0*cprlat_even - 60.0*cprlat_odd)/131072.0 + 0.5);
  double rlat_even = 360.0/60.0 * (fmod(j,60) + cprlat_even/131072.0);
  double rlat_odd  = 360.0/59.0 * (fmod(j,59) + cprlat_odd/131072.0);
  double rlat = rlat_even; int nl=cprNL(rlat); if(nl<=0) return false; int ni=(nl>1)?nl:1;
  int m=(int)floor((cprlon_even*(nl-1) - cprlon_odd*nl)/131072.0 + 0.5);
  double rlon = 360.0/ni * (fmod(m, ni) + cprlon_even/131072.0);
  lat=rlat; lon=(rlon>180.0)?(rlon-360.0):rlon; return true;
}

bool cpr_local_decode(double& lat, double& lon, const uint8_t* me, const CprRef& ref){
  bool odd=(me[0]&0x04)!=0; uint32_t cprlat=getbits(me,17,17); uint32_t cprlon=getbits(me,34,17);
  auto cprNL=[](double lat)->int{ double a=fabs(lat);
    if(a<10.47047130) return 59; if(a<14.82817437) return 58; if(a<18.18626357) return 57; if(a<21.02939493) return 56; if(a<23.54504487) return 55; if(a<25.82924707) return 54; if(a<27.93898710) return 53; if(a<29.91135686) return 52; if(a<31.77209708) return 51; if(a<33.53993436) return 50; if(a<35.22899598) return 49; if(a<36.85025108) return 48; if(a<38.41241892) return 47; if(a<39.92256684) return 46; if(a<41.38651832) return 45; if(a<42.80914012) return 44; if(a<44.19454951) return 43; if(a<45.54626723) return 42; if(a<46.86733252) return 41; if(a<48.16039128) return 40; if(a<49.42776439) return 39; if(a<50.67150166) return 38; if(a<51.89342469) return 37; if(a<53.09516153) return 36; if(a<54.27817472) return 35; if(a<55.44378444) return 34; if(a<56.59318756) return 33; if(a<57.72747354) return 32; if(a<58.84763776) return 31; if(a<59.95459277) return 30; if(a<61.04917774) return 29; if(a<62.13216659) return 28; if(a<63.20427479) return 27; if(a<64.26616523) return 26; if(a<65.31845310) return 25; if(a<66.36171008) return 24; if(a<67.39646774) return 23; if(a<68.42322022) return 22; if(a<69.44242631) return 21; if(a<70.45451075) return 20; if(a<71.45986473) return 19; if(a<72.45884545) return 18; if(a<73.45177442) return 17; if(a<74.43893416) return 16; if(a<75.42056257) return 15; if(a<76.39684391) return 14; if(a<77.36789461) return 13; if(a<78.33374083) return 12; if(a<79.29428225) return 11; if(a<80.24923213) return 10; if(a<81.19801349) return 9; if(a<82.13956981) return 8; if(a<83.07199445) return 7; if(a<83.99173563) return 6; if(a<84.89166191) return 5; if(a<85.75541621) return 4; if(a<86.53536998) return 3; if(a<87.00000000) return 2; return 1; };
  int nz = odd ? 59 : 60; double dlat = 360.0 / nz; int j = (int)floor(ref.ref_lat/dlat) + (int)floor((fmod(ref.ref_lat, dlat)/dlat) - cprlat/131072.0 + 0.5);
  double rlat = dlat * (j + cprlat/131072.0); int nl = cprNL(rlat); if (nl <= 0) return false; int ni = (nl > 1) ? nl : 1; double dlon = 360.0 / ni;
  int m = (int)floor(ref.ref_lon/dlon) + (int)floor((fmod(ref.ref_lon, dlon)/dlon) - cprlon/131072.0 + 0.5);
  double rlon = dlon * (m + cprlon/131072.0); if (rlon > 180.0) rlon -= 360.0; lat=rlat; lon=rlon; return true;
}

int decode_altitude_gillham(uint16_t ac){ int D1=(ac>>0)&1, D2=(ac>>1)&1, D4=(ac>>2)&1; int A1=(ac>>3)&1, A2=(ac>>4)&1, A4=(ac>>5)&1; int B1=(ac>>6)&1, B2=(ac>>7)&1, B4=(ac>>8)&1; int C1=(ac>>9)&1, C2=(ac>>10)&1, C4=(ac>>11)&1; int f100=(D1|D2|D4); int n500 = ((A4<<2)|(A2<<1)|A1); int n5k = ((B4<<2)|(B2<<1)|B1); int n20k = ((C4<<2)|(C2<<1)|C1);
  auto gray2bin=[](int g){ int b=0; for(; g; g>>=1) b^=g; return b; };
  n500=gray2bin(n500); n5k=gray2bin(n5k); n20k=gray2bin(n20k);
  static const int tbl100[8]={0,100,200,300,400,500,600,700}; int idx100=gray2bin((D4<<2)|(D2<<1)|D1);
  int alt=(n20k*16000)+(n5k*2000)+(n500*500); if(f100) alt += tbl100[idx100]; alt -= 1300;
  if(alt<-1200||alt>126000) return -1; return alt;
}

int decode_altitude(const uint8_t* me){ uint64_t acc=0; for(int i=0;i<7;i++) acc=(acc<<8)|me[i]; int q=(acc>>(56-8-20))&1; if(q){ uint32_t raw=((acc>>(56-8-19))&0x7FF); int n=raw; return n*25 - 1000; } else { uint16_t ac13=(uint16_t)((((acc>>(56-8-8))&0xFF)<<5) | ((acc>>(56-8-21))&0x1F)); return decode_altitude_gillham(ac13);} }

bool decode_velocity_tc19(const uint8_t* me, double& gs, double& trk, int& vs, int& baro_geo){
  uint64_t acc=0; for(int i=0;i<7;i++) acc=(acc<<8)|me[i];
  int subtype=(acc>>(56-8-37))&0x7; gs=NAN; trk=NAN; vs=INT32_MIN; baro_geo=INT32_MIN;
  if(subtype==1||subtype==2){
    int ew_dir=(acc>>(56-8-38))&1; int ew_spd=(acc>>(56-8-46))&0x1FF;
    int ns_dir=(acc>>(56-8-47))&1; int ns_spd=(acc>>(56-8-55))&0x1FF;
    double vE=(ew_spd==0)?0:(ew_spd-1); vE=(ew_dir?-vE:vE);
    double vN=(ns_spd==0)?0:(ns_spd-1); vN=(ns_dir?-vN:vN);
    gs=sqrt(vE*vE+vN*vN);
    trk=fmod(atan2(vE,vN)*180.0/M_PI + 360.0, 360.0);
  } else if(subtype==3){
    int hdg=((acc>>(56-8-46))&0x1FF); int spd=((acc>>(56-8-55))&0x1FF);
    if(spd){ trk=hdg*360.0/512.0; gs=spd-1; }
  } else if(subtype==4){
    int hdg=((acc>>(56-8-46))&0x1FF); int tas=((acc>>(56-8-55))&0x1FF);
    if(tas){ trk=hdg*360.0/512.0; gs=tas-1; }
  }
  return (!isnan(gs));
}

bool parse_bds60_vs(const uint8_t* me, int& vs_baro, int& vs_inertial){
  uint8_t bds1=(me[0]>>4)&0xF, bds2=me[0]&0xF; if(bds1!=6||bds2!=0) return false;
  int baro_stat=(getbits56(me,37,1)); int baro_sign=(getbits56(me,38,1)); int baro_val=(int)getbits56(me,39,9);
  int iner_stat=(getbits56(me,48,1)); int iner_sign=(getbits56(me,49,1)); int iner_val=(int)getbits56(me,50,9);
  vs_baro=vs_inertial=INT32_MIN;
  if(baro_stat && baro_val){ int v=(baro_val-1)*64; vs_baro=baro_sign? -v: v; }
  if(iner_stat && iner_val){ int v=(iner_val-1)*64; vs_inertial=iner_sign? -v: v; }
  return (vs_baro!=INT32_MIN || vs_inertial!=INT32_MIN);
}

bool parse_bds61_baro_geo(const uint8_t* me, int& baro_geo_diff){
  uint8_t bds1=(me[0]>>4)&0xF, bds2=me[0]&0xF; if(bds1!=6||bds2!=1) return false;
  int bg_stat=(getbits56(me,37,1)); int bg_sign=(getbits56(me,38,1)); int bg_val=(int)getbits56(me,39,7);
  if(!bg_stat||bg_val==0){ baro_geo_diff=INT32_MIN; return true; }
  int v=bg_val*25; baro_geo_diff=bg_sign? -v : v; return true;
}
