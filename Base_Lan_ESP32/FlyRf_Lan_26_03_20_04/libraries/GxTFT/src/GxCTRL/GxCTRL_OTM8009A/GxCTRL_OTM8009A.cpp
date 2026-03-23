// created by Jean-Marc Zingg to be the GxCTRL_OTM8009A class for the GxTFT library
// code extracts taken from code and documentation from Ruijia Industry (lcd.h, lcd.c)
//
// License: GNU GENERAL PUBLIC LICENSE V3, see LICENSE
//
// this class works with "IPS 3.97 inch 16.7M HD TFT LCD Touch Screen Module OTM8009A Drive IC 480(RGB)*800" display from Ruijia Industry
// e.g. https://www.aliexpress.com/item/IPS-3-97-inch-HD-TFT-LCD-Touch-Screen-Module-OTM8009A-Drive-IC-800-480/32676929794.html
// this display matches the FSMC TFT connector of the STM32F407ZG-M4 board, EXCEPT FOR POWER SUPPLY PINS
// e.g. https://www.aliexpress.com/item/STM32F407ZGT6-Development-Board-ARM-M4-STM32F4-cortex-M4-core-Board-Compatibility-Multiple-Extension/32795142050.html
// CAUTION: the display needs 5V on VCC pins; data and control pins are 3.3V
//
// note: this display needs 16bit commands, aka "(MDDI/SPI) Address" in some OTM8009A.pdf

#include "GxCTRL_OTM8009A.h"

#define OTM8009A_MADCTL     0x3600
#define OTM8009A_MADCTL_MY  0x80
#define OTM8009A_MADCTL_MX  0x40
#define OTM8009A_MADCTL_MV  0x20
#define OTM8009A_MADCTL_ML  0x10
#define OTM8009A_MADCTL_RGB 0x00
#define OTM8009A_MADCTL_BGR 0x08

uint32_t GxCTRL_OTM8009A::readID()
{
  return readRegister(0xA1, 2, 2);
}

uint32_t GxCTRL_OTM8009A::readRegister(uint8_t nr, uint8_t index, uint8_t bytes)
{
  uint32_t rv = 0;
  bytes = gx_uint8_min(bytes, 4);
  IO.startTransaction();
  IO.writeCommand16(nr << 8);
  IO.readData(); // dummy
  for (uint8_t i = 0; i < index; i++)
  {
    IO.readData(); // skip
  }
  for (; bytes > 0; bytes--)
  {
    rv <<= 8;
    rv |= IO.readData();
  }
  IO.endTransaction();
  return rv;
}

uint16_t GxCTRL_OTM8009A::readPixel(uint16_t x, uint16_t y)
{
  uint16_t rv;
  readRect(x, y, 1, 1, &rv);
  return rv;
}

#if defined(OTM8009A_RAMRD_AUTO_INCREMENT_OK) // not ok on my display

void GxCTRL_OTM8009A::readRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t* data)
{
  uint16_t xe = x + w - 1;
  uint16_t ye = y + h - 1;
  uint32_t num = uint32_t(w) * uint32_t(h);
  IO.startTransaction();
  setWindowAddress(x, y, xe, ye);
  IO.writeCommand16(0x2E00);  // read from RAM
  IO.readData16(); // dummy
  for (; num > 0; num--)
  {
    uint16_t g = IO.readData();
    uint16_t r = IO.readData();
    uint16_t b = IO.readData();
    *data++ = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3);
  }
  IO.endTransaction();
}

#else

void GxCTRL_OTM8009A::readRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t* data)
{
  uint16_t xe = x + w - 1;
  uint16_t ye = y + h - 1;
  for (uint16_t yy = y; yy <= ye; yy++)
  {
    for (uint16_t xx = x; xx <= xe; xx++)
    {
      IO.startTransaction();
      setWindowAddress(xx, yy, xx, yy);
      IO.writeCommand16(0x2E00);  // read from RAM
      IO.readData16(); // dummy
      uint16_t g = IO.readData();
      uint16_t r = IO.readData();
      uint16_t b = IO.readData();
      *data++ = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3);
      IO.endTransaction();
    }
  }
}

#endif

