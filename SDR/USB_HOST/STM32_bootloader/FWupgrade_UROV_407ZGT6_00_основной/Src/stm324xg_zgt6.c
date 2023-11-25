/**
  ******************************************************************************
  * @file    stm324xg_eval.c
  * @author  MCD Application Team
  * @brief   This file provides a set of firmware functions to manage LEDs, 
  *          push-buttons and COM ports available on STM324xG-EVAL evaluation 
  *          board(MB786) RevB from STMicroelectronics.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT(c) 2017 STMicroelectronics</center></h2>
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */ 
  
/* File Info: ------------------------------------------------------------------
                                   User NOTE

   This driver requires the stm324xG_eval_io.c driver to manage the joystick

------------------------------------------------------------------------------*/

/* Includes ------------------------------------------------------------------*/
#include "stm324xg_zgt6.h"


/**
  * @brief STM324xG EVAL BSP Driver version number V3.0.2
  */
#define __STM324xG_EVAL_BSP_VERSION_MAIN   (0x03) /*!< [31:24] main version */
#define __STM324xG_EVAL_BSP_VERSION_SUB1   (0x00) /*!< [23:16] sub1 version */
#define __STM324xG_EVAL_BSP_VERSION_SUB2   (0x02) /*!< [15:8]  sub2 version */
#define __STM324xG_EVAL_BSP_VERSION_RC     (0x00) /*!< [7:0]  release candidate */ 
#define __STM324xG_EVAL_BSP_VERSION         ((__STM324xG_EVAL_BSP_VERSION_MAIN << 24)\
                                             |(__STM324xG_EVAL_BSP_VERSION_SUB1 << 16)\
                                             |(__STM324xG_EVAL_BSP_VERSION_SUB2 << 8 )\
                                             |(__STM324xG_EVAL_BSP_VERSION_RC))



/** @defgroup STM324xG_EVAL_LOW_LEVEL_Private_Variables STM324xG EVAL LOW LEVEL Private Variables
  * @{
  */ 
GPIO_TypeDef* GPIO_PORT[LEDn] = {LED1_GPIO_PORT, 
	                               LED2_GPIO_PORT, 
                                 LED3_GPIO_PORT};

const uint16_t GPIO_PIN[LEDn] = {LED1_PIN, 
	                               LED2_PIN, 
                                 LED3_PIN};

GPIO_TypeDef* BUTTON_PORT[BUTTONn] = {BUTTON_KEY1_GPIO_PORT, 
                                      BUTTON_KEY2_GPIO_PORT,
                                      BUTTON_KEY3_GPIO_PORT}; 

const uint16_t BUTTON_PIN[BUTTONn] = {BUTTON_KEY1_PIN, 
                                      BUTTON_KEY2_PIN,
                                      BUTTON_KEY3_PIN}; 

const uint16_t BUTTON_IRQn[BUTTONn] = {BUTTON_KEY1_EXTI_IRQn, 
                                       BUTTON_KEY2_EXTI_IRQn,
                                       BUTTON_KEY3_EXTI_IRQn};


/**
  * @brief  This method returns the STM324xG EVAL BSP Driver revision
  * @retval version: 0xXYZR (8bits for each decimal, R for RC)
  */
uint32_t BSP_GetVersion(void)
{
  return __STM324xG_EVAL_BSP_VERSION;
}

/**
  * @brief  Configures LED GPIO.
  * @param  Led: LED to be configured. 
  *          This parameter can be one of the following values:
  *            @arg  LED1
  *            @arg  LED2
 
  */
void BSP_LED_Init(Led_TypeDef Led)
{
  GPIO_InitTypeDef  GPIO_InitStruct;
  
  /* Enable the GPIO_LED clock */
  LEDx_GPIO_CLK_ENABLE(Led);

  /* Configure the GPIO_LED pin */
  GPIO_InitStruct.Pin = GPIO_PIN[Led];
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FAST;
  
  HAL_GPIO_Init(GPIO_PORT[Led], &GPIO_InitStruct);
	HAL_GPIO_WritePin(GPIO_PORT[Led], GPIO_PIN[Led], GPIO_PIN_RESET); 
	
}

