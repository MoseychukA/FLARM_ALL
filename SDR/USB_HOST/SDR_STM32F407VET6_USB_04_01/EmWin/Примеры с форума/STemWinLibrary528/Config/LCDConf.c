/*********************************************************************
*          Portions COPYRIGHT 2013 STMicroelectronics                *
*          Portions SEGGER Microcontroller GmbH & Co. KG             *
*        Solutions for real time microcontroller applications        *
**********************************************************************
*                                                                    *
*        (c) 1996 - 2013  SEGGER Microcontroller GmbH & Co. KG       *
*                                                                    *
*        Internet: www.segger.com    Support:  support@segger.com    *
*                                                                    *
**********************************************************************

** emWin V5.20 - Graphical user interface for embedded applications **
All  Intellectual Property rights  in the Software belongs to  SEGGER.
emWin is protected by  international copyright laws.  Knowledge of the
source code may not be used to write a similar product.  This file may
only be used in accordance with the following terms:

The  software has  been licensed  to STMicroelectronics International
N.V. a Dutch company with a Swiss branch and its headquarters in Plan-
les-Ouates, Geneva, 39 Chemin du Champ des Filles, Switzerland for the
purposes of creating libraries for ARM Cortex-M-based 32-bit microcon_
troller products commercialized by Licensee only, sublicensed and dis_
tributed under the terms and conditions of the End User License Agree_
ment supplied by STMicroelectronics International N.V.
Full source code is available at: www.segger.com

We appreciate your understanding and fairness.
----------------------------------------------------------------------
File        : LCDConf.c
Purpose     : Display controller configuration

---------------------------END-OF-HEADER------------------------------
*/

/**
  ******************************************************************************
  * @file    LCDConf.c
  * @author  MCD Application Team
  * @version V1.0.0
  * @date    22-July-2013
  * @brief   Template driver, should be used as starting point for new display drivers
  ******************************************************************************
  * @attention
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software 
  * distributed under the License is distributed on an "AS IS" BASIS, 
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

// Драйвер SSD1963
#include "SSD1963.h"

#include "LCD_Private.h"
#include "GUI_Private.h"
#include "LCD_ConfDefaults.h"
#include "global_includes.h"

/*********************************************************************
*
*       Layer configuration 
*
**********************************************************************
*/
#define USE_OPTIMIZED_DRAW 

#undef LCD_SWAP_XY
#undef LCD_MIRROR_Y
#undef GUI_NUM_LAYERS

#define GUI_NUM_LAYERS  1
//#define LCD_SWAP_XY  1
//#define LCD_MIRROR_Y 1
#define LCD_SWAP_XY  0
#define LCD_MIRROR_Y 0

//
// Physical display size
//
//#define XSIZE_PHYS  320
//#define YSIZE_PHYS  240
#define XSIZE_PHYS  DISP_WIDTH
#define YSIZE_PHYS  DISP_HEIGHT

//
// Color conversion
//
#define COLOR_CONVERSION  GUICC_M565

//
// Display driver
//
#define DISPLAY_DRIVER  GUIDRV_API

#ifndef   VXSIZE_PHYS
  #define VXSIZE_PHYS XSIZE_PHYS
#endif
#ifndef   VYSIZE_PHYS
  #define VYSIZE_PHYS YSIZE_PHYS
#endif
#ifndef   LCD_XSIZE 
  #define LCD_XSIZE XSIZE_PHYS
#endif
#ifndef   LCD_YSIZE
  #define LCD_YSIZE YSIZE_PHYS
#endif

#ifndef   XSIZE_PHYS
  #error Physical X size of display is not defined!
#endif
#ifndef   YSIZE_PHYS
  #error Physical Y size of display is not defined!
#endif
#ifndef   COLOR_CONVERSION
  #error Color conversion not defined!
#endif
#ifndef   DISPLAY_DRIVER
  #error No display driver defined!
#endif


