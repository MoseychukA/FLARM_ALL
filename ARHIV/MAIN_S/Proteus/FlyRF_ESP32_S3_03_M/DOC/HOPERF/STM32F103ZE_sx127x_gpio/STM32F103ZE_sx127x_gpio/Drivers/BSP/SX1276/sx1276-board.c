/*!
 * \file      sx1276-board.c
 *
 * \brief     Target board SX1276 driver implementation
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
 * \endcode
 *
 * \author    Miguel Luis ( Semtech )
 *
 * \author    Gregory Cristian ( Semtech )
 *
 * \author    Marten Lootsma(TWTG) on behalf of Microchip/Atmel (c)2017
 */
#include "./BSP/SX1276/board-config.h"
#include "./SYSTEM/delay/delay.h"
#include "radio.h"
#include "stdio.h"
#include "./BSP/SX1276/sx1276-board.h"
#include "./BSP/SX1276/sx1276.h"
/*!
 * \brief Gets the board PA selection configuration
 *
 * \param [IN] channel Channel frequency in Hz
 * \retval PaSelect RegPaConfig PaSelect value
 */
static uint8_t SX1276GetPaSelect( uint32_t channel );

/*!
 * Flag used to set the RF switch control pins in low power mode when the radio is not active.
 */
static bool RadioIsActive = false;

/*!
 * Radio driver structure initialization
 */
const struct Radio_s Radio =
{
    SX1276Init,
    SX1276GetStatus,
    SX1276SetModem,
    SX1276SetChannel,
    SX1276Random,
    SX1276SetRxConfig,
    SX1276SetTxConfig,
    SX1276CheckRfFrequency,
    SX1276GetTimeOnAir,
    SX1276Send,
    SX1276SetSleep,
    SX1276SetStby,
    SX1276SetRx,
    SX1276StartCad,
    SX1276SetTxContinuousWave,
    SX1276ReadRssi,
    SX1276Write,
    SX1276Read,
    SX1276WriteBuffer,
    SX1276ReadBuffer,
    SX1276SetMaxPayloadLength,
    SX1276SetPublicNetwork,
    SX1276GetWakeupTime,
};


void SX1276IoInit( void )
{
	GPIO_InitTypeDef gpio_init_struct;

	SX1276_GPIO_CLK_ENABLE();

	gpio_init_struct.Pin = SX1276Nss_Pin;                /* Nss引脚 */
	gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;         /* 推挽输出 */
	gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;       /* 高速 */
	HAL_GPIO_Init(SX1276_Port1, &gpio_init_struct);         

	gpio_init_struct.Pin = SX1276MOSI_Pin;                /* MOSI引脚 */
	HAL_GPIO_Init(SX1276_Port2, &gpio_init_struct);
	
  gpio_init_struct.Pin = SX1276SCK_Pin;                 /* SCK引脚 */
	HAL_GPIO_Init(SX1276_Port2, &gpio_init_struct);
	
	gpio_init_struct.Pin =  SX1276MISO_Pin;                /* MISO引脚 */
	gpio_init_struct.Mode = GPIO_MODE_INPUT;               /* 输入 */
	gpio_init_struct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(SX1276_Port2, &gpio_init_struct);  
	
	gpio_init_struct.Pin =  SX1276DIO0_Pin;                /* DIO0引脚 */
	gpio_init_struct.Mode = GPIO_MODE_IT_RISING;           /* 输入 */
	gpio_init_struct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(SX1276_Port1, &gpio_init_struct);   
	HAL_NVIC_SetPriority(Dio0_INT_IRQn, 0, 2);             /* 抢占0，子优先级2 */
	HAL_NVIC_EnableIRQ(Dio0_INT_IRQn);                     /* 使能中断线1 */
	
	gpio_init_struct.Pin = SX1276DIO1_Pin;                 /* DIO1引脚 */
	HAL_GPIO_Init(SX1276_Port1, &gpio_init_struct);   
	HAL_NVIC_SetPriority(Dio1_INT_IRQn, 0, 2);             /* 抢占0，子优先级2 */
	HAL_NVIC_EnableIRQ(Dio1_INT_IRQn);                     /* 使能中断线1 */
	
	SetSX1276Nss();
	ClrSX1276SCK();
			
}
#define BASIC_TIM                 TIM6
#define BASIC_TIM_CLK_ENABLE()    __TIM6_CLK_ENABLE()

