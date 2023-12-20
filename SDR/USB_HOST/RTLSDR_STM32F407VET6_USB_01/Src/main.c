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
#include "usb_host.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stdint.h"
#include "string.h"
#include <stdlib.h>
#include <stdio.h>

#include "XPT2046_touch.h"
#include "ili9341.h"


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
SPI_HandleTypeDef hspi2;

TIM_HandleTypeDef htim3;

UART_HandleTypeDef huart1;

SRAM_HandleTypeDef hsram1;

/* USER CODE BEGIN PV */
	#ifdef __GNUC__
  /* With GCC, small printf (option LD Linker->Libraries->Small printf
     set to 'Yes') calls __io_putchar() */
  #define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
  #define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif /* __GNUC__ */
	
	
//static void USBH_UserProcess(USBH_HandleTypeDef *phost, uint8_t id);
static void RTLSDR_InitApplication(void);
	
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_FSMC_Init(void);
static void MX_SPI2_Init(void);
static void MX_TIM3_Init(void);
void MX_USB_HOST_Process(void);

/* USER CODE BEGIN PFP */

	uint16_t x = 0, y = 0;
	uint16_t xx = 0, y1 = 0;
	 
 char str1[20]; 
 char str2[20]; 
 char str3[20];
 char str4[20];
 char str[20];
 bool flag1;
 uint16_t strw;

