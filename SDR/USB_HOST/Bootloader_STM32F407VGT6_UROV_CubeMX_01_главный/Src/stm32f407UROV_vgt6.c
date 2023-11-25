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
  *
  ******************************************************************************
  */ 
  
/* File Info: ------------------------------------------------------------------
                                   User NOTE

   This driver requires the stm324xG_eval_io.c driver to manage the joystick

------------------------------------------------------------------------------*/

/* Includes ------------------------------------------------------------------*/
#include "stm32f407UROV_vgt6.h"



/** @defgroup STM324xG_EVAL_LOW_LEVEL_Private_Variables STM324xG EVAL LOW LEVEL Private Variables
  * @{
  */ 
GPIO_TypeDef* GPIO_PORT[LEDn] = {LED1_GREEN_GPIO_Port, 
	                               LED2_RED_GPIO_Port, 
                                 LED3_YELLOW_GPIO_Port};

const uint16_t GPIO_PIN[LEDn] = {LED1_GREEN_Pin, 
	                               LED2_RED_Pin, 
                                 LED3_YELLOW_Pin};

GPIO_TypeDef* BUTTON_PORT[BUTTONn] = {BUTTON_1_GPIO_Port, 
                                      BUTTON_2_GPIO_Port,
                                      BUTTON_3_GPIO_Port}; 

const uint16_t BUTTON_PIN[BUTTONn] = {BUTTON_1_Pin, 
                                      BUTTON_2_Pin,
                                      BUTTON_3_Pin}; 

const uint16_t BUTTON_IRQn[BUTTONn] = {BUTTON_1_Pin_EXTI_IRQn, 
                                       BUTTON_2_Pin_EXTI_IRQn,
                                       BUTTON_3_Pin_EXTI_IRQn};



/**
  * @brief  Configures LED GPIO.
  * @param  Led: LED to be configured. 
  *          This parameter can be one of the following values:
  *            @arg  LED1
  *            @arg  LED2
 
  */
//void BSP_LED_Init(Led_TypeDef Led)
//{
//  GPIO_InitTypeDef  GPIO_InitStruct;
//  
//  /* Enable the GPIO_LED clock */
//  LEDx_GPIO_CLK_ENABLE(Led);

//  /* Configure the GPIO_LED pin */
//  GPIO_InitStruct.Pin = GPIO_PIN[Led];
//  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
//  GPIO_InitStruct.Pull = GPIO_PULLUP;
//  GPIO_InitStruct.Speed = GPIO_SPEED_FAST;
//  
//  HAL_GPIO_Init(GPIO_PORT[Led], &GPIO_InitStruct);
//	HAL_GPIO_WritePin(GPIO_PORT[Led], GPIO_PIN[Led], GPIO_PIN_SET); 
//	
//}

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
//    if(Button != BUTTON_WAKEUP)
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
