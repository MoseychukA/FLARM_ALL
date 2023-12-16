#include "LCD_ini.h"

/////////////////////////////////////////////////////////////////////////
////////     ?????? ??? STM32F4-Discovery (STM32F407VGT6)     ///////////
////   ??????? ?? ??????????? ILI9341, ???? ? ????? STMF429I-Disco  /////
////////////      ????????????  StdPeriph Library Driver     ////////////
/////////////////////// Zlodey ?????? 2014 //////////////////////////////
/////////////////////////////////////////////////////////////////////////

// ???????????? ????????
void __delay_ms(uint32_t ms)
{
    volatile uint32_t nCount;
//    RCC_ClocksTypeDef RCC_Clocks;
//    RCC_GetClocksFreq (&RCC_Clocks);

//    nCount=(RCC_Clocks.HCLK_Frequency/10000)*ms;
    for (; nCount!=0; nCount--);
}

// *********  ???????? ???????  ************//
void ILI9341SendCommand(uint16_t com) {
 //LCD_REG = com; 
	*(__IO uint16_t *)(0x60000000) = com;	
}

// *********  ???????? 1 ????? ?????? ************//
void ILI9341SendData(uint16_t data) { 
 //LCD_DATA = data;
	*(__IO uint16_t *)(0x60080000) = data;	
}		

// *********  ????? 1 ????? ?????? ************//
uint16_t ILI9341ResaveData(uint16_t data) { 
  int tmp1;
  *(__IO uint16_t *)(0x60000000) = 0x0000;
  tmp1 =  *(__IO uint16_t *)(0x60080000);   
  return tmp1;
}		

// ********* ???????? ???? ? ?????? ILI9341 ************// 
//void ILI9341_SetCursorPosition(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {

//  ILI9341SendCommand (ILI9341_COLUMN_ADDR);	
//  ILI9341SendData(x1>>8);
//  ILI9341SendData(x1 & 0xFF);	
//  ILI9341SendData(x2>>8);	
//  ILI9341SendData(x2 & 0xFF);	
//				
//  ILI9341SendCommand (ILI9341_PAGE_ADDR);		
//  ILI9341SendData(y1>>8);	
//  ILI9341SendData(y1 & 0xFF);		
//  ILI9341SendData(y2>>8);
//  ILI9341SendData(y2 & 0xFF);		
//  ILI9341SendCommand (ILI9341_GRAM);		
//}

// ?????????????? FSMC ??????????, ? ???????? ????????? ??????? ?? ???? Intel-8080 (D0...D15, RS, WR, RD, CS)
// ???? ?????? ??????? 16 ???
// ??????????? ??????? ? ???????????:
// FSMC D0  <-> LCD D0
// FSMC D1  <-> LCD D1
// FSMC D2  <-> LCD D2
//         ...
// FSMC D15 <-> LCD D15
// FSMC NWE <-> LCD WR
// FSMC NOE <-> LCD RD
// FSMC NE1 <-> LCD CS
// FSMC A18 <-> LCD RS