/*********************************************************************
*
*       Defines
*
**********************************************************************
*/
/*********************************************************************
*
*       Macros for MIRROR_, SWAP_ and LUT_
*/
#if (!defined (LCD_LUT_COM) && !defined(LCD_LUT_SEG))
  #if   (!LCD_MIRROR_X && !LCD_MIRROR_Y && !LCD_SWAP_XY) 
    #define LOG2PHYS_X(x, y) x
    #define LOG2PHYS_Y(x, y) y
  #elif (!LCD_MIRROR_X && !LCD_MIRROR_Y &&  LCD_SWAP_XY) 
    #define LOG2PHYS_X(x, y) y
    #define LOG2PHYS_Y(x, y) x
  #elif (!LCD_MIRROR_X &&  LCD_MIRROR_Y && !LCD_SWAP_XY) 
    #define LOG2PHYS_X(x, y) x
    #define LOG2PHYS_Y(x, y) LCD_YSIZE - 1 - (y)
  #elif (!LCD_MIRROR_X &&  LCD_MIRROR_Y &&  LCD_SWAP_XY) 
    #define LOG2PHYS_X(x, y) y
    #define LOG2PHYS_Y(x, y) LCD_XSIZE - 1 - (x)
  #elif ( LCD_MIRROR_X && !LCD_MIRROR_Y && !LCD_SWAP_XY) 
    #define LOG2PHYS_X(x, y) LCD_XSIZE - 1 - (x)
    #define LOG2PHYS_Y(x, y) y
  #elif ( LCD_MIRROR_X && !LCD_MIRROR_Y &&  LCD_SWAP_XY) 
    #define LOG2PHYS_X(x, y) LCD_YSIZE - 1 - (y)
    #define LOG2PHYS_Y(x, y) x
  #elif ( LCD_MIRROR_X &&  LCD_MIRROR_Y && !LCD_SWAP_XY) 
    #define LOG2PHYS_X(x, y) LCD_XSIZE - 1 - (x)
    #define LOG2PHYS_Y(x, y) LCD_YSIZE - 1 - (y)
  #elif ( LCD_MIRROR_X &&  LCD_MIRROR_Y &&  LCD_SWAP_XY) 
    #define LOG2PHYS_X(x, y) LCD_YSIZE - 1 - (y)
    #define LOG2PHYS_Y(x, y) LCD_XSIZE - 1 - (x)
  #endif
#else
  #if   ( defined (LCD_LUT_COM) && !defined(LCD_LUT_SEG))
    #define LOG2PHYS_X(x, y) x
    #define LOG2PHYS_Y(x, y) LCD__aLine2Com0[y]
  #elif (!defined (LCD_LUT_COM) &&  defined(LCD_LUT_SEG))
    #define LOG2PHYS_X(x, y) LCD__aCol2Seg0[x]
    #define LOG2PHYS_Y(x, y) y
  #elif ( defined (LCD_LUT_COM) &&  defined(LCD_LUT_SEG))
    #define LOG2PHYS_X(x, y) LCD__aCol2Seg0[x]
    #define LOG2PHYS_Y(x, y) LCD__aLine2Com0[y]
  #endif
#endif

/*********************************************************************
*
*       Types
*
**********************************************************************
*/
typedef struct
{
  U32 VRAMAddr;
  int xSize, ySize;
  int vxSize, vySize;
  int vxSizePhys;
  int BitsPerPixel;
} DRIVER_CONTEXT;

/*********************************************************************
*
*       Static functions
*
**********************************************************************
*/
/*********************************************************************
*
*       _SetPixelIndex
*
* Purpose:
*   Sets the index of the given pixel. The upper layers
*   calling this routine make sure that the coordinates are in range, so
*   that no check on the parameters needs to be performed.
*/
static void _SetPixelIndex(GUI_DEVICE * pDevice, int x, int y, int PixelIndex)
{
  #ifdef WIN32
    LCDSIM_SetPixelIndex(x, y, PixelIndex, pDevice->LayerIndex);
  #else
    //
    // Convert logical into physical coordinates (Dep. on LCDConf.h)
    //
    #if (LCD_MIRROR_X == 1) || (LCD_MIRROR_Y == 1) || (LCD_SWAP_XY == 1)
      int xPhys, yPhys;

      xPhys = LOG2PHYS_X(x, y);
      yPhys = LOG2PHYS_Y(x, y);
    #else
      #define xPhys x
      #define yPhys y
    #endif
    GUI_USE_PARA(pDevice);
    GUI_USE_PARA(x);
    GUI_USE_PARA(y);
    GUI_USE_PARA(PixelIndex);

      //
      // Write into hardware ... Adapt to your system
	  //
	  
	  //Example : _SetPixel(PixelIndex)
      // Поставить точку указанного цвета
      SSD1963_PutPixel ( xPhys, yPhys, PixelIndex );

    #if (LCD_MIRROR_X == 0) && (LCD_MIRROR_Y == 0) && (LCD_SWAP_XY == 0)
      #undef xPhys
      #undef yPhys
    #endif
  #endif
} // _SetPixelIndex

