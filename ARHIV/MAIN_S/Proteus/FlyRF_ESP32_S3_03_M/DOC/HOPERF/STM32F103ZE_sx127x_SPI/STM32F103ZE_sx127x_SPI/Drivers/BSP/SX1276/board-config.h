/*!
 * \file      board-config.h
 *
 * \brief     Board configuration
 *
 * \copyright Revised BSD License, see section \ref LICENSE.
 *
 * \code
 *                ______                              _
 *               / _____)             _              | |
 *              ( (____  _____ ____ _| |_ _____  ____| |__
 *               \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 *               _____) ) ____| | | || |_| ____( (___| | | |
 *              (______/|_____)_|_|_| \__)_____)\____)_| |_|
 *              (C)2013-2017 Semtech
 *
 *               ___ _____ _   ___ _  _____ ___  ___  ___ ___
 *              / __|_   _/_\ / __| |/ / __/ _ \| _ \/ __| __|
 *              \__ \ | |/ _ \ (__| ' <| _| (_) |   / (__| _|
 *              |___/ |_/_/ \_\___|_|\_\_| \___/|_|_\\___|___|
 *              embedded.connectivity.solutions===============
 *
 * \endcode
 *
 * \author    Miguel Luis ( Semtech )
 *
 * \author    Gregory Cristian ( Semtech )
 *
 * \author    Daniel Jaeckle ( STACKFORCE )
 *
 * \author    Johannes Bruder ( STACKFORCE )
 */
#ifndef __BOARD_CONFIG_H__
#define __BOARD_CONFIG_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "./SYSTEM/sys/sys.h"
/*!
 * Defines the time required for the TCXO to wakeup [ms].
 */
#define BOARD_TCXO_WAKEUP_TIME                      0

/*!
 * Enables the choice between Led1 and Potentiometer.
 * LED1 and Potentiometer are exclusive.
 * \remark When using Potentiometer don't forget  that the connection between
 *         ADC input pin of iM980A and the Demoboard Poti requires a connection
 *         between X5:11 - X5:18.
 *         Remove the original jumpers for that.
 *         On SKiM980A X5 is the 20 pin header close to the DIP SW and Buttons
 */
#define USE_POTENTIOMETER                           1


/*!
 * Board MCU pins definitions
 */

#define SX1276_GPIO_CLK_ENABLE()         do{ __HAL_RCC_GPIOF_CLK_ENABLE(); }while(0)   /* PF口时钟使能 */

#define	SX1276_Port	     GPIOF

/******************************************************************************************/
/* SPI1 引脚 定义 */

#define SPI1_SCK_GPIO_PORT              GPIOA
#define SPI1_SCK_GPIO_PIN               GPIO_PIN_5
#define SPI1_SCK_GPIO_CLK_ENABLE()      do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)   /* PA口时钟使能 */

#define SPI1_MISO_GPIO_PORT             GPIOA
#define SPI1_MISO_GPIO_PIN              GPIO_PIN_6
#define SPI1_MISO_GPIO_CLK_ENABLE()     do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)   /* PA口时钟使能 */

#define SPI1_MOSI_GPIO_PORT             GPIOA
#define SPI1_MOSI_GPIO_PIN              GPIO_PIN_7
#define SPI1_MOSI_GPIO_CLK_ENABLE()     do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)   /* PA口时钟使能 */

/* SPI1相关定义 */
#define SPI1_SPI                        SPI1
#define SPI1_SPI_CLK_ENABLE()           do{ __HAL_RCC_SPI1_CLK_ENABLE(); }while(0)    /* SPI1时钟使能 */

/******************************************************************************************/

/* SPI总线速度设置 */
#define SPI_SPEED_2         0
#define SPI_SPEED_4         1
#define SPI_SPEED_8         2
#define SPI_SPEED_16        3
#define SPI_SPEED_32        4
#define SPI_SPEED_64        5
#define SPI_SPEED_128       6
#define SPI_SPEED_256       7

#define SX1276DIO0_Pin   GPIO_PIN_0  
#define SX1276DIO1_Pin   GPIO_PIN_1
#define SX1276POR_Pin	   GPIO_PIN_2
#define SX1276Nss_Pin	   GPIO_PIN_3
#define SX1276Switch_Pin GPIO_PIN_7//暂时没用到

//#define SX1276DIO2_Pin  GPIO_PIN_3//暂时没用到
//#define SX1276DIO3_Pin  GPIO_PIN_4//暂时没用到
#define SX1276TXCO_Pin  GPIO_PIN_5//暂时没用到

#define Dio0_INT_IRQn        EXTI0_IRQn
#define Dio0_INT_IRQHandler  EXTI0_IRQHandler

#define Dio1_INT_IRQn        EXTI1_IRQn
#define Dio1_INT_IRQHandler  EXTI1_IRQHandler

#define	SetSX1276POR()	(SX1276_Port->BSRR = SX1276POR_Pin)
#define	ClrSX1276POR()	(SX1276_Port->BRR = SX1276POR_Pin)

#define	SetSX1276Nss()	(SX1276_Port->BSRR = SX1276Nss_Pin)
#define	ClrSX1276Nss()	(SX1276_Port->BRR = SX1276Nss_Pin)

#define	SetSX1276TXCO()	(SX1276_Port->BSRR = SX1276TXCO_Pin)//暂时没用到
#define	ClrSX1276TXCO()	(SX1276_Port->BRR = SX1276TXCO_Pin) //暂时没用到

//引脚判断
#define	SX1276DIO1()		(SX1276_Port->IDR & SX1276DIO1_Pin)


uint8_t spi1_read_write_byte(uint8_t txdata);
void spi1_write_byte(uint8_t txdata);
	
#ifdef __cplusplus
}
#endif

#endif // __BOARD_CONFIG_H__

