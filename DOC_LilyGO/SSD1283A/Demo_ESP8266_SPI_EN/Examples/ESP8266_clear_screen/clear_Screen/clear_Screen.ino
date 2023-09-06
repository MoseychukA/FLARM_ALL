#include <SPI.h>

#define X_MAX_PIXEL 130
#define Y_MAX_PIXEL 130

#define LCD_CS  D1
#define LCD_RST D2
#define LCD_RS  D3

#define LCD_CS_SET (digitalWrite(LCD_CS,HIGH))
#define LCD_RST_SET (digitalWrite(LCD_RST,HIGH))
#define LCD_RS_SET (digitalWrite(LCD_RS,HIGH))

#define LCD_CS_CLR (digitalWrite(LCD_CS,LOW))
#define LCD_RST_CLR (digitalWrite(LCD_RST,LOW))
#define LCD_RS_CLR (digitalWrite(LCD_RS,LOW))

#define BLACK 0x0000
#define WHITE 0xFFFF
#define RED 0xF800
#define GREEN 0x07E0
#define BLUE  0x001F

void spi_init(void)
{
  SPI.begin();
  SPI.setClockDivider(SPI_CLOCK_DIV4); // 4 MHz (half speed)
  SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(SPI_MODE0); 
}

void spi_write(unsigned char data)
{
  SPI.transfer(data);
}

void Lcd_WriteIndex(unsigned char index)
{
   LCD_CS_CLR;
   LCD_RS_CLR;
   spi_write(index);
   LCD_CS_SET;
}

void Lcd_WriteData(unsigned char data)
{
   LCD_CS_CLR;
   LCD_RS_SET;
   spi_write(data);
   LCD_CS_SET; 
}

void Lcd_SetRegion(unsigned int x_start,unsigned int y_start,unsigned int x_end,unsigned int y_end)
{    
  Lcd_WriteIndex(0x44);
  Lcd_WriteData(x_end+2);
  Lcd_WriteData(x_start+2);
  
  Lcd_WriteIndex(0x45);
  Lcd_WriteData(y_end+2); 
  Lcd_WriteData(y_start+2);
  
  Lcd_WriteIndex(0x21);
  Lcd_WriteData(x_start+2);
  Lcd_WriteData(y_start+2);

  Lcd_WriteIndex(0x22);  
}

void Lcd_Reset(void)
{
  LCD_RST_CLR;
  delay(100);
  LCD_RST_SET;
  delay(50);
}

void Lcd_WriteReg(unsigned char index,unsigned int data)
{
  Lcd_WriteIndex(index);
  LCD_WriteData_16Bit(data);
}

void LCD_WriteData_16Bit(unsigned int data)
{
   LCD_CS_CLR;
   LCD_RS_SET;
   spi_write(data>>8); 
   spi_write(data);    
   LCD_CS_SET; 
}

void Lcd_Clear(unsigned int color)               
{  
   unsigned int i,m;
   Lcd_SetRegion(0,0,X_MAX_PIXEL-1,Y_MAX_PIXEL-1);
   for(i=0;i<X_MAX_PIXEL;i++)
    for(m=0;m<Y_MAX_PIXEL;m++)
    { 
      LCD_WriteData_16Bit(color);
    }   
}

void Lcd_Init(void)
{  
  Lcd_Reset(); //Reset before LCD Init.
  Lcd_WriteReg(0x10,0x2F8E); /* power control 1 */
  Lcd_WriteReg(0x11,0x000C); /* power control 2 */
  Lcd_WriteReg(0x07,0x0021); /* display control */
  Lcd_WriteReg(0x28,0x0006); /* vcom OTP */
  Lcd_WriteReg(0x28,0x0005);
  Lcd_WriteReg(0x27,0x057F); /* further bias current setting */
  Lcd_WriteReg(0x29,0x89A1); /* vcom OTP */
  Lcd_WriteReg(0x00,0x0001); /* OSC en */
  delay(100);
  Lcd_WriteReg(0x29,0x80B0); /* vcom OTP */
  delay(30);
  Lcd_WriteReg(0x29,0xFFFE); /* vcom OTP */
  Lcd_WriteReg(0x07,0x0023); /* display control */
  delay(30);
  Lcd_WriteReg(0x07,0x0033); /* display control */
  Lcd_WriteReg(0x01,0x2183); /* driver output control, REV, TB, RGB */
  Lcd_WriteReg(0x03,0x6830); /* entry mode, 65K, ram, ID0 */
  Lcd_WriteReg(0x2F,0xFFFF); /* 2A ~ 2F, test */
  Lcd_WriteReg(0x2C,0x8000);
  Lcd_WriteReg(0x27,0x0570);
  Lcd_WriteReg(0x02,0x0300); /* driving wave form control */
  Lcd_WriteReg(0x0B,0x580C); /* frame cycle control */
  Lcd_WriteReg(0x12,0x0609); /* power control 3 */
  Lcd_WriteReg(0x13,0x3100); /* power control 4 */
}

void pin_init(void)
{
    pinMode(LCD_CS, OUTPUT);
    pinMode(LCD_RST, OUTPUT);
    pinMode(LCD_RS, OUTPUT); 
    LCD_CS_SET;
    LCD_RST_SET;
    LCD_RS_SET;
}

void setup() 
{
    Serial.begin(9600);
    spi_init();
    pin_init();
    Lcd_Init();
    Lcd_Clear(BLACK); 
}

void loop() 
{    
   Lcd_Clear(WHITE);
   delay(1000); 
   Lcd_Clear(BLACK);
    delay(1000); 
   Lcd_Clear(RED); 
   delay(1000); 
   Lcd_Clear(GREEN);
   delay(1000);  
   Lcd_Clear(BLUE); 
  delay(1000); 
}
