
#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <semphr.h>
#include <LittleFS.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/adc.h"
#include "hardware/irq.h"
#include "hardware/gpio.h"
#include "adsb_sampler.pio.h"
#include "adsb_crc.h"
#include "adsb_decoder.h"
#include "dma_ring.h"
#include "storage.h"

static const uint8_t PIN_UART2_TX = 4, PIN_UART2_RX = 5;
struct RxChanCfg { uint8_t pin_in; uint8_t pin_led; };
static const RxChanCfg CH[3] = { {18,17}, {19,20}, {22,23} };
static const uint8_t PIN_BLINK_CORE0 = 15, PIN_BLINK_CORE1 = 25, PIN_RSSI = 26;

static const uint32_t SAMPLE_RATE_HZ = 10000000u; // 10 MHz
static const uint8_t  T04 = (uint8_t)(SAMPLE_RATE_HZ * 0.4e-6 + 0.5);
static const uint8_t  T05 = (uint8_t)(SAMPLE_RATE_HZ * 0.5e-6 + 0.5);
static const uint8_t  T07 = (uint8_t)(SAMPLE_RATE_HZ * 0.7e-6 + 0.5);
static const uint8_t  TBIT = (uint8_t)(SAMPLE_RATE_HZ * 1.0e-6 + 0.5); // ~10
static const uint16_t TPRE = (uint16_t)(SAMPLE_RATE_HZ * 8.0e-6 + 0.5); // 80

// Adaptive preamble correlation
static float g_neg_avg = 0.0f; static float g_neg_alpha = 0.02f; static int g_base_thr = 36; static float g_k = 1.5f; static int PRE_PENALTY_THR = 6; static int PRE_MIN_POS_HITS = 14;

struct PreambleProfile{ int base; float k; float alpha; int penalty; int minhits; const char* name; };
static const PreambleProfile PROF_NORMAL = { 36, 1.5f, 0.02f, 6, 14, "NORMAL" };
static const PreambleProfile PROF_EMI    = { 44, 2.0f, 0.01f, 4, 18, "EMI" };
static const PreambleProfile PROF_URBAN  = { 40, 1.8f, 0.02f, 5, 16, "URBAN" };
static const PreambleProfile PROF_REMOTE = { 32, 1.2f, 0.03f, 8, 12, "REMOTE" };
static void apply_profile(const PreambleProfile& p){ g_base_thr=p.base; g_k=p.k; g_neg_alpha=p.alpha; PRE_PENALTY_THR=p.penalty; PRE_MIN_POS_HITS=p.minhits; }
static const PreambleProfile* profile_from_string(const String& s){ String up=s; up.toUpperCase(); if(up=="NORMAL") return &PROF_NORMAL; if(up=="EMI") return &PROF_EMI; if(up=="URBAN") return &PROF_URBAN; if(up=="REMOTE") return &PROF_REMOTE; return &PROF_NORMAL; }

// DMA rings
static DmaRing RINGS[3];

typedef struct { uint8_t ch; uint8_t nbits; uint8_t data[14]; uint32_t rssi_u; } AdsbPacket;
static QueueHandle_t q_packets; static QueueHandle_t q_fixed_packets;

struct PreambleState { uint32_t shift[4]; bool last; uint8_t runlen; uint32_t sample_pos; uint32_t pulse_times[4096]; uint16_t phead, ptail; };
static inline void feed_sample_and_filter(PreambleState& ps, bool s);
static PreambleState PS[3];

static inline void fast_gpio_set(uint8_t pin, bool v){ if(v) sio_hw->gpio_set=1u<<pin; else sio_hw->gpio_clr=1u<<pin; }
static inline void fast_gpio_toggle(uint8_t pin){ sio_hw->gpio_togl=1u<<pin; }

static inline void shift_in_bit(PreambleState &ps, bool b){ ps.shift[0]=((ps.shift[0]<<1)|(ps.shift[1]>>31)); ps.shift[1]=((ps.shift[1]<<1)|(ps.shift[2]>>31)); ps.shift[2]=((ps.shift[2]<<1)|(ps.shift[3]>>31)); ps.shift[3]=((ps.shift[3]<<1)|(b?1:0)); }
static inline void push_pulse(PreambleState& ps, uint32_t tcenter){ uint16_t n=(ps.phead+1)&0x0FFF; if(n==ps.ptail) ps.ptail=(ps.ptail+1)&0x0FFF; ps.pulse_times[ps.phead]=tcenter; ps.phead=n; }
static inline bool peek_pulse(const PreambleState& ps, uint16_t idx_from_tail, uint32_t& t){ uint16_t cnt=(ps.phead-ps.ptail)&0x0FFF; if(idx_from_tail>=cnt) return false; uint16_t i=(ps.ptail+idx_from_tail)&0x0FFF; t=ps.pulse_times[i]; return true; }

