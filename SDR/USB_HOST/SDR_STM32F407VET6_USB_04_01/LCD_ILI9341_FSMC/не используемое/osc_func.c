/*----------------------------------------------------------------------------
 *      OSC
 *---------------------------------------------------------------------------*/	
 /* Includes ------------------------------------------------------------------*/
#include "cmsis_os.h"
//#include "rl_usb.h"
//#include "init.h"

#include "tft_lcd.h"
#include "osc_func.h"
 
	// Display colours
#define BEAM1_COLOUR ILI9341_GREEN
#define BEAM2_COLOUR ILI9341_RED
#define GRATICULE_COLOUR 0x07FF
#define BEAM_OFF_COLOUR ILI9341_BLACK
#define CURSOR_COLOUR ILI9341_GREEN

// Analog input
#define ANALOG_MAX_VALUE 4096 
float samplingTime = 0;
float displayTime = 0;

// Variables for the beam position
uint16_t signalX ;
uint16_t signalY ;
uint16_t signalY1;
int16_t xZoomFactor = 1;
// yZoomFactor (percentage)
int16_t yZoomFactor = 100; //Adjusted to get 3.3V wave to fit on screen
int16_t yPosition = 0 ;

// Startup with sweep hold off or on
//bool triggerHeld = 0 ;


unsigned long sweepDelayFactor = 1;
unsigned long timeBase = 200;  //Timebase in microseconds

// Screen dimensions
int16_t myWidth ;
int16_t myHeight ;

//Trigger stuff
//bool notTriggered ;

// Sensitivity is the necessary change in AD value which will cause the scope to trigger.
// If VAD=3.3 volts, then 1 unit of sensitivity is around 0.8mV but this assumes no external attenuator. Calibration is needed to match this with the magnitude of the input signal.

// Trigger is setup in one of 32 positions
#define TRIGGER_POSITION_STEP ANALOG_MAX_VALUE/32
// Trigger default position (half of full scale)
int32_t triggerValue = 2048; 

int16_t retriggerDelay = 0;
int8_t triggerType = 2; //0-both 1-negative 2-positive

//Array for trigger points
uint16_t triggerPoints[2];
	
// Samples - depends on available RAM 6K is about the limit on an STM32F103C8T6
// Bear in mind that the ILI9341 display is only able to display 240x320 pixels, at any time but we can output far more to the serial port, we effectively only show a window on our samples on the TFT.
# define maxSamples 1024*6 //1024*6
uint32_t startSample = 0; //10
uint32_t endSample = maxSamples ;

// Array for the ADC data
//uint16_t dataPoints[maxSamples];
uint32_t dataPoints32[maxSamples / 2];
uint16_t *dataPoints = (uint16_t *)&dataPoints32;

//array for computed data (speedup)
uint16_t dataPlot[320]; //max(width,height) for this display
		
	void showGraticule(void)
{ uint8_t TicksX;
	uint8_t TicksY;
	
  ILI9341_DrawRectangle(0, 0, myHeight, myWidth, GRATICULE_COLOUR);
  // Dot grid - ten distinct divisions (9 dots) in both X and Y axis.
	
  for ( TicksX = 1; TicksX < 10; TicksX++)
  { 
    for ( TicksY = 1; TicksY < 10; TicksY++)
    {
      ILI9341_DrawPixel(  TicksX * (myHeight / 10), TicksY * (myWidth / 10), GRATICULE_COLOUR);
    }
  }
  // Horizontal and Vertical centre lines 5 ticks per grid square with a longer tick in line with our dots
	
  for ( TicksX = 0; TicksX < myWidth; TicksX += (myHeight / 50))
  {
    if (TicksX % (myWidth / 10) > 0 )
    {
      ILI9341_DrawLine(  (myHeight / 2) - 1 , TicksX,  ((myHeight / 2) - 1)+3, TicksX, GRATICULE_COLOUR); //TFT.drawFastHLine
    }
    else
    {
      ILI9341_DrawLine(  (myHeight / 2) - 3 , TicksX, ((myHeight / 2) - 1)+7, TicksX, GRATICULE_COLOUR);
    }

  }
	
  for ( TicksY = 0; TicksY < myHeight; TicksY += (myHeight / 50) )
  {
    if (TicksY % (myHeight / 10) > 0 )
    {
      ILI9341_DrawLine( TicksY,  (myWidth / 2) - 1 , TicksY, ((myWidth / 2) - 1)+3, GRATICULE_COLOUR); //TFT.drawFastVLine
    }
    else
    {
      ILI9341_DrawLine( TicksY,  (myWidth / 2) - 3 , TicksY, ((myWidth / 2) - 1)+7, GRATICULE_COLOUR);
    }
  }
}	
	
	
	void TFTSamplesClear (uint16_t beamColour)
{
  for (signalX=1 ; signalX < myWidth - 2; signalX++)
  {
    //use saved data to improve speed TFT.drawLine
     ILI9341_DrawLine(  dataPlot[signalX-1], signalX, dataPlot[signalX] , signalX + 1, beamColour) ;
  }
}


void TFTSamples (uint16_t beamColour)
{
  //calculate first sample
  signalY =  ((myHeight * dataPoints[0 * ((endSample - startSample) / (myWidth * timeBase / 100)) + 1]) / ANALOG_MAX_VALUE) * (yZoomFactor / 100) + yPosition;
  dataPlot[0]=signalY * 99 / 100 + 1;
  
  for (signalX=1 ; signalX < myWidth - 2; signalX++)
  {
    // Scale our samples to fit our screen. Most scopes increase this in steps of 5,10,25,50,100 250,500,1000 etc
    // Pick the nearest suitable samples for each of our myWidth screen resolution points
    signalY1 = ((myHeight * dataPoints[(signalX + 1) * ((endSample - startSample) / (myWidth * timeBase / 100)) + 1]) / ANALOG_MAX_VALUE) * (yZoomFactor / 100) + yPosition ;
    dataPlot[signalX] = signalY1 * 99 / 100 + 1;
    ILI9341_DrawLine (  dataPlot[signalX-1], signalX, dataPlot[signalX] , signalX + 1, beamColour) ;
    signalY = signalY1;
  }
}