uint8_t usb_device_ready;
uint8_t OutPipe, InPipe;




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
  MX_USB_HOST_Init();
  MX_FSMC_Init();
  MX_SPI2_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */

	printf("Start system\r\n");

	 /* Init RTLSDR Application */
  RTLSDR_InitApplication();
	
	printf("Start system END\r\n");
	
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
    MX_USB_HOST_Process();

    /* USER CODE BEGIN 3 */
			
		  if (hUsbHostFS.gState == HOST_CHECK_CLASS&& !usb_device_ready) 
			{
					// attempt a connection on the control endpoint
					// note: max packet size 64 bytes for FS and 512 bytes for HS
					InPipe = USBH_AllocPipe(&hUsbHostFS, USB_PIPE_NUMBER);
					USBH_StatusTypeDef status = USBH_OpenPipe(&hUsbHostFS,
																			InPipe,
																			USB_PIPE_NUMBER,
																			hUsbHostFS.device.address,
																			hUsbHostFS.device.speed,
																			USB_EP_TYPE_BULK,
																			USBH_MAX_DATA_BUFFER);

					// continue connection attempt until successful
					if (status == USBH_OK) 
					{
						printf("usb_device_ready\r\n");
						  usb_device_ready = 1;
					}

			}
	
		
		if (flag1 ==1)         //  Флаг по которому мы определяем, что сработало прерывание 
		{    
	   	flag1 =0;     // Сбрасываем установившийся в подпрограмме прерывания флаг.
		  //Test_TFT();           
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
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 0;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 65535;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_OC_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_TIMING;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_OC_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

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
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, LED1_GREEN_Pin|LED2_RED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LCD_BL_GPIO_Port, LCD_BL_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(T_CS_GPIO_Port, T_CS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pins : BUTTON_K1_Pin BUTTON_K0_Pin */
  GPIO_InitStruct.Pin = BUTTON_K1_Pin|BUTTON_K0_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pin : BUTTON_WK_AP_Pin */
  GPIO_InitStruct.Pin = BUTTON_WK_AP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BUTTON_WK_AP_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LED1_GREEN_Pin LED2_RED_Pin */
  GPIO_InitStruct.Pin = LED1_GREEN_Pin|LED2_RED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : T_PEN_Pin */
  GPIO_InitStruct.Pin = T_PEN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(T_PEN_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LCD_BL_Pin */
  GPIO_InitStruct.Pin = LCD_BL_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LCD_BL_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : T_CS_Pin */
  GPIO_InitStruct.Pin = T_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(T_CS_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* FSMC initialization function */
static void MX_FSMC_Init(void)
{

  /* USER CODE BEGIN FSMC_Init 0 */

  /* USER CODE END FSMC_Init 0 */

  FSMC_NORSRAM_TimingTypeDef Timing = {0};

  /* USER CODE BEGIN FSMC_Init 1 */

  /* USER CODE END FSMC_Init 1 */

  /** Perform the SRAM1 memory initialization sequence
  */
  hsram1.Instance = FSMC_NORSRAM_DEVICE;
  hsram1.Extended = FSMC_NORSRAM_EXTENDED_DEVICE;
  /* hsram1.Init */
  hsram1.Init.NSBank = FSMC_NORSRAM_BANK1;
  hsram1.Init.DataAddressMux = FSMC_DATA_ADDRESS_MUX_DISABLE;
  hsram1.Init.MemoryType = FSMC_MEMORY_TYPE_SRAM;
  hsram1.Init.MemoryDataWidth = FSMC_NORSRAM_MEM_BUS_WIDTH_16;
  hsram1.Init.BurstAccessMode = FSMC_BURST_ACCESS_MODE_DISABLE;
  hsram1.Init.WaitSignalPolarity = FSMC_WAIT_SIGNAL_POLARITY_LOW;
  hsram1.Init.WrapMode = FSMC_WRAP_MODE_DISABLE;
  hsram1.Init.WaitSignalActive = FSMC_WAIT_TIMING_BEFORE_WS;
  hsram1.Init.WriteOperation = FSMC_WRITE_OPERATION_ENABLE;
  hsram1.Init.WaitSignal = FSMC_WAIT_SIGNAL_DISABLE;
  hsram1.Init.ExtendedMode = FSMC_EXTENDED_MODE_DISABLE;
  hsram1.Init.AsynchronousWait = FSMC_ASYNCHRONOUS_WAIT_DISABLE;
  hsram1.Init.WriteBurst = FSMC_WRITE_BURST_DISABLE;
  hsram1.Init.PageSize = FSMC_PAGE_SIZE_NONE;
  /* Timing */
  Timing.AddressSetupTime = 15;
  Timing.AddressHoldTime = 15;
  Timing.DataSetupTime = 255;
  Timing.BusTurnAroundDuration = 15;
  Timing.CLKDivision = 16;
  Timing.DataLatency = 17;
  Timing.AccessMode = FSMC_ACCESS_MODE_A;
  /* ExtTiming */

  if (HAL_SRAM_Init(&hsram1, &Timing, NULL) != HAL_OK)
  {
    Error_Handler( );
  }

  /* USER CODE BEGIN FSMC_Init 2 */

  /* USER CODE END FSMC_Init 2 */
}

/* USER CODE BEGIN 4 */
PUTCHAR_PROTOTYPE
{
  /* Place your implementation of fputc here */
  /* e.g. write a character to the EVAL_COM1 and Loop until the end of transmission */
  HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 0xFFFF); 

  return ch;
}
 

void HAL_GPIO_EXTI_Callback (uint16_t GPIO_Pin)
{
  if (GPIO_Pin ==T_PEN_Pin)
	{
  	if(XPT2046_TouchPressed())
		{
			if(XPT2046_TouchGetCoordinates(&x, &y))
			{
				flag1=1;   // если нажали на экран выставляем флаг срабатывания нажатия
				lcdDrawPixel(x, y, COLOR_RED); // будет рисоваться точка при нажании на экран
			}
		}
	}
}

static void RTLSDR_InitApplication(void)
{
	
	BSP_LED_On(LED1);
	BSP_LED_On(LED2);
	HAL_Delay(200);
	BSP_LED_Off(LED1);
	BSP_LED_Off(LED2); 
	HAL_Delay(200);
	BSP_LED_On(LED1);
	BSP_LED_On(LED2);
	HAL_Delay(200);
	BSP_LED_Off(LED1);
	BSP_LED_Off(LED2); 
	
	lcdBacklightOn();
  lcdInit();
  lcdSetOrientation(LCD_ORIENTATION_LANDSCAPE);
  lcdFillRGB(COLOR_WHITE);
	
	lcdSetTextFont(&Font16);
	lcdSetTextColor(COLOR_WHITE, COLOR_BLUE);
	lcdFillRect(0,0,319,20,COLOR_BLUE);
	lcdSetCursor(2, 5);   // xy	
	
	
	
	
	
	
	
//  
//  /* Initialize the LCD */
//  BSP_LCD_Init();
//  
//  /* LCD Layer Initialization */
//  BSP_LCD_LayerDefaultInit(0, LCD_FB_START_ADDRESS); 
//  
//  /* Select the LCD Layer */
//  BSP_LCD_SelectLayer(0);
//  BSP_LCD_SetTransparency(0, 0xFF);
//  
//  /* Other layer*/
//  BSP_LCD_LayerDefaultInit(1, ((uint32_t)(LCD_FB_START_ADDRESS + (RK043FN48H_WIDTH * RK043FN48H_HEIGHT * 4)))); 
//  BSP_LCD_SelectLayer(1);
//  BSP_LCD_SetTransparency(1, 0x00);
//  
//  /* Draw a test circle and background to the layer 1 */
//  BSP_LCD_Clear(LCD_COLOR_TRANSPARENT);
//  BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
//  BSP_LCD_DrawCircle(BSP_LCD_GetXSize()/2, BSP_LCD_GetYSize()/2, 10);
//  
//  /* Return to layer 0 for putting the console */
//  BSP_LCD_SelectLayer(0);
//  
//  /* Enable the display */
//  BSP_LCD_DisplayOn();
//  
//  /* Initialize the LCD Log module */
//  LCD_LOG_Init();
  
  /* Configure Button pin as input with External interrupt */

  
//#ifdef USE_USB_HS 
//  //LCD_LOG_SetHeader((uint8_t *)" USB OTG HS RTLSDR Host");
//  LCD_LOG_SetHeader((uint8_t *)" STM32F7 USB HS RTL-SDR Host");
//#else
//  LCD_LOG_SetHeader((uint8_t *)" STM32F7 USB FS RTL-SDR Host");
//#endif
//  
//  LCD_UsrLog("USB Host library started.\n"); 
//  
//  /* Start RTLSDR Interface */
//  USBH_UsrLog("Starting RTLSDR Demo");
//  

		
		
	#ifdef USE_USB_HS 
		//LCD_LOG_SetHeader((uint8_t *)" USB OTG HS RTLSDR Host");
		lcdPrintf(" STM32F4 USB HS RTL-SDR Host");
	#else
		lcdPrintf(" STM32F4 USB FS RTL-SDR Host");
	#endif



}






void Start_TFT(void)
{
//	lcdBacklightOn();
//	lcdInit();
//	lcdSetOrientation(LCD_ORIENTATION_LANDSCAPE);
//	lcdFillRGB(COLOR_WHITE);

	// Пишем текст сверху экрана
	lcdSetTextFont(&Font20);
	lcdSetTextColor(COLOR_BLACK, COLOR_WHITE);
	lcdSetCursor(50, 5);   // xy
	lcdPrintf("www.stm32res.ru");
	// Начинаем рисовать рамки
	lcdDrawRect(50,70,230,30,COLOR_BLUE);

	lcdDrawRect(30,120,40,30,COLOR_BLUE);  //lcdDrawRect(x, y, w, h, color)
	lcdDrawRect(75,120,40,30,COLOR_BLUE);
	lcdDrawRect(120,120,40,30,COLOR_BLUE);
	lcdDrawRect(165,120,40,30,COLOR_BLUE);
	lcdDrawRect(210,120,40,30,COLOR_BLUE);
	lcdDrawRect(255,120,50,30,COLOR_BLUE);

	lcdDrawRect(30,155,40,30,COLOR_BLUE);  //lcdDrawRect(x, y, w, h, color)
	lcdDrawRect(75,155,40,30,COLOR_BLUE);
	lcdDrawRect(120,155,40,30,COLOR_BLUE);
	lcdDrawRect(165,155,40,30,COLOR_BLUE);
	lcdDrawRect(210,155,40,30,COLOR_BLUE);
	lcdDrawRect(255,155,50,30,COLOR_BLUE);
	// Начинаем заполнять рамки цифрами    
	lcdSetTextFont(&Font24);
	lcdSetTextColor(COLOR_BLACK, COLOR_WHITE);
			
	lcdSetCursor(45, 125);   // x
	lcdPrintf("1");
	lcdSetCursor(90, 125);   // x
	lcdPrintf("2");
	lcdSetCursor(135, 125);   // x
	lcdPrintf("3");
	lcdSetCursor(180, 125);   // x
	lcdPrintf("4");
	lcdSetCursor(225, 125);   // x
	lcdPrintf("5");

	lcdSetCursor(45, 160);   // x
	lcdPrintf("6");
	lcdSetCursor(90, 160);   // x
	lcdPrintf("7");
	lcdSetCursor(135, 160);   // x
	lcdPrintf("8");
	lcdSetCursor(180, 160);   // x
	lcdPrintf("9");
	lcdSetCursor(225, 160);   // x
	lcdPrintf("0");
			
	lcdSetCursor(263, 125);   // x
	lcdPrintf("<-");

	lcdSetTextFont(&Font20);
	lcdSetCursor(265, 160);   // x
	lcdPrintf("Ok");

}

void Test_TFT(void)
{
	// проверяем в какую область нажали на экране. Нас интересует только 
	// внутренняя площадь рамок    

	if(x>=31 && x<=65)   // Координаты по х
	{
	if(y>=119 && y<=146)  // Координаты по y
	{
	xx =1;                 // Если попали то передаем переменной - номер 1  
	sprintf(str3,"%u",xx); 
	strcat(str,str3);      // Добавляем символ к общей строчке
	strw =strlen(str);
	sprintf(str4,"%u",strw);
	}
	}

	if(x>=80 && x<=109)
	{
	if(y>=111 && y<=148)
	{
	xx =2;
	sprintf(str3,"%u",xx);
	strcat(str,str3);
	strw =strlen(str);
	sprintf(str4,"%u",strw);
	}
	}

	if(x>=131 && x<=158)
	{
	if(y>=109 && y<=151)
	{
	xx =3;
	sprintf(str3,"%u",xx);
	strcat(str,str3);
	strw =strlen(str);
	sprintf(str4,"%u",strw);
	}        
	}

	if(x>=166 && x<=203)
	{
	if(y>=120 && y<=151)
	{
	xx =4;
	sprintf(str3,"%u",xx);
	strcat(str,str3);
	strw =strlen(str);
	sprintf(str4,"%u",strw);
	}        
	}

	if(x>=201 && x<=250)
	{
	if(y>=110 && y<=151)
	{
	xx =5;
	sprintf(str3,"%u",xx);
	strcat(str,str3);
	strw =strlen(str);
	sprintf(str4,"%u",strw);
	}        
	}

	if(x>=32 && x<=80)
	{
	if(y>=156 && y<=181)
	{
	xx =6;
	sprintf(str3,"%u",xx);
	strcat(str,str3);
	strw =strlen(str);
	sprintf(str4,"%u",strw);
	}        
	}

	if(x>=78 && x<=110)
	{
	if(y>=150 && y<=181)
	{
	xx =7;
	sprintf(str3,"%u",xx);
	strcat(str,str3);
	strw =strlen(str);
	sprintf(str4,"%u",strw);
	}        
	}

	if(x>=121 && x<=165)
	{
	if(y>=145 && y<=181)
	{
	xx =8;
	sprintf(str3,"%u",xx);
	strcat(str,str3);
	strw =strlen(str);
	sprintf(str4,"%u",strw);
	}        
	}

	if(x>=168 && x<=203)
	{
	if(y>=156 && y<=181)
	{
	xx =9;
	sprintf(str3,"%u",xx);
	strcat(str,str3);
	strw =strlen(str);
	sprintf(str4,"%u",strw);
	}        
	}

	if(x>=212 && x<=247)
	{
	if(y>=156 && y<=181)
	{
	xx =0;
	sprintf(str3,"%u",xx);
	strcat(str,str3);
	strw =strlen(str);
	sprintf(str4,"%u",strw);
	}        
	}


	if(x>=255 && x<=280)
	{
	if(y>=119 && y<=150)
	{
	// Эту рамку назначьте самостоятельно                  
	}
	//
	}     


		// Выводим то что врамках 
		lcdSetTextFont(&Font20);
		lcdSetCursor(60, 78);   // x,y
		lcdPrintf(str) ; 

		lcdSetTextFont(&Font20);
		lcdSetCursor(60, 200);   // x,y
		lcdPrintf(str1) ; //"x= "

		lcdSetTextFont(&Font20);
		lcdSetCursor(180, 200);   // x,y
		lcdPrintf(str2); //"y= "

		lcdSetTextFont(&Font20);
		lcdSetCursor(285, 75);   // x,y
		lcdPrintf(str4); 

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
	
	BSP_LED_On(LED2);
	lcdSetTextFont(&Font20);
	lcdSetCursor(10, 200);   // 
	lcdPrintf("ERROR");
	
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
