// SSD1963_Drv.c - Драйвер дисплея SSD1963 (FSMC)

#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>		// abs
//#include "delay.h"
#include "System.h"

#include "FSMC_LowLevel.h"
#include "SSD1963.h"

#define PIXEL_COUNT (DISP_WIDTH+1)*DISP_HEIGHT

#define HDP		(DISP_WIDTH - 1)
#define HT 		928
#define HPS		46
#define LPS		15
#define HPW		48

#define VDP		(DISP_HEIGHT - 1)
#define VT		525
#define VPS		16
#define FPS		8
#define VPW		16

// Flip:
// Bit 1 - Hor: 0 - normal, 1 - flip
// Bit 0 - Vert: 0 - normal, 1 - flip
//static uint8_t flipState = 0x03; // мой 0А Паши 03
static uint8_t flipState = 0x00; // мой 0А Паши 03

// Инициализация дисплея
void SSD1963_Init ( void )
{
     // Инициализация FSMC
     FSMC_Init ( );

     // Soft reset
     FSMC_LcdWriteCmd ( 0x01 );     // software reset
     FSMC_LcdWriteCmd ( 0x01 );     // software reset
     FSMC_LcdWriteCmd ( 0x01 );     // software reset
     delay_ms ( 120 ); //задержка
//     delay ( 120 * 1000 ); //задержка

     FSMC_LcdWriteCmd ( 0xE2 );                         // PLL multiplier, set PLL clock to 120M
     FSMC_LcdWriteData ( 0x23 );  //множитель PLL(M)    // N=0x36 for 6.5M, 0x23 for 10M crystal
     FSMC_LcdWriteData ( 0x02 );  //делитель PLL(N)
     FSMC_LcdWriteData ( 0x04 );  //команда на использование умножителя и делителя PLL
     FSMC_LcdWriteCmd ( 0xE0 );  //запуск PLL
     FSMC_LcdWriteData ( 0x01 ); // PLL включена и использовать внутренний генератор как системный
     delay_ms(1); //ожидание запуска PLL
//     delay ( 1000 ); //ожидание запуска PLL
     FSMC_LcdWriteCmd ( 0xE0 );  //Переключение системы с внутреннего генератора на PLL
     FSMC_LcdWriteData ( 0x03 );
     FSMC_LcdWriteCmd ( 0x01 );   // software reset
     delay_ms(120);// а здесь лучше сделать 120 вместо 5
//     delay ( 120 * 1000 ); //задержка

     FSMC_LcdWriteCmd ( 0xE6 );     //Установка частоты переключения пикселя
                                    // PLL setting for PCLK, depends on resolution
     FSMC_LcdWriteData ( 0x04 );    //5,3мГц = PLL freq x ((частота переключения + 1) / 2^20)
     FSMC_LcdWriteData ( 0xFF );    //для 100мГц должно быть 0х00D916
     FSMC_LcdWriteData ( 0xFF );

     FSMC_LcdWriteCmd ( 0xB0 );                         // Set LCD mode
     FSMC_LcdWriteData ( 0x27 ); //10 - мой 18bit 27 - Паши 24bit
     FSMC_LcdWriteData ( 0x00 ); // 0x00 00 и 20 - TFT режим, 40 serial RGB режим
     FSMC_LcdWriteData ( (HDP>>8) & 0xFF );  //Разрешение по горизонтали        // Set HDP
     FSMC_LcdWriteData ( HDP & 0xFF );
     FSMC_LcdWriteData ( (VDP>>8) & 0xFF );   //Разрешение по вертикали       // Set VDP
     FSMC_LcdWriteData ( VDP & 0xFF );
//     FSMC_LcdWriteData ( 0x00 ); //а здесь попробуй поставить 0х2D и тогда не надо мучать 36 регистр
     FSMC_LcdWriteData ( 0x00 ); //а здесь попробуй поставить 0х2D и тогда не надо мучать 36 регистр
     	 	 	 	 	 	 	 // 0010 1101 - G[5..3]= 101
     	 	 	 	 	 	 	 //				G[2..0] = 101 - BGR

     //В4 и В6 частота обновления по горизонтали и вертикали (это вкратце)
     FSMC_LcdWriteCmd ( 0xB4 );                    // Set horizontal period
     FSMC_LcdWriteData ( (HT>>8)& 0xFF );          // Set HT
     FSMC_LcdWriteData ( HT & 0xFF );
     FSMC_LcdWriteData ( (HPS >> 8) & 0XFF );      // Set HPS
     FSMC_LcdWriteData ( HPS & 0xFF );
     FSMC_LcdWriteData ( HPW );                    // Set HPW
     FSMC_LcdWriteData ( (LPS>>8) & 0XFF );        // Set HPS
     FSMC_LcdWriteData ( LPS & 0XFF );
     FSMC_LcdWriteData ( 0x00 );

	FSMC_LcdWriteCmd ( 0xB6 );                    // Set vertical period
	FSMC_LcdWriteData ( (VT>>8) & 0xFF );         // Set VT
	FSMC_LcdWriteData ( VT & 0xFF );
	FSMC_LcdWriteData ( (VPS>>8) & 0xFF );        // Set VPS
	FSMC_LcdWriteData ( VPS & 0xFF );
	FSMC_LcdWriteData ( VPW );                    // Set VPW
	FSMC_LcdWriteData ( (FPS>>8) & 0xFF );        // Set FPS
	FSMC_LcdWriteData ( FPS & 0xFF );

 	// ВА и В8 конфигурация портов между SSD1963 и дисплеем
     FSMC_LcdWriteCmd ( 0xBA );
     FSMC_LcdWriteData ( 0x0F );                         // GPIO[3:0] out 1
     FSMC_LcdWriteCmd ( 0xB8 );
     FSMC_LcdWriteData ( 0x07 );                         // 0x07 GPIO3=input, GPIO[2:0]=output
     FSMC_LcdWriteData ( 0x01 );                         // 0x01 GPIO0 normal

     FSMC_LcdWriteCmd ( 0x36 );                         // Set Address mode - rotation
     FSMC_LcdWriteData ( flipState );  // бит 0 - flip vertical бит 1 - flip horizontal

     FSMC_LcdWriteCmd ( 0xBC );                         // настройка яркости и тд
     FSMC_LcdWriteData ( 0x40 );                         // контраст
     FSMC_LcdWriteData ( 0x90 );                         // яркость
     FSMC_LcdWriteData ( 0x40 );                         // насыщеность цвета
     FSMC_LcdWriteData ( 0x01 );                         // пост процессор(1- вкл параметры, 0- выкл)

     FSMC_LcdWriteCmd ( 0xF0 );                         // pixel data interface
     FSMC_LcdWriteData ( 0x03 );                         // 03h - RGB565

     delay_ms ( 5 );
//     delay ( 5000 );

     FSMC_LcdWriteCmd ( 0x29 );                         // display on

     FSMC_LcdWriteCmd ( 0xd0 );  //отключено ручное управления яркостью      // Dynamic Bright Control
     FSMC_LcdWriteData ( 0x0d ); //режим энергосбережения - агрессивный

     // Заливка экрана чёрным
     SSD1963_FillScr ( 0x0000 );
} // InitLCD

