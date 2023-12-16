#ifndef FONT_H
#define FONT_H
#include "stm32f10x.h"  
//#include "font_18pts.h" 

#define indent 5   // точек (px)        // сколько точек остается поле после символа (отступ справа от символа до следующего символа)

//#define FONT      cousine_14ptcBitmaps // прописать нужный шрифт
//#define FONT_info cousine_14ptcFontInfo // описание шрифта, размер и т.д.
//extern const u8 cousine_14ptcBitmaps[];
//extern const u8 cousine_14ptcFontInfo[];	

//#define FONT      cousine_16ptcBitmaps  // прописать нужный шрифт
//#define FONT_info cousine_16ptcFontInfo // описание шрифта, размер и т.д. см.в конце файла шрифта
//extern const u8 cousine_16ptcBitmaps[];
//extern const u8 cousine_16ptcFontInfo[];	

#define FONT      cousine_18ptBitmaps // прописать нужный шрифт
#define FONT_info cousine_18ptFontInfo // описание шрифта, размер и т.д.
extern const u8 cousine_18ptBitmaps[];
extern const u8 cousine_18ptFontInfo[];	

//#define FONT      courierNew_36ptBitmaps  // прописать нужный шрифт
//#define FONT_info courierNew_36ptFontInfo // описание шрифта, размер и т.д. см.в конце файла шрифта
//extern const u8 courierNew_36ptBitmaps[];
//extern const u8 courierNew_36ptFontInfo[];

#endif /* FONT_H */

