/*********************************************************************
This is an example for our Monochrome OLEDs based on SSD1306 drivers

  Pick one up today in the adafruit shop!
  ------> http://www.adafruit.com/category/63_98

This example is for a 128x64 size display using I2C to communicate
3 pins are required to interface (2 I2C and one reset)

Adafruit invests time and resources providing this open source code, 
please support Adafruit and open-source hardware by purchasing 
products from Adafruit!

Written by Limor Fried/Ladyada  for Adafruit Industries.  
BSD license, check license.txt for more information
All text above, and the splash screen must be included in any redistribution
*********************************************************************/

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define OLED_RESET 4
Adafruit_SSD1306 display(OLED_RESET);

#define NUMFLAKES 1
#define XPOS 0
#define YPOS 1
#define DELTAY 2


#define LOGO16_GLCD_HEIGHT 64
#define LOGO16_GLCD_WIDTH  128

long unsigned int startMillis;
short unsigned int iter = 0;              // used to calculate the frames per second (FPS)
int winkel = 0; 

const static uint8_t bmp[] PROGMEM = {
0x20, 0x00, 0x20, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0xFF, 0xFF, 0xFF, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00,
0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00,
0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00,
0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00,
0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x42, 0x41, 0x00, 0x80, 0x42, 0x41
};


#if (SSD1306_LCDHEIGHT != 64)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

void setup()   {                
  Serial.begin(9600);

  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3D (for the 128x64)
  // init done
  
  // Show image buffer on the display hardware.
  // Since the buffer is intialized with an Adafruit splashscreen
  // internally, this will display the splashscreen.
  display.display();
  delay(2000);

  // Clear the buffer.
  display.clearDisplay();

  // miniature bitmap display
  display.drawBitmap(0, 0, bmp, 64 , 32, 1);
  display.display();
  delay(1);
}



void Rotate_and_Draw_Bitmap(const uint8_t * bitmap, int winkel, uint8_t x, uint8_t y, uint8_t color){
 uint8_t width, height;
 width = 32;            // Read the image width from the array in PROGMEM
 height = 32;           // Read the height width from the array in PROGMEM

 int altes_x, altes_y, neues_x, neues_y; // old and new (rotated) Pixel-Coordinates

 int drehpunkt_x = width / 2;      // Calculate the (rotation) center of the image (x fraction)
 int drehpunkt_y = height / 2;     // Calculate the (rotation) center of the image (y fraction)

 float winkel_rad = winkel / 57.3;

 float sin_winkel = sin(winkel_rad);   // Lookup the sinus
 float cos_winkel = cos(winkel_rad);   // Lookup the cosinus

 uint8_t gedrehtes_bild[height/8*width+2]; // Image array in RAM (will contain the rotated image)
 memset(gedrehtes_bild,0,sizeof(gedrehtes_bild)); // Clear the array with 0
 
 int i, j, counter = 0;

 gedrehtes_bild[0] = width;                // First byte of the rotated image contains (as the original) the width
 gedrehtes_bild[1] = height;               // Second byte of the rotated image contains (as the original) the height

 for(i = 0; i < height * width / 8; i++) { // i goes through all the Bytes of the image
        uint8_t displayData = bmp;  // Read the image data from PROGMEM
      for(j = 0; j < 8; j++) {           // j goes through all the Bits of a Byte
                  if(displayData & (1 << j)){ // if a Bit is set, rotate it
                    altes_x = ((i % width) + 1) - drehpunkt_x;                     // Calculate the x-position of the Pixel to be rotated
                    altes_y = drehpunkt_y - (((int)(i/width))*8+j+1);              // Calculate the y-position of the Pixel to be rotated
                    neues_x = (int) (altes_x * cos_winkel - altes_y * sin_winkel); // Calculate the x-position of the rotated Pixel
                    neues_y = (int) (altes_y * cos_winkel + altes_x * sin_winkel); // Calculate the y-position of the rotated Pixel
                    
                    // Check if the rotated pixel is withing the image (important if non-square images are used). If not, continue with the next pixel.
                    if (neues_x <= (drehpunkt_x - 1) && neues_x >= (1 - drehpunkt_x) && neues_y <= (drehpunkt_y - 1) && neues_y >= (1 - drehpunkt_y)){ 
                      // Write the rotated bit to the array (gedrehtes_bild[]) in RAM
                      gedrehtes_bild[(neues_x + drehpunkt_x)%width + ((int)((drehpunkt_y - neues_y - 1) / 8)*width) +2] |= (1 << (drehpunkt_y - neues_y - 1)%8); 
                    }
                  }
      }
 }

  

 display.drawBitmap(50,20, gedrehtes_bild, x, y, 1);
 
 display.display();
// display.clearDisplay();
 //GLCD.DrawRamBitmap(gedrehtes_bild,x,y,color); // Draw the rotated image
}


void loop() {
 iter = 0;
 startMillis = millis();
 while( millis() - startMillis < 1000){ // loop for one second
   winkel += 6;                         // increase angle by 4 degrees
   Rotate_and_Draw_Bitmap(bmp, winkel, 32,32, 1); 
   display.clearDisplay();
   iter++;
 }}