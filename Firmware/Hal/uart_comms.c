/*
 * handles interrupt driven uart I/O
 */
#include "uart_comms.h"

#include <stdlib.h>

static xTaskHandle xTaskToNotify = NULL;

/* Transmit and receive ring buffers */
STATIC RINGBUFF_T txring, rxring;

/* Transmit and receive ring buffer sizes */
#define UART_SRB_SIZE 128	/* Send */
#define UART_RRB_SIZE 256	/* Receive */

/* Transmit and receive buffers */
static uint8_t rxbuff[UART_RRB_SIZE], txbuff[UART_SRB_SIZE];

// select the UART to use
#if defined(Bambino) && defined (USE_UART0)
/* Use UART0 for Bambino boards P6.5 : UART0_TXD, P6.4 : UART0_RXD */
#define LPC_UARTX       LPC_USART0
#define UARTx_IRQn      USART0_IRQn
#define UARTx_IRQHandler UART0_IRQHandler

// Mini alpha also needs to be told which uart it uses as the pins are different
#elif defined(Minialpha) && defined(USE_UART2)
/* Use UART2 for mini alpha boards PA.1 : UART2_TXD, PA.2 : UART2_RX */
#define LPC_UARTX       LPC_USART2
#define UARTx_IRQn      USART2_IRQn
#define UARTx_IRQHandler UART2_IRQHandler

#elif defined(Minialpha) && defined(USE_UART0)
/* Use UART0 for mini alpha boards P2.0 : UART0_TXD, P2.1 : UART0_RX */
#define LPC_UARTX       LPC_USART0
#define UARTx_IRQn      USART0_IRQn
#define UARTx_IRQHandler UART0_IRQHandler

#elif defined(Minialpha) && defined(USE_UART1)
// Use UART1 P1.13 : UART1_TXD, P1.14 : UART1_RX
#define LPC_UARTX       LPC_UART1
#define UARTx_IRQn      UART1_IRQn
#define UARTx_IRQHandler UART1_IRQHandler

#elif defined(Alpha) && defined(USE_UART0)
/* Use UART0 for Alpha boards PF.10 : UART0_TXD, PF.11 : UART0_RX */
#define LPC_UARTX       LPC_USART0
#define UARTx_IRQn      USART0_IRQn
#define UARTx_IRQHandler UART0_IRQHandler

#else
#error Board needs to define which UART to use (USE_UART[0|1|2])
#endif


/**
 * @brief	UART interrupt handler using ring buffers
 * @return	Nothing
 */
void UARTx_IRQHandler(void)
{
	/* Want to handle any errors? Do it here. */

	/* Use default ring buffer handler. Override this with your own
	   code if you need more capability. */
	Chip_UART_IRQRBHandler(LPC_UARTX, &rxring, &txring);

	if(xTaskToNotify != NULL) {
		/* Notify the task that the transmission is complete. */
		static BaseType_t xHigherPriorityTaskWoken;
		xHigherPriorityTaskWoken = pdFALSE;
		vTaskNotifyGiveFromISR( xTaskToNotify, &xHigherPriorityTaskWoken );
		portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
	}
}

void set_notification_uart(xTaskHandle h)
{
	/* Store the handle of the calling task. */
	xTaskToNotify = h;
}

int setup_uart()
{
	Board_UART_Init(LPC_UARTX);

	/* Setup UART for 115.2K8N1 */
	Chip_UART_Init(LPC_UARTX);
	Chip_UART_SetBaud(LPC_UARTX, 115200);
	Chip_UART_ConfigData(LPC_UARTX, (UART_LCR_WLEN8 | UART_LCR_SBS_1BIT));
	Chip_UART_SetupFIFOS(LPC_UARTX, (UART_FCR_FIFO_EN | UART_FCR_TRG_LEV2));
	Chip_UART_TXEnable(LPC_UARTX);

	/* Before using the ring buffers, initialize them using the ring
	   buffer init function */
	RingBuffer_Init(&rxring, rxbuff, 1, UART_RRB_SIZE);
	RingBuffer_Init(&txring, txbuff, 1, UART_SRB_SIZE);

	/* Reset and enable FIFOs, FIFO trigger level 3 (14 chars) */
	Chip_UART_SetupFIFOS(LPC_UARTX, (UART_FCR_FIFO_EN | UART_FCR_RX_RS |
							UART_FCR_TX_RS | UART_FCR_TRG_LEV3));

	/* Enable receive data and line status interrupt */
	Chip_UART_IntEnable(LPC_UARTX, (UART_IER_RBRINT | UART_IER_RLSINT));

	NVIC_SetPriority(UARTx_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
	NVIC_EnableIRQ(UARTx_IRQn);

	return 1;
}

size_t read_uart(char * buf, size_t length)
{
	int bytes = Chip_UART_ReadRB(LPC_UARTX, &rxring, buf, length);
	return bytes;
}

size_t write_uart(const char *buf, size_t length)
{
	// Note we do a blocking write here until all is written
	size_t sent_cnt = 0;
	while(sent_cnt < length) {
		int n = Chip_UART_SendRB(LPC_UARTX, &txring, buf+sent_cnt, length-sent_cnt);
		if(n > 0) {
			sent_cnt += n;
		}
	}

	return length;
}