void GxCTRL_OTM8009A::init()
{

	////3.97inch OTM8009 Init 
	//IO.writeCommand16Transaction(0xff00);
	//IO.writeData16Transaction(0x80);
	//IO.writeCommand16Transaction(0xff01);
	//IO.writeData16Transaction(0x09);
	//IO.writeCommand16Transaction(0xff02);
	//IO.writeData16Transaction(0x01);

	//IO.writeCommand16Transaction(0xff80);
	//IO.writeData16Transaction(0x80);
	//IO.writeCommand16Transaction(0xff81);
	//IO.writeData16Transaction(0x09);

	//IO.writeCommand16Transaction(0xff03);
	//IO.writeData16Transaction(0x01);

	//IO.writeCommand16Transaction(0xf5b6);
	//IO.writeData16Transaction(0x06);
	//IO.writeCommand16Transaction(0xc480);
	//IO.writeData16Transaction(0x30);
	//IO.writeCommand16Transaction(0xc48a);
	//IO.writeData16Transaction(0x40);
	////===================================================//
	//IO.writeCommand16Transaction(0xc0a3);
	//IO.writeData16Transaction(0x1B);

	//IO.writeCommand16Transaction(0xc0ba);
	//IO.writeData16Transaction(0x50);

	//IO.writeCommand16Transaction(0xc181);
	//IO.writeData16Transaction(0x66);

	//IO.writeCommand16Transaction(0xc1a1);
	//IO.writeData16Transaction(0x0E);

	//IO.writeCommand16Transaction(0xc481);
	//IO.writeData16Transaction(0x83);

	//IO.writeCommand16Transaction(0xc582);
	//IO.writeData16Transaction(0x83);

	//IO.writeCommand16Transaction(0xc590);
	//IO.writeData16Transaction(0x96);

	//IO.writeCommand16Transaction(0xc591);
	//IO.writeData16Transaction(0x2B);

	//IO.writeCommand16Transaction(0xc592);
	//IO.writeData16Transaction(0x01);


	//IO.writeCommand16Transaction(0xc594);
	//IO.writeData16Transaction(0x33);

	//IO.writeCommand16Transaction(0xc595);
	//IO.writeData16Transaction(0x34);


	//IO.writeCommand16Transaction(0xc5b1);
	//IO.writeData16Transaction(0xa9);

	//IO.writeCommand16Transaction(0xce80);
	//IO.writeData16Transaction(0x86);
	//IO.writeCommand16Transaction(0xce81);
	//IO.writeData16Transaction(0x01);
	//IO.writeCommand16Transaction(0xce82);
	//IO.writeData16Transaction(0x00);

	//IO.writeCommand16Transaction(0xce83);
	//IO.writeData16Transaction(0x85);
	//IO.writeCommand16Transaction(0xce84);
	//IO.writeData16Transaction(0x01);
	//IO.writeCommand16Transaction(0xce85);
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xce86);
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xce87);
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xce88);
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xce89);
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xce8A);
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xce8B);
	//IO.writeData16Transaction(0x00);

	//IO.writeCommand16Transaction(0xcea0);// cea1[7:0] : clka1_width[3:0], clka1_shift[11:8]                         
	//IO.writeData16Transaction(0x18);
	//IO.writeCommand16Transaction(0xcea1);// cea2[7:0] : clka1_shift[7:0]                                            
	//IO.writeData16Transaction(0x04);
	//IO.writeCommand16Transaction(0xcea2);// cea3[7:0] : clka1_sw_tg, odd_high, flat_head, flat_tail, switch[11:8]   
	//IO.writeData16Transaction(0x03);
	//IO.writeCommand16Transaction(0xcea3);// cea4[7:0] : clka1_switch[7:0]                                               
	//IO.writeData16Transaction(0x21);
	//IO.writeCommand16Transaction(0xcea4);// cea5[7:0] : clka1_extend[7:0]                                           
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcea5);// cea6[7:0] : clka1_tchop[7:0]                                            
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcea6);// cea7[7:0] : clka1_tglue[7:0]                                            
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcea7);// cea8[7:0] : clka2_width[3:0], clka2_shift[11:8]                         
	//IO.writeData16Transaction(0x18);
	//IO.writeCommand16Transaction(0xcea8);// cea9[7:0] : clka2_shift[7:0]                                            
	//IO.writeData16Transaction(0x03);
	//IO.writeCommand16Transaction(0xcea9);// ceaa[7:0] : clka2_sw_tg, odd_high, flat_head, flat_tail, switch[11:8]   
	//IO.writeData16Transaction(0x03);
	//IO.writeCommand16Transaction(0xceaa);// ceab[7:0] : clka2_switch[7:0]                                                
	//IO.writeData16Transaction(0x22);
	//IO.writeCommand16Transaction(0xceab);// ceac[7:0] : clka2_extend                                                
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xceac);// cead[7:0] : clka2_tchop                                                 
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcead);// ceae[7:0] : clka2_tglue 
	//IO.writeData16Transaction(0x00);

	//IO.writeCommand16Transaction(0xceb0);// ceb1[7:0] : clka3_width[3:0], clka3_shift[11:8]                          
	//IO.writeData16Transaction(0x18);
	//IO.writeCommand16Transaction(0xceb1);// ceb2[7:0] : clka3_shift[7:0]                                             
	//IO.writeData16Transaction(0x02);
	//IO.writeCommand16Transaction(0xceb2);// ceb3[7:0] : clka3_sw_tg, odd_high, flat_head, flat_tail, switch[11:8]    
	//IO.writeData16Transaction(0x03);
	//IO.writeCommand16Transaction(0xceb3);// ceb4[7:0] : clka3_switch[7:0]                                               
	//IO.writeData16Transaction(0x23);
	//IO.writeCommand16Transaction(0xceb4);// ceb5[7:0] : clka3_extend[7:0]                                            
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xceb5);// ceb6[7:0] : clka3_tchop[7:0]                                             
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xceb6);// ceb7[7:0] : clka3_tglue[7:0]                                             
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xceb7);// ceb8[7:0] : clka4_width[3:0], clka2_shift[11:8]                          
	//IO.writeData16Transaction(0x18);
	//IO.writeCommand16Transaction(0xceb8);// ceb9[7:0] : clka4_shift[7:0]                                             
	//IO.writeData16Transaction(0x01);
	//IO.writeCommand16Transaction(0xceb9);// ceba[7:0] : clka4_sw_tg, odd_high, flat_head, flat_tail, switch[11:8]    
	//IO.writeData16Transaction(0x03);
	//IO.writeCommand16Transaction(0xceba);// cebb[7:0] : clka4_switch[7:0]                                                
	//IO.writeData16Transaction(0x24);
	//IO.writeCommand16Transaction(0xcebb);// cebc[7:0] : clka4_extend                                                 
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcebc);// cebd[7:0] : clka4_tchop                                                  
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcebd);// cebe[7:0] : clka4_tglue                                                  
	//IO.writeData16Transaction(0x00);


	//IO.writeCommand16Transaction(0xcfc0);// cfc1[7:0] : eclk_normal_width[7:0]   
	//IO.writeData16Transaction(0x01);
	//IO.writeCommand16Transaction(0xcfc1);// cfc2[7:0] : eclk_partial_width[7:0]                                                                                  
	//IO.writeData16Transaction(0x01);
	//IO.writeCommand16Transaction(0xcfc2);// cfc3[7:0] : all_normal_tchop[7:0]                                                                                    
	//IO.writeData16Transaction(0x20);
	//IO.writeCommand16Transaction(0xcfc3);// cfc4[7:0] : all_partial_tchop[7:0]                                                                                   
	//IO.writeData16Transaction(0x20);
	//IO.writeCommand16Transaction(0xcfc4);// cfc5[7:0] : eclk1_follow[3:0], eclk2_follow[3:0]                                                                     
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcfc5);// cfc6[7:0] : eclk3_follow[3:0], eclk4_follow[3:0]                                                                     
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcfc6);// cfc7[7:0] : 00, vstmask, vendmask, 00, dir1, dir2 (0=VGL, 1=VGH)                                                     
	//IO.writeData16Transaction(0x01);
	//IO.writeCommand16Transaction(0xcfc7);// cfc8[7:0] : reg_goa_gnd_opt, reg_goa_dpgm_tail_set, reg_goa_f_gating_en, reg_goa_f_odd_gating, toggle_mod1, 2, 3, 4  
	//IO.writeData16Transaction(0x00);    // GND OPT1 (00-->80  2011/10/28)
	//IO.writeCommand16Transaction(0xcfc8);// cfc9[7:0] : duty_block[3:0], DGPM[3:0]                                                                               
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcfc9);// cfca[7:0] : reg_goa_gnd_period[7:0]                                                                                  
	//IO.writeData16Transaction(0x00);    // Gate PCH (CLK base) (00-->0a  2011/10/28)

	//IO.writeCommand16Transaction(0xcfd0);// cfd1[7:0] : 0000000, reg_goa_frame_odd_high
	//IO.writeData16Transaction(0x00);

	//IO.writeCommand16Transaction(0xcbc0);//cbc1[7:0] : enmode H-byte of sig1  (pwrof_0, pwrof_1, norm, pwron_4 )           
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcbc1);//cbc2[7:0] : enmode H-byte of sig2  (pwrof_0, pwrof_1, norm, pwron_4 )          
	//IO.writeData16Transaction(0x04);
	//IO.writeCommand16Transaction(0xcbc2);//cbc3[7:0] : enmode H-byte of sig3  (pwrof_0, pwrof_1, norm, pwron_4 )           
	//IO.writeData16Transaction(0x04);
	//IO.writeCommand16Transaction(0xcbc3);//cbc4[7:0] : enmode H-byte of sig4  (pwrof_0, pwrof_1, norm, pwron_4 )        
	//IO.writeData16Transaction(0x04);
	//IO.writeCommand16Transaction(0xcbc4);//cbc5[7:0] : enmode H-byte of sig5  (pwrof_0, pwrof_1, norm, pwron_4 )             
	//IO.writeData16Transaction(0x04);
	//IO.writeCommand16Transaction(0xcbc5);//cbc6[7:0] : enmode H-byte of sig6  (pwrof_0, pwrof_1, norm, pwron_4 )           
	//IO.writeData16Transaction(0x04);
	//IO.writeCommand16Transaction(0xcbc6);//cbc7[7:0] : enmode H-byte of sig7  (pwrof_0, pwrof_1, norm, pwron_4 )           
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcbc7);//cbc8[7:0] : enmode H-byte of sig8  (pwrof_0, pwrof_1, norm, pwron_4 )           
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcbc8);//cbc9[7:0] : enmode H-byte of sig9  (pwrof_0, pwrof_1, norm, pwron_4 )           
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcbc9);//cbca[7:0] : enmode H-byte of sig10 (pwrof_0, pwrof_1, norm, pwron_4 )        
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcbca);//cbcb[7:0] : enmode H-byte of sig11 (pwrof_0, pwrof_1, norm, pwron_4 )        
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcbcb);//cbcc[7:0] : enmode H-byte of sig12 (pwrof_0, pwrof_1, norm, pwron_4 )        
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcbcc);//cbcd[7:0] : enmode H-byte of sig13 (pwrof_0, pwrof_1, norm, pwron_4 )        
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcbcd);//cbce[7:0] : enmode H-byte of sig14 (pwrof_0, pwrof_1, norm, pwron_4 ) 
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcbce);//cbcf[7:0] : enmode H-byte of sig15 (pwrof_0, pwrof_1, norm, pwron_4 )
	//IO.writeData16Transaction(0x00);

	//IO.writeCommand16Transaction(0xcbd0);//cbd1[7:0] : enmode H-byte of sig16 (pwrof_0, pwrof_1, norm, pwron_4 )           
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcbd1);//cbd2[7:0] : enmode H-byte of sig17 (pwrof_0, pwrof_1, norm, pwron_4 )
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcbd2);//cbd3[7:0] : enmode H-byte of sig18 (pwrof_0, pwrof_1, norm, pwron_4 )
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcbd3);//cbd4[7:0] : enmode H-byte of sig19 (pwrof_0, pwrof_1, norm, pwron_4 )
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcbd4);//cbd5[7:0] : enmode H-byte of sig20 (pwrof_0, pwrof_1, norm, pwron_4 )
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcbd5);//cbd6[7:0] : enmode H-byte of sig21 (pwrof_0, pwrof_1, norm, pwron_4 )
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcbd6);//cbd7[7:0] : enmode H-byte of sig22 (pwrof_0, pwrof_1, norm, pwron_4 )
	//IO.writeData16Transaction(0x04);
	//IO.writeCommand16Transaction(0xcbd7);//cbd8[7:0] : enmode H-byte of sig23 (pwrof_0, pwrof_1, norm, pwron_4 )
	//IO.writeData16Transaction(0x04);
	//IO.writeCommand16Transaction(0xcbd8);//cbd9[7:0] : enmode H-byte of sig24 (pwrof_0, pwrof_1, norm, pwron_4 )
	//IO.writeData16Transaction(0x04);
	//IO.writeCommand16Transaction(0xcbd9);//cbda[7:0] : enmode H-byte of sig25 (pwrof_0, pwrof_1, norm, pwron_4 )
	//IO.writeData16Transaction(0x04);
	//IO.writeCommand16Transaction(0xcbda);//cbdb[7:0] : enmode H-byte of sig26 (pwrof_0, pwrof_1, norm, pwron_4 )
	//IO.writeData16Transaction(0x04);
	//IO.writeCommand16Transaction(0xcbdb);//cbdc[7:0] : enmode H-byte of sig27 (pwrof_0, pwrof_1, norm, pwron_4 )
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcbdc);//cbdd[7:0] : enmode H-byte of sig28 (pwrof_0, pwrof_1, norm, pwron_4 )
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcbdd);//cbde[7:0] : enmode H-byte of sig29 (pwrof_0, pwrof_1, norm, pwron_4 )
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcbde);//cbdf[7:0] : enmode H-byte of sig30 (pwrof_0, pwrof_1, norm, pwron_4 )
	//IO.writeData16Transaction(0x00);

	//IO.writeCommand16Transaction(0xcbe0);//cbe1[7:0] : enmode H-byte of sig31 (pwrof_0, pwrof_1, norm, pwron_4 )             
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcbe1);//cbe2[7:0] : enmode H-byte of sig32 (pwrof_0, pwrof_1, norm, pwron_4 )
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcbe2);//cbe3[7:0] : enmode H-byte of sig33 (pwrof_0, pwrof_1, norm, pwron_4 )
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcbe3);//cbe4[7:0] : enmode H-byte of sig34 (pwrof_0, pwrof_1, norm, pwron_4 )
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcbe4);//cbe5[7:0] : enmode H-byte of sig35 (pwrof_0, pwrof_1, norm, pwron_4 )
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcbe5);//cbe6[7:0] : enmode H-byte of sig36 (pwrof_0, pwrof_1, norm, pwron_4 )
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcbe6);//cbe7[7:0] : enmode H-byte of sig37 (pwrof_0, pwrof_1, norm, pwron_4 )
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcbe7);//cbe8[7:0] : enmode H-byte of sig38 (pwrof_0, pwrof_1, norm, pwron_4 )
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcbe8);//cbe9[7:0] : enmode H-byte of sig39 (pwrof_0, pwrof_1, norm, pwron_4 )
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcbe9);//cbea[7:0] : enmode H-byte of sig40 (pwrof_0, pwrof_1, norm, pwron_4 )
	//IO.writeData16Transaction(0x00);

	//// cc8x   
	//IO.writeCommand16Transaction(0xcc80);//cc81[7:0] : reg setting for signal01 selection with u2d mode   
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcc81);//cc82[7:0] : reg setting for signal02 selection with u2d mode 
	//IO.writeData16Transaction(0x26);
	//IO.writeCommand16Transaction(0xcc82);//cc83[7:0] : reg setting for signal03 selection with u2d mode 
	//IO.writeData16Transaction(0x09);
	//IO.writeCommand16Transaction(0xcc83);//cc84[7:0] : reg setting for signal04 selection with u2d mode 
	//IO.writeData16Transaction(0x0B);
	//IO.writeCommand16Transaction(0xcc84);//cc85[7:0] : reg setting for signal05 selection with u2d mode 
	//IO.writeData16Transaction(0x01);
	//IO.writeCommand16Transaction(0xcc85);//cc86[7:0] : reg setting for signal06 selection with u2d mode 
	//IO.writeData16Transaction(0x25);
	//IO.writeCommand16Transaction(0xcc86);//cc87[7:0] : reg setting for signal07 selection with u2d mode 
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcc87);//cc88[7:0] : reg setting for signal08 selection with u2d mode 
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcc88);//cc89[7:0] : reg setting for signal09 selection with u2d mode 
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcc89);//cc8a[7:0] : reg setting for signal10 selection with u2d mode 
	//IO.writeData16Transaction(0x00);

	//// cc9x   
	//IO.writeCommand16Transaction(0xcc90);//cc91[7:0] : reg setting for signal11 selection with u2d mode   
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcc91);//cc92[7:0] : reg setting for signal12 selection with u2d mode
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcc92);//cc93[7:0] : reg setting for signal13 selection with u2d mode 
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcc93);//cc94[7:0] : reg setting for signal14 selection with u2d mode 
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcc94);//cc95[7:0] : reg setting for signal15 selection with u2d mode 
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcc95);//cc96[7:0] : reg setting for signal16 selection with u2d mode 
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcc96);//cc97[7:0] : reg setting for signal17 selection with u2d mode 
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcc97);//cc98[7:0] : reg setting for signal18 selection with u2d mode 
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcc98);//cc99[7:0] : reg setting for signal19 selection with u2d mode 
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcc99);//cc9a[7:0] : reg setting for signal20 selection with u2d mode 
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcc9a);//cc9b[7:0] : reg setting for signal21 selection with u2d mode 
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcc9b);//cc9c[7:0] : reg setting for signal22 selection with u2d mode 
	//IO.writeData16Transaction(0x26);
	//IO.writeCommand16Transaction(0xcc9c);//cc9d[7:0] : reg setting for signal23 selection with u2d mode 
	//IO.writeData16Transaction(0x0A);
	//IO.writeCommand16Transaction(0xcc9d);//cc9e[7:0] : reg setting for signal24 selection with u2d mode 
	//IO.writeData16Transaction(0x0C);
	//IO.writeCommand16Transaction(0xcc9e);//cc9f[7:0] : reg setting for signal25 selection with u2d mode 
	//IO.writeData16Transaction(0x02);
	//// ccax   
	//IO.writeCommand16Transaction(0xcca0);//cca1[7:0] : reg setting for signal26 selection with u2d mode   
	//IO.writeData16Transaction(0x25);
	//IO.writeCommand16Transaction(0xcca1);//cca2[7:0] : reg setting for signal27 selection with u2d mode
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcca2);//cca3[7:0] : reg setting for signal28 selection with u2d mode 
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcca3);//cca4[7:0] : reg setting for signal29 selection with u2d mode 
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcca4);//cca5[7:0] : reg setting for signal20 selection with u2d mode 
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcca5);//cca6[7:0] : reg setting for signal31 selection with u2d mode 
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcca6);//cca7[7:0] : reg setting for signal32 selection with u2d mode 
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcca7);//cca8[7:0] : reg setting for signal33 selection with u2d mode 
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcca8);//cca9[7:0] : reg setting for signal34 selection with u2d mode 
	//IO.writeData16Transaction(0x00);
	//IO.writeCommand16Transaction(0xcca9);//ccaa[7:0] : reg setting for signal35 selection with u2d mode 
	//IO.writeData16Transaction(0x00);

	//IO.writeCommand16Transaction(0x3A00);//ccaa[7:0] : reg setting for signal35 selection with u2d mode 
	//IO.writeData16Transaction(0x55);//0x55

	//IO.writeCommand16Transaction(0x1100);
	//delay(100);
	//IO.writeCommand16Transaction(0x2900);
	//delay(50);
	//IO.writeCommand16Transaction(0x2C00);

	//IO.writeCommand16Transaction(0x3600);//выбираем ориентацию диспле€ и пор€док цветов BGR
	//IO.writeData16Transaction((1 << 3) | (1 << 5) | (1 << 6));

