/**
  ******************************************************************************
  * @file    stm32f407black_zgt6.h
  * @author  MCD Application Team
  * @brief   This file contains definitions for STM324xG_EVAL's LEDs, 
  *          push-buttons and COM ports hardware resources.
  ******************************************************************************
  * @attention
  *
 
  *
  ******************************************************************************
  */ 
  
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __STM32F407UROV_VGT6_H  //stm32f407black_zgt6
#define __STM32F407UROV_VGT6_H

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include "main.h"
   
typedef enum 
{
  LED1 = 0,
  LED2 = 1,
}Led_TypeDef;

typedef enum 
{  
  KEY0 = 0,
  KEY1 = 1,
 }Button_TypeDef;

typedef enum 
{  
  BUTTON_MODE_GPIO = 0,
  BUTTON_MODE_EXTI = 1
}ButtonMode_TypeDef;
  


/** 
  * @brief  Define for STM324xG_EVAL board  
  */ 
#if !defined (USE_STM324xG_EVAL)
 #define USE_STM324xG_EVAL
#endif

/** @addtogroup STM324xG_EVAL_LOW_LEVEL_LED STM324xG EVAL LOW LEVEL LED
  * @{
  */
#define LEDn                             2

#define LED1_GREEN_Pin_CLK_ENABLE()           __HAL_RCC_GPIOA_CLK_ENABLE()
#define LED1_GREEN_Pin_CLK_DISABLE()          __HAL_RCC_GPIOA_CLK_DISABLE()
  
#define LED2_RED_Pin_CLK_ENABLE()           __HAL_RCC_GPIOA_CLK_ENABLE()
#define LED2_RED_Pin_CLK_DISABLE()          __HAL_RCC_GPIOA_CLK_DISABLE()

 

#define LEDx_GPIO_CLK_ENABLE(__INDEX__)  do{if((__INDEX__) == 0) LED1_GREEN_Pin_CLK_ENABLE(); else \
                                            if((__INDEX__) == 1) LED2_RED_Pin_CLK_ENABLE(); \
	                                          }while(0)

																						
																						
#define LEDx_GPIO_CLK_DISABLE(__INDEX__) do{if((__INDEX__) == 0) LED1_GREEN_Pin_CLK_ENABLE(); else \
	                                          if((__INDEX__) == 1) LED2_RED_Pin_CLK_ENABLE(); \
                                            }while(0)
/**
  * @}
  */ 
  
/** @addtogroup STM324xG_EVAL_LOW_LEVEL_BUTTON STM324xG EVAL LOW LEVEL BUTTON
  * @{
  */  
/* Joystick pins are connected to IO Expander (accessible through I2C1 interface) */
#define BUTTONn                              2

/**
  * @brief Wakeup push-button
  */

#define KEY0_Pin_GPIO_CLK_ENABLE()     __HAL_RCC_GPIOE_CLK_ENABLE()
#define KEY0_Pin_GPIO_CLK_DISABLE()    __HAL_RCC_GPIOE_CLK_DISABLE()
#define KEY0_Pin_EXTI_IRQn               EXTI15_10_IRQn

/**
  * @brief Tamper push-button
  */
#define KEY1_Pin_GPIO_CLK_ENABLE()      __HAL_RCC_GPIOE_CLK_ENABLE()
#define KEY1_Pin_GPIO_CLK_DISABLE()     __HAL_RCC_GPIOE_CLK_DISABLE()
#define KEY1_Pin_EXTI_IRQn                EXTI15_10_IRQn

/**
  * @brief Key push-button
  */

//#define BUTTON_3_Pin_GPIO_CLK_ENABLE()         __HAL_RCC_GPIOE_CLK_ENABLE()
//#define BUTTON_3_Pin_GPIO_CLK_DISABLE()        __HAL_RCC_GPIOE_CLK_DISABLE()
//#define BUTTON_3_Pin_EXTI_IRQn                   EXTI15_10_IRQn

#define BUTTONx_GPIO_CLK_ENABLE(__INDEX__)  do{if((__INDEX__) == 0) KEY0_Pin_GPIO_CLK_ENABLE(); else \
                                               if((__INDEX__) == 2) KEY1_Pin_GPIO_CLK_ENABLE(); \
                                               }while(0)
#define BUTTONx_GPIO_CLK_DISABLE(__INDEX__) do{if((__INDEX__) == 0) KEY0_Pin_GPIO_CLK_DISABLE(); else \
                                               if((__INDEX__) == 2) KEY1_Pin_GPIO_CLK_DISABLE(); \
                                               }while(0)


/** @defgroup STM324xG_EVAL_LOW_LEVEL_Exported_Functions STM324xG EVAL LOW LEVEL Exported Functions
  * @{
  */

//void             BSP_LED_Init(Led_TypeDef Led);
void             BSP_LED_On(Led_TypeDef Led);
void             BSP_LED_Off(Led_TypeDef Led);
void             BSP_LED_Toggle(Led_TypeDef Led);
void             BSP_PB_Init(Button_TypeDef Button, ButtonMode_TypeDef Button_Mode);
uint32_t         BSP_PB_GetState(Button_TypeDef Button);



#ifdef __cplusplus
}
#endif

#endif /* __STM324xG_EVAL_H */
 
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
