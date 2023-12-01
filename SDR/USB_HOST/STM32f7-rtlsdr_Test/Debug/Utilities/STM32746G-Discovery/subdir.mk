################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Utilities/STM32746G-Discovery/stm32746g_discovery.c \
../Utilities/STM32746G-Discovery/stm32746g_discovery_audio.c \
../Utilities/STM32746G-Discovery/stm32746g_discovery_camera.c \
../Utilities/STM32746G-Discovery/stm32746g_discovery_eeprom.c \
../Utilities/STM32746G-Discovery/stm32746g_discovery_lcd.c \
../Utilities/STM32746G-Discovery/stm32746g_discovery_qspi.c \
../Utilities/STM32746G-Discovery/stm32746g_discovery_sd.c \
../Utilities/STM32746G-Discovery/stm32746g_discovery_sdram.c \
../Utilities/STM32746G-Discovery/stm32746g_discovery_ts.c 

OBJS += \
./Utilities/STM32746G-Discovery/stm32746g_discovery.o \
./Utilities/STM32746G-Discovery/stm32746g_discovery_audio.o \
./Utilities/STM32746G-Discovery/stm32746g_discovery_camera.o \
./Utilities/STM32746G-Discovery/stm32746g_discovery_eeprom.o \
./Utilities/STM32746G-Discovery/stm32746g_discovery_lcd.o \
./Utilities/STM32746G-Discovery/stm32746g_discovery_qspi.o \
./Utilities/STM32746G-Discovery/stm32746g_discovery_sd.o \
./Utilities/STM32746G-Discovery/stm32746g_discovery_sdram.o \
./Utilities/STM32746G-Discovery/stm32746g_discovery_ts.o 

C_DEPS += \
./Utilities/STM32746G-Discovery/stm32746g_discovery.d \
./Utilities/STM32746G-Discovery/stm32746g_discovery_audio.d \
./Utilities/STM32746G-Discovery/stm32746g_discovery_camera.d \
./Utilities/STM32746G-Discovery/stm32746g_discovery_eeprom.d \
./Utilities/STM32746G-Discovery/stm32746g_discovery_lcd.d \
./Utilities/STM32746G-Discovery/stm32746g_discovery_qspi.d \
./Utilities/STM32746G-Discovery/stm32746g_discovery_sd.d \
./Utilities/STM32746G-Discovery/stm32746g_discovery_sdram.d \
./Utilities/STM32746G-Discovery/stm32746g_discovery_ts.d 