static bool corr_preamble_and_get_t0(const PreambleState &ps, uint32_t cur_pos, uint32_t &t0){ uint8_t bits[80]; for(int i=0;i<80;i++){ int idx=127-i; int w=idx>>5; int b=31-(idx&31); bits[79-i]=(ps.shift[w]>>b)&1; } int score=0, penalty=0, posHits=0, negHits=0; for(int i=0;i<80;i++){ bool expect=((i>=2 && i<=7)||(i>=12 && i<=17)||(i>=37 && i<=42)||(i>=47 && i<=52)); if(bits[i]){ if(expect){ score+=3; posHits++; } else { score+=-1; negHits++; penalty++; } } } int dynamic_thr=g_base_thr + (int)(g_k * g_neg_avg); bool ok=(score>=dynamic_thr)&&(penalty<=PRE_PENALTY_THR)&&(posHits>=PRE_MIN_POS_HITS); if(!ok){ g_neg_avg=(1.0f-g_neg_alpha)*g_neg_avg + g_neg_alpha*(float)negHits; return false; } t0=cur_pos-80; return true; }

static bool demod_bits(const PreambleState& ps, uint32_t t0, uint8_t* out, uint8_t& nbits){ uint32_t data_start=t0+TPRE; memset(out,0,14); nbits=112; uint16_t cnt=(ps.phead-ps.ptail)&0x0FFF; for(uint8_t bit=0; bit<112; bit++){ uint32_t w0=data_start+bit*TBIT; uint32_t w1=w0+5; bool first=false,second=false; for(uint16_t k=0;k<cnt;k++){ uint32_t tp; if(!peek_pulse(ps,k,tp)) break; if(tp+2<w0) continue; if(tp>w0+TBIT+2) break; if(tp>=(int)w0-2 && tp<=(int)w1+2) first=true; else if(tp>(int)w1-2 && tp<=(int)(w0+TBIT)+2) second=true; } uint8_t val= first?1:(second?0:0); if(val) out[bit>>3]|=(1<<(7-(bit&7))); if(bit==55){ bool any_after=false; for(uint8_t b2=56;b2<112 && !any_after;b2++){ uint32_t w0b=data_start+b2*TBIT; uint32_t wE=w0b+TBIT; for(uint16_t k=0;k<cnt;k++){ uint32_t tp; if(!peek_pulse(ps,k,tp)) break; if(tp<w0b-2) continue; if(tp>wE+2) break; any_after=true; break; } } if(!any_after){ nbits=56; break; } } } return true; }

// CPR state
typedef struct{ uint32_t icao; bool has_even,has_odd; uint8_t even_me[7], odd_me[7]; uint32_t even_time_ms, odd_time_ms; } IcaoCprState; 
#define ICAO_MAP_SIZE 256
static IcaoCprState ICAO_CPR[ICAO_MAP_SIZE]; static inline uint8_t hash_icao(uint32_t icao){ icao^=icao>>12; icao^=icao>>6; icao^=icao>>3; return (uint8_t)icao; }
static CprRef CPR_REF = {55.0, 37.0};

// Output modes
enum OutMode { OUT_RAW, OUT_CSV, OUT_NMEA, OUT_JSON, OUT_UBX };
static volatile OutMode g_out_mode = OUT_JSON; static volatile bool g_log_csv_dup=false;

static void nmea_write_sentence(const String& payload){ uint8_t cs=0; for(size_t i=0;i<payload.length();++i) cs ^= (uint8_t)payload[i]; Serial2.print('$'); Serial2.print(payload); Serial2.print('*'); auto hex=[&](uint8_t x){ char h=x<10?('0'+x):('A'+x-10); Serial2.print(h); }; hex((cs>>4)&0xF); hex(cs&0xF); Serial2.print(""); }

