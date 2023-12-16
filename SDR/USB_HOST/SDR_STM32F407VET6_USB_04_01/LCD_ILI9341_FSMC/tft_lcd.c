#include "tft_lcd.h"

uint16_t ILI9341_x;
uint16_t ILI9341_y;
ILI9341_Options_t ILI9341_Opts;

/////////////////////////////////////////////////////////////////////////
////////     проект для STM32F4-Discovery (STM32F407VGT6)     ///////////
////   дисплей на контроллере ILI9341, взят с платы STMF429I-Disco  /////
////////////      используется  StdPeriph Library Driver     ////////////
/////////////////////// Zlodey январь 2014 //////////////////////////////
/////////////////////////////////////////////////////////////////////////

// подпрограмма задержки
void _delay_ms(uint32_t ms)
{
    volatile uint32_t nCount;
    RCC_ClocksTypeDef RCC_Clocks;
    RCC_GetClocksFreq (&RCC_Clocks);

    nCount=(RCC_Clocks.HCLK_Frequency/10000)*ms;
    for (; nCount!=0; nCount--);
}

// *********  Отправка КОМАНДЫ  ************//
void ILI9341_SendCommand(uint16_t com) {
 //LCD_REG = com; 
	*(__IO uint16_t *)(0x60000000) = com;	
}

// *********  Отправка 1 байта ДАННЫХ ************//
void ILI9341_SendData(uint16_t data) { 
 //LCD_DATA = data;
	*(__IO uint16_t *)(0x60080000) = data;	
}		

// *********  Прием 1 байта ДАННЫХ ************//
uint16_t ILI9341_ResaveData(uint16_t data) { 
  int tmp1;
  *(__IO uint16_t *)(0x60000000) = 0x0000;
  tmp1 =  *(__IO uint16_t *)(0x60080000);   
  return tmp1;
}		

