/*
 * handles interrupt driven uart I/O for RPI UART port USART3
 */
#ifdef BOARD_PRIMEALPHA
#include "uart3_comms.h"
#include "board.h"

#include <stdlib.h>

static xTaskHandle xTaskToNotify = NULL;

/* Transmit and receive ring buffers */
static RINGBUFF_T txring, rxring;

/* Transmit and receive ring buffer sizes */
#define UART_SRB_SIZE 128	/* Send */
#define UART_RRB_SIZE 256	/* Receive */

/* Transmit and receive buffers */
static uint8_t rxbuff[UART_RRB_SIZE], txbuff[UART_SRB_SIZE];

/* Use UART3 for Prime alpha boards P2.3 : UART3_TXD, P2.4 : UART3_RX on RPI header*/

#define LPC_UARTX       LPC_USART3
#define UARTx_IRQn      USART3_IRQn
#define UARTx_IRQHandler UART3_IRQHandler

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

void set_notification_uart3(xTaskHandle h)
{
	/* Store the handle of the calling task. */
	xTaskToNotify = h;
}

int setup_uart3(uint32_t baudrate)
{
	Board_UART_Init(LPC_UARTX);

	/* Setup UART for baudrate 8N1 */
	Chip_UART_Init(LPC_UARTX);
	Chip_UART_SetBaud(LPC_UARTX, baudrate);
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

void stop_uart3()
{
	NVIC_DisableIRQ(UARTx_IRQn);
	Chip_UART_IntDisable(LPC_UARTX, (UART_IER_RBRINT | UART_IER_RLSINT));
}

size_t read_uart3(char * buf, size_t length)
{
	int bytes = Chip_UART_ReadRB(LPC_UARTX, &rxring, buf, length);
	return bytes;
}

size_t write_uart3(const char *buf, size_t length)
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
#endif // BOARD_PRIMEALPHA