static inline void ubx_write_adsb(uint32_t icao, double lat, double lon, int alt, double gs, double trk, int vs, const char* src){ uint8_t cls=0xAD, id=0x54; uint16_t len=24; int32_t lat_i=(int32_t)(lat*1e7); int32_t lon_i=(int32_t)(lon*1e7); uint16_t gs_i=(uint16_t)(isnan(gs)?0:(int)(gs*10)); uint16_t trk_i=(uint16_t)(isnan(trk)?0:(int)(trk*10)); int16_t vs_i=(int16_t)((vs==INT32_MIN)?0:vs); uint8_t srcb = (strcmp(src,"TISB")==0?1: strcmp(src,"ADSR")==0?2: strcmp(src,"MLAT")==0?3: 0); uint8_t b[2+2+24]; size_t o=0; b[o++]=cls; b[o++]=id; b[o++]=len & 0xFF; b[o++]=len>>8; auto put32=[&](uint32_t v){ b[o++]=v&0xFF; b[o++]=(v>>8)&0xFF; b[o++]=(v>>16)&0xFF; b[o++]=(v>>24)&0xFF; }; auto put16=[&](uint16_t v){ b[o++]=v&0xFF; b[o++]=(v>>8)&0xFF; }; put32(icao); put32((uint32_t)lat_i); put32((uint32_t)lon_i); put32((uint32_t)alt); put16(gs_i); put16(trk_i); b[o++]=vs_i & 0xFF; b[o++]=(vs_i>>8)&0xFF; b[o++]=srcb; b[o++]=0; uint8_t ck_a=0, ck_b=0; for(size_t i=0;i<o;i++){ ck_a = ck_a + b[i]; ck_b = ck_b + ck_a; } Serial2.write(0xB5); Serial2.write(0x62); Serial2.write(b, o); Serial2.write(ck_a); Serial2.write(ck_b); }

static void output_record(uint8_t ch, uint8_t df, const char* src, uint32_t icao, const char* callsign, bool has_pos, double lat, double lon, int alt, bool has_vel, double gs, double trk, int vs, int bg, uint32_t rssi_u, bool crc_ok)
{ auto print_csv = [&](void){ Serial2.print("CSV,"); Serial2.print((uint32_t)millis()); Serial2.print(','); Serial2.print(ch+1); Serial2.print(','); Serial2.print(df);
Serial2.print(','); Serial2.print(src); Serial2.print(','); 
Serial2.printf("%06X", icao & 0xFFFFFF); Serial2.print(','); 
Serial2.print(callsign); 
Serial2.print(',');
if(has_pos)
{ 
    Serial2.print(lat,6); Serial2.print(','); 
    Serial2.print(lon,6); } else { Serial2.print(',');
    Serial2.print(','); } Serial2.print(','); Serial2.print(alt);
    Serial2.print(','); if(has_vel){ Serial2.print(gs,1); Serial2.print(','); 
    Serial2.print(trk,1);} else { Serial2.print(','); Serial2.print(','); } Serial2.print(','); 
    if (vs!=INT32_MIN){ Serial2.print(vs);} Serial2.print(','); if (bg!=INT32_MIN){ Serial2.print(bg);} Serial2.print(',');
    Serial2.print(rssi_u); Serial2.print(','); Serial2.print(crc_ok?"Y":"N"); Serial2.print(""); }; switch(g_out_mode){ case OUT_RAW: if (g_log_csv_dup) print_csv(); break; case OUT_CSV: print_csv(); break; case OUT_NMEA: { String s="PADSB,"; char icaohex[7]; 
    snprintf(icaohex,sizeof(icaohex),"%06X",icao&0xFFFFFF); s+=icaohex; s+=','; s+=src; s+=','; s+=callsign; s+=','; if(has_pos){ s+=String(lat,6); s+=','; 
    s+=String(lon,6);} else { s+=","; s+=","; } s+=','; s+=String(alt); s+=','; if(has_vel){ s+=String(gs,1); s+=','; s+=String(trk,1);} else { s+=","; s+=","; } s+=','; 
    if(vs!=INT32_MIN) s+=String(vs); s+=','; if(bg!=INT32_MIN) s+=String(bg); s+=','; s+=(crc_ok?"Y":"N");
    nmea_write_sentence(s); 
    if (g_log_csv_dup) print_csv(); break; } case OUT_JSON: 
    { Serial2.print('{');
            Serial2.print("ts:"); 
            Serial2.print((uint32_t)millis()); 
            Serial2.print(','); Serial2.print("ch:"); 
            Serial2.print(ch+1); Serial2.print(','); 
            Serial2.print("df:"); Serial2.print(df); Serial2.print(','); 
            Serial2.print("src:"); Serial2.print(src); Serial2.print(",");
            Serial2.print("icao:"); Serial2.printf("%06X",icao&0xFFFFFF); Serial2.print(","); 
            Serial2.print("callsign:"); Serial2.print(callsign); 
            Serial2.print(","); Serial2.print("crc_ok:"); Serial2.print(crc_ok?"true":"false"); 
            Serial2.print(','); Serial2.print("rssi:");
            Serial2.print(rssi_u); Serial2.print(','); 
            if(has_pos){ Serial2.print("lat:"); Serial2.print(lat,6);
    Serial2.print(','); Serial2.print("lon:");
    Serial2.print(lon,6); Serial2.print(','); } Serial2.print("alt:");
    Serial2.print(alt); Serial2.print(','); if(has_vel){ Serial2.print("gs:"); 
    Serial2.print(gs,1); Serial2.print(','); Serial2.print("trk:"); Serial2.print(trk,1);
    Serial2.print(','); } if(vs!=INT32_MIN){ Serial2.print("vs:"); 
    Serial2.print(vs); Serial2.print(','); } if(bg!=INT32_MIN){ Serial2.print("bg:");
    Serial2.print(bg); Serial2.print(','); } Serial2.print("mode:json}"); 
    Serial2.print(""); if (g_log_csv_dup) print_csv(); break; 
    } case OUT_UBX: if(has_pos) ubx_write_adsb(icao,lat,lon,alt,has_vel?gs:NAN,has_vel?trk:NAN,vs,src); if (g_log_csv_dup) print_csv(); break; }
}