// ********* Выделяем поле в памяти ILI9341 ************// 
void ILI9341_SetCursorPosition(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {

  ILI9341_SendCommand (ILI9341_COLUMN_ADDR);	
  ILI9341_SendData(x1>>8);
  ILI9341_SendData(x1 & 0xFF);	
  ILI9341_SendData(x2>>8);	
  ILI9341_SendData(x2 & 0xFF);	
				
  ILI9341_SendCommand (ILI9341_PAGE_ADDR);		
  ILI9341_SendData(y1>>8);	
  ILI9341_SendData(y1 & 0xFF);		
  ILI9341_SendData(y2>>8);
  ILI9341_SendData(y2 & 0xFF);		
  ILI9341_SendCommand (ILI9341_GRAM);		
}

// Инициализируем FSMC контроллер, к которому подключен дисплей по шине Intel-8080 (D0...D15, RS, WR, RD, CS)
// Шина данных дисплея 16 бит
// Подключение дисплея к контроллеру:
// FSMC D0  <-> LCD D0
// FSMC D1  <-> LCD D1
// FSMC D2  <-> LCD D2
//         ...
// FSMC D15 <-> LCD D15
// FSMC NWE <-> LCD WR
// FSMC NOE <-> LCD RD
// FSMC NE1 <-> LCD CS
// FSMC A18 <-> LCD RS

void Init_FSMC(void)
 {
   // Объявляем структуру для инициализации GPIO
   GPIO_InitTypeDef GPIO_InitStructure;
   FSMC_NORSRAMInitTypeDef  FSMC_NORSRAMInitStructure;
   FSMC_NORSRAMTimingInitTypeDef  p;
	 
   // Включить тактирование портов GPIOD, GPIOE, и AFIO
   RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD | RCC_AHB1Periph_GPIOE, ENABLE);

   // SRAM Data lines,  NOE and NWE configuration
   // GPIOD выводы настроим как выходы
   GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_4 | GPIO_Pin_5 |
                                 GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 |
                                GPIO_Pin_10 |GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
   GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;                 // режим альтернативной функции
   GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;            // частота 50 МГц
   GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;               // двухтактный выход
   GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;            // без подтяжки
   GPIO_Init(GPIOD, &GPIO_InitStructure);                       // выполняем инициализацию
   // GPIOD используются FSMC контроллером
   GPIO_PinAFConfig(GPIOD, GPIO_PinSource0, GPIO_AF_FSMC);      // GPIO D0  <-> FSMC D2
   GPIO_PinAFConfig(GPIOD, GPIO_PinSource1, GPIO_AF_FSMC);      // GPIO D1  <-> FSMC D3
   GPIO_PinAFConfig(GPIOD, GPIO_PinSource4, GPIO_AF_FSMC);      // GPIO D4  <-> FSMC NOE
   GPIO_PinAFConfig(GPIOD, GPIO_PinSource5, GPIO_AF_FSMC);      // GPIO D5  <-> FSMC NWE
   GPIO_PinAFConfig(GPIOD, GPIO_PinSource7, GPIO_AF_FSMC);      // GPIO D7  <-> FSMC NE1
   GPIO_PinAFConfig(GPIOD, GPIO_PinSource8, GPIO_AF_FSMC);      // GPIO D8  <-> FSMC D13
   GPIO_PinAFConfig(GPIOD, GPIO_PinSource9, GPIO_AF_FSMC);      // GPIO D9  <-> FSMC D14
   GPIO_PinAFConfig(GPIOD, GPIO_PinSource10, GPIO_AF_FSMC);     // GPIO D10 <-> FSMC D15
   GPIO_PinAFConfig(GPIOD, GPIO_PinSource13, GPIO_AF_FSMC);     // GPIO D13 <-> FSMC A18
   GPIO_PinAFConfig(GPIOD, GPIO_PinSource14, GPIO_AF_FSMC);     // GPIO D14 <-> FSMC D0
   GPIO_PinAFConfig(GPIOD, GPIO_PinSource15, GPIO_AF_FSMC);     // GPIO D15 <-> FSMC D1

   // GPIOE выводы настроим как выходы
   GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 |
                                GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | 
                                GPIO_Pin_15;
   GPIO_Init(GPIOE, &GPIO_InitStructure);
   // GPIOE используются FSMC контроллером
   GPIO_PinAFConfig(GPIOE, GPIO_PinSource7 , GPIO_AF_FSMC);     // GPIO E7  <-> FSMC D4
   GPIO_PinAFConfig(GPIOE, GPIO_PinSource8 , GPIO_AF_FSMC);     // GPIO E8  <-> FSMC D5
   GPIO_PinAFConfig(GPIOE, GPIO_PinSource9 , GPIO_AF_FSMC);     // GPIO E9  <-> FSMC D6
   GPIO_PinAFConfig(GPIOE, GPIO_PinSource10 , GPIO_AF_FSMC);    // GPIO E10 <-> FSMC D7
   GPIO_PinAFConfig(GPIOE, GPIO_PinSource11 , GPIO_AF_FSMC);    // GPIO E11 <-> FSMC D8
   GPIO_PinAFConfig(GPIOE, GPIO_PinSource12 , GPIO_AF_FSMC);    // GPIO E12 <-> FSMC D9
   GPIO_PinAFConfig(GPIOE, GPIO_PinSource13 , GPIO_AF_FSMC);    // GPIO E13 <-> FSMC D10
   GPIO_PinAFConfig(GPIOE, GPIO_PinSource14 , GPIO_AF_FSMC);    // GPIO E14 <-> FSMC D11
   GPIO_PinAFConfig(GPIOE, GPIO_PinSource15 , GPIO_AF_FSMC);    // GPIO E15 <-> FSMC D12
   
	//LED
	 RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);	
	 GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
	 GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	 GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
	 GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	 GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;	
	 GPIO_Init(GPIOB, &GPIO_InitStructure);
	 
   // Настройка контроллера FSMC    
   // Включить тактирование FSMC
   RCC_AHB3PeriphClockCmd(RCC_AHB3Periph_FSMC, ENABLE);
   
   // SRAM Bank 1
   // Конфигурация FSMC_Bank1_NORSRAM1
   p.FSMC_AddressSetupTime = 0x6;
   p.FSMC_AddressHoldTime = 0;
   p.FSMC_DataSetupTime = 0x6;
   p.FSMC_BusTurnAroundDuration = 0;
   p.FSMC_CLKDivision = 0;
   p.FSMC_DataLatency = 0;	 
   p.FSMC_AccessMode = FSMC_AccessMode_A;
   // Color LCD configuration
   //   LCD configured as follow:
   //      - Data/Address MUX = Disable
   //      - Memory Type = SRAM
   //      - Data Width = 16bit
   //      - Write Operation = Enable
   //      - Extended Mode = Enable
   //      - Asynchronous Wait = Disable */
   FSMC_NORSRAMInitStructure.FSMC_Bank = FSMC_Bank1_NORSRAM1;
   FSMC_NORSRAMInitStructure.FSMC_DataAddressMux = FSMC_DataAddressMux_Disable;
   FSMC_NORSRAMInitStructure.FSMC_MemoryType = FSMC_MemoryType_SRAM;
   FSMC_NORSRAMInitStructure.FSMC_MemoryDataWidth = FSMC_MemoryDataWidth_16b;
   FSMC_NORSRAMInitStructure.FSMC_BurstAccessMode = FSMC_BurstAccessMode_Disable;
   FSMC_NORSRAMInitStructure.FSMC_AsynchronousWait = FSMC_AsynchronousWait_Disable;
   FSMC_NORSRAMInitStructure.FSMC_WaitSignalPolarity = FSMC_WaitSignalPolarity_Low;
   FSMC_NORSRAMInitStructure.FSMC_WrapMode = FSMC_WrapMode_Disable;
   FSMC_NORSRAMInitStructure.FSMC_WaitSignalActive = FSMC_WaitSignalActive_BeforeWaitState;
   FSMC_NORSRAMInitStructure.FSMC_WriteOperation = FSMC_WriteOperation_Enable;
   FSMC_NORSRAMInitStructure.FSMC_WaitSignal = FSMC_WaitSignal_Disable;
   FSMC_NORSRAMInitStructure.FSMC_ExtendedMode = FSMC_ExtendedMode_Disable;
   FSMC_NORSRAMInitStructure.FSMC_WriteBurst = FSMC_WriteBurst_Disable;
   FSMC_NORSRAMInitStructure.FSMC_ReadWriteTimingStruct = &p;
   FSMC_NORSRAMInitStructure.FSMC_WriteTimingStruct = &p; 
   FSMC_NORSRAMInit(&FSMC_NORSRAMInitStructure);  
   // Enable FSMC NOR/SRAM Bank1
   FSMC_NORSRAMCmd(FSMC_Bank1_NORSRAM1, ENABLE);
 }

