/*
    FreeRTOS V7.6.0 - Copyright (C) 2013 Real Time Engineers Ltd. 
    All rights reserved

    VISIT http://www.FreeRTOS.org TO ENSURE YOU ARE USING THE LATEST VERSION.

    ***************************************************************************
     *                                                                       *
     *    FreeRTOS provides completely free yet professionally developed,    *
     *    robust, strictly quality controlled, supported, and cross          *
     *    platform software that has become a de facto standard.             *
     *                                                                       *
     *    Help yourself get started quickly and support the FreeRTOS         *
     *    project by purchasing a FreeRTOS tutorial book, reference          *
     *    manual, or both from: http://www.FreeRTOS.org/Documentation        *
     *                                                                       *
     *    Thank you!                                                         *
     *                                                                       *
    ***************************************************************************

    This file is part of the FreeRTOS distribution.

    FreeRTOS is free software; you can redistribute it and/or modify it under
    the terms of the GNU General Public License (version 2) as published by the
    Free Software Foundation >>!AND MODIFIED BY!<< the FreeRTOS exception.

    >>! NOTE: The modification to the GPL is included to allow you to distribute
    >>! a combined work that includes FreeRTOS without being obliged to provide
    >>! the source code for proprietary components outside of the FreeRTOS
    >>! kernel.

    FreeRTOS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.  Full license text is available from the following
    link: http://www.freertos.org/a00114.html

    1 tab == 4 spaces!

    ***************************************************************************
     *                                                                       *
     *    Having a problem?  Start by reading the FAQ "My application does   *
     *    not run, what could be wrong?"                                     *
     *                                                                       *
     *    http://www.FreeRTOS.org/FAQHelp.html                               *
     *                                                                       *
    ***************************************************************************

    http://www.FreeRTOS.org - Documentation, books, training, latest versions,
    license and Real Time Engineers Ltd. contact details.

    http://www.FreeRTOS.org/plus - A selection of FreeRTOS ecosystem products,
    including FreeRTOS+Trace - an indispensable productivity tool, a DOS
    compatible FAT file system, and our tiny thread aware UDP/IP stack.

    http://www.OpenRTOS.com - Real Time Engineers ltd license FreeRTOS to High
    Integrity Systems to sell under the OpenRTOS brand.  Low cost OpenRTOS
    licenses offer ticketed support, indemnification and middleware.

    http://www.SafeRTOS.com - High Integrity Systems also provide a safety
    engineered and independently SIL3 certified version for use in safety and
    mission critical applications that require provable dependability.

    1 tab == 4 spaces!
*/

/*
	BASIC INTERRUPT DRIVEN SERIAL PORT DRIVER FOR UART0.

	*NOTE* - This file is designed to test some of the RTOS features - it is
	not intended to represent an efficient implementation!
*/

/* Standard includes. */
#include <stdlib.h>

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "queue.h"

/* Demo application includes. */
#include "serial.h"


/* Hardware specifics. */
#define serRX_DATA_PIN		( 0x01 )
#define serTX_DATA_PIN		( 0x02 )
#define serCLOCK_Fxx_DIV_8		0x03
#define serUPWR		( 0x80 )
#define serUTXE		( 0x40 )
#define serURXE		( 0x20 )
#define serUCL		( 0x02 )
#define serLSB		( 0x10 )

/* Misc. */
#define serINVALID_QUEUE				( ( xQueueHandle ) 0 )
#define serHANDLE						( ( xComPortHandle ) 1 )
#define serNO_BLOCK						( ( portTickType ) 0 )

/*-----------------------------------------------------------*/

/* Queues used to hold received characters, and characters waiting to be
transmitted. */
static xQueueHandle xRxedChars;
static xQueueHandle xCharsForTx;

/*-----------------------------------------------------------*/

/* Interrupt entry point written in the assembler file serialISR.s85. */
extern void vSerialISREntry( void );

/* Flag to indicate whether or not there are characters already queued to send. */
static volatile unsigned long ulTxInProgress = pdFALSE;

/*-----------------------------------------------------------*/

/*
 * See the serial2.h header file.
 */