#define BASIC_TIM_IRQn            TIM6_DAC_IRQn
#define BASIC_TIM_IRQHandler      TIM6_DAC_IRQHandler

TIM_HandleTypeDef  TIM_TimeBaseStructure;

void TIM6_Init(void)
{	
	BASIC_TIM_CLK_ENABLE();

	TIM_TimeBaseStructure.Instance = BASIC_TIM;

	HAL_TIM_Base_Init(&TIM_TimeBaseStructure);
 		 
}

void TIM6_Int_Init(uint16_t arr,uint16_t psc)
{	
	BASIC_TIM_CLK_ENABLE();
	
  TIM6->CR1 |= TIM_CR1_URS;	//如果不设置这个会导致定时器启动的时候立即进入中断
	
	TIM_TimeBaseStructure.Instance = BASIC_TIM;

	TIM_TimeBaseStructure.Init.Period = arr;

	TIM_TimeBaseStructure.Init.Prescaler =psc;

	HAL_TIM_Base_Init(&TIM_TimeBaseStructure);

	//设置抢占优先级，子优先级
	HAL_NVIC_SetPriority(BASIC_TIM_IRQn, 0, 3);
	// 设置中断来源
	HAL_NVIC_EnableIRQ(BASIC_TIM_IRQn);

	// 开启定时器更新中断
	HAL_TIM_Base_Start_IT(&TIM_TimeBaseStructure);
 		 
}

void  BASIC_TIM_IRQHandler (void)
{
  /* TIM Update event */
  if (__HAL_TIM_GET_FLAG(&TIM_TimeBaseStructure, TIM_FLAG_UPDATE) != RESET)
  {
    if (__HAL_TIM_GET_IT_SOURCE(&TIM_TimeBaseStructure, TIM_IT_UPDATE) != RESET)
    {
      __HAL_TIM_CLEAR_IT(&TIM_TimeBaseStructure, TIM_IT_UPDATE);
			SX1276OnTimeoutIrq(NULL);	//发生中断后需要调用 SX1276OnTimeoutIrq 函数
    }
  }
}

//开启 TX 超时定时器，定时timeoutMs 毫秒
void SX1276TxTimeoutTimerStart( uint32_t timeoutMs ){
	//实际工程中最好使用独立的3个定时器，或者使用软件定时器
	TIM6_Int_Init((timeoutMs * 10) -1,7199);//10Khz的计数频率，计数到10次为1ms 
}

//关闭 TX 超时定时器
void SX1276TxTimeoutTimerStop(void){
	HAL_TIM_Base_Stop_IT(&TIM_TimeBaseStructure);
}

//开启 RX 超时定时器，定时timeoutMs 毫秒
void SX1276RxTimeoutTimerStart( uint32_t timeoutMs ){
	//这里简化使用了一个定时器，实际工程中最好使用独立的3个定时器，或者使用软件定时器
	TIM6_Int_Init((timeoutMs * 10) -1,7199);//10Khz的计数频率，计数到10次为1ms 
}

//关闭 RX 超时定时器
void SX1276RxTimeoutTimerStop(void){
	HAL_TIM_Base_Stop_IT(&TIM_TimeBaseStructure);
}

//开启 SyncWord 超时定时器，定时timeoutMs 毫秒
void SX1276SyncWordTimeoutTimerStart( uint32_t timeoutMs ){
	//这里简化使用了一个定时器，实际工程中最好使用独立的3个定时器，或者使用软件定时器
	TIM6_Int_Init((timeoutMs * 10) -1,7199);//10Khz的计数频率，计数到10次为1ms 
}

//关闭 SyncWord 超时定时器
void SX1276SyncWordTimeoutTimerStop(void){
	HAL_TIM_Base_Stop_IT(&TIM_TimeBaseStructure);
}


void SX1276IoTcxoInit( void )
{
		GPIO_InitTypeDef gpio_init_struct;
	
		gpio_init_struct.Pin = SX1276TXCO_Pin;                /* TXCO引脚 */
		gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;         /* 推挽输出 */
		gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;       /* 高速 */
		HAL_GPIO_Init(SX1276_Port1, &gpio_init_struct);   
		ClrSX1276TXCO();
}