//===========================================================================================================================


  //============ OTM8009A+HSD3.97 20140613 ===============================================//
  IO.writeCommand16Transaction(0xff00);      IO.writeData16Transaction(0x80);    //enable access command2
  IO.writeCommand16Transaction(0xff01);      IO.writeData16Transaction(0x09);    //enable access command2
  IO.writeCommand16Transaction(0xff02);      IO.writeData16Transaction(0x01);    //enable access command2
  IO.writeCommand16Transaction(0xff80);      IO.writeData16Transaction(0x80);    //enable access command2
  IO.writeCommand16Transaction(0xff81);      IO.writeData16Transaction(0x09);    //enable access command2
  IO.writeCommand16Transaction(0xff03);      IO.writeData16Transaction(0x01);    //
  //========================================================================================
  IO.writeCommand16Transaction(0xf5b6);      IO.writeData16Transaction(0x06);    //
  IO.writeCommand16Transaction(0xc480);      IO.writeData16Transaction(0x30);    //
  IO.writeCommand16Transaction(0xc48a);      IO.writeData16Transaction(0x40);    //




  IO.writeCommand16Transaction(0xc5b1);      IO.writeData16Transaction(0xA9);    //power control

  IO.writeCommand16Transaction(0xc591);      IO.writeData16Transaction(0x0F);    //power control
  IO.writeCommand16Transaction(0xc0B4);      IO.writeData16Transaction(0x50);

  //panel driving mode : column inversion

  //////  gamma
  IO.writeCommand16Transaction(0xE100);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xE101);      IO.writeData16Transaction(0x09);
  IO.writeCommand16Transaction(0xE102);      IO.writeData16Transaction(0x0F);
  IO.writeCommand16Transaction(0xE103);      IO.writeData16Transaction(0x0E);
  IO.writeCommand16Transaction(0xE104);      IO.writeData16Transaction(0x07);
  IO.writeCommand16Transaction(0xE105);      IO.writeData16Transaction(0x10);
  IO.writeCommand16Transaction(0xE106);      IO.writeData16Transaction(0x0B);
  IO.writeCommand16Transaction(0xE107);      IO.writeData16Transaction(0x0A);
  IO.writeCommand16Transaction(0xE108);      IO.writeData16Transaction(0x04);
  IO.writeCommand16Transaction(0xE109);      IO.writeData16Transaction(0x07);
  IO.writeCommand16Transaction(0xE10A);      IO.writeData16Transaction(0x0B);
  IO.writeCommand16Transaction(0xE10B);      IO.writeData16Transaction(0x08);
  IO.writeCommand16Transaction(0xE10C);      IO.writeData16Transaction(0x0F);
  IO.writeCommand16Transaction(0xE10D);      IO.writeData16Transaction(0x10);
  IO.writeCommand16Transaction(0xE10E);      IO.writeData16Transaction(0x0A);
  IO.writeCommand16Transaction(0xE10F);      IO.writeData16Transaction(0x01);
  IO.writeCommand16Transaction(0xE200);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xE201);      IO.writeData16Transaction(0x09);
  IO.writeCommand16Transaction(0xE202);      IO.writeData16Transaction(0x0F);
  IO.writeCommand16Transaction(0xE203);      IO.writeData16Transaction(0x0E);
  IO.writeCommand16Transaction(0xE204);      IO.writeData16Transaction(0x07);
  IO.writeCommand16Transaction(0xE205);      IO.writeData16Transaction(0x10);
  IO.writeCommand16Transaction(0xE206);      IO.writeData16Transaction(0x0B);
  IO.writeCommand16Transaction(0xE207);      IO.writeData16Transaction(0x0A);
  IO.writeCommand16Transaction(0xE208);      IO.writeData16Transaction(0x04);
  IO.writeCommand16Transaction(0xE209);      IO.writeData16Transaction(0x07);
  IO.writeCommand16Transaction(0xE20A);      IO.writeData16Transaction(0x0B);
  IO.writeCommand16Transaction(0xE20B);      IO.writeData16Transaction(0x08);
  IO.writeCommand16Transaction(0xE20C);      IO.writeData16Transaction(0x0F);
  IO.writeCommand16Transaction(0xE20D);      IO.writeData16Transaction(0x10);
  IO.writeCommand16Transaction(0xE20E);      IO.writeData16Transaction(0x0A);
  IO.writeCommand16Transaction(0xE20F);      IO.writeData16Transaction(0x01);
  IO.writeCommand16Transaction(0xD900);      IO.writeData16Transaction(0x4E);    //vcom setting

  IO.writeCommand16Transaction(0xc181);      IO.writeData16Transaction(0x66);    //osc=65HZ

  IO.writeCommand16Transaction(0xc1a1);      IO.writeData16Transaction(0x08);
  IO.writeCommand16Transaction(0xc592);      IO.writeData16Transaction(0x01);    //power control

  IO.writeCommand16Transaction(0xc595);      IO.writeData16Transaction(0x34);    //power control

  IO.writeCommand16Transaction(0xd800);      IO.writeData16Transaction(0x79);    //GVDD / NGVDD setting

  IO.writeCommand16Transaction(0xd801);      IO.writeData16Transaction(0x79);    //GVDD / NGVDD setting

  IO.writeCommand16Transaction(0xc594);      IO.writeData16Transaction(0x33);    //power control

  IO.writeCommand16Transaction(0xc0a3);      IO.writeData16Transaction(0x1B);    //panel timing setting
  IO.writeCommand16Transaction(0xc582);      IO.writeData16Transaction(0x83);    //power control

  IO.writeCommand16Transaction(0xc481);      IO.writeData16Transaction(0x83);    //source driver setting

  IO.writeCommand16Transaction(0xc1a1);      IO.writeData16Transaction(0x0E);
  IO.writeCommand16Transaction(0xb3a6);      IO.writeData16Transaction(0x20);
  IO.writeCommand16Transaction(0xb3a7);      IO.writeData16Transaction(0x01);
  IO.writeCommand16Transaction(0xce80);      IO.writeData16Transaction(0x85);    // GOA VST

  IO.writeCommand16Transaction(0xce81);      IO.writeData16Transaction(0x01);    // GOA VST

  IO.writeCommand16Transaction(0xce82);      IO.writeData16Transaction(0x00);    // GOA VST

  IO.writeCommand16Transaction(0xce83);      IO.writeData16Transaction(0x84);    // GOA VST
  IO.writeCommand16Transaction(0xce84);      IO.writeData16Transaction(0x01);    // GOA VST
  IO.writeCommand16Transaction(0xce85);      IO.writeData16Transaction(0x00);    // GOA VST

  IO.writeCommand16Transaction(0xcea0);      IO.writeData16Transaction(0x18);    // GOA CLK
  IO.writeCommand16Transaction(0xcea1);      IO.writeData16Transaction(0x04);    // GOA CLK
  IO.writeCommand16Transaction(0xcea2);      IO.writeData16Transaction(0x03);    // GOA CLK
  IO.writeCommand16Transaction(0xcea3);      IO.writeData16Transaction(0x39);    // GOA CLK
  IO.writeCommand16Transaction(0xcea4);      IO.writeData16Transaction(0x00);    // GOA CLK
  IO.writeCommand16Transaction(0xcea5);      IO.writeData16Transaction(0x00);    // GOA CLK
  IO.writeCommand16Transaction(0xcea6);      IO.writeData16Transaction(0x00);    // GOA CLK
  IO.writeCommand16Transaction(0xcea7);      IO.writeData16Transaction(0x18);    // GOA CLK
  IO.writeCommand16Transaction(0xcea8);      IO.writeData16Transaction(0x03);    // GOA CLK
  IO.writeCommand16Transaction(0xcea9);      IO.writeData16Transaction(0x03);    // GOA CLK
  IO.writeCommand16Transaction(0xceaa);      IO.writeData16Transaction(0x3a);    // GOA CLK
  IO.writeCommand16Transaction(0xceab);      IO.writeData16Transaction(0x00);    // GOA CLK
  IO.writeCommand16Transaction(0xceac);      IO.writeData16Transaction(0x00);    // GOA CLK
  IO.writeCommand16Transaction(0xcead);      IO.writeData16Transaction(0x00);    // GOA CLK
  IO.writeCommand16Transaction(0xceb0);      IO.writeData16Transaction(0x18);    // GOA CLK
  IO.writeCommand16Transaction(0xceb1);      IO.writeData16Transaction(0x02);    // GOA CLK
  IO.writeCommand16Transaction(0xceb2);      IO.writeData16Transaction(0x03);    // GOA CLK
  IO.writeCommand16Transaction(0xceb3);      IO.writeData16Transaction(0x3b);    // GOA CLK
  IO.writeCommand16Transaction(0xceb4);      IO.writeData16Transaction(0x00);    // GOA CLK
  IO.writeCommand16Transaction(0xceb5);      IO.writeData16Transaction(0x00);    // GOA CLK
  IO.writeCommand16Transaction(0xceb6);      IO.writeData16Transaction(0x00);    // GOA CLK
  IO.writeCommand16Transaction(0xceb7);      IO.writeData16Transaction(0x18);    // GOA CLK
  IO.writeCommand16Transaction(0xceb8);      IO.writeData16Transaction(0x01);    // GOA CLK
  IO.writeCommand16Transaction(0xceb9);      IO.writeData16Transaction(0x03);    // GOA CLK
  IO.writeCommand16Transaction(0xceba);      IO.writeData16Transaction(0x3c);    // GOA CLK
  IO.writeCommand16Transaction(0xcebb);      IO.writeData16Transaction(0x00);    // GOA CLK
  IO.writeCommand16Transaction(0xcebc);      IO.writeData16Transaction(0x00);    // GOA CLK
  IO.writeCommand16Transaction(0xcebd);      IO.writeData16Transaction(0x00);    // GOA CLK
  IO.writeCommand16Transaction(0xcfc0);      IO.writeData16Transaction(0x01);    // GOA ECLK
  IO.writeCommand16Transaction(0xcfc1);      IO.writeData16Transaction(0x01);    // GOA ECLK
  IO.writeCommand16Transaction(0xcfc2);      IO.writeData16Transaction(0x20);    // GOA ECLK

  IO.writeCommand16Transaction(0xcfc3);      IO.writeData16Transaction(0x20);    // GOA ECLK

  IO.writeCommand16Transaction(0xcfc4);      IO.writeData16Transaction(0x00);    // GOA ECLK

  IO.writeCommand16Transaction(0xcfc5);      IO.writeData16Transaction(0x00);    // GOA ECLK

  IO.writeCommand16Transaction(0xcfc6);      IO.writeData16Transaction(0x01);    // GOA other options

  IO.writeCommand16Transaction(0xcfc7);      IO.writeData16Transaction(0x00);

  // GOA signal toggle option setting

  IO.writeCommand16Transaction(0xcfc8);      IO.writeData16Transaction(0x00);    //GOA signal toggle option setting
  IO.writeCommand16Transaction(0xcfc9);      IO.writeData16Transaction(0x00);

  //GOA signal toggle option setting

  IO.writeCommand16Transaction(0xcfd0);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcb80);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcb81);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcb82);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcb83);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcb84);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcb85);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcb86);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcb87);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcb88);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcb89);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcb90);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcb91);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcb92);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcb93);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcb94);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcb95);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcb96);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcb97);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcb98);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcb99);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcb9a);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcb9b);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcb9c);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcb9d);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcb9e);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcba0);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcba1);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcba2);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcba3);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcba4);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcba5);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcba6);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcba7);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcba8);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcba9);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcbaa);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcbab);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcbac);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcbad);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcbae);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcbb0);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcbb1);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcbb2);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcbb3);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcbb4);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcbb5);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcbb6);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcbb7);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcbb8);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcbb9);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcbc0);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcbc1);      IO.writeData16Transaction(0x04);
  IO.writeCommand16Transaction(0xcbc2);      IO.writeData16Transaction(0x04);
  IO.writeCommand16Transaction(0xcbc3);      IO.writeData16Transaction(0x04);
  IO.writeCommand16Transaction(0xcbc4);      IO.writeData16Transaction(0x04);
  IO.writeCommand16Transaction(0xcbc5);      IO.writeData16Transaction(0x04);
  IO.writeCommand16Transaction(0xcbc6);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcbc7);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcbc8);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcbc9);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcbca);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcbcb);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcbcc);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcbcd);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcbce);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcbd0);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcbd1);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcbd2);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcbd3);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcbd4);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcbd5);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcbd6);      IO.writeData16Transaction(0x04);
  IO.writeCommand16Transaction(0xcbd7);      IO.writeData16Transaction(0x04);
  IO.writeCommand16Transaction(0xcbd8);      IO.writeData16Transaction(0x04);
  IO.writeCommand16Transaction(0xcbd9);      IO.writeData16Transaction(0x04);
  IO.writeCommand16Transaction(0xcbda);      IO.writeData16Transaction(0x04);
  IO.writeCommand16Transaction(0xcbdb);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcbdc);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcbdd);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcbde);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcbe0);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcbe1);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcbe2);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcbe3);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcbe4);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcbe5);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcbe6);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcbe7);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcbe8);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcbe9);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcbf0);      IO.writeData16Transaction(0xFF);
  IO.writeCommand16Transaction(0xcbf1);      IO.writeData16Transaction(0xFF);
  IO.writeCommand16Transaction(0xcbf2);      IO.writeData16Transaction(0xFF);
  IO.writeCommand16Transaction(0xcbf3);      IO.writeData16Transaction(0xFF);
  IO.writeCommand16Transaction(0xcbf4);      IO.writeData16Transaction(0xFF);
  IO.writeCommand16Transaction(0xcbf5);      IO.writeData16Transaction(0xFF);
  IO.writeCommand16Transaction(0xcbf6);      IO.writeData16Transaction(0xFF);
  IO.writeCommand16Transaction(0xcbf7);      IO.writeData16Transaction(0xFF);
  IO.writeCommand16Transaction(0xcbf8);      IO.writeData16Transaction(0xFF);
  IO.writeCommand16Transaction(0xcbf9);      IO.writeData16Transaction(0xFF);
  IO.writeCommand16Transaction(0xcc80);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcc81);      IO.writeData16Transaction(0x26);
  IO.writeCommand16Transaction(0xcc82);      IO.writeData16Transaction(0x09);
  IO.writeCommand16Transaction(0xcc83);      IO.writeData16Transaction(0x0B);
  IO.writeCommand16Transaction(0xcc84);      IO.writeData16Transaction(0x01);
  IO.writeCommand16Transaction(0xcc85);      IO.writeData16Transaction(0x25);
  IO.writeCommand16Transaction(0xcc86);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcc87);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcc88);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcc89);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcc90);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcc91);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcc92);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcc93);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcc94);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcc95);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcc96);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcc97);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcc98);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcc99);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcc9a);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcc9b);      IO.writeData16Transaction(0x26);
  IO.writeCommand16Transaction(0xcc9c);      IO.writeData16Transaction(0x0A);
  IO.writeCommand16Transaction(0xcc9d);      IO.writeData16Transaction(0x0C);
  IO.writeCommand16Transaction(0xcc9e);      IO.writeData16Transaction(0x02);
  IO.writeCommand16Transaction(0xcca0);      IO.writeData16Transaction(0x25);
  IO.writeCommand16Transaction(0xcca1);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcca2);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcca3);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcca4);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcca5);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcca6);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcca7);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcca8);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcca9);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xccaa);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xccab);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xccac);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xccad);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xccae);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xccb0);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xccb1);      IO.writeData16Transaction(0x25);
  IO.writeCommand16Transaction(0xccb2);      IO.writeData16Transaction(0x0C);
  IO.writeCommand16Transaction(0xccb3);      IO.writeData16Transaction(0x0A);
  IO.writeCommand16Transaction(0xccb4);      IO.writeData16Transaction(0x02);
  IO.writeCommand16Transaction(0xccb5);      IO.writeData16Transaction(0x26);
  IO.writeCommand16Transaction(0xccb6);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xccb7);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xccb8);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xccb9);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xccc0);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xccc1);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xccc2);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xccc3);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xccc4);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xccc5);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xccc6);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xccc7);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xccc8);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xccc9);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xccca);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xcccb);      IO.writeData16Transaction(0x25);
  IO.writeCommand16Transaction(0xcccc);      IO.writeData16Transaction(0x0B);
  IO.writeCommand16Transaction(0xcccd);      IO.writeData16Transaction(0x09);
  IO.writeCommand16Transaction(0xccce);      IO.writeData16Transaction(0x01);
  IO.writeCommand16Transaction(0xccd0);      IO.writeData16Transaction(0x26);
  IO.writeCommand16Transaction(0xccd1);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xccd2);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xccd3);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xccd4);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xccd5);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xccd6);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xccd7);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xccd8);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xccd9);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xccda);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xccdb);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xccdc);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xccdd);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0xccde);      IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0x3A00);      IO.writeData16Transaction(0x55);

  IO.writeCommand16Transaction(0x1100);
  delay(100);
  IO.writeCommand16Transaction(0x2900);
  delay(50);
  IO.writeCommand16Transaction(0x2C00);
  IO.writeCommand16Transaction(0x2A00);     IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0x2A01);     IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0x2A02);     IO.writeData16Transaction(0x01);
  IO.writeCommand16Transaction(0x2A03);     IO.writeData16Transaction(0xe0);
  IO.writeCommand16Transaction(0x2B00);     IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0x2B01);     IO.writeData16Transaction(0x00);
  IO.writeCommand16Transaction(0x2B02);     IO.writeData16Transaction(0x03);
  IO.writeCommand16Transaction(0x2B03);     IO.writeData16Transaction(0x20);
}

