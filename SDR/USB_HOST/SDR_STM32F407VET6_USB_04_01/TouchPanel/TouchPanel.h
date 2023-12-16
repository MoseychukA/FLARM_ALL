#ifndef __TOUCH_7846_H
#define __TOUCH_7846_H
#include "stdbool.h"
#ifdef __cplusplus
 extern "C" {
#endif
#include "stdio.h"
#include "main.h"


#define TOUCH_PRESSED	1
#define TOUCH_UNPRESSED	0
#define LCD_PIXEL_WIDTH			320
#define LCD_PIXEL_HEIGHT		240

// 
void TouchInit ( void );
void TouchCalibrate (void);

// 
bool TouchReadXY ( uint16_t *px, uint16_t *py, bool isReadCorrected);
void TouchReadStore ( void );

// 
bool isTouch ( void );

#ifdef __cplusplus
}
#endif

#endif
