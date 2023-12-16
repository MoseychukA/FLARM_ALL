/**
  @page Demo_Binary   Description of the 32F746GDISCOVERY StemWin Demo firmware's binary files
 
  @verbatim
  ******************************************************************************
  * @file    STemWin/readme.txt 
  * @author  MCD Application Team
  * @brief   Description of the 32F746GDISCOVERY STemWin Demo firmware's binary files.
  ******************************************************************************
  *
  * Copyright (c) 2016 STMicroelectronics. All rights reserved.
  *
  * This software component is licensed by ST under Ultimate Liberty license SLA0044,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                       http://www.st.com/SLA0044
  *
  ******************************************************************************
  @endverbatim

@par Demo Description

The STM32Cube Demonstration platform comes on top of the STM32CubeTM as a firmware
package that offers a full set of software components based on a modules architecture
allowing re-using them separately in standalone applications. All these modules are
managed by the STM32Cube Demonstration kernel allowing to dynamically adding new
modules and access to common resources (storage, graphical components and widgets,
memory management, Real-Time operating system)

The STM32Cube Demonstration platform is built around the powerful graphical library
STemWin and the FreeRTOS real time operating system and uses almost the whole STM32
capability to offer a large scope of usage based on the STM32Cube HAL BSP and several
middleware components.

@par Demo Description

The STM32 F7 demonstration is running on 32F746GDISCOVERY boards RevB. 

  
Below you find an overview of the different offered module in the demonstration:

  + Video player
 ---------------
 The video player module provides a video solution based on the STM32F7xx. 
 It supports playing movie in AVI format.
 You can use the *.avi video files (with 480x272 resolution) provided 
 under "Media/Video".
 
 + Audio player
 --------------
 The audio player module provides a complete audio solution based on the STM32F7xx and
 delivers a high-quality music experience. It supports playing music in WAV format but may
 be extended to support other compressed formats such as MP3 and WMA audio formats.
 The Module supports background mode feature.
 
  + Audio recorder
 -----------------
 The audio recorder module allows recording audio and playing it back.
 
 + VNC Server
 ------------
 The VNC server module allows to control the demo from a remote machine. It is based on
 the TCP/IP LwIP stacks. The background mode is supported.
 @note Launch any VNC Client or the emVNC software.
 
 + Audio Recorder
 ----------------
 The Audio record module allows to record an audio file. The audio format supported is 
 WAV format but may be extended to support other compressed formats such as MP3.
 The recorded files are stored in USB Disk flash(USB High Speed).
  
 + Home alarm
 ------------ 
 Control of Home alarm system, equipped with cameras.
 Static picture shown when a room is selected and then the camera icon pressed
 General room alarm activation/deactivation when pressed.
 
 + Gardening control
 -------------------
 The gardening control module provides a graphic garden watering system behavior
 
 + Game
 ------
 The game coming in the STM32Cube demonstration is based on the Reversi game. It is a
 strategy board game for two players, played on an 8×8 board. The goal of the game is to
 have the majority of disks turned to display your color when the last playable empty square
 is filled.
 
 + System Info
 --------------  
 The system info module provides information about the core, CPU speed and demonstration version.  
    
    For more details about the demonstration modules please refers to  STM32CubeF7 demonstration (UM1906)
    
@note The STM32F7xx devices can reach a maximum clock frequency of 216MHz but as this demonstration uses SDRAM,
      the system clock is limited to 200MHz. Indeed proper functioning of the SDRAM is only guaranteed 
      at a maximum system clock frequency of 200MHz.
      
@par Hardware and Software environment

  - This application runs on STM32F746xx devices.  

  - This example has been tested with STMicroelectronics 32F746GDISCOVERY
    boards RevB and can be easily tailored to any other supported device 
    and development board.

@par How to use it ? 

The QSPI external flash loader is not integrated with supported toolchains, it’s only supported with STM32
ST-Link Utility V3.9 or later
To load the demonstration, use STM32 ST-Link Utility to program both internal Flash and external QSPI memory.
To edit and debug the demonstration you need first to program the external QSPI memory using STLink utility
and then use your preferred toolchain to update and debug the internal flash content.

In order to program the demonstration you must do the following:
1- Open STM32 ST-Link Utility, click on "External Loader" from the bar menu then check 
   "N25Q128A_STM32F746G-DISCO" box 
2- Connect the 32F746GDISCOVERY board to PC with USB cable through CN14
3- Use "STM32CubeDemo_STM32746G-DISCO_VX.Y.Z.hex" file provided under “Binary” with STM32 ST-Link Utility
   to program both internal Flash and external QSPI memory
4- copy the audio and video files provided under "Media/" in the USB key
5- Plug a USB micro A-Male to A-Female cable on CN12 connector
6- Plug a headphone into CN10 Connector to hear the sound
-> The internal Flash and the external QSPI are now programmed and the demonstration is shown on the board.

 * <h3><center>&copy; COPYRIGHT STMicroelectronics</center></h3>
 */
 