// Получить максимальный X
uint16_t SSD1963_GetMaxX ( void )
{
	return DISP_WIDTH - 1;
} // GetMaxX

// Получить максимальный Y
uint16_t SSD1963_GetMaxY ( void )
{
	return DISP_HEIGHT - 1;
} // GetMaxY

// Заливка экрана указанным цветом
void SSD1963_FillScr ( uint16_t color )
{
//	SSD1963_FillRect ( 0, 0, DISP_WIDTH - 1, DISP_HEIGHT - 1, color );
	SSD1963_FillRect ( 0, 0, DISP_WIDTH - 1, 149, color );
	SSD1963_FillRect ( 0, 149, DISP_WIDTH - 1, 299, color );
	SSD1963_FillRect ( 0, 300, DISP_WIDTH - 1, 449, color );
	SSD1963_FillRect ( 0, 450, DISP_WIDTH - 1, 479, color );
} // SSD1963_FillScr

// Заливка прямоугольника указанным цветом
void SSD1963_FillRect ( uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color )
{
	int i,size;
    size = (x2-x1+1) * (y2-y1+1);

    SSD1963_SetDisplayWindow ( x1, y1, x2, y2, SSD1963_Hor_IncrIncr );
    FSMC_LcdWriteCmd ( 0x2C );          // SSD1963_WRITE_MEMORY_START

    for ( i = 0; i < size; i ++ )
    	FSMC_LcdWriteData ( color );
} // SSD1963_FillRect

// Заливка прямоугольника указанным цветом
void SSD1963_FillRect_fast ( uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color )
{
	uint32_t index = 0, size;

	// Задание окна
	SSD1963_SetDisplayWindow ( x1, y1, x2, y2, SSD1963_Hor_IncrIncr );

	// Prepare to write GRAM
	FSMC_LcdWriteCmd ( 0x2C );   // SSD1963_WRITE_MEMORY_START
	size = (x2-x1+1) * (y2-y1+1);
	index = color|(color<<16);
	FSMC_TransferDataDMAToLCD ( &index, size*2, false );
} // SSD1963_FillRect