/*********************************************************************
*
*       _GetPixelIndex
*
* Purpose:
*   Returns the index of the given pixel. The upper layers
*   calling this routine make sure that the coordinates are in range, so
*   that no check on the parameters needs to be performed.
*/
static unsigned int _GetPixelIndex(GUI_DEVICE * pDevice, int x, int y)
{
  uint16_t PixelIndex;

  #ifdef WIN32
    PixelIndex = LCDSIM_GetPixelIndex(x, y, pDevice->LayerIndex);
  #else
    //
    // Convert logical into physical coordinates (Dep. on LCDConf.h)
    //
    #if (LCD_MIRROR_X == 1) || (LCD_MIRROR_Y == 1) || (LCD_SWAP_XY == 1)
      int xPhys, yPhys;

      xPhys = LOG2PHYS_X(x, y);
      yPhys = LOG2PHYS_Y(x, y);
    #else
      #define xPhys x
      #define yPhys y
    #endif
    GUI_USE_PARA(pDevice);
    GUI_USE_PARA(x);
    GUI_USE_PARA(y);

	  //
      // Read from hardware ... Adapt to your system
	  //
	  
      //Example : _GetPixel(PixelIndex)
    // Считать точку
    PixelIndex = SSD1963_GetPixel ( xPhys, yPhys );

    #if (LCD_MIRROR_X == 0) && (LCD_MIRROR_Y == 0) && (LCD_SWAP_XY == 0)
      #undef xPhys
      #undef yPhys
    #endif
  #endif
  return PixelIndex;
} // _GetPixelIndex

/*********************************************************************
*
*       _XorPixel
*/
static void _XorPixel ( GUI_DEVICE *pDevice, int x, int y )
{
  LCD_PIXELINDEX PixelIndex;
  LCD_PIXELINDEX IndexMask;

  PixelIndex = _GetPixelIndex(pDevice, x, y);
  IndexMask  = pDevice->pColorConvAPI->pfGetIndexMask();
  _SetPixelIndex(pDevice, x, y, PixelIndex ^ IndexMask);
} // _XorPixel

/*********************************************************************
*
*       _DrawHLine
*/
static void _DrawHLine ( GUI_DEVICE *pDevice, int x0, int y, int x1 )
{
//	  int xPhys, yPhys, lphys, idx;
  LCD_PIXELINDEX ColorIndex;	// unsigned long

  if (GUI_pContext->DrawMode & LCD_DRAWMODE_XOR )
  {
    for ( ; x0 <= x1; x0 ++ )
    {
      _XorPixel ( pDevice, x0, y );
    } // for
  } // if
  else
  {
    ColorIndex = LCD__GetColorIndex();

    for ( ; x0 <= x1; x0 ++ )
    {
//      _XorPixel ( pDevice, x0, y );
      _SetPixelIndex ( pDevice, x0, y, ColorIndex );
    } // for

    // Поставить точку указанного цвета
//    SSD1963_PutPixel ( t16_t x, uint16_t y, uint16_t color );

/*
#ifdef USE_OPTIMIZED_DRAW   
#if LCD_SWAP_XY | LCD_MIRROR_X| LCD_MIRROR_Y
   xPhys = LOG2PHYS_X(x0, y);
   yPhys = LOG2PHYS_Y(x0, y); 
   lphys = (x1 - x0);
  #else
    #define xPhys x0
    #define yPhys y
    #define lphys x1
  #endif
   
    for ( idx = 0; idx <= lphys; idx ++ )
    {
	  //
      // Write into hardware ... Adapt to your system
	  //
	  
	  //Example : _SetPixel(ColorIndex)
    } // for
#else
    for (; x0 <= x1; x0++)
    {
      _SetPixelIndex(pDevice, x0, y, ColorIndex);
    } // for
#endif
*/
  } // else
} // _DrawHLine

/*********************************************************************
*
*       _DrawVLine, not optimized
*/
static void _DrawVLine ( GUI_DEVICE * pDevice, int x, int y0,  int y1 )
{
  LCD_PIXELINDEX ColorIndex;

  if ( GUI_pContext->DrawMode & LCD_DRAWMODE_XOR )
  {
    for ( ; y0 <= y1; y0 ++ )
    {
      _XorPixel ( pDevice, x, y0 );
    } // for
  } // if
  else
  {
    ColorIndex = LCD__GetColorIndex ( );
    for ( ; y0 <= y1; y0 ++ )
    {
      _SetPixelIndex ( pDevice, x, y0, ColorIndex );
    } // for
  } // else
} // _DrawVLine