# Each subdirectory must supply rules for building sources it contributes
Utilities/STM32746G-Discovery/%.o: ../Utilities/STM32746G-Discovery/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: MCU GCC Compiler'
	@echo $(PWD)
	arm-none-eabi-gcc -mcpu=cortex-m7 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16 -DDEBUG -DSTM32F746G_DISCO -DSTM32F746NGHx -DSTM32F7 -DSTM32 -DUSE_HAL_DRIVER -DSTM32F746xx -I"D:/1 GIT FLARM_ALL/SDR/USB_HOST/STM32f7-rtlsdr_Test/inc" -I"D:/1 GIT FLARM_ALL/SDR/USB_HOST/STM32f7-rtlsdr_Test/HAL_Driver/Inc" -I"D:/1 GIT FLARM_ALL/SDR/USB_HOST/STM32f7-rtlsdr_Test/HAL_Driver/Inc/Legacy" -I"D:/1 GIT FLARM_ALL/SDR/USB_HOST/STM32f7-rtlsdr_Test/Utilities/STM32746G-Discovery" -I"D:/1 GIT FLARM_ALL/SDR/USB_HOST/STM32f7-rtlsdr_Test/Utilities/Fonts" -I"D:/1 GIT FLARM_ALL/SDR/USB_HOST/STM32f7-rtlsdr_Test/Utilities/Log" -I"D:/1 GIT FLARM_ALL/SDR/USB_HOST/STM32f7-rtlsdr_Test/Utilities" -I"D:/1 GIT FLARM_ALL/SDR/USB_HOST/STM32f7-rtlsdr_Test/Utilities/Components/ft5336" -I"D:/1 GIT FLARM_ALL/SDR/USB_HOST/STM32f7-rtlsdr_Test/Utilities/Components/otm8009a" -I"D:/1 GIT FLARM_ALL/SDR/USB_HOST/STM32f7-rtlsdr_Test/Utilities/Components/ov9655" -I"D:/1 GIT FLARM_ALL/SDR/USB_HOST/STM32f7-rtlsdr_Test/Utilities/Components/st7735" -I"D:/1 GIT FLARM_ALL/SDR/USB_HOST/STM32f7-rtlsdr_Test/Utilities/Components/ampire480272" -I"D:/1 GIT FLARM_ALL/SDR/USB_HOST/STM32f7-rtlsdr_Test/Utilities/Components/ft6x06" -I"D:/1 GIT FLARM_ALL/SDR/USB_HOST/STM32f7-rtlsdr_Test/Utilities/Components/ampire640480" -I"D:/1 GIT FLARM_ALL/SDR/USB_HOST/STM32f7-rtlsdr_Test/Utilities/Components/wm8994" -I"D:/1 GIT FLARM_ALL/SDR/USB_HOST/STM32f7-rtlsdr_Test/Utilities/Components/Common" -I"D:/1 GIT FLARM_ALL/SDR/USB_HOST/STM32f7-rtlsdr_Test/Utilities/Components/adv7533" -I"D:/1 GIT FLARM_ALL/SDR/USB_HOST/STM32f7-rtlsdr_Test/Utilities/Components/s5k5cag" -I"D:/1 GIT FLARM_ALL/SDR/USB_HOST/STM32f7-rtlsdr_Test/Utilities/Components/mx25l512" -I"D:/1 GIT FLARM_ALL/SDR/USB_HOST/STM32f7-rtlsdr_Test/Utilities/Components/mfxstm32l152" -I"D:/1 GIT FLARM_ALL/SDR/USB_HOST/STM32f7-rtlsdr_Test/Utilities/Components/n25q128a" -I"D:/1 GIT FLARM_ALL/SDR/USB_HOST/STM32f7-rtlsdr_Test/Utilities/Components/n25q512a" -I"D:/1 GIT FLARM_ALL/SDR/USB_HOST/STM32f7-rtlsdr_Test/Utilities/Components/exc7200" -I"D:/1 GIT FLARM_ALL/SDR/USB_HOST/STM32f7-rtlsdr_Test/Utilities/Components/ts3510" -I"D:/1 GIT FLARM_ALL/SDR/USB_HOST/STM32f7-rtlsdr_Test/Utilities/Components/rk043fn48h" -I"D:/1 GIT FLARM_ALL/SDR/USB_HOST/STM32f7-rtlsdr_Test/Utilities/Components/stmpe811" -I"D:/1 GIT FLARM_ALL/SDR/USB_HOST/STM32f7-rtlsdr_Test/CMSIS/core" -I"D:/1 GIT FLARM_ALL/SDR/USB_HOST/STM32f7-rtlsdr_Test/CMSIS/device" -I"D:/1 GIT FLARM_ALL/SDR/USB_HOST/STM32f7-rtlsdr_Test/Middlewares/ST/STM32_USB_Host_Library/Core/Inc" -I"D:/1 GIT FLARM_ALL/SDR/USB_HOST/STM32f7-rtlsdr_Test/Middlewares/ST/STM32_USB_Host_Library/Class/CDC/Inc" -I"D:/1 GIT FLARM_ALL/SDR/USB_HOST/STM32f7-rtlsdr_Test/Middlewares/ST/STM32_USB_Host_Library/Class/RTLSDR/Inc" -O0 -g3 -Wall -fmessage-length=0 -ffunction-sections -c -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


