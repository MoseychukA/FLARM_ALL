/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "fatfs.h"
#include "usb_host.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "stdint.h"
#include "string.h"
#include <stdlib.h>
#include <stdio.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

GPIO_TypeDef* GPIO_PORT[LEDn] = {LED1_GREEN_GPIO_Port, 
                                 LED2_RED_GPIO_Port};

const uint16_t GPIO_PIN[LEDn] = {LED1_GREEN_Pin, 
	                               LED2_RED_Pin};

void BSP_LED_On(Led_TypeDef Led)
{
  HAL_GPIO_WritePin(GPIO_PORT[Led], GPIO_PIN[Led], GPIO_PIN_RESET); 
}

void BSP_LED_Off(Led_TypeDef Led)
{
  HAL_GPIO_WritePin(GPIO_PORT[Led], GPIO_PIN[Led], GPIO_PIN_SET); 
}
			
void BSP_LED_Toggle(Led_TypeDef Led)
{
  HAL_GPIO_TogglePin(GPIO_PORT[Led], GPIO_PIN[Led]);
}
					
GPIO_TypeDef* BUTTON_PORT[BUTTONn] = {BUTTON_WK_AP_GPIO_Port, 
                                      BUTTON_K0_GPIO_Port,
                                      BUTTON_K1_GPIO_Port}; 

const uint16_t BUTTON_PIN[BUTTONn] = {BUTTON_WK_AP_Pin, 
                                      BUTTON_K0_Pin,
                                      BUTTON_K1_Pin}; 

const uint16_t BUTTON_IRQn[BUTTONn] = {BUTTON_WK_AP_EXTI_IRQn, 
                                       BUTTON_K0_EXTI_IRQn,
                                       BUTTON_K1_EXTI_IRQn};


uint32_t BSP_PB_GetState(Button_TypeDef Button)
{
  return HAL_GPIO_ReadPin(BUTTON_PORT[Button], BUTTON_PIN[Button]);
}

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */

extern ApplicationTypeDef Appli_state;
FATFS USBDISKFatFs;
FIL MyFile;
extern USBH_HandleTypeDef hUsbHostFS;
char USBH_Path[4];          /* USB Host logical drive path */


/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
void MX_USB_HOST_Process(void);

/* USER CODE BEGIN PFP */
	int file_write = FALSE;
	int file_read = FALSE;
	
	#ifdef __GNUC__
  /* With GCC, small printf (option LD Linker->Libraries->Small printf
     set to 'Yes') calls __io_putchar() */
  #define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
  #define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif /* __GNUC__ */
	

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void FileWrite(void)
{
	FRESULT res;
	uint32_t byteswritten;
	uint8_t rtext[100];
	uint8_t wtext[]="Hello from STM32yy!\n\r";

	if (file_write == FALSE && file_read == FALSE)
	{
		if(f_mount(&USBDISKFatFs,(TCHAR const*)USBH_Path,0)!=FR_OK)
		{
			Error_Handler();
		}
		else
		{
			if(f_open(&MyFile,"1234.txt",FA_CREATE_ALWAYS|FA_WRITE)!=FR_OK)
			{
				Error_Handler();
			}
			else
			{
				BSP_LED_On(LED1);
				res = f_write(&MyFile,wtext,sizeof(wtext),(void*)byteswritten);
				BSP_LED_Off(LED2);
				if((byteswritten==0)||(res!=FR_OK))
				{
					Error_Handler();
				}
				else
				{
					BSP_LED_On(LED1);
					file_write = TRUE;
					printf("f_close\r\n");
					f_close(&MyFile);
					HAL_Delay(1000);
					BSP_LED_Off(LED1);
				}
			}
		}
	}
	

}

void FileRead(void)
{
	FRESULT res;
	uint32_t bytesread;
	uint8_t rtext[100];

		//Read
		
	if (file_write == TRUE && file_read == FALSE)
	{

		if(f_mount(&USBDISKFatFs,(TCHAR const*)/*USBH_Path*/0,0)!=FR_OK)
		{
			Error_Handler();
		}
		else
		{
			if(f_open(&MyFile,"1234.txt",FA_READ)!=FR_OK)
			{
				Error_Handler();
			}
			else
			{
				res = f_read(&MyFile,rtext,sizeof(rtext),(void*)bytesread);
				if((bytesread==0)||(res!=FR_OK))
				{
					Error_Handler();
				}
				else
				{
					HAL_UART_Transmit(&huart1,rtext,20,0xFFFF);
					//rtext[bytesread]=0;//
					HAL_Delay(100);
					f_close(&MyFile);
					BSP_LED_On(LED1);
					HAL_Delay(500);
					file_read = TRUE;
					BSP_LED_Off(LED1);
					printf("\r\nf_close\r\n");
				}
			}
	  }
  }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_FATFS_Init();
  MX_USB_HOST_Init();
  /* USER CODE BEGIN 2 */

	BSP_LED_Off(LED1);
	BSP_LED_Off(LED2); 
	HAL_Delay(200);
	BSP_LED_On(LED1);
	BSP_LED_On(LED2);
	HAL_Delay(200);
	BSP_LED_Off(LED1);
	BSP_LED_Off(LED2); 
	printf("Start system\r\n");

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
    MX_USB_HOST_Process();

    /* USER CODE BEGIN 3 */
		
		if(Appli_state==APPLICATION_START)
		{
			FileWrite();
			FileRead();
			Appli_state=APPLICATION_IDLE;
			
		}
		else if(Appli_state==APPLICATION_IDLE)
		{
		}
		
		
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOF, LED1_GREEN_Pin|LED2_RED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : BUTTON_K1_Pin BUTTON_K0_Pin */
  GPIO_InitStruct.Pin = BUTTON_K1_Pin|BUTTON_K0_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pins : LED1_GREEN_Pin LED2_RED_Pin */
  GPIO_InitStruct.Pin = LED1_GREEN_Pin|LED2_RED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

  /*Configure GPIO pin : BUTTON_WK_AP_Pin */
  GPIO_InitStruct.Pin = BUTTON_WK_AP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BUTTON_WK_AP_GPIO_Port, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */


PUTCHAR_PROTOTYPE
{
  /* Place your implementation of fputc here */
  /* e.g. write a character to the EVAL_COM1 and Loop until the end of transmission */
  HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 0xFFFF); 

  return ch;
}


/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
   BSP_LED_Toggle(LED2);
	 HAL_Delay(100);
	  BSP_LED_Toggle(LED2);
	 HAL_Delay(100);
	  BSP_LED_Toggle(LED2);
	 HAL_Delay(100);
	 BSP_LED_Toggle(LED2);
	//	BSP_LED_On(LED2); 
//  while(1)
//  {
//  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