/*********************************************************************
*
*       _FillRect
*/
static void _FillRect ( GUI_DEVICE * pDevice, int x0, int y0, int x1, int y1 )
{
/*
  for (; y0 <= y1; y0 ++ )
  {
    _DrawHLine(pDevice, x0, y0, x1);
  } // for
*/
	// Заливка прямоугольника указанным цветом
	SSD1963_FillRect_fast ( x0, y0, x1, y1, LCD__GetColorIndex() );
} // _FillRect

/*********************************************************************
*
*       Draw Bitmap 1 BPP
*/
static void _DrawBitLine1BPP ( GUI_DEVICE * pDevice, int x, int y, U8 const GUI_UNI_PTR *p, int Diff,
		int xsize, const LCD_PIXELINDEX * pTrans )
{
  LCD_PIXELINDEX IndexMask, Index0, Index1, Pixel;

  Index0 = *(pTrans + 0);
  Index1 = *(pTrans + 1);
  x += Diff;
  switch (GUI_pContext->DrawMode & (LCD_DRAWMODE_TRANS | LCD_DRAWMODE_XOR))
  {
	  case 0:
		do
		{
		  _SetPixelIndex ( pDevice, x++, y, (*p & (0x80 >> Diff)) ? Index1 : Index0 );
		  if ( ++Diff == 8 )
		  {
			Diff = 0;
			p++;
		  } // if
		} while (--xsize);
		break;

	  case LCD_DRAWMODE_TRANS:
		do
		{
		  if ( *p & (0x80 >> Diff) )
			_SetPixelIndex( pDevice, x, y, Index1 );
		  x++;
		  if ( ++Diff == 8 )
		  {
			Diff = 0;
			p++;
		  } // if
		} while (--xsize);
		break;

	  case LCD_DRAWMODE_XOR | LCD_DRAWMODE_TRANS:
	  case LCD_DRAWMODE_XOR:
		IndexMask = pDevice->pColorConvAPI->pfGetIndexMask ( );
		do
		{
		  if (*p & (0x80 >> Diff))
		  {
			Pixel = _GetPixelIndex(pDevice, x, y);
			_SetPixelIndex(pDevice, x, y, Pixel ^ IndexMask);
		  } // if
		  x++;
		  if (++Diff == 8)
		  {
			Diff = 0;
			p++;
		  } // if
		} while (--xsize);
		break;
  } // switch
} // _DrawBitLine1BPP

/*********************************************************************
*
*       Draw Bitmap 2 BPP
*/
static void _DrawBitLine2BPP ( GUI_DEVICE * pDevice, int x, int y, U8 const GUI_UNI_PTR *p,
		int Diff, int xsize, const LCD_PIXELINDEX * pTrans)
{
  LCD_PIXELINDEX Pixels, PixelIndex;
  int CurrentPixel, Shift, Index;

  Pixels = *p;
  CurrentPixel = Diff;
  x += Diff;
  switch (GUI_pContext->DrawMode & (LCD_DRAWMODE_TRANS | LCD_DRAWMODE_XOR))
  {
	  case 0:
		if (pTrans) {
		  do {
			Shift = (3 - CurrentPixel) << 1;
			Index = (Pixels & (0xC0 >> (6 - Shift))) >> Shift;
			PixelIndex = *(pTrans + Index);
			_SetPixelIndex(pDevice, x++, y, PixelIndex);
			if (++CurrentPixel == 4) {
			  CurrentPixel = 0;
			  Pixels = *(++p);
			}
		  } while (--xsize);
		} else {
		  do {
			Shift = (3 - CurrentPixel) << 1;
			Index = (Pixels & (0xC0 >> (6 - Shift))) >> Shift;
			_SetPixelIndex(pDevice, x++, y, Index);
			if (++CurrentPixel == 4) {
			  CurrentPixel = 0;
			  Pixels = *(++p);
			}
		  } while (--xsize);
		}
		break;
	  case LCD_DRAWMODE_TRANS:
		if (pTrans) {
		  do {
			Shift = (3 - CurrentPixel) << 1;
			Index = (Pixels & (0xC0 >> (6 - Shift))) >> Shift;
			if (Index) {
			  PixelIndex = *(pTrans + Index);
			  _SetPixelIndex(pDevice, x, y, PixelIndex);
			}
			x++;
			if (++CurrentPixel == 4) {
			  CurrentPixel = 0;
			  Pixels = *(++p);
			}
		  } while (--xsize);
		} else {
		  do {
			Shift = (3 - CurrentPixel) << 1;
			Index = (Pixels & (0xC0 >> (6 - Shift))) >> Shift;
			if (Index) {
			  _SetPixelIndex(pDevice, x, y, Index);
			}
			x++;
			if (++CurrentPixel == 4) {
			  CurrentPixel = 0;
			  Pixels = *(++p);
			}
		  } while (--xsize);
		}
		break;
  } // switch
} // _DrawBitLine2BPP

