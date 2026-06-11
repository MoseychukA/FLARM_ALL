#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "radio.h"
#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/usart/usart.h"
#include "./SYSTEM/delay/delay.h"
#include "./BSP/SX1276/SX1276.h"
#include "./BSP/SX1276/board-config.h"
#include "./BSP/SX1276/sx1276-board.h"


/**************************************************
RFM95 & RFM96 & RFM97 Frequency Range£º137~1020MHZ
RFM98 Frequency Range£º137~525MHZ

RFM95 & RFM96 & RFM98 Spreading Factor Range£º6~12
RFM97 Spreading Factor Range£º6~9
***************************************************/
#define RF_FREQUENCY                                915000000 // Hz

#define TX_OUTPUT_POWER                             17        // dBm

#define LORA_BANDWIDTH                              0         // [0: 125 kHz,
                                                              //  1: 250 kHz,
                                                              //  2: 500 kHz,
                                                              //  3: Reserved]
#define LORA_SPREADING_FACTOR                       10         // [SF6..SF12]
#define LORA_CODINGRATE                             1         // [1: 4/5,
                                                              //  2: 4/6,
                                                              //  3: 4/7,
                                                              //  4: 4/8]
#define LORA_PREAMBLE_LENGTH                        8        // Same for Tx and Rx
#define LORA_SYMBOL_TIMEOUT                         0        // Symbols
#define LORA_FIX_LENGTH_PAYLOAD_ON                  false
#define LORA_IQ_INVERSION_ON                        false

typedef enum
{
    LOWPOWER,
    RX,
    RX_TIMEOUT,
    RX_ERROR,
    TX,
    TX_TIMEOUT
}States_t;

	
#define RX_TIMEOUT_VALUE                            1000
#define BUFFER_SIZE                                 5 // Define the payload size here

const uint8_t PingMsg[] = "PING";
const uint8_t PongMsg[] = "PONG";

uint16_t BufferSize = BUFFER_SIZE;
uint8_t Buffer[BUFFER_SIZE];

volatile States_t State = LOWPOWER;

int8_t RssiValue = 0;
int8_t SnrValue = 0;

uint32_t ChipId[2] = {0};

/*!
 * Radio events function pointer
 */
static RadioEvents_t RadioEvents;
/*!
 * \brief Function to be executed on Radio Tx Done event
 */
void OnTxDone( void );

/*!
 * \brief Function to be executed on Radio Rx Done event
 */
void OnRxDone( uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr );

/*!
 * \brief Function executed on Radio Tx Timeout event
 */
void OnTxTimeout( void );

/*!
 * \brief Function executed on Radio Rx Timeout event
 */
void OnRxTimeout( void );

/*!
 * \brief Function executed on Radio Rx Error event
 */
void OnRxError( void );