void FSMC_ini(void)
 {
//   // ????????? ????????? ??? ????????????? GPIO
//   GPIO_InitTypeDef GPIO_InitStructure;
//   FSMC_NORSRAMInitTypeDef  FSMC_NORSRAMInitStructure;
//   FSMC_NORSRAMTimingInitTypeDef  p;
//	 
//   // ???????? ???????????? ?????? GPIOD, GPIOE, ? AFIO
//   RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD | RCC_AHB1Periph_GPIOE, ENABLE);

//   // SRAM Data lines,  NOE and NWE configuration
//   // GPIOD ?????? ???????? ??? ??????
//   GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_4 | GPIO_Pin_5 |
//                                 GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 |
//                                GPIO_Pin_10 |GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
//   GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;                 // ????? ?????????????? ???????
//   GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;            // ??????? 50 ???
//   GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;               // ??????????? ?????
//   GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;            // ??? ????????
//   GPIO_Init(GPIOD, &GPIO_InitStructure);                       // ????????? ?????????????
//   // GPIOD ???????????? FSMC ????????????
//   GPIO_PinAFConfig(GPIOD, GPIO_PinSource0, GPIO_AF_FSMC);      // GPIO D0  <-> FSMC D2
//   GPIO_PinAFConfig(GPIOD, GPIO_PinSource1, GPIO_AF_FSMC);      // GPIO D1  <-> FSMC D3
//   GPIO_PinAFConfig(GPIOD, GPIO_PinSource4, GPIO_AF_FSMC);      // GPIO D4  <-> FSMC NOE
//   GPIO_PinAFConfig(GPIOD, GPIO_PinSource5, GPIO_AF_FSMC);      // GPIO D5  <-> FSMC NWE
//   GPIO_PinAFConfig(GPIOD, GPIO_PinSource7, GPIO_AF_FSMC);      // GPIO D7  <-> FSMC NE1
//   GPIO_PinAFConfig(GPIOD, GPIO_PinSource8, GPIO_AF_FSMC);      // GPIO D8  <-> FSMC D13
//   GPIO_PinAFConfig(GPIOD, GPIO_PinSource9, GPIO_AF_FSMC);      // GPIO D9  <-> FSMC D14
//   GPIO_PinAFConfig(GPIOD, GPIO_PinSource10, GPIO_AF_FSMC);     // GPIO D10 <-> FSMC D15
//   GPIO_PinAFConfig(GPIOD, GPIO_PinSource13, GPIO_AF_FSMC);     // GPIO D13 <-> FSMC A18
//   GPIO_PinAFConfig(GPIOD, GPIO_PinSource14, GPIO_AF_FSMC);     // GPIO D14 <-> FSMC D0
//   GPIO_PinAFConfig(GPIOD, GPIO_PinSource15, GPIO_AF_FSMC);     // GPIO D15 <-> FSMC D1

//   // GPIOE ?????? ???????? ??? ??????
//   GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 |
//                                GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | 
//                                GPIO_Pin_15;
//   GPIO_Init(GPIOE, &GPIO_InitStructure);
//   // GPIOE ???????????? FSMC ????????????
//   GPIO_PinAFConfig(GPIOE, GPIO_PinSource7 , GPIO_AF_FSMC);     // GPIO E7  <-> FSMC D4
//   GPIO_PinAFConfig(GPIOE, GPIO_PinSource8 , GPIO_AF_FSMC);     // GPIO E8  <-> FSMC D5
//   GPIO_PinAFConfig(GPIOE, GPIO_PinSource9 , GPIO_AF_FSMC);     // GPIO E9  <-> FSMC D6
//   GPIO_PinAFConfig(GPIOE, GPIO_PinSource10 , GPIO_AF_FSMC);    // GPIO E10 <-> FSMC D7
//   GPIO_PinAFConfig(GPIOE, GPIO_PinSource11 , GPIO_AF_FSMC);    // GPIO E11 <-> FSMC D8
//   GPIO_PinAFConfig(GPIOE, GPIO_PinSource12 , GPIO_AF_FSMC);    // GPIO E12 <-> FSMC D9
//   GPIO_PinAFConfig(GPIOE, GPIO_PinSource13 , GPIO_AF_FSMC);    // GPIO E13 <-> FSMC D10
//   GPIO_PinAFConfig(GPIOE, GPIO_PinSource14 , GPIO_AF_FSMC);    // GPIO E14 <-> FSMC D11
//   GPIO_PinAFConfig(GPIOE, GPIO_PinSource15 , GPIO_AF_FSMC);    // GPIO E15 <-> FSMC D12
//   
//	//LED
//	 RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);	
//	 GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
//	 GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
//	 GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
//	 GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
//	 GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;	
//	 GPIO_Init(GPIOB, &GPIO_InitStructure);
//	 
//   // ????????? ??????????? FSMC    
//   // ???????? ???????????? FSMC
//   RCC_AHB3PeriphClockCmd(RCC_AHB3Periph_FSMC, ENABLE);
//   
//   // SRAM Bank 1
//   // ???????????? FSMC_Bank1_NORSRAM1
//   p.FSMC_AddressSetupTime = 0x6;
//   p.FSMC_AddressHoldTime = 0;
//   p.FSMC_DataSetupTime = 0x6;
//   p.FSMC_BusTurnAroundDuration = 0;
//   p.FSMC_CLKDivision = 0;
//   p.FSMC_DataLatency = 0;	 
//   p.FSMC_AccessMode = FSMC_AccessMode_A;
//   // Color LCD configuration
//   //   LCD configured as follow:
//   //      - Data/Address MUX = Disable
//   //      - Memory Type = SRAM
//   //      - Data Width = 16bit
//   //      - Write Operation = Enable
//   //      - Extended Mode = Enable
//   //      - Asynchronous Wait = Disable */
//   FSMC_NORSRAMInitStructure.FSMC_Bank = FSMC_Bank1_NORSRAM1;
//   FSMC_NORSRAMInitStructure.FSMC_DataAddressMux = FSMC_DataAddressMux_Disable;
//   FSMC_NORSRAMInitStructure.FSMC_MemoryType = FSMC_MemoryType_SRAM;
//   FSMC_NORSRAMInitStructure.FSMC_MemoryDataWidth = FSMC_MemoryDataWidth_16b;
//   FSMC_NORSRAMInitStructure.FSMC_BurstAccessMode = FSMC_BurstAccessMode_Disable;
//   FSMC_NORSRAMInitStructure.FSMC_AsynchronousWait = FSMC_AsynchronousWait_Disable;
//   FSMC_NORSRAMInitStructure.FSMC_WaitSignalPolarity = FSMC_WaitSignalPolarity_Low;
//   FSMC_NORSRAMInitStructure.FSMC_WrapMode = FSMC_WrapMode_Disable;
//   FSMC_NORSRAMInitStructure.FSMC_WaitSignalActive = FSMC_WaitSignalActive_BeforeWaitState;
//   FSMC_NORSRAMInitStructure.FSMC_WriteOperation = FSMC_WriteOperation_Enable;
//   FSMC_NORSRAMInitStructure.FSMC_WaitSignal = FSMC_WaitSignal_Disable;
//   FSMC_NORSRAMInitStructure.FSMC_ExtendedMode = FSMC_ExtendedMode_Disable;
//   FSMC_NORSRAMInitStructure.FSMC_WriteBurst = FSMC_WriteBurst_Disable;
//   FSMC_NORSRAMInitStructure.FSMC_ReadWriteTimingStruct = &p;
//   FSMC_NORSRAMInitStructure.FSMC_WriteTimingStruct = &p; 
//   FSMC_NORSRAMInit(&FSMC_NORSRAMInitStructure);  
//   // Enable FSMC NOR/SRAM Bank1
//   FSMC_NORSRAMCmd(FSMC_Bank1_NORSRAM1, ENABLE);
 }