// инициализируем дисплей
void ILI9341_Init(void)
 {
	 Init_FSMC();
   LCD_LED_ON;
	 
   ILI9341_SendCommand (ILI9341_RESET); // software reset comand
   _delay_ms(100);
   ILI9341_SendCommand (ILI9341_DISPLAY_OFF); // display off
   //------------power control------------------------------
   ILI9341_SendCommand (ILI9341_POWER1); // power control
   ILI9341_SendData   (0x26); // GVDD = 4.75v
   ILI9341_SendCommand (ILI9341_POWER2); // power control
   ILI9341_SendData   (0x11); // AVDD=VCIx2, VGH=VCIx7, VGL=-VCIx3
   //--------------VCOM-------------------------------------
   ILI9341_SendCommand (ILI9341_VCOM1); // vcom control
   ILI9341_SendData   (0x35); // Set the VCOMH voltage (0x35 = 4.025v)
   ILI9341_SendData   (0x3e); // Set the VCOML voltage (0x3E = -0.950v)
   ILI9341_SendCommand (ILI9341_VCOM2); // vcom control
   ILI9341_SendData   (0xbe); // 0x94 (0xBE = nVM: 1, VCOMH: VMH–2, VCOML: VML–2)
	 
   //------------memory access control------------------------
   ILI9341_SendCommand (ILI9341_MAC); // memory access control
	 //ILI9341_SendData   (0x48); // 0048 my,mx,mv,ml,BGR,mh,0.0 (mirrors)
   #if Dspl_Rotation_0_degr
   ILI9341_SendData(0x48); //0градусов
   #elif Dspl_Rotation_90_degr
   ILI9341_SendData(0x28); //90градусов
   #elif Dspl_Rotation_180_degr
   ILI9341_SendData(0x88); //180градусов
   #elif Dspl_Rotation_270_degr
   ILI9341_SendData(0xE8); //270градусов
   #endif	   
	 
   ILI9341_SendCommand (ILI9341_PIXEL_FORMAT); // pixel format set
   ILI9341_SendData   (0x55); // 16bit /pixel
	 
	 ILI9341_SendCommand(ILI9341_FRC);
   ILI9341_SendData(0);
   #if Fram_Rate_61Hz
   ILI9341_SendData(0x1F);
   #elif Fram_Rate_70Hz
   ILI9341_SendData(0x1B);
   #elif Fram_Rate_90Hz
   ILI9341_SendData(0x15);
   #elif Fram_Rate_100Hz
   ILI9341_SendData(0x13);
   #elif Fram_Rate_119Hz
   ILI9341_SendData(0x10);
   #endif
	 
   //-------------ddram ----------------------------
   ILI9341_SendCommand (ILI9341_COLUMN_ADDR); // column set
   ILI9341_SendData   (0x00); // x0_HIGH---0
   ILI9341_SendData   (0x00); // x0_LOW----0
   ILI9341_SendData   (0x00); // x1_HIGH---240
   ILI9341_SendData   (0xEF); // x1_LOW----240
   ILI9341_SendCommand (ILI9341_PAGE_ADDR); // page address set
   ILI9341_SendData   (0x00); // y0_HIGH---0
   ILI9341_SendData   (0x00); // y0_LOW----0
   ILI9341_SendData   (0x01); // y1_HIGH---320
   ILI9341_SendData   (0x3F); // y1_LOW----320
	 
   ILI9341_SendCommand (ILI9341_TEARING_OFF); // tearing effect off
   //LCD_write_cmd(ILI9341_TEARING_ON); // tearing effect on
   //LCD_write_cmd(ILI9341_DISPLAY_INVERSION); // display inversion
   ILI9341_SendCommand (ILI9341_Entry_Mode_Set); // entry mode set
   // Deep Standby Mode: OFF
   // Set the output level of gate driver G1~G320: Normal display
   // Low voltage detection: Disable
   ILI9341_SendData   (0x07); 
   //-----------------display------------------------
   ILI9341_SendCommand (ILI9341_DFC); // display function control
   //Set the scan mode in non-display area
   //Determine source/VCOM output in a non-display area in the partial display mode
   ILI9341_SendData   (0x0a);
   //Select whether the liquid crystal type is normally white type or normally black type
   //Sets the direction of scan by the gate driver in the range determined by SCN and NL
   //Select the shift direction of outputs from the source driver
   //Sets the gate driver pin arrangement in combination with the GS bit to select the optimal scan mode for the module
   //Specify the scan cycle interval of gate driver in non-display area when PTG to select interval scan
   ILI9341_SendData   (0x82);
   // Sets the number of lines to drive the LCD at an interval of 8 lines
   ILI9341_SendData   (0x27); 
   ILI9341_SendData   (0x00); // clock divisor
	 
   ILI9341_SendCommand (ILI9341_SLEEP_OUT); // sleep out
   _delay_ms(100);
   ILI9341_SendCommand (ILI9341_DISPLAY_ON); // display on
   _delay_ms(100);
   ILI9341_SendCommand (ILI9341_GRAM); // memory write
   _delay_ms(5);
	 ILI9341_Opts.Width = ILI9341_HEIGHT;
	 ILI9341_Opts.Height = ILI9341_WIDTH;
	 ILI9341_Opts.orientation = ILI9341_Landscape; //ILI9341_Portrait;	
 }

