/**
  ******************************************************************************
  * @file    USB_Host/FWupgrade_Standalone/Src/command.c
  * @author  MCD Application Team
  * @brief   This file provides all the IAP command functions.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2017 STMicroelectronics International N.V.
  * All rights reserved.</center></h2>
  *
  * Redistribution and use in source and binary forms, with or without
  * modification, are permitted, provided that the following conditions are met:
  *
  * 1. Redistribution of source code must retain the above copyright notice,
  *    this list of conditions and the following disclaimer.
  * 2. Redistributions in binary form must reproduce the above copyright notice,
  *    this list of conditions and the following disclaimer in the documentation
  *    and/or other materials provided with the distribution.
  * 3. Neither the name of STMicroelectronics nor the names of other
  *    contributors to this software may be used to endorse or promote products
  *    derived from this software without specific written permission.
  * 4. This software, including modifications and/or derivative works of this
  *    software, must execute solely and exclusively on microcontroller or
  *    microprocessor devices manufactured by or for STMicroelectronics.
  * 5. Redistribution and use of this software other than as permitted under
  *    this license is void and will automatically terminate your rights under
  *    this license.
  *
  * THIS SOFTWARE IS PROVIDED BY STMICROELECTRONICS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS, IMPLIED OR STATUTORY WARRANTIES, INCLUDING, BUT NOT
  * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
  * PARTICULAR PURPOSE AND NON-INFRINGEMENT OF THIRD PARTY INTELLECTUAL PROPERTY
  * RIGHTS ARE DISCLAIMED TO THE FULLEST EXTENT PERMITTED BY LAW. IN NO EVENT
  * SHALL STMICROELECTRONICS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
  * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
  * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
  * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
  * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/

#include "command.h"
#include "fatfs.h"

/* Private typedef -----------------------------------------------------------*/

/* Private defines -----------------------------------------------------------*/

extern char DOWNLOAD_FILENAME[15];                               // Переменная для хранения имени загружаемого bin файла

/* Private macros ------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
static uint32_t TmpReadSize = 0x00;
static uint32_t RamAddress = 0x00;
static __IO uint32_t LastPGAddress = APPLICATION_ADDRESS;

static uint8_t RAM_Buf[BUFFER_SIZE] = {0x00}; // Буфер для временного хранения блоков при чтении файла из SD карты. 

extern FATFS SDFatFs;
extern FIL MyFile; /* File object for upload operation */
extern DIR dir;
extern FILINFO fno;
extern char SD_Path[4];  /* SD logical drive path */

/* Private function prototypes -----------------------------------------------*/
static void COMMAND_ProgramFlashMemory(void);

/* Private functions ---------------------------------------------------------*/


/**
  * @brief  IAP Write Flash memory.
  * @param  None
  * @retval None
  */
void COMMAND_Download(void)
{
 	
	FLASH_If_FlashUnlock();

	SD_BufferSizeControl();        // Проверить размер буфера.
	
	if(f_mount(&SDFatFs,(TCHAR const*)SD_Path,0)!=FR_OK)
	{
		Error_Handler();
	}
	else
	{
		if(f_open(&MyFile, DOWNLOAD_FILENAME, FA_READ) == FR_OK)
		 {
			printf("Open the binary file:%s",DOWNLOAD_FILENAME);
			printf("\r\n");
			if(f_size(&MyFile) > USER_FLASH_SIZE)
			{
				/* No available Flash memory size for the binary file: Toggle LED2 in infinite loop */
				Fail_Handler();
			}
			else
			{
				/* Erase FLASH sectors to download image */
				if(FLASH_If_EraseSectors(APPLICATION_ADDRESS) != 0x00)
				{
					/* Flash erase error:Toggle LED2 infinite loop */
					Erase_Fail_Handler();
				}
				printf("Program flash memory\r\n");

				/* Program flash memory */
				COMMAND_ProgramFlashMemory();

				/* Download Done: Turn LED2 Off and LED2 On */
				printf("Download Done\r\n");
				BSP_LED_Off(LED2);
				BSP_LED_On(LED1);

				/* Close file */
				FLASH_If_FlashLock();
				f_close(&MyFile);
				printf("Close file\r\n");
				COMMAND_Jump();
			}
		}
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

	FLASH_If_FlashUnlock();
  /* RAM Address Initialization */
  RamAddress = (uint32_t) &RAM_Buf;

  /* Erase address init */
  LastPGAddress = APPLICATION_ADDRESS;

  /* While file still contain data */
  while ((readflag == TRUE))
  {
    /* Read maximum 512 Kbyte from the selected file */
    f_read (&MyFile, RAM_Buf, BUFFER_SIZE, (void *)&bytesread);

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


static void SD_BufferSizeControl(void)
{
  /* Control BUFFER_SIZE and limit this value to 32Kbyte maximum */
  /* Контролируем размер буфера BUFFER_SIZE и ограничиваем это значение до 32 Кбайт */
  if((BUFFER_SIZE % 4 != 0x00) || (BUFFER_SIZE / 4 > 8192))
  {
    while(1)
    {
      /* Toggle LED2, LED3 and LED4 */
      BSP_LED_Toggle(LED2);
      BSP_LED_Toggle(LED1);
      HAL_Delay(300);
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



/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