/*********************************************************************
*
*       Draw Bitmap 4 BPP
*/
static void _DrawBitLine4BPP ( GUI_DEVICE * pDevice, int x, int y, U8 const GUI_UNI_PTR *p,
		int Diff, int xsize, const LCD_PIXELINDEX * pTrans )
{
  LCD_PIXELINDEX Pixels, PixelIndex;
  int CurrentPixel, Shift, Index;

  Pixels = *p;
  CurrentPixel = Diff;
  x += Diff;
  switch (GUI_pContext->DrawMode & (LCD_DRAWMODE_TRANS | LCD_DRAWMODE_XOR))
  {
	  case 0:
		if (pTrans)
		{
		  do {
			Shift = (1 - CurrentPixel) << 2;
			Index = (Pixels & (0xF0 >> (4 - Shift))) >> Shift;
			PixelIndex = *(pTrans + Index);
			_SetPixelIndex(pDevice, x++, y, PixelIndex);
			if (++CurrentPixel == 2) {
			  CurrentPixel = 0;
			  Pixels = *(++p);
			}
		  } while (--xsize);
		} else {
		  do {
			Shift = (1 - CurrentPixel) << 2;
			Index = (Pixels & (0xF0 >> (4 - Shift))) >> Shift;
			_SetPixelIndex(pDevice, x++, y, Index);
			if (++CurrentPixel == 2) {
			  CurrentPixel = 0;
			  Pixels = *(++p);
			}
		  } while (--xsize);
		}
		break;

	  case LCD_DRAWMODE_TRANS:
		if (pTrans)
		{
		  do {
			Shift = (1 - CurrentPixel) << 2;
			Index = (Pixels & (0xF0 >> (4 - Shift))) >> Shift;
			if (Index) {
			  PixelIndex = *(pTrans + Index);
			  _SetPixelIndex(pDevice, x, y, PixelIndex);
			}
			x++;
			if (++CurrentPixel == 2) {
			  CurrentPixel = 0;
			  Pixels = *(++p);
			}
		  } while (--xsize);
		} else {
		  do {
			Shift = (1 - CurrentPixel) << 2;
			Index = (Pixels & (0xF0 >> (4 - Shift))) >> Shift;
			if (Index) {
			  _SetPixelIndex(pDevice, x, y, Index);
			}
			x++;
			if (++CurrentPixel == 2) {
			  CurrentPixel = 0;
			  Pixels = *(++p);
			}
		  } while (--xsize);
		} // else
		break;
  } // switch
} // _DrawBitLine4BPP

/*********************************************************************
*
*       Draw Bitmap 8 BPP
*/
static void _DrawBitLine8BPP(GUI_DEVICE * pDevice, int x, int y, U8 const GUI_UNI_PTR *p,
		int xsize, const LCD_PIXELINDEX * pTrans)
{
  LCD_PIXELINDEX Pixel;

  switch (GUI_pContext->DrawMode & (LCD_DRAWMODE_TRANS | LCD_DRAWMODE_XOR))
  {
	  case 0:
		if (pTrans) {
		  for (; xsize > 0; xsize--, x++, p++) {
			Pixel = *p;
			_SetPixelIndex(pDevice, x, y, *(pTrans + Pixel));
		  }
		} else {
		  for (; xsize > 0; xsize--, x++, p++) {
			_SetPixelIndex(pDevice, x, y, *p);
		  }
		}
		break;
	  case LCD_DRAWMODE_TRANS:
		if (pTrans) {
		  for (; xsize > 0; xsize--, x++, p++) {
			Pixel = *p;
			if (Pixel) {
			  _SetPixelIndex(pDevice, x, y, *(pTrans + Pixel));
			}
		  }
		} else {
		  for (; xsize > 0; xsize--, x++, p++) {
			Pixel = *p;
			if (Pixel) {
			  _SetPixelIndex(pDevice, x, y, Pixel);
			}
		  }
		}
		break;
  } // switch
} // _DrawBitLine8BPP

/*********************************************************************
*
*       Draw Bitmap 16 BPP, not optimized
*/
static void _DrawBitLine16BPP ( GUI_DEVICE *pDevice, int x, int y, U16 const GUI_UNI_PTR * p, int xsize )
{
  for (;xsize > 0; xsize--, x++, p++)
  {
    _SetPixelIndex(pDevice, x, y, *p);
  }
} // _DrawBitLine16BPP