static const char* classify_df18_src(uint8_t cf){ switch(cf & 7){ case 1: case 2: return "TISB"; case 3: case 4: return "ADSR"; case 5: return "MLAT"; default: return "ADSB"; } }

static void decode_and_send(AdsbPacket& p){ const char* src_tag="ADSB"; uint8_t df=(p.data[0]>>3)&0x1F; if(df==18){ uint8_t cf=(p.data[0]&0x07); src_tag=classify_df18_src(cf); }
  const uint8_t nbytes=(p.nbits+7)/8; uint8_t msg[14]; memcpy(msg,p.data,nbytes);
  uint32_t parity=(msg[nbytes-3]<<16)|(msg[nbytes-2]<<8)|msg[nbytes-1]; uint32_t crc_wo=modes_crc24(msg,nbytes-3); uint32_t syndrome=(crc_wo ^ parity) & 0xFFFFFF; if(syndrome!=0){ int fixed_bit=modes_try_single_bit_fix(msg,p.nbits); if(fixed_bit>=0){ AdsbPacket fp=p; memcpy(fp.data,msg,nbytes); xQueueSend(q_fixed_packets,&fp,0); parity=(msg[nbytes-3]<<16)|(msg[nbytes-2]<<8)|msg[nbytes-1]; crc_wo=modes_crc24(msg,nbytes-3); syndrome=(crc_wo ^ parity)&0xFFFFFF; } }
  uint32_t icao = (df==17 || df==18) ? (parity ^ modes_crc24(msg, nbytes-3)) : ((msg[1]<<16)|(msg[2]<<8)|msg[3]);
  const uint8_t* me=&msg[4]; char callsign[9]={0}; double lat=NAN,lon=NAN,gs=NAN,trk=NAN; int alt=-1; int vs=INT32_MIN, baro_geo=INT32_MIN;
  if((df==17 || df==18) && p.nbits==112){ uint8_t tc=(me[0]>>3)&0x1F; if(tc>=1&&tc<=4){ decode_callsign(me,callsign);} else if(tc>=9&&tc<=18){ bool odd=(me[0]&0x04)!=0; uint8_t h=hash_icao(icao); if(ICAO_CPR[h].icao!=icao){ memset(&ICAO_CPR[h],0,sizeof(IcaoCprState)); ICAO_CPR[h].icao=icao; } if(odd){ memcpy(ICAO_CPR[h].odd_me,me,7); ICAO_CPR[h].odd_time_ms=millis(); ICAO_CPR[h].has_odd=true; } else { memcpy(ICAO_CPR[h].even_me,me,7); ICAO_CPR[h].even_time_ms=millis(); ICAO_CPR[h].has_even=true; } if(ICAO_CPR[h].has_even && ICAO_CPR[h].has_odd && (uint32_t)abs((int)(ICAO_CPR[h].even_time_ms - ICAO_CPR[h].odd_time_ms)) <= 10000){ cpr_global_decode(lat,lon,ICAO_CPR[h].even_me,ICAO_CPR[h].odd_me);} else { cpr_local_decode(lat,lon,me,CPR_REF);} alt=decode_altitude(me);} else if(tc==19){ decode_velocity_tc19(me,gs,trk,vs,baro_geo);} }
  else if (df==20 || df==21){ int vs_b=INT32_MIN, vs_i=INT32_MIN; if(parse_bds60_vs(me, vs_b, vs_i)){ if(vs==INT32_MIN) vs=(vs_b!=INT32_MIN?vs_b:vs_i);} int bg=INT32_MIN; if(parse_bds61_baro_geo(me, bg)){ if(bg!=INT32_MIN) baro_geo=bg; } }
  output_record(p.ch, df, src_tag, icao, callsign, !isnan(lat)&&!isnan(lon), lat, lon, alt, !isnan(gs), gs, trk, vs, baro_geo, p.rssi_u, syndrome==0);
}

