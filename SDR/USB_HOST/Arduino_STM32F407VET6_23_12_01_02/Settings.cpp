//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#include "Settings.h"
#include "usbh_def.h"
#include "usbh_conf.h"
#include "usbh_core.h"

USBH_HandleTypeDef hUsbHostFS;
ApplicationTypeDef Appli_state = APPLICATION_IDLE;

static void USBH_UserProcess(USBH_HandleTypeDef* phost, uint8_t id);
USBH_StatusTypeDef  USBH_Init(USBH_HandleTypeDef* phost, void (*pUsrFunc)(USBH_HandleTypeDef* phost, uint8_t id), uint8_t id);

static void USBH_UserProcess(USBH_HandleTypeDef* phost, uint8_t id)
{
    /* USER CODE BEGIN CALL_BACK_1 */
    switch (id)
    {
        case HOST_USER_SELECT_CONFIGURATION:
            break;

        case HOST_USER_DISCONNECTION:
            Appli_state = APPLICATION_DISCONNECT;
            break;

        case HOST_USER_CLASS_ACTIVE:
            Appli_state = APPLICATION_READY;
            break;

        case HOST_USER_CONNECTION: 
            break;

    default:
        break;
    }
    /* USER CODE END CALL_BACK_1 */
}


//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
SettingsClass Settings;
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
SettingsClass::SettingsClass()
{
 
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void SettingsClass::begin()
{
    /* Init host Library, add supported class and start the library. */
    if (USBH_Init(&hUsbHostFS, USBH_UserProcess, HOST_FS) != USBH_OK)
    {
        //Error_Handler();
    }
    //if (USBH_RegisterClass(&hUsbHostFS, USBH_HID_CLASS) != USBH_OK)
    //{
    //    Error_Handler();
    //}
    //if (USBH_Start(&hUsbHostFS) != USBH_OK)
    //{
    //    Error_Handler();
    //}
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void SettingsClass::update()
{
 
}



USBH_StatusTypeDef  USBH_Init(USBH_HandleTypeDef* phost, void (*pUsrFunc)(USBH_HandleTypeDef* phost,  uint8_t id), uint8_t id)
{
    /* Check whether the USB Host handle is valid */
    if (phost == NULL)
    {
       //!! USBH_ErrLog("Invalid Host handle");
        return USBH_FAIL;
    }

    /* Set DRiver ID */
    phost->id = id;

    /* Unlink class*/
    phost->pActiveClass = NULL;
    phost->ClassNumber = 0U;

    /* Restore default states and prepare EP0 */
   //!! (void)DeInitStateMachine(phost);

    /* Restore default Device connection states */
    phost->device.PortEnabled = 0U;
    phost->device.is_connected = 0U;
    phost->device.is_disconnected = 0U;
    phost->device.is_ReEnumerated = 0U;

    /* Assign User process */
    if (pUsrFunc != NULL)
    {
        phost->pUser = pUsrFunc;
    }



    /* Initialize low level driver */
    //!!(void)USBH_LL_Init(phost);

    return USBH_OK;
}

//void USBH_UserProcess(USBH_HandleTypeDef* phost, uint8_t id)
//{
//    /* USER CODE BEGIN CALL_BACK_1 */
//    switch (id)
//    {
//    //case HOST_USER_SELECT_CONFIGURATION:
//    //    break;
//
//    //case HOST_USER_DISCONNECTION:
//    //    Appli_state = APPLICATION_DISCONNECT;
//    //    break;
//
//    //case HOST_USER_CLASS_ACTIVE:
//    //    Appli_state = APPLICATION_READY;
//    //    break;
//
//    //case HOST_USER_CONNECTION:
//    //    Appli_state = APPLICATION_START;
//    //    break;
//
//    default:
//        break;
//    }
//    /* USER CODE END CALL_BACK_1 */
//}



//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
