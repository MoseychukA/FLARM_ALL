// IMPORTANT: LCDWIKI_SPI LIBRARY MUST BE SPECIFICALLY
// CONFIGURED FOR EITHER THE TFT SHIELD OR THE BREAKOUT BOARD.

//This program is a demo of displaying string

//when using the BREAKOUT BOARD only and using these hardware spi lines to the LCD,
//the SDA pin and SCK pin is defined by the system and can't be modified.
//if you don't need to control the LED pin,you can set it to 3.3V and set the pin definition to -1.
//other pins can be defined by youself,for example
//pin usage as follow:
//                 CS  A0/DC  RESET  SDA  SCK  LED    VCC     GND    
//ESP8266 D1 nimi  D1   D3     D2    D7   D5   D4   5V/3.3V   GND

//Remember to set the pins to suit your display module!
/********************************************************************************
* @attention
*
* THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
* WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
* TIME. AS A RESULT, QD electronic SHALL NOT BE HELD LIABLE FOR ANY
* DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
* FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE 
* CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
**********************************************************************************/
#include <LCDWIKI_GUI.h> //Core graphics library
#include <LCDWIKI_SPI.h> //Hardware-specific library

//paramters define
#define MODEL SSD1283A
#define CS   D1
#define CD   D3
//#define SDA  A2   //if you use the hardware spi,this pin is no need to set
//#define SCK  A1   //if you use the hardware spi,this pin is no need to set
#define RST  D2
#define LED  D4   //if you don't need to control the LED pin,you should set it to -1 and set it to 3.3V

//the definiens of hardware spi mode as follow:
//if the IC model is known or the modules is unreadable,you can use this constructed function
LCDWIKI_SPI mylcd(MODEL,CS,CD,RST,LED); 

#define  BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

void setup() 
{
  mylcd.Init_LCD();
  mylcd.Fill_Screen(BLACK);
}

void loop() 
{
  mylcd.Set_Text_Mode(0);
  
  mylcd.Fill_Screen(0x0000);
  mylcd.Set_Text_colour(RED);
  mylcd.Set_Text_Back_colour(BLACK);
  mylcd.Set_Text_Size(1);
  mylcd.Print_String("Hello World!", 0, 0);
  mylcd.Print_Number_Float(01234.56789, 2, 0, 8, '.', 0, ' ');  
  mylcd.Print_Number_Int(0xDEADBEF, 0, 16, 0, ' ',16);

  mylcd.Set_Text_colour(GREEN);
  mylcd.Set_Text_Size(2);
  mylcd.Print_String("Hello", 0, 32);
  mylcd.Print_Number_Float(01234.56789, 2, 0, 48, '.', 0, ' ');  
  mylcd.Print_Number_Int(0xDEADBEF, 0, 64, 0, ' ',16);

  mylcd.Set_Text_colour(BLUE);
  mylcd.Set_Text_Size(3);
  mylcd.Print_String("Hello", 0, 86);
  mylcd.Print_Number_Float(01234.56789, 2, 0, 110, '.', 0, ' ');  
  mylcd.Print_Number_Int(0xDEADBEF, 0, 134, 0, ' ',16);

  delay(3000);
}
