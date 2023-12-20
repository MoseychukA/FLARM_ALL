/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
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

#include "stdio.h"
#include "usbh_rtlsdr.h"

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
#define USB_PIPE_NUMBER 0x81
#define KILOBYTES 1024
#define RAW_BUFFER_BYTES (25*KILOBYTES)
#define SIZEOF_DEMOD_BUF_EL 2
#define DEMOD_BUFF_BYTES (RAW_BUFFER_BYTES/SIZEOF_DEMOD_BUF_EL)
#define DOWNSAMPLE 15
#define RTL_SAMPLERATE 240000
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
#define BUTTON_WK_AP_Pin GPIO_PIN_0
#define BUTTON_WK_AP_GPIO_Port GPIOA
#define LED1_GREEN_Pin GPIO_PIN_6
#define LED1_GREEN_GPIO_Port GPIOA
#define LED2_RED_Pin GPIO_PIN_7
#define LED2_RED_GPIO_Port GPIOA
#define T_PEN_Pin GPIO_PIN_5
#define T_PEN_GPIO_Port GPIOC
#define T_PEN_EXTI_IRQn EXTI9_5_IRQn
#define LCD_BL_Pin GPIO_PIN_1
#define LCD_BL_GPIO_Port GPIOB
#define T_CS_Pin GPIO_PIN_12
#define T_CS_GPIO_Port GPIOB

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

//extern USBH_HandleTypeDef hUSBHost;
extern USBH_HandleTypeDef hUsbHostFS;

void Start_TFT(void);
void Test_TFT(void);
	
//uint8_t usb_device_ready1;
//uint8_t OutPipe1, InPipe1;
	
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