//==============================================================


//******************************************************************************
//***       ИНИЦИАЛИЗАЦИЯ ДИСПЛЕЯ
//******************************************************************************
// кому интересно, можно почитать даташит, там действительно много интересного ;-)
// или просто используем, все реально работает
// не забываем про две задержки в 100мс (0,1 сек) (см. ниже)
//void ILI9341_Init(void){
//  Init_FSMC();
//	LCD_LED_ON;
//	
//  // сброс дисплея
//  ILI9341_SendCommand(ILI9341_RESET);
//  _delay_ms(100);
//  /// комманды и данные
//  ILI9341_SendCommand(ILI9341_POWER1);
//  ILI9341_SendData(0x23); 
//  ILI9341_SendCommand(ILI9341_POWER2);
//  ILI9341_SendData(0x10);
//  ILI9341_SendCommand(ILI9341_VCOM1);
//  ILI9341_SendData(0x3E);
//  ILI9341_SendData(0x28);
//  ILI9341_SendCommand(ILI9341_VCOM2);
//  ILI9341_SendData(0x86);
//	
//  ILI9341_SendCommand(ILI9341_MAC);
//  #if Dspl_Rotation_0_degr
//  ILI9341_SendData(0x48); //0градусов
//  #elif Dspl_Rotation_90_degr
//  ILI9341_SendData(0x28); //90градусов
//  #elif Dspl_Rotation_180_degr
//  ILI9341_SendData(0x88); //180градусов
//  #elif Dspl_Rotation_270_degr
//  ILI9341_SendData(0xE8); //270градусов
//  #endif
//	
//  ILI9341_SendCommand(ILI9341_FRC);
//  ILI9341_SendData(0);
//  #if Fram_Rate_61Hz
//  ILI9341_SendData(0x1F);
//  #elif Fram_Rate_70Hz
//  ILI9341_SendData(0x1B);
//  #elif Fram_Rate_90Hz
//  ILI9341_SendData(0x15);
//  #elif Fram_Rate_100Hz
//  ILI9341_SendData(0x13);
//  #elif Fram_Rate_119Hz
//  ILI9341_SendData(0x10);
//  #endif
//	
////  ILI9341_SendCommand(ILI9341_WDB);//Write Display Brightness
////  ILI9341_SendData(0xFF);	
//  ILI9341_SendCommand(ILI9341_PIXEL_FORMAT);
//  ILI9341_SendData(0x55); //0x66 -18bit 0x55-16bit

//  ILI9341_SendCommand(ILI9341_DFC);
//  ILI9341_SendData(0x08);
//  ILI9341_SendData(0x82);
//  ILI9341_SendData(0x27);

//  ILI9341_SendCommand(ILI9341_COLUMN_ADDR);
//  ILI9341_SendData(0x00);
//  ILI9341_SendData(0x00);
//  ILI9341_SendData(0x00);
//  ILI9341_SendData(0xEF);
//  ILI9341_SendCommand(ILI9341_PAGE_ADDR);
//  ILI9341_SendData(0x00);
//  ILI9341_SendData(0x00);
//  ILI9341_SendData(0x01);
//  ILI9341_SendData(0x3F);
//	
////  ILI9341_SendCommand(ILI9341_POWERA);
////  ILI9341_SendData(0x39);
////  ILI9341_SendData(0x2C);
////  ILI9341_SendData(0x00);
////  ILI9341_SendData(0x34);
////  ILI9341_SendData(0x02);
////  ILI9341_SendCommand(ILI9341_POWERB);
////  ILI9341_SendData(0x00);
////  ILI9341_SendData(0xC1);
////  ILI9341_SendData(0x30);
////  ILI9341_SendCommand(ILI9341_DTCA);
////  ILI9341_SendData(0x85);
////  ILI9341_SendData(0x00);
////  ILI9341_SendData(0x78);
////  ILI9341_SendCommand(ILI9341_DTCB);
////  ILI9341_SendData(0x00);
////  ILI9341_SendData(0x00);
////  ILI9341_SendCommand(ILI9341_POWER_SEQ);
////  ILI9341_SendData(0x64);
////  ILI9341_SendData(0x03);
////  ILI9341_SendData(0x12);
////  ILI9341_SendData(0x81);
////  ILI9341_SendCommand(ILI9341_PRC);
////  ILI9341_SendData(0x20);	
//  ILI9341_SendCommand(ILI9341_3GAMMA_EN);
//  ILI9341_SendData(0x00);	
//  ILI9341_SendCommand(ILI9341_GAMMA);
//  ILI9341_SendData(0x01); 
//  ILI9341_SendCommand(ILI9341_PGAMMA);
//  ILI9341_SendData(0x0F);
//  ILI9341_SendData(0x31);
//  ILI9341_SendData(0x2B);
//  ILI9341_SendData(0x0C);
//  ILI9341_SendData(0x0E);
//  ILI9341_SendData(0x08);
//  ILI9341_SendData(0x4E);
//  ILI9341_SendData(0xF1);
//  ILI9341_SendData(0x37);
//  ILI9341_SendData(0x07);
//  ILI9341_SendData(0x10);
//  ILI9341_SendData(0x03);
//  ILI9341_SendData(0x0E);
//  ILI9341_SendData(0x09);
//  ILI9341_SendData(0x00);
//  ILI9341_SendCommand(ILI9341_NGAMMA);
//  ILI9341_SendData(0x00);
//  ILI9341_SendData(0x0E);
//  ILI9341_SendData(0x14);
//  ILI9341_SendData(0x03);
//  ILI9341_SendData(0x11);
//  ILI9341_SendData(0x07);
//  ILI9341_SendData(0x31);
//  ILI9341_SendData(0xC1);
//  ILI9341_SendData(0x48);
//  ILI9341_SendData(0x08);
//  ILI9341_SendData(0x0F);
//  ILI9341_SendData(0x0C);
//  ILI9341_SendData(0x31);
//  ILI9341_SendData(0x36);
//  ILI9341_SendData(0x0F);

