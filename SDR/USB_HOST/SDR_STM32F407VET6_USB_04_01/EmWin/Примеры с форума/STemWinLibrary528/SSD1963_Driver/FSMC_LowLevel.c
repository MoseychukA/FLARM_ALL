// FSMC_LowLevel.c - низкоуровневый драйвер FSMC для LCD

// https://my.st.com/public/STe2ecommunities/mcu/Lists/cortex_mx_stm32/Attachments/19797/main.c

#include <stdint.h>
#include <stdbool.h>

#include "stm32f4xx.h"
#include "misc.h"
#include "stm32f4xx_dma.h"

#include "delay.h"
#include "FSMC_LowLevel.h"

// Для SSD1963
//  RS = 1 - данные
//  RS = 0 - команды

// Maximum timeout value
#define TIMEOUT_MAX              10000

#define DMA_STREAM               DMA2_Stream0
#define DMA_CHANNEL              DMA_Channel_0
#define DMA_STREAM_CLOCK         RCC_AHB1Periph_DMA2
#define DMA_STREAM_IRQ           DMA2_Stream0_IRQn

typedef struct
{
  volatile uint16_t LCD_REG;
  volatile uint16_t LCD_RAM;
} LCD_TypeDef;

// Note: LCD /CS is NE1 - Bank 1 of NOR/SRAM Bank 1~4
#define LCD_BASE           ((uint32_t)(0x60000000 | 0x0001fffE))
#define LCD                ((LCD_TypeDef *) LCD_BASE)

// Конфигурирование ног FSMC
static void FSMC_LinesConfig ( void );
// Настройка FSMC
static void FSMC_Config ( void );

// флаг завершения DMA передачи
static volatile uint8_t isDmaTransferOk;

// Инициализация FSMC
void FSMC_Init ( void )
{
	// Конфигурирование ног FSMC
	FSMC_LinesConfig ( );

	// Настройка FSMC
	FSMC_Config ( );

	isDmaTransferOk = false;
} // FSMC_Init

// Запись в регистр LCD
void FSMC_LcdWriteReg ( uint8_t LCD_Reg, uint16_t LCD_RegValue )
{
	// Write 16-bit Index, then Write Reg
	LCD->LCD_REG = LCD_Reg;
	// Write 16-bit Reg
	LCD->LCD_RAM = LCD_RegValue;
} // FSMC_LcdWriteReg

// Чтение регистра LCD
uint16_t FSMC_LcdReadReg ( uint8_t LCD_Reg )
{
	// Write 16-bit Index (then Read Reg)
	LCD->LCD_REG = LCD_Reg;
	// Read 16-bit Reg
	return (LCD->LCD_RAM);
} // FSMC_LcdReadReg

// Запись команды в LCD
void FSMC_LcdWriteCmd ( uint16_t val )
{
	// Write 16-bit Index (then Read Reg)
	LCD->LCD_REG = val;
} // FSMC_LcdWriteCmd

// чтение команды из LCD
uint16_t FSMC_LcdReadCmd ( void )
{
	return (LCD->LCD_REG);
} // FSMC_LcdWriteCmd

// чтение данных из LCD
uint16_t FSMC_LcdReadData ( void )
{
	return (LCD->LCD_RAM);
} // FSMC_LcdWriteCmd

// Запись данных в LCD
void FSMC_LcdWriteData ( uint16_t val )
{
	// Write 16-bit Reg
	LCD->LCD_RAM = val;
} // FSMC_LcdWriteData

