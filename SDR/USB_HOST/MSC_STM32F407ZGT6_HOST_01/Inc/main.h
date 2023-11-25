/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */
typedef enum 
{
  LED1 = 0,
  LED2 = 1
}Led_TypeDef;
	
typedef enum 
{  
  BUTTON_WAKEUP = 0,
  BUTTON_KEY0   = 1,
  BUTTON_KEY1   = 2
}Button_TypeDef;

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define BUTTON_K1_Pin GPIO_PIN_3
#define BUTTON_K1_GPIO_Port GPIOE
#define BUTTON_K0_Pin GPIO_PIN_4
#define BUTTON_K0_GPIO_Port GPIOE
#define LED1_GREEN_Pin GPIO_PIN_9
#define LED1_GREEN_GPIO_Port GPIOF
#define LED2_RED_Pin GPIO_PIN_10
#define LED2_RED_GPIO_Port GPIOF
#define BUTTON_WK_AP_Pin GPIO_PIN_0
#define BUTTON_WK_AP_GPIO_Port GPIOA

/* USER CODE BEGIN Private defines */

#define BUTTON_WK_AP_EXTI_IRQn         EXTI0_IRQn
#define BUTTON_K0_EXTI_IRQn            EXTI15_10_IRQn
#define BUTTON_K1_EXTI_IRQn            EXTI15_10_IRQn

#define LEDn                             2
#define BUTTONn                          3

void             BSP_LED_On(Led_TypeDef Led);
void             BSP_LED_Off(Led_TypeDef Led);
void             BSP_LED_Toggle(Led_TypeDef Led);
uint32_t         BSP_PB_GetState(Button_TypeDef Button);



//#define LED1_ON  HAL_GPIO_WritePin(LED1_GREEN_GPIO_Port,LED1_GREEN_Pin, GPIO_PIN_RESET); //  
//#define LED1_OFF HAL_GPIO_WritePin(LED1_GREEN_GPIO_Port,LED1_GREEN_Pin, GPIO_PIN_SET); 

//#define LED2_ON  HAL_GPIO_WritePin(LED2_RED_GPIO_Port,LED2_RED_Pin, GPIO_PIN_RESET); //  
//#define LED2_OFF HAL_GPIO_WritePin(LED2_RED_GPIO_Port,LED2_RED_Pin, GPIO_PIN_SET); 

//#define LED1_Toggle  HAL_GPIO_TogglePin(LED1_GREEN_GPIO_Port,LED1_GREEN_Pin);
//#define LED2_Toggle  HAL_GPIO_TogglePin(LED2_RED_GPIO_Port,LED2_RED_Pin);




/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
