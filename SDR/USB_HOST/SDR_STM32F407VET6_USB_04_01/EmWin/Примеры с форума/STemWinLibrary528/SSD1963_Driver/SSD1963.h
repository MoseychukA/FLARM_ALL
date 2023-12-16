// SSD1963_Drv.h - Драйвер дисплея SSD1963 (FSMC)

// Размеры дисплея (без учёта разворотов)
#define DISP_WIDTH  800
#define DISP_HEIGHT 480

// Аппаратные повороты дисплея
// бит 7 == AM, биты 6,5 == ID[1..0]
typedef enum
{
	SSD1963_Hor_DecrDecr = 0x0000,		// ID=00, AM=0 - 0000
	SSD1963_Hor_IncrDecr = 0x0020,		// ID=01, AM=0 - 0010
	SSD1963_Hor_DecrIncr = 0x0040,		// ID=10, AM=0 - 0100
	SSD1963_Hor_IncrIncr = 0x0070,		// ID=11, AM=0 - 0110
	SSD1963_Vert_DecrDecr = 0x0080,		// ID=00, AM=1 - 1000
	SSD1963_Vert_IncrDecr = 0x00A0,		// ID=01, AM=1 - 1010
	SSD1963_Vert_DecrIncr = 0x00C0,		// ID=10, AM=1 - 1100
	SSD1963_Vert_IncrIncr = 0x00E0 		// ID=11, AM=1 - 1110
} TSSD1963Rotate;

// Инициализация дисплея
void SSD1963_Init ( void );

// Получить максимальный X
uint16_t SSD1963_GetMaxX ( void );

// Получить максимальный Y
uint16_t SSD1963_GetMaxY ( void );

void SSD1963_SetCursor ( uint16_t Xpos, uint16_t Ypos );

// Задание окна x1:y1 -координаты левого верхнего угла, x2:y2 -координаты правого нижнего угла
void SSD1963_SetDisplayWindow ( uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, TSSD1963Rotate rotateId );

// вывод BMP
void SSD1963_WriteBMP ( uint16_t x1, uint16_t y1, uint16_t Width, uint8_t Height, void *bitmap,
		TSSD1963Rotate rotateId, bool viaDma );

// Заливка экрана указанным цветом
void SSD1963_FillScr ( uint16_t color );

// Заливка прямоугольника указанным цветом
void SSD1963_FillRect ( uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color );

// Заливка прямоугольника указанным цветом
void SSD1963_FillRect_fast ( uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color );

// Поставить точку указанного цвета
void SSD1963_PutPixel ( uint16_t x, uint16_t y, uint16_t color );

// Считать точку
uint16_t SSD1963_GetPixel ( uint16_t x, uint16_t y );

// Рисование линии
void SSD1963_drawLine ( int x1, int y1, int x2, int y2, uint16_t color );

// Рисование прямоугольника
void SSD1963_DrawRect ( int x1, int y1, int x2, int y2, uint16_t color );

// Установка яркости дисплея
void SSD1963_setup (uint8_t contrast, uint8_t bright, uint8_t color);

// Сон если "1" и проснутся если "0"
void SSD1963_sleep (uint8_t sleep);