/*********************************************************************
*
*       Draw Bitmap 32 BPP, not optimized
*/
static void _DrawBitLine32BPP ( GUI_DEVICE *pDevice, int x, int y, U32 const GUI_UNI_PTR *p, int xsize )
{
  for ( ; xsize > 0; xsize--, x++, p++ )
  {
    _SetPixelIndex(pDevice, x, y, *p);
  }
} // _DrawBitLine32BPP

/*********************************************************************
*
*       _DrawBitmap
*/
static void _DrawBitmap ( GUI_DEVICE * pDevice, int x0, int y0,
                       int xSize, int ySize,
                       int BitsPerPixel, 
                       int BytesPerLine,
                       const U8 GUI_UNI_PTR * pData, int Diff,
                       const LCD_PIXELINDEX* pTrans )
{
  int i;

  switch (BitsPerPixel)
  {
	  case 1:
		for ( i = 0; i < ySize; i ++ )
		{
		  _DrawBitLine1BPP(pDevice, x0, i + y0, pData, Diff, xSize, pTrans);
		  pData += BytesPerLine;
		}
		break;

	  case 2:
		for ( i = 0; i < ySize; i ++ )
		{
		  _DrawBitLine2BPP ( pDevice, x0, i + y0, pData, Diff, xSize, pTrans );
		  pData += BytesPerLine;
		}
		break;

	  case 4:
		for ( i = 0; i < ySize; i ++ )
		{
		  _DrawBitLine4BPP(pDevice, x0, i + y0, pData, Diff, xSize, pTrans);
		  pData += BytesPerLine;
		}
		break;

	  case 8:
		for ( i = 0; i < ySize; i++ )
		{
		  _DrawBitLine8BPP ( pDevice, x0, i + y0, pData, xSize, pTrans );
		  pData += BytesPerLine;
		}
		break;

	  case 16:
		for ( i = 0; i < ySize; i ++ )
		{
		  _DrawBitLine16BPP ( pDevice, x0, i + y0, (const U16 *)pData, xSize );
		  pData += BytesPerLine;
		}
		break;

	  case 32:
		for (i = 0; i < ySize; i++)
		{
		  _DrawBitLine32BPP(pDevice, x0, i + y0, (const U32 *)pData, xSize);
		  pData += BytesPerLine;
		}
		break;
  } // switch
} // _DrawBitmap

/*********************************************************************
*
*       _InitOnce
*
* Purpose:
*   Allocates a fixed block for the context of the driver
*
* Return value:
*   0 on success, 1 on error
*/
static int _InitOnce ( GUI_DEVICE *pDevice )
{
  DRIVER_CONTEXT * pContext;

  if ( pDevice->u.pContext == NULL )
  {
    pDevice->u.pContext = GUI_ALLOC_GetFixedBlock ( sizeof(DRIVER_CONTEXT) );
    pContext = (DRIVER_CONTEXT *)pDevice->u.pContext;
    pContext->BitsPerPixel = LCD__GetBPP ( pDevice->pColorConvAPI->pfGetIndexMask ( ) );
  }
  return pDevice->u.pContext ? 0 : 1;
} // _InitOnce

/*********************************************************************
*
*       _GetDevProp
*/
static I32 _GetDevProp ( GUI_DEVICE * pDevice, int Index )
{
  DRIVER_CONTEXT * pContext;

  pContext = (DRIVER_CONTEXT *)pDevice->u.pContext;
  switch (Index)
  {
	  case LCD_DEVCAP_XSIZE:
		return pContext->xSize;
	  case LCD_DEVCAP_YSIZE:
		return pContext->ySize;
	  case LCD_DEVCAP_VXSIZE:
		return pContext->vxSize;
	  case LCD_DEVCAP_VYSIZE:
		return pContext->vySize;
	  case LCD_DEVCAP_BITSPERPIXEL:
		return pContext->BitsPerPixel;
	  case LCD_DEVCAP_NUMCOLORS:
		return 0;
	  case LCD_DEVCAP_XMAG:
		return 1;
	  case LCD_DEVCAP_YMAG:
		return 1;
	  case LCD_DEVCAP_MIRROR_X:
		return 0;
	  case LCD_DEVCAP_MIRROR_Y:
		return 0;
	  case LCD_DEVCAP_SWAP_XY:
		return 0;
  } // switch

  return -1;
} // _GetDevProp