void SX1276SetBoardTcxo( uint8_t state )
{
		if(state)
			SetSX1276TXCO();
		else
			ClrSX1276TXCO();
		delay_ms( BOARD_TCXO_WAKEUP_TIME );
}

uint32_t SX1276GetBoardTcxoWakeupTime( void )
{
    return BOARD_TCXO_WAKEUP_TIME;
}

void SX1276Reset( void )
{
		GPIO_InitTypeDef gpio_init_struct;

 // Enables the TCXO if available on the board design
 // SX1276SetBoardTcxo( true );

	  SX1276IoInit();
	
    // Set RESET pin to 0
		gpio_init_struct.Pin = SX1276POR_Pin;                /* Reset引脚 */
		gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;         /* 推挽输出 */
		gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;       /* 高速 */
		HAL_GPIO_Init(SX1276_Port1, &gpio_init_struct); 
	  ClrSX1276POR();
	  

    // Wait 1 ms
     delay_ms( 1 );
	
	   SetSX1276POR();
    // Configure RESET as input
		gpio_init_struct.Pin = SX1276POR_Pin;                /* reset引脚 */
		gpio_init_struct.Mode = GPIO_MODE_INPUT;             /* 输入 */
		gpio_init_struct.Pull = GPIO_PULLUP;   
		HAL_GPIO_Init(SX1276_Port1, &gpio_init_struct); 

    // Wait 6 ms
    delay_ms( 6 );
}

void SX1276SetRfTxPower( int8_t power )
{
    uint8_t paConfig = 0;
    uint8_t paDac = 0;

    paConfig = SX1276Read( REG_PACONFIG );
    paDac = SX1276Read( REG_PADAC );

    paConfig = ( paConfig & RF_PACONFIG_PASELECT_MASK ) | SX1276GetPaSelect( SX1276.Settings.Channel );

    if( ( paConfig & RF_PACONFIG_PASELECT_PABOOST ) == RF_PACONFIG_PASELECT_PABOOST )
    {
        if( power > 17 )
        {
            paDac = ( paDac & RF_PADAC_20DBM_MASK ) | RF_PADAC_20DBM_ON;
        }
        else
        {
            paDac = ( paDac & RF_PADAC_20DBM_MASK ) | RF_PADAC_20DBM_OFF;
        }
        if( ( paDac & RF_PADAC_20DBM_ON ) == RF_PADAC_20DBM_ON )
        {
            if( power < 5 )
            {
                power = 5;
            }
            if( power > 20 )
            {
                power = 20;
            }
            paConfig = ( paConfig & RF_PACONFIG_OUTPUTPOWER_MASK ) | ( uint8_t )( ( uint16_t )( power - 5 ) & 0x0F );
        }
        else
        {
            if( power < 2 )
            {
                power = 2;
            }
            if( power > 17 )
            {
                power = 17;
            }
            paConfig = ( paConfig & RF_PACONFIG_OUTPUTPOWER_MASK ) | ( uint8_t )( ( uint16_t )( power - 2 ) & 0x0F );
        }
    }
    else
    {
        if( power > 0 )
        {
            if( power > 15 )
            {
                power = 15;
            }
            paConfig = ( paConfig & RF_PACONFIG_MAX_POWER_MASK & RF_PACONFIG_OUTPUTPOWER_MASK ) | ( 7 << 4 ) | ( power );
        }
        else
        {
            if( power < -4 )
            {
                power = -4;
            }
            paConfig = ( paConfig & RF_PACONFIG_MAX_POWER_MASK & RF_PACONFIG_OUTPUTPOWER_MASK ) | ( 0 << 4 ) | ( power + 4 );
        }
    }
    SX1276Write( REG_PACONFIG, paConfig );
    SX1276Write( REG_PADAC, paDac );
}

static uint8_t SX1276GetPaSelect( uint32_t channel )
{
    return RF_PACONFIG_PASELECT_PABOOST;
}

void SX1276SetAntSwLowPower( bool status )
{
    // Control the TCXO and Antenna switch
    if( RadioIsActive != status )
    {
        RadioIsActive = status;
    }
}

void SX1276SetAntSw( uint8_t opMode )
{
}

bool SX1276CheckRfFrequency( uint32_t frequency )
{
    // Implement check. Currently all frequencies are supported
    return true;
}

uint32_t SX1276GetDio1PinState( void )
{
   return SX1276DIO1();
}


