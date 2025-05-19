
/*
 * THE FOLLOWING FIRMWARE IS PROVIDED: 
 * (1) "AS IS" WITH NO WARRANTY; 
 * (2) TO ENABLE ACCESS TO CODING INFORMATION TO GUIDE AND FACILITATE CUSTOMER.
 * CONSEQUENTLY, HopeRF SHALL NOT BE HELD LIABLE FOR ANY DIRECT, INDIRECT OR
 * CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING FROM THE CONTENT
 * OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE CODING INFORMATION
 * CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
 * 
 * Copyright (C) HopeRF
 * website: www.HopeRF.com
 *          www.HopeRF.cn
 *
 */

/*! 
 * file       sx127x_demo
 * hardware   RFM95、RFM96、RFM97、RFM98、STM32F103ZE           
 * version    1.0
 * date       Aug 7 2023
 * author     XC Zou
 */
 
#include "./stm32f1xx_it.h"
#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/usart/usart.h"
#include "./SYSTEM/delay/delay.h"

extern int app_start( void );
extern void TIM6_Init(void);

int main(void)
{
    HAL_Init();                            
    sys_stm32_clock_init(RCC_PLL_MUL9);    /* system clock 72Mhz */
    delay_init(72);                        
    usart_init(115200);            
		TIM6_Init();
	  printf("RESET\r\n");
	
    app_start();
}