//  ILI9341_SendCommand(ILI9341_SLEEP_OUT);
//  _delay_ms(100);
//  ILI9341_SendCommand(ILI9341_DISPLAY_ON);
//  ILI9341_SendCommand(ILI9341_GRAM);
//	
//	ILI9341_Opts.Width = ILI9341_HEIGHT;
//	ILI9341_Opts.Height = ILI9341_WIDTH;
//	ILI9341_Opts.orientation = ILI9341_Landscape; //ILI9341_Portrait;	
//} 

//=================================================================
void ILI9341_DrawPixel(uint16_t x, uint16_t y, uint16_t color) {
  ILI9341_SetCursorPosition(x, y, x, y); 
	ILI9341_SendData(color);
}
//=================================================================
void ILI9341_Fill(uint16_t color) { 
	uint32_t n = ILI9341_PIXEL_COUNT; 
	ILI9341_SetCursorPosition(0, 0,   ILI9341_Opts.Width -1, ILI9341_Opts.Height -1); 
	while (n) {
			n--;	
       ILI9341_SendData(color);	
	}
}
//=================================================================
void ILI9341_Fill_Rect(unsigned int x0,unsigned int y0, unsigned int x1,unsigned int y1, uint16_t color) { // просто заливка одним цветом	
	uint32_t n = ((x1+1)-x0)*((y1+1)-y0);
	if (n>ILI9341_PIXEL_COUNT) n=ILI9341_PIXEL_COUNT;	
	ILI9341_SetCursorPosition(x0, y0, x1, y1); 
	while (n) {
			n--;	    
      ILI9341_SendData(color);		
	}	
}
//=================================================================
void ILI9341_DrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color) {
	int16_t dx, dy, sx, sy, err, e2; 
	
	dx = (x0 < x1) ? (x1 - x0) : (x0 - x1); 
	dy = (y0 < y1) ? (y1 - y0) : (y0 - y1); 
	sx = (x0 < x1) ? 1 : -1; 
	sy = (y0 < y1) ? 1 : -1; 
	err = ((dx > dy) ? dx : -dy) / 2; 
	
	while (1) {		
    ILI9341_DrawPixel(x0, y0, color); 		

		if (x0 == x1 && y0 == y1) {
			break;
		}
		e2 = err; 
		if (e2 > -dx) {
			err -= dy;
			x0 += sx;
		} 
		if (e2 < dy) {
			err += dx;
			y0 += sy;
		}
	}		
}
//=================================================================
void ILI9341_DrawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
  if ((x >= ILI9341_Opts.Width) || (y >= ILI9341_Opts.Height || h < 1)) return;
  if ((y + h - 1) >= ILI9341_Opts.Height)
    h = ILI9341_Opts.Height - y;
  if (h < 2 ) {
	ILI9341_DrawPixel(x, y, color);		
	return;
  }
}
//=================================================================
void ILI9341_DrawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
  if ((x >= ILI9341_Opts.Width) || (y >= ILI9341_Opts.Height || w < 1)) return;
  if ((x + w - 1) >= ILI9341_Opts.Width)  w = ILI9341_Opts.Width - x;
  if (w < 2 ) {
	ILI9341_DrawPixel(x, y, color);
	return;
  }
}
//=================================================================

void ILI9341_IMAGE(uint16_t x11, uint16_t y11, uint16_t size_x, uint16_t size_y, uint16_t *color) 
    {
     unsigned int n, m, i, j, dt;        
	
     for(m = 0; m < size_y; m++)
       {
        ILI9341_SetCursorPosition(x11, y11 + m, ILI9341_Opts.Width - 1, ILI9341_Opts.Height - 1);
        
	for (n = 0; n < size_x; n++) 
          {
           i = color[n+dt] >> 8;
	         j = color[n+dt] & 0xFF;
           ILI9341_SendData(i);
           ILI9341_SendData(j);          
	  }
        dt += size_x;
        
       }
    }