xComPortHandle xSerialPortInitMinimal( unsigned portLONG ulWantedBaud, unsigned portBASE_TYPE uxQueueLength )
{
xComPortHandle xReturn = serHANDLE;
const unsigned portLONG ulFuclk = ( configCPU_CLOCK_HZ / 2 ) / 8UL;

	/* Create the queues used to hold Rx and Tx characters. */
	xRxedChars = xQueueCreate( uxQueueLength, ( unsigned portBASE_TYPE ) sizeof( signed portCHAR ) );
	xCharsForTx = xQueueCreate( uxQueueLength + 1, ( unsigned portBASE_TYPE ) sizeof( signed portCHAR ) );

	/* If the queues were created correctly then setup the serial port
	hardware. */
	if( ( xRxedChars != serINVALID_QUEUE ) && ( xCharsForTx != serINVALID_QUEUE ) )
	{
		portENTER_CRITICAL();
		{
			/* Set the UART0 Rx and Tx pins to their alternative function. */
			PMC3 |= ( serRX_DATA_PIN | serTX_DATA_PIN );
			PM3 &= ~( serTX_DATA_PIN );

			/* Setup clock for required baud. */			
			UD0CTL1 = serCLOCK_Fxx_DIV_8;
			UD0CTL2 = ulFuclk / ( 2 * ulWantedBaud );

			/* Enable, n81. */			
			UD0CTL0 = ( serUPWR | serUTXE | serURXE | serUCL | serLSB );
			
			/* Enable interrupts for both Rx and Tx. */
			UD0TIC  = 0x07;
			UD0RIC  = 0x07;
			
			ulTxInProgress = pdFALSE;
		}
		portEXIT_CRITICAL();
	}
	else
	{
		xReturn = ( xComPortHandle ) 0;
	}

	/* This demo file only supports a single port but we have to return
	something to comply with the standard demo header file. */
	return xReturn;
}
/*-----------------------------------------------------------*/

signed portBASE_TYPE xSerialGetChar( xComPortHandle pxPort, signed portCHAR *pcRxedChar, portTickType xBlockTime )
{
	/* The port handle is not required as this driver only supports one port. */
	( void ) pxPort;

	/* Get the next character from the buffer.  Return false if no characters
	are available, or arrive before xBlockTime expires. */
	if( xQueueReceive( xRxedChars, pcRxedChar, xBlockTime ) )
	{
		return pdTRUE;
	}
	else
	{
		return pdFALSE;
	}
}
/*-----------------------------------------------------------*/

void vSerialPutString( xComPortHandle pxPort, const signed portCHAR * const pcString, unsigned portSHORT usStringLength )
{
signed portCHAR *pxNext;

	/* A couple of parameters that this port does not use. */
	( void ) usStringLength;
	( void ) pxPort;

	/* NOTE: This implementation does not handle the queue being full as no
	block time is used! */

	/* The port handle is not required as this driver only supports UART0. */
	( void ) pxPort;

	/* Send each character in the string, one at a time. */
	pxNext = ( signed portCHAR * ) pcString;
	while( *pxNext )
	{
		xSerialPutChar( pxPort, *pxNext, serNO_BLOCK );
		pxNext++;
	}
}
/*-----------------------------------------------------------*/

signed portBASE_TYPE xSerialPutChar( xComPortHandle pxPort, signed portCHAR cOutChar, portTickType xBlockTime )
{
portBASE_TYPE xReturn = pdPASS;

	portENTER_CRITICAL();
	{
		/* There are currently no characters queued up to send so write the
		character directly to the UART. */
		if( ulTxInProgress == pdFALSE )
		{
			UD0TX = cOutChar;
			ulTxInProgress = pdTRUE;
		}
		else
		{
			/* The UART is already busy so write the character to the Tx queue.
			The queue is drained from within the Tx interrupt. */
			if( xQueueSend( xCharsForTx, &cOutChar, xBlockTime ) != pdPASS )
			{
				xReturn = pdFAIL;
			}
		}
	}
	portEXIT_CRITICAL();
	
	return xReturn;
}
/*-----------------------------------------------------------*/

void vSerialClose( xComPortHandle xPort )
{
	/* Not supported as not required by the demo application. */
}
/*-----------------------------------------------------------*/

/* Tx interrupt handler.  This is called from the asm file wrapper. */
void vUARTTxISRHandler( void )
{
char cChar;
portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;

	/* Are there any more characters queue to transmit? */
	if( xQueueReceiveFromISR( xCharsForTx, &cChar, &xHigherPriorityTaskWoken ) == pdTRUE )
	{
		/* Send the next character. */
		UD0TX = cChar;
	}
	else
	{
		/* The UART is no longer active. */
		ulTxInProgress = pdFALSE;
	}
	
	/* If reading a character from the Rx queue caused a task to unblock, and
	the unblocked task has a priority higher than the currently running task,
	then xHigherPriorityTaskWoken will have been set to true and a context
	switch should occur now. */
	portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}
/*-----------------------------------------------------------*/

/* Rx interrupt handler.  This is called from the asm file wrapper. */
void vUARTRxISRHandler( void )
{
portCHAR cChar;
portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;

	/* Send the received character to the Rx queue. */
	cChar = UD0RX;
	xQueueSendFromISR( xRxedChars, &cChar, &xHigherPriorityTaskWoken );
	
	/* If sending a character to the Tx queue caused a task to unblock, and
	the unblocked task has a priority higher than the currently running task,
	then xHigherPriorityTaskWoken will have been set to true and a context
	switch should occur now. */
	portYIELD_FROM_ISR( xHigherPriorityTaskWoken );	
}




	
