// Visual Micro is in vMicro>General>Tutorial Mode
// 
/*
    Name:       ArduinoSDR_USB_Host_23_11_23_01.ino
    Created:	23.11.2023 13:54:55
    Author:     MASTER\Alex
*/
#include <stdio.h>                // define I/O functions
#include <Arduino.h>              // define I/O functions
//#include "SPI.h"
#include "pgmstrings.h"
#include "Configuration_STM32.h"
#include "stdint.h"
#include "string.h"
#include <stdlib.h>
#include <stdio.h>

#include "main.h"

#include "usb_host.h"
#include "USBDevice.h"

#include "usb_device.h"  // обеспечивает функцию инициализации, вызываемую из main()
#include "usbd_conf.h"   // обеспечивает функции низкого уровня / HAL
#include "usbd_core.h"   // обеспечивает все основные функции USB–устройства
#include "usbd_ctlreq.h" // обеспечивает запросы ввода-вывода через USB
#include "usbd_def.h"
#include "usbd_desc.h"  // предоставляет дескрипторы USB-устройств 
#include "usbd_ioreq.h" // обеспечивает запросы ввода-вывода через USB


//#include "usb_device.h"
//#include "usbd_core.h"
//#include "usbd_desc.h"
#include "cdc/usbd_cdc.h"
#include "cdc/usbd_cdc_if.h"



//extern USBD_HandleTypeDef hUsbDeviceFS;
//extern USBD_HandleTypeDef hUsbDeviceFS;
//USBD_HandleTypeDef hUSBHost;

//PCD_HandleTypeDef g_hpcd;

extern USBD_HandleTypeDef hUsbHostFS;
extern ApplicationTypeDef Appli_state;
extern USBD_StatusTypeDef usbresult;

#include "RamBlockDevice.h"

RamBlockDevice ramBlockDevice(100 /* x 512 blocks*/, true);

BlockDevice* getMassStorage() 
{
    return &ramBlockDevice;
}

void blinkOnRead(uint32_t block, size_t count) 
{
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
}

void blinkOnWrite(uint32_t block, size_t count) 
{
    digitalWrite(LED1_BUILTIN, !digitalRead(LED1_BUILTIN));
}


void setup()
{
    Serial.begin(115200);
    Serial.println("Start");
    /* Отправляем в КОМ порт версию программы */
    String ver_soft = __FILE__;
    int val_srt = ver_soft.lastIndexOf('\\');
    ver_soft.remove(0, val_srt + 1);
    val_srt = ver_soft.lastIndexOf('.');
    ver_soft.remove(val_srt);
    Serial.println(ver_soft);

    pinMode(LED1_GREEN, OUTPUT);
    pinMode(LED2_RED, OUTPUT);


    //digitalWrite(LED1_GREEN, LOW);
    //digitalWrite(LED2_RED, LOW);
    //delay(200);
    //digitalWrite(LED1_GREEN, HIGH);
    //digitalWrite(LED2_RED, HIGH);

  //  MX_USB_HOST_Init();
  
   // MX_USB_DEVICE_Init();
    ramBlockDevice.setReadListener(blinkOnRead);
    ramBlockDevice.setWriteListener(blinkOnWrite);
    delay(200);
}

// Add the main program code into the continuous loop() function
void loop()
{
  //  MX_USB_HOST_Process();

}



/* prints hex numbers with leading zeroes */
// copyright, Peter H Anderson, Baltimore, MD, Nov, '07
// source: http://www.phanderson.com/arduino/arduino_display.html
void print_hex(int v, int num_places)
{
    int mask = 0, n, num_nibbles, digit;

    for (n = 1; n <= num_places; n++) {
        mask = (mask << 1) | 0x0001;
    }
    v = v & mask; // truncate v to specified number of places

    num_nibbles = num_places / 4;
    if ((num_places % 4) != 0) {
        ++num_nibbles;
    }
    do {
        digit = ((v >> (num_nibbles - 1) * 4)) & 0x0f;
        Serial.print(digit, HEX);
    } while (--num_nibbles);
}
/* function to print configuration descriptor */
void printconfdescr(uint8_t* descr_ptr)
{
    /*USB_CONFIGURATION_DESCRIPTOR* conf_ptr = (USB_CONFIGURATION_DESCRIPTOR*)descr_ptr;
    printProgStr(Conf_Header_str);
    printProgStr(Conf_Totlen_str);
    print_hex(conf_ptr->wTotalLength, 16);
    printProgStr(Conf_Nint_str);
    print_hex(conf_ptr->bNumInterfaces, 8);
    printProgStr(Conf_Value_str);
    print_hex(conf_ptr->bConfigurationValue, 8);
    printProgStr(Conf_String_str);
    print_hex(conf_ptr->iConfiguration, 8);
    printProgStr(Conf_Attr_str);
    print_hex(conf_ptr->bmAttributes, 8);
    printProgStr(Conf_Pwr_str);
    print_hex(conf_ptr->bMaxPower, 8);*/
    return;
}
/* function to print interface descriptor */
void printintfdescr(uint8_t* descr_ptr)
{
  /*  USB_INTERFACE_DESCRIPTOR* intf_ptr = (USB_INTERFACE_DESCRIPTOR*)descr_ptr;
    printProgStr(Int_Header_str);
    printProgStr(Int_Number_str);
    print_hex(intf_ptr->bInterfaceNumber, 8);
    printProgStr(Int_Alt_str);
    print_hex(intf_ptr->bAlternateSetting, 8);
    printProgStr(Int_Endpoints_str);
    print_hex(intf_ptr->bNumEndpoints, 8);
    printProgStr(Int_Class_str);
    print_hex(intf_ptr->bInterfaceClass, 8);
    printProgStr(Int_Subclass_str);
    print_hex(intf_ptr->bInterfaceSubClass, 8);
    printProgStr(Int_Protocol_str);
    print_hex(intf_ptr->bInterfaceProtocol, 8);
    printProgStr(Int_String_str);
    print_hex(intf_ptr->iInterface, 8);*/
    return;
}
/* function to print endpoint descriptor */
void printepdescr(uint8_t* descr_ptr)
{
   /* USB_ENDPOINT_DESCRIPTOR* ep_ptr = (USB_ENDPOINT_DESCRIPTOR*)descr_ptr;
    printProgStr(End_Header_str);
    printProgStr(End_Address_str);
    print_hex(ep_ptr->bEndpointAddress, 8);
    printProgStr(End_Attr_str);
    print_hex(ep_ptr->bmAttributes, 8);
    printProgStr(End_Pktsize_str);
    print_hex(ep_ptr->wMaxPacketSize, 16);
    printProgStr(End_Interval_str);
    print_hex(ep_ptr->bInterval, 8);*/

    return;
}
/*function to print unknown descriptor */
void printunkdescr(uint8_t* descr_ptr)
{
    uint8_t length = *descr_ptr;
    uint8_t i;
    printProgStr(Unk_Header_str);
    printProgStr(Unk_Length_str);
    print_hex(*descr_ptr, 8);
    printProgStr(Unk_Type_str);
    print_hex(*(descr_ptr + 1), 8);
    printProgStr(Unk_Contents_str);
    descr_ptr += 2;
    for (i = 0; i < length; i++) {
        print_hex(*descr_ptr, 8);
        descr_ptr++;
    }
}


/* Print a string from Program Memory directly to save RAM */
void printProgStr(const char* str)
{
    char c;
    if (!str) return;
    while ((c = pgm_read_byte(str++)))
        Serial.print(c);
}