/*********************************************************************
*
*       _GetDevData
*/
static void * _GetDevData(GUI_DEVICE * pDevice, int Index)
{
  GUI_USE_PARA(pDevice);
  #if GUI_SUPPORT_MEMDEV
    switch (Index)
    {
		case LCD_DEVDATA_MEMDEV:
		  return (void *)&GUI_MEMDEV_DEVICE_16;
    } // switch
  #else
    GUI_USE_PARA(Index);
  #endif
  return NULL;
}

/*********************************************************************
*
*       _GetRect
*/
static void _GetRect (GUI_DEVICE * pDevice, LCD_RECT * pRect)
{
  DRIVER_CONTEXT * pContext;

  pContext = (DRIVER_CONTEXT *)pDevice->u.pContext;
  pRect->x0 = 0;
  pRect->y0 = 0;
  pRect->x1 = pContext->vxSize - 1;
  pRect->y1 = pContext->vySize - 1;
} // _GetRect

/*********************************************************************
*
*       _SetOrg
*/
static void _SetOrg ( GUI_DEVICE *pDevice, int x, int y )
{
  LCD_X_SETORG_INFO Data = {0};

  Data.xPos = x;
  Data.yPos = y;
  LCD_X_DisplayDriver(pDevice->LayerIndex, LCD_X_SETORG, (void *)&Data);
} // _SetOrg

/*********************************************************************
*
*       Static code: Functions available by _GetDevFunc()
*
**********************************************************************
*/
/*********************************************************************
*
*       _SetVRAMAddr
*/
static void _SetVRAMAddr ( GUI_DEVICE *pDevice, void *pVRAM )
{
  DRIVER_CONTEXT * pContext;
  LCD_X_SETVRAMADDR_INFO Data = {0};

  _InitOnce(pDevice);
  if (pDevice->u.pContext)
  {
    pContext = (DRIVER_CONTEXT *)pDevice->u.pContext;
    pContext->VRAMAddr = (U32)pVRAM;
    Data.pVRAM = pVRAM;
    LCD_X_DisplayDriver(pDevice->LayerIndex, LCD_X_SETVRAMADDR, (void *)&Data);
  } // if
} // _SetVRAMAddr

/*********************************************************************
*
*       _SetVSize
*/
static void _SetVSize ( GUI_DEVICE *pDevice, int xSize, int ySize )
{
  DRIVER_CONTEXT * pContext;

  _InitOnce(pDevice);
  if (pDevice->u.pContext)
  {
    pContext = (DRIVER_CONTEXT *)pDevice->u.pContext;
    pContext->vxSize = xSize;
    pContext->vySize = ySize;
    pContext->vxSizePhys = xSize;
  }
} // _SetVSize

/*********************************************************************
*
*       _SetSize
*/
static void _SetSize ( GUI_DEVICE * pDevice, int xSize, int ySize)
{
  DRIVER_CONTEXT * pContext;
  LCD_X_SETSIZE_INFO Data = {0};

  _InitOnce(pDevice);
  if (pDevice->u.pContext)
  {
    pContext = (DRIVER_CONTEXT *)pDevice->u.pContext;
    pContext->vxSizePhys = (pContext->vxSizePhys == 0) ? xSize : pContext->vxSizePhys;
    pContext->xSize = xSize;
    pContext->ySize = ySize;
    Data.xSize = xSize;
    Data.ySize = ySize;
    LCD_X_DisplayDriver ( pDevice->LayerIndex, LCD_X_SETSIZE, (void *)&Data );
  } // if
} // _SetSize

/********************************************************************
*
*       _InitLcdController
*
* Function description:
*   Initializes the LCD controller
*/
static void _InitLcdController(void) 
{
  //
  // Initialize the LCD if not already done elsewhere
  //
  
  // Example : STM32xxx_LCD_Init();

	// Инициализация дисплея
	SSD1963_Init ( );
} // _InitLcdController

/*********************************************************************
*
*       _Init
*/
static int _Init ( GUI_DEVICE *pDevice )
{
  int r;

  r = _InitOnce ( pDevice );
  r |= LCD_X_DisplayDriver ( pDevice->LayerIndex, LCD_X_INITCONTROLLER, NULL );
  return r;
} // _Init

/*********************************************************************
*
*       _On
*/
static void _On ( GUI_DEVICE *pDevice )
{
  LCD_X_DisplayDriver ( pDevice->LayerIndex, LCD_X_ON, NULL );
} // _On

/*********************************************************************
*
*       _Off
*/
static void _Off ( GUI_DEVICE * pDevice )
{
  LCD_X_DisplayDriver ( pDevice->LayerIndex, LCD_X_OFF, NULL );
} // _Off