void GxCTRL_OTM8009A::setWindowAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
  IO.writeCommand16(0x2A00);
  IO.writeData16(x0 >> 8);
  IO.writeCommand16(0x2A01);
  IO.writeData16(x0 & 0x00ff);
  IO.writeCommand16(0x2A02);
  IO.writeData16(x1 >> 8);
  IO.writeCommand16(0x2A03);
  IO.writeData16(x1 & 0x00ff);
  IO.writeCommand16(0x2B00);
  IO.writeData16(y0 >> 8);
  IO.writeCommand16(0x2B01);
  IO.writeData16(y0 & 0x00ff);
  IO.writeCommand16(0x2B02);
  IO.writeData16(y1 >> 8);
  IO.writeCommand16(0x2B03);
  IO.writeData16(y1 & 0x00ff);
  IO.writeCommand16(0x2C00);
}

void GxCTRL_OTM8009A::setRotation(uint8_t r)
{
  IO.startTransaction();
  IO.writeCommand16(OTM8009A_MADCTL);
  switch (r & 3)
  {
    case 0:
      IO.writeData(0);
      break;
    case 1:
      IO.writeData(OTM8009A_MADCTL_MX | OTM8009A_MADCTL_MV);
      break;
    case 2:
      IO.writeData(OTM8009A_MADCTL_MX | OTM8009A_MADCTL_MY);
      break;
    case 3:
      IO.writeData(OTM8009A_MADCTL_MY | OTM8009A_MADCTL_MV);
      break;
  }
  IO.endTransaction();
}