//=================================================================
void ILI9341_Rotate(ILI9341_Orientation_t orientation) {
	ILI9341_SendCommand(ILI9341_MAC);
	if (orientation == ILI9341_Orientation_Portrait_1) {
		ILI9341_SendData(0x58);
	} else if (orientation == ILI9341_Orientation_Portrait_2) {
		ILI9341_SendData(0x88);
	} else if (orientation == ILI9341_Orientation_Landscape_1) {
		ILI9341_SendData(0x28);
	} else if (orientation == ILI9341_Orientation_Landscape_2) {
		ILI9341_SendData(0xE8);
	}
	if (orientation == ILI9341_Orientation_Portrait_1 || orientation == ILI9341_Orientation_Portrait_2) {
		ILI9341_Opts.Width = ILI9341_WIDTH;
		ILI9341_Opts.Height = ILI9341_HEIGHT;
		ILI9341_Opts.orientation = ILI9341_Portrait;
	} else {
		ILI9341_Opts.Width = ILI9341_HEIGHT;
		ILI9341_Opts.Height = ILI9341_WIDTH;
		ILI9341_Opts.orientation = ILI9341_Landscape;
	}
}
//=================================================================
void ILI9341_Puts(uint16_t x, uint16_t y, char *str, FontDef_t *font, uint16_t foreground, uint16_t background) {
	uint16_t startX = x;
	
	/* Set X and Y coordinates */
	ILI9341_x = x;
	ILI9341_y = y;
	
	while (*str) {
		//New line
		if (*str == '\n') {
			ILI9341_y += font->FontHeight + 1;
			//if after \n is also \r, than go to the left of the screen
			if (*(str + 1) == '\r') {
				ILI9341_x = 0;
				str++;
			} else {
				ILI9341_x = startX;
			}
			str++;
			continue;
		} else if (*str == '\r') {
			str++;
			continue;
		}
		
		ILI9341_Putc(ILI9341_x, ILI9341_y, *str++, font, foreground, background);
	}
}
//=================================================================
void ILI9341_GetStringSize(char *str, FontDef_t *font, uint16_t *width, uint16_t *height) {
	uint16_t w = 0;
	*height = font->FontHeight;
	while (*str++) {
		w += font->FontWidth;
	}
	*width = w;
}
//=================================================================
void ILI9341_Putc(uint16_t x, uint16_t y, char c, FontDef_t *font, uint16_t foreground, uint16_t background) {
	uint32_t i, b, j;
	/* Set coordinates */
	ILI9341_x = x;
	ILI9341_y = y;
	if ((ILI9341_x + font->FontWidth) > ILI9341_Opts.Width) {
		//If at the end of a line of display, go to new line and set x to 0 position
		ILI9341_y += font->FontHeight;
		ILI9341_x = 0;
	}
	for (i = 0; i < font->FontHeight; i++) {
		b = font->data[(c - 32) * font->FontHeight + i];
		for (j = 0; j < font->FontWidth; j++) {
			if ((b << j) & 0x8000) {
				ILI9341_DrawPixel(ILI9341_x + j, (ILI9341_y + i), foreground);
			} else {
				ILI9341_DrawPixel(ILI9341_x + j, (ILI9341_y + i), background);
			}
		}
	}
	ILI9341_x += font->FontWidth;
}
//=================================================================

void ILI9341_DrawRectangle(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color) {
	ILI9341_DrawLine(x0, y0, x1, y0, color); //Top
	ILI9341_DrawLine(x0, y0, x0, y1, color);	//Left
	ILI9341_DrawLine(x1, y0, x1, y1, color);	//Right
	ILI9341_DrawLine(x0, y1, x1, y1, color);	//Bottom
}
//=================================================================
void ILI9341_DrawFilledRectangle(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color) {
	for (; y0 < y1; y0++) {
		ILI9341_DrawLine(x0, y0, x1, y0, color);
	}
}
//=================================================================
void ILI9341_DrawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
	int16_t f = 1 - r;
	int16_t ddF_x = 1;
	int16_t ddF_y = -2 * r;
	int16_t x = 0;
	int16_t y = r;

    ILI9341_DrawPixel(x0, y0 + r, color);
    ILI9341_DrawPixel(x0, y0 - r, color);
    ILI9341_DrawPixel(x0 + r, y0, color);
    ILI9341_DrawPixel(x0 - r, y0, color);

    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        ILI9341_DrawPixel(x0 + x, y0 + y, color);
        ILI9341_DrawPixel(x0 - x, y0 + y, color);
        ILI9341_DrawPixel(x0 + x, y0 - y, color);
        ILI9341_DrawPixel(x0 - x, y0 - y, color);

        ILI9341_DrawPixel(x0 + y, y0 + x, color);
        ILI9341_DrawPixel(x0 - y, y0 + x, color);
        ILI9341_DrawPixel(x0 + y, y0 - x, color);
        ILI9341_DrawPixel(x0 - y, y0 - x, color);
    }
}
//=================================================================
void ILI9341_DrawFilledCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
	int16_t f = 1 - r;
	int16_t ddF_x = 1;
	int16_t ddF_y = -2 * r;
	int16_t x = 0;
	int16_t y = r;

    ILI9341_DrawPixel(x0, y0 + r, color);
    ILI9341_DrawPixel(x0, y0 - r, color);
    ILI9341_DrawPixel(x0 + r, y0, color);
    ILI9341_DrawPixel(x0 - r, y0, color);
    ILI9341_DrawLine(x0 - r, y0, x0 + r, y0, color);

    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        ILI9341_DrawLine(x0 - x, y0 + y, x0 + x, y0 + y, color);
        ILI9341_DrawLine(x0 + x, y0 - y, x0 - x, y0 - y, color);

        ILI9341_DrawLine(x0 + y, y0 + x, x0 - y, y0 + x, color);
        ILI9341_DrawLine(x0 + y, y0 - x, x0 - y, y0 - x, color);
    }
}
//=================================================================