static TaskHandle_t hRxTask=nullptr; static void gpio_irq_cb(uint gpio,uint32_t events){ BaseType_t hpw=pdFALSE; if(hRxTask) vTaskNotifyGiveFromISR(hRxTask,&hpw); portYIELD_FROM_ISR(hpw); }

static void rx_task(void* arg){
  PIO pio=pio0; uint off=pio_add_program(pio,&adsb_sampler_program);
  for(int c=0;c<3;c++){
    PS[c]={}; gpio_init(CH[c].pin_led); gpio_set_dir(CH[c].pin_led,GPIO_OUT); fast_gpio_set(CH[c].pin_led,0);
    pio_sm_config cfg=adsb_sampler_program_get_default_config(off);
    sm_config_set_in_pins(&cfg, CH[c].pin_in); sm_config_set_in_shift(&cfg,true,true,32);
    float div=(float)clock_get_hz(clk_sys)/(float)SAMPLE_RATE_HZ; sm_config_set_clkdiv(&cfg,div);
    pio_gpio_init(pio, CH[c].pin_in);
    pio_sm_set_consecutive_pindirs(pio,c,CH[c].pin_in,1,false);
    pio_sm_init(pio,c,off,&cfg); pio_sm_set_enabled(pio,c,true);
    dma_ring_init(RINGS[c], pio, c, 1024);
  }
  gpio_init(PIN_BLINK_CORE1); gpio_set_dir(PIN_BLINK_CORE1,GPIO_OUT); fast_gpio_set(PIN_BLINK_CORE1,0);
  TickType_t lastBlink=xTaskGetTickCount();
  adc_init(); adc_gpio_init(PIN_RSSI); adc_select_input(0);
  gpio_set_irq_enabled_with_callback(CH[0].pin_in, GPIO_IRQ_EDGE_RISE|GPIO_IRQ_EDGE_FALL, true, &gpio_irq_cb);
  gpio_set_irq_enabled(CH[1].pin_in, GPIO_IRQ_EDGE_RISE|GPIO_IRQ_EDGE_FALL, true);
  gpio_set_irq_enabled(CH[2].pin_in, GPIO_IRQ_EDGE_RISE|GPIO_IRQ_EDGE_FALL, true);

  while(1)
  {
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(2));
    for(int c=0;c<3;c++){
      auto &ring=RINGS[c]; auto &ps=PS[c]; uint32_t w;
      while(dma_ring_get_word(ring,w)){
        for(int i=31;i>=0;i--){ bool bit=(w>>i)&1; shift_in_bit(ps, bit); feed_sample_and_filter(ps, bit);
          uint32_t t0; if(corr_preamble_and_get_t0(ps, ps.sample_pos, t0)){
            fast_gpio_set(CH[c].pin_led,1);
            uint8_t out[14]; uint8_t nbits=0;
            if(demod_bits(ps, t0, out, nbits)){
              AdsbPacket pkt{}; pkt.ch=c; pkt.nbits=nbits; memcpy(pkt.data,out,(nbits+7)/8);
              uint32_t acc=0; const int N=16; for(int k=0;k<N;k++) acc+=adc_read(); pkt.rssi_u=acc/N;
              xQueueSend(q_packets,&pkt,0);
            }
            fast_gpio_set(CH[c].pin_led,0);
          }
        }
      }
    }
    if(xTaskGetTickCount()-lastBlink>=pdMS_TO_TICKS(500)){
      fast_gpio_toggle(PIN_BLINK_CORE1);
      lastBlink=xTaskGetTickCount();
    }
    taskYIELD();
  }
}

