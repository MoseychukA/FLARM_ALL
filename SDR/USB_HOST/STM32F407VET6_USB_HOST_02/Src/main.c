/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "fatfs.h"
#include "usb_host.h"

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

extern ApplicationTypeDef Appli_state;          // Переменная для определения состояния USB порта. 
extern USBH_HandleTypeDef hUsbHostFS;	
FATFS USBH_fatfs;
FIL MyFile; /* File object for upload operation */
FIL MyFileR;  /* File object for download operation */
extern DIR dir;
extern FILINFO fno;
char USBDISKPath[4];

#define DEMO_INIT       ((uint8_t)0x00)
#define DEMO_IAP        ((uint8_t)0x01)

__IO uint32_t UploadCondition = 0x00;

static uint8_t Demo_State = DEMO_INIT;
extern USBH_HandleTypeDef hUsbHostFS;
		
uint32_t JumpAddress;
pFunction Jump_To_Application;

static void COMMAND_ProgramFlashMemory(void);
static void IAP_UploadTimeout(void);
static void USBH_USR_BufferSizeControl(void);

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

#define BUFFER_SIZE        ((uint16_t)512*64)

#define UPLOAD_FILENAME      "0:UPLOAD_u.BIN"
static uint32_t TmpReadSize = 0x00;
static uint32_t RamAddress  = 0x00;
static __IO uint32_t LastPGAddress = APPLICATION_ADDRESS;
static uint8_t RAM_Buf[BUFFER_SIZE] = {0x00};
char DOWNLOAD_FILENAME[15];           // Переменная для хранения имени загружаемого bin файла

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
void MX_USB_HOST_Process(void);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
	
  FLASH_If_FlashUnlock();
	BSP_PB_Init(KEY0, BUTTON_MODE_GPIO);
	BSP_PB_Init(KEY1, BUTTON_MODE_GPIO);

	
	if (BSP_PB_GetState(KEY0) == GPIO_PIN_RESET)
	{
		sprintf(DOWNLOAD_FILENAME,"0:%s","image_u1.bin");
	}
	else if(BSP_PB_GetState(KEY1) == GPIO_PIN_RESET)
	{
		sprintf(DOWNLOAD_FILENAME,"0:%s","image_u2.bin");
	}
	else
  {
    /* Check Vector Table: Test if user code is programmed starting from address
    "APPLICATION_ADDRESS" */
    if ((((*(__IO uint32_t*)APPLICATION_ADDRESS) & 0xFF000000 ) == 0x20000000) || \
      (((*(__IO uint32_t*)APPLICATION_ADDRESS) & 0xFF000000 ) == 0x10000000))
    {
      /* Jump to user application */
      JumpAddress = *(__IO uint32_t*) (APPLICATION_ADDRESS + 4);
      Jump_To_Application = (pFunction) JumpAddress;
      /* Initialize user application's Stack Pointer */
      __set_MSP(*(__IO uint32_t*) APPLICATION_ADDRESS);
      Jump_To_Application();
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
  MX_USART2_UART_Init();
  MX_FATFS_Init();
  MX_USB_HOST_Init();
  /* USER CODE BEGIN 2 */

     // HAL_GPIO_WritePin(GPIOA, LED1_GREEN_Pin|LED2_RED_Pin, GPIO_PIN_RESET);
		//	HAL_GPIO_WritePin(GPIOA, LED1_GREEN_Pin|LED2_RED_Pin, GPIO_PIN_SET);
    //  BSP_LED_Off(LED2);  // RED
    //  BSP_LED_On(LED1);     // GREEN   

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
    MX_USB_HOST_Process();

    /* USER CODE BEGIN 3 */
		//FW_UPGRADE_Process();
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
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

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
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, LED1_GREEN_Pin|LED2_RED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : KEY1_Pin KEY0_Pin */
  GPIO_InitStruct.Pin = KEY1_Pin|KEY0_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pins : LED1_GREEN_Pin LED2_RED_Pin */
  GPIO_InitStruct.Pin = LED1_GREEN_Pin|LED2_RED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

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

void COMMAND_Upload(void)
{
  __IO uint32_t address = APPLICATION_ADDRESS;
  __IO uint32_t counterread = 0x00;
  uint32_t tmpcounter = 0x00, indexoffset = 0x00;
  FlagStatus readoutstatus = SET;
  uint16_t byteswritten;

  /* Get the read out protection status */
  readoutstatus = FLASH_If_ReadOutProtectionStatus();
  if(readoutstatus == RESET)
  {
    /* Remove UPLOAD file if it exists on flash disk */
    f_unlink(UPLOAD_FILENAME);

    /* Init written byte counter */
    indexoffset = (APPLICATION_ADDRESS - USER_FLASH_STARTADDRESS);

    /* Open binary file to write on it */
    if(( Appli_state == APPLICATION_READY) && (f_open(&MyFile, UPLOAD_FILENAME, FA_CREATE_ALWAYS | FA_WRITE) == FR_OK))
    {
      /* Upload On Going: Turn LED4 On and LED3 Off */
      BSP_LED_On(LED1);
      BSP_LED_Off(LED2);

      /* Read flash memory */
      while ((indexoffset < USER_FLASH_SIZE) && ( Appli_state == APPLICATION_READY))
      {
        for(counterread = 0; counterread < BUFFER_SIZE; counterread++)
        {
          /* Check the read bytes versus the end of flash */
          if(indexoffset + counterread < USER_FLASH_SIZE)
          {
            tmpcounter = counterread;
            RAM_Buf[tmpcounter] = (*(uint8_t*)(address++));
          }
          /* In this case all flash was read */
          else
          {
            break;
          }
        }

        /* Write buffer to file */
        f_write (&MyFile, RAM_Buf, BUFFER_SIZE, (void *)&byteswritten);

        /* Number of byte written  */
        indexoffset = indexoffset + counterread;
      }

      /* Turn LED1 On: Upload Done */
      BSP_LED_Off(LED2);
      BSP_LED_On(LED1);

      /* Close file and filesystem */
      f_close(&MyFile);
      f_mount(0, 0, 0);
    }
    /* Keep These LEDS OFF when Device connected */
    BSP_LED_Off(LED2);
    BSP_LED_Off(LED1);
  }
  else
  {
    /* Message ROP active: Turn LED2 On and Toggle LED3 in infinite loop */
    BSP_LED_On(LED2);
    Fail_Handler();
  }
}

/**
  * @brief  IAP Write Flash memory.
  * @param  None
  * @retval None
  */
void COMMAND_Download(void)
{
 
	if(f_mount(&USBH_fatfs,(TCHAR const*)USBDISKPath,0)==FR_OK)
	{
		printf("f_mount\r\n");
	 if(f_open(&MyFileR, DOWNLOAD_FILENAME, FA_READ) != FR_OK)
	 {
		 printf("Program flash memory not found \r\n");
		 FatFs_Fail_Handler();
	 }
	 else
	 {
			BSP_LED_Toggle(LED2);
			printf("Open the binary file\r\n");
			
		 if(f_size(&MyFileR) > USER_FLASH_SIZE)
			{
				/* No available Flash memory size for the binary file: Turn LED4 On and
					 Toggle LED3 in infinite loop */
				BSP_LED_On(LED2);
				Fail_Handler();
			}
			else
			{
				/* Download On Going: Turn LED4 On */
				BSP_LED_On(LED1);

				/* Erase FLASH sectors to download image */
				if(FLASH_If_EraseSectors(APPLICATION_ADDRESS) != 0x00)
				{
					/* Flash erase error: Turn LED4 On and Toggle LED2 and LED3 in
						 infinite loop */
					BSP_LED_Off(LED1);
					Erase_Fail_Handler();
				}
				printf("Program flash memory\r\n");
				BSP_LED_On(LED2);
				/* Program flash memory */
				COMMAND_ProgramFlashMemory();
				HAL_Delay(500);
				/* Download Done: Turn LED4 Off and LED2 On */
						 printf("Download Done\r\n");
				BSP_LED_Off(LED1);
				BSP_LED_On(LED2);

				/* Close file */
				FLASH_If_FlashLock();       // Блокирум флеш микроконтроллера
				f_close(&MyFileR);          // Закрываем файл с прошивкой
				printf("Close file\r\n");
			}
		}
  }
  else
  {
		printf("The f_mount falure\r\n");
    //Fail_Handler(); 
  }
}

/**
  * @brief  IAP jump to user program.
  * @param  None
  * @retval None
  */
void COMMAND_Jump(void)
{
  /* Software reset */
  NVIC_SystemReset();
}

/**
  * @brief  Programs the internal Flash memory.
  * @param  None
  * @retval None
  */
static void COMMAND_ProgramFlashMemory(void)
{
  uint32_t programcounter = 0x00;
  uint8_t readflag = TRUE;
  uint16_t bytesread;

  /* RAM Address Initialization */
  RamAddress = (uint32_t) &RAM_Buf;

  /* Erase address init */
  LastPGAddress = APPLICATION_ADDRESS;

  /* While file still contain data */
  while ((readflag == TRUE))
  {
    /* Read maximum 512 Kbyte from the selected file */
    f_read (&MyFileR, RAM_Buf, BUFFER_SIZE, (void *)&bytesread);

    /* Temp variable */
    TmpReadSize = bytesread;

    /* The read data < "BUFFER_SIZE" Kbyte */
    if(TmpReadSize < BUFFER_SIZE)
    {
      readflag = FALSE;
    }

   /* Program flash memory */
    for (programcounter = 0; programcounter < TmpReadSize; programcounter += 4)
    {
      /* Write word into flash memory */
      if (FLASH_If_Write((LastPGAddress + programcounter),
                         *(uint32_t *) (RamAddress + programcounter)) != 0x00)
      {
        /* Flash programming error: Turn LED2 On and Toggle LED3 in infinite
         * loop */
        BSP_LED_On(LED2);
        Fail_Handler();
      }
    }
    /* Update last programmed address value */
    LastPGAddress += TmpReadSize;
  }
}



void FW_UPGRADE_Process(void)
{

  switch(Demo_State)
  {
  case DEMO_INIT:
    /* Register the file system object to the FatFs module */
 	if (FATFS_LinkDriver(&USBH_Driver, USBDISKPath) == 0)
    {
      if (f_mount(&USBH_fatfs, "", 0) != FR_OK)
      {
        /* FatFs initialization fails */
        /* Toggle LED3 and LED4 in infinite loop */
        FatFs_Fail_Handler();
      }
    }

    /* Go to IAP menu */
    Demo_State = DEMO_IAP;
    break;

  case DEMO_IAP:
				BSP_LED_On(LED1);

    while(USBH_MSC_IsReady(&hUsbHostFS))
    {
      /* Control BUFFER_SIZE value */
      USBH_USR_BufferSizeControl();

      /* Keep LED1 and LED3 Off when Device connected */
      BSP_LED_Off(LED1);
      BSP_LED_Off(LED2);

      /* KEY Button pressed Delay */
      IAP_UploadTimeout();

      /* Writes Flash memory */
			printf("COMMAND_Download\n\r");
      COMMAND_Download();
      COMMAND_Jump();
      /* Check if KEY Button is already pressed */
      if((UploadCondition == 0x01))
      {
        /* Reads all flash memory */
				printf("COMMAND_Upload\n\r");
        COMMAND_Upload();
				printf("COMMAND_Upload  Ok!\n\r");
	    }
      else
      {
        /* Turn LED2 Off: Download Done */
        BSP_LED_Off(LED2);
        /* Turn LED1 On: Waiting KEY button pressed */
        BSP_LED_On(LED1);
      }

      /* Waiting KEY Button Released */
      while((BSP_PB_GetState(KEY0) == GPIO_PIN_RESET) && (Appli_state == APPLICATION_READY))
      {}

      /* Waiting KEY Button Pressed */
      while((BSP_PB_GetState(KEY0) != GPIO_PIN_RESET) && (Appli_state == APPLICATION_READY))
      {}

      /* Waiting KEY Button Released */
      while((BSP_PB_GetState(KEY0) == GPIO_PIN_RESET) && (Appli_state == APPLICATION_READY))
      {}

      if(Appli_state == APPLICATION_READY)
      {
        /* Jump to user application code located in the internal Flash memory */
        COMMAND_Jump();
      }
   // }
    break;

  default:
    break;
  }

 // FileRead();
  if(Appli_state == APPLICATION_DISCONNECT)
  {
    /* Toggle LED3: USB device disconnected */
    BSP_LED_Toggle(LED2);
    HAL_Delay(100);
  }
 }
}

static void IAP_UploadTimeout(void)
{
  /* Check if KEY button is pressed */
  if(BSP_PB_GetState(KEY0) == GPIO_PIN_RESET)
  {
    /* To execute the UPLOAD command the KEY button should be kept pressed 3s
       just after a board reset, at firmware startup */
    HAL_Delay (3000);

    if(BSP_PB_GetState(KEY0) == GPIO_PIN_RESET)
    {
      /* UPLOAD command will be executed immediately after
      completed execution of the DOWNLOAD command */

      UploadCondition = 0x01;

      /* Turn LED3 on : Upload condition Verified */
      BSP_LED_On(LED2);
    }
    else
    {
      /* Only the DOWNLOAD command is executed */
      UploadCondition = 0x00;
    }
  }
}




void Fail_Handler(void)
{
  while(1)
  {
    /* Toggle LED3 */
    BSP_LED_Toggle(LED2);
    HAL_Delay(100);
  }
}

/**
  * @brief  Handles the Flash Erase fail.
  * @param  None
  * @retval None
  */
void Erase_Fail_Handler(void)
{
  while(1)
  {
    /* Toggle LED2 and LED3 */
    BSP_LED_Toggle(LED2);
    BSP_LED_Toggle(LED1);
    HAL_Delay(100);
  }
}

/**
  * @brief  Handles the FatFs fail.
  * @param  None
  * @retval None
  */
void FatFs_Fail_Handler(void)
{
  while(1)
  {
    /* Toggle LED3 and LED4 */
    BSP_LED_Toggle(LED1);
    BSP_LED_Toggle(LED2);
    HAL_Delay(300);
  }
}

/**
  * @brief  Controls Buffer size value.
  * @param  None
  * @retval None
  */
static void USBH_USR_BufferSizeControl(void)
{
  /* Control BUFFER_SIZE and limit this value to 32Kbyte maximum */
  if((BUFFER_SIZE % 4 != 0x00) || (BUFFER_SIZE / 4 > 8192))
  {
    while(1)
    {
      /* Toggle LED2, LED3 and LED4 */
      BSP_LED_Toggle(LED2);
      BSP_LED_Toggle(LED1);
      HAL_Delay(100);
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
  __disable_irq();
  while (1)
  {
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
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