// Рисование линии
void SSD1963_drawLine ( int x1, int y1, int x2, int y2, uint16_t color)
{
  int deltaX, deltaY, signX, signY, error, error2;
  int dX, dY, size;
//  int x, y, size;
  uint16_t index;

  if (x1 == x2 || y1 == y2)
  {
	  if (y1>y2){dY=y2; y2=y1; y1=dY;}
	  if (x1>x2){dX=x2; x2=x1; x1=dX;}
	  SSD1963_SetDisplayWindow ( x1, y1, x2, y2, SSD1963_Hor_IncrIncr );
	  FSMC_LcdWriteCmd ( 0x2C );   // SSD1963_WRITE_MEMORY_START
	  size = ((x2-x1+1) * (y2-y1+1));

	  index = color|(color<<16);
	  FSMC_TransferDataDMAToLCD ( &index, size*2, false );
	  return;
  } // if

/*
  if ( x1 == x2 )
  { // Вертикальная линия
    if ( y2 > y1 )
    {
      for ( y = y1; y < y2; y ++ )
    	  SSD1963_PutPixel ( x1, y, color );
    } // if
    else
    { // y1 больше y2
      for ( y = y2; y < y1; y ++ )
    	  SSD1963_PutPixel ( x1, y, color );
    } // if

    return;
  } // if

  if ( y1 == y2 )
  { // Гoризонтальная линия
    if ( x2 > x1 )
    {
      for ( x = x1; x < x2; x ++ )
    	  SSD1963_PutPixel ( x, y1, color );
    } // if
    else
    {
      for ( x = x2; x < x1; x ++ )
    	  SSD1963_PutPixel ( x, y1, color );
    } // else

    return;
  } // if
*/

  dX = x2 - x1;
  dY = y2 - y1;
  deltaX = abs(dX);
  deltaY = abs(dY);
  signX = (x1 < x2) ? 1 : -1;
  signY = (y1 < y2) ? 1 : -1;
  error = deltaX - deltaY;

  SSD1963_PutPixel ( x2, y2, color );
  while ( x1 != x2 || y1 != y2 )
  {
	  SSD1963_PutPixel ( x1, y1, color );
	  error2 = error * 2;

    if ( error2 > -deltaY )
    {
      error -= deltaY;
      x1 += signX;
    } // if

    if ( error2 < deltaX )
    {
      error += deltaX;
      y1 += signY;
    } // if
  } // while
} // SSD1963_drawLine

// Рисование прямоугольника
void SSD1963_DrawRect ( int x1, int y1, int x2, int y2, uint16_t color )
{
	// Верхняя горизонталь
	SSD1963_drawLine ( x1, y1, x2, y1, color );
	// Правая вертикаль
	SSD1963_drawLine ( x2, y1, x2, y2, color );
	// Нижняя горизонталь
	SSD1963_drawLine ( x1, y2, x2, y2, color );
	// Левая вертикаль
	SSD1963_drawLine ( x1, y1, x1, y2, color );
} // SSD1963_DrawRect

void SSD1963_SetCursor ( uint16_t Xpos, uint16_t Ypos )
{
	SSD1963_SetDisplayWindow ( Xpos, Ypos, Xpos, Ypos, SSD1963_Hor_DecrDecr );
} // SSD1963_SetCursor

// Поставить точку указанного цвета
void SSD1963_PutPixel ( uint16_t x, uint16_t y, uint16_t color )
{
	uint16_t maxX, maxY;

	maxX = SSD1963_GetMaxX ( );
	maxY = SSD1963_GetMaxY ( );

	if ( x > maxX || y > maxY )
		return;

	SSD1963_SetCursor ( x, y );

	FSMC_LcdWriteCmd ( 0x2C );		// SSD1963_WRITE_MEMORY_START
	FSMC_LcdWriteData ( color );
} // SSD1963_PutPixel

// Считать точку
uint16_t SSD1963_GetPixel ( uint16_t x, uint16_t y )
{
	uint16_t maxX, maxY;

	maxX = SSD1963_GetMaxX ( );
	maxY = SSD1963_GetMaxY ( );

	if ( x > maxX || y > maxY )
		return 0;

	SSD1963_SetCursor ( x, y );

	// или тут надо так: FSMC_LcdWriteCmd (0x2E); return (FSMC_LcdReadReg ( 0xFF ));
	return (FSMC_LcdReadReg ( 0x2E ));
} // SSD1963_GetPixel