void FSMC_TransferDataDMAToLCD ( void *buffer, uint32_t count, bool isRamAddressIncrement )
{
	uint16_t temper, temper1;
    u8 i;
	volatile uint32_t Timeout = TIMEOUT_MAX;
	uint32_t tmpreg;

	temper1=count/32768;

    for(i=0; i<=temper1;i++)
    {
    	temper=32786;

    	if(i==temper1)
    	{temper=count;}


    	isDmaTransferOk = false;

    	// Если длина не кратна 4 - не работаем
//    	if ( count % 4 != 0 )
//    		return;

    	// длина буфера в DWORD
    	temper /= 4;

    	// Enable DMA clock
    	RCC->AHB1ENR |= (uint32_t)0x00400000;

    	// Reset DMA Stream registers (for debug purpose)
    	DMA2_Stream0->CR &= ~((uint32_t)DMA_SxCR_EN);
    	DMA2_Stream0->CR = 0;
    	DMA2_Stream0->NDTR = 0;
    	DMA2_Stream0->PAR  = 0;
    	DMA2_Stream0->M0AR = 0;
    	DMA2_Stream0->M1AR = 0;
    	DMA2_Stream0->FCR = (uint32_t)0x00000021;
    	DMA2->LIFCR = (uint32_t)0x0000003D;

    	while ( ( DMA2_Stream0->CR & (uint32_t)DMA_SxCR_EN ) != 0 );

    	// Clear CHSEL, MBURST, PBURST, PL, MSIZE, PSIZE, MINC, PINC, CIRC and DIR bits
    	tmpreg = DMA2_Stream0->CR & ( (uint32_t)~(DMA_SxCR_CHSEL | DMA_SxCR_MBURST | DMA_SxCR_PBURST |
    			DMA_SxCR_PL | DMA_SxCR_MSIZE | DMA_SxCR_PSIZE | DMA_SxCR_MINC | DMA_SxCR_PINC | DMA_SxCR_CIRC |
    			DMA_SxCR_DIR) );

    	if ( isRamAddressIncrement )
    		DMA2_Stream0->CR = tmpreg | (uint32_t)0x00025280;
    	else
    		DMA2_Stream0->CR = tmpreg | (uint32_t)0x00025080;

    	tmpreg = DMA2_Stream0->FCR;
    	// Clear DMDIS and FTH bits
    	tmpreg &= (uint32_t)~(DMA_SxFCR_DMDIS | DMA_SxFCR_FTH);
    	DMA2_Stream0->FCR = tmpreg | (uint32_t)0x00000003;
    	DMA2_Stream0->NDTR = temper;
    	DMA2_Stream0->PAR = (uint32_t)buffer;
    	DMA2_Stream0->M0AR = (uint32_t)(&(LCD->LCD_RAM));

    	// Enable DMA Stream Transfer Complete interrupt
    	// DMA_ITConfig ( DMA2_Stream0, DMA_IT_TC, ENABLE );
    	DMA2_Stream0->FCR &= (uint32_t)0x00000080;
    	// Enable the selected DMA transfer interrupts
    	DMA2_Stream0->CR |= (uint32_t)0x00000010;

    	// DMA Stream enable
    	// DMA_Cmd ( DMA2_Stream0, ENABLE );
    	DMA2_Stream0->CR |= (uint32_t)0x00000001;

    	Timeout = TIMEOUT_MAX;
    	while ( ((DMA2_Stream0->CR & (uint32_t)0x00000001) == 0) && (Timeout-- > 0) );

    	// Check if a timeout condition occurred
    	if (Timeout == 0)
    	{ // Manage the error: to simplify the code enter an infinite loop
    		while (1);
    	} // if

    	NVIC_SetPriority ( DMA2_Stream0_IRQn, 18 );
    	NVIC_EnableIRQ ( DMA2_Stream0_IRQn );

    	// ждать конца передачи
    	while ( !isDmaTransferOk );
    } // for temper1
} // FSMC_TransferDataDMAToLCD
/*
void FSMC_TransferDataDMAToLCD ( void *buffer, uint32_t count, bool isRamAddressIncrement )
{
	uint16_t temper=65535, temper1;
    uint8_t i;
	volatile uint32_t Timeout = TIMEOUT_MAX;
	uint32_t tmpreg;

    temper1 = count/65536;
    for ( i = 0; i <= temper1; i ++ )
    {
    	temper = 65535;
    	if ( i == temper1 )
    		temper = count;

    	isDmaTransferOk = false;

    	// Если длина не кратна 4 - не работаем
    	if ( count % 4 != 0 )
    		return;

    	// длина буфера в DWORD
    	temper /= 4;

    	// Enable DMA clock
    	RCC->AHB1ENR |= (uint32_t)0x00400000;

    	// Reset DMA Stream registers (for debug purpose)
    	DMA2_Stream0->CR &= ~((uint32_t)DMA_SxCR_EN);
    	DMA2_Stream0->CR = 0;
    	DMA2_Stream0->NDTR = 0;
    	DMA2_Stream0->PAR  = 0;
    	DMA2_Stream0->M0AR = 0;
    	DMA2_Stream0->M1AR = 0;
    	DMA2_Stream0->FCR = (uint32_t)0x00000021;
    	DMA2->LIFCR = (uint32_t)0x0000003D;

    	while ( ( DMA2_Stream0->CR & (uint32_t)DMA_SxCR_EN ) != 0 );

    	// Clear CHSEL, MBURST, PBURST, PL, MSIZE, PSIZE, MINC, PINC, CIRC and DIR bits
    	tmpreg = DMA2_Stream0->CR & ( (uint32_t)~(DMA_SxCR_CHSEL | DMA_SxCR_MBURST | DMA_SxCR_PBURST |
    			DMA_SxCR_PL | DMA_SxCR_MSIZE | DMA_SxCR_PSIZE | DMA_SxCR_MINC | DMA_SxCR_PINC | DMA_SxCR_CIRC |
    			DMA_SxCR_DIR) );

    	if ( isRamAddressIncrement )
    		DMA2_Stream0->CR = tmpreg | (uint32_t)0x00025280;
    	else
    		DMA2_Stream0->CR = tmpreg | (uint32_t)0x00025080;

    	tmpreg = DMA2_Stream0->FCR;
    	// Clear DMDIS and FTH bits
    	tmpreg &= (uint32_t)~(DMA_SxFCR_DMDIS | DMA_SxFCR_FTH);
    	DMA2_Stream0->FCR = tmpreg | (uint32_t)0x00000003;
    	DMA2_Stream0->NDTR = temper;
    	DMA2_Stream0->PAR = (uint32_t)buffer;
    	DMA2_Stream0->M0AR = (uint32_t)(&(LCD->LCD_RAM));

    	// Enable DMA Stream Transfer Complete interrupt
    	// DMA_ITConfig ( DMA2_Stream0, DMA_IT_TC, ENABLE );
    	DMA2_Stream0->FCR &= (uint32_t)0x00000080;
    	// Enable the selected DMA transfer interrupts
    	DMA2_Stream0->CR |= (uint32_t)0x00000010;

    	// DMA Stream enable
    	// DMA_Cmd ( DMA2_Stream0, ENABLE );
    	DMA2_Stream0->CR |= (uint32_t)0x00000001;

    	Timeout = TIMEOUT_MAX;
    	while ( ((DMA2_Stream0->CR & (uint32_t)0x00000001) == 0) && (Timeout-- > 0) );

    	// Check if a timeout condition occurred
    	if (Timeout == 0)
    	{ // Manage the error: to simplify the code enter an infinite loop
    		while (1);
    	} // if

    	NVIC_SetPriority ( DMA2_Stream0_IRQn, 18 );
    	NVIC_EnableIRQ ( DMA2_Stream0_IRQn );
    	// ждать конца передачи
    	while ( !isDmaTransferOk );
    } //for temper1
} // FSMC_TransferDataDMAToLCD
*/