static void proc_task(void* arg){
  Serial2.setTX(PIN_UART2_TX); Serial2.setRX(PIN_UART2_RX); Serial2.begin(921600);
  storage_init(); apply_profile(PROF_NORMAL); g_out_mode=OUT_JSON; g_log_csv_dup=false;
  Settings set; if(storage_load(set)){ if(set.mode.length()){ String m=set.mode; m.toUpperCase(); if(m=="CSV") g_out_mode=OUT_CSV; else if(m=="NMEA") g_out_mode=OUT_NMEA; else if(m=="JSON") g_out_mode=OUT_JSON; else if(m=="UBX") g_out_mode=OUT_UBX; else if(m=="RAW") g_out_mode=OUT_RAW; } if(set.profile.length()) apply_profile(*profile_from_string(set.profile)); g_log_csv_dup=set.log_on; CPR_REF.ref_lat=set.ref_lat; CPR_REF.ref_lon=set.ref_lon; }
  gpio_init(PIN_BLINK_CORE0); gpio_set_dir(PIN_BLINK_CORE0,GPIO_OUT); fast_gpio_set(PIN_BLINK_CORE0,0);
  TickType_t lastBlink=xTaskGetTickCount(); String line=""; AdsbPacket pkt;
  while(1){
    while(Serial2.available()>0){ char ch=(char)Serial2.read(); if(ch==' ') continue; if(ch==' '){ String s=line; s.trim(); String up=s; up.toUpperCase(); if(up.startsWith("MODE ")){ String m=up.substring(5); if(m=="CSV") g_out_mode=OUT_CSV; else if(m=="NMEA") g_out_mode=OUT_NMEA; else if(m=="JSON") g_out_mode=OUT_JSON; else if(m=="UBX") g_out_mode=OUT_UBX; else if(m=="RAW") g_out_mode=OUT_RAW; Serial2.print("OK MODE "); Serial2.println(m); } else if(up.startsWith("PROFILE ")){ String p=up.substring(8); apply_profile(*profile_from_string(p)); Serial2.print("OK PROFILE "); Serial2.println(p); } else if(up.startsWith("LOG ")){ String v=up.substring(4); g_log_csv_dup=(v=="ON"); Serial2.print("OK LOG "); Serial2.println(v); } else if(up.startsWith("REF ")){ double la=CPR_REF.ref_lat, lo=CPR_REF.ref_lon; sscanf(s.c_str()+4, "%lf %lf", &la, &lo); CPR_REF.ref_lat=la; CPR_REF.ref_lon=lo; Serial2.print("OK REF "); Serial2.print(la,6); Serial2.print(' '); Serial2.println(lo,6); } else { Serial2.println("OK"); } line=""; } else if(line.length()<128){ line+=ch; } }
    if(xQueueReceive(q_packets,&pkt,pdMS_TO_TICKS(10))==pdPASS){ decode_and_send(pkt);} if(xQueueReceive(q_fixed_packets,&pkt,0)==pdPASS){ if(g_out_mode==OUT_RAW){ Serial2.print("FIXED_RAW="); for(uint8_t i=0;i<(pkt.nbits+7)/8;i++){ if(pkt.data[i]<16) Serial2.print('0'); Serial2.print(pkt.data[i],HEX);} Serial2.print(""); } }
    if(xTaskGetTickCount()-lastBlink>=pdMS_TO_TICKS(1000)){ fast_gpio_toggle(PIN_BLINK_CORE0); lastBlink=xTaskGetTickCount(); }
  }
}

extern "C" void vTaskCoreAffinitySet(TaskHandle_t xTask, UBaseType_t uxCoreAffinityMask);
__attribute__((constructor)) static void start_rtos(){ q_packets=xQueueCreate(256,sizeof(AdsbPacket)); q_fixed_packets=xQueueCreate(64,sizeof(AdsbPacket)); TaskHandle_t hRx=nullptr,hProc=nullptr; xTaskCreate(rx_task,"rx",16384,NULL,configMAX_PRIORITIES-1,&hRx); xTaskCreate(proc_task,"proc",12288,NULL,configMAX_PRIORITIES-2,&hProc); if(hRx) vTaskCoreAffinitySet(hRx,(1<<1)); if(hProc) vTaskCoreAffinitySet(hProc,(1<<0)); }

void setup(){}
void loop(){}