//=================================================================
//void ILI9341_Fill_String(unsigned int y, uint16_t color) { // просто заливка одним цветом
//ILI9341_Fill_Rect(0, (y-1)*FONT_info [1], ILI9341_Opts.Width, y*FONT_info [1], color);		
//}
//=================================================================

//void ILI9341_WriteString(unsigned char x0,unsigned int y0,unsigned char *s,unsigned int color, unsigned int back_color)// заменить, если надо выводить конкретный цвет фона
//{
//	  // пример отправки в эту функцию: 
//	  // WriteString( 1, 5, "Атм. давление" , GREEN);	
//		// где: 1 - строка,   5 - начало с 5-й точки строки, "собсно текст",  GREEN -  ранее задефайненный цвет, (16-бит) (#define GREEN 0x07E0)
//		// если требуется кроме текста передать некие значения, то используем спринтф и передаем через массив, (!) массив (array) надо объявить ранее!
//		// sprintf( array,"Атм.давление %u.%ummHg", Press_bmp/10, Press_bmp%10 );  -сперва передаем все в array (ранее объявленный)
//	  // WriteString( 1, 0, array , GREEN); и теперь из массива выводим строку
//	
//		// Даллее, в итоге всех манипуляций ниже в этой функции будем иметь:
//		
//		// d  - ширина символа бАйт, (не меняется до окончания строки)
//		// he - высота символа, строк
//		// wi - вычисленная ширина символа, бит (не меняется до окончания строки)
//	  // i  - номер начального (первого) байта символа в массиве, ессно меняется вместе с символом (PS.масив всех символов непрерывный )
//		// nb - номер конечного (последнего) байта символа в массиве
//		// по сути байты от i до nb это байты выводимого в данный момент символа		
//		// color - цвет символа
//		// (здесь не использовано)back_color - цвет фона (можно передать в функцию при желании, немного переделав код)
//	 	// размер стандартного пропуска между 127-м и 192-м по ANSI корректируется в функции "correct_ANSI", 
//		// т.к. не вижу смысла хранить в памяти МК кучу кракозябр
//	  // но! всегда присутствует коррекция на первые 32 непечатных символа по ANSI
//		// s - поход в массив шрифта (косвенная адр.) ...(*s ) 
//		// z - номер символа в кодировке ANSI, передаваемый в функцию из строки 
//		// st1 (start) - самый первый действительный (=1) бит слева по ширине символа, т.е. все =0 биты не рисуются, 
//		// en (end) - тоже справа от символа, используются для поиска фактической ширины конкретного символа
//		// k - номер п.порядку символа в строке при поиске
//		// q - для цикла, для строки, 
//	  // так-же дальше, для фактической (вычисленной) длины строки (используется повторно)
//		// j - очередной байт в строке
//	  // так-же дальше для отделения первого байта строки от последующих		
//		// x0 - переданныйИ в эту функцию номер строки, в которую будем писать символы
//		// y0 - переданный в эту функц. номер позиции (точки), (!)(т.е. конкретной точки на длине строки)
//		// spi16(); или //spi8(); используется при возможности переключения в 16-битный SPI, для бОльшей скорости вывода
//		// если нет 16-ти бит, то закомментить эти функции и перекомментить шесть строк кода, т.е. сделать выдачу двух байт в два приема
//		
//  unsigned char z, y, nb, he, wi, d, j, q, st1, en1,y2;
//	unsigned int x, i, k;
//	y2 = y0;            // копируем начальнуюу позицию для случая переноса длинной строки
//	d = FONT_info [0];  // ширина символа, бАйт, берется из файла шрифта
//	wi=d*8;             // вычисляемая ширина символа , бИт 
//	he = FONT_info [1]; // высота символа, строк, берется из файла шрифта
//	nb = (he*d);        //вычисляем кол-во байт на один символ