int app_start( void )
{
    bool isMaster = true;
    uint8_t i;
    // Radio initialization
    RadioEvents.TxDone = OnTxDone;
    RadioEvents.RxDone = OnRxDone;
    RadioEvents.TxTimeout = OnTxTimeout;
    RadioEvents.RxTimeout = OnRxTimeout;
    RadioEvents.RxError = OnRxError;

    Radio.Init( &RadioEvents );
	
#ifdef RFM98
		if(RF_FREQUENCY > 525000000)
			 printf("ERROR FREQ!\r\n");
#endif

#ifdef RFM97
	 if(LORA_SPREADING_FACTOR > 9)
			printf("ERROR SF!\r\n");
#endif


    Radio.SetChannel( RF_FREQUENCY );
	 
    Radio.SetTxConfig( MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                                   LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                                   LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                                   true, 0, 0, LORA_IQ_INVERSION_ON, 3000 );

    Radio.SetRxConfig( MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                                   LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                                   LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
                                   0, true, 0, 0, LORA_IQ_INVERSION_ON, true );


		Radio.Rx( RX_TIMEOUT_VALUE );

    while( 1 )
    {
        switch( State )
        {
          case RX:
            if( isMaster == true )
            {
                if( BufferSize > 0 )
                {
                    if( strncmp( ( const char* )Buffer, ( const char* )PongMsg, 4 ) == 0 )
                    {
                        printf("RX:PONG");

                        // Send the next PING frame
                        Buffer[0] = 'P';
                        Buffer[1] = 'I';
                        Buffer[2] = 'N';
                        Buffer[3] = 'G';
                        // We fill the buffer with numbers for the payload
                        for( i = 4; i < BufferSize; i++ )
                        {
                            Buffer[i] = i - 4;
                        }
                        delay_ms(10);
                        printf("SEND:PING");
                        Radio.Send( Buffer, BufferSize );
                    }
                    else if( strncmp( ( const char* )Buffer, ( const char* )PingMsg, 4 ) == 0 )
                    { // A master already exists then become a slave
                        isMaster = false;
                        Radio.Rx( RX_TIMEOUT_VALUE );
                    }
                    else // valid reception but neither a PING or a PONG message
                    {    // Set device as master ans start again
                        isMaster = true;
                        Radio.Rx( RX_TIMEOUT_VALUE );
                    }
                }
            }
            else
            {
                if( BufferSize > 0 )
                {
                    if( strncmp( ( const char* )Buffer, ( const char* )PingMsg, 4 ) == 0 )
                    {
                        printf("RX:PING");
                        // Send the reply to the PONG string
                        Buffer[0] = 'P';
                        Buffer[1] = 'O';
                        Buffer[2] = 'N';
                        Buffer[3] = 'G';
                        // We fill the buffer with numbers for the payload
                        for( i = 4; i < BufferSize; i++ )
                        {
                            Buffer[i] = i - 4;
                        }
                        delay_ms(10);
                        Radio.Send( Buffer, BufferSize );
												printf("SEND:PONG");
                    }
                    else // valid reception but not a PING as expected
                    {    // Set device as master and start again
                        isMaster = true;
                        Radio.Rx( RX_TIMEOUT_VALUE );
                    }
                }
            }
            State = LOWPOWER;
            break;
        case TX:
            Radio.Rx( RX_TIMEOUT_VALUE );
            State = LOWPOWER;
            break;
        case RX_TIMEOUT:
        case RX_ERROR:
            if( isMaster == true )
            {
                // Send the next PING frame
                Buffer[0] = 'P';
                Buffer[1] = 'I';
                Buffer[2] = 'N';
                Buffer[3] = 'G';
                
                for( i = 4; i < BufferSize; i++ )
                {
                    Buffer[i] = i - 4;
                }
                delay_ms(10);
                Radio.Send( Buffer, BufferSize );
                printf("SEND:PING");
            }
            else
            {
                Radio.Rx( RX_TIMEOUT_VALUE );
            }
            State = LOWPOWER;
            break;
        case TX_TIMEOUT:
            Radio.Rx( RX_TIMEOUT_VALUE );
            State = LOWPOWER;
            break;
        case LOWPOWER:
        default:
            // Set low power
            break;
        }
    }
}

void OnTxDone( void )
{
    Radio.Sleep( );
    State = TX;
}

void OnRxDone( uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr )
{
    Radio.Sleep( );
    BufferSize = size;
    memcpy( Buffer, payload, BufferSize );
    RssiValue = rssi;
    SnrValue = snr;
    State = RX;
}

void OnTxTimeout( void )
{
    Radio.Sleep( );
    State = TX_TIMEOUT;
		printf("Tx Timeout\r\n");
}

void OnRxTimeout( void )
{
    Radio.Sleep( );
    State = RX_TIMEOUT;
		printf("Rx Timeout\r\n");
}

void OnRxError( void )
{
    Radio.Sleep( );
    State = RX_ERROR;
		printf("Rx Error\r\n");
}