/**
  * @brief  Turns selected LED On.
  * @param  Led: LED to be set on 
  *          This parameter can be one of the following values:
  *            @arg  LED1
  *            @arg  LED2
  
  */
void BSP_LED_On(Led_TypeDef Led)
{
  HAL_GPIO_WritePin(GPIO_PORT[Led], GPIO_PIN[Led], GPIO_PIN_SET); 
}

/**
  * @brief  Turns selected LED Off. 
  * @param  Led: LED to be set off
  *          This parameter can be one of the following values:
  *            @arg  LED1
  *            @arg  LED2
 
  */
void BSP_LED_Off(Led_TypeDef Led)
{
  HAL_GPIO_WritePin(GPIO_PORT[Led], GPIO_PIN[Led], GPIO_PIN_RESET); 
}

/**
  * @brief  Toggles the selected LED.
  * @param  Led: LED to be toggled
  *          This parameter can be one of the following values:
  *            @arg  LED1
  *            @arg  LED2
  
  */
void BSP_LED_Toggle(Led_TypeDef Led)
{
  HAL_GPIO_TogglePin(GPIO_PORT[Led], GPIO_PIN[Led]);
}

/**
  * @brief  Configures button GPIO and EXTI Line.
  * @param  Button: Button to be configured
  *          This parameter can be one of the following values:
  *            @arg  BUTTON_WAKEUP: Wakeup Push Button 
  *            @arg  BUTTON_TAMPER: Tamper Push Button
  *            @arg  BUTTON_KEY: Key Push Button 
  *            @arg  BUTTON_RIGHT: Joystick Right Push Button 
  *            @arg  BUTTON_LEFT: Joystick Left Push Button 
  *            @arg  BUTTON_UP: Joystick Up Push Button 
  *            @arg  BUTTON_DOWN: Joystick Down Push Button
  *            @arg  BUTTON_SEL: Joystick Sel Push Button
  * @param  Button_Mode: Button mode
  *          This parameter can be one of the following values:
  *            @arg  BUTTON_MODE_GPIO: Button will be used as simple IO
  *            @arg  BUTTON_MODE_EXTI: Button will be connected to EXTI line 
  *                                    with interrupt generation capability  
  */
void BSP_PB_Init(Button_TypeDef Button, ButtonMode_TypeDef Button_Mode)
{
  GPIO_InitTypeDef GPIO_InitStruct;
  
  /* Enable the BUTTON clock */
  BUTTONx_GPIO_CLK_ENABLE(Button);
	//__HAL_RCC_GPIOE_CLK_ENABLE();
  
  if(Button_Mode == BUTTON_MODE_GPIO)
  {
    /* Configure Button pin as input */
    GPIO_InitStruct.Pin = BUTTON_PIN[Button];
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FAST;
    
    HAL_GPIO_Init(BUTTON_PORT[Button], &GPIO_InitStruct);
  }
  
//  if(Button_Mode == BUTTON_MODE_EXTI)
//  {
//    /* Configure Button pin as input with External interrupt */
//    GPIO_InitStruct.Pin = BUTTON_PIN[Button];
//    GPIO_InitStruct.Pull = GPIO_NOPULL;
//    GPIO_InitStruct.Speed = GPIO_SPEED_FAST;
//    
//    if(Button != BUTTON_KEY1)
//    {
//      GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING; 
//    }
//    else
//    {
//      GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
//    }
//    
//    HAL_GPIO_Init(BUTTON_PORT[Button], &GPIO_InitStruct);
//    
//    /* Enable and set Button EXTI Interrupt to the lowest priority */
//    HAL_NVIC_SetPriority((IRQn_Type)(BUTTON_IRQn[Button]), 0x0F, 0x0);
//    HAL_NVIC_EnableIRQ((IRQn_Type)(BUTTON_IRQn[Button]));
//  }
}



uint32_t BSP_PB_GetState(Button_TypeDef Button)
{
  return HAL_GPIO_ReadPin(BUTTON_PORT[Button], BUTTON_PIN[Button]);
}



/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
