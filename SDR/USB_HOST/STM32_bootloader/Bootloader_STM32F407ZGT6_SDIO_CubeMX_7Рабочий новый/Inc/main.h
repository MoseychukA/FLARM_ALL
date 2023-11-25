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
#include "stm32f407black_zgt6.h"
#include "flash_if.h"
#include "command.h"

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

#ifndef NULL
#define NULL  0U
#endif

#ifndef FALSE
#define FALSE 0U
#endif

#ifndef TRUE
#define TRUE  1U
#endif
/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */
#define BUFFER_SIZE        ((uint16_t)512*64)     // Размер буфера для временного хранения блоков при чтении файла из SD карты. 

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
void SD_UPGRADE_Process(void);

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