// ?????????????? ???????
void LCD_ini(void)
 {
	 FSMC_ini();
   LCD_LED_ON;
	 
   ILI9341SendCommand (ILI9341_RESET); // software reset comand
   __delay_ms(100);
   ILI9341SendCommand (ILI9341_DISPLAY_OFF); // display off
   //------------power control------------------------------
   ILI9341SendCommand (ILI9341_POWER1); // power control
   ILI9341SendData   (0x26); // GVDD = 4.75v
   ILI9341SendCommand (ILI9341_POWER2); // power control
   ILI9341SendData   (0x11); // AVDD=VCIx2, VGH=VCIx7, VGL=-VCIx3
   //--------------VCOM-------------------------------------
   ILI9341SendCommand (ILI9341_VCOM1); // vcom control
   ILI9341SendData   (0x35); // Set the VCOMH voltage (0x35 = 4.025v)
   ILI9341SendData   (0x3e); // Set the VCOML voltage (0x3E = -0.950v)
   ILI9341SendCommand (ILI9341_VCOM2); // vcom control
   ILI9341SendData   (0xbe); // 0x94 (0xBE = nVM: 1, VCOMH: VMH–2, VCOML: VML–2)
	 
   //------------memory access control------------------------
   ILI9341SendCommand (ILI9341_MAC); // memory access control
	 //ILI9341SendData   (0x48); // 0048 my,mx,mv,ml,BGR,mh,0.0 (mirrors)
   #if Dspl_Rotation_0_degr
   ILI9341SendData(0x48); //0????????
   #elif Dspl_Rotation_90_degr
   ILI9341SendData(0x28); //90????????
   #elif Dspl_Rotation_180_degr
   ILI9341SendData(0x88); //180????????
   #elif Dspl_Rotation_270_degr
   ILI9341SendData(0xE8); //270????????
   #endif	   
	 
   ILI9341SendCommand (ILI9341_PIXEL_FORMAT); // pixel format set
   ILI9341SendData   (0x55); // 16bit /pixel
	 
	 ILI9341SendCommand(ILI9341_FRC);
   ILI9341SendData(0);
   #if Fram_Rate_61Hz
   ILI9341SendData(0x1F);
   #elif Fram_Rate_70Hz
   ILI9341SendData(0x1B);
   #elif Fram_Rate_90Hz
   ILI9341SendData(0x15);
   #elif Fram_Rate_100Hz
   ILI9341SendData(0x13);
   #elif Fram_Rate_119Hz
   ILI9341SendData(0x10);
   #endif
	 
   //-------------ddram ----------------------------
   ILI9341SendCommand (ILI9341_COLUMN_ADDR); // column set
   ILI9341SendData   (0x00); // x0_HIGH---0
   ILI9341SendData   (0x00); // x0_LOW----0
   ILI9341SendData   (0x00); // x1_HIGH---240
   ILI9341SendData   (0xEF); // x1_LOW----240
   ILI9341SendCommand (ILI9341_PAGE_ADDR); // page address set
   ILI9341SendData   (0x00); // y0_HIGH---0
   ILI9341SendData   (0x00); // y0_LOW----0
   ILI9341SendData   (0x01); // y1_HIGH---320
   ILI9341SendData   (0x3F); // y1_LOW----320
	 
   ILI9341SendCommand (ILI9341_TEARING_OFF); // tearing effect off
   //LCD_write_cmd(ILI9341_TEARING_ON); // tearing effect on
   //LCD_write_cmd(ILI9341_DISPLAY_INVERSION); // display inversion
   ILI9341SendCommand (ILI9341_Entry_Mode_Set); // entry mode set
   // Deep Standby Mode: OFF
   // Set the output level of gate driver G1~G320: Normal display
   // Low voltage detection: Disable
   ILI9341SendData   (0x07); 
   //-----------------display------------------------
   ILI9341SendCommand (ILI9341_DFC); // display function control
   //Set the scan mode in non-display area
   //Determine source/VCOM output in a non-display area in the partial display mode
   ILI9341SendData   (0x0a);
   //Select whether the liquid crystal type is normally white type or normally black type
   //Sets the direction of scan by the gate driver in the range determined by SCN and NL
   //Select the shift direction of outputs from the source driver
   //Sets the gate driver pin arrangement in combination with the GS bit to select the optimal scan mode for the module
   //Specify the scan cycle interval of gate driver in non-display area when PTG to select interval scan
   ILI9341SendData   (0x82);
   // Sets the number of lines to drive the LCD at an interval of 8 lines
   ILI9341SendData   (0x27); 
   ILI9341SendData   (0x00); // clock divisor
	 
   ILI9341SendCommand (ILI9341_SLEEP_OUT); // sleep out
   __delay_ms(100);
   ILI9341SendCommand (ILI9341_DISPLAY_ON); // display on
   __delay_ms(100);
   ILI9341SendCommand (ILI9341_GRAM); // memory write
   __delay_ms(5);
	 
	 //ILI9341_Opts.Width = ILI9341_HEIGHT;
	 //ILI9341_Opts.Height = ILI9341_WIDTH;
	 //ILI9341_Opts.orientation = ILI9341_Landscape; //ILI9341_Portrait;	
 }

//==============================================================