void SSD1963_SetDisplayWindow ( uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, TSSD1963Rotate rotateId )
{
	uint16_t hi, lo;
	uint8_t val, conf0x36;

	// set_column_address
	FSMC_LcdWriteCmd ( 0x2A );							// SSD1963_SET_COLUMN_ADDRESS
	hi = x1 >> 8;
	lo = x1 & 0x00ff;
	FSMC_LcdWriteData ( hi );					// Hi
	FSMC_LcdWriteData ( lo );					// Low
	hi = x2 >> 8;
	lo = x2 & 0x00ff;
	FSMC_LcdWriteData ( hi );					// Hi
	FSMC_LcdWriteData ( lo );					// Low

	// set_page_addres
	FSMC_LcdWriteCmd ( 0x2B );							// SSD1963_SET_PAGE_ADDRESS
	hi = y1 >> 8;
	lo = y1 & 0x00ff;
	FSMC_LcdWriteData ( hi );					// Hi
	FSMC_LcdWriteData ( lo );					// Low
	hi = y2 >> 8;
	lo = y2 & 0x00ff;
	FSMC_LcdWriteData ( hi );					// Hi
	FSMC_LcdWriteData ( lo );					// Low

	// Задать разворот
	// 0x36 = 3 - A0=1 - Flip Vertical on; A1=1 - Flip Horizontal on
/*
	A[7] : Page address order (POR = 0)
	A[6] : Column address order (POR = 0)
	A[5] : Page / Column order (POR = 0)
	A[4] : Line address order (POR = 0)
	A[3] : RGB / BGR order (POR = 0)
	A[2] : Display data latch data (POR = 0)
*/
	conf0x36 = FSMC_LcdReadReg ( 0x0B );
	conf0x36 &= 0x1F;
	conf0x36 |= flipState;	// initial flip state

	val = conf0x36 | (uint8_t)rotateId;

/*
	// настройка поворота содержимого окна
	// бит 7 == AM, биты 6,5 == ID[1..0]
	if ( isVertical )
		val |= 0x80;

	switch ( rotate )
	{
		case 0:		//
		  // ID[6,5] = 00 Hor:Decrement, Vert:Decrement
		  break;

		case 1:		//
		  // ID[6,5] = 01 Hor:Increment, Vert:Decrement
			val |= 0x20;
		  break;

		case 2:		//
		  // ID[6,5] = 10 Hor:Decrement, Vert:Increment
			val |= 0x40;
		  break;

		case 3:		//
		  // ID[6,5] = 11 Hor:Increment, Vert:increment
			val |= 0x60;
		  break;

		default:
		  break;
	} // swich
*/
	// задать разворот
    FSMC_LcdWriteCmd ( 0x36 );                         // Set Address mode - rotation
    FSMC_LcdWriteData ( val );

} // SSD1963_SetDisplayWindow

// вывод BMP
void SSD1963_WriteBMP ( uint16_t x1, uint16_t y1, uint16_t Width, uint8_t Height, void *bitmap,
		TSSD1963Rotate rotateId, bool viaDma )
{
	uint32_t index, size;
	uint16_t *bitmap_ptr = (uint16_t *)bitmap;
	uint16_t x2, y2;

	x2 = x1 + Width - 1;
	y2 = y1 + Height - 1;

	// задать окно
	SSD1963_SetDisplayWindow ( x1, y1, x2, y2, rotateId );

  	// write_memory_start
	FSMC_LcdWriteCmd ( 0x2C );		// SSD1963_WRITE_MEMORY_START

	if ( viaDma )
	{
		// размер в байтах
		size = Height * Width * 2;

	    // Передача блока данных по DMA в LCD
	    // count - длина данных в байтах, д.б. кратна 4
	    FSMC_TransferDataDMAToLCD ( bitmap, size, true );
	} // if
	else
	{
		size = Height * Width - 1;
		for ( index = 0; index < size; index ++ )
			FSMC_LcdWriteData ( bitmap_ptr [ index ] );
	} // else

	// Сброс окна
	SSD1963_SetDisplayWindow ( 0, 0, SSD1963_GetMaxX ( ), SSD1963_GetMaxY ( ), SSD1963_Hor_DecrDecr );
} // SSD1963_WriteBMP

void SSD1963_setup (uint8_t contrast, uint8_t bright, uint8_t color)
{
    FSMC_LcdWriteCmd ( 0xBC );                         // настройка яркости и тд
    FSMC_LcdWriteData ( contrast );                         // контраст
    FSMC_LcdWriteData ( bright );                         // яркость
    FSMC_LcdWriteData ( color );                         // насыщеность цвета
    FSMC_LcdWriteData ( 0x01 );                         // пост процессор(1- вкл параметры, 0- выкл)
} // SSD1963_setup

void SSD1963_sleep (uint8_t sleep)
{
	if (sleep) {FSMC_LcdWriteCmd ( 0x10 );}  //Установка PWM конфигурации
       else {FSMC_LcdWriteCmd ( 0x11 );}

	delay_ms(100);
//	delay ( 100 * 1000 );
} // SSD1963_sleep

