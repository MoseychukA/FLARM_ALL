/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file            : usb_host.c
  * @version         : v1.0_Cube
  * @brief           : This file implements the USB Host
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

#include "usb_host.h"
#include "usbh_core.h"
#include "usbh_hid.h"

/* USER CODE BEGIN Includes */
#include "main.h"
/* USER CODE END Includes */

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/

/* USER CODE END PV */

/* USER CODE BEGIN PFP */
/* Private function prototypes -----------------------------------------------*/

/* USER CODE END PFP */

/* USB Host core handle declaration */
USBH_HandleTypeDef hUsbHostFS;
ApplicationTypeDef Appli_state = APPLICATION_IDLE;

/*
 * -- Insert your variables declaration here --
 */
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/*
 * user callback declaration
 */
static void USBH_UserProcess(USBH_HandleTypeDef *phost, uint8_t id);

/*
 * -- Insert your external function declaration here --
 */
/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/**
  * Init USB host library, add supported class and start the library
  * @retval None
  */
void MX_USB_HOST_Init(void)
{
  /* USER CODE BEGIN USB_HOST_Init_PreTreatment */

  /* USER CODE END USB_HOST_Init_PreTreatment */

  /* Init host Library, add supported class and start the library. */
  if (USBH_Init(&hUsbHostFS, USBH_UserProcess, HOST_FS) != USBH_OK)
  {
    Error_Handler();
  }
	
	if (USBH_RegisterClass(&hUsbHostFS, USBH_RTLSDR_CLASS) != USBH_OK)
  {
    Error_Handler();
  }
	
	
  if (USBH_Start(&hUsbHostFS) != USBH_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USB_HOST_Init_PostTreatment */

  /* USER CODE END USB_HOST_Init_PostTreatment */
}

/*
 * Background task
 */
void MX_USB_HOST_Process(void)
{
  /* USB Host Background task */
  USBH_Process(&hUsbHostFS);
}
/*
 * user callback definition
 */

 uint8_t id_prev = HOST_USER_SELECT_CONFIGURATION;

static void USBH_UserProcess  (USBH_HandleTypeDef *phost, uint8_t id)
{
  /* USER CODE BEGIN CALL_BACK_1 */
	
	
	
	  if (id != id_prev) 
		{
    switch(id) { 
    case HOST_USER_SELECT_CONFIGURATION:
      USBH_UsrLog("HOST_USER_SELECT_CONFIGURATION");
      break;
    
    case HOST_USER_CLASS_ACTIVE:
      USBH_UsrLog("HOST_USER_CLASS_ACTIVE");
      
      break;

    case HOST_USER_CLASS_SELECTED:
      USBH_UsrLog("HOST_USER_CLASS_SELECTED");
      break;
      
    case HOST_USER_CONNECTION:
      USBH_UsrLog("HOST_USER_CONNECTION");
      break;
    
    case HOST_USER_DISCONNECTION:
      USBH_UsrLog("HOST_USER_DISCONNECTION");
      break;
    
    case HOST_USER_UNRECOVERED_ERROR:
      USBH_UsrLog("HOST_USER_UNRECOVERED_ERROR");
      break;

    default:
    USBH_UsrLog("Unknown USBH User State: %d", id);
      break; 
    }
  }
  id_prev = id;
  
//    
//  RTLSDR_HandleTypeDef *RTLSDR_Handle = (RTLSDR_HandleTypeDef*) phost->pActiveClass->pData;
//  
//  if (phost->gState==HOST_CLASS) 
//	{
//		if (RTLSDR_Handle->xferState==RTLSDR_XFER_COMPLETE) 
//		{
//			USBH_UsrLog("St %d", *RTLSDR_Handle);
//			//RTLSDR_Handle->xferState = RTLSDR_XFER_START;
//		}
//	}
//  
	
	
	
//  switch(id)
//  {
//	
//		USBH_UsrLog("HOST_USER_SELECT_CONFIGURATION");
//		case HOST_USER_SELECT_CONFIGURATION:
//		break;

//  case HOST_USER_DISCONNECTION:
//		USBH_UsrLog("HOST_USER_DISCONNECTION");
//		Appli_state = APPLICATION_DISCONNECT;
//		break;

//  case HOST_USER_CLASS_ACTIVE:
//		USBH_UsrLog("HOST_USER_CLASS_ACTIVE");	
//		Appli_state = APPLICATION_READY;
//		break;

//  case HOST_USER_CONNECTION:
//		USBH_UsrLog("HOST_USER_CONNECTION");	
//		Appli_state = APPLICATION_START;
//  	USBH_UsrLog("APPLICATION_START");	
//    break;

//  default:
//  break;
//  }
  /* USER CODE END CALL_BACK_1 */
}

/**
  * @}
  */

/**
  * @}
  */