/*
// Передача блока данных по DMA в LCD
// count - длина данных в байтах, д.б. кратна 4
void FSMC_TransferDataDMAToLCD ( void *buffer, uint32_t count, bool isRamAddressIncrement )
{
	volatile uint32_t Timeout = TIMEOUT_MAX;
	uint32_t tmpreg;

	isDmaTransferOk = false;

	// Если длина не кратна 4 - не работаем
	if ( count % 4 != 0 )
		return;

	// длина буфера в DWORD
	count /= 4;

	// Enable DMA clock
    RCC->AHB1ENR |= (uint32_t)0x00400000;

	// Reset DMA Stream registers (for debug purpose)
    DMA2_Stream0->CR &= ~((uint32_t)DMA_SxCR_EN);
    DMA2_Stream0->CR = 0;
    DMA2_Stream0->NDTR = 0;
    DMA2_Stream0->PAR  = 0;
    DMA2_Stream0->M0AR = 0;
    DMA2_Stream0->M1AR = 0;
    DMA2_Stream0->FCR = (uint32_t)0x00000021;
    DMA2->LIFCR = (uint32_t)0x0000003D;

    while ( ( DMA2_Stream0->CR & (uint32_t)DMA_SxCR_EN ) != 0 );

	// Configure DMA Stream (Memory -> Periph)
	// Clear CHSEL, MBURST, PBURST, PL, MSIZE, PSIZE, MINC, PINC, CIRC and DIR bits
	tmpreg = DMA2_Stream0->CR & ( (uint32_t)~(DMA_SxCR_CHSEL | DMA_SxCR_MBURST | DMA_SxCR_PBURST |
			DMA_SxCR_PL | DMA_SxCR_MSIZE | DMA_SxCR_PSIZE | DMA_SxCR_MINC | DMA_SxCR_PINC | DMA_SxCR_CIRC |
			DMA_SxCR_DIR) );

	if ( isRamAddressIncrement )
		DMA2_Stream0->CR = tmpreg | (uint32_t)0x00025280;
	else
		DMA2_Stream0->CR = tmpreg | (uint32_t)0x00025080;

	tmpreg = DMA2_Stream0->FCR;
	// Clear DMDIS and FTH bits
	tmpreg &= (uint32_t)~(DMA_SxFCR_DMDIS | DMA_SxFCR_FTH);
	DMA2_Stream0->FCR = tmpreg | (uint32_t)0x00000003;
	DMA2_Stream0->NDTR = count;
	DMA2_Stream0->PAR = (uint32_t)buffer;
	DMA2_Stream0->M0AR = (uint32_t)(&(LCD->LCD_RAM));

	// Enable DMA Stream Transfer Complete interrupt
//	DMA_ITConfig ( DMA2_Stream0, DMA_IT_TC, ENABLE );
	DMA2_Stream0->FCR &= (uint32_t)0x00000080;
    // Enable the selected DMA transfer interrupts
	DMA2_Stream0->CR |= (uint32_t)0x00000010;

	// DMA Stream enable
//	DMA_Cmd ( DMA2_Stream0, ENABLE );
	DMA2_Stream0->CR |= (uint32_t)0x00000001;

	Timeout = TIMEOUT_MAX;
	while ( ((DMA2_Stream0->CR & (uint32_t)0x00000001) == 0) && (Timeout-- > 0) );

	// Check if a timeout condition occurred
	if (Timeout == 0)
	{ // Manage the error: to simplify the code enter an infinite loop
	    while (1);
	} // if

	NVIC_SetPriority ( DMA2_Stream0_IRQn, 18 );
	NVIC_EnableIRQ ( DMA2_Stream0_IRQn );

	// ждать конца передачи
	while ( !isDmaTransferOk );
} // FSMC_TransferDataDMAToLCD
*/