/*********************************************************************
*
*       _SetLUTEntry
*/
static void _SetLUTEntry ( GUI_DEVICE * pDevice, U8 Pos, LCD_COLOR Color )
{
  LCD_X_SETLUTENTRY_INFO Data = {0};

  Data.Pos   = Pos;
  Data.Color = Color;
  LCD_X_DisplayDriver ( pDevice->LayerIndex, LCD_X_SETLUTENTRY, (void *)&Data );
} // _SetLUTEntry

/*********************************************************************
*
*       _GetDevFunc
*/
static void (* _GetDevFunc(GUI_DEVICE ** ppDevice, int Index) )(void)
{
  GUI_USE_PARA(ppDevice);
  switch (Index)
  {
	  case LCD_DEVFUNC_SET_VRAM_ADDR:
		return (void (*)(void))_SetVRAMAddr;
	  case LCD_DEVFUNC_SET_VSIZE:
		return (void (*)(void))_SetVSize;
	  case LCD_DEVFUNC_SET_SIZE:
		return (void (*)(void))_SetSize;
	  case LCD_DEVFUNC_INIT:
		return (void (*)(void))_Init;
	  case LCD_DEVFUNC_ON:
		return (void (*)(void))_On;
	  case LCD_DEVFUNC_OFF:
		return (void (*)(void))_Off;
	  case LCD_DEVFUNC_SETLUTENTRY:
		return (void (*)(void))_SetLUTEntry;
  } // switch
  return NULL;
} //  _GetDevFunc

/*********************************************************************
*
*       Public data
*
**********************************************************************
*/
/*********************************************************************
*
*       GUI_DEVICE_API structure
*/
const GUI_DEVICE_API GUIDRV_API =
{
  //
  // Data
  //
  DEVICE_CLASS_DRIVER,
  //
  // Drawing functions
  //
  _DrawBitmap,
  _DrawHLine,
  _DrawVLine,
  _FillRect,
  _GetPixelIndex,
  _SetPixelIndex,
  _XorPixel,
  //
  // Set origin
  //
  _SetOrg,
  //
  // Request information
  //
  _GetDevFunc,
  _GetDevProp,
  _GetDevData,
  _GetRect,
};

/*********************************************************************
*
*       Public code
*
**********************************************************************
*/
/*********************************************************************
*
*       LCD_X_Config
*
* Purpose:
*   Called during the initialization process in order to set up the
*   display driver configuration.
*   
*/
// P. 1046
void LCD_X_Config(void) 
{
  /* Create and link the driver */
  GUI_DEVICE_CreateAndLink ( &GUIDRV_API, COLOR_CONVERSION, 0, 0 );
  
  //
  // Display driver configuration
  //
  LCD_SetSizeEx ( 0, XSIZE_PHYS , YSIZE_PHYS );
  LCD_SetVSizeEx ( 0, VXSIZE_PHYS, VYSIZE_PHYS );

  // for mouse
  GUI_TOUCH_SetOrientation ( 0 );
  GUI_TOUCH_Calibrate (GUI_COORD_X, 0, 800-1, 0, 800-1 );
  GUI_TOUCH_Calibrate (GUI_COORD_Y, 0, 480-1, 0, 480-1 );

}

/*********************************************************************
*
*       LCD_X_DisplayDriver
*
* Function description:
*   This function is called by the display driver for several purposes.
*   To support the according task the routine needs to be adapted to
*   the display controller. Please note that the commands marked with
*   'optional' are not cogently required and should only be adapted if
*   the display controller supports these features.
*
* Parameter:
*   LayerIndex - Index of layer to be configured
*   Cmd        - Please refer to the details in the switch statement below
*   pData      - Pointer to a LCD_X_DATA structure
*
* Return Value:
*   < -1 - Error
*     -1 - Command not handled
*      0 - Ok
*/
int LCD_X_DisplayDriver ( unsigned LayerIndex, unsigned Cmd, void * pData )
{
  int r = 0;
  (void) LayerIndex;
  (void) pData;
  
  switch (Cmd) 
  {
	  case LCD_X_INITCONTROLLER:
		{
			// Инициализация LCD
		  _InitLcdController();
		  return 0;
		}

	  case LCD_X_ON:
	  {
//		LCD_DisplayOn();
		break;
	  }
	  case LCD_X_OFF:
		{
//		LCD_DisplayOff();
		break;
	  }
	  default:
		r = -1;
  } // switch
  return r;
} //  LCD_X_DisplayDriver
/*************************** End of file ****************************/