//   for(z = 0;s[z]!='\0';z++) // перебираем все символы в строке, пока не дойдем до пустышки
//    {
//	  if(s [z] < 128) 				    // если символ латинница, то..
//    {i = (s [z]-32)*nb;}      //корректируем только на первые 32 символа
//    else if (s [z] > 191)  		// если символ русский, то..
//		{i =(s [z]-32-(FONT_info [2]))*nb;} //пропуск кол-ва симв., между 127-м и 192-м по ANSI		(см.файл шрифта в конце)
//		else 																// если между рус. и англ, т.е. между 127 и 192, то	
//		{ 																	// идем корректировать в файл correct_ANSI.c в соответствии с нашими требованиями 
//			i = (corr_ANSI (s, z))*nb;				// , т.е. смотря сколько мы выкинули из шрифта всяких символов - кракозябр
//		}
//													// теперь получаем реальную ширину текущего символа, т.е. вычисляем ширину пустоты слева и справа от символа
//													// чтобы отрезать слишком широкие поля, скажем, от маленькой точки (PS. высота символа не меняется)
//		x = i;                                      // копируем номер первого байта символа в массиве (указываем байт, с которого начинается символ в массиве)
//		st1=0;										// просто очистка, от результатов предыдущего символа
//		en1=0;										// -*-*-
//		for (q = 0; q < he; q++)  // перебираем (проходим) "строки" символа
//		{				
//			for(j = 0, k = 0; j < d; j++)           // перебираем все байты строки, зайдя сюда выводим все байты текущей строки, 
//			{ 										                  // PS. "к" - счетчик байт с этой строке, ессно сбрасуется перед новой строкой
//				 	y = 8;      					              // счетчик бИт одного байта строки
//          while (y--) 						            // перебираем и проверяем бит за битом
//            {
//					k++;					       // прибавляем счетчик бита в строке							
//		      		if (((FONT[x]&(1<<y)) && (st1 == 0))||((FONT[x]&(1<<y)) && (k < st1))) {st1 = k;} // ищем среди всех строк самый левый бит =1							
//	      			if  ((FONT[x]&(1<<y)) && (en1 < k))  	{en1 = k;}							// ищем среди всех строк самый ПРАВЫЙ бит =1
//		    }
//					x++; 							// следующий байт этого символа
//			}	
//		}	
//		
//		if (st1 > 0) {st1--;} 				// немного уменьшаем, чтобы символы не "слипались"
//		if (en1 < 2){en1 = wi/3;}			// если символ пустой, то это пробел, задаем ему ширину 1/3 от ширины символа
//		else if (en1 < wi){en1 = (en1 - st1 + indent);} // высчитываем реальную ширину и прибавляем отступ (см. font.h), дабы обеспечить расстояние между симв.

//		j=0;      // обнуляем "отделитель-указатель" что это первый байт в строке
//		k = nb+i; // получаем номер последнего (в массиве) байта  этого символа
//		q = en1;  // копируем ширину символа, БИТ , en1 остается "хранительницей" ширины этого символа, пока выводятся "строки"
//				
//		if ((y0+en1) > ILI9341_Opts.Height) {y0=y2; x0++;}  // если последний символ не вмещается, то переносим на следующую строку (ILI9341_HEIGHT - см.дефайн)
//													                     // если не нужен перенос, то оставшиеся символы следует "убить" иначе в конце сроки будет клякса
//		ILI9341_SetCursorPosition( y0, x0*he,(y0)+(en1-1), (x0*he)+(he-1)); // выделяем поле в памяти ILI9341, х - вертикаль, у - горизонталь
//		y0=y0+en1; 														     // указываем у0, где в строке будет начало следующего символа
//		
//		///////////////////////////////////////////////////
//		//spi16();  // переключаем на 16-битный режим SPI	
//	  TFT_DC_SET;
//    //TFT_CS_RESET;			
//    for(x = i; x < k; x++) // проходим (в массиве) по очереди все байты текущего символа
//        {					
//         if (j++ == 0) {y = 10-st1;} // если это первый байт строки (j=0), то отнимаем пустые биты сначал строки, но прибавляем промежуток 2 pt
//		     else {y = 8;}               // значит это не первый байт строки
//		 
//         while (y--)                    // выводим байт строки, т.е. проверяем все биты (?=0 или ?=1) этого байта
//         {									
//           if((FONT[x]&(1<<y))!=0)        // бит =1 ? или =0 ?
//           {
//			       while (!SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE));		    
//             SPI1->DR = color;		// ! изменить под свой МК,
//		       }
//		       else
//		       { 			  
//			       while (!SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE));		    
//             SPI1->DR = back_color;		// ! изменить под свой МК,
//		       }  

//			    if(!--q)                        // смотрим сколько действительных бит строки вывели, если уже все (значит конец строки)
//					{
//						if (j != d){x = x+(d-j);} // то.. проверим, вдруг фактическая ширина меньше начальной больше чем на байт, если так, то пропустим ненужные 
//						                          // (пустые байты в массиве) (допустим: фактически точка = 3бит, а на 1 символ отведено 3 байта = 24бит, 2 байт "пустые")
//					  y = 0; j = 0; q = en1;    // но в любом случае обнуляем счетчик бит, байт строки и заносим ширину строки
//					} 					
//        }		 
//    }	
//			while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_BSY));
//	    TFT_CS_SET;			
//	  	//spi8(); // переключаем на 8-битный режим SPI  
//		  ///////////////////////////////////////////////////	
//    }
//}
//=================================================================