// Передача блока данных по DMA из LCD
// count - длина данных в байтах, д.б. кратна 4
void FSMC_TransferDataDMAFromLCD ( void *buffer, uint32_t count )
{
	volatile uint32_t Timeout = TIMEOUT_MAX;
//	uint32_t tmpreg;
	DMA_InitTypeDef  DMA_InitStructure;

	isDmaTransferOk = false;

	// Если длина не кратна 4 - не работаем
	if ( count % 4 != 0 )
		return;

	// длина буфера в DWORD
	count /= 4;

	// Enable DMA clock
    RCC->AHB1ENR |= (uint32_t)0x00400000;

	// Reset DMA Stream registers (for debug purpose)
    DMA2_Stream0->CR &= ~((uint32_t)DMA_SxCR_EN);
    DMA2_Stream0->CR = 0;
    DMA2_Stream0->NDTR = 0;
    DMA2_Stream0->PAR  = 0;
    DMA2_Stream0->M0AR = 0;
    DMA2_Stream0->M1AR = 0;
    DMA2_Stream0->FCR = (uint32_t)0x00000021;
    DMA2->LIFCR = (uint32_t)0x0000003D;

    while ( ( DMA2_Stream0->CR & (uint32_t)DMA_SxCR_EN ) != 0 );

	// Configure DMA Stream (Periph -> Memory)
	DMA_InitStructure.DMA_Channel = DMA_CHANNEL;
	DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)(&(LCD->LCD_RAM));	// откуда
	DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)buffer;				// куда
	DMA_InitStructure.DMA_DIR = DMA_DIR_MemoryToMemory;
	DMA_InitStructure.DMA_BufferSize = count;
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
	DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Word;
	DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Word;
	DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
	DMA_InitStructure.DMA_Priority = DMA_Priority_High;
	DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Disable;
	DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_Full;
	DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;
	DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
	DMA_Init ( DMA_STREAM, &DMA_InitStructure );

