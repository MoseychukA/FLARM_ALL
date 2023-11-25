#include "main.h"

//void USBD_UserProcess(USBD_HandleTypeDef* phost, uint8_t id) {
//
//    UNUSED(phost);
//
//    DEBUG_PRINT("USBD_UserProcess\n");
//
//    switch (id) {
//
//        case HOST_USER_SELECT_CONFIGURATION:
//            USBD_DbgLog("HOST_USER_SELECT_CONFIGURATION");
//            break;
//
//        case HOST_USER_CLASS_ACTIVE:
//            USBD_DbgLog("HOST_USER_CLASS_ACTIVE");
//            break;
//
//        case HOST_USER_CLASS_SELECTED:
//            USBD_DbgLog("HOST_USER_CLASS_SELECTED");
//            break;
//
//        case HOST_USER_CONNECTION:
//            USBD_DbgLog("HOST_USER_CONNECTION");
//            break;
//
//        case HOST_USER_DISCONNECTION:
//            usb_device_ready = 0;
//            USBD_DbgLog("HOST_USER_DISCONNECTION");
//            break;
//
//        case HOST_USER_UNRECOVERED_ERROR:
//            USBD_DbgLog("HOST_USER_UNRECOVERED_ERROR");
//            break;
//
//        default:
//            USBD_DbgLog("Default");
//            break;
//
//    }
//}
