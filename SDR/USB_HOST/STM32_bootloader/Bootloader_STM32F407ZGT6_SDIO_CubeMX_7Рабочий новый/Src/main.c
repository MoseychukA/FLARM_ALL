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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "stdint.h"   // устанавливает типы переменных типа int8_t и.т.д.
#include "string.h"   // Работа со строками
#include <stdlib.h>   // объявляет четыре типа, несколько функций общего назначения,и определяет несколько макросов.
#include <stdio.h>    // объявляет два типа, несколько макросов и множество функций для выполнение ввода и вывода.

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* Вывод строки в КОМ порт  */
	#ifdef __GNUC__
  /* With GCC, small printf (option LD Linker->Libraries->Small printf
     set to 'Yes') calls __io_putchar() */
  #define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
  #define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif /* __GNUC__ */
	
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SD_HandleTypeDef hsd;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_SDIO_SD_Init(void);
/* USER CODE BEGIN PFP */

void SD_UPGRADE_Process(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

FATFS SDFatFs;
FIL MyFile;
char SD_Path[4];  /* SD logical drive path */

uint32_t JumpAddress;                                     // Переменная для хранения адреса выполнения основной программы
pFunction Jump_To_Application;                            // Функция перехода на выполнение основной программы
static __IO uint32_t LastPGAddress = APPLICATION_ADDRESS; // Адрес окончания программы загрузчика.
char DOWNLOAD_FILENAME[15];                               // Переменная для хранения пути и имени загружаемого bin файла

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
	BSP_PB_Init(BUTTON_KEY0, BUTTON_MODE_GPIO);
	BSP_PB_Init(BUTTON_KEY1, BUTTON_MODE_GPIO);

	if (BSP_PB_GetState(BUTTON_KEY0) == GPIO_PIN_RESET)
  {
		sprintf(DOWNLOAD_FILENAME,"0:%s","image1.bin");
	}
	else if(BSP_PB_GetState(BUTTON_KEY1) == GPIO_PIN_RESET)
	{
		sprintf(DOWNLOAD_FILENAME,"0:%s","image2.bin"); 
	}
	else
  {
    /* Check Vector Table: Test if user code is programmed starting from address  "APPLICATION_ADDRESS" */
	  /* Проверить таблицу векторов: проверьте, запрограммирован ли код пользователя, начиная с адреса «APPLICATION_ADDRESS» */
    if ((((*(__IO uint32_t*)APPLICATION_ADDRESS) & 0xFF000000 ) == 0x20000000) || \
       (((*(__IO uint32_t*)APPLICATION_ADDRESS) & 0xFF000000 ) == 0x10000000)) 
    {
      /* Jump to user application */
      JumpAddress = *(__IO uint32_t*) (APPLICATION_ADDRESS + 4);     //извлекаем адрес перехода из вектора Reset
      Jump_To_Application = (pFunction) JumpAddress;                // приводим его к пользовательскому типу
       /* Initialize user application's Stack Pointer */
	     /* �?нициализировать указатель стека пользовательского приложения */
      __set_MSP(*(__IO uint32_t*) APPLICATION_ADDRESS);  //  устанавливаем SP приложения 
      Jump_To_Application();                             //запускаем приложение	
    }
  }
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
  MX_SDIO_SD_Init();
  MX_FATFS_Init();
  /* USER CODE BEGIN 2 */

  printf("\n\r UART Printf Example: retarget the C library printf function to the UART\n\r");
	BSP_LED_Off(LED1);
	BSP_LED_Off(LED2);

//	HAL_Delay(500);
//	BSP_LED_On(LED1);
//	BSP_LED_On(LED2);
//	HAL_Delay(500);
//	BSP_LED_Off(LED1);
//	BSP_LED_Off(LED2);
//	HAL_Delay(500);
//	BSP_LED_On(LED1);
//	BSP_LED_On(LED2);
//	HAL_Delay(500);
//	BSP_LED_Off(LED1);
//	BSP_LED_Off(LED2);
//	HAL_Delay(500);
SD_UPGRADE_Process();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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
  * @brief SDIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_SDIO_SD_Init(void)
{

  /* USER CODE BEGIN SDIO_Init 0 */

  /* USER CODE END SDIO_Init 0 */

  /* USER CODE BEGIN SDIO_Init 1 */

  /* USER CODE END SDIO_Init 1 */
  hsd.Instance = SDIO;
  hsd.Init.ClockEdge = SDIO_CLOCK_EDGE_RISING;
  hsd.Init.ClockBypass = SDIO_CLOCK_BYPASS_DISABLE;
  hsd.Init.ClockPowerSave = SDIO_CLOCK_POWER_SAVE_DISABLE;
  hsd.Init.BusWide = SDIO_BUS_WIDE_4B;
  hsd.Init.HardwareFlowControl = SDIO_HARDWARE_FLOW_CONTROL_DISABLE;
  hsd.Init.ClockDiv = 0;
  /* USER CODE BEGIN SDIO_Init 2 */

  /* USER CODE END SDIO_Init 2 */

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
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

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

/* Вывод строки в КОМ порт  */
PUTCHAR_PROTOTYPE
{
  /* Place your implementation of fputc here */
  /* e.g. write a character to the EVAL_COM1 and Loop until the end of transmission */
  HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 0xFFFF); 

  return ch;
} 

void SD_UPGRADE_Process(void)
{
	if(f_mount(&SDFatFs,(TCHAR const*)SD_Path,0)!=FR_OK)
	{
		Error_Handler();
	}
	else
	{
		// Проверяем наличие файла прошивки на SD карте
	 if(f_open(&MyFile, DOWNLOAD_FILENAME, FA_READ) == FR_OK) 
		{
			printf("The binary file:%s",DOWNLOAD_FILENAME);   // Файл прошивки обнаружен. Начинаем прошивку.
			printf(" is available! \r\n");
			BSP_LED_On(LED1);
			BSP_LED_On(LED2);
			printf("COMMAND_Download\n\r");
			COMMAND_Download();
		}
		else
		{
			 printf("Program flash memory not found \n");
			 printf("The binary file is not available \r\n");
			 Error_Handler();
		}
	}
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
   while(1)
    {
      /* Toggle LED2, LED3 and LED4 */
      BSP_LED_Toggle(LED2);
      BSP_LED_Toggle(LED1);
      HAL_Delay(300);
    }
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