/*
//////////////////
	// Clear CHSEL, MBURST, PBURST, PL, MSIZE, PSIZE, MINC, PINC, CIRC and DIR bits
	tmpreg = DMA2_Stream0->CR & ( (uint32_t)~(DMA_SxCR_CHSEL | DMA_SxCR_MBURST | DMA_SxCR_PBURST |
			DMA_SxCR_PL | DMA_SxCR_MSIZE | DMA_SxCR_PSIZE | DMA_SxCR_MINC | DMA_SxCR_PINC | DMA_SxCR_CIRC |
			DMA_SxCR_DIR) );

	if ( isRamAddressIncrement )
		DMA2_Stream0->CR = tmpreg | (uint32_t)0x00025280;
	else
		DMA2_Stream0->CR = tmpreg | (uint32_t)0x00025080;

	tmpreg = DMA2_Stream0->FCR;
	// Clear DMDIS and FTH bits
	tmpreg &= (uint32_t)~(DMA_SxFCR_DMDIS | DMA_SxFCR_FTH);
	DMA2_Stream0->FCR = tmpreg | (uint32_t)0x00000003;
	DMA2_Stream0->NDTR = count;
	DMA2_Stream0->PAR = (uint32_t)buffer;
	DMA2_Stream0->M0AR = (uint32_t)(&(LCD->LCD_RAM));
////////////////
*/

	// Enable DMA Stream Transfer Complete interrupt
//	DMA_ITConfig ( DMA2_Stream0, DMA_IT_TC, ENABLE );
	DMA2_Stream0->FCR &= (uint32_t)0x00000080;
    // Enable the selected DMA transfer interrupts
	DMA2_Stream0->CR |= (uint32_t)0x00000010;

	// DMA Stream enable
//	DMA_Cmd ( DMA2_Stream0, ENABLE );
	DMA2_Stream0->CR |= (uint32_t)0x00000001;

	Timeout = TIMEOUT_MAX;
	while ( ((DMA2_Stream0->CR & (uint32_t)0x00000001) == 0) && (Timeout-- > 0) );

	// Check if a timeout condition occurred
	if (Timeout == 0)
	{ // Manage the error: to simplify the code enter an infinite loop
	    while (1);
	} // if

	NVIC_SetPriority ( DMA2_Stream0_IRQn, 18 );
	NVIC_EnableIRQ ( DMA2_Stream0_IRQn );

	// ждать конца передачи
	while ( !isDmaTransferOk );
} // FSMC_TransferDataDMAFromLCD

/////////////////////////////////////////////////////////////
// Внутренние функции

// Прерывание по завершению передачи
void DMA2_Stream0_IRQHandler ( void )
{
//		#define DMA_IT_TCIF0   (uint32_t)0x10008020
		#define RESERVED_MASK  (uint32_t)0x0F7D0F7D
		#define TRANSFER_IT_ENABLE_MASK (uint32_t)0x0000001E

		// Test on DMA Stream Transfer Complete interrupt
		uint32_t tmpreg = 0, enablestatus = 0;

	    // Get the interrupt enable position mask in CR register
	    tmpreg = (uint32_t)((DMA_IT_TCIF0 >> 11) & TRANSFER_IT_ENABLE_MASK);

	    // Check the enable bit in CR register
	    enablestatus = (uint32_t)(DMA2_Stream0->CR & tmpreg);

	    tmpreg = DMA2->LISR & (uint32_t)RESERVED_MASK;
	    /* Check the status of the specified DMA interrupt */
	    if ( ((tmpreg & DMA_IT_TCIF0) != (uint32_t)RESET) && (enablestatus != (uint32_t)RESET) )
	    {
		    DMA2->LIFCR = (uint32_t)(DMA_IT_TCIF0 & RESERVED_MASK);

			isDmaTransferOk = true;
	    } // if
} // DMA2_Stream0_IRQHandler

// Конфигурирование ног FSMC
static void FSMC_LinesConfig ( void )
{	// Enable GPIOD, GPIOE clocks
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN | RCC_AHB1ENR_GPIOEEN;

	// -- GPIO Configuration ------------------------------------------------------
	// LCD DB15-DB0       PD10..PD8, PE15..PE7, PD1, PD0, PD15, PD14
	// LCD nCS  (7)       NCE2 (PD7)
	// LCD nRD  (10)      NOE  (PD4)
	// LCD nWR  (9)       NWE  (PD5)
	// LCD RS   (8)       CLE  (PD11)
	// SRAM Data lines,  NOE and NWE configuration
	                            //15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00
	GPIOD->MODER |= 0xA0AA8A0A; //10 10 00 00 10 10 10 10 10 00 10 10 00 00 10 10
	GPIOD->OTYPER |= 0;
	GPIOD->PUPDR |= 0;
	                             //  7     6    5    4    3    2    1    0
	GPIOD->AFR[0] |= 0xC0CC00CC; // 1100 0000 1100 1100 0000 0000 1100 1100
	GPIOD->AFR[1] |= 0xCC00CCCC; // 1100 1100 0000 0000 1100 1100 1100 1100
	GPIOD->OSPEEDR |= 0xA0AA8A0A; // 50MHz

                                //15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00
    GPIOE->MODER |= 0xAAAA8000; //10 10 10 10 10 10 10 10 10 00 00 00 00 00 00 00
    GPIOE->OTYPER |= 0;
    GPIOE->PUPDR |= 0;
                                 //  7     6    5    4    3    2    1    0
    GPIOE->AFR[0] |= 0xC0000000; // 1100 0000 0000 0000 0000 0000 0000 0000
    GPIOE->AFR[1] |= 0xCCCCCCCC; // 1100 1100 1100 1100 1100 1100 1100 1100
    GPIOE->OSPEEDR |= 0xAAAA8000; // 50MHz

    SysTick_Config ( 168000); //конфигурация системного таймера - вызывать прерывание каждые тактовая частота делённая на 168000. Это 1мС.
} // FSMC_LinesConfig

// Настройка FSMC
static void FSMC_Config ( void )
{
	// Enable FSMC clock
	RCC->AHB3ENR = RCC_AHB3ENR_FSMCEN;

	// -- FSMC Configuration SRAM Bank 3----------------------------------------
	// FSMC_Bank1_NORSRAM4 configuration

    FSMC_Bank1->BTCR [ 0 ] =
                (uint32_t)(0x00001010);

    // Bank1 NOR/SRAM timing register configuration
    FSMC_Bank1->BTCR [ 0 + 1 ] =(uint32_t)( 10 | (1 << 4) | (15 << 8) );
	//FSMC_AddressSetupTime = 10;
	//FSMC_AddressHoldTime = 1;
	//FSMC_DataSetupTime = 15;
   FSMC_Bank1E->BWTR [ 0 ] = 0x0FFFFFFF;

    // Enable FSMC Bank1_SRAM Bank
   FSMC_Bank1->BTCR [ 0 ] |= 0x00000001;


} // FSMC_Config

